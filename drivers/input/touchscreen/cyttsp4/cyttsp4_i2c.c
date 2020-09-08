/*
 * cyttsp4_i2c.c
 * Cypress TrueTouch(TM) Standard Product V4 I2C Driver module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012-2014 Cypress Semiconductor
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
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp4_regs.h"
#include <linux/i2c.h>
#include <linux/version.h>

#include "cyttsp4_platform.h"


#include <linux/cyttsp4_core.h>


#include "b188_systconfig.h"

#define CY_I2C_DATA_SIZE  (2 * 256)


 //end











////////////////////end



////////////////////////////////////////////////////////////////////////
static int cyttsp4_i2c_read_block_data(struct device *dev, u16 addr,
	int length, void *values, int max_xfer)
{
	struct i2c_client *client = to_i2c_client(dev);
	int trans_len;
	u8 client_addr;
	u8 addr_lo;
	struct i2c_msg msgs[2];
	int rc = -EINVAL;

	while (length > 0) {
		client_addr = client->addr | ((addr >> 8) & 0x1);
		addr_lo = addr & 0xFF;
		trans_len = min(length, max_xfer);

		memset(msgs, 0, sizeof(msgs));
		msgs[0].addr = client_addr;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &addr_lo;

		msgs[1].addr = client_addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = trans_len;
		msgs[1].buf = values;

		rc = i2c_transfer(client->adapter, msgs, 2);
		if (rc != 2)
		{
		    printk("msg %s cyttsp4_i2c_read_block_data error: %d\n", __func__, rc);
			goto exit;
		}

		length -= trans_len;
		values += trans_len;
		addr += trans_len;
	}

exit:
	return (rc < 0) ? rc : rc != ARRAY_SIZE(msgs) ? -EIO : 0;
}

static int cyttsp4_i2c_write_block_data(struct device *dev, u16 addr,
	u8 *wr_buf, int length, const void *values, int max_xfer)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 client_addr;
	u8 addr_lo;
	int trans_len;
	struct i2c_msg msg;
	int rc = -EINVAL;

	while (length > 0) {
		client_addr = client->addr | ((addr >> 8) & 0x1);
		addr_lo = addr & 0xFF;
		trans_len = min(length, max_xfer);

		memset(&msg, 0, sizeof(msg));
		msg.addr = client_addr;
		msg.flags = 0;
		msg.len = trans_len + 1;
		msg.buf = wr_buf;

		wr_buf[0] = addr_lo;
		memcpy(&wr_buf[1], values, trans_len);

		/* write data */
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc != 1)
			goto exit;

		length -= trans_len;
		values += trans_len;
		addr += trans_len;
	}

exit:
	return (rc < 0) ? rc : rc != 1 ? -EIO : 0;
}

static int cyttsp4_i2c_write(struct device *dev, u16 addr, u8 *wr_buf,
	const void *buf, int size, int max_xfer)
{
	int rc;

	pm_runtime_get_noresume(dev);
	rc = cyttsp4_i2c_write_block_data(dev, addr, wr_buf, size, buf,
		max_xfer);
	pm_runtime_put_noidle(dev);

	return rc;
}

static int cyttsp4_i2c_read(struct device *dev, u16 addr, void *buf, int size,
	int max_xfer)
{
	int rc;

	pm_runtime_get_noresume(dev);
	rc = cyttsp4_i2c_read_block_data(dev, addr, size, buf, max_xfer);
	pm_runtime_put_noidle(dev);

	return rc;
}

static struct cyttsp4_bus_ops cyttsp4_i2c_bus_ops = {
	.write = cyttsp4_i2c_write,
	.read = cyttsp4_i2c_read,
};

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT
static struct of_device_id cyttsp4_i2c_of_match[] = {
	 { 
		.compatible = "cy,cyttsp4_i2c_adapter", 
	  }, 

	{ },
};

MODULE_DEVICE_TABLE(of, cyttsp4_i2c_of_match);
#endif

static int cyttsp4_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	int rc;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C functionality not Supported\n");
		return -EIO;
	}
	cytts4_printk();

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp4_i2c_of_match), dev);
	if (match) {
		rc = cyttsp4_devtree_create_and_get_pdata(dev);
		if (rc < 0)
			return rc;
	}
#endif
	cytts4_printk("\n");

	rc = cyttsp4_probe(&cyttsp4_i2c_bus_ops, &client->dev, client->irq,
			CY_I2C_DATA_SIZE);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT
	if (rc && match)
		cyttsp4_devtree_clean_pdata(dev);
#endif

	return rc;
}

static int cyttsp4_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT
	struct device *dev = &client->dev;
	const struct of_device_id *match;
#endif
	struct cyttsp4_core_data *cd = i2c_get_clientdata(client);

	cyttsp4_release(cd);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp4_i2c_of_match), dev);
	if (match)
		cyttsp4_devtree_clean_pdata(dev);
#endif

	return 0;
}


/////////////  b188

//MODULE_DEVICE_TABLE(i2c, cyttsp4_i2c_id);
static const unsigned short normal_i2c[2] = {CYTTSP4_I2C_ADDR, I2C_CLIENT_END};

///////////////////////////
static const struct i2c_device_id cyttsp4_i2c_id[] = {
	{ CYTTSP4_I2C_NAME, 0 },  { }
};

/* Addresses to scan 
 union{
	unsigned short dirty_addr_buf[2];
	const unsigned short normal_i2c[2];
}u_i2c_addr = {{ELAN_KTF2K_I2C_ADDR,I2C_CLIENT_END},};				

		*/


static struct i2c_driver cyttsp4_i2c_driver = {
    .class = I2C_CLASS_HWMON,
	.probe = cyttsp4_i2c_probe,
	.remove = cyttsp4_i2c_remove,
	.id_table = cyttsp4_i2c_id,
	.detect   = ctp_detect,
	.driver = {
		.name = CYTTSP4_I2C_NAME,
		.owner = THIS_MODULE,
		//.pm = &cyttsp4_pm_ops,
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT
		.of_match_table = cyttsp4_i2c_of_match,
#endif
	},
	/////////////////b188
	.address_list	= normal_i2c,
 ///////////////b188 end
};

#if 0
static const unsigned short normal_i2c[2] = {TPS65185_I2C_ADDR, I2C_CLIENT_END};

static struct i2c_driver tps65185_driver = {
	.class = I2C_CLASS_HWMON,
	.probe	= tps65185_probe,
	.remove 	= tps65185_remove,
	.id_table	= tps65185_id,
	.driver = {
		.name	  = TPS65185_I2C_NAME,
		.owner	  = THIS_MODULE,
	},
	.address_list	= normal_i2c,
	.detect   = TP65185_detect,
};
static struct i2c_driver ektf2k_ts_driver = {
        .class = I2C_CLASS_HWMON,
	.probe		= elan_ktf2k_ts_probe,
	.remove		= elan_ktf2k_ts_remove,
	.suspend	= elan_ktf2k_ts_suspend,
	.resume		= elan_ktf2k_ts_resume,
	.id_table	= elan_ktf2k_ts_id,
	.detect   = ctp_detect,
	.driver		= {
		.name   = ELAN_KTF2K_NAME,
		.owner  = THIS_MODULE,
	},
	.address_list   = u_i2c_addr.normal_i2c,
};

#endif




//#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
//module_i2c_driver(cyttsp4_i2c_driver);
//
//#else
static int __init cyttsp4_i2c_init(void)
{

#if 0
	int rc = i2c_add_driver(&cyttsp4_i2c_driver);

	cytts4_printk("%s: Cypress TTSP v4 I2C Driver (Built %s) %s rc=%d\n",
		 __func__, CY_DRIVER_DATE,rc);

	return rc;
#else
	int ret = -1;     

    cytts4_printk();
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
		cytts4_printk("*** ctp_used set to 0 !\n");
		cytts4_printk("*** if use ctp,please put the sys_config.fex ctp_used set to 1. \n");
		return 0;
	}
	if (!ctp_get_system_config()) {
		printk("%s:read config fail!\n",__func__);
		return ret;
	}
	//enable power
	input_set_power_enable(&(config_info.input_type), 1);
	//setctp18V();
   // msleep(20);
	ctp_wakeup(1,0);
	ret = i2c_add_driver(&cyttsp4_i2c_driver);
	return ret;
#endif
	
}
module_init(cyttsp4_i2c_init);

static void __exit cyttsp4_i2c_exit(void)
{
	i2c_del_driver(&cyttsp4_i2c_driver);
}
module_exit(cyttsp4_i2c_exit);
//#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product I2C driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
