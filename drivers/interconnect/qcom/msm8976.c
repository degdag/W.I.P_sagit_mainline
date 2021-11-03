// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm MSM8956/8976 Network-on-Chip (NoC) QoS driver
 * Copyright (C) 2021, AngeloGioacchino Del Regno
 *                     <angelogioacchino.delregno@somainline.org>
 */

#include <dt-bindings/interconnect/qcom,msm8976.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include "smd-rpm.h"

#define RPM_BUS_MASTER_REQ	0x73616d62
#define RPM_BUS_SLAVE_REQ	0x766c7362

/* BIMC QoS */
#define M_BKE_REG_BASE(n)		(0x300 + (0x4000 * n))
#define M_BKE_EN_ADDR(n)		(M_BKE_REG_BASE(n))
#define M_BKE_HEALTH_CFG_ADDR(i, n)	(M_BKE_REG_BASE(n) + 0x40 + (0x4 * i))

#define M_BKE_HEALTH_CFG_LIMITCMDS_MASK	0x80000000
#define M_BKE_HEALTH_CFG_AREQPRIO_MASK	0x300
#define M_BKE_HEALTH_CFG_PRIOLVL_MASK	0x3
#define M_BKE_HEALTH_CFG_AREQPRIO_SHIFT	0x8
#define M_BKE_HEALTH_CFG_LIMITCMDS_SHIFT 0x1f

#define M_BKE_EN_EN_BMASK		0x1

/* Valid for both NoC and BIMC */
#define NOC_QOS_MODE_FIXED		0x0
#define NOC_QOS_MODE_LIMITER		0x1
#define NOC_QOS_MODE_BYPASS		0x2

/* NoC QoS */
#define NOC_PERM_MODE_FIXED		1
#define NOC_PERM_MODE_BYPASS		(1 << NOC_QOS_MODE_BYPASS)

#define NOC_QOS_PRIORITYn_ADDR(n)	(0x8 + (n * 0x1000))
#define NOC_QOS_PRIORITY_MASK		0xf
#define NOC_QOS_PRIORITY_P1_SHIFT	0x2
#define NOC_QOS_PRIORITY_P0_SHIFT	0x3

#define NOC_QOS_MODEn_ADDR(n)		(0xc + (n * 0x1000))
#define NOC_QOS_MODEn_MASK		0x3

enum {
	MSM8976_MASTER_APPS_PROC = 1,
	MSM8976_MASTER_BIMC_SNOC,
	MSM8976_MASTER_BLSP_1,
	MSM8976_MASTER_BLSP_2,
	MSM8976_MASTER_BLSP_3,
	MSM8976_MASTER_CPP,
	MSM8976_MASTER_CRYPTO_C0,
	MSM8976_MASTER_DEHR,
	MSM8976_MASTER_IPA,
	MSM8976_MASTER_JPEG,
	MSM8976_MASTER_LPASS_AHB,
	MSM8976_MASTER_LPASS_PROC,
	MSM8976_MASTER_MDP_P0,
	MSM8976_MASTER_MDP_P1,
	MSM8976_MASTER_MM_INT_0,
	MSM8976_MASTER_OXILI,
	MSM8976_MASTER_PCNOC_INT_0,
	MSM8976_MASTER_PCNOC_INT_1,
	MSM8976_MASTER_PCNOC_INT_2,
	MSM8976_MASTER_PCNOC_M_0,
	MSM8976_MASTER_PCNOC_M_1,
	MSM8976_MASTER_PCNOC_S_1,
	MSM8976_MASTER_PCNOC_S_2,
	MSM8976_MASTER_PCNOC_S_3,
	MSM8976_MASTER_PCNOC_S_4,
	MSM8976_MASTER_PCNOC_S_8,
	MSM8976_MASTER_PCNOC_S_9,
	MSM8976_MASTER_PCNOC_SNOC,
	MSM8976_MASTER_QDSS_BAM,
	MSM8976_MASTER_QDSS_ETR,
	MSM8976_MASTER_QDSS_INT,
	MSM8976_MASTER_SDCC_1,
	MSM8976_MASTER_SDCC_2,
	MSM8976_MASTER_SDCC_3,
	MSM8976_MASTER_SMMNOC_BIMC,
	MSM8976_MASTER_SNOC_BIMC,
	MSM8976_MASTER_SNOC_INT_0,
	MSM8976_MASTER_SNOC_INT_1,
	MSM8976_MASTER_SNOC_INT_2,
	MSM8976_MASTER_SNOC_PCNOC,
	MSM8976_MASTER_SPDM,
	MSM8976_MASTER_TCU0,
	MSM8976_MASTER_USB_HS1,
	MSM8976_MASTER_USB_HS2,
	MSM8976_MASTER_VENUS_0,
	MSM8976_MASTER_VENUS_1,
	MSM8976_MASTER_VFE_0,
	MSM8976_MASTER_VFE_1,
	MSM8976_MASTER_XM_USB_HS1,
	MSM8976_SLAVE_BIMC_SNOC,
	MSM8976_SLAVE_BLSP_1,
	MSM8976_SLAVE_BLSP_2,
	MSM8976_SLAVE_CAMERA_SS_CFG,
	MSM8976_SLAVE_CATS_0,
	MSM8976_SLAVE_CATS_1,
	MSM8976_SLAVE_CRYPTO_0_CFG,
	MSM8976_SLAVE_DCC_CFG,
	MSM8976_SLAVE_DISP_SS_CFG,
	MSM8976_SLAVE_EBI,
	MSM8976_SLAVE_GPU_CFG,
	MSM8976_SLAVE_IMEM,
	MSM8976_SLAVE_KPSS_AHB,
	MSM8976_SLAVE_LPASS,
	MSM8976_SLAVE_MESSAGE_RAM,
	MSM8976_SLAVE_PCNOC_SNOC,
	MSM8976_SLAVE_PDM,
	MSM8976_SLAVE_PMIC_ARB,
	MSM8976_SLAVE_PRNG,
	MSM8976_SLAVE_QDSS_STM,
	MSM8976_SLAVE_SDCC_1,
	MSM8976_SLAVE_SDCC_2,
	MSM8976_SLAVE_SDCC_3,
	MSM8976_SLAVE_SMMNOC_BIMC,
	MSM8976_SLAVE_SNOC_BIMC,
	MSM8976_SLAVE_SNOC_CFG,
	MSM8976_SLAVE_SNOC_PCNOC,
	MSM8976_SLAVE_TCSR,
	MSM8976_SLAVE_TLMM,
	MSM8976_SLAVE_USB_HS,
	MSM8976_SLAVE_USB_HS2,
	MSM8976_SLAVE_VENUS_CFG,

	MSM8976_BIMC,
	MSM8976_PCNOC,
	MSM8976_MNOC,
	MSM8976_SNOC,
};

#define to_qcom_provider(_provider) \
	container_of(_provider, struct qcom_icc_provider, provider)

static const struct clk_bulk_data bus_clocks[] = {
	{ .id = "bus" },
	{ .id = "bus_a" },
};

/**
 * struct qcom_icc_provider - Qualcomm specific interconnect provider
 * @provider: generic interconnect provider
 * @bus_clks: the clk_bulk_data table of bus clocks
 * @num_clks: the total number of clk_bulk_data entries
 * @is_bimc_node: indicates whether to use bimc specific setting
 * @regmap: regmap for QoS registers read/write access
 * @mmio: NoC base iospace
 */
struct qcom_icc_provider {
	struct icc_provider provider;
	struct clk_bulk_data *bus_clks;
	int num_clks;
	bool is_bimc_node;
	struct regmap *regmap;
	void __iomem *mmio;
};

#define MSM8976_MAX_LINKS	10

/**
 * struct qcom_icc_qos - Qualcomm specific interconnect QoS parameters
 * @areq_prio: node requests priority
 * @prio_level: priority level for bus communication
 * @limit_commands: activate/deactivate limiter mode during runtime
 * @ap_owned: indicates if the node is owned by the AP or by the RPM
 * @qos_mode: default qos mode for this node
 * @qos_port: qos port number for finding qos registers of this node
 */
struct qcom_icc_qos {
	u32 areq_prio;
	u32 prio_level;
	bool limit_commands;
	bool ap_owned;
	int qos_mode;
	int qos_port;
};

/**
 * struct qcom_icc_node - Qualcomm specific interconnect nodes
 * @name: the node name used in debugfs
 * @id: a unique node identifier
 * @links: an array of nodes where we can go next while traversing
 * @num_links: the total number of @links
 * @buswidth: width of the interconnect between a node and the bus (bytes)
 * @mas_rpm_id: RPM id for devices that are bus masters
 * @slv_rpm_id: RPM id for devices that are bus slaves
 * @qos: NoC QoS setting parameters
 * @rate: current bus clock rate in Hz
 */
struct qcom_icc_node {
	unsigned char *name;
	u16 id;
	u16 links[MSM8976_MAX_LINKS];
	u16 num_links;
	u16 buswidth;
	int mas_rpm_id;
	int slv_rpm_id;
	struct qcom_icc_qos qos;
	u64 rate;
};

struct qcom_icc_desc {
	struct qcom_icc_node **nodes;
	size_t num_nodes;
	const struct regmap_config *regmap_cfg;
};

static struct qcom_icc_node mas_apss_proc = {
	.name = "mas_apss_proc",
	.id = MSM8976_MASTER_APPS_PROC,
	.buswidth = 16,
	.mas_rpm_id = 0,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 2,
	.links = { MSM8976_SLAVE_EBI, MSM8976_SLAVE_BIMC_SNOC },
};

static struct qcom_icc_node mas_bimc_snoc = {
	.name = "mas_bimc_snoc",
	.id = MSM8976_MASTER_BIMC_SNOC,
	.buswidth = 8,
	.mas_rpm_id = 21,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_SNOC_INT_2 },
};

static struct qcom_icc_node mas_blsp_1 = {
	.name = "mas_blsp_1",
	.id = MSM8976_MASTER_BLSP_1,
	.buswidth = 4,
	.mas_rpm_id = 41,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_M_1 },
};

static struct qcom_icc_node mas_blsp_2 = {
	.name = "mas_blsp_2",
	.id = MSM8976_MASTER_BLSP_2,
	.buswidth = 4,
	.mas_rpm_id = 39,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_M_1 },
};

static struct qcom_icc_node mas_cpp = {
	.name = "mas_cpp",
	.id = MSM8976_MASTER_CPP,
	.buswidth = 16,
	.mas_rpm_id = 115,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 12,
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
};

static struct qcom_icc_node mas_crypto = {
	.name = "mas_crypto",
	.id = MSM8976_MASTER_CRYPTO_C0,
	.buswidth = 8,
	.mas_rpm_id = 23,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 0,
		.qos_port = 0,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_INT_1 },
};

static struct qcom_icc_node mas_dehr = {
	.name = "mas_dehr",
	.id = MSM8976_MASTER_DEHR,
	.buswidth = 4,
	.mas_rpm_id = 48,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_M_0 },
};

static struct qcom_icc_node mas_ipa = {
	.name = "mas_ipa",
	.id = MSM8976_MASTER_IPA,
	.buswidth = 8,
	.mas_rpm_id = 59,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 1,
		.qos_port = 18,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_SNOC_INT_2 },
};

static struct qcom_icc_node mas_jpeg = {
	.name = "mas_jpeg",
	.id = MSM8976_MASTER_JPEG,
	.buswidth = 16,
	.mas_rpm_id = 7,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 6,
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
};

static struct qcom_icc_node mas_lpass_ahb = {
	.name = "mas_lpass_ahb",
	.id = MSM8976_MASTER_LPASS_AHB,
	.buswidth = 8,
	.mas_rpm_id = 18,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 12,
	},
	.num_links = 1,
	.links = { MSM8976_SLAVE_PCNOC_SNOC },
};

static struct qcom_icc_node mas_lpass_proc = {
	.name = "mas_lpass_proc",
	.id = MSM8976_MASTER_LPASS_PROC,
	.buswidth = 8,
	.mas_rpm_id = 25,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 19,
	},
	.num_links = 3,
	.links = { MSM8976_MASTER_SNOC_INT_0, MSM8976_MASTER_SNOC_INT_1, MSM8976_SLAVE_SNOC_BIMC },
};

static struct qcom_icc_node mas_mdp_p0 = {
	.name = "mas_mdp_p0",
	.id = MSM8976_MASTER_MDP_P0,
	.buswidth = 16,
	.mas_rpm_id = 8,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 7,
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
}; /* vrail-comp???? */

static struct qcom_icc_node mas_mdp_p1 = {
	.name = "mas_mdp_p1",
	.id = MSM8976_MASTER_MDP_P1,
	.buswidth = 16,
	.mas_rpm_id = 61,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 13,
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
}; /* vrail-comp??? */

static struct qcom_icc_node mas_mm_int_0 = {
	.name = "mas_mm_int_0",
	.id = MSM8976_MASTER_MM_INT_0,
	.buswidth = 16,
	.mas_rpm_id = 79,
	.slv_rpm_id = 108,
	.qos = {
		.ap_owned = true,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_SNOC_INT_0 },
}; /* vrail-comp??? */

static struct qcom_icc_node mas_oxili = {
	.name = "mas_oxili",
	.id = MSM8976_MASTER_OXILI,
	.buswidth = 16,
	.mas_rpm_id = 6,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 16, /* 16 and 17 */
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
}; /* vrail-comp??? */

static struct qcom_icc_node mas_pcnoc_snoc = {
	.name = "mas_pcnoc_snoc",
	.id = MSM8976_MASTER_PCNOC_SNOC,
	.buswidth = 8,
	.mas_rpm_id = 29,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 0,
		.qos_port = 5,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_SNOC_INT_2 },
};

static struct qcom_icc_node mas_sdcc_1 = {
	.name = "mas_sdcc_1",
	.id = MSM8976_MASTER_SDCC_1,
	.buswidth = 8,
	.mas_rpm_id = 33,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 0,
		.qos_port = 7,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_INT_1 },
};

static struct qcom_icc_node mas_sdcc_2 = {
	.name = "mas_sdcc_2",
	.id = MSM8976_MASTER_SDCC_2,
	.buswidth = 8,
	.mas_rpm_id = 35,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 0,
		.qos_port = 8,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_INT_1 },
};

static struct qcom_icc_node mas_sdcc_3 = {
	.name = "mas_sdcc_3",
	.id = MSM8976_MASTER_SDCC_3,
	.buswidth = 8,
	.mas_rpm_id = 34,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 0,
		.qos_port = 10,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_INT_1 },
};

static struct qcom_icc_node mas_smmnoc_bimc = {
	.name = "mas_smmnoc_bimc",
	.id = MSM8976_MASTER_SMMNOC_BIMC,
	.buswidth = 16,
	.mas_rpm_id = 135,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = 2,
	},
	.num_links = 1,
	.links = { MSM8976_SLAVE_EBI },
};

static struct qcom_icc_node mas_snoc_bimc = {
	.name = "mas_snoc_bimc",
	.id = MSM8976_MASTER_SNOC_BIMC,
	.buswidth = 16,
	.mas_rpm_id = 3,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 3,
	},
	.num_links = 1,
	.links = { MSM8976_SLAVE_EBI },
};

static struct qcom_icc_node mas_snoc_pcnoc = {
	.name = "mas_snoc_pcnoc",
	.id = MSM8976_MASTER_SNOC_PCNOC,
	.buswidth = 8,
	.mas_rpm_id = 77,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 0,
		.qos_port = 9,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_INT_2 },
};

static struct qcom_icc_node mas_spdm = {
	.name = "mas_spdm",
	.id = MSM8976_MASTER_SPDM,
	.buswidth = 4,
	.mas_rpm_id = 50,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_M_0 },
};

static struct qcom_icc_node mas_qdss_bam = {
	.name = "mas_qdss_bam",
	.id = MSM8976_MASTER_QDSS_BAM,
	.buswidth = 4,
	.mas_rpm_id = 19,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 1,
		.qos_port = 11,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_QDSS_INT },
};

static struct qcom_icc_node mas_qdss_etr = {
	.name = "mas_qdss_etr",
	.id = MSM8976_MASTER_QDSS_ETR,
	.buswidth = 8,
	.mas_rpm_id = 31,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 1,
		.qos_port = 10,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_QDSS_INT },
};

static struct qcom_icc_node mas_qdss_int = {
	.name = "mas_qdss_int",
	.id = MSM8976_MASTER_QDSS_INT,
	.buswidth = 8,
	.mas_rpm_id = 31,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_SNOC_INT_2 },
};

static struct qcom_icc_node mas_tcu0 = {
	.name = "mas_tcu0",
	.id = MSM8976_MASTER_TCU0,
	.buswidth = 16,
	.mas_rpm_id = 102,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 2,
		.qos_port = 4,
	},
	.num_links = 2,
	.links = { MSM8976_SLAVE_EBI, MSM8976_SLAVE_BIMC_SNOC },
};

static struct qcom_icc_node mas_usb_hs1 = {
	.name = "mas_usb_hs1",
	.id = MSM8976_MASTER_USB_HS1,
	.buswidth = 4,
	.mas_rpm_id = 42,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_M_1 },
};

static struct qcom_icc_node mas_usb_hs2 = {
	.name = "mas_usb_hs2",
	.id = MSM8976_MASTER_USB_HS2,
	.buswidth = 4,
	.mas_rpm_id = 57,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 1,
		.qos_port = 2,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_M_0 },
};

static struct qcom_icc_node mas_venus_0 = {
	.name = "mas_venus_0",
	.id = MSM8976_MASTER_VENUS_0,
	.buswidth = 16,
	.mas_rpm_id = 9,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 8,
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
};

static struct qcom_icc_node mas_venus_1 = {
	.name = "mas_venus_1",
	.id = MSM8976_MASTER_VENUS_1,
	.buswidth = 16,
	.mas_rpm_id = 10,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 14,
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
};

static struct qcom_icc_node mas_vfe_0 = {
	.name = "mas_vfe_0",
	.id = MSM8976_MASTER_VFE_0,
	.buswidth = 16,
	.mas_rpm_id = 11,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 9,
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
};

static struct qcom_icc_node mas_vfe_1 = {
	.name = "mas_vfe_1",
	.id = MSM8976_MASTER_VFE_1,
	.buswidth = 16,
	.mas_rpm_id = 133,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = true,
		.qos_mode = NOC_QOS_MODE_BYPASS,
		.prio_level = 0,
		.qos_port = 15,
	},
	.num_links = 2,
	.links = { MSM8976_MASTER_MM_INT_0, MSM8976_SLAVE_SMMNOC_BIMC },
};

static struct qcom_icc_node mas_xm_usb_hs1 = {
	.name = "mas_xm_usb_hs1",
	.id = MSM8976_MASTER_XM_USB_HS1,
	.buswidth = 8,
	.mas_rpm_id = 136,
	.slv_rpm_id = -1,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_INT_0 },
};

static struct qcom_icc_node pcnoc_int_0 = {
	.name = "pcnoc_int_0",
	.id = MSM8976_MASTER_PCNOC_INT_0,
	.buswidth = 4,
	.mas_rpm_id = 85,
	.slv_rpm_id = 114,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = 5,
	},
	.num_links = 2,
	.links = { MSM8976_SLAVE_PCNOC_SNOC, MSM8976_MASTER_PCNOC_INT_2 },
};

static struct qcom_icc_node pcnoc_int_1 = {
	.name = "pcnoc_int_1",
	.id = MSM8976_MASTER_PCNOC_INT_1,
	.buswidth = 4,
	.mas_rpm_id = 86,
	.slv_rpm_id = 115,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 2,
	.links = { MSM8976_SLAVE_PCNOC_SNOC, MSM8976_MASTER_PCNOC_INT_2 },
};

static struct qcom_icc_node pcnoc_int_2 = {
	.name = "pcnoc_int_2",
	.id = MSM8976_MASTER_PCNOC_INT_2,
	.buswidth = 8,
	.mas_rpm_id = 124,
	.slv_rpm_id = 184,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 7,
	.links = { MSM8976_SLAVE_PCNOC_SNOC, MSM8976_MASTER_PCNOC_S_1, MSM8976_MASTER_PCNOC_S_2,
		   MSM8976_MASTER_PCNOC_S_3, MSM8976_MASTER_PCNOC_S_4, MSM8976_MASTER_PCNOC_S_8,
		   MSM8976_MASTER_PCNOC_S_9 },
};

static struct qcom_icc_node pcnoc_m_0 = {
	.name = "pcnoc_m_0",
	.id = MSM8976_MASTER_PCNOC_M_0,
	.buswidth = 4,
	.mas_rpm_id = 87,
	.slv_rpm_id = 116,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 0,
		.qos_port = 5,
	},
	.num_links = 1,
	.links = { MSM8976_SLAVE_PCNOC_SNOC },
};

static struct qcom_icc_node pcnoc_m_1 = {
	.name = "pcnoc_m_1",
	.id = MSM8976_MASTER_PCNOC_M_1,
	.buswidth = 4,
	.mas_rpm_id = 88,
	.slv_rpm_id = 117,
	.qos = {
		.ap_owned = false,
		.qos_mode = NOC_QOS_MODE_FIXED,
		.prio_level = 0,
		.qos_port = 6,
	},
	.num_links = 1,
	.links = { MSM8976_SLAVE_PCNOC_SNOC },
};

static struct qcom_icc_node pcnoc_s_1 = {
	.name = "pcnoc_s_1",
	.id = MSM8976_MASTER_PCNOC_S_1,
	.buswidth = 4,
	.mas_rpm_id = 90,
	.slv_rpm_id = 119,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 4,
	.links = { MSM8976_SLAVE_CRYPTO_0_CFG, MSM8976_SLAVE_PRNG, MSM8976_SLAVE_PDM,
		   MSM8976_SLAVE_MESSAGE_RAM },
};

static struct qcom_icc_node pcnoc_s_2 = {
	.name = "pcnoc_s_2",
	.id = MSM8976_MASTER_PCNOC_S_2,
	.buswidth = 8,
	.mas_rpm_id = 91,
	.slv_rpm_id = 120,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_SLAVE_PMIC_ARB },
};

static struct qcom_icc_node pcnoc_s_3 = {
	.name = "pcnoc_s_3",
	.id = MSM8976_MASTER_PCNOC_S_3,
	.buswidth = 4,
	.mas_rpm_id = 92,
	.slv_rpm_id = 121,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 2,
	.links = { MSM8976_SLAVE_DCC_CFG, MSM8976_SLAVE_SNOC_CFG },
};

static struct qcom_icc_node pcnoc_s_4 = {
	.name = "pcnoc_s_4",
	.id = MSM8976_MASTER_PCNOC_S_4,
	.buswidth = 4,
	.mas_rpm_id = 93,
	.slv_rpm_id = 122,
	.qos = {
		.ap_owned = true,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 2,
	.links = { MSM8976_SLAVE_CAMERA_SS_CFG, MSM8976_SLAVE_DISP_SS_CFG, MSM8976_SLAVE_VENUS_CFG },
};

static struct qcom_icc_node pcnoc_s_8 = {
	.name = "pcnoc_s_8",
	.id = MSM8976_MASTER_PCNOC_S_8,
	.buswidth = 4,
	.mas_rpm_id = 96,
	.slv_rpm_id = 125,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 4,
	.links = { MSM8976_SLAVE_BLSP_1, MSM8976_SLAVE_SDCC_1, MSM8976_SLAVE_SDCC_3,
		   MSM8976_SLAVE_USB_HS },
};

static struct qcom_icc_node pcnoc_s_9 = {
	.name = "pcnoc_s_9",
	.id = MSM8976_MASTER_PCNOC_S_9,
	.buswidth = 4,
	.mas_rpm_id = 97,
	.slv_rpm_id = 126,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 2,
	.links = { MSM8976_SLAVE_BLSP_2, MSM8976_SLAVE_GPU_CFG, MSM8976_SLAVE_USB_HS2,
		   MSM8976_SLAVE_SDCC_2 },
};

static struct qcom_icc_node snoc_int_0 = {
	.name = "snoc_int_0",
	.id = MSM8976_MASTER_SNOC_INT_0,
	.buswidth = 8,
	.mas_rpm_id = 99,
	.slv_rpm_id = 130,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 3,
	.links = { MSM8976_SLAVE_IMEM, MSM8976_SLAVE_QDSS_STM, MSM8976_SLAVE_SNOC_PCNOC },
};

static struct qcom_icc_node snoc_int_1 = {
	.name = "snoc_int_1",
	.id = MSM8976_MASTER_SNOC_INT_1,
	.buswidth = 8,
	.mas_rpm_id = 100,
	.slv_rpm_id = 131,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 4,
	.links = { MSM8976_SLAVE_CATS_0, MSM8976_SLAVE_CATS_1, MSM8976_SLAVE_KPSS_AHB,
		   MSM8976_SLAVE_LPASS },
};

static struct qcom_icc_node snoc_int_2 = {
	.name = "snoc_int_2",
	.id = MSM8976_MASTER_SNOC_INT_2,
	.buswidth = 8,
	.mas_rpm_id = 134,
	.slv_rpm_id = 197,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 3,
	.links = { MSM8976_MASTER_SNOC_INT_0, MSM8976_MASTER_SNOC_INT_1, MSM8976_SLAVE_SNOC_BIMC },
};

static struct qcom_icc_node slv_bimc_snoc = {
	.name = "slv_bimc_snoc",
	.id = MSM8976_SLAVE_BIMC_SNOC,
	.buswidth = 16,
	.mas_rpm_id = -1,
	.slv_rpm_id = 2,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_BIMC_SNOC },
};

static struct qcom_icc_node slv_blsp_1 = {
	.name = "slv_blsp_1",
	.id = MSM8976_SLAVE_BLSP_1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 39,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_blsp_2 = {
	.name = "slv_blsp_2",
	.id = MSM8976_SLAVE_BLSP_2,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 37,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_camera_ss_cfg = {
	.name = "slv_camera_ss_cfg",
	.id = MSM8976_SLAVE_CAMERA_SS_CFG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 3,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_cats_0 = {
	.name = "slv_cats_0",
	.id = MSM8976_SLAVE_CATS_0,
	.buswidth = 16,
	.mas_rpm_id = -1,
	.slv_rpm_id = 106,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_cats_1 = {
	.name = "slv_cats_1",
	.id = MSM8976_SLAVE_CATS_1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = 107,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_crypto_0_cfg = {
	.name = "slv_crypto_0_cfg",
	.id = MSM8976_SLAVE_CRYPTO_0_CFG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 52,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_dcc_cfg = {
	.name = "slv_dcc_cfg",
	.id = MSM8976_SLAVE_DCC_CFG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 155,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_disp_ss_cfg = {
	.name = "slv_disp_ss_cfg",
	.id = MSM8976_SLAVE_DISP_SS_CFG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 4,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_ebi = {
	.name = "slv_ebi",
	.id = MSM8976_SLAVE_EBI,
	.buswidth = 16,
	.mas_rpm_id = -1,
	.slv_rpm_id = 0,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_gpu_cfg = {
	.name = "slv_gpu_cfg",
	.id = MSM8976_SLAVE_GPU_CFG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 11,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_imem = {
	.name = "slv_imem",
	.id = MSM8976_SLAVE_IMEM,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = 26,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_kpss_ahb = {
	.name = "slv_kpss_ahb",
	.id = MSM8976_SLAVE_GPU_CFG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 20,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_lpass = {
	.name = "slv_lpass",
	.id = MSM8976_SLAVE_LPASS,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = 21,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_message_ram = {
	.name = "slv_message_ram",
	.id = MSM8976_SLAVE_MESSAGE_RAM,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 55,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};


static struct qcom_icc_node slv_pcnoc_snoc = {
	.name = "slv_pcnoc_snoc",
	.id = MSM8976_SLAVE_PCNOC_SNOC,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = 45,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_PCNOC_SNOC },
};

static struct qcom_icc_node slv_pdm = {
	.name = "slv_pdm",
	.id = MSM8976_SLAVE_PDM,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 41,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_pmic_arb = {
	.name = "slv_pmic_arb",
	.id = MSM8976_SLAVE_PMIC_ARB,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 59,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_prng = {
	.name = "slv_prng",
	.id = MSM8976_SLAVE_PRNG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 44,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_qdss_stm = {
	.name = "slv_qdss_stm",
	.id = MSM8976_SLAVE_QDSS_STM,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 30,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_sdcc_1 = {
	.name = "slv_sdcc_1",
	.id = MSM8976_SLAVE_SDCC_1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 31,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_sdcc_2 = {
	.name = "slv_sdcc_2",
	.id = MSM8976_SLAVE_SDCC_2,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 33,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_sdcc_3 = {
	.name = "slv_sdcc_3",
	.id = MSM8976_SLAVE_SDCC_3,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 32,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_smmnoc_bimc = {
	.name = "slv_pcnoc_snoc",
	.id = MSM8976_SLAVE_SMMNOC_BIMC,
	.buswidth = 16,
	.mas_rpm_id = -1,
	.slv_rpm_id = 198,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_SMMNOC_BIMC },
	/* vrail-comp??? */
};

static struct qcom_icc_node slv_snoc_bimc = {
	.name = "slv_snoc_bimc",
	.id = MSM8976_SLAVE_SNOC_BIMC,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = 24,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_SNOC_BIMC },
};

static struct qcom_icc_node slv_snoc_cfg = {
	.name = "slv_snoc_cfg",
	.id = MSM8976_SLAVE_SNOC_CFG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 70,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_snoc_pcnoc = { //
	.name = "slv_snoc_pcnoc",
	.id = MSM8976_SLAVE_SNOC_PCNOC,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = 28,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 1,
	.links = { MSM8976_MASTER_SNOC_PCNOC },
};

static struct qcom_icc_node slv_tcsr = {
	.name = "slv_tcsr",
	.id = MSM8976_SLAVE_TCSR,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 50,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_tlmm = {
	.name = "slv_tlmm",
	.id = MSM8976_SLAVE_TLMM,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 51,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_usb_hs = {
	.name = "slv_usb_hs",
	.id = MSM8976_SLAVE_USB_HS,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 40,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_usb_hs2 = {
	.name = "slv_usb_hs2",
	.id = MSM8976_SLAVE_USB_HS2,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 79,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node slv_venus_cfg = {
	.name = "slv_venus_cfg",
	.id = MSM8976_SLAVE_VENUS_CFG,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = 10,
	.qos = {
		.ap_owned = false,
		.qos_mode = -1,
		.prio_level = 0,
		.qos_port = -1,
	},
	.num_links = 0,
};

static struct qcom_icc_node *msm8976_bimc_nodes[] = {
	[MASTER_APPS_PROC] = &mas_apss_proc,
	[MASTER_SMMNOC_BIMC] = &mas_smmnoc_bimc,
	[MASTER_SNOC_BIMC] = &mas_snoc_bimc,
	[MASTER_TCU0] = &mas_tcu0,
	[SLAVE_BIMC_SNOC] = &slv_bimc_snoc,
	[SLAVE_EBI] = &slv_ebi,
};

static const struct regmap_config msm8976_bimc_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x62000,
	.fast_io	= true,
};

static struct qcom_icc_desc msm8976_bimc = {
	.nodes = msm8976_bimc_nodes,
	.num_nodes = ARRAY_SIZE(msm8976_bimc_nodes),
	.regmap_cfg = &msm8976_bimc_regmap_config,
};

static struct qcom_icc_node *msm8976_pcnoc_nodes[] = {
	[MASTER_BLSP_1] = &mas_blsp_1,
	[MASTER_BLSP_2] = &mas_blsp_2,
	[MASTER_CRYPTO_C0] = &mas_crypto,
	[MASTER_DEHR] = &mas_dehr,
	[MASTER_LPASS_AHB] = &mas_lpass_ahb,
	[MASTER_SDCC_1] = &mas_sdcc_1,
	[MASTER_SDCC_2] = &mas_sdcc_2,
	[MASTER_SDCC_3] = &mas_sdcc_3,
	[MASTER_SNOC_PCNOC] = &mas_snoc_pcnoc,
	[MASTER_SPDM] = &mas_spdm,
	[MASTER_USB_HS1] = &mas_usb_hs1,
	[MASTER_USB_HS2] = &mas_usb_hs2,
	[MASTER_XM_USB_HS1] = &mas_xm_usb_hs1,
	[MASTER_PCNOC_M_0] = &pcnoc_m_0,
	[MASTER_PCNOC_M_1] = &pcnoc_m_1,
	[MASTER_PCNOC_INT_0] = &pcnoc_int_0,
	[MASTER_PCNOC_INT_1] = &pcnoc_int_1,
	[MASTER_PCNOC_INT_2] = &pcnoc_int_2,
	[MASTER_PCNOC_S_1] = &pcnoc_s_1,
	[MASTER_PCNOC_S_2] = &pcnoc_s_2,
	[MASTER_PCNOC_S_3] = &pcnoc_s_3,
	[MASTER_PCNOC_S_4] = &pcnoc_s_4,
	[MASTER_PCNOC_S_8] = &pcnoc_s_8,
	[MASTER_PCNOC_S_9] = &pcnoc_s_9,
	[SLAVE_BLSP_1] = &slv_blsp_1,
	[SLAVE_BLSP_2] = &slv_blsp_2,
	[SLAVE_CAMERA_SS_CFG] = &slv_camera_ss_cfg,
	[SLAVE_CRYPTO_0_CFG] = &slv_crypto_0_cfg,
	[SLAVE_DCC_CFG] = &slv_dcc_cfg,
	[SLAVE_DISP_SS_CFG] = &slv_disp_ss_cfg,
	[SLAVE_GPU_CFG] = &slv_gpu_cfg,
	[SLAVE_MESSAGE_RAM] = &slv_message_ram,
	[SLAVE_PDM] = &slv_pdm,
	[SLAVE_PMIC_ARB] = &slv_pmic_arb,
	[SLAVE_SNOC_CFG] = &slv_snoc_cfg,
	[SLAVE_PCNOC_SNOC] = &slv_pcnoc_snoc,
	[SLAVE_PRNG] = &slv_prng,
	[SLAVE_SDCC_1] = &slv_sdcc_1,
	[SLAVE_SDCC_2] = &slv_sdcc_2,
	[SLAVE_SDCC_3] = &slv_sdcc_3,
	[SLAVE_TCSR] = &slv_tcsr,
	[SLAVE_TLMM] = &slv_tlmm,
	[SLAVE_USB_HS] = &slv_usb_hs,
	[SLAVE_USB_HS2] = &slv_usb_hs2,
	[SLAVE_VENUS_CFG] = &slv_venus_cfg,
};

static const struct regmap_config msm8976_pcnoc_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x14000,
	.fast_io	= true,
};

static struct qcom_icc_desc msm8976_pcnoc = {
	.nodes = msm8976_pcnoc_nodes,
	.num_nodes = ARRAY_SIZE(msm8976_pcnoc_nodes),
	.regmap_cfg = &msm8976_pcnoc_regmap_config,
};

static struct qcom_icc_node *msm8976_snoc_nodes[] = {
	[MASTER_BIMC_SNOC] = &mas_bimc_snoc,
	[MASTER_CPP] = &mas_cpp,
	[MASTER_IPA] = &mas_ipa,
	[MASTER_LPASS_PROC] = &mas_lpass_proc,
	[MASTER_JPEG] = &mas_jpeg,
	[MASTER_MDP_P0] = &mas_mdp_p0,
	[MASTER_MDP_P1] = &mas_mdp_p1,
	[MASTER_MM_INT_0] = &mas_mm_int_0,
	[MASTER_OXILI] = &mas_oxili,
	[MASTER_PCNOC_SNOC] = &mas_pcnoc_snoc,
	[MASTER_QDSS_BAM] = &mas_qdss_bam,
	[MASTER_QDSS_ETR] = &mas_qdss_etr,
	[MASTER_QDSS_INT] = &mas_qdss_int,
	[MASTER_SNOC_INT_0] = &snoc_int_0,
	[MASTER_SNOC_INT_1] = &snoc_int_1,
	[MASTER_SNOC_INT_2] = &snoc_int_2,
	[MASTER_VENUS_0] = &mas_venus_0,
	[MASTER_VENUS_1] = &mas_venus_1,
	[MASTER_VFE_0] = &mas_vfe_0,
	[MASTER_VFE_1] = &mas_vfe_1,
	[SLAVE_CATS_0] = &slv_cats_0,
	[SLAVE_CATS_1] = &slv_cats_1,
	[SLAVE_KPSS_AHB] = &slv_kpss_ahb,
	[SLAVE_LPASS] = &slv_lpass,
	[SLAVE_QDSS_STM] = &slv_qdss_stm,
	[SLAVE_SMMNOC_BIMC] = &slv_smmnoc_bimc,
	[SLAVE_SNOC_BIMC] = &slv_snoc_bimc,
	[SLAVE_SNOC_PCNOC] = &slv_snoc_pcnoc,
	[SLAVE_IMEM] = &slv_imem,
};

static const struct regmap_config msm8976_snoc_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x1a000,
	.fast_io	= true,
};

static struct qcom_icc_desc msm8976_snoc = {
	.nodes = msm8976_snoc_nodes,
	.num_nodes = ARRAY_SIZE(msm8976_snoc_nodes),
	.regmap_cfg = &msm8976_snoc_regmap_config,
};

static int qcom_icc_bimc_set_qos_health(struct regmap *rmap,
					struct qcom_icc_qos *qos,
					int regnum)
{
	u32 val;
	u32 mask;

	val = qos->prio_level;
	mask = M_BKE_HEALTH_CFG_PRIOLVL_MASK;

	val |= qos->areq_prio << M_BKE_HEALTH_CFG_AREQPRIO_SHIFT;
	mask |= M_BKE_HEALTH_CFG_AREQPRIO_MASK;

	/* LIMITCMDS is not present on M_BKE_HEALTH_3 */
	if (regnum != 3) {
		val |= qos->limit_commands << M_BKE_HEALTH_CFG_LIMITCMDS_SHIFT;
		mask |= M_BKE_HEALTH_CFG_LIMITCMDS_MASK;
	}

	return regmap_update_bits(rmap,
				  M_BKE_HEALTH_CFG_ADDR(regnum, qos->qos_port),
				  mask, val);
}

static int qcom_icc_set_bimc_qos(struct icc_node *src, u64 max_bw,
				 bool bypass_mode)
{
	struct qcom_icc_provider *qp;
	struct qcom_icc_node *qn;
	struct icc_provider *provider;
	u32 mode = NOC_QOS_MODE_BYPASS;
	u32 val = 0;
	int i, rc = 0;

	qn = src->data;
	provider = src->provider;
	qp = to_qcom_provider(provider);

	if (qn->qos.qos_mode != -1)
		mode = qn->qos.qos_mode;

	/*
	 * QoS Priority: The QoS Health parameters are getting considered
	 * only if we are NOT in Bypass Mode.
	 */
	if (mode != NOC_QOS_MODE_BYPASS) {
		for (i = 3; i >= 0; i--) {
			rc = qcom_icc_bimc_set_qos_health(qp->regmap,
							  &qn->qos, i);
			if (rc)
				return rc;
		}

		/* Set BKE_EN to 1 when Fixed, Regulator or Limiter Mode */
		val = 1;
	}

	return regmap_update_bits(qp->regmap, M_BKE_EN_ADDR(qn->qos.qos_port),
				  M_BKE_EN_EN_BMASK, val);
}

static int qcom_icc_noc_set_qos_priority(struct regmap *rmap,
					 struct qcom_icc_qos *qos)
{
	u32 val;
	int rc;

	/* Must be updated one at a time, P1 first, P0 last */
	val = qos->areq_prio << NOC_QOS_PRIORITY_P1_SHIFT;
	rc = regmap_update_bits(rmap, NOC_QOS_PRIORITYn_ADDR(qos->qos_port),
				NOC_QOS_PRIORITY_MASK, val);
	if (rc)
		return rc;

	val = qos->prio_level << NOC_QOS_PRIORITY_P0_SHIFT;
	return regmap_update_bits(rmap, NOC_QOS_PRIORITYn_ADDR(qos->qos_port),
				  NOC_QOS_PRIORITY_MASK, val);
}

static int qcom_icc_set_noc_qos(struct icc_node *src, u64 max_bw)
{
	struct qcom_icc_provider *qp;
	struct qcom_icc_node *qn;
	struct icc_provider *provider;
	u32 mode = NOC_QOS_MODE_BYPASS;
	int rc = 0;

	qn = src->data;
	provider = src->provider;
	qp = to_qcom_provider(provider);

	if (qn->qos.qos_port < 0) {
		dev_dbg(src->provider->dev,
			"NoC QoS: Skipping %s: vote aggregated on parent.\n",
			qn->name);
		return 0;
	}

	if (qn->qos.qos_mode != -1)
		mode = qn->qos.qos_mode;

	if (mode == NOC_QOS_MODE_FIXED) {
		dev_dbg(src->provider->dev, "NoC QoS: %s: Set Fixed mode\n",
			qn->name);
		rc = qcom_icc_noc_set_qos_priority(qp->regmap, &qn->qos);
		if (rc)
			return rc;
	} else if (mode == NOC_QOS_MODE_BYPASS) {
		dev_dbg(src->provider->dev, "NoC QoS: %s: Set Bypass mode\n",
			qn->name);
	}

	return regmap_update_bits(qp->regmap,
				  NOC_QOS_MODEn_ADDR(qn->qos.qos_port),
				  NOC_QOS_MODEn_MASK, mode);
}

static int qcom_icc_qos_set(struct icc_node *node, u64 sum_bw)
{
	struct qcom_icc_provider *qp = to_qcom_provider(node->provider);
	struct qcom_icc_node *qn = node->data;

	dev_dbg(node->provider->dev, "Setting QoS for %s\n", qn->name);

	if (qp->is_bimc_node)
		return qcom_icc_set_bimc_qos(node, sum_bw,
				(qn->qos.qos_mode == NOC_QOS_MODE_BYPASS));

	return qcom_icc_set_noc_qos(node, sum_bw);
}

static int qcom_icc_rpm_set(int mas_rpm_id, int slv_rpm_id, u64 sum_bw)
{
	int ret = 0;

	if (mas_rpm_id != -1) {
		ret = qcom_icc_rpm_smd_send(QCOM_SMD_RPM_ACTIVE_STATE,
					    RPM_BUS_MASTER_REQ,
					    mas_rpm_id,
					    sum_bw);
		if (ret) {
			pr_err("qcom_icc_rpm_smd_send mas %d error %d\n",
			       mas_rpm_id, ret);
			return ret;
		}
	}

	if (slv_rpm_id != -1) {
		ret = qcom_icc_rpm_smd_send(QCOM_SMD_RPM_ACTIVE_STATE,
					    RPM_BUS_SLAVE_REQ,
					    slv_rpm_id,
					    sum_bw);
		if (ret) {
			pr_err("qcom_icc_rpm_smd_send slv %d error %d\n",
			       slv_rpm_id, ret);
			return ret;
		}
	}

	return ret;
}

static int qcom_icc_set(struct icc_node *src, struct icc_node *dst)
{
	struct qcom_icc_provider *qp;
	struct qcom_icc_node *qn;
	struct icc_provider *provider;
	struct icc_node *n;
	u64 sum_bw;
	u64 max_peak_bw;
	u64 rate;
	u32 agg_avg = 0;
	u32 agg_peak = 0;
	int ret, i;

	qn = src->data;
	provider = src->provider;
	qp = to_qcom_provider(provider);

	list_for_each_entry(n, &provider->nodes, node_list)
		provider->aggregate(n, 0, n->avg_bw, n->peak_bw,
				    &agg_avg, &agg_peak);

	sum_bw = icc_units_to_bps(agg_avg);
	max_peak_bw = icc_units_to_bps(agg_peak);

	if (!qn->qos.ap_owned) {
		/* send bandwidth request message to the RPM processor */
		ret = qcom_icc_rpm_set(qn->mas_rpm_id, qn->slv_rpm_id, sum_bw);
		if (ret)
			return ret;
	} else if (qn->qos.qos_mode != -1) {
		/* set bandwidth directly from the AP */
		ret = qcom_icc_qos_set(src, sum_bw);
		if (ret)
			return ret;
	}

	rate = max(sum_bw, max_peak_bw);

	do_div(rate, qn->buswidth);

	if (qn->rate == rate)
		return 0;

	for (i = 0; i < qp->num_clks; i++) {
		ret = clk_set_rate(qp->bus_clks[i].clk, rate);
		if (ret) {
			pr_err("%s clk_set_rate error: %d\n",
			       qp->bus_clks[i].id, ret);
			return ret;
		}
	}

	qn->rate = rate;

	return 0;
}

static int qnoc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct qcom_icc_desc *desc;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct qcom_icc_node **qnodes;
	struct qcom_icc_provider *qp;
	struct icc_node *node;
	struct resource *res;
	size_t num_nodes, i;
	int ret;

	/* wait for the RPM proxy */
	if (!qcom_icc_rpm_smd_available())
		return -EPROBE_DEFER;

	desc = of_device_get_match_data(dev);
	if (!desc)
		return -EINVAL;

	qnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	qp = devm_kzalloc(dev, sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return -ENOMEM;

	data = devm_kzalloc(dev, struct_size(data, nodes, num_nodes),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;


	if (of_device_is_compatible(dev->of_node, "qcom,msm8976-bimc"))
		qp->is_bimc_node = true;

	qp->bus_clks = devm_kmemdup(dev, bus_clocks, sizeof(bus_clocks),
				    GFP_KERNEL);
	qp->num_clks = ARRAY_SIZE(bus_clocks);
	if (!qp->bus_clks)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	qp->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(qp->mmio)) {
		dev_err(dev, "Cannot ioremap interconnect bus resource\n");
		return PTR_ERR(qp->mmio);
	}

	qp->regmap = devm_regmap_init_mmio(dev, qp->mmio, desc->regmap_cfg);
	if (IS_ERR(qp->regmap)) {
		dev_err(dev, "Cannot regmap interconnect bus resource\n");
		return PTR_ERR(qp->regmap);
	}

	ret = devm_clk_bulk_get(dev, qp->num_clks, qp->bus_clks);
	if (ret)
		return ret;

	ret = clk_bulk_prepare_enable(qp->num_clks, qp->bus_clks);
	if (ret)
		return ret;

	provider = &qp->provider;
	INIT_LIST_HEAD(&provider->nodes);
	provider->dev = dev;
	provider->set = qcom_icc_set;
	provider->aggregate = icc_std_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	provider->data = data;

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(dev, "error adding interconnect provider: %d\n", ret);
		clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);
		return ret;
	}

	for (i = 0; i < num_nodes; i++) {
		size_t j;

		node = icc_node_create(qnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		node->name = qnodes[i]->name;
		node->data = qnodes[i];
		icc_node_add(node, provider);

		for (j = 0; j < qnodes[i]->num_links; j++)
			icc_link_create(node, qnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;
	platform_set_drvdata(pdev, qp);

	return 0;
err:
	icc_nodes_remove(provider);
	clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);
	icc_provider_del(provider);

	return ret;
}

static int qnoc_remove(struct platform_device *pdev)
{
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);

	icc_nodes_remove(&qp->provider);
	clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);
	return icc_provider_del(&qp->provider);
}

static const struct of_device_id msm8976_noc_of_match[] = {
	{ .compatible = "qcom,msm8976-bimc", .data = &msm8976_bimc },
	{ .compatible = "qcom,msm8976-pcnoc", .data = &msm8976_pcnoc },
	{ .compatible = "qcom,msm8976-snoc", .data = &msm8976_snoc },
	{ },
};
MODULE_DEVICE_TABLE(of, msm8976_noc_of_match);

static struct platform_driver msm8976_noc_driver = {
	.probe = qnoc_probe,
	.remove = qnoc_remove,
	.driver = {
		.name = "qnoc-msm8976",
		.of_match_table = msm8976_noc_of_match,
		.sync_state = icc_sync_state,
	},
};
module_platform_driver(msm8976_noc_driver);
MODULE_AUTHOR("AngeloGioacchino Del Regno <angelogioacchino.delregno@somainline.org>");
MODULE_DESCRIPTION("Qualcomm msm8976 NoC driver");
MODULE_LICENSE("GPL v2");
