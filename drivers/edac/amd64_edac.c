#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#include "amd64_edac.h"
#include <asm/amd_nb.h>

static struct edac_pci_ctl_info *amd64_ctl_pci;

static int report_gart_errors;
module_param(report_gart_errors, int, 0644);

static int ecc_enable_override;
module_param(ecc_enable_override, int, 0644);

static struct msr __percpu *msrs;

static atomic_t drv_instances = ATOMIC_INIT(0);

static struct mem_ctl_info **mcis;
static struct ecc_settings **ecc_stngs;

static const struct scrubrate {
       u32 scrubval;            
       u32 bandwidth;           
} scrubrates[] = {
	{ 0x01, 1600000000UL},
	{ 0x02, 800000000UL},
	{ 0x03, 400000000UL},
	{ 0x04, 200000000UL},
	{ 0x05, 100000000UL},
	{ 0x06, 50000000UL},
	{ 0x07, 25000000UL},
	{ 0x08, 12284069UL},
	{ 0x09, 6274509UL},
	{ 0x0A, 3121951UL},
	{ 0x0B, 1560975UL},
	{ 0x0C, 781440UL},
	{ 0x0D, 390720UL},
	{ 0x0E, 195300UL},
	{ 0x0F, 97650UL},
	{ 0x10, 48854UL},
	{ 0x11, 24427UL},
	{ 0x12, 12213UL},
	{ 0x13, 6101UL},
	{ 0x14, 3051UL},
	{ 0x15, 1523UL},
	{ 0x16, 761UL},
	{ 0x00, 0UL},         
};

int __amd64_read_pci_cfg_dword(struct pci_dev *pdev, int offset,
			       u32 *val, const char *func)
{
	int err = 0;

	err = pci_read_config_dword(pdev, offset, val);
	if (err)
		amd64_warn("%s: error reading F%dx%03x.\n",
			   func, PCI_FUNC(pdev->devfn), offset);

	return err;
}

int __amd64_write_pci_cfg_dword(struct pci_dev *pdev, int offset,
				u32 val, const char *func)
{
	int err = 0;

	err = pci_write_config_dword(pdev, offset, val);
	if (err)
		amd64_warn("%s: error writing to F%dx%03x.\n",
			   func, PCI_FUNC(pdev->devfn), offset);

	return err;
}

static int k8_read_dct_pci_cfg(struct amd64_pvt *pvt, int addr, u32 *val,
			       const char *func)
{
	if (addr >= 0x100)
		return -EINVAL;

	return __amd64_read_pci_cfg_dword(pvt->F2, addr, val, func);
}

static int f10_read_dct_pci_cfg(struct amd64_pvt *pvt, int addr, u32 *val,
				 const char *func)
{
	return __amd64_read_pci_cfg_dword(pvt->F2, addr, val, func);
}

static void f15h_select_dct(struct amd64_pvt *pvt, u8 dct)
{
	u32 reg = 0;

	amd64_read_pci_cfg(pvt->F1, DCT_CFG_SEL, &reg);
	reg &= 0xfffffffe;
	reg |= dct;
	amd64_write_pci_cfg(pvt->F1, DCT_CFG_SEL, reg);
}

static int f15_read_dct_pci_cfg(struct amd64_pvt *pvt, int addr, u32 *val,
				 const char *func)
{
	u8 dct  = 0;

	if (addr >= 0x140 && addr <= 0x1a0) {
		dct   = 1;
		addr -= 0x100;
	}

	f15h_select_dct(pvt, dct);

	return __amd64_read_pci_cfg_dword(pvt->F2, addr, val, func);
}

static int __amd64_set_scrub_rate(struct pci_dev *ctl, u32 new_bw, u32 min_rate)
{
	u32 scrubval;
	int i;

	for (i = 0; i < ARRAY_SIZE(scrubrates) - 1; i++) {
		 
		if (scrubrates[i].scrubval < min_rate)
			continue;

		if (scrubrates[i].bandwidth <= new_bw)
			break;
	}

	scrubval = scrubrates[i].scrubval;

	pci_write_bits32(ctl, SCRCTRL, scrubval, 0x001F);

	if (scrubval)
		return scrubrates[i].bandwidth;

	return 0;
}

static int amd64_set_scrub_rate(struct mem_ctl_info *mci, u32 bw)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	u32 min_scrubrate = 0x5;

	if (boot_cpu_data.x86 == 0xf)
		min_scrubrate = 0x0;

	if (boot_cpu_data.x86 == 0x15)
		f15h_select_dct(pvt, 0);

	return __amd64_set_scrub_rate(pvt->F3, bw, min_scrubrate);
}

static int amd64_get_scrub_rate(struct mem_ctl_info *mci)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	u32 scrubval = 0;
	int i, retval = -EINVAL;

	if (boot_cpu_data.x86 == 0x15)
		f15h_select_dct(pvt, 0);

	amd64_read_pci_cfg(pvt->F3, SCRCTRL, &scrubval);

	scrubval = scrubval & 0x001F;

	for (i = 0; i < ARRAY_SIZE(scrubrates); i++) {
		if (scrubrates[i].scrubval == scrubval) {
			retval = scrubrates[i].bandwidth;
			break;
		}
	}
	return retval;
}

static bool amd64_base_limit_match(struct amd64_pvt *pvt, u64 sys_addr,
				   u8 nid)
{
	u64 addr;

	addr = sys_addr & 0x000000ffffffffffull;

	return ((addr >= get_dram_base(pvt, nid)) &&
		(addr <= get_dram_limit(pvt, nid)));
}

static struct mem_ctl_info *find_mc_by_sys_addr(struct mem_ctl_info *mci,
						u64 sys_addr)
{
	struct amd64_pvt *pvt;
	u8 node_id;
	u32 intlv_en, bits;

	pvt = mci->pvt_info;

	intlv_en = dram_intlv_en(pvt, 0);

	if (intlv_en == 0) {
		for (node_id = 0; node_id < DRAM_RANGES; node_id++) {
			if (amd64_base_limit_match(pvt, sys_addr, node_id))
				goto found;
		}
		goto err_no_match;
	}

	if (unlikely((intlv_en != 0x01) &&
		     (intlv_en != 0x03) &&
		     (intlv_en != 0x07))) {
		amd64_warn("DRAM Base[IntlvEn] junk value: 0x%x, BIOS bug?\n", intlv_en);
		return NULL;
	}

	bits = (((u32) sys_addr) >> 12) & intlv_en;

	for (node_id = 0; ; ) {
		if ((dram_intlv_sel(pvt, node_id) & intlv_en) == bits)
			break;	 

		if (++node_id >= DRAM_RANGES)
			goto err_no_match;
	}

	if (unlikely(!amd64_base_limit_match(pvt, sys_addr, node_id))) {
		amd64_warn("%s: sys_addr 0x%llx falls outside base/limit address"
			   "range for node %d with node interleaving enabled.\n",
			   __func__, sys_addr, node_id);
		return NULL;
	}

found:
	return edac_mc_find((int)node_id);

err_no_match:
	edac_dbg(2, "sys_addr 0x%lx doesn't match any node\n",
		 (unsigned long)sys_addr);

	return NULL;
}

static void get_cs_base_and_mask(struct amd64_pvt *pvt, int csrow, u8 dct,
				 u64 *base, u64 *mask)
{
	u64 csbase, csmask, base_bits, mask_bits;
	u8 addr_shift;

	if (boot_cpu_data.x86 == 0xf && pvt->ext_model < K8_REV_F) {
		csbase		= pvt->csels[dct].csbases[csrow];
		csmask		= pvt->csels[dct].csmasks[csrow];
#if defined (MY_DEF_HERE)
		base_bits	= GENMASK_ULL(31, 21) | GENMASK_ULL(15, 9);
		mask_bits	= GENMASK_ULL(29, 21) | GENMASK_ULL(15, 9);
#else  
		base_bits	= GENMASK(21, 31) | GENMASK(9, 15);
		mask_bits	= GENMASK(21, 29) | GENMASK(9, 15);
#endif  
		addr_shift	= 4;

	} else if (boot_cpu_data.x86 == 0x16) {
		csbase          = pvt->csels[dct].csbases[csrow];
		csmask          = pvt->csels[dct].csmasks[csrow >> 1];

#if defined (MY_DEF_HERE)
		*base  = (csbase & GENMASK_ULL(15,  5)) << 6;
		*base |= (csbase & GENMASK_ULL(30, 19)) << 8;

		*mask = ~0ULL;
		 
		*mask &= ~((GENMASK_ULL(15, 5)  << 6) |
			   (GENMASK_ULL(30, 19) << 8));

		*mask |= (csmask & GENMASK_ULL(15, 5))  << 6;
		*mask |= (csmask & GENMASK_ULL(30, 19)) << 8;
#else  
		*base  = (csbase & GENMASK(5,  15)) << 6;
		*base |= (csbase & GENMASK(19, 30)) << 8;

		*mask = ~0ULL;
		 
		*mask &= ~((GENMASK(5, 15)  << 6) |
			   (GENMASK(19, 30) << 8));

		*mask |= (csmask & GENMASK(5, 15))  << 6;
		*mask |= (csmask & GENMASK(19, 30)) << 8;

#endif  

		return;
	} else {
		csbase		= pvt->csels[dct].csbases[csrow];
		csmask		= pvt->csels[dct].csmasks[csrow >> 1];
		addr_shift	= 8;

#if defined (MY_DEF_HERE)
		if (pvt->fam == 0x15)
			base_bits = mask_bits =
				GENMASK_ULL(30,19) | GENMASK_ULL(13,5);
		else
			base_bits = mask_bits =
				GENMASK_ULL(28,19) | GENMASK_ULL(13,5);
#else  
		if (boot_cpu_data.x86 == 0x15)
			base_bits = mask_bits = GENMASK(19,30) | GENMASK(5,13);
		else
			base_bits = mask_bits = GENMASK(19,28) | GENMASK(5,13);
#endif  
	}

	*base  = (csbase & base_bits) << addr_shift;

	*mask  = ~0ULL;
	 
	*mask &= ~(mask_bits << addr_shift);
	 
	*mask |= (csmask & mask_bits) << addr_shift;
}

#define for_each_chip_select(i, dct, pvt) \
	for (i = 0; i < pvt->csels[dct].b_cnt; i++)

#define chip_select_base(i, dct, pvt) \
	pvt->csels[dct].csbases[i]

#define for_each_chip_select_mask(i, dct, pvt) \
	for (i = 0; i < pvt->csels[dct].m_cnt; i++)

static int input_addr_to_csrow(struct mem_ctl_info *mci, u64 input_addr)
{
	struct amd64_pvt *pvt;
	int csrow;
	u64 base, mask;

	pvt = mci->pvt_info;

	for_each_chip_select(csrow, 0, pvt) {
		if (!csrow_enabled(csrow, 0, pvt))
			continue;

		get_cs_base_and_mask(pvt, csrow, 0, &base, &mask);

		mask = ~mask;

		if ((input_addr & mask) == (base & mask)) {
			edac_dbg(2, "InputAddr 0x%lx matches csrow %d (node %d)\n",
				 (unsigned long)input_addr, csrow,
				 pvt->mc_node_id);

			return csrow;
		}
	}
	edac_dbg(2, "no matching csrow for InputAddr 0x%lx (MC node %d)\n",
		 (unsigned long)input_addr, pvt->mc_node_id);

	return -1;
}

int amd64_get_dram_hole_info(struct mem_ctl_info *mci, u64 *hole_base,
			     u64 *hole_offset, u64 *hole_size)
{
	struct amd64_pvt *pvt = mci->pvt_info;

	if (boot_cpu_data.x86 == 0xf && pvt->ext_model < K8_REV_E) {
		edac_dbg(1, "  revision %d for node %d does not support DHAR\n",
			 pvt->ext_model, pvt->mc_node_id);
		return 1;
	}

	if (boot_cpu_data.x86 >= 0x10 && !dhar_mem_hoist_valid(pvt)) {
		edac_dbg(1, "  Dram Memory Hoisting is DISABLED on this system\n");
		return 1;
	}

	if (!dhar_valid(pvt)) {
		edac_dbg(1, "  Dram Memory Hoisting is DISABLED on this node %d\n",
			 pvt->mc_node_id);
		return 1;
	}

	*hole_base = dhar_base(pvt);
	*hole_size = (1ULL << 32) - *hole_base;

	if (boot_cpu_data.x86 > 0xf)
		*hole_offset = f10_dhar_offset(pvt);
	else
		*hole_offset = k8_dhar_offset(pvt);

	edac_dbg(1, "  DHAR info for node %d base 0x%lx offset 0x%lx size 0x%lx\n",
		 pvt->mc_node_id, (unsigned long)*hole_base,
		 (unsigned long)*hole_offset, (unsigned long)*hole_size);

	return 0;
}
EXPORT_SYMBOL_GPL(amd64_get_dram_hole_info);

static u64 sys_addr_to_dram_addr(struct mem_ctl_info *mci, u64 sys_addr)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	u64 dram_base, hole_base, hole_offset, hole_size, dram_addr;
	int ret;

	dram_base = get_dram_base(pvt, pvt->mc_node_id);

	ret = amd64_get_dram_hole_info(mci, &hole_base, &hole_offset,
				      &hole_size);
	if (!ret) {
		if ((sys_addr >= (1ULL << 32)) &&
		    (sys_addr < ((1ULL << 32) + hole_size))) {
			 
			dram_addr = sys_addr - hole_offset;

			edac_dbg(2, "using DHAR to translate SysAddr 0x%lx to DramAddr 0x%lx\n",
				 (unsigned long)sys_addr,
				 (unsigned long)dram_addr);

			return dram_addr;
		}
	}

#if defined (MY_DEF_HERE)
	dram_addr = (sys_addr & GENMASK_ULL(39, 0)) - dram_base;
#else  
	dram_addr = (sys_addr & GENMASK(0, 39)) - dram_base;
#endif  

	edac_dbg(2, "using DRAM Base register to translate SysAddr 0x%lx to DramAddr 0x%lx\n",
		 (unsigned long)sys_addr, (unsigned long)dram_addr);
	return dram_addr;
}

static int num_node_interleave_bits(unsigned intlv_en)
{
	static const int intlv_shift_table[] = { 0, 1, 0, 2, 0, 0, 0, 3 };
	int n;

	BUG_ON(intlv_en > 7);
	n = intlv_shift_table[intlv_en];
	return n;
}

static u64 dram_addr_to_input_addr(struct mem_ctl_info *mci, u64 dram_addr)
{
	struct amd64_pvt *pvt;
	int intlv_shift;
	u64 input_addr;

	pvt = mci->pvt_info;

	intlv_shift = num_node_interleave_bits(dram_intlv_en(pvt, 0));
#if defined (MY_DEF_HERE)
	input_addr = ((dram_addr >> intlv_shift) & GENMASK_ULL(35, 12)) +
		      (dram_addr & 0xfff);
#else  
	input_addr = ((dram_addr >> intlv_shift) & GENMASK(12, 35)) +
		      (dram_addr & 0xfff);
#endif  

	edac_dbg(2, "  Intlv Shift=%d DramAddr=0x%lx maps to InputAddr=0x%lx\n",
		 intlv_shift, (unsigned long)dram_addr,
		 (unsigned long)input_addr);

	return input_addr;
}

static u64 sys_addr_to_input_addr(struct mem_ctl_info *mci, u64 sys_addr)
{
	u64 input_addr;

	input_addr =
	    dram_addr_to_input_addr(mci, sys_addr_to_dram_addr(mci, sys_addr));

	edac_dbg(2, "SysAdddr 0x%lx translates to InputAddr 0x%lx\n",
		 (unsigned long)sys_addr, (unsigned long)input_addr);

	return input_addr;
}

static inline void error_address_to_page_and_offset(u64 error_address,
						    struct err_info *err)
{
	err->page = (u32) (error_address >> PAGE_SHIFT);
	err->offset = ((u32) error_address) & ~PAGE_MASK;
}

static int sys_addr_to_csrow(struct mem_ctl_info *mci, u64 sys_addr)
{
	int csrow;

	csrow = input_addr_to_csrow(mci, sys_addr_to_input_addr(mci, sys_addr));

	if (csrow == -1)
		amd64_mc_err(mci, "Failed to translate InputAddr to csrow for "
				  "address 0x%lx\n", (unsigned long)sys_addr);
	return csrow;
}

static int get_channel_from_ecc_syndrome(struct mem_ctl_info *, u16);

static unsigned long amd64_determine_edac_cap(struct amd64_pvt *pvt)
{
	u8 bit;
	unsigned long edac_cap = EDAC_FLAG_NONE;

	bit = (boot_cpu_data.x86 > 0xf || pvt->ext_model >= K8_REV_F)
		? 19
		: 17;

	if (pvt->dclr0 & BIT(bit))
		edac_cap = EDAC_FLAG_SECDED;

	return edac_cap;
}

static void amd64_debug_display_dimm_sizes(struct amd64_pvt *, u8);

static void amd64_dump_dramcfg_low(u32 dclr, int chan)
{
	edac_dbg(1, "F2x%d90 (DRAM Cfg Low): 0x%08x\n", chan, dclr);

	edac_dbg(1, "  DIMM type: %sbuffered; all DIMMs support ECC: %s\n",
		 (dclr & BIT(16)) ?  "un" : "",
		 (dclr & BIT(19)) ? "yes" : "no");

	edac_dbg(1, "  PAR/ERR parity: %s\n",
		 (dclr & BIT(8)) ?  "enabled" : "disabled");

	if (boot_cpu_data.x86 == 0x10)
		edac_dbg(1, "  DCT 128bit mode width: %s\n",
			 (dclr & BIT(11)) ?  "128b" : "64b");

	edac_dbg(1, "  x4 logical DIMMs present: L0: %s L1: %s L2: %s L3: %s\n",
		 (dclr & BIT(12)) ?  "yes" : "no",
		 (dclr & BIT(13)) ?  "yes" : "no",
		 (dclr & BIT(14)) ?  "yes" : "no",
		 (dclr & BIT(15)) ?  "yes" : "no");
}

static void dump_misc_regs(struct amd64_pvt *pvt)
{
	edac_dbg(1, "F3xE8 (NB Cap): 0x%08x\n", pvt->nbcap);

	edac_dbg(1, "  NB two channel DRAM capable: %s\n",
		 (pvt->nbcap & NBCAP_DCT_DUAL) ? "yes" : "no");

	edac_dbg(1, "  ECC capable: %s, ChipKill ECC capable: %s\n",
		 (pvt->nbcap & NBCAP_SECDED) ? "yes" : "no",
		 (pvt->nbcap & NBCAP_CHIPKILL) ? "yes" : "no");

	amd64_dump_dramcfg_low(pvt->dclr0, 0);

	edac_dbg(1, "F3xB0 (Online Spare): 0x%08x\n", pvt->online_spare);

	edac_dbg(1, "F1xF0 (DRAM Hole Address): 0x%08x, base: 0x%08x, offset: 0x%08x\n",
		 pvt->dhar, dhar_base(pvt),
		 (boot_cpu_data.x86 == 0xf) ? k8_dhar_offset(pvt)
		 : f10_dhar_offset(pvt));

	edac_dbg(1, "  DramHoleValid: %s\n", dhar_valid(pvt) ? "yes" : "no");

	amd64_debug_display_dimm_sizes(pvt, 0);

	if (boot_cpu_data.x86 == 0xf)
		return;

	amd64_debug_display_dimm_sizes(pvt, 1);

	amd64_info("using %s syndromes.\n", ((pvt->ecc_sym_sz == 8) ? "x8" : "x4"));

	if (!dct_ganging_enabled(pvt))
		amd64_dump_dramcfg_low(pvt->dclr1, 1);
}

static void prep_chip_selects(struct amd64_pvt *pvt)
{
	if (boot_cpu_data.x86 == 0xf && pvt->ext_model < K8_REV_F) {
		pvt->csels[0].b_cnt = pvt->csels[1].b_cnt = 8;
		pvt->csels[0].m_cnt = pvt->csels[1].m_cnt = 8;
	} else {
		pvt->csels[0].b_cnt = pvt->csels[1].b_cnt = 8;
		pvt->csels[0].m_cnt = pvt->csels[1].m_cnt = 4;
	}
}

static void read_dct_base_mask(struct amd64_pvt *pvt)
{
	int cs;

	prep_chip_selects(pvt);

	for_each_chip_select(cs, 0, pvt) {
		int reg0   = DCSB0 + (cs * 4);
		int reg1   = DCSB1 + (cs * 4);
		u32 *base0 = &pvt->csels[0].csbases[cs];
		u32 *base1 = &pvt->csels[1].csbases[cs];

		if (!amd64_read_dct_pci_cfg(pvt, reg0, base0))
			edac_dbg(0, "  DCSB0[%d]=0x%08x reg: F2x%x\n",
				 cs, *base0, reg0);

		if (boot_cpu_data.x86 == 0xf || dct_ganging_enabled(pvt))
			continue;

		if (!amd64_read_dct_pci_cfg(pvt, reg1, base1))
			edac_dbg(0, "  DCSB1[%d]=0x%08x reg: F2x%x\n",
				 cs, *base1, reg1);
	}

	for_each_chip_select_mask(cs, 0, pvt) {
		int reg0   = DCSM0 + (cs * 4);
		int reg1   = DCSM1 + (cs * 4);
		u32 *mask0 = &pvt->csels[0].csmasks[cs];
		u32 *mask1 = &pvt->csels[1].csmasks[cs];

		if (!amd64_read_dct_pci_cfg(pvt, reg0, mask0))
			edac_dbg(0, "    DCSM0[%d]=0x%08x reg: F2x%x\n",
				 cs, *mask0, reg0);

		if (boot_cpu_data.x86 == 0xf || dct_ganging_enabled(pvt))
			continue;

		if (!amd64_read_dct_pci_cfg(pvt, reg1, mask1))
			edac_dbg(0, "    DCSM1[%d]=0x%08x reg: F2x%x\n",
				 cs, *mask1, reg1);
	}
}

static enum mem_type amd64_determine_memory_type(struct amd64_pvt *pvt, int cs)
{
	enum mem_type type;

	if (boot_cpu_data.x86 >= 0x15)
		type = (pvt->dclr0 & BIT(16)) ?	MEM_DDR3 : MEM_RDDR3;
	else if (boot_cpu_data.x86 == 0x10 || pvt->ext_model >= K8_REV_F) {
		if (pvt->dchr0 & DDR3_MODE)
			type = (pvt->dclr0 & BIT(16)) ?	MEM_DDR3 : MEM_RDDR3;
		else
			type = (pvt->dclr0 & BIT(16)) ? MEM_DDR2 : MEM_RDDR2;
	} else {
		type = (pvt->dclr0 & BIT(18)) ? MEM_DDR : MEM_RDDR;
	}

	amd64_info("CS%d: %s\n", cs, edac_mem_types[type]);

	return type;
}

static int k8_early_channel_count(struct amd64_pvt *pvt)
{
	int flag;

	if (pvt->ext_model >= K8_REV_F)
		 
		flag = pvt->dclr0 & WIDTH_128;
	else
		 
		flag = pvt->dclr0 & REVE_WIDTH_128;

	pvt->dclr1 = 0;

	return (flag) ? 2 : 1;
}

static u64 get_error_address(struct mce *m)
{
	struct cpuinfo_x86 *c = &boot_cpu_data;
	u64 addr;
	u8 start_bit = 1;
	u8 end_bit   = 47;

	if (c->x86 == 0xf) {
		start_bit = 3;
		end_bit   = 39;
	}

#if defined (MY_DEF_HERE)
	addr = m->addr & GENMASK_ULL(end_bit, start_bit);
#else  
	addr = m->addr & GENMASK(start_bit, end_bit);
#endif  

	if (c->x86 == 0x15) {
		struct amd64_pvt *pvt;
		u64 cc6_base, tmp_addr;
		u32 tmp;
		u16 mce_nid;
		u8 intlv_en;

#if defined (MY_DEF_HERE)
		if ((addr & GENMASK_ULL(47, 24)) >> 24 != 0x00fdf7)
			return addr;
#else  
		if ((addr & GENMASK(24, 47)) >> 24 != 0x00fdf7)
			return addr;
#endif  

		mce_nid	= amd_get_nb_id(m->extcpu);
		pvt	= mcis[mce_nid]->pvt_info;

		amd64_read_pci_cfg(pvt->F1, DRAM_LOCAL_NODE_LIM, &tmp);
		intlv_en = tmp >> 21 & 0x7;

#if defined (MY_DEF_HERE)
		cc6_base  = (tmp & GENMASK_ULL(20, 0)) << 3;
#else  
		cc6_base  = (tmp & GENMASK(0, 20)) << 3;
#endif  

		cc6_base |= intlv_en ^ 0x7;

		cc6_base <<= 24;

#if defined (MY_DEF_HERE)
		if (!intlv_en)
			return cc6_base | (addr & GENMASK_ULL(23, 0));
#else  
		if (!intlv_en)
			return cc6_base | (addr & GENMASK(0, 23));
#endif  

		amd64_read_pci_cfg(pvt->F1, DRAM_LOCAL_NODE_BASE, &tmp);

#if defined (MY_DEF_HERE)
							 
		tmp_addr  = (addr & GENMASK_ULL(23, 12)) << __fls(intlv_en + 1);

		tmp_addr |= (tmp & GENMASK_ULL(23, 21)) >> 9;

		tmp_addr |= addr & GENMASK_ULL(11, 0);
#else  
							 
		tmp_addr  = (addr & GENMASK(12, 23)) << __fls(intlv_en + 1);

		tmp_addr |= (tmp & GENMASK(21, 23)) >> 9;

		tmp_addr |= addr & GENMASK(0, 11);
#endif  

		return cc6_base | tmp_addr;
	}

	return addr;
}

static struct pci_dev *pci_get_related_function(unsigned int vendor,
						unsigned int device,
						struct pci_dev *related)
{
	struct pci_dev *dev = NULL;

	while ((dev = pci_get_device(vendor, device, dev))) {
		if (pci_domain_nr(dev->bus) == pci_domain_nr(related->bus) &&
		    (dev->bus->number == related->bus->number) &&
		    (PCI_SLOT(dev->devfn) == PCI_SLOT(related->devfn)))
			break;
	}

	return dev;
}

static void read_dram_base_limit_regs(struct amd64_pvt *pvt, unsigned range)
{
	struct amd_northbridge *nb;
	struct pci_dev *misc, *f1 = NULL;
	struct cpuinfo_x86 *c = &boot_cpu_data;
	int off = range << 3;
	u32 llim;

	amd64_read_pci_cfg(pvt->F1, DRAM_BASE_LO + off,  &pvt->ranges[range].base.lo);
	amd64_read_pci_cfg(pvt->F1, DRAM_LIMIT_LO + off, &pvt->ranges[range].lim.lo);

	if (c->x86 == 0xf)
		return;

	if (!dram_rw(pvt, range))
		return;

	amd64_read_pci_cfg(pvt->F1, DRAM_BASE_HI + off,  &pvt->ranges[range].base.hi);
	amd64_read_pci_cfg(pvt->F1, DRAM_LIMIT_HI + off, &pvt->ranges[range].lim.hi);

	if (c->x86 != 0x15)
		return;

	nb = node_to_amd_nb(dram_dst_node(pvt, range));
	if (WARN_ON(!nb))
		return;

	misc = nb->misc;
	f1 = pci_get_related_function(misc->vendor, PCI_DEVICE_ID_AMD_15H_NB_F1, misc);
	if (WARN_ON(!f1))
		return;

	amd64_read_pci_cfg(f1, DRAM_LOCAL_NODE_LIM, &llim);

#if defined (MY_DEF_HERE)
	pvt->ranges[range].lim.lo &= GENMASK_ULL(15, 0);
#else  
	pvt->ranges[range].lim.lo &= GENMASK(0, 15);
#endif  

	pvt->ranges[range].lim.lo |= ((llim & 0x1fff) << 3 | 0x7) << 16;

#if defined (MY_DEF_HERE)
	pvt->ranges[range].lim.hi &= GENMASK_ULL(7, 0);
#else  
	pvt->ranges[range].lim.hi &= GENMASK(0, 7);
#endif  

	pvt->ranges[range].lim.hi |= llim >> 13;

	pci_dev_put(f1);
}

static void k8_map_sysaddr_to_csrow(struct mem_ctl_info *mci, u64 sys_addr,
				    struct err_info *err)
{
	struct amd64_pvt *pvt = mci->pvt_info;

	error_address_to_page_and_offset(sys_addr, err);

	err->src_mci = find_mc_by_sys_addr(mci, sys_addr);
	if (!err->src_mci) {
		amd64_mc_err(mci, "failed to map error addr 0x%lx to a node\n",
			     (unsigned long)sys_addr);
		err->err_code = ERR_NODE;
		return;
	}

	err->csrow = sys_addr_to_csrow(err->src_mci, sys_addr);
	if (err->csrow < 0) {
		err->err_code = ERR_CSROW;
		return;
	}

	if (pvt->nbcfg & NBCFG_CHIPKILL) {
		err->channel = get_channel_from_ecc_syndrome(mci, err->syndrome);
		if (err->channel < 0) {
			 
			amd64_mc_warn(err->src_mci, "unknown syndrome 0x%04x - "
				      "possible error reporting race\n",
				      err->syndrome);
			err->err_code = ERR_CHANNEL;
			return;
		}
	} else {
		 
		err->channel = ((sys_addr & BIT(3)) != 0);
	}
}

static int ddr2_cs_size(unsigned i, bool dct_width)
{
	unsigned shift = 0;

	if (i <= 2)
		shift = i;
	else if (!(i & 0x1))
		shift = i >> 1;
	else
		shift = (i + 1) >> 1;

	return 128 << (shift + !!dct_width);
}

static int k8_dbam_to_chip_select(struct amd64_pvt *pvt, u8 dct,
				  unsigned cs_mode)
{
	u32 dclr = dct ? pvt->dclr1 : pvt->dclr0;

	if (pvt->ext_model >= K8_REV_F) {
		WARN_ON(cs_mode > 11);
		return ddr2_cs_size(cs_mode, dclr & WIDTH_128);
	}
	else if (pvt->ext_model >= K8_REV_D) {
		unsigned diff;
		WARN_ON(cs_mode > 10);

		diff = cs_mode/3 + (unsigned)(cs_mode > 5);

		return 32 << (cs_mode - diff);
	}
	else {
		WARN_ON(cs_mode > 6);
		return 32 << cs_mode;
	}
}

static int f1x_early_channel_count(struct amd64_pvt *pvt)
{
	int i, j, channels = 0;

	if (boot_cpu_data.x86 == 0x10 && (pvt->dclr0 & WIDTH_128))
		return 2;

	edac_dbg(0, "Data width is not 128 bits - need more decoding\n");

	for (i = 0; i < 2; i++) {
		u32 dbam = (i ? pvt->dbam1 : pvt->dbam0);

		for (j = 0; j < 4; j++) {
			if (DBAM_DIMM(j, dbam) > 0) {
				channels++;
				break;
			}
		}
	}

	if (channels > 2)
		channels = 2;

	amd64_info("MCT channel count: %d\n", channels);

	return channels;
}

static int ddr3_cs_size(unsigned i, bool dct_width)
{
	unsigned shift = 0;
	int cs_size = 0;

	if (i == 0 || i == 3 || i == 4)
		cs_size = -1;
	else if (i <= 2)
		shift = i;
	else if (i == 12)
		shift = 7;
	else if (!(i & 0x1))
		shift = i >> 1;
	else
		shift = (i + 1) >> 1;

	if (cs_size != -1)
		cs_size = (128 * (1 << !!dct_width)) << shift;

	return cs_size;
}

static int f10_dbam_to_chip_select(struct amd64_pvt *pvt, u8 dct,
				   unsigned cs_mode)
{
	u32 dclr = dct ? pvt->dclr1 : pvt->dclr0;

	WARN_ON(cs_mode > 11);

	if (pvt->dchr0 & DDR3_MODE || pvt->dchr1 & DDR3_MODE)
		return ddr3_cs_size(cs_mode, dclr & WIDTH_128);
	else
		return ddr2_cs_size(cs_mode, dclr & WIDTH_128);
}

static int f15_dbam_to_chip_select(struct amd64_pvt *pvt, u8 dct,
				   unsigned cs_mode)
{
	WARN_ON(cs_mode > 12);

	return ddr3_cs_size(cs_mode, false);
}

static int f16_dbam_to_chip_select(struct amd64_pvt *pvt, u8 dct,
				unsigned cs_mode)
{
	WARN_ON(cs_mode > 12);

	if (cs_mode == 6 || cs_mode == 8 ||
	    cs_mode == 9 || cs_mode == 12)
		return -1;
	else
		return ddr3_cs_size(cs_mode, false);
}

static void read_dram_ctl_register(struct amd64_pvt *pvt)
{

	if (boot_cpu_data.x86 == 0xf)
		return;

	if (!amd64_read_dct_pci_cfg(pvt, DCT_SEL_LO, &pvt->dct_sel_lo)) {
		edac_dbg(0, "F2x110 (DCTSelLow): 0x%08x, High range addrs at: 0x%x\n",
			 pvt->dct_sel_lo, dct_sel_baseaddr(pvt));

		edac_dbg(0, "  DCTs operate in %s mode\n",
			 (dct_ganging_enabled(pvt) ? "ganged" : "unganged"));

		if (!dct_ganging_enabled(pvt))
			edac_dbg(0, "  Address range split per DCT: %s\n",
				 (dct_high_range_enabled(pvt) ? "yes" : "no"));

		edac_dbg(0, "  data interleave for ECC: %s, DRAM cleared since last warm reset: %s\n",
			 (dct_data_intlv_enabled(pvt) ? "enabled" : "disabled"),
			 (dct_memory_cleared(pvt) ? "yes" : "no"));

		edac_dbg(0, "  channel interleave: %s, "
			 "interleave bits selector: 0x%x\n",
			 (dct_interleave_enabled(pvt) ? "enabled" : "disabled"),
			 dct_sel_interleave_addr(pvt));
	}

	amd64_read_dct_pci_cfg(pvt, DCT_SEL_HI, &pvt->dct_sel_hi);
}

static u8 f1x_determine_channel(struct amd64_pvt *pvt, u64 sys_addr,
				bool hi_range_sel, u8 intlv_en)
{
	u8 dct_sel_high = (pvt->dct_sel_lo >> 1) & 1;

	if (dct_ganging_enabled(pvt))
		return 0;

	if (hi_range_sel)
		return dct_sel_high;

	if (dct_interleave_enabled(pvt)) {
		u8 intlv_addr = dct_sel_interleave_addr(pvt);

		if (!intlv_addr)
			return sys_addr >> 6 & 1;

		if (intlv_addr & 0x2) {
			u8 shift = intlv_addr & 0x1 ? 9 : 6;
			u32 temp = hweight_long((u32) ((sys_addr >> 16) & 0x1F)) % 2;

			return ((sys_addr >> shift) & 1) ^ temp;
		}

		return (sys_addr >> (12 + hweight8(intlv_en))) & 1;
	}

	if (dct_high_range_enabled(pvt))
		return ~dct_sel_high & 1;

	return 0;
}

static u64 f1x_get_norm_dct_addr(struct amd64_pvt *pvt, u8 range,
				 u64 sys_addr, bool hi_rng,
				 u32 dct_sel_base_addr)
{
	u64 chan_off;
	u64 dram_base		= get_dram_base(pvt, range);
	u64 hole_off		= f10_dhar_offset(pvt);
	u64 dct_sel_base_off	= (u64)(pvt->dct_sel_hi & 0xFFFFFC00) << 16;

	if (hi_rng) {
		 
		if ((!(dct_sel_base_addr >> 16) ||
		     dct_sel_base_addr < dhar_base(pvt)) &&
		    dhar_valid(pvt) &&
		    (sys_addr >= BIT_64(32)))
			chan_off = hole_off;
		else
			chan_off = dct_sel_base_off;
	} else {
		 
		if (dhar_valid(pvt) && (sys_addr >= BIT_64(32)))
			chan_off = hole_off;
		else
			chan_off = dram_base;
	}
#if defined (MY_DEF_HERE)
	return (sys_addr & GENMASK_ULL(47,6)) - (chan_off & GENMASK_ULL(47,23));
#else  

	return (sys_addr & GENMASK(6,47)) - (chan_off & GENMASK(23,47));
#endif  
}

static int f10_process_possible_spare(struct amd64_pvt *pvt, u8 dct, int csrow)
{
	int tmp_cs;

	if (online_spare_swap_done(pvt, dct) &&
	    csrow == online_spare_bad_dramcs(pvt, dct)) {

		for_each_chip_select(tmp_cs, dct, pvt) {
			if (chip_select_base(tmp_cs, dct, pvt) & 0x2) {
				csrow = tmp_cs;
				break;
			}
		}
	}
	return csrow;
}

static int f1x_lookup_addr_in_dct(u64 in_addr, u8 nid, u8 dct)
{
	struct mem_ctl_info *mci;
	struct amd64_pvt *pvt;
	u64 cs_base, cs_mask;
	int cs_found = -EINVAL;
	int csrow;

	mci = mcis[nid];
	if (!mci)
		return cs_found;

	pvt = mci->pvt_info;

	edac_dbg(1, "input addr: 0x%llx, DCT: %d\n", in_addr, dct);

	for_each_chip_select(csrow, dct, pvt) {
		if (!csrow_enabled(csrow, dct, pvt))
			continue;

		get_cs_base_and_mask(pvt, csrow, dct, &cs_base, &cs_mask);

		edac_dbg(1, "    CSROW=%d CSBase=0x%llx CSMask=0x%llx\n",
			 csrow, cs_base, cs_mask);

		cs_mask = ~cs_mask;

		edac_dbg(1, "    (InputAddr & ~CSMask)=0x%llx (CSBase & ~CSMask)=0x%llx\n",
			 (in_addr & cs_mask), (cs_base & cs_mask));

		if ((in_addr & cs_mask) == (cs_base & cs_mask)) {
			cs_found = f10_process_possible_spare(pvt, dct, csrow);

			edac_dbg(1, " MATCH csrow=%d\n", cs_found);
			break;
		}
	}
	return cs_found;
}

static u64 f1x_swap_interleaved_region(struct amd64_pvt *pvt, u64 sys_addr)
{
	u32 swap_reg, swap_base, swap_limit, rgn_size, tmp_addr;

	if (boot_cpu_data.x86 == 0x10) {
		 
		if (boot_cpu_data.x86_model < 4 ||
		    (boot_cpu_data.x86_model < 0xa &&
		     boot_cpu_data.x86_mask < 3))
			return sys_addr;
	}

	amd64_read_dct_pci_cfg(pvt, SWAP_INTLV_REG, &swap_reg);

	if (!(swap_reg & 0x1))
		return sys_addr;

	swap_base	= (swap_reg >> 3) & 0x7f;
	swap_limit	= (swap_reg >> 11) & 0x7f;
	rgn_size	= (swap_reg >> 20) & 0x7f;
	tmp_addr	= sys_addr >> 27;

	if (!(sys_addr >> 34) &&
	    (((tmp_addr >= swap_base) &&
	     (tmp_addr <= swap_limit)) ||
	     (tmp_addr < rgn_size)))
		return sys_addr ^ (u64)swap_base << 27;

	return sys_addr;
}

static int f1x_match_to_this_node(struct amd64_pvt *pvt, unsigned range,
				  u64 sys_addr, int *chan_sel)
{
	int cs_found = -EINVAL;
	u64 chan_addr;
	u32 dct_sel_base;
	u8 channel;
	bool high_range = false;

	u8 node_id    = dram_dst_node(pvt, range);
	u8 intlv_en   = dram_intlv_en(pvt, range);
	u32 intlv_sel = dram_intlv_sel(pvt, range);

	edac_dbg(1, "(range %d) SystemAddr= 0x%llx Limit=0x%llx\n",
		 range, sys_addr, get_dram_limit(pvt, range));

	if (dhar_valid(pvt) &&
	    dhar_base(pvt) <= sys_addr &&
	    sys_addr < BIT_64(32)) {
		amd64_warn("Huh? Address is in the MMIO hole: 0x%016llx\n",
			    sys_addr);
		return -EINVAL;
	}

	if (intlv_en && (intlv_sel != ((sys_addr >> 12) & intlv_en)))
		return -EINVAL;

	sys_addr = f1x_swap_interleaved_region(pvt, sys_addr);

	dct_sel_base = dct_sel_baseaddr(pvt);

	if (dct_high_range_enabled(pvt) &&
	   !dct_ganging_enabled(pvt) &&
	   ((sys_addr >> 27) >= (dct_sel_base >> 11)))
		high_range = true;

	channel = f1x_determine_channel(pvt, sys_addr, high_range, intlv_en);

	chan_addr = f1x_get_norm_dct_addr(pvt, range, sys_addr,
					  high_range, dct_sel_base);

	if (intlv_en)
		chan_addr = ((chan_addr >> (12 + hweight8(intlv_en))) << 12) |
			    (chan_addr & 0xfff);

	if (dct_interleave_enabled(pvt) &&
	   !dct_high_range_enabled(pvt) &&
	   !dct_ganging_enabled(pvt)) {

		if (dct_sel_interleave_addr(pvt) != 1) {
			if (dct_sel_interleave_addr(pvt) == 0x3)
				 
				chan_addr = ((chan_addr >> 10) << 9) |
					     (chan_addr & 0x1ff);
			else
				 
				chan_addr = ((chan_addr >> 7) << 6) |
					     (chan_addr & 0x3f);
		} else
			 
			chan_addr = ((chan_addr >> 13) << 12) |
				     (chan_addr & 0xfff);
	}

	edac_dbg(1, "   Normalized DCT addr: 0x%llx\n", chan_addr);

	cs_found = f1x_lookup_addr_in_dct(chan_addr, node_id, channel);

	if (cs_found >= 0)
		*chan_sel = channel;

	return cs_found;
}

static int f1x_translate_sysaddr_to_cs(struct amd64_pvt *pvt, u64 sys_addr,
				       int *chan_sel)
{
	int cs_found = -EINVAL;
	unsigned range;

	for (range = 0; range < DRAM_RANGES; range++) {

		if (!dram_rw(pvt, range))
			continue;

		if ((get_dram_base(pvt, range)  <= sys_addr) &&
		    (get_dram_limit(pvt, range) >= sys_addr)) {

			cs_found = f1x_match_to_this_node(pvt, range,
							  sys_addr, chan_sel);
			if (cs_found >= 0)
				break;
		}
	}
	return cs_found;
}

static void f1x_map_sysaddr_to_csrow(struct mem_ctl_info *mci, u64 sys_addr,
				     struct err_info *err)
{
	struct amd64_pvt *pvt = mci->pvt_info;

	error_address_to_page_and_offset(sys_addr, err);

	err->csrow = f1x_translate_sysaddr_to_cs(pvt, sys_addr, &err->channel);
	if (err->csrow < 0) {
		err->err_code = ERR_CSROW;
		return;
	}

	if (dct_ganging_enabled(pvt))
		err->channel = get_channel_from_ecc_syndrome(mci, err->syndrome);
}

static void amd64_debug_display_dimm_sizes(struct amd64_pvt *pvt, u8 ctrl)
{
	int dimm, size0, size1;
	u32 *dcsb = ctrl ? pvt->csels[1].csbases : pvt->csels[0].csbases;
	u32 dbam  = ctrl ? pvt->dbam1 : pvt->dbam0;

	if (boot_cpu_data.x86 == 0xf) {
		 
	       if (pvt->ext_model < K8_REV_F)
			return;
	       else
		       WARN_ON(ctrl != 0);
	}

	dbam = (ctrl && !dct_ganging_enabled(pvt)) ? pvt->dbam1 : pvt->dbam0;
	dcsb = (ctrl && !dct_ganging_enabled(pvt)) ? pvt->csels[1].csbases
						   : pvt->csels[0].csbases;

	edac_dbg(1, "F2x%d80 (DRAM Bank Address Mapping): 0x%08x\n",
		 ctrl, dbam);

	edac_printk(KERN_DEBUG, EDAC_MC, "DCT%d chip selects:\n", ctrl);

	for (dimm = 0; dimm < 4; dimm++) {

		size0 = 0;
		if (dcsb[dimm*2] & DCSB_CS_ENABLE)
			size0 = pvt->ops->dbam_to_cs(pvt, ctrl,
						     DBAM_DIMM(dimm, dbam));

		size1 = 0;
		if (dcsb[dimm*2 + 1] & DCSB_CS_ENABLE)
			size1 = pvt->ops->dbam_to_cs(pvt, ctrl,
						     DBAM_DIMM(dimm, dbam));

		amd64_info(EDAC_MC ": %d: %5dMB %d: %5dMB\n",
				dimm * 2,     size0,
				dimm * 2 + 1, size1);
	}
}

static struct amd64_family_type amd64_family_types[] = {
	[K8_CPUS] = {
		.ctl_name = "K8",
		.f1_id = PCI_DEVICE_ID_AMD_K8_NB_ADDRMAP,
		.f3_id = PCI_DEVICE_ID_AMD_K8_NB_MISC,
		.ops = {
			.early_channel_count	= k8_early_channel_count,
			.map_sysaddr_to_csrow	= k8_map_sysaddr_to_csrow,
			.dbam_to_cs		= k8_dbam_to_chip_select,
			.read_dct_pci_cfg	= k8_read_dct_pci_cfg,
		}
	},
	[F10_CPUS] = {
		.ctl_name = "F10h",
		.f1_id = PCI_DEVICE_ID_AMD_10H_NB_MAP,
		.f3_id = PCI_DEVICE_ID_AMD_10H_NB_MISC,
		.ops = {
			.early_channel_count	= f1x_early_channel_count,
			.map_sysaddr_to_csrow	= f1x_map_sysaddr_to_csrow,
			.dbam_to_cs		= f10_dbam_to_chip_select,
			.read_dct_pci_cfg	= f10_read_dct_pci_cfg,
		}
	},
	[F15_CPUS] = {
		.ctl_name = "F15h",
		.f1_id = PCI_DEVICE_ID_AMD_15H_NB_F1,
		.f3_id = PCI_DEVICE_ID_AMD_15H_NB_F3,
		.ops = {
			.early_channel_count	= f1x_early_channel_count,
			.map_sysaddr_to_csrow	= f1x_map_sysaddr_to_csrow,
			.dbam_to_cs		= f15_dbam_to_chip_select,
			.read_dct_pci_cfg	= f15_read_dct_pci_cfg,
		}
	},
	[F16_CPUS] = {
		.ctl_name = "F16h",
		.f1_id = PCI_DEVICE_ID_AMD_16H_NB_F1,
		.f3_id = PCI_DEVICE_ID_AMD_16H_NB_F3,
		.ops = {
			.early_channel_count	= f1x_early_channel_count,
			.map_sysaddr_to_csrow	= f1x_map_sysaddr_to_csrow,
			.dbam_to_cs		= f16_dbam_to_chip_select,
			.read_dct_pci_cfg	= f10_read_dct_pci_cfg,
		}
	},
};

static const u16 x4_vectors[] = {
	0x2f57, 0x1afe, 0x66cc, 0xdd88,
	0x11eb, 0x3396, 0x7f4c, 0xeac8,
	0x0001, 0x0002, 0x0004, 0x0008,
	0x1013, 0x3032, 0x4044, 0x8088,
	0x106b, 0x30d6, 0x70fc, 0xe0a8,
	0x4857, 0xc4fe, 0x13cc, 0x3288,
	0x1ac5, 0x2f4a, 0x5394, 0xa1e8,
	0x1f39, 0x251e, 0xbd6c, 0x6bd8,
	0x15c1, 0x2a42, 0x89ac, 0x4758,
	0x2b03, 0x1602, 0x4f0c, 0xca08,
	0x1f07, 0x3a0e, 0x6b04, 0xbd08,
	0x8ba7, 0x465e, 0x244c, 0x1cc8,
	0x2b87, 0x164e, 0x642c, 0xdc18,
	0x40b9, 0x80de, 0x1094, 0x20e8,
	0x27db, 0x1eb6, 0x9dac, 0x7b58,
	0x11c1, 0x2242, 0x84ac, 0x4c58,
	0x1be5, 0x2d7a, 0x5e34, 0xa718,
	0x4b39, 0x8d1e, 0x14b4, 0x28d8,
	0x4c97, 0xc87e, 0x11fc, 0x33a8,
	0x8e97, 0x497e, 0x2ffc, 0x1aa8,
	0x16b3, 0x3d62, 0x4f34, 0x8518,
	0x1e2f, 0x391a, 0x5cac, 0xf858,
	0x1d9f, 0x3b7a, 0x572c, 0xfe18,
	0x15f5, 0x2a5a, 0x5264, 0xa3b8,
	0x1dbb, 0x3b66, 0x715c, 0xe3f8,
	0x4397, 0xc27e, 0x17fc, 0x3ea8,
	0x1617, 0x3d3e, 0x6464, 0xb8b8,
	0x23ff, 0x12aa, 0xab6c, 0x56d8,
	0x2dfb, 0x1ba6, 0x913c, 0x7328,
	0x185d, 0x2ca6, 0x7914, 0x9e28,
	0x171b, 0x3e36, 0x7d7c, 0xebe8,
	0x4199, 0x82ee, 0x19f4, 0x2e58,
	0x4807, 0xc40e, 0x130c, 0x3208,
	0x1905, 0x2e0a, 0x5804, 0xac08,
	0x213f, 0x132a, 0xadfc, 0x5ba8,
	0x19a9, 0x2efe, 0xb5cc, 0x6f88,
};

static const u16 x8_vectors[] = {
	0x0145, 0x028a, 0x2374, 0x43c8, 0xa1f0, 0x0520, 0x0a40, 0x1480,
	0x0211, 0x0422, 0x0844, 0x1088, 0x01b0, 0x44e0, 0x23c0, 0xed80,
	0x1011, 0x0116, 0x022c, 0x0458, 0x08b0, 0x8c60, 0x2740, 0x4e80,
	0x0411, 0x0822, 0x1044, 0x0158, 0x02b0, 0x2360, 0x46c0, 0xab80,
	0x0811, 0x1022, 0x012c, 0x0258, 0x04b0, 0x4660, 0x8cc0, 0x2780,
	0x2071, 0x40e2, 0xa0c4, 0x0108, 0x0210, 0x0420, 0x0840, 0x1080,
	0x4071, 0x80e2, 0x0104, 0x0208, 0x0410, 0x0820, 0x1040, 0x2080,
	0x8071, 0x0102, 0x0204, 0x0408, 0x0810, 0x1020, 0x2040, 0x4080,
	0x019d, 0x03d6, 0x136c, 0x2198, 0x50b0, 0xb2e0, 0x0740, 0x0e80,
	0x0189, 0x03ea, 0x072c, 0x0e58, 0x1cb0, 0x56e0, 0x37c0, 0xf580,
	0x01fd, 0x0376, 0x06ec, 0x0bb8, 0x1110, 0x2220, 0x4440, 0x8880,
	0x0163, 0x02c6, 0x1104, 0x0758, 0x0eb0, 0x2be0, 0x6140, 0xc280,
	0x02fd, 0x01c6, 0x0b5c, 0x1108, 0x07b0, 0x25a0, 0x8840, 0x6180,
	0x0801, 0x012e, 0x025c, 0x04b8, 0x1370, 0x26e0, 0x57c0, 0xb580,
	0x0401, 0x0802, 0x015c, 0x02b8, 0x22b0, 0x13e0, 0x7140, 0xe280,
	0x0201, 0x0402, 0x0804, 0x01b8, 0x11b0, 0x31a0, 0x8040, 0x7180,
	0x0101, 0x0202, 0x0404, 0x0808, 0x1010, 0x2020, 0x4040, 0x8080,
	0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
	0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000,
};

static int decode_syndrome(u16 syndrome, const u16 *vectors, unsigned num_vecs,
			   unsigned v_dim)
{
	unsigned int i, err_sym;

	for (err_sym = 0; err_sym < num_vecs / v_dim; err_sym++) {
		u16 s = syndrome;
		unsigned v_idx =  err_sym * v_dim;
		unsigned v_end = (err_sym + 1) * v_dim;

		for (i = 1; i < (1U << 16); i <<= 1) {

			if (v_idx < v_end && vectors[v_idx] & i) {
				u16 ev_comp = vectors[v_idx++];

				if (s & i) {
					 
					s ^= ev_comp;

					if (!s)
						return err_sym;
				}

			} else if (s & i)
				 
				break;
		}
	}

	edac_dbg(0, "syndrome(%x) not found\n", syndrome);
	return -1;
}

static int map_err_sym_to_channel(int err_sym, int sym_size)
{
	if (sym_size == 4)
		switch (err_sym) {
		case 0x20:
		case 0x21:
			return 0;
			break;
		case 0x22:
		case 0x23:
			return 1;
			break;
		default:
			return err_sym >> 4;
			break;
		}
	 
	else
		switch (err_sym) {
		 
		case 0x10:
			WARN(1, KERN_ERR "Invalid error symbol: 0x%x\n",
					  err_sym);
			return -1;
			break;

		case 0x11:
			return 0;
			break;
		case 0x12:
			return 1;
			break;
		default:
			return err_sym >> 3;
			break;
		}
	return -1;
}

static int get_channel_from_ecc_syndrome(struct mem_ctl_info *mci, u16 syndrome)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	int err_sym = -1;

	if (pvt->ecc_sym_sz == 8)
		err_sym = decode_syndrome(syndrome, x8_vectors,
					  ARRAY_SIZE(x8_vectors),
					  pvt->ecc_sym_sz);
	else if (pvt->ecc_sym_sz == 4)
		err_sym = decode_syndrome(syndrome, x4_vectors,
					  ARRAY_SIZE(x4_vectors),
					  pvt->ecc_sym_sz);
	else {
		amd64_warn("Illegal syndrome type: %u\n", pvt->ecc_sym_sz);
		return err_sym;
	}

	return map_err_sym_to_channel(err_sym, pvt->ecc_sym_sz);
}

static void __log_bus_error(struct mem_ctl_info *mci, struct err_info *err,
			    u8 ecc_type)
{
	enum hw_event_mc_err_type err_type;
	const char *string;

	if (ecc_type == 2)
		err_type = HW_EVENT_ERR_CORRECTED;
	else if (ecc_type == 1)
		err_type = HW_EVENT_ERR_UNCORRECTED;
	else {
		WARN(1, "Something is rotten in the state of Denmark.\n");
		return;
	}

	switch (err->err_code) {
	case DECODE_OK:
		string = "";
		break;
	case ERR_NODE:
		string = "Failed to map error addr to a node";
		break;
	case ERR_CSROW:
		string = "Failed to map error addr to a csrow";
		break;
	case ERR_CHANNEL:
		string = "unknown syndrome - possible error reporting race";
		break;
	default:
		string = "WTF error";
		break;
	}

	edac_mc_handle_error(err_type, mci, 1,
			     err->page, err->offset, err->syndrome,
			     err->csrow, err->channel, -1,
			     string, "");
}

static inline void __amd64_decode_bus_error(struct mem_ctl_info *mci,
					    struct mce *m)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	u8 ecc_type = (m->status >> 45) & 0x3;
	u8 xec = XEC(m->status, 0x1f);
	u16 ec = EC(m->status);
	u64 sys_addr;
	struct err_info err;

	if (PP(ec) == NBSL_PP_OBS)
		return;

	if (xec && xec != F10_NBSL_EXT_ERR_ECC)
		return;

	memset(&err, 0, sizeof(err));

	sys_addr = get_error_address(m);

	if (ecc_type == 2)
		err.syndrome = extract_syndrome(m->status);

	pvt->ops->map_sysaddr_to_csrow(mci, sys_addr, &err);

	__log_bus_error(mci, &err, ecc_type);
}

void amd64_decode_bus_error(int node_id, struct mce *m)
{
	__amd64_decode_bus_error(mcis[node_id], m);
}

static int reserve_mc_sibling_devs(struct amd64_pvt *pvt, u16 f1_id, u16 f3_id)
{
	 
	pvt->F1 = pci_get_related_function(pvt->F2->vendor, f1_id, pvt->F2);
	if (!pvt->F1) {
		amd64_err("error address map device not found: "
			  "vendor %x device 0x%x (broken BIOS?)\n",
			  PCI_VENDOR_ID_AMD, f1_id);
		return -ENODEV;
	}

	pvt->F3 = pci_get_related_function(pvt->F2->vendor, f3_id, pvt->F2);
	if (!pvt->F3) {
		pci_dev_put(pvt->F1);
		pvt->F1 = NULL;

		amd64_err("error F3 device not found: "
			  "vendor %x device 0x%x (broken BIOS?)\n",
			  PCI_VENDOR_ID_AMD, f3_id);

		return -ENODEV;
	}
	edac_dbg(1, "F1: %s\n", pci_name(pvt->F1));
	edac_dbg(1, "F2: %s\n", pci_name(pvt->F2));
	edac_dbg(1, "F3: %s\n", pci_name(pvt->F3));

	return 0;
}

static void free_mc_sibling_devs(struct amd64_pvt *pvt)
{
	pci_dev_put(pvt->F1);
	pci_dev_put(pvt->F3);
}

static void read_mc_regs(struct amd64_pvt *pvt)
{
	struct cpuinfo_x86 *c = &boot_cpu_data;
	u64 msr_val;
	u32 tmp;
	unsigned range;

	rdmsrl(MSR_K8_TOP_MEM1, pvt->top_mem);
	edac_dbg(0, "  TOP_MEM:  0x%016llx\n", pvt->top_mem);

	rdmsrl(MSR_K8_SYSCFG, msr_val);
	if (msr_val & (1U << 21)) {
		rdmsrl(MSR_K8_TOP_MEM2, pvt->top_mem2);
		edac_dbg(0, "  TOP_MEM2: 0x%016llx\n", pvt->top_mem2);
	} else
		edac_dbg(0, "  TOP_MEM2 disabled\n");

	amd64_read_pci_cfg(pvt->F3, NBCAP, &pvt->nbcap);

	read_dram_ctl_register(pvt);

	for (range = 0; range < DRAM_RANGES; range++) {
		u8 rw;

		read_dram_base_limit_regs(pvt, range);

		rw = dram_rw(pvt, range);
		if (!rw)
			continue;

		edac_dbg(1, "  DRAM range[%d], base: 0x%016llx; limit: 0x%016llx\n",
			 range,
			 get_dram_base(pvt, range),
			 get_dram_limit(pvt, range));

		edac_dbg(1, "   IntlvEn=%s; Range access: %s%s IntlvSel=%d DstNode=%d\n",
			 dram_intlv_en(pvt, range) ? "Enabled" : "Disabled",
			 (rw & 0x1) ? "R" : "-",
			 (rw & 0x2) ? "W" : "-",
			 dram_intlv_sel(pvt, range),
			 dram_dst_node(pvt, range));
	}

	read_dct_base_mask(pvt);

	amd64_read_pci_cfg(pvt->F1, DHAR, &pvt->dhar);
	amd64_read_dct_pci_cfg(pvt, DBAM0, &pvt->dbam0);

	amd64_read_pci_cfg(pvt->F3, F10_ONLINE_SPARE, &pvt->online_spare);

	amd64_read_dct_pci_cfg(pvt, DCLR0, &pvt->dclr0);
	amd64_read_dct_pci_cfg(pvt, DCHR0, &pvt->dchr0);

	if (!dct_ganging_enabled(pvt)) {
		amd64_read_dct_pci_cfg(pvt, DCLR1, &pvt->dclr1);
		amd64_read_dct_pci_cfg(pvt, DCHR1, &pvt->dchr1);
	}

	pvt->ecc_sym_sz = 4;

	if (c->x86 >= 0x10) {
		amd64_read_pci_cfg(pvt->F3, EXT_NB_MCA_CFG, &tmp);
		if (c->x86 != 0x16)
			 
			amd64_read_dct_pci_cfg(pvt, DBAM1, &pvt->dbam1);

		if ((c->x86 > 0x10 || c->x86_model > 7) && tmp & BIT(25))
			pvt->ecc_sym_sz = 8;
	}
	dump_misc_regs(pvt);
}

static u32 amd64_csrow_nr_pages(struct amd64_pvt *pvt, u8 dct, int csrow_nr)
{
	u32 cs_mode, nr_pages;
	u32 dbam = dct ? pvt->dbam1 : pvt->dbam0;

	cs_mode = DBAM_DIMM(csrow_nr / 2, dbam);

	nr_pages = pvt->ops->dbam_to_cs(pvt, dct, cs_mode) << (20 - PAGE_SHIFT);

	edac_dbg(0, "csrow: %d, channel: %d, DBAM idx: %d\n",
		    csrow_nr, dct,  cs_mode);
	edac_dbg(0, "nr_pages/channel: %u\n", nr_pages);

	return nr_pages;
}

static int init_csrows(struct mem_ctl_info *mci)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	struct csrow_info *csrow;
	struct dimm_info *dimm;
	enum edac_type edac_mode;
	enum mem_type mtype;
	int i, j, empty = 1;
	int nr_pages = 0;
	u32 val;

	amd64_read_pci_cfg(pvt->F3, NBCFG, &val);

	pvt->nbcfg = val;

	edac_dbg(0, "node %d, NBCFG=0x%08x[ChipKillEccCap: %d|DramEccEn: %d]\n",
		 pvt->mc_node_id, val,
		 !!(val & NBCFG_CHIPKILL), !!(val & NBCFG_ECC_ENABLE));

	for_each_chip_select(i, 0, pvt) {
		bool row_dct0 = !!csrow_enabled(i, 0, pvt);
		bool row_dct1 = false;

		if (boot_cpu_data.x86 != 0xf)
			row_dct1 = !!csrow_enabled(i, 1, pvt);

		if (!row_dct0 && !row_dct1)
			continue;

		csrow = mci->csrows[i];
		empty = 0;

		edac_dbg(1, "MC node: %d, csrow: %d\n",
			    pvt->mc_node_id, i);

		if (row_dct0) {
			nr_pages = amd64_csrow_nr_pages(pvt, 0, i);
			csrow->channels[0]->dimm->nr_pages = nr_pages;
		}

		if (boot_cpu_data.x86 != 0xf && row_dct1) {
			int row_dct1_pages = amd64_csrow_nr_pages(pvt, 1, i);

			csrow->channels[1]->dimm->nr_pages = row_dct1_pages;
			nr_pages += row_dct1_pages;
		}

		mtype = amd64_determine_memory_type(pvt, i);

		edac_dbg(1, "Total csrow%d pages: %u\n", i, nr_pages);

		if (pvt->nbcfg & NBCFG_ECC_ENABLE)
			edac_mode = (pvt->nbcfg & NBCFG_CHIPKILL) ?
				    EDAC_S4ECD4ED : EDAC_SECDED;
		else
			edac_mode = EDAC_NONE;

		for (j = 0; j < pvt->channel_count; j++) {
			dimm = csrow->channels[j]->dimm;
			dimm->mtype = mtype;
			dimm->edac_mode = edac_mode;
		}
	}

	return empty;
}

static void get_cpus_on_this_dct_cpumask(struct cpumask *mask, u16 nid)
{
	int cpu;

	for_each_online_cpu(cpu)
		if (amd_get_nb_id(cpu) == nid)
			cpumask_set_cpu(cpu, mask);
}

static bool amd64_nb_mce_bank_enabled_on_node(u16 nid)
{
	cpumask_var_t mask;
	int cpu, nbe;
	bool ret = false;

	if (!zalloc_cpumask_var(&mask, GFP_KERNEL)) {
		amd64_warn("%s: Error allocating mask\n", __func__);
		return false;
	}

	get_cpus_on_this_dct_cpumask(mask, nid);

	rdmsr_on_cpus(mask, MSR_IA32_MCG_CTL, msrs);

	for_each_cpu(cpu, mask) {
		struct msr *reg = per_cpu_ptr(msrs, cpu);
		nbe = reg->l & MSR_MCGCTL_NBE;

		edac_dbg(0, "core: %u, MCG_CTL: 0x%llx, NB MSR is %s\n",
			 cpu, reg->q,
			 (nbe ? "enabled" : "disabled"));

		if (!nbe)
			goto out;
	}
	ret = true;

out:
	free_cpumask_var(mask);
	return ret;
}

static int toggle_ecc_err_reporting(struct ecc_settings *s, u16 nid, bool on)
{
	cpumask_var_t cmask;
	int cpu;

	if (!zalloc_cpumask_var(&cmask, GFP_KERNEL)) {
		amd64_warn("%s: error allocating mask\n", __func__);
		return false;
	}

	get_cpus_on_this_dct_cpumask(cmask, nid);

	rdmsr_on_cpus(cmask, MSR_IA32_MCG_CTL, msrs);

	for_each_cpu(cpu, cmask) {

		struct msr *reg = per_cpu_ptr(msrs, cpu);

		if (on) {
			if (reg->l & MSR_MCGCTL_NBE)
				s->flags.nb_mce_enable = 1;

			reg->l |= MSR_MCGCTL_NBE;
		} else {
			 
			if (!s->flags.nb_mce_enable)
				reg->l &= ~MSR_MCGCTL_NBE;
		}
	}
	wrmsr_on_cpus(cmask, MSR_IA32_MCG_CTL, msrs);

	free_cpumask_var(cmask);

	return 0;
}

static bool enable_ecc_error_reporting(struct ecc_settings *s, u16 nid,
				       struct pci_dev *F3)
{
	bool ret = true;
	u32 value, mask = 0x3;		 

	if (toggle_ecc_err_reporting(s, nid, ON)) {
		amd64_warn("Error enabling ECC reporting over MCGCTL!\n");
		return false;
	}

	amd64_read_pci_cfg(F3, NBCTL, &value);

	s->old_nbctl   = value & mask;
	s->nbctl_valid = true;

	value |= mask;
	amd64_write_pci_cfg(F3, NBCTL, value);

	amd64_read_pci_cfg(F3, NBCFG, &value);

	edac_dbg(0, "1: node %d, NBCFG=0x%08x[DramEccEn: %d]\n",
		 nid, value, !!(value & NBCFG_ECC_ENABLE));

	if (!(value & NBCFG_ECC_ENABLE)) {
		amd64_warn("DRAM ECC disabled on this node, enabling...\n");

		s->flags.nb_ecc_prev = 0;

		value |= NBCFG_ECC_ENABLE;
		amd64_write_pci_cfg(F3, NBCFG, value);

		amd64_read_pci_cfg(F3, NBCFG, &value);

		if (!(value & NBCFG_ECC_ENABLE)) {
			amd64_warn("Hardware rejected DRAM ECC enable,"
				   "check memory DIMM configuration.\n");
			ret = false;
		} else {
			amd64_info("Hardware accepted DRAM ECC Enable\n");
		}
	} else {
		s->flags.nb_ecc_prev = 1;
	}

	edac_dbg(0, "2: node %d, NBCFG=0x%08x[DramEccEn: %d]\n",
		 nid, value, !!(value & NBCFG_ECC_ENABLE));

	return ret;
}

static void restore_ecc_error_reporting(struct ecc_settings *s, u16 nid,
					struct pci_dev *F3)
{
	u32 value, mask = 0x3;		 

	if (!s->nbctl_valid)
		return;

	amd64_read_pci_cfg(F3, NBCTL, &value);
	value &= ~mask;
	value |= s->old_nbctl;

	amd64_write_pci_cfg(F3, NBCTL, value);

	if (!s->flags.nb_ecc_prev) {
		amd64_read_pci_cfg(F3, NBCFG, &value);
		value &= ~NBCFG_ECC_ENABLE;
		amd64_write_pci_cfg(F3, NBCFG, value);
	}

	if (toggle_ecc_err_reporting(s, nid, OFF))
		amd64_warn("Error restoring NB MCGCTL settings!\n");
}

static const char *ecc_msg =
	"ECC disabled in the BIOS or no ECC capability, module will not load.\n"
	" Either enable ECC checking or force module loading by setting "
	"'ecc_enable_override'.\n"
	" (Note that use of the override may cause unknown side effects.)\n";

static bool ecc_enabled(struct pci_dev *F3, u16 nid)
{
	u32 value;
	u8 ecc_en = 0;
	bool nb_mce_en = false;

	amd64_read_pci_cfg(F3, NBCFG, &value);

	ecc_en = !!(value & NBCFG_ECC_ENABLE);
	amd64_info("DRAM ECC %s.\n", (ecc_en ? "enabled" : "disabled"));

	nb_mce_en = amd64_nb_mce_bank_enabled_on_node(nid);
	if (!nb_mce_en)
		amd64_notice("NB MCE bank disabled, set MSR "
			     "0x%08x[4] on node %d to enable.\n",
			     MSR_IA32_MCG_CTL, nid);

	if (!ecc_en || !nb_mce_en) {
		amd64_notice("%s", ecc_msg);
		return false;
	}
	return true;
}

static int set_mc_sysfs_attrs(struct mem_ctl_info *mci)
{
	int rc;

	rc = amd64_create_sysfs_dbg_files(mci);
	if (rc < 0)
		return rc;

	if (boot_cpu_data.x86 >= 0x10) {
		rc = amd64_create_sysfs_inject_files(mci);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static void del_mc_sysfs_attrs(struct mem_ctl_info *mci)
{
	amd64_remove_sysfs_dbg_files(mci);

	if (boot_cpu_data.x86 >= 0x10)
		amd64_remove_sysfs_inject_files(mci);
}

static void setup_mci_misc_attrs(struct mem_ctl_info *mci,
				 struct amd64_family_type *fam)
{
	struct amd64_pvt *pvt = mci->pvt_info;

	mci->mtype_cap		= MEM_FLAG_DDR2 | MEM_FLAG_RDDR2;
	mci->edac_ctl_cap	= EDAC_FLAG_NONE;

	if (pvt->nbcap & NBCAP_SECDED)
		mci->edac_ctl_cap |= EDAC_FLAG_SECDED;

	if (pvt->nbcap & NBCAP_CHIPKILL)
		mci->edac_ctl_cap |= EDAC_FLAG_S4ECD4ED;

	mci->edac_cap		= amd64_determine_edac_cap(pvt);
	mci->mod_name		= EDAC_MOD_STR;
	mci->mod_ver		= EDAC_AMD64_VERSION;
	mci->ctl_name		= fam->ctl_name;
	mci->dev_name		= pci_name(pvt->F2);
	mci->ctl_page_to_phys	= NULL;

	mci->set_sdram_scrub_rate = amd64_set_scrub_rate;
	mci->get_sdram_scrub_rate = amd64_get_scrub_rate;
}

static struct amd64_family_type *amd64_per_family_init(struct amd64_pvt *pvt)
{
	u8 fam = boot_cpu_data.x86;
	struct amd64_family_type *fam_type = NULL;

	switch (fam) {
	case 0xf:
		fam_type		= &amd64_family_types[K8_CPUS];
		pvt->ops		= &amd64_family_types[K8_CPUS].ops;
		break;

	case 0x10:
		fam_type		= &amd64_family_types[F10_CPUS];
		pvt->ops		= &amd64_family_types[F10_CPUS].ops;
		break;

	case 0x15:
		fam_type		= &amd64_family_types[F15_CPUS];
		pvt->ops		= &amd64_family_types[F15_CPUS].ops;
		break;

	case 0x16:
		fam_type		= &amd64_family_types[F16_CPUS];
		pvt->ops		= &amd64_family_types[F16_CPUS].ops;
		break;

	default:
		amd64_err("Unsupported family!\n");
		return NULL;
	}

	pvt->ext_model = boot_cpu_data.x86_model >> 4;

	amd64_info("%s %sdetected (node %d).\n", fam_type->ctl_name,
		     (fam == 0xf ?
				(pvt->ext_model >= K8_REV_F  ? "revF or later "
							     : "revE or earlier ")
				 : ""), pvt->mc_node_id);
	return fam_type;
}

static int amd64_init_one_instance(struct pci_dev *F2)
{
	struct amd64_pvt *pvt = NULL;
	struct amd64_family_type *fam_type = NULL;
	struct mem_ctl_info *mci = NULL;
	struct edac_mc_layer layers[2];
	int err = 0, ret;
	u16 nid = amd_get_node_id(F2);

	ret = -ENOMEM;
	pvt = kzalloc(sizeof(struct amd64_pvt), GFP_KERNEL);
	if (!pvt)
		goto err_ret;

	pvt->mc_node_id	= nid;
	pvt->F2 = F2;

	ret = -EINVAL;
	fam_type = amd64_per_family_init(pvt);
	if (!fam_type)
		goto err_free;

	ret = -ENODEV;
	err = reserve_mc_sibling_devs(pvt, fam_type->f1_id, fam_type->f3_id);
	if (err)
		goto err_free;

	read_mc_regs(pvt);

	ret = -EINVAL;
	pvt->channel_count = pvt->ops->early_channel_count(pvt);
	if (pvt->channel_count < 0)
		goto err_siblings;

	ret = -ENOMEM;
	layers[0].type = EDAC_MC_LAYER_CHIP_SELECT;
	layers[0].size = pvt->csels[0].b_cnt;
	layers[0].is_virt_csrow = true;
	layers[1].type = EDAC_MC_LAYER_CHANNEL;

	layers[1].size = 2;
	layers[1].is_virt_csrow = false;

	mci = edac_mc_alloc(nid, ARRAY_SIZE(layers), layers, 0);
	if (!mci)
		goto err_siblings;

	mci->pvt_info = pvt;
	mci->pdev = &pvt->F2->dev;

	setup_mci_misc_attrs(mci, fam_type);

	if (init_csrows(mci))
		mci->edac_cap = EDAC_FLAG_NONE;

	ret = -ENODEV;
	if (edac_mc_add_mc(mci)) {
		edac_dbg(1, "failed edac_mc_add_mc()\n");
		goto err_add_mc;
	}
	if (set_mc_sysfs_attrs(mci)) {
		edac_dbg(1, "failed edac_mc_add_mc()\n");
		goto err_add_sysfs;
	}

	if (report_gart_errors)
		amd_report_gart_errors(true);

	amd_register_ecc_decoder(amd64_decode_bus_error);

	mcis[nid] = mci;

	atomic_inc(&drv_instances);

	return 0;

err_add_sysfs:
	edac_mc_del_mc(mci->pdev);
err_add_mc:
	edac_mc_free(mci);

err_siblings:
	free_mc_sibling_devs(pvt);

err_free:
	kfree(pvt);

err_ret:
	return ret;
}

static int amd64_probe_one_instance(struct pci_dev *pdev,
				    const struct pci_device_id *mc_type)
{
	u16 nid = amd_get_node_id(pdev);
	struct pci_dev *F3 = node_to_amd_nb(nid)->misc;
	struct ecc_settings *s;
	int ret = 0;

	ret = pci_enable_device(pdev);
	if (ret < 0) {
		edac_dbg(0, "ret=%d\n", ret);
		return -EIO;
	}

	ret = -ENOMEM;
	s = kzalloc(sizeof(struct ecc_settings), GFP_KERNEL);
	if (!s)
		goto err_out;

	ecc_stngs[nid] = s;

	if (!ecc_enabled(F3, nid)) {
		ret = -ENODEV;

		if (!ecc_enable_override)
			goto err_enable;

		amd64_warn("Forcing ECC on!\n");

		if (!enable_ecc_error_reporting(s, nid, F3))
			goto err_enable;
	}

	ret = amd64_init_one_instance(pdev);
	if (ret < 0) {
		amd64_err("Error probing instance: %d\n", nid);
		restore_ecc_error_reporting(s, nid, F3);
	}

	return ret;

err_enable:
	kfree(s);
	ecc_stngs[nid] = NULL;

err_out:
	return ret;
}

static void amd64_remove_one_instance(struct pci_dev *pdev)
{
	struct mem_ctl_info *mci;
	struct amd64_pvt *pvt;
	u16 nid = amd_get_node_id(pdev);
	struct pci_dev *F3 = node_to_amd_nb(nid)->misc;
	struct ecc_settings *s = ecc_stngs[nid];

	mci = find_mci_by_dev(&pdev->dev);
	del_mc_sysfs_attrs(mci);
	 
	mci = edac_mc_del_mc(&pdev->dev);
	if (!mci)
		return;

	pvt = mci->pvt_info;

	restore_ecc_error_reporting(s, nid, F3);

	free_mc_sibling_devs(pvt);

	amd_report_gart_errors(false);
	amd_unregister_ecc_decoder(amd64_decode_bus_error);

	kfree(ecc_stngs[nid]);
	ecc_stngs[nid] = NULL;

	mci->pvt_info = NULL;
	mcis[nid] = NULL;

	kfree(pvt);
	edac_mc_free(mci);
}

static DEFINE_PCI_DEVICE_TABLE(amd64_pci_table) = {
	{
		.vendor		= PCI_VENDOR_ID_AMD,
		.device		= PCI_DEVICE_ID_AMD_K8_NB_MEMCTL,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.class		= 0,
		.class_mask	= 0,
	},
	{
		.vendor		= PCI_VENDOR_ID_AMD,
		.device		= PCI_DEVICE_ID_AMD_10H_NB_DRAM,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.class		= 0,
		.class_mask	= 0,
	},
	{
		.vendor		= PCI_VENDOR_ID_AMD,
		.device		= PCI_DEVICE_ID_AMD_15H_NB_F2,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.class		= 0,
		.class_mask	= 0,
	},
	{
		.vendor		= PCI_VENDOR_ID_AMD,
		.device		= PCI_DEVICE_ID_AMD_16H_NB_F2,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.class		= 0,
		.class_mask	= 0,
	},

	{0, }
};
MODULE_DEVICE_TABLE(pci, amd64_pci_table);

static struct pci_driver amd64_pci_driver = {
	.name		= EDAC_MOD_STR,
	.probe		= amd64_probe_one_instance,
	.remove		= amd64_remove_one_instance,
	.id_table	= amd64_pci_table,
};

static void setup_pci_device(void)
{
	struct mem_ctl_info *mci;
	struct amd64_pvt *pvt;

	if (amd64_ctl_pci)
		return;

	mci = mcis[0];
	if (mci) {

		pvt = mci->pvt_info;
		amd64_ctl_pci =
			edac_pci_create_generic_ctl(&pvt->F2->dev, EDAC_MOD_STR);

		if (!amd64_ctl_pci) {
			pr_warning("%s(): Unable to create PCI control\n",
				   __func__);

			pr_warning("%s(): PCI error report via EDAC not set\n",
				   __func__);
			}
	}
}

static int __init amd64_edac_init(void)
{
	int err = -ENODEV;

	printk(KERN_INFO "AMD64 EDAC driver v%s\n", EDAC_AMD64_VERSION);

	opstate_init();

	if (amd_cache_northbridges() < 0)
		goto err_ret;

	err = -ENOMEM;
	mcis	  = kzalloc(amd_nb_num() * sizeof(mcis[0]), GFP_KERNEL);
	ecc_stngs = kzalloc(amd_nb_num() * sizeof(ecc_stngs[0]), GFP_KERNEL);
	if (!(mcis && ecc_stngs))
		goto err_free;

	msrs = msrs_alloc();
	if (!msrs)
		goto err_free;

	err = pci_register_driver(&amd64_pci_driver);
	if (err)
		goto err_pci;

	err = -ENODEV;
	if (!atomic_read(&drv_instances))
		goto err_no_instances;

	setup_pci_device();
	return 0;

err_no_instances:
	pci_unregister_driver(&amd64_pci_driver);

err_pci:
	msrs_free(msrs);
	msrs = NULL;

err_free:
	kfree(mcis);
	mcis = NULL;

	kfree(ecc_stngs);
	ecc_stngs = NULL;

err_ret:
	return err;
}

static void __exit amd64_edac_exit(void)
{
	if (amd64_ctl_pci)
		edac_pci_release_generic_ctl(amd64_ctl_pci);

	pci_unregister_driver(&amd64_pci_driver);

	kfree(ecc_stngs);
	ecc_stngs = NULL;

	kfree(mcis);
	mcis = NULL;

	msrs_free(msrs);
	msrs = NULL;
}

module_init(amd64_edac_init);
module_exit(amd64_edac_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SoftwareBitMaker: Doug Thompson, "
		"Dave Peterson, Thayne Harbaugh");
MODULE_DESCRIPTION("MC support for AMD64 memory controllers - "
		EDAC_AMD64_VERSION);

module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting state: 0=Poll,1=NMI");
