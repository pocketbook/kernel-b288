/*
 *  mma7660.c - Linux kernel modules for 3-Axis Orientation/Motion
 *  Detection Sensor 
 *
 *  Copyright (C) 2009-2010 Freescale Semiconductor Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input-polldev.h>
#include <linux/device.h>
#include <linux/init-input.h>
#include <linux/sys_config.h>
#include <linux/of_gpio.h>

//#include <mach/system.h>
//#include <mach/hardware.h>

//#ifdef CONFIG_HAS_EARLYSUSPEND
//#include <linux/earlysuspend.h>
//#endif


#define LM3630A_NAME "LM3630A"
#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_PM)
#include <linux/pm.h>
#endif


/* Addresses to scan */
static const unsigned short normal_i2c[2] = {0x36, I2C_CLIENT_END};
static __u32 twi_id = 1;

//Function as i2c_master_send, and return 1 if operation is successful. 
static int i2c_write_bytes(struct i2c_client *client, uint8_t *data, uint16_t len)
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
static bool LM3630A_i2c_test(struct i2c_client * client)
{
	int ret, retry;
	uint8_t test_data[1] = { 0x1F };	//only write a data address.
	
	for(retry=0; retry < 2; retry++)
	{
		ret =i2c_write_bytes(client, test_data, 1);	//Test i2c.
		if (ret == 1)
			break;
		msleep(5);
	}
	
	return ret==1 ? true : false;
}

/**
 * gsensor_detect - Device detection callback for automatic device creation
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
static int LM3630A_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int ret;
	
	printk( "%s enter \n", __func__);
	
	ret = i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA);
	if (!ret)
        	return -ENODEV;
    
	if(twi_id == adapter->nr){
            pr_info("%s: addr= %x\n",__func__,client->addr);

            ret = LM3630A_i2c_test(client);
        	if(!ret){
        		pr_info("%s:I2C connection might be something wrong or maybe the other gsensor equipment! \n",__func__);
        		return -ENODEV;
        	}else{           	    
            	pr_info(" LM3630A_i2c_test I2C connection sucess!\n");
            	strlcpy(info->type, LM3630A_NAME, I2C_NAME_SIZE);
    		    return 0;	
	             }

	}else{
		return -ENODEV;
	}
}



/*
 * Initialization function
 */
static int LM3630A_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result;
	printk("======%s=========. \n", __func__);
	result = i2c_smbus_read_byte_data(client, 0x1F);
	printk("hxm  LM3630A version is=%X",result);

	result = i2c_smbus_read_byte_data(client, 0x00);
	printk("hxm  i2c_smbus_read_byte_data 0X00 is=%X\n",result);
	
	result = i2c_smbus_read_byte_data(client, 0x01);
	printk("hxm  i2c_smbus_read_byte_data 0X01 is=%X\n",result);

	result = i2c_smbus_read_byte_data(client, 0x02);
	printk("hxm  i2c_smbus_read_byte_data 0X02 is=%X\n",result);

	result = i2c_smbus_read_byte_data(client, 0x03);
	printk("hxm  i2c_smbus_read_byte_data 0X03is=%X\n",result);

	result = i2c_smbus_read_byte_data(client, 0x04);
	printk("hxm  i2c_smbus_read_byte_data 0X04is=%X\n",result);	
	
        result = i2c_smbus_write_byte_data(client, 0x00,0x06);
	printk("hxm  i2c_smbus_write_byte_data 0x00 is=%X\n",result);	
        result = i2c_smbus_write_byte_data(client, 0x01,0x19);
	printk("hxm  i2c_smbus_write_byte_data 0x01 is=%X\n",result);
        result = i2c_smbus_write_byte_data(client, 0x03,0xFF);
	printk("hxm  i2c_smbus_write_byte_data 0x03 is=%X\n",result);
	 result =i2c_smbus_write_byte_data(client, 0x04,0xFF);
	printk("hxm  i2c_smbus_write_byte_data 0x04 is=%X\n",result);


	result = i2c_smbus_read_byte_data(client, 0x00);
	printk("hxm  i2c_smbus_read_byte_data 0X00 is=%X\n",result);
	
	result = i2c_smbus_read_byte_data(client, 0x01);
	printk("hxm  i2c_smbus_read_byte_data 0X01 is=%X\n",result);

	result = i2c_smbus_read_byte_data(client, 0x02);
	printk("hxm  i2c_smbus_read_byte_data 0X02 is=%X\n",result);

	result = i2c_smbus_read_byte_data(client, 0x03);
	printk("hxm  i2c_smbus_read_byte_data 0X03is=%X",result);

	result = i2c_smbus_read_byte_data(client, 0x04);
	printk("hxm  i2c_smbus_read_byte_data 0X04is=%X\n",result);	

	return 0;
}


 /* CONFIG_HAS_EARLYSUSPEND */

static const struct i2c_device_id LM3630A_id[] = {
	{ LM3630A_NAME, 1 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, LM3630A_id);

static struct i2c_driver  LM3630A_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name	= LM3630A_NAME,
		.owner	= THIS_MODULE,
		//.of_match_table = "allwinner,sun50i-gsensor-para",
	},
	.probe	= LM3630A_probe,
	.id_table = LM3630A_id,
	.detect = LM3630A_detect,
	.address_list	= normal_i2c,
};

u32 pg10_gpio;
u32 pg09_gpio;
int pg10_gpio_sysconfig_para(void)
{
       struct device_node *np = NULL;	
	int ret = -1;

	np = of_find_node_by_name(NULL,"LM3630A_para");
	if (!np) {
		pr_err("ERROR! get SPEAKER_PARA failed, func:%s, line:%d\n",__FUNCTION__, __LINE__);
		goto devicetree_get_item_err;
	}
	pg10_gpio = of_get_named_gpio_flags(np, "lcd_bl_en", 0, (enum of_gpio_flags *)(&pg10_gpio));
	if (!gpio_is_valid(pg10_gpio)){
		pr_err("%s: lcd_bl_en is invalid. \n",__func__ );
		goto devicetree_get_item_err;
	}
	printk("lcd_bl_en=%d \n",pg10_gpio);


       if (pg10_gpio!= -1) {
		gpio_free(pg10_gpio);
		ret = gpio_request(pg10_gpio, "lcd_bl_en");
		if (ret) {
			printk("Failed to request speaker_gpio :%d, ERRNO:%d", (int)pg10_gpio, ret);
			ret = -ENODEV;
			goto devicetree_get_item_err;
		}
		///default is high
		ret = gpio_direction_output(pg10_gpio,1);	
		if (ret) {
			ret = -ENODEV;
			goto devicetree_get_item_err;
		}
	}

	pg09_gpio = of_get_named_gpio_flags(np, "work_gpio", 0, (enum of_gpio_flags *)(&pg09_gpio));
		if (!gpio_is_valid(pg09_gpio)){
			pr_err("%s: work_gpio is invalid. \n",__func__ );
			goto devicetree_get_item_err;
		}
		printk("work_gpio =%d \n",pg09_gpio);


	       if (pg09_gpio!= -1) {
			gpio_free(pg09_gpio);
			ret = gpio_request(pg09_gpio, "work_gpio");
			if (ret) {
				printk("Failed to request speaker_gpio :%d, ERRNO:%d", (int)pg09_gpio, ret);
				ret = -ENODEV;
				goto devicetree_get_item_err;
			}
			///default is high
			ret = gpio_direction_output(pg09_gpio,1);	
			if (ret) {
				ret = -ENODEV;
				goto devicetree_get_item_err;
			}
		}
	   
	return 0;
devicetree_get_item_err:
	pr_notice("=========script_get_item_err=====%s, %s line=%d=======\n",__FILE__,__FUNCTION__,__LINE__);
  
	return ret;

}


static int __init LM3630A_init(void)
{
	int ret = -1;
	printk("======%s=========. \n", __func__);
	pg10_gpio_sysconfig_para();
	ret = i2c_add_driver(&LM3630A_driver);
	if (ret < 0) {
		printk(KERN_INFO "add LM3630A_driver i2c driver failed\n");
		return -ENODEV;
	}
	printk("add LM3630A_driver i2c driver\n");
	


	return ret;
}

static void __exit LM3630A_exit(void)
{
	printk(KERN_INFO "remove LM3630A i2c driver.\n");
	i2c_del_driver(&LM3630A_driver);
}

MODULE_AUTHOR("Chen Gang <gang.chen@freescale.com>");
MODULE_DESCRIPTION("LM3630A 3-Axis Orientation/Motion  driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1");

module_init(LM3630A_init);
module_exit(LM3630A_exit);

