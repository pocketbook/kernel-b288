/*
 * sunxi-gpio0.c 
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
#include <linux/power/scenelock.h>


#include "sunxi-gpio0.h"

static int gpio_index;

#define PMU_NUM (0)
extern int  aw_send_hall_sensor_near_key(void);
extern int  aw_send_hall_sensor_leave_key(void);

extern int axp22_read_gpio0_status(void);
static int gpio0_irq_handler(int irq, void *data)
{	
	//do nothing
	int status;
	//int old_status;
	status =  axp22_read_gpio0_status();
	printk("** AXP_GPIO0 IRQ:%d **\n", status);
	/*if(status == 1 && (check_scene_locked(SCENE_SUPER_STANDBY) == 0)){
		aw_send_wakeup_key();
	}
	if(status == 0 && (check_scene_locked(SCENE_SUPER_STANDBY) == 1)){
		aw_send_wakeup_key();
	}*/
	if(status == 1 /*&& (check_scene_locked(SCENE_SUPER_STANDBY) == 0)*/){
		aw_send_hall_sensor_leave_key();
	}
	if(status == 0/* && (check_scene_locked(SCENE_SUPER_STANDBY) == 1)*/){
		aw_send_hall_sensor_near_key();
	}
	return 0;
}


static int enable_gpio0_irq(struct platform_device *pdev)
{
	int ret;
	
	struct gpio_config config;

	gpio_index = of_get_named_gpio_flags(pdev->dev.of_node, "enable_gpio0", 0, (enum of_gpio_flags *)&config);
	if (!gpio_is_valid(gpio_index)) {
		dev_err(&pdev->dev, "get gpio enable_gpio0 failed\n");
		return -EINVAL;
	}

	printk(KERN_INFO "pn_debug gpio_index=%d\n", gpio_index);
	ret = axp_gpio_irq_request(PMU_NUM, gpio_index, gpio0_irq_handler, NULL);
	if (ret) {
		printk(KERN_ERR "pn_debug request irq failed !\n");
		return -EINVAL;
	}

	axp_gpio_irq_set_type(PMU_NUM, gpio_index, AXP_GPIO_IRQF_TRIGGER_FALLING|AXP_GPIO_IRQF_TRIGGER_RISING);

	axp_gpio_irq_enable(PMU_NUM, gpio_index);

	printk("AXP_GPIO0 enable\n");
	return 0;
}

static int disable_gpio0_irq(int gpio_index)
{
	int ret;
	
	axp_gpio_irq_disable(PMU_NUM, gpio_index);
	printk("AXP_GPIO0 disable\n");
	return 0;
}

static int sunxi_gpio0_probe(struct platform_device *pdev)
{
	printk("AXP_GPIO0 probe\n");
	enable_gpio0_irq(pdev);
	return 0;
}

static int sunxi_gpio0_remove(struct platform_device *pdev)
{	
	disable_gpio0_irq(gpio_index);
	return 0;
}

static const struct of_device_id sunxi_gpio0_ids[] = {
	{ .compatible = "allwinner,sunxi-gpio0" },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, sunxi_gpio0_ids);

static struct platform_driver sunxi_gpio0_driver = {
	.probe	= sunxi_gpio0_probe,
	.remove	= sunxi_gpio0_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "sunxi-gpio0",
		.of_match_table	= sunxi_gpio0_ids,
	},
};

static int __init sunxi_gpio0_init(void){	
	printk("AXP_GPIO0 init\n");
	return platform_driver_register(&sunxi_gpio0_driver);
}
static void __exit sunxi_gpio0_exit(void){
	platform_driver_unregister(&sunxi_gpio0_driver);
}
module_init(sunxi_gpio0_init);
module_exit(sunxi_gpio0_exit);


MODULE_AUTHOR("Soft-Reuuimlla");
MODULE_DESCRIPTION("Driver for Allwinner gpio0 irq controller");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:gpio0-sunxi");

