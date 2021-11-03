// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019, Linaro Limited

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/mfd/wcd9335/registers.h>
#include <linux/mfd/wcd9335/wcd9335.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slimbus.h>

#define WCD9335_REGMAP_IRQ_REG(_irq, _off, _mask)		\
	[_irq] = {						\
		.reg_offset = (_off),				\
		.mask = (_mask),				\
		.type = {					\
			.type_reg_offset = (_off),		\
			.types_supported = IRQ_TYPE_EDGE_BOTH,	\
			.type_reg_mask  = (_mask),		\
			.type_level_low_val = (_mask),		\
			.type_level_high_val = (_mask),		\
			.type_falling_val = 0,			\
			.type_rising_val = 0,			\
		},						\
	}

static const struct mfd_cell wcd9335_devices[] = {
	{
		.name = "wcd9335-codec",
	}, {
		.name = "wcd9335-gpio",
		.of_compatible = "qcom,wcd9340-gpio",
	}, {
		.name = "wcd9335-soundwire",
		.of_compatible = "qcom,soundwire-v1.3.0",
	},
};

static const struct regmap_irq wcd9335_irqs[] = {
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_SLIMBUS, 0, BIT(0)),
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_HPH_PA_OCPL_FAULT, 0, BIT(2)),
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_HPH_PA_OCPR_FAULT, 0, BIT(3)),
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_MBHC_SW_DET, 1, BIT(0)),
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_MBHC_ELECT_INS_REM_DET, 1, BIT(1)),
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_MBHC_BUTTON_PRESS_DET, 1, BIT(2)),
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_MBHC_BUTTON_RELEASE_DET, 1, BIT(3)),
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_MBHC_ELECT_INS_REM_LEG_DET, 1, BIT(4)),
	WCD9335_REGMAP_IRQ_REG(WCD9335_IRQ_SOUNDWIRE, 2, BIT(4)),
};

static const struct regmap_irq_chip wcd9335_regmap_irq_chip = {
	.name = "wcd9335_irq",
	.status_base = WCD9335_INTR_PIN1_STATUS0,
	.mask_base = WCD9335_INTR_PIN1_MASK0,
	.ack_base = WCD9335_INTR_PIN1_CLEAR0,
	.type_base = WCD9335_INTR_LEVEL0,
	.num_type_reg = 4,
	.type_in_mask = false,
	.num_regs = 4,
	.irqs = wcd9335_irqs,
	.num_irqs = ARRAY_SIZE(wcd9335_irqs),
};

static bool wcd9335_is_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WCD9335_INTR_PIN1_STATUS0...WCD9335_INTR_PIN2_CLEAR3:
	case WCD9335_SWR_AHB_BRIDGE_RD_DATA_0:
	case WCD9335_SWR_AHB_BRIDGE_RD_DATA_1:
	case WCD9335_SWR_AHB_BRIDGE_RD_DATA_2:
	case WCD9335_SWR_AHB_BRIDGE_RD_DATA_3:
	case WCD9335_SWR_AHB_BRIDGE_ACCESS_STATUS:
	case WCD9335_ANA_MBHC_RESULT_3:
	case WCD9335_ANA_MBHC_RESULT_2:
	case WCD9335_ANA_MBHC_RESULT_1:
	case WCD9335_ANA_MBHC_MECH:
	case WCD9335_ANA_MBHC_ELECT:
	case WCD9335_ANA_MBHC_ZDET:
	case WCD9335_ANA_MICB2:
	case WCD9335_ANA_RCO:
	case WCD9335_ANA_BIAS:
		return true;
	default:
		return false;
	}
};

static const struct regmap_range_cfg wcd9335_ranges[] = {
	{	.name = "WCD9335",
		.range_min =  0x0,
		.range_max =  WCD9335_MAX_REGISTER,
		.selector_reg = WCD9335_SEL_REGISTER,
		.selector_mask = WCD9335_SEL_MASK,
		.selector_shift = WCD9335_SEL_SHIFT,
		.window_start = WCD9335_WINDOW_START,
		.window_len = WCD9335_WINDOW_LENGTH,
	},
};

static struct regmap_config wcd9335_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.max_register = 0xffff,
	.can_multi_write = true,
	.ranges = wcd9335_ranges,
	.num_ranges = ARRAY_SIZE(wcd9335_ranges),
	.volatile_reg = wcd9335_is_volatile_register,
};

static int wcd9335_bring_up(struct wcd9335_ddata *ddata)
{
	struct regmap *regmap = ddata->regmap;
	u16 id_minor, id_major;
	int ret;

	ret = regmap_bulk_read(regmap, WCD9335_CHIP_TIER_CTRL_CHIP_ID_BYTE0,
			       (u8 *)&id_minor, sizeof(u16));
	if (ret)
		return ret;

	ret = regmap_bulk_read(regmap, WCD9335_CHIP_TIER_CTRL_CHIP_ID_BYTE2,
			       (u8 *)&id_major, sizeof(u16));
	if (ret)
		return ret;

	dev_info(ddata->dev, "WCD934x chip id major 0x%x, minor 0x%x\n",
		 id_major, id_minor);

	regmap_write(regmap, WCD9335_CODEC_RPM_RST_CTL, 0x01);
	regmap_write(regmap, WCD9335_SIDO_NEW_VOUT_A_STARTUP, 0x19);
	regmap_write(regmap, WCD9335_SIDO_NEW_VOUT_D_STARTUP, 0x15);
	/* Add 1msec delay for VOUT to settle */
	usleep_range(1000, 1100);
	regmap_write(regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x5);
	regmap_write(regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x7);
	regmap_write(regmap, WCD9335_CODEC_RPM_RST_CTL, 0x3);
	regmap_write(regmap, WCD9335_CODEC_RPM_RST_CTL, 0x7);
	regmap_write(regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x3);

	return 0;
}

static int wcd9335_slim_status_up(struct slim_device *sdev)
{
	struct device *dev = &sdev->dev;
	struct wcd9335_ddata *ddata;
	int ret;

	ddata = dev_get_drvdata(dev);

	ddata->regmap = regmap_init_slimbus(sdev, &wcd9335_regmap_config);
	if (IS_ERR(ddata->regmap)) {
		dev_err(dev, "Error allocating slim regmap\n");
		return PTR_ERR(ddata->regmap);
	}

	ret = wcd9335_bring_up(ddata);
	if (ret) {
		dev_err(dev, "Failed to bring up WCD9335: err = %d\n", ret);
		return ret;
	}

	ret = devm_regmap_add_irq_chip(dev, ddata->regmap, ddata->irq,
				       IRQF_TRIGGER_HIGH, 0,
				       &wcd9335_regmap_irq_chip,
				       &ddata->irq_data);
	if (ret) {
		dev_err(dev, "Failed to add IRQ chip: err = %d\n", ret);
		return ret;
	}

	ret = mfd_add_devices(dev, PLATFORM_DEVID_AUTO, wcd9335_devices,
			      ARRAY_SIZE(wcd9335_devices), NULL, 0, NULL);
	if (ret) {
		dev_err(dev, "Failed to add child devices: err = %d\n",
			ret);
		return ret;
	}

	return ret;
}

static int wcd9335_slim_status(struct slim_device *sdev,
			       enum slim_device_status status)
{
	switch (status) {
	case SLIM_DEVICE_STATUS_UP:
		return wcd9335_slim_status_up(sdev);
	case SLIM_DEVICE_STATUS_DOWN:
		mfd_remove_devices(&sdev->dev);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int wcd9335_slim_probe(struct slim_device *sdev)
{
	struct device *dev = &sdev->dev;
	struct device_node *np = dev->of_node;
	struct wcd9335_ddata *ddata;
	int reset_gpio, ret;

	ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return	-ENOMEM;

	ddata->irq = of_irq_get(np, 0);
	if (ddata->irq < 0)
		return dev_err_probe(ddata->dev, ddata->irq,
				     "Failed to get IRQ\n");

	reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (reset_gpio < 0) {
		dev_err(dev, "Failed to get reset gpio: err = %d\n",
			reset_gpio);
		return reset_gpio;
	}

	ddata->extclk = devm_clk_get(dev, "extclk");
	if (IS_ERR(ddata->extclk)) {
		dev_err(dev, "Failed to get extclk");
		return PTR_ERR(ddata->extclk);
	}

	ddata->supplies[0].supply = "vdd-buck";
	ddata->supplies[1].supply = "vdd-buck-sido";
	ddata->supplies[2].supply = "vdd-tx";
	ddata->supplies[3].supply = "vdd-rx";
	ddata->supplies[4].supply = "vdd-io";

	ret = regulator_bulk_get(dev, WCD9335_MAX_SUPPLY, ddata->supplies);
	if (ret) {
		dev_err(dev, "Failed to get supplies: err = %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_enable(WCD9335_MAX_SUPPLY, ddata->supplies);
	if (ret) {
		dev_err(dev, "Failed to enable supplies: err = %d\n", ret);
		return ret;
	}

	/*
	 * For WCD9335, it takes about 600us for the Vout_A and
	 * Vout_D to be ready after BUCK_SIDO is powered up.
	 * SYS_RST_N shouldn't be pulled high during this time
	 */
	usleep_range(600, 650);
	gpio_direction_output(reset_gpio, 0);
	msleep(20);
	gpio_set_value(reset_gpio, 1);
	msleep(20);

	ddata->dev = dev;
	dev_set_drvdata(dev, ddata);

	return 0;
}

static void wcd9335_slim_remove(struct slim_device *sdev)
{
	struct wcd9335_ddata *ddata = dev_get_drvdata(&sdev->dev);

	regulator_bulk_disable(WCD9335_MAX_SUPPLY, ddata->supplies);
	mfd_remove_devices(&sdev->dev);
}

static const struct slim_device_id wcd9335_slim_id[] = {
	{ SLIM_MANF_ID_QCOM, SLIM_PROD_CODE_WCD9340,
	  SLIM_DEV_IDX_WCD9340, SLIM_DEV_INSTANCE_ID_WCD9340 },
	{}
};

static struct slim_driver wcd9335_slim_driver = {
	.driver = {
		.name = "wcd9335-slim",
	},
	.probe = wcd9335_slim_probe,
	.remove = wcd9335_slim_remove,
	.device_status = wcd9335_slim_status,
	.id_table = wcd9335_slim_id,
};

module_slim_driver(wcd9335_slim_driver);
MODULE_DESCRIPTION("WCD9335 slim driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("slim:217:250:*");
MODULE_AUTHOR("Srinivas Kandagatla <srinivas.kandagatla@linaro.org>");
