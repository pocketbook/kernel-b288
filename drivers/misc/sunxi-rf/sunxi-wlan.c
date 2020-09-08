/*
 * sunxi-wlan.c -- power on/off wlan part of SoC
 *
 * Copyright (c) 2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Wei Li<liwei@allwinnertech.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/rfkill.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/capability.h>
#include <linux/of_irq.h>
#include <asm/io.h>

struct sunxi_wlan_platdata {
	int bus_index;
	int wlan_irq_num;
	struct clk 	*wlan_cfg;
	/* used to supply PA part of wlan */
	struct regulator *wlan_pa_power;
	/* used to supply core part of wlan */
	struct regulator *wlan_core_power;

	char *wlan_pa_power_name;
	char *wlan_core_power_name;

	int power_state;
	struct platform_device *pdev;
};
static struct sunxi_wlan_platdata *wlan_data;

static int sunxi_wlan_on(struct sunxi_wlan_platdata *data, int on_off);
static DEFINE_MUTEX(sunxi_wlan_mutex);

void sunxi_wlan_set_power(int on_off)
{
	struct platform_device *pdev;
	int ret = 0;
	BUG_ON(!wlan_data);

	pdev = wlan_data->pdev;
	mutex_lock(&sunxi_wlan_mutex);
	if (on_off != wlan_data->power_state) {
		ret = sunxi_wlan_on(wlan_data, on_off);
		if (ret)
			dev_err(&pdev->dev, "set wlan power failed\n");
	}
	mutex_unlock(&sunxi_wlan_mutex);
}
EXPORT_SYMBOL_GPL(sunxi_wlan_set_power);

int sunxi_wlan_get_bus_index(void)
{
	struct platform_device *pdev;
	BUG_ON(!wlan_data);

	pdev = wlan_data->pdev;
	dev_info(&pdev->dev, "bus_index: %d\n", wlan_data->bus_index);
	return wlan_data->bus_index;
}
EXPORT_SYMBOL_GPL(sunxi_wlan_get_bus_index);

int sunxi_wlan_get_irq(void)
{
	BUG_ON(!wlan_data);
	return wlan_data->wlan_irq_num;
}
EXPORT_SYMBOL_GPL(sunxi_wlan_get_irq);

static int sunxi_wlan_regulator_set(struct regulator **wlan_regulator, struct device *dev, char *regulator_name, int on_off)
{
	int ret = 0;
	if (!regulator_name)
		return -EINVAL;
	*wlan_regulator = regulator_get(dev, regulator_name);
	if (IS_ERR(*wlan_regulator)) {
		dev_err(dev, "get regulator %s failed!\n", regulator_name);
		return -EINVAL;
	}
	if (on_off) {
		ret = regulator_enable(*wlan_regulator);
		if (ret < 0) {
			dev_err(dev, "regulator %s enable failed!\n", regulator_name);
			regulator_put(*wlan_regulator);
		}
		ret = regulator_get_voltage(*wlan_regulator);
		if (ret < 0) {
			dev_err(dev, "regulator %s get voltage failed!\n", regulator_name);
			regulator_put(*wlan_regulator);
			return ret;
		}
		dev_info(dev, "check wlan %s voltage: %d\n", regulator_name, ret);
	} else {
		ret = regulator_disable(*wlan_regulator);
		if (ret < 0) {
			dev_err(dev, "regulator %s disable failed\n", regulator_name);
			regulator_put(*wlan_regulator);
			return ret;
		}
	}
	regulator_put(*wlan_regulator);
	return ret;
}
static int sunxi_wlan_on(struct sunxi_wlan_platdata *data, int on_off)
{
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	int ret = 0;

	ret = sunxi_wlan_regulator_set(&data->wlan_pa_power, dev, data->wlan_pa_power_name, on_off);
	if (ret < 0)
		return -EINVAL;
	ret = sunxi_wlan_regulator_set(&data->wlan_core_power, dev, data->wlan_core_power_name, on_off);
	if (ret < 0)
		return -EINVAL;

	if (on_off) {
		ret = clk_prepare_enable(data->wlan_cfg);
		if (ret < 0) {
			dev_err(dev, "can't reset wlan and 32K clock!\n");
		}
	} else {
		clk_disable_unprepare(data->wlan_cfg);
	}
	wlan_data->power_state = on_off;

	return 0;
}

static ssize_t power_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", wlan_data->power_state);
}

static ssize_t power_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long state;
	int err;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	err = kstrtoul(buf, 0, &state);
	if (err)
		return err;

	if (state > 1)
		return -EINVAL;

	mutex_lock(&sunxi_wlan_mutex);
	if (state != wlan_data->power_state) {
		err = sunxi_wlan_on(wlan_data, state);
		if (err)
			dev_err(dev, "set power failed\n");
	}
	mutex_unlock(&sunxi_wlan_mutex);

	return count;
}

static DEVICE_ATTR(power_state, S_IRUGO | S_IWUSR,
	power_state_show, power_state_store);

static int sunxi_wlan_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct sunxi_wlan_platdata *data;
	u32 val;
	const char *pa_power, *core_power;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	data->pdev = pdev;
	wlan_data = data;

	data->bus_index = -1;
	if (!of_property_read_u32(np, "wlan_busnum", &val)) {
		switch (val) {
		case 0:
		case 1:
		case 2:
			data->bus_index = val;
			break;
		default:
			dev_err(dev, "unsupported wlan_busnum (%u)\n", val);
			devm_kfree(dev, (void *)wlan_data);
			return -EINVAL;
		}
	}
	dev_info(dev, "wlan_busnum (%u)\n", val);
	data->wlan_irq_num = irq_of_parse_and_map(np, 0);
	if (data->wlan_irq_num == NO_IRQ) {
		dev_err(dev, "wlan_irq get failed!\n");
		devm_kfree(dev, (void *)wlan_data);
		return -EINVAL;
	}
	dev_info(dev, "wlan_irq (%u)\n", data->wlan_irq_num);

	if (of_property_read_string(np, "wlan_pa_power", &pa_power)) {
		dev_warn(dev, "Missing wlan_pa_power.\n");
	} else {
		data->wlan_pa_power_name = devm_kzalloc(dev, strlen(pa_power) + 1, GFP_KERNEL);
		if (!data->wlan_pa_power_name) {
			devm_kfree(dev, (void *)wlan_data);
			return -ENOMEM;
		} else
			strncpy(data->wlan_pa_power_name, pa_power, strlen(pa_power));
	}
	dev_info(dev, "wlan_pa_power_name (%s)\n", data->wlan_pa_power_name);

	if (of_property_read_string(np, "wlan_core_power", &core_power)) {
		dev_warn(dev, "Missing wlan_core_power.\n");
	} else {
		data->wlan_core_power_name = devm_kzalloc(dev, strlen(core_power) + 1, GFP_KERNEL);
		if (!data->wlan_core_power_name) {
			devm_kfree(dev, (void *)wlan_data->wlan_pa_power_name);
			devm_kfree(dev, (void *)wlan_data);
			return -ENOMEM;
		} else
			strncpy(data->wlan_core_power_name, core_power, strlen(core_power));
	}
	dev_info(dev, "wlan_core_power_name (%s)\n", data->wlan_core_power_name);

	data->wlan_cfg = of_clk_get(np, 0);
	if (IS_ERR_OR_NULL(data->wlan_cfg)) {
		dev_err(dev, "clk not config\n");
	}
	device_create_file(dev, &dev_attr_power_state);
	data->power_state = 0;

	return 0;
}

static int sunxi_wlan_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_power_state);
	if (!!wlan_data->wlan_core_power_name) {
		devm_kfree(&pdev->dev, (void *)wlan_data->wlan_core_power_name);
	}
	if (!!wlan_data->wlan_pa_power_name) {
		devm_kfree(&pdev->dev, (void *)wlan_data->wlan_pa_power_name);
	}
	if (!IS_ERR_OR_NULL(wlan_data->wlan_cfg)) {
		clk_put(wlan_data->wlan_cfg);
	}
	if (!!wlan_data) {
		devm_kfree(&pdev->dev, (void *)wlan_data);
	}
	return 0;
}

static const struct of_device_id sunxi_wlan_ids[] = {
	{ .compatible = "allwinner,sunxi-wlan" },
	{ /* Sentinel */ }
};

static struct platform_driver sunxi_wlan_driver = {
	.probe	= sunxi_wlan_probe,
	.remove	= sunxi_wlan_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "sunxi-wlan",
		.of_match_table	= sunxi_wlan_ids,
	},
};

module_platform_driver(sunxi_wlan_driver);

MODULE_DESCRIPTION("sunxi wlan driver");
MODULE_LICENSE(GPL);