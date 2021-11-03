/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Qualcomm MSM8976 interconnect IDs
 *
 * Copyright (c) 2021, AngeloGioacchino Del Regno
 *                     <angelogioacchino.delregno@somainline.org>
 */

#ifndef __DT_BINDINGS_INTERCONNECT_QCOM_MSM8976_H
#define __DT_BINDINGS_INTERCONNECT_QCOM_MSM8976_H

/* BIMC */
#define MASTER_APPS_PROC	0
#define MASTER_SMMNOC_BIMC	1
#define MASTER_SNOC_BIMC	2
#define MASTER_TCU0		3
#define SLAVE_BIMC_SNOC		4
#define SLAVE_EBI		5

/* PCNOC */
#define MASTER_BLSP_1		0
#define MASTER_BLSP_2		1
#define MASTER_CRYPTO_C0	2
#define MASTER_DEHR		3
#define MASTER_LPASS_AHB	4
#define MASTER_SDCC_1		5
#define MASTER_SDCC_2		6
#define MASTER_SDCC_3		7
#define MASTER_SNOC_PCNOC	8
#define MASTER_SPDM		9
#define MASTER_USB_HS1		10
#define MASTER_USB_HS2		11
#define MASTER_XM_USB_HS1	12
#define MASTER_PCNOC_M_0	13
#define MASTER_PCNOC_M_1	14
#define MASTER_PCNOC_INT_0	15
#define MASTER_PCNOC_INT_1	16
#define MASTER_PCNOC_INT_2	17
#define MASTER_PCNOC_S_1	18
#define MASTER_PCNOC_S_2	19
#define MASTER_PCNOC_S_3	20
#define MASTER_PCNOC_S_4	21
#define MASTER_PCNOC_S_8	22
#define MASTER_PCNOC_S_9	23
#define SLAVE_BLSP_1		24
#define SLAVE_BLSP_2		25
#define SLAVE_CAMERA_SS_CFG	26
#define SLAVE_CRYPTO_0_CFG	27
#define SLAVE_DCC_CFG		28
#define SLAVE_DISP_SS_CFG	29
#define SLAVE_GPU_CFG		30
#define SLAVE_MESSAGE_RAM	31
#define SLAVE_PDM		32
#define SLAVE_PMIC_ARB		33
#define SLAVE_SNOC_CFG		34
#define SLAVE_PCNOC_SNOC	35
#define SLAVE_PRNG		36
#define SLAVE_SDCC_1		37
#define SLAVE_SDCC_2		38
#define SLAVE_SDCC_3		39
#define SLAVE_TCSR		40
#define SLAVE_TLMM		41
#define SLAVE_USB_HS		42
#define SLAVE_USB_HS2		43
#define SLAVE_VENUS_CFG		44

/* SNOC */
#define MASTER_BIMC_SNOC	0
#define MASTER_CPP		1
#define MASTER_IPA		2
#define MASTER_LPASS_PROC	3
#define MASTER_JPEG		4
#define MASTER_MDP_P0		5
#define MASTER_MDP_P1		6
#define MASTER_MM_INT_0		7
#define MASTER_OXILI		8
#define MASTER_PCNOC_SNOC	9
#define MASTER_QDSS_BAM		10
#define MASTER_QDSS_ETR		11
#define MASTER_QDSS_INT		12
#define MASTER_SNOC_INT_0	13
#define MASTER_SNOC_INT_1	14
#define MASTER_SNOC_INT_2	15
#define MASTER_VENUS_0		16
#define MASTER_VENUS_1		17
#define MASTER_VFE_0		18
#define MASTER_VFE_1		19
#define SLAVE_CATS_0		20
#define SLAVE_CATS_1		21
#define SLAVE_KPSS_AHB		22
#define SLAVE_LPASS		23
#define SLAVE_QDSS_STM		24
#define SLAVE_SMMNOC_BIMC	25
#define SLAVE_SNOC_BIMC		26
#define SLAVE_SNOC_PCNOC	27
#define SLAVE_IMEM		28

#endif
