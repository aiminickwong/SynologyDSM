#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include "../tal/tal.h"
#include "tdm_if.h"
#include "../tal/tal_dev.h"
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <plat/drv_dxt_if.h>
#include <plat/zarlink_if.h>
#include <plat/silabs_if.h>
#ifndef CONFIG_OF
#include "gpp/mvGppRegs.h"
#include "ctrlEnv/mvCtrlEnvLib.h"
#endif

#define TDM_STOP_MAX_POLLING_TIME 20  

static irqreturn_t tdm_if_isr(int irq, void* dev_id);

static void tdm_if_pcm_start(void);
static void tdm_if_pcm_stop(void);

#if !(defined CONFIG_MV_PHONE_USE_IRQ_PROCESSING) && !(defined CONFIG_MV_PHONE_USE_FIQ_PROCESSING)
static void tdm_if_pcm_rx_process(unsigned long arg);
static void tdm_if_pcm_tx_process(unsigned long arg);
#else
static inline void tdm_if_pcm_rx_process(void);
static inline void tdm_if_pcm_tx_process(void);
#endif
 
#ifndef CONFIG_OF
static int proc_tdm_init_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr);
static int proc_rx_miss_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr);
static int proc_tx_miss_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr);
static int proc_rx_over_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr);
static int proc_tx_under_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr);
#ifdef CONFIG_MV_TDM_EXT_STATS
static int proc_dump_ext_stats(char *buffer, char **buffer_location, off_t offset,
				int buffer_length, int *zero, void *ptr);
#endif
#endif  

static void tdm2c_if_stop_channels(unsigned long args);

static int __init tdm_if_module_init(void);
static void __exit tdm_if_module_exit(void);

#if !(defined CONFIG_MV_PHONE_USE_IRQ_PROCESSING) && !(defined CONFIG_MV_PHONE_USE_FIQ_PROCESSING)
static DECLARE_TASKLET(tdm_if_rx_tasklet, tdm_if_pcm_rx_process, 0);
static DECLARE_TASKLET(tdm_if_tx_tasklet, tdm_if_pcm_tx_process, 0);
#endif
static DECLARE_TASKLET(tdm2c_if_stop_tasklet, tdm2c_if_stop_channels, 0);
static DEFINE_SPINLOCK(tdm_if_lock);
static unsigned char *rxBuff = NULL, *txBuff = NULL;
static char irqnr;
static unsigned int rx_miss = 0, tx_miss = 0;
static unsigned int rx_over = 0, tx_under = 0;
static struct proc_dir_entry *tdm_stats;
static int pcm_enable = 0;
static int irq_init = 0;
static int tdm_init = 0;
static int buff_size = 0;
static unsigned short test_enable = 0;
#ifdef CONFIG_MV_TDM_EXT_STATS
static unsigned int pcm_stop_fail;
#endif
static int pcm_stop_flag;
static int pcm_stop_status;
static unsigned int pcm_start_stop_state;
static unsigned int is_pcm_stopping;
static unsigned int mv_tdm_unit_type;

#ifdef CONFIG_OF
static int proc_tdm_status_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_MV_TDM_EXT_STATS
	MV_TDM_EXTENDED_STATS tdm_ext_stats;
#endif

	seq_printf(m, "tdm_init:	%u\n", tdm_init);
	seq_printf(m, "rx_miss:		%u\n", rx_miss);
	seq_printf(m, "tx_miss:		%u\n", tx_miss);
	seq_printf(m, "rx_over:		%u\n", rx_over);
	seq_printf(m, "tx_under:	%u\n", tx_under);

#ifdef CONFIG_MV_TDM_EXT_STATS
	mvTdmExtStatsGet(&tdm_ext_stats);

	seq_printf(m, "\nTDM Extended Statistics:\n");
	seq_printf(m, "intRxCount	= %u\n", tdm_ext_stats.intRxCount);
	seq_printf(m, "intTxCount	= %u\n", tdm_ext_stats.intTxCount);
	seq_printf(m, "intRx0Count	= %u\n", tdm_ext_stats.intRx0Count);
	seq_printf(m, "intTx0Count	= %u\n", tdm_ext_stats.intTx0Count);
	seq_printf(m, "intRx1Count	= %u\n", tdm_ext_stats.intRx1Count);
	seq_printf(m, "intTx1Count	= %u\n", tdm_ext_stats.intTx1Count);
	seq_printf(m, "intRx0Miss	= %u\n", tdm_ext_stats.intRx0Miss);
	seq_printf(m, "intTx0Miss	= %u\n", tdm_ext_stats.intTx0Miss);
	seq_printf(m, "intRx1Miss	= %u\n", tdm_ext_stats.intRx1Miss);
	seq_printf(m, "intTx1Miss	= %u\n", tdm_ext_stats.intTx1Miss);
	seq_printf(m, "pcmRestartCount	= %u\n", tdm_ext_stats.pcmRestartCount);
	seq_printf(m, "pcm_stop_fail	= %u\n", pcm_stop_fail);
#endif

	return 0;
}

static int proc_tdm_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_tdm_status_show, PDE_DATA(inode));
}

static const struct file_operations proc_tdm_operations = {
	.open		= proc_tdm_status_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#else
static int proc_tdm_init_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr)
{
	return sprintf(buffer, "%u\n", tdm_init);
}

static int proc_rx_miss_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr)
{
	return sprintf(buffer, "%u\n", rx_miss);
}

static int proc_tx_miss_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr)
{
	return sprintf(buffer, "%u\n", tx_miss);
}

static int proc_rx_over_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr)
{
	return sprintf(buffer, "%u\n", rx_over);
}

static int proc_tx_under_read(char *buffer, char **buffer_location, off_t offset,
                            int buffer_length, int *zero, void *ptr)
{
	return sprintf(buffer, "%u\n", tx_under);
}
#endif  

static void tdm_if_unit_type_set(unsigned int tdm_unit)
{
	unsigned long flags;

	TRC_REC("->%s\n", __func__);

	spin_lock_irqsave(&tdm_if_lock, flags);

	mv_tdm_unit_type = tdm_unit;
	spin_unlock_irqrestore(&tdm_if_lock, flags);

	TRC_REC("<-%s\n", __func__);
	return;
}

static unsigned int tdm_if_unit_type_get(void)
{
	return mv_tdm_unit_type;
}

#ifndef CONFIG_OF
#ifdef CONFIG_MV_TDM_EXT_STATS
static int proc_dump_ext_stats(char *buffer, char **buffer_location, off_t offset,
				int buffer_length, int *zero, void *ptr)
{
	char *str;
	MV_TDM_EXTENDED_STATS tdm_ext_stats;

	if (offset > 0)
		return 0;

	mvTdmExtStatsGet(&tdm_ext_stats);

	str = buffer;
	str += sprintf(str, "\nTDM Extended Statistics:\n");
	str += sprintf(str, "intRxCount = %u\n", tdm_ext_stats.intRxCount);
	str += sprintf(str, "intTxCount = %u\n", tdm_ext_stats.intTxCount);
	str += sprintf(str, "intRx0Count = %u\n", tdm_ext_stats.intRx0Count);
	str += sprintf(str, "intTx0Count = %u\n", tdm_ext_stats.intTx0Count);
	str += sprintf(str, "intRx1Count = %u\n", tdm_ext_stats.intRx1Count);
	str += sprintf(str, "intTx1Count = %u\n", tdm_ext_stats.intTx1Count);
	str += sprintf(str, "intRx0Miss = %u\n", tdm_ext_stats.intRx0Miss);
	str += sprintf(str, "intTx0Miss	= %u\n", tdm_ext_stats.intTx0Miss);
	str += sprintf(str, "intRx1Miss	= %u\n", tdm_ext_stats.intRx1Miss);
	str += sprintf(str, "intTx1Miss	= %u\n", tdm_ext_stats.intTx1Miss);
	str += sprintf(str, "pcmRestartCount = %u\n", tdm_ext_stats.pcmRestartCount);
	str += sprintf(str, "pcm_stop_fail = %u\n", pcm_stop_fail);

	return (int)(str - buffer);
}
#endif
#endif  

MV_STATUS tdm_if_init(tal_params_t *tal_params)
{
	MV_TDM_PARAMS tdm_params;

	if (tdm_init) {
		printk(KERN_INFO "Marvell Telephony Driver already started...\n");
		return MV_OK;
	}

	printk(KERN_INFO "Loading Marvell Telephony Driver\n");

#ifndef CONFIG_OF
	 
	if (mvCtrlSocUnitInfoNumGet(TDM_UNIT_ID) == 0) {
		mvCtrlPwrClckSet(TDM_UNIT_ID, 0, MV_FALSE);
		printk(KERN_WARNING "%s: Warning, no SLIC module is connected\n", __func__);
		return MV_OK;
	}
#endif

	if (tal_params == NULL) {
		printk(KERN_ERR "%s: bad parameters\n", __func__);
		return MV_ERROR;

	}

	rxBuff = txBuff = NULL;
	irq_init = 0;
	tdm_init = 0;

#ifdef CONFIG_MV_TDM2C_SUPPORT
	if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get()) {

		pcm_enable = 0;
		is_pcm_stopping = 0;
		pcm_stop_flag = 0;
		pcm_stop_status = 0;
	}
#endif
#ifdef CONFIG_MV_TDMMC_SUPPORT
	if (MV_TDM_UNIT_TDMMC == tdm_if_unit_type_get())
		pcm_enable = 1;
#endif

#ifdef CONFIG_MV_TDM_EXT_STATS
	pcm_stop_fail = 0;
#endif

	buff_size = (tal_params->pcm_format * tal_params->total_lines * 80 *
			(tal_params->sampling_period/MV_TDM_BASE_SAMPLING_PERIOD));

	irqnr = mvCtrlTdmUnitIrqGet();

	TRC_INIT();
	TRC_START();
	TRC_REC("->%s\n", __func__);

	memcpy(&tdm_params, tal_params, sizeof(MV_TDM_PARAMS));

#if defined(MY_ABC_HERE)
	 
#else  
	 
#ifdef CONFIG_MV_TDM2C_SUPPORT
	if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get())
		mvTdmPcmIfReset();
#endif
#endif  

	if (mvSysTdmInit(&tdm_params) != MV_OK) {
			printk(KERN_ERR "%s: Error, TDM initialization failed !!!\n", __func__);
			return MV_ERROR;
	}
	tdm_init = 1;

#if defined(MY_ABC_HERE)
	 
#ifdef CONFIG_MV_TDM2C_SUPPORT
	if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get())
		mvTdmPcmIfReset();
#endif
#endif  

#ifdef CONFIG_MV_PHONE_USE_FIQ_PROCESSING
	if (request_fiq(irqnr, tdm_if_isr, IRQF_DISABLED, "tdm", NULL)) {
		printk(KERN_ERR "%s: Failed to connect fiq(%d)\n", __func__, irqnr);
		return MV_ERROR;
	}
#else  
	if (request_irq(irqnr, tdm_if_isr, IRQF_DISABLED, "tdm", NULL)) {
		printk(KERN_ERR "%s: Failed to connect irq(%d)\n", __func__, irqnr);
		return MV_ERROR;
	}
#endif  

	irq_init = 1;

	tdm_stats = proc_mkdir("tdm", NULL);
	if (tdm_stats != NULL) {
#ifdef CONFIG_OF
		if (!proc_create("tdm_stats", S_IRUGO, tdm_stats, &proc_tdm_operations))
			return -ENOMEM;
#else
		create_proc_read_entry("tdm_init", 0, tdm_stats, proc_tdm_init_read, NULL);
		create_proc_read_entry("rx_miss", 0, tdm_stats, proc_rx_miss_read, NULL);
		create_proc_read_entry("tx_miss", 0, tdm_stats, proc_tx_miss_read, NULL);
		create_proc_read_entry("rx_over", 0, tdm_stats, proc_rx_over_read, NULL);
		create_proc_read_entry("tx_under", 0, tdm_stats, proc_tx_under_read, NULL);
#ifdef CONFIG_MV_TDM_EXT_STATS
		create_proc_read_entry("tdm_extended_stats", 0, tdm_stats, proc_dump_ext_stats, NULL);
#endif
#endif  
	}

	TRC_REC("Marvell Telephony Driver Loaded Successfully\n");

#ifdef CONFIG_MV_TDMMC_SUPPORT
	if (MV_TDM_UNIT_TDMMC == tdm_if_unit_type_get())
		tdm_if_pcm_stop();
#endif
	TRC_REC("<-%s\n", __func__);
	return MV_OK;
}

void tdm_if_exit(void)
{
	u32 max_poll = 0;

	if (!irq_init && !pcm_enable && !tdm_init)
		return;

	TRC_REC("->%s\n", __func__);

	if (pcm_enable)
		tdm_if_pcm_stop();

#ifdef CONFIG_MV_TDM2C_SUPPORT
		if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get()) {
			while ((is_pcm_stopping != 0) && (max_poll < 20)) {
				mdelay(1);
				max_poll++;
			}

			if (max_poll >= 20)
				printk(KERN_WARNING "%s: waiting for pcm channels to stop exceeded 20ms\n", __func__);
		}
#endif

	if (irq_init) {
		 
#ifndef CONFIG_MV_PHONE_USE_FIQ_PROCESSING
		free_irq(irqnr, NULL);
#else  
		free_fiq(irqnr, NULL);
#endif  
		irq_init = 0;
	}

	if (tdm_init) {
#ifdef CONFIG_MV_TDM2C_SUPPORT
		if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get())
			mvTdmRelease();
#endif
#ifdef CONFIG_MV_TDMMC_SUPPORT
		if (MV_TDM_UNIT_TDMMC == tdm_if_unit_type_get())
			mvCommUnitRelease();
#endif
		 
#ifdef CONFIG_OF
		remove_proc_entry("tdm_stats", tdm_stats);
#else
		remove_proc_entry("tdm_init", tdm_stats);
		remove_proc_entry("rx_miss", tdm_stats);
		remove_proc_entry("tx_miss", tdm_stats);
		remove_proc_entry("rx_over", tdm_stats);
		remove_proc_entry("tx_under", tdm_stats);
#ifdef CONFIG_MV_TDM_EXT_STATS
		remove_proc_entry("tdm_extended_stats", tdm_stats);
#endif
#endif  
		remove_proc_entry("tdm", NULL);

		tdm_init = 0;
	}

	TRC_REC("<-%s\n", __func__);

	TRC_OUTPUT(0, 1);
	TRC_RELEASE();
}

static void tdm_if_pcm_start(void)
{
	unsigned long flags;
	u32 max_poll = 0;

	TRC_REC("->%s\n", __func__);

	spin_lock_irqsave(&tdm_if_lock, flags);
	if (!pcm_enable) {
		pcm_enable = 1;
#ifdef CONFIG_MV_TDM2C_SUPPORT
		if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get()) {
			if (is_pcm_stopping == 0) {
				pcm_stop_flag = 0;
				pcm_stop_status = 0;
				pcm_start_stop_state = 0;
				rxBuff = txBuff = NULL;
				mvTdmPcmStart();
			} else {
				pcm_start_stop_state++;
				while (is_pcm_stopping && max_poll < TDM_STOP_MAX_POLLING_TIME) {
					spin_unlock_irqrestore(&tdm_if_lock, flags);
					mdelay(1);
					max_poll++;
					spin_lock_irqsave(&tdm_if_lock, flags);
				}

				if (is_pcm_stopping) { 
					if (mvPcmStopIntMiss())
						TRC_REC("pcm stop issue found\n");
					else
						TRC_REC("pcm stop timeout\n");
						is_pcm_stopping = 0;
						pcm_stop_flag = 0;
						pcm_stop_status = 0;
						pcm_start_stop_state = 0;
						rxBuff = txBuff = NULL;
						mvTdmPcmStart();
				} else {
					TRC_REC("pcm_start_stop_state(%d), max_poll=%d\n",
						pcm_start_stop_state, max_poll);
				}
			}
		}
#endif
#ifdef CONFIG_MV_TDMMC_SUPPORT
		if (MV_TDM_UNIT_TDMMC == tdm_if_unit_type_get()) {
			rxBuff = txBuff = NULL;
			mvCommUnitPcmStart();
		}
#endif
	}
	spin_unlock_irqrestore(&tdm_if_lock, flags);

	TRC_REC("<-%s\n", __func__);
	return;
}

static void tdm_if_pcm_stop(void)
{
	unsigned long flags;

	TRC_REC("->%s\n", __func__);

	spin_lock_irqsave(&tdm_if_lock, flags);
	if (pcm_enable) {
		pcm_enable = 0;
#ifdef CONFIG_MV_TDM2C_SUPPORT
		if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get()) {
			if (is_pcm_stopping == 0) {
				is_pcm_stopping = 1;
				mvTdmPcmStop();
			} else {
				pcm_start_stop_state--;
				TRC_REC("pcm_start_stop_state(%d)\n", pcm_start_stop_state);
			}
		}
#endif
#ifdef CONFIG_MV_TDMMC_SUPPORT
		if (MV_TDM_UNIT_TDMMC == tdm_if_unit_type_get())
			mvCommUnitPcmStop();
#endif
	}
	spin_unlock_irqrestore(&tdm_if_lock, flags);

	TRC_REC("<-%s\n", __func__);
	return;
}

static irqreturn_t tdm_if_isr(int irq, void* dev_id)
{
	MV_TDM_INT_INFO tdm_int_info;
	unsigned int int_type;
	int ret = 0;

	TRC_REC("->%s\n", __func__);

#ifdef CONFIG_MV_TDM2C_SUPPORT
	if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get())
		ret = mvTdmIntLow(&tdm_int_info);
#endif
#ifdef CONFIG_MV_TDMMC_SUPPORT
	if (MV_TDM_UNIT_TDMMC == tdm_if_unit_type_get())
		ret = mvCommUnitIntLow(&tdm_int_info);
#endif

	int_type = tdm_int_info.intType;
	 
	if (MV_BOARD_SLIC_ZSI_ID == mvBoardSlicUnitTypeGet())
		zarlink_if_zsi_interrupt();
	 
	else if (MV_BOARD_SLIC_ISI_ID == mvBoardSlicUnitTypeGet())
		silabs_if_isi_interrupt();

	if (int_type == MV_EMPTY_INT)
		goto out;

#ifdef CONFIG_MV_TDM2C_SUPPORT
	if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get()) {
		if ((ret == -1) && (pcm_stop_status == 0))	{
			pcm_stop_status = 1;

			if ((!rxBuff) && (!txBuff)) {
				TRC_REC("Stopping the TDM\n");
				tdm_if_pcm_stop();
				pcm_stop_flag = 0;
				tasklet_hi_schedule(&tdm2c_if_stop_tasklet);
			} else {
				TRC_REC("Some tasklet is running, mark pcm_stop_flag\n");
				pcm_stop_flag = 1;
			}
		}

		if (pcm_stop_status)
			goto skip_rx_tx;
	}
#endif

	if (int_type & MV_RX_INT) {
		if (rxBuff != NULL) {
			rx_miss++;
			TRC_REC("%s: Warning, missed Rx buffer processing !!!\n", __func__);
		} else {
			rxBuff = tdm_int_info.tdmRxBuff;
#if (defined CONFIG_MV_PHONE_USE_IRQ_PROCESSING) || (defined CONFIG_MV_PHONE_USE_FIQ_PROCESSING)
			TRC_REC("%s: running Rx in ISR\n", __func__);
			tdm_if_pcm_rx_process();
#else
			 
			TRC_REC("%s: schedule Rx tasklet\n", __func__);
			tasklet_hi_schedule(&tdm_if_rx_tasklet);
#endif
		}
	}

	if (int_type & MV_TX_INT) {
		if (txBuff != NULL) {
			tx_miss++;
			TRC_REC("%s: Warning, missed Tx buffer processing !!!\n", __func__);
		} else {
			txBuff = tdm_int_info.tdmTxBuff;
#if (defined CONFIG_MV_PHONE_USE_IRQ_PROCESSING) || (defined CONFIG_MV_PHONE_USE_FIQ_PROCESSING)
			TRC_REC("%s: running Tx in ISR\n", __func__);
			tdm_if_pcm_tx_process();
#else
			 
			TRC_REC("%s: schedule Tx tasklet\n", __func__);
			tasklet_hi_schedule(&tdm_if_tx_tasklet);
#endif
		}
	}

#ifdef CONFIG_MV_TDM2C_SUPPORT
	if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get()) {
		 
		if ((int_type & MV_CHAN_STOP_INT) && (tdm_int_info.data == 4)) {
			TRC_REC("%s: Received MV_CHAN_STOP_INT indication\n", __func__);
			is_pcm_stopping = 0;
			if (pcm_start_stop_state) {
				TRC_REC("%s: calling to tdm_if_pcm_start()\n", __func__);
				pcm_enable = 0;
				tdm_if_pcm_start();
			}
		}
	}
#endif

skip_rx_tx:

	if (int_type & MV_PHONE_INT) {
		 
		drv_dxt_if_signal_interrupt();
	}

	if (int_type & MV_ERROR_INT) {
		if (int_type & MV_RX_ERROR_INT)
			rx_over++;

		if (int_type & MV_TX_ERROR_INT)
			tx_under++;
	}

out:
	TRC_REC("<-%s\n", __func__);
	return IRQ_HANDLED;
}
#if (defined CONFIG_MV_PHONE_USE_IRQ_PROCESSING) || (defined CONFIG_MV_PHONE_USE_FIQ_PROCESSING)
static inline void tdm_if_pcm_rx_process(void)
#else
 
static void tdm_if_pcm_rx_process(unsigned long arg)
#endif
{
	unsigned long flags;
	unsigned int tdm_type;

	TRC_REC("->%s\n", __func__);
	tdm_type = tdm_if_unit_type_get();
	if (pcm_enable) {
		if(rxBuff == NULL) {
			TRC_REC("%s: Error, empty Rx processing\n", __func__);
			return;
		}
#ifdef CONFIG_MV_TDM2C_SUPPORT
		 
		if (MV_TDM_UNIT_TDM2C == tdm_type) {
			if (mvTdmRx(rxBuff) == MV_OK)
				tal_mmp_rx(rxBuff, buff_size);  
			else
				printk(KERN_WARNING "%s: could not fill Rx buffer\n", __func__);
		}
#endif
#ifdef CONFIG_MV_TDMMC_SUPPORT
		if (MV_TDM_UNIT_TDMMC == tdm_type) {
			if (mvCommUnitRx(rxBuff) == MV_OK) {
				tal_mmp_rx(rxBuff, buff_size);  
				 
				if (pcm_enable)
					mvOsCacheInvalidate(NULL, rxBuff, buff_size);
			} else
				printk(KERN_WARNING "%s: could not fill Rx buffer\n", __func__);
		}
#endif
	}

	spin_lock_irqsave(&tdm_if_lock, flags);
	 
	rxBuff = NULL;
	spin_unlock_irqrestore(&tdm_if_lock, flags);

#ifdef CONFIG_MV_TDM2C_SUPPORT
	if (MV_TDM_UNIT_TDM2C == tdm_type) {
		if ((pcm_stop_flag == 1) && !txBuff) {
			TRC_REC("Stopping TDM from Rx tasklet\n");
			tdm_if_pcm_stop();
			spin_lock_irqsave(&tdm_if_lock, flags);
			pcm_stop_flag = 0;
			spin_unlock_irqrestore(&tdm_if_lock, flags);
			tasklet_hi_schedule(&tdm2c_if_stop_tasklet);
		}
	}
#endif

	TRC_REC("<-%s\n", __func__);
	return;
}

#if (defined CONFIG_MV_PHONE_USE_IRQ_PROCESSING) || (defined CONFIG_MV_PHONE_USE_FIQ_PROCESSING)
static inline void tdm_if_pcm_tx_process(void)
#else
 
static void tdm_if_pcm_tx_process(unsigned long arg)
#endif
{
	unsigned long flags;
	unsigned int tdm_type;

	TRC_REC("->%s\n", __func__);
	tdm_type = tdm_if_unit_type_get();

	if (pcm_enable) {
		if (txBuff == NULL) {
			TRC_REC("%s: Error, empty Tx processing\n", __func__);
			return;
		}

		tal_mmp_tx(txBuff, buff_size);

		if (test_enable == 0) {
#ifdef CONFIG_MV_TDM2C_SUPPORT
			 
			if (MV_TDM_UNIT_TDM2C == tdm_type) {
				if (mvTdmTx(txBuff) != MV_OK)
					printk(KERN_WARNING "%s: could not fill Tx buffer\n", __func__);
			}
#endif
#ifdef CONFIG_MV_TDMMC_SUPPORT
			if (MV_TDM_UNIT_TDMMC == tdm_type) {
				if (mvCommUnitTx(txBuff) != MV_OK)
					printk(KERN_WARNING "%s: could not fill Tx buffer\n", __func__);
			}
#endif
		}
	}

	spin_lock_irqsave(&tdm_if_lock, flags);
	 
	txBuff = NULL;
	spin_unlock_irqrestore(&tdm_if_lock, flags);

#ifdef CONFIG_MV_TDM2C_SUPPORT
	if (MV_TDM_UNIT_TDM2C == tdm_type) {
		if ((pcm_stop_flag == 1) && !rxBuff) {
			TRC_REC("Stopping TDM from Tx tasklet\n");
			tdm_if_pcm_stop();
			spin_lock_irqsave(&tdm_if_lock, flags);
			pcm_stop_flag = 0;
			spin_unlock_irqrestore(&tdm_if_lock, flags);
			tasklet_hi_schedule(&tdm2c_if_stop_tasklet);
		}
	}
#endif

	TRC_REC("<-%s\n", __func__);
	return;
}

void tdm_if_stats_get(tal_stats_t *tdm_if_stats)
{
	if (tdm_init == 0)
		return;

	tdm_if_stats->tdm_init = tdm_init;
	tdm_if_stats->rx_miss = rx_miss;
	tdm_if_stats->tx_miss = tx_miss;
	tdm_if_stats->rx_over = rx_over;
	tdm_if_stats->tx_under = tx_under;
#ifdef CONFIG_MV_TDM_EXT_STATS
	mvTdmExtStatsGet(&tdm_if_stats->tdm_ext_stats);
#endif
	return;
}

static void tdm2c_if_stop_channels(unsigned long arg)
{
	u32 max_poll = 0;
	unsigned long flags;

	TRC_REC("->%s\n", __func__);

	while (((MV_REG_READ(CH_ENABLE_REG(0)) & 0x101) || (MV_REG_READ(CH_ENABLE_REG(1)) & 0x101)) && (max_poll < 30)) {
		mdelay(1);
		max_poll++;
	}

	TRC_REC("Finished polling on channels disable\n");
	if (max_poll >= 30) {
		MV_REG_WRITE(CH_ENABLE_REG(0), 0);
		MV_REG_WRITE(CH_ENABLE_REG(1), 0);
		printk(KERN_WARNING "\n\npolling on channels disabling exceeded 30ms\n\n");
#ifdef CONFIG_MV_TDM_EXT_STATS
		pcm_stop_fail++;
#endif
		mdelay(10);
	}

	spin_lock_irqsave(&tdm_if_lock, flags);
	is_pcm_stopping = 0;
	spin_unlock_irqrestore(&tdm_if_lock, flags);
	tdm_if_pcm_start();

	TRC_REC("<-%s\n", __func__);
	return;
}

static int tdm_if_control(int cmd, void *arg)
{
#ifdef MV_TDM_USE_DCO
	tdm_dev_clk_t *tdm_dev_clk = arg;
#endif

	switch (cmd) {
	case TDM_DEV_TDM_TEST_MODE_ENABLE:
		test_enable = 1;
		break;

	case TDM_DEV_TDM_TEST_MODE_DISABLE:
		test_enable = 0;
		break;

#ifdef MV_TDM_USE_DCO
	case TDM_DEV_TDM_CLK_CONFIG:
		mvCtrlTdmClkCtrlConfig();
		printk(KERN_INFO "ioctl: TDM_DEV_TDM_CLK_CONFIG\n");
		break;

	case TDM_DEV_TDM_CLK_GET:
		tdm_dev_clk->correction = mvCtrlTdmClkCtrlGet();
		printk(KERN_INFO "ioctl: TDM_DEV_TDM_CLK_GET\n");
		break;

	case TDM_DEV_TDM_CLK_SET:
		printk(KERN_INFO "ioctl: TDM_DEV_TDM_CLK_SET: %x\n", tdm_dev_clk->correction);
		mvCtrlTdmClkCtrlSet(tdm_dev_clk->correction);
		break;
#endif  
	default:
		return -EINVAL;
	};

	return 0;
}

static int tdm_if_write(unsigned char *buffer, int size)
{
	if (test_enable) {
#ifdef CONFIG_MV_TDM2C_SUPPORT
		if (MV_TDM_UNIT_TDM2C == tdm_if_unit_type_get())
			return mvTdmTx(buffer);
#endif
#ifdef CONFIG_MV_TDMMC_SUPPORT
		if (MV_TDM_UNIT_TDMMC == tdm_if_unit_type_get())
			return mvCommUnitTx(buffer);
#endif
	}
	return MV_OK;
}

static tal_if_t tdm_if = {
	.init		= tdm_if_init,
	.exit		= tdm_if_exit,
	.pcm_start	= tdm_if_pcm_start,
	.pcm_stop	= tdm_if_pcm_stop,
	.control	= tdm_if_control,
	.write		= tdm_if_write,
	.stats_get	= tdm_if_stats_get,
};

static int __init tdm_if_module_init(void)
{
	u32 tdmUnit;
	tdmUnit = mvCtrlTdmUnitTypeGet();
	if ((MV_TDM_UNIT_TDM2C == tdmUnit) ||
		MV_TDM_UNIT_TDMMC == tdmUnit) {
		tal_set_if(&tdm_if);
		tdm_if_unit_type_set(tdmUnit);
	}
	return 0;
}

static void __exit tdm_if_module_exit(void)
{
	u32 tdmUnit;
	tdmUnit = mvCtrlTdmUnitTypeGet();
	if ((MV_TDM_UNIT_TDM2C == tdmUnit) ||
		MV_TDM_UNIT_TDMMC == tdmUnit) {
		tal_set_if(NULL);
	}
	return;
}

module_init(tdm_if_module_init);
module_exit(tdm_if_module_exit);
MODULE_DESCRIPTION("Marvell TDM I/F Device Driver - www.marvell.com");
MODULE_AUTHOR("Eran Ben-Avi <benavi@marvell.com>");
MODULE_LICENSE("GPL");
