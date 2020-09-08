/*
 * cyttsp5_i2c.c
 * Parade TrueTouch(TM) Standard Product V5 I2C Module.
 * For use with Parade touchscreen controllers.
 * Supported parts include:
 * CYTMA5XX
 * CYTMA448
 * CYTMA445A
 * CYTT21XXX
 * CYTT31XXX
 *
 * Copyright (C) 2015 Parade Technologies
 * Copyright (C) 2012-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Parade Technologies at www.paradetech.com <ttdrivers@paradetech.com>
 *
 */

#include "cyttsp5_regs.h"

#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/init-input.h>

#define CY_I2C_DATA_SIZE  (2 * 256)
/////////hxm add /////////////////////////////////////////////////
struct ctp_config_info config_info = {
	.input_type = CTP_TYPE,
	.name = NULL,
	.int_number = 0,
};
int screen_max_x = 0;
int screen_max_y = 0;
int revert_x_flag = 0;
int revert_y_flag = 0;
int exchange_x_y_flag = 0;		
__u32 twi_id = 0;
#define CYTTSP4_I2C_ADDR 0x24
static const unsigned short normal_i2c[2] = {CYTTSP4_I2C_ADDR, I2C_CLIENT_END};
/* cyttsp */
#include <linux/cyttsp5_core.h>
#include <linux/cyttsp5_platform.h>

#define CYTTSP5_USE_I2C
/* #define CYTTSP5_USE_SPI */

#ifdef CYTTSP5_USE_I2C
#define CYTTSP5_I2C_TCH_ADR 0x24
#define CYTTSP5_LDR_TCH_ADR 0x24
#define CYTTSP5_I2C_IRQ_GPIO 47 /* J6.9, C19, GPMC_AD14/GPIO_38 */
#define CYTTSP5_I2C_RST_GPIO 48 /* J6.10, D18, GPMC_AD13/GPIO_37 */
#endif

#ifdef CYTTSP5_USE_SPI
/* Change GPIO numbers when using I2C and SPI at the same time
 * Following is possible alternative:
 * IRQ: J6.17, C18, GPMC_AD12/GPIO_36
 * RST: J6.24, D17, GPMC_AD11/GPIO_35
 */
#define CYTTSP5_SPI_IRQ_GPIO 38 /* J6.9, C19, GPMC_AD14/GPIO_38 */
#define CYTTSP5_SPI_RST_GPIO 37 /* J6.10, D18, GPMC_AD13/GPIO_37 */
#endif

/* Check GPIO numbers if both I2C and SPI are enabled */
#if defined(CYTTSP5_USE_I2C) && defined(CYTTSP5_USE_SPI)
#if CYTTSP5_I2C_IRQ_GPIO == CYTTSP5_SPI_IRQ_GPIO || \
	CYTTSP5_I2C_RST_GPIO == CYTTSP5_SPI_RST_GPIO
#error "GPIO numbers should be different when both I2C and SPI are on!"
#endif
#endif

#ifndef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT

#define CYTTSP5_HID_DESC_REGISTER 1

#define CY_VKEYS_X 720
#define CY_VKEYS_Y 1280
#define CY_MAXX 1872//880
#define CY_MAXY 1404//1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_W 255
#define CY_PROXIMITY_MIN_VAL	0
#define CY_PROXIMITY_MAX_VAL	1

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

/* Button to keycode conversion */
static u16 cyttsp5_btn_keys[] = {
	/* use this table to map buttons to keycodes (see input.h) */
	KEY_HOMEPAGE,		/* 172 */ /* Previously was KEY_HOME (102) */
				/* New Android versions use KEY_HOMEPAGE */
	KEY_MENU,		/* 139 */
	KEY_BACK,		/* 158 */
	KEY_SEARCH,		/* 217 */
	KEY_VOLUMEDOWN,		/* 114 */
	KEY_VOLUMEUP,		/* 115 */
	KEY_CAMERA,		/* 212 */
	KEY_POWER		/* 116 */
};

static struct touch_settings cyttsp5_sett_btn_keys = {
	.data = (uint8_t *)&cyttsp5_btn_keys[0],
	.size = ARRAY_SIZE(cyttsp5_btn_keys),
	.tag = 0,
};

static struct cyttsp5_core_platform_data _cyttsp5_core_platform_data = {
	.irq_gpio = CYTTSP5_I2C_IRQ_GPIO,
	.rst_gpio = CYTTSP5_I2C_RST_GPIO,
	.hid_desc_register = CYTTSP5_HID_DESC_REGISTER,
	.xres = cyttsp5_xres,
	.init = cyttsp5_init,
	.power = cyttsp5_power,
	.detect = cyttsp5_detect,
	.irq_stat = cyttsp5_irq_stat,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Parade Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL,	/* &cyttsp5_sett_param_regs, */
		NULL,	/* &cyttsp5_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp5_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp5_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		&cyttsp5_sett_btn_keys,	/* button-to-keycode table */
	},
	.flags = CY_CORE_FLAG_RESTORE_PARAMETERS,
	.easy_wakeup_gesture = CY_CORE_EWG_NONE,
};

static const int16_t cyttsp5_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -127, 127, 0, 0,
	ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0,
	ABS_MT_DISTANCE, 0, 255, 0, 0,	/* Used with hover */
};

struct touch_framework cyttsp5_framework = {
	.abs = (uint16_t *)&cyttsp5_abs[0],
	.size = ARRAY_SIZE(cyttsp5_abs),
	.enable_vkeys = 0,
};

static struct cyttsp5_mt_platform_data _cyttsp5_mt_platform_data = {
	.frmwrk = &cyttsp5_framework,
	.flags = 0,//CY_MT_FLAG_INV_X | CY_MT_FLAG_INV_Y,
	.inp_dev_name = CYTTSP5_MT_NAME,
	.vkeys_x = CY_VKEYS_X,
	.vkeys_y = CY_VKEYS_Y,
};

static struct cyttsp5_btn_platform_data _cyttsp5_btn_platform_data = {
	.inp_dev_name = CYTTSP5_BTN_NAME,
};

static const int16_t cyttsp5_prox_abs[] = {
	ABS_DISTANCE, CY_PROXIMITY_MIN_VAL, CY_PROXIMITY_MAX_VAL, 0, 0,
};

struct touch_framework cyttsp5_prox_framework = {
	.abs = (uint16_t *)&cyttsp5_prox_abs[0],
	.size = ARRAY_SIZE(cyttsp5_prox_abs),
};

static struct cyttsp5_proximity_platform_data
		_cyttsp5_proximity_platform_data = {
	.frmwrk = &cyttsp5_prox_framework,
	.inp_dev_name = CYTTSP5_PROXIMITY_NAME,
};

static struct cyttsp5_platform_data _cyttsp5_platform_data = {
	.core_pdata = &_cyttsp5_core_platform_data,
	.mt_pdata = &_cyttsp5_mt_platform_data,
	.loader_pdata = &_cyttsp5_loader_platform_data,
	.btn_pdata = &_cyttsp5_btn_platform_data,
	.prox_pdata = &_cyttsp5_proximity_platform_data,
};

static ssize_t cyttsp5_virtualkeys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		__stringify(EV_KEY) ":"
		__stringify(KEY_BACK) ":1360:90:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_MENU) ":1360:270:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_HOMEPAGE) ":1360:450:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_SEARCH) ":1360:630:160:180"
		"\n");
}

static struct kobj_attribute cyttsp5_virtualkeys_attr = {
	.attr = {
		.name = "virtualkeys.cyttsp5_mt",
		.mode = S_IRUGO,
	},
	.show = &cyttsp5_virtualkeys_show,
};

static struct attribute *cyttsp5_properties_attrs[] = {
	&cyttsp5_virtualkeys_attr.attr,
	NULL
};

static struct attribute_group cyttsp5_properties_attr_group = {
	.attrs = cyttsp5_properties_attrs,
};
#endif /* !CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT */

////////end////////////////////////////////////////////////////////

#if 1
#define CYDBG(s...)
#define xdump(func,buf,size,rc)
#else
#define CYDBG(s...) printk(s)
static inline void xdump(char *func, void *buf, int size, int rc)
{
	uint8_t *data = (uint8_t *)buf;
	char tmpbuf[256];
	int i;

	tmpbuf[0] = 0;
	for (i=0; i<size && i<sizeof(tmpbuf)/3; i++) {
		sprintf(tmpbuf+i*3, "%02x ", data[i]);
	}
	CYDBG("%s: (%d) [%s] rc=%d\n", func, size, tmpbuf, rc);
}
#endif

static int cyttsp5_i2c_read_default(struct device *dev, void *buf, int size)
{
	struct i2c_client *client = to_i2c_client(dev);
	int rc;

	if (!buf || !size || size > CY_I2C_DATA_SIZE)
		return -EINVAL;

	rc = i2c_master_recv(client, buf, size);

	xdump("CY:READ", buf, size, rc);

	return (rc < 0) ? rc : rc != size ? -EIO : 0;
}

static int cyttsp5_i2c_read_default_nosize(struct device *dev, u8 *buf, u32 max)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;
	u32 size;

	if (!buf) {
		CYDBG("CY:READNS buf=null\n");
		return -EINVAL;
	}

	memset(buf, 0, max);

	msgs[0].addr = client->addr;
	msgs[0].flags = (client->flags & I2C_M_TEN) | I2C_M_RD;
	msgs[0].len = 2;
	msgs[0].buf = buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);
	if (rc < 0 || rc != msg_count) {
		CYDBG("CY:READNS rc=%d\n", rc);
		return (rc < 0) ? rc : -EIO;
	}

	size = get_unaligned_le16(&buf[0]);
	if (!size || size == 2 || size >= CY_PIP_1P7_EMPTY_BUF) {
		/* Before PIP 1.7, empty buffer is 0x0002;
		From PIP 1.7, empty buffer is 0xFFXX */
		CYDBG("CY:READNS empty\n", size);
		return 0;
	}

	if (size > max) {
		CYDBG("CY:READNS size=%d max=%d\n", size, max);
		return -EINVAL;
	}

	rc = i2c_master_recv(client, buf, size);

	xdump("CY:READNS", buf, size, rc);

	return (rc < 0) ? rc : rc != (int)size ? -EIO : 0;
}

static int cyttsp5_i2c_write_read_specific(struct device *dev, u8 write_len,
		u8 *write_buf, u8 *read_buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;

	if (!write_buf || !write_len)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].len = write_len;
	msgs[0].buf = write_buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);

	xdump("CY:WRITE", write_buf, write_len, rc);

	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;

	rc = 0;

	if (read_buf)
		rc = cyttsp5_i2c_read_default_nosize(dev, read_buf,
				CY_I2C_DATA_SIZE);

	return rc;
}

static struct cyttsp5_bus_ops cyttsp5_i2c_bus_ops = {
	.bustype = BUS_I2C,
	.read_default = cyttsp5_i2c_read_default,
	.read_default_nosize = cyttsp5_i2c_read_default_nosize,
	.write_read_specific = cyttsp5_i2c_write_read_specific,
};

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
static const struct of_device_id cyttsp5_i2c_of_match[] = {
	{ .compatible = "cy,cyttsp5_i2c_adapter", },
	{ }
};
MODULE_DEVICE_TABLE(of, cyttsp5_i2c_of_match);
#endif

static int cyttsp5_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	int rc;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C functionality not Supported\n");
		return -EIO;
	}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp5_i2c_of_match), dev);
	if (match) {
		rc = cyttsp5_devtree_create_and_get_pdata(dev);
		if (rc < 0)
			return rc;
	}
#endif

	rc = cyttsp5_probe(&cyttsp5_i2c_bus_ops, &client->dev, client->irq,
			  CY_I2C_DATA_SIZE);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	if (rc && match)
		cyttsp5_devtree_clean_pdata(dev);
#endif

	return rc;
}

static int cyttsp5_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	struct device *dev = &client->dev;
	const struct of_device_id *match;
#endif
	struct cyttsp5_core_data *cd = i2c_get_clientdata(client);

	cyttsp5_release(cd);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp5_i2c_of_match), dev);
	if (match)
		cyttsp5_devtree_clean_pdata(dev);
#endif

	return 0;
}

static const struct i2c_device_id cyttsp5_i2c_id[] = {
	{ CYTTSP5_I2C_NAME, 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cyttsp5_i2c_id);


int ctp_i2c_write_bytes(struct i2c_client *client, uint8_t *data, uint16_t len)
{
	struct i2c_msg msg;
	int ret=-1;
	
	msg.flags = !I2C_M_RD;
	msg.addr = client->addr;
	msg.len = len;
	msg.buf = data;		
	
	ret=i2c_transfer(client->adapter, &msg,1);
	return ret;
}
bool ctp_i2c_test(struct i2c_client * client)
{
	int ret,retry;
	uint8_t test_data[1] = { 0 };	//only write a data address. 12
	for(retry=0; retry < 12; retry++)
	{
		ret =ctp_i2c_write_bytes(client, test_data, 1);	//Test i2c.
		printk(" retry =%d ret =%d \n",retry,ret);
		if (ret == 1)
		break;
		msleep(10);
	}
	//ret =1;
	return ret==1 ? true : false;
}
int ctp_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	int ret;
	struct i2c_adapter *adapter = client->adapter;
	printk("\n");
	if( config_info.twi_id == adapter->nr)
	{
		printk("Detected chip %s at adapter %d, address 0x%02x\n",
			  CYTTSP5_I2C_NAME, i2c_adapter_id(adapter), client->addr);
       // ret = ctp_i2c_test(client);
		//printk(" ctp_i2c_test =%d\n ",ret);
       // if(!ret){
        	//pr_info("%s:I2C connection might be something wrong --------------------\n",__func__);
        	//return -ENODEV;
       // }else
              //{      
            //printk("I2C connection sucess==============================!\n");
		strlcpy(info->type, CYTTSP5_I2C_NAME, I2C_NAME_SIZE);
	        //hxm add
	        info->platform_data = &_cyttsp5_platform_data;
	//}
		
		return 0;
	}else{
		return -ENODEV;
	}
}


static struct i2c_driver cyttsp5_i2c_driver = {
        .class = I2C_CLASS_HWMON,
	.driver = {
		.name = CYTTSP5_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &cyttsp5_pm_ops,
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
		.of_match_table = cyttsp5_i2c_of_match,
#endif
	},
	.probe = cyttsp5_i2c_probe,
	.remove = cyttsp5_i2c_remove,
	.id_table = cyttsp5_i2c_id,
	.address_list	= normal_i2c,
	.detect   = ctp_detect,
};
void ctp_print_info(struct ctp_config_info info)
{
	
		printk("info.ctp_used:%d\n",info.ctp_used);
		printk("info.ctp_name:%s\n",info.name);
		printk("info.twi_id:%d\n",info.twi_id);
		printk("info.screen_max_x:%d\n",info.screen_max_x);
		printk("info.screen_max_y:%d\n",info.screen_max_y);
		printk("info.revert_x_flag:%d\n",info.revert_x_flag);
		printk("info.revert_y_flag:%d\n",info.revert_y_flag);
		printk("info.exchange_x_y_flag:%d\n",info.exchange_x_y_flag);
		printk("info.irq_gpio_number:%d\n",info.irq_gpio.gpio);
		printk("info.wakeup_gpio_number:%d\n",info.wakeup_gpio.gpio);
	
}
static int ctp_get_system_config(void)
{
    ctp_print_info(config_info);

    printk("%s:fwname:%s\n",__func__,config_info.name);


    twi_id = config_info.twi_id;
    screen_max_x = config_info.screen_max_x;
    screen_max_y = config_info.screen_max_y;
    revert_x_flag = config_info.revert_x_flag;
    revert_y_flag = config_info.revert_y_flag;
    exchange_x_y_flag = config_info.exchange_x_y_flag;
	if((screen_max_x == 0) || (screen_max_y == 0)){
           printk("%s:read config error!\n",__func__);
           return 0;
    }
    return 1;
}
#if 0
struct regulator *TP_3V_regulator;
//hxm add ELD03  =3.3v
void set_tp_3V_vol(void){	
	int ret = 0;        
	TP_3V_regulator = regulator_get(NULL, "axp227_eldo3");
	if (IS_ERR(TP_3V_regulator)) 
	{		regulator_put(TP_3V_regulator);
	printk("hxm Unable to TP_3V_regulator.err = 0x%x\n", (int)TP_3V_regulator);
	}else{  
	printk("hxm  get TP_3V_regulator.ok = 0x%x\n", (int)TP_3V_regulator);
	regulator_set_voltage(TP_3V_regulator,	(int)(3300)*1000,(int)(3300)*1000);	}
	ret = regulator_enable( TP_3V_regulator);	
	if (IS_ERR((void *)ret)) 
	{
	printk("Unable to enable TP_3V_regulator err = 0x%x\n", ret);
	}	
	msleep(1);
}
#endif
void  ctp_wakeup(void)
{
	__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
	msleep(20);
	__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
	msleep(20);
	__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
	msleep(2);
}

//END
static int __init cyttsp5_i2c_init(void)
{
	int ret = -1;  
	
        if (input_fetch_sysconfig_para(&(config_info.input_type))) {
		printk("%s: ctp_fetch_sysconfig_para err.\n", __func__);
		return 0;
	} else
	{
		ret = input_init_platform_resource(&(config_info.input_type));
		if (0 != ret) {
			printk("%s:ctp_ops.init_platform_resource err. \n", __func__);    
		}
	}
	if (config_info.ctp_used == 0) {
		printk("*** ctp_used set to 0 !\n");
		printk("*** if use ctp,please put the sys_config.fex ctp_used set to 1. \n");
		return 0;
	}
	if (!ctp_get_system_config()) {
		printk("%s:read config fail!\n",__func__);
		return ret;
	}
	//enable power
	input_set_power_enable(&(config_info.input_type), 1);

	_cyttsp5_core_platform_data.irq_gpio=config_info.irq_gpio.gpio;
	_cyttsp5_core_platform_data.rst_gpio=config_info.wakeup_gpio.gpio;
	//_cyttsp5_core_platform_data.configinfo=&config_info;
         //set_tp_3V_vol();
	// msleep(20);
	//ctp_wakeup();
	ret=i2c_add_driver(&cyttsp5_i2c_driver);

	pr_info("%s: Parade TTSP I2C Driver (Built %s) ret=%d\n",
		 __func__, CY_DRIVER_VERSION, ret);
	return ret;
}
module_init(cyttsp5_i2c_init);

static void __exit cyttsp5_i2c_exit(void)
{
	i2c_del_driver(&cyttsp5_i2c_driver);
	input_free_platform_resource(&(config_info.input_type));
}
module_exit(cyttsp5_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Parade TrueTouch(R) Standard Product I2C driver");
MODULE_AUTHOR("Parade Technologies <ttdrivers@paradetech.com>");
