/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-08-15
 *
******************************************************************************/
#ifndef HINFC620_GENH
#define HINFC620_GENH
/******************************************************************************/

#include "../hinfc_gen.h"

enum hinfc620_ecc_reg {
	hinfc620_ecc_none   = 0x00,
	hinfc620_ecc_8bit   = 0x02,
	hinfc620_ecc_16bit  = 0x03,
	hinfc620_ecc_24bit  = 0x04,
	hinfc620_ecc_40bit  = 0x05,
	hinfc620_ecc_64bit  = 0x06,
	hinfc620_ecc_28bit  = 0x07,
	hinfc620_ecc_42bit  = 0x08,
};

enum hinfc620_page_reg {
	hinfc620_pagesize_2K    = 0x01,
	hinfc620_pagesize_4K    = 0x02,
	hinfc620_pagesize_8K    = 0x03,
	hinfc620_pagesize_16K   = 0x04,
	hinfc620_pagesize_32K   = 0x05,
};

enum hinfc620_page_reg hinfc620_page_type2reg(int type);

int hinfc620_page_reg2type(enum hinfc620_page_reg reg);

enum hinfc620_ecc_reg hinfc620_ecc_type2reg(int type);

int hinfc620_ecc_reg2type(enum hinfc620_ecc_reg reg);

/******************************************************************************/
#endif /* HINFC620_GENH */
