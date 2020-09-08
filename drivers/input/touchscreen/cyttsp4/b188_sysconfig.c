

#include <linux/input.h>

#include <linux/init-input.h>

#include <linux/gpio.h>
#include <linux/sys_config.h>
#include <linux/hrtimer.h>

#include <linux/jiffies.h>

#include <linux/delay.h>
#include "cyttsp4_regs.h"

#include "cyttsp4_platform.h"

#include <linux/regulator/consumer.h>


#define CTP_NAME		CYTTSP4_I2C_NAME//	"cyttsp4_i2c_adapter"


__u32 twi_id = 0;

const char* fwname;

int screen_max_x = 0;
int screen_max_y = 0;
int revert_x_flag = 0;
int revert_y_flag = 0;
int exchange_x_y_flag = 0;
//int	int_cfg_addr[]={PIO_INT_CFG0_OFFSET,PIO_INT_CFG1_OFFSET,
//			PIO_INT_CFG2_OFFSET, PIO_INT_CFG3_OFFSET};

struct ctp_config_info config_info = {
	.input_type = CTP_TYPE,
	.name = NULL,
	.int_number = 0,
};
#if 1 //shy

////////////////////shy add borad to this
/**
 * ctp_detect - Device detection callback for automatic device creation
 * return value:  
 *                   = 0; success;
 *                   < 0; err
 */
 //hxm add
#define CYTTSP4_USE_I2C

#ifdef CYTTSP4_USE_I2C
#define CYTTSP4_I2C_TCH_ADR 0x24
#define CYTTSP4_LDR_TCH_ADR 0x24
#define CYTTSP4_I2C_IRQ_GPIO 38 /* J6.9, C19, GPMC_AD14/GPIO_38 */
#define CYTTSP4_I2C_RST_GPIO 37 /* J6.10, D18, GPMC_AD13/GPIO_37 */
#endif

#ifndef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT
#define CY_VKEYS_X 720
#define CY_VKEYS_Y 1280
#define CY_MAXX 880
#define CY_MAXY 1280
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
static u16 cyttsp4_btn_keys[] = {
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

static struct touch_settings cyttsp4_sett_btn_keys = {
	.data = (uint8_t *)&cyttsp4_btn_keys[0],
	.size = ARRAY_SIZE(cyttsp4_btn_keys),
	.tag = 0,
};

static struct cyttsp4_core_platform_data _cyttsp4_core_platform_data = {
	.irq_gpio = CYTTSP4_I2C_IRQ_GPIO,
	.rst_gpio = CYTTSP4_I2C_RST_GPIO,
	.xres = cyttsp4_xres,
	.init = cyttsp4_init,
	.power = cyttsp4_power,
	.detect = cyttsp4_detect,
	.irq_stat = cyttsp4_irq_stat,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Cypress Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL,	/* &cyttsp4_sett_param_regs, */
		NULL,	/* &cyttsp4_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp4_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp4_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		&cyttsp4_sett_btn_keys,	/* button-to-keycode table */
	},
	.flags = CY_CORE_FLAG_WAKE_ON_GESTURE,
	.easy_wakeup_gesture = CY_CORE_EWG_TAP_TAP
		| CY_CORE_EWG_TWO_FINGER_SLIDE,
};

static const int16_t cyttsp4_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -127, 127, 0, 0,
	ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0,
	ABS_DISTANCE, 0, 255, 0, 0,	/* Used with hover */
};

struct touch_framework cyttsp4_framework = {
	.abs = cyttsp4_abs,
	.size = ARRAY_SIZE(cyttsp4_abs),
	.enable_vkeys = 0,
};

static struct cyttsp4_mt_platform_data _cyttsp4_mt_platform_data = {
	.frmwrk = &cyttsp4_framework,
	.flags = CY_MT_FLAG_FLIP | CY_MT_FLAG_INV_X | CY_MT_FLAG_INV_Y,
	.inp_dev_name = CYTTSP4_MT_NAME,
	.vkeys_x = CY_VKEYS_X,
	.vkeys_y = CY_VKEYS_Y,
};

static struct cyttsp4_btn_platform_data _cyttsp4_btn_platform_data = {
	.inp_dev_name = CYTTSP4_BTN_NAME,
};

static const int16_t cyttsp4_prox_abs[] = {
	ABS_DISTANCE, CY_PROXIMITY_MIN_VAL, CY_PROXIMITY_MAX_VAL, 0, 0,
};

struct touch_framework cyttsp4_prox_framework = {
	.abs = cyttsp4_prox_abs,
	.size = ARRAY_SIZE(cyttsp4_prox_abs),
};

static struct cyttsp4_proximity_platform_data
		_cyttsp4_proximity_platform_data = {
	.frmwrk = &cyttsp4_prox_framework,
	.inp_dev_name = CYTTSP4_PROXIMITY_NAME,
};

static struct cyttsp4_platform_data _cyttsp4_platform_data = {
	.core_pdata = &_cyttsp4_core_platform_data,
	.mt_pdata = &_cyttsp4_mt_platform_data,
	.btn_pdata = &_cyttsp4_btn_platform_data,
	.prox_pdata = &_cyttsp4_proximity_platform_data,
	.loader_pdata = &_cyttsp4_loader_platform_data,
};

static ssize_t cyttsp4_virtualkeys_show(struct kobject *kobj,
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

static struct kobj_attribute cyttsp4_virtualkeys_attr = {
	.attr = {
		.name = "virtualkeys.cyttsp4_mt",
		.mode = S_IRUGO,
	},
	.show = &cyttsp4_virtualkeys_show,
};

static struct attribute *cyttsp4_properties_attrs[] = {
	&cyttsp4_virtualkeys_attr.attr,
	NULL
};

static struct attribute_group cyttsp4_properties_attr_group = {
	.attrs = cyttsp4_properties_attrs,
};
#endif /* !CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_DEVICETREE_SUPPORT */

#endif



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
	cytts4_printk();
	for(retry=0; retry < 12; retry++)
	{
		ret =ctp_i2c_write_bytes(client, test_data, 1);	//Test i2c.
		//if (ret == 1)
		//	break;
		msleep(10);
		cytts4_printk(" retry =%d ret =%d \n",retry,ret);

	}
	//ret =1;
	return ret==1 ? true : false;
}

int ctp_detect(struct i2c_client *client, struct i2c_board_info *info)
#if 0
{
	cytts4_printk();
	struct i2c_adapter *adapter = client->adapter;
	if(twi_id == adapter->nr)
	{
		printk("%s: Detected chip %s at adapter %d, address 0x%02x\n",
			 __func__, CTP_NAME, i2c_adapter_id(adapter), client->addr);

		strlcpy(info->type, CTP_NAME, I2C_NAME_SIZE);
		return 0;
	}else{
		return -ENODEV;
	}
	
}////////////////////////////////////////////////////////////////
#else
{
	int ret;
	struct i2c_adapter *adapter = client->adapter;
	cytts4_printk("\n");
	if(twi_id == adapter->nr)
	{
		cytts4_printk("Detected chip %s at adapter %d, address 0x%02x\n",
			  CTP_NAME, i2c_adapter_id(adapter), client->addr);
        ret = ctp_i2c_test(client);
		cytts4_printk(" ctp_i2c_test =%d\n ",ret);
        if(!ret){
        	pr_info("%s:I2C connection might be something wrong --------------------\n",__func__);
        	return -ENODEV;
        }else
        {      
            cytts4_printk("I2C connection sucess==============================!\n");
			strlcpy(info->type, CTP_NAME, I2C_NAME_SIZE);
	        //hxm add
	        info->platform_data = &_cyttsp4_platform_data;
		}
		
		return 0;
	}else{
		return -ENODEV;
	}
}
#endif



/**
 * ctp_print_info - sysconfig print function
 * return value:
 *
 */
void ctp_print_info(struct ctp_config_info info,int debug_level)
{
	{
		cytts4_printk();

		printk("info.ctp_used:%d\n",info.ctp_used);
		printk("info.twi_id:%d\n",info.twi_id);
		printk("info.screen_max_x:%d\n",info.screen_max_x);
		printk("info.screen_max_y:%d\n",info.screen_max_y);
		printk("info.revert_x_flag:%d\n",info.revert_x_flag);
		printk("info.revert_y_flag:%d\n",info.revert_y_flag);
		printk("info.exchange_x_y_flag:%d\n",info.exchange_x_y_flag);
		printk("info.irq_gpio_number:%d\n",info.irq_gpio.gpio);
		printk("info.wakeup_gpio_number:%d\n",info.wakeup_gpio.gpio);
	}
}


int ctp_get_system_config(void)
{
	cytts4_printk();

    ctp_print_info(config_info,1);

	fwname = config_info.name;
	cytts4_printk("%s:fwname:%s\n",__func__,fwname);

#if 0	
	fw_index = gsl_find_fw_idx(fwname);
	if (fw_index == -1) {
		printk("gslx680: no matched TP firmware(%s)!\n", fwname);
		return 0;
	}
	dprintk(DEBUG_INIT,"fw_index = %d\n",fw_index);
#endif
	twi_id = config_info.twi_id;
    screen_max_x = config_info.screen_max_x;
    screen_max_y = config_info.screen_max_y;
    revert_x_flag = config_info.revert_x_flag;
    revert_y_flag = config_info.revert_y_flag;
    exchange_x_y_flag = config_info.exchange_x_y_flag;


	printk("twi_id:%d\n",twi_id);
	printk("screen_max_x:%d\n",screen_max_x);
	printk("screen_max_y:%d\n",screen_max_y);
	printk("revert_x_flag:%d\n",revert_x_flag);
	printk("revert_y_flag:%d\n",revert_y_flag);
	printk("exchange_x_y_flag:%d\n",exchange_x_y_flag);

	
	
	if((screen_max_x == 0) || (screen_max_y == 0)){
           printk("%s:read config error!\n",__func__);
           return 0;
    }
    return 1;
}



/**
 * ctp_wakeup - function
 *
 */
int ctp_wakeup(int status,int ms)
{

	int ret;
	//ret = gpio_direction_output(config_info.wakeup_gpio.gpio, 0);
	//ret = gpio_direction_input(config_info.wakeup_gpio.gpio);
#if 0
	cytts4_printk("wakeup_get_value = %d ret=%d\n",__gpio_get_value(config_info.wakeup_gpio.gpio),ret);
	
	__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
			cytts4_printk("wakeup_get_value = %d \n",__gpio_get_value(config_info.wakeup_gpio.gpio));
	msleep(50);

	__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
	msleep(500);
	cytts4_printk("wakeup_get_value = %d \n",__gpio_get_value(config_info.wakeup_gpio.gpio));

	__gpio_set_value(config_info.wakeup_gpio.gpio, 1);

	msleep(250);
	cytts4_printk("wakeup_get_value = %d \n",__gpio_get_value(config_info.wakeup_gpio.gpio));

#else	

	if (status == 0) {

		if(ms == 0) {
			__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
		}else {
			__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
			cytts4_printk("wakeup_get_value = %d \n",__gpio_get_value(config_info.wakeup_gpio.gpio));
	
			msleep(ms);
			__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
			cytts4_printk("wakeup_get_value = %d \n",__gpio_get_value(config_info.wakeup_gpio.gpio));
		}
	}
	if (status == 1) {
		if(ms == 0) {
			__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
			cytts4_printk("wakeup_get_value = %d \n",__gpio_get_value(config_info.wakeup_gpio.gpio));
		}else {
			__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
			cytts4_printk("wakeup_get_value = %d \n",__gpio_get_value(config_info.wakeup_gpio.gpio));
			msleep(ms);
			__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
			cytts4_printk("wakeup_get_value = %d \n",__gpio_get_value(config_info.wakeup_gpio.gpio));
		}
	}
	msleep(5);

#endif
	return 0;
}
//extern void axp22_get_size(s32 reg);
//extern void axp22_write_size(s32 reg, u8 *val);
int setctp18V(void)
{
	int ret;
	struct regulator *ctp_power_ldo_V18;
		struct regulator *pa_shdn;

	u32 ctp_power_vol_V18;

	
	cytts4_printk();

#if 0
	axp22_get_size(0x93);
    axp22_get_size(0x92);
	msleep(3000);
	/*ctp_power_ldo_V18= regulator_get(NULL, "axp227_eldo1");
	ret = regulator_get_voltage(ctp_power_ldo_V18);
	cytts4_printk(" get ctp_power_vol_V18=%d\n",ret);
	
	if (! ctp_power_ldo_V18)
	{
		pr_err("%s: could not get ctp ldo '%s' , check -----------\n",__func__);
	}
	else
	{
		ctp_power_vol_V18 =2700;
 		cytts4_printk(" ctp_power_vol_V18=%d\n",ctp_power_vol_V18);
		ret = regulator_set_voltage(ctp_power_ldo_V18,
				(int)(ctp_power_vol_V18)*1000,
				(int)(ctp_power_vol_V18)*1000);
		cytts4_printk(" set ctp_power_ldo_V18 ret=%d\n",ret);
		ret = regulator_get_voltage(ctp_power_ldo_V18);
		cytts4_printk(" ctp_power_vol_V18=%d\n",ret);
		
	}
	*/
    axp22_get_size(0x93);
    axp22_get_size(0x92);
	msleep(3000);
    axp22_get_size(0x93);
    axp22_get_size(0x92);
	msleep(3000);


	axp22_write_size(0x92,0x3);
	axp22_write_size(0x93,0xB);
	msleep(5000);

#endif
	//axp22_write_size(0x93,0x1F); //3.3v
	
	//axp22_write_size(0x90,0x3);/gpio1_ldo  1.8v
 //   axp22_get_size(0x90);

	

	
	//axp22_write_size(0x91,0xB);//gpio0_lod 1.8v
//	axp22_write_size(0x91,0x1F);//gpio0_ldo 3.3v
//	axp22_get_size(0x91);
//// xiamian shi gpio0  shangmian shi gpio1
}


