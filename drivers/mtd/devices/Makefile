#
# linux/drivers/mtd/devices/Makefile
#

obj-$(CONFIG_MTD_DOCG3)		+= docg3.o
obj-$(CONFIG_MTD_SLRAM)		+= slram.o
obj-$(CONFIG_MTD_PHRAM)		+= phram.o
obj-$(CONFIG_MTD_PMC551)	+= pmc551.o
obj-$(CONFIG_MTD_MS02NV)	+= ms02-nv.o
obj-$(CONFIG_MTD_MTDRAM)	+= mtdram.o
obj-$(CONFIG_MTD_LART)		+= lart.o
obj-$(CONFIG_MTD_BLOCK2MTD)	+= block2mtd.o
obj-$(CONFIG_MTD_DATAFLASH)	+= mtd_dataflash.o
obj-$(CONFIG_MTD_M25P80)	+= m25p80.o
obj-$(CONFIG_MTD_NAND_OMAP_BCH)	+= elm.o
obj-$(CONFIG_MTD_SPEAR_SMI)	+= spear_smi.o
obj-$(CONFIG_MTD_SST25L)	+= sst25l.o
obj-$(CONFIG_MTD_BCM47XXSFLASH)	+= bcm47xxsflash.o
obj-$(CONFIG_MTD_STM_SPI_FSM)	+= stm_spi_fsm.o

ifeq ($(CONFIG_SYNO_LSP_HI3536),y)
obj-$(CONFIG_MTD_SPI_IDS)	+= spi_ids.o
obj-$(CONFIG_MTD_HISFC350)	+= hisfc350/
obj-$(CONFIG_HIFMC_SPI_NOR)	+= hifmc100/
endif # CONFIG_SYNO_LSP_HI3536

CFLAGS_docg3.o			+= -I$(src)
