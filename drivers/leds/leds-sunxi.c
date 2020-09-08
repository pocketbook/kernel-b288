/*
 * Copyright (C) 2016 Allwinnertech
 * guoguo <guoguo@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/sys_config.h>
#include <linux/of_gpio.h>
#include <linux/mm.h>
#include <linux/slab.h>
struct gpio_led gpio_leds[] = {
		{
				.name                   = "default_led",
				.default_trigger        = "none",
				.gpio                   = 0xffff,
				.default_state			= LEDS_GPIO_DEFSTATE_ON,
				.retain_state_suspended = 1,
		}
};

void led_dev_release(struct device *dev)  
{  
        pr_notice("leds release\n"); 
}

static struct gpio_led_platform_data gpio_led_info = {
	.leds		= gpio_leds,
	.num_leds	= 0,
};

static struct platform_device sunxi_leds = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_info,
		.release = led_dev_release,
	}
};

static int sunxi_leds_fetch_sysconfig_para(void)
{
	int ret = -1;
	int num = 0,i = 0;
	int val;
	//script_item_value_type_e  type;
	static char* led_name = "white_led";
	char led_active_low[25];

	struct device_node *np = NULL;
	struct gpio_config led_io;

	np = of_find_node_by_name(NULL,"leds_para");
	if (!np) {
		pr_err("ERROR! get leds_para failed, func:%s, line:%d\n",__FUNCTION__, __LINE__);
		goto script_get_err;
	}

	if (!of_device_is_available(np)) {
		pr_err("%s: leds is not available\n", __func__);
		goto script_get_err;
	}

	ret = of_property_read_u32(np, "leds_used", &val);
	if (ret) {
		pr_err("%s:get leds_used is fail, %d\n",__func__, ret);
		goto script_get_err;
	}

	if(!val){
		pr_err("%s: leds is not used\n", __func__);
		return -1;
	}

	led_io.gpio = of_get_named_gpio_flags(np, led_name, 0, (enum of_gpio_flags *)&led_io);
	if (!gpio_is_valid(led_io.gpio)) {
		pr_err("%s: %s is invalid. \n",__func__,led_name);
		goto script_get_err;
	} else {
		gpio_leds[num].name = led_name;
		gpio_leds[num].gpio = led_io.gpio;
		gpio_leds[num].default_state = led_io.data ? LEDS_GPIO_DEFSTATE_ON : LEDS_GPIO_DEFSTATE_OFF;
		sprintf(led_active_low,"%s_active_low", led_name);
		ret = of_property_read_u32(np, led_active_low, &val);
		if (ret) {
			pr_err("%s:get led_active_low is fail, %d\n",__func__, ret);
			goto script_get_err;
		} else
			gpio_leds[num].active_low = val ? true : false;
		pr_notice("%s %d %d %s:%d\n",led_name,led_io.gpio,led_io.data,led_active_low,val);
		num++;
	}

	return num;
	script_get_err:
	pr_notice("=========script_get_err============\n");
	return -1;
}

static int __init sunxi_leds_init(void)
{
        int led_num = 0,ret = 0;
		pr_notice("=========sunxi_leds_init start============\n");
        led_num = sunxi_leds_fetch_sysconfig_para();
        if(led_num > 0){
                gpio_led_info.num_leds = led_num;
                ret = platform_device_register(&sunxi_leds);
        }
		pr_notice("=========sunxi_leds_init done============\n");
        return ret;
}

static void __exit sunxi_leds_exit(void)
{
		pr_notice("=========sunxi_leds_exit============\n");
        if(gpio_led_info.num_leds > 0){
                platform_device_unregister(&sunxi_leds);
        }
}

module_init(sunxi_leds_init);
module_exit(sunxi_leds_exit);

MODULE_DESCRIPTION("sunxi leds driver");
MODULE_AUTHOR("Guoguo");
MODULE_LICENSE("GPL");



