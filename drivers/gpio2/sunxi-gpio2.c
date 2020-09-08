/*
 * sunxi-gpio2.c 
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
#include <linux/power/axp_depend.h>
#include <linux/sys_config.h>



#include "sunxi-gpio2.h"

#define PMU_NUM (0)
static int gpio_index;
static int gpio2_irq_handler(int irq, void *data)
{	
	//do nothing
	return 0;
}


static int enable_gpio2_irq(struct platform_device *pdev)
{
	int ret;
	
	struct gpio_config config;

	gpio_index = of_get_named_gpio_flags(pdev->dev.of_node, "enable_gpio2", 0, (enum of_gpio_flags *)&config);
	if (!gpio_is_valid(gpio_index)) {
		dev_err(&pdev->dev, "get gpio test_gpio failed\n");
		return -EINVAL;
	}

	printk(KERN_INFO "pn_debug gpio_index=%d\n", gpio_index);
	ret = axp_gpio_irq_request(PMU_NUM, gpio_index, gpio2_irq_handler, NULL);
	if (ret) {
		printk(KERN_ERR "pn_debug request irq failed !\n");
		return -EINVAL;
	}

	axp_gpio_irq_set_type(PMU_NUM, gpio_index, AXP_GPIO_IRQF_TRIGGER_FALLING);
	printk(KERN_INFO "pn_debug set axp gpio trigger falling\n");

	axp_gpio_irq_enable(PMU_NUM, gpio_index);
	printk(KERN_INFO "pn_debug enable axp gpio interrupt\n");

	printk(KERN_INFO "pn_debug request irq ok !\n");
	return 0;
}

static int disable_gpio2_irq(int gpio_index)
{
	int ret;
	
	axp_gpio_irq_disable(PMU_NUM, gpio_index);
	printk(KERN_INFO "axp_gpio_irq_disable ok \n");
	return 0;
}

static int sunxi_gpio2_probe(struct platform_device *pdev)
{
	printk(KERN_INFO "sunxi_gpio2_probe \n");
	enable_gpio2_irq(pdev);
	return 0;
}

static int sunxi_gpio2_remove(struct platform_device *pdev)
{	
	printk(KERN_INFO "sunxi_gpio2_remove \n");
	disable_gpio2_irq(gpio_index);
	return 0;
}

static const struct of_device_id sunxi_gpio2_ids[] = {
	{ .compatible = "allwinner,sunxi-gpio2" },
	{ /* Sentinel */ }
};

static struct platform_driver sunxi_gpio2_driver = {
	.probe	= sunxi_gpio2_probe,
	.remove	= sunxi_gpio2_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "sunxi-gpio2",
		.of_match_table	= sunxi_gpio2_ids,
	},
};

static int __init sunxi_gpio2_init(void){	
	printk(KERN_INFO "sunxi gpio2 init begin \n");	
	return platform_driver_register(&sunxi_gpio2_driver);
}
static void __exit sunxi_gpio2_exit(void){
	printk(KERN_INFO "sunxi gpio2 exit \n");
	platform_driver_unregister(&sunxi_gpio2_driver);
}
module_init(sunxi_gpio2_init);
module_exit(sunxi_gpio2_exit);


MODULE_AUTHOR("Soft-Reuuimlla");
MODULE_DESCRIPTION("Driver for Allwinner gpio2 irq controller");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:gpio2-sunxi");

