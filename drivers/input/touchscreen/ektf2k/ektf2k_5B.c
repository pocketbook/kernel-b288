/* drivers/input/touchscreen/ektf2k.c - ELAN EKTF2K verions of driver
 *
 * Copyright (C) 2011 Elan Microelectronics Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 2011/12/06: The first release, version 0x0001
 * 2012/2/15:  The second release, version 0x0002 for new bootcode
 * 2012/5/8:   Release version 0x0003 for china market
 *             Integrated 2 and 5 fingers driver code together and
 *             auto-mapping resolution.
 * 2012/12/1:	 Release version 0x0005: support up to 10 fingers but no buffer mode.
 *             Please change following parameters
 *                 1. For 5 fingers protocol, please enable ELAN_PROTOCOL.
                      The packet size is 18 or 24 bytes.
 *                 2. For 10 fingers, please enable both ELAN_PROTOCOL and ELAN_TEN_FINGERS.
                      The packet size is 40 or 4+40+40+40 (Buffer mode) bytes.
 *                 3. Please enable the ELAN_BUTTON configuraton to support button.
 *								 
 */



#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
//#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>

//#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
//#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>

// for linux 2.6.36.3
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/ioctl.h>

#include <linux/init-input.h>

//#include <mach/irqs.h>
//#include <mach/system.h>
//#include <mach/hardware.h>
#include <linux/sys_config.h>
//#include "../ctp_platform_ops.h"
#include "ektf2k.h"

#include <linux/gpio.h>

#include <linux/power/scenelock.h>

#define DBG(s...)
//#define DBG(s...) printk(s)
#define INF(s...) printk(s)
#define ERR(s...) printk(s)

/* The ELAN_PROTOCOL support normanl packet format */	
#define ELAN_PROTOCOL		

//#define ELAN_TEN_FINGERS   /* james check: Can not be use to auto-resolution mapping */

/* Ҫȡ�������ELAN_BUTTON*/
//#define ELAN_BUFFER_MODE
//#define ELAN_BUTTON


/* ֧�ֶ��ٸ���ָ����
 *  ���ݰ���С�Ĳ�����
 *  ����Ҫ���ն����ֽ�
 */
#ifdef ELAN_TEN_FINGERS
#define PACKET_SIZE		44		/* support 10 fingers packet */
#else
#define PACKET_SIZE		8 		/* support 2 fingers packet  */
//#define PACKET_SIZE		24			/* support 5 fingers packet  */
#endif


#define PRESS_DOWN  1
#define FREE_UP  0

#define PWR_STATE_DEEP_SLEEP	0
#define PWR_STATE_NORMAL		1
#define PWR_STATE_MASK			BIT(3)

#define CMD_S_PKT		0x52
#define CMD_R_PKT		0x53
#define CMD_W_PKT		0x54

#define HELLO_PKT		0x55

#define TWO_FINGERS_PKT		0x5A
#define FIVE_FINGERS_PKT	       0x5D
#define MTK_FINGERS_PKT		0x6D
#define TEN_FINGERS_PKT		0x62
#define BUFFER_PKT		0x63

#define RESET_PKT		0x77
#define CALIB_PKT		0xA8

// modify
#define SYSTEM_RESET_PIN_SR 	10

//Add these Define
#define IAP_IN_DRIVER_MODE 	1
#define IAP_PORTION            	0
#define PAGERETRY  30
#define IAPRESTART 5

#define ELAN_PAGE_SIZE            132
#define MAX_FIRMWARE_SIZE    (ELAN_PAGE_SIZE * 249)

// For Firmware Update 
#define ELAN_IOCTLID	0xD0
#define IOCTL_I2C_SLAVE	_IOW(ELAN_IOCTLID,  1, int)
#define IOCTL_MAJOR_FW_VER  _IOR(ELAN_IOCTLID, 2, int)
#define IOCTL_MINOR_FW_VER  _IOR(ELAN_IOCTLID, 3, int)
#define IOCTL_RESET  _IOR(ELAN_IOCTLID, 4, int)
#define IOCTL_IAP_MODE_LOCK  _IOR(ELAN_IOCTLID, 5, int)
#define IOCTL_CHECK_RECOVERY_MODE  _IOR(ELAN_IOCTLID, 6, int)
#define IOCTL_FW_VER  _IOR(ELAN_IOCTLID, 7, int)
#define IOCTL_X_RESOLUTION  _IOR(ELAN_IOCTLID, 8, int)
#define IOCTL_Y_RESOLUTION  _IOR(ELAN_IOCTLID, 9, int)
#define IOCTL_FW_ID  _IOR(ELAN_IOCTLID, 10, int)
#define IOCTL_ROUGH_CALIBRATE  _IOR(ELAN_IOCTLID, 11, int)
#define IOCTL_IAP_MODE_UNLOCK  _IOR(ELAN_IOCTLID, 12, int)
#define IOCTL_I2C_INT  _IOR(ELAN_IOCTLID, 13, int)
#define IOCTL_RESUME  _IOR(ELAN_IOCTLID, 14, int)
#define IOCTL_POWER_LOCK  _IOR(ELAN_IOCTLID, 15, int)
#define IOCTL_POWER_UNLOCK  _IOR(ELAN_IOCTLID, 16, int)
#define IOCTL_FW_UPDATE  _IOR(ELAN_IOCTLID, 17, int)
#define IOCTL_BC_VER  _IOR(ELAN_IOCTLID, 18, int)
#define IOCTL_2WIREICE  _IOR(ELAN_IOCTLID, 19, int)

#define CUSTOMER_IOCTLID	0xA0
#define IOCTL_CIRCUIT_CHECK  _IOR(CUSTOMER_IOCTLID, 1, int)
#define IOCTL_GET_UPDATE_PROGREE	_IOR(CUSTOMER_IOCTLID,  2, int)

uint8_t RECOVERY=0x00;
int FW_VERSION=0x00;
int FW_ID=0x00;
int X_NATIVE=2012; // 7" 1280 tp sensor resolution
int Y_NATIVE=4752;  // 7" 2112 tp sensor resolution
int X_RESOLUTION=758; // 7" 1280 screen resolution
int Y_RESOLUTION=1024;  // 7" 2112 screen resolution
int X_REVERT=0;
int Y_REVERT=0;
int XY_EXCHANGE=0;
int work_lock=0x00;
int power_lock=0x00;
int circuit_ver=0x01;
/*++++i2c transfer start+++++++*/
int file_fops_addr=0x15;
/*++++i2c transfer end+++++++*/

static unsigned char *firmware_file = NULL;
static int firmware_size = 0;

int button_state = 0;

uint8_t ic_status=0x00;	//0:OK 1:master fail 2:slave fail
int update_progree=0;
uint8_t I2C_DATA[3] = {0x15, 0x20, 0x21};/*I2C devices address*/  
int is_OldBootCode = 0; // 0:new 1:old

enum
{
	PageSize		= 132,
	PageNum		        = 249,
	ACK_Fail		= 0x00,
	ACK_OK			= 0xAA,
	ACK_REWRITE		= 0x55,
};

enum
{
	E_FD			= -1,
};

static const char* fwname;

struct elan_ktf2k_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *elan_wq;
	struct work_struct work;
//	struct early_suspend early_suspend;
	int intr_gpio;
// Firmware Information
	int fw_ver;
	int fw_id;
	int bc_ver;
	int x_resolution;
	int y_resolution;
// For Firmare Update 
	struct miscdevice firmware;
};

#define TRAIL_SIZE 32

static struct trail_record {
        unsigned char mask;
        int x1;
        int x2;
        int y1;
        int y2;
} trail_data[TRAIL_SIZE];
static int trail_head=0, trail_tail=0;

static DECLARE_WAIT_QUEUE_HEAD(trail_wq);

static struct elan_ktf2k_ts_data *private_ts;
static int __fw_packet_handler(struct i2c_client *client);
static int elan_ktf2k_ts_rough_calibrate(struct i2c_client *client);
static int elan_ktf2k_ts_resume(struct i2c_client *client);

int Update_FW_One(/*struct file *filp,*/ struct i2c_client *client, int recovery);
static int __hello_packet_handler(struct i2c_client *client);

///////////////////////////////////////////////
//specific tp related macro: need be configured for specific tp
//add by ffz 
//#define CTP_IRQ_MODE			(IRQF_TRIGGER_FALLING)
#define CTP_IRQ_MODE			(IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND)
//end
#define CTP_NAME			ELAN_KTF2K_NAME
//#define CTP_IRQ_NO			(gpio_int_info[0].port_num)

#define SCREEN_MAX_X			(screen_max_x)
#define SCREEN_MAX_Y			(screen_max_y)
 /* X�ֱ��� */
#define ELAN_TS_X_MAX      (screen_max_x)
 /* Y�ֱ��� */
#define ELAN_TS_Y_MAX      (screen_max_y)

#define MAX_CONTACTS 		10
#define PRESS_MAX    			255

void* __iomem gpio_addr = NULL;
//int gpio_int_hdle = 0;
//int gpio_wakeup_hdle = 0;
//int gpio_reset_hdle = 0;
//int gpio_wakeup_enable = 1;
//int gpio_reset_enable = 1;
//static user_gpio_set_t  gpio_int_info[1];
//int gpio_Switch_hdle = 0;
//int gpio_Switch_hdle_pc25 = 0;

#define ELAN_KTF2K_I2C_ADDR 0x15

int screen_max_x = 0;
int screen_max_y = 0;
int revert_x_flag = 0;
int revert_y_flag = 0;
int exchange_x_y_flag = 0;
//int	int_cfg_addr[]={PIO_INT_CFG0_OFFSET,PIO_INT_CFG1_OFFSET,
//			PIO_INT_CFG2_OFFSET, PIO_INT_CFG3_OFFSET};

/* Addresses to scan */
 union{
	unsigned short dirty_addr_buf[2];
	const unsigned short normal_i2c[2];
}u_i2c_addr = {{ELAN_KTF2K_I2C_ADDR,I2C_CLIENT_END},};				

__u32 twi_id = 0;

struct ctp_config_info config_info = {
	.input_type = CTP_TYPE,
	.name = NULL,
	.int_number = 0,
};

//add by ffz
bool being_work = true;
static int int_cnt;
//end

/*
 * ctp_get_pendown_state  : get the int_line data state, 
 * 
 * return value:
 *             return PRESS_DOWN: if down
 *             return FREE_UP: if up,
 *             return 0: do not need process, equal free up.
 */
 int ctp_get_pendown_state(void)
{
	unsigned int reg_val;
	static int state = FREE_UP;
	reg_val = gpio_get_value(config_info.irq_gpio.gpio);

	if(reg_val == 1)
	{
		state = PRESS_DOWN;
	}
	else
	{
		state = FREE_UP;
	}
	return state;
}
/**
 * ctp_clear_penirq - clear int pending
 *
 */
 void ctp_clear_penirq(void)
{
	int reg_val;

	reg_val = gpio_get_value(config_info.irq_gpio.gpio);
	gpio_set_value(config_info.irq_gpio.gpio, reg_val);
	return;
}

/**
 * ctp_judge_int_occur - whether interrupt occur.
 *
 * return value: 
 *              0:      int occur;
 *              others: no int occur; 
 */
 int ctp_judge_int_occur(void)
{
	//int reg_val[3];
	int reg_val;
	int ret = -1;

	reg_val = gpio_get_value(config_info.irq_gpio.gpio);
	if(reg_val == 1)
	{
		ret = 0;
	}
	return ret; 	
}

/**
 * ctp_reset - function
 *
 */
 void ctp_reset(void)
{
}

/**
 * ctp_detect - Device detection callback for automatic device creation
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
int ctp_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if(twi_id == adapter->nr)
	{
		INF("%s: Detected chip %s at adapter %d, address 0x%02x\n",
			 __func__, CTP_NAME, i2c_adapter_id(adapter), client->addr);

		strlcpy(info->type, CTP_NAME, I2C_NAME_SIZE);
		return 0;
	}else{
		return -ENODEV;
	}
	
}////////////////////////////////////////////////////////////////

 
//#if IAP_PORTION		// add by hibernate
// For Firmware Update 
int elan_iap_open(struct inode *inode, struct file *filp)
{ 
	return 0;
}

int elan_iap_release(struct inode *inode, struct file *filp)
{    
	return 0;
}

static ssize_t elan_iap_write(struct file *filp, const char *buff, size_t count, loff_t *offp){  
    int ret;
    char *tmp;
    DBG("[ELAN]into elan_iap_write\n");

    /*++++i2c transfer start+++++++*/    	
    struct i2c_adapter *adap = private_ts->client->adapter;    	
    struct i2c_msg msg;
    /*++++i2c transfer end+++++++*/	

    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);
    
    if (tmp == NULL)
        return -ENOMEM;

    if (copy_from_user(tmp, buff, count)) {
        return -EFAULT;
    }

/*++++i2c transfer start+++++++*/
#if 1
	//down(&worklock);
	msg.addr = file_fops_addr;
	msg.flags = 0x00;// 0x00
	msg.len = count;
	msg.buf = (char *)tmp;
	//up(&worklock);
	ret = i2c_transfer(adap, &msg, 1);
#else
	
    ret = i2c_master_send(private_ts->client, tmp, count);
#endif	
/*++++i2c transfer end+++++++*/

    //if (ret != count) printk("ELAN i2c_master_send fail, ret=%d \n", ret);
    kfree(tmp);
    //return ret;
    return (ret == 1) ? count : ret;

}

ssize_t elan_iap_read(struct file *filp, char *buff, size_t count, loff_t *offp){    
    char *tmp;
    int ret;  
    long rc;
    DBG("[ELAN]into elan_iap_read\n");
   /*++++i2c transfer start+++++++*/
    	struct i2c_adapter *adap = private_ts->client->adapter;
    	struct i2c_msg msg;
/*++++i2c transfer end+++++++*/
    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);

    if (tmp == NULL)
        return -ENOMEM;
/*++++i2c transfer start+++++++*/
#if 1
	//down(&worklock);
	msg.addr = file_fops_addr;
	//msg.flags |= I2C_M_RD;
	msg.flags = 0x00;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = tmp;
	//up(&worklock);
	ret = i2c_transfer(adap, &msg, 1);
#else
    ret = i2c_master_recv(private_ts->client, tmp, count);
#endif
/*++++i2c transfer end+++++++*/
    if (ret >= 0)
        rc = copy_to_user(buff, tmp, count);
    
    kfree(tmp);

    //return ret;
    return (ret == 1) ? count : ret;
	
}

static long elan_iap_ioctl(/*struct inode *inode,*/ struct file *filp,    unsigned int cmd, unsigned long arg){

	int __user *ip = (int __user *)arg;
	DBG("[ELAN]into elan_iap_ioctl\n");
	DBG("cmd value %x\n",cmd);
	
	switch (cmd) {        
		case IOCTL_I2C_SLAVE: 
			//private_ts->client->addr = (int __user)arg;
			file_fops_addr = 0x15;
			break;   
		case IOCTL_MAJOR_FW_VER:            
			break;        
		case IOCTL_MINOR_FW_VER:            
			break;        
		case IOCTL_RESET:
// modify
//			gpio_write_one_pin_value(gpio_reset_hdle , 0, "ctp_reset");
			__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
		        //gpio_set_value(SYSTEM_RESET_PIN_SR, 0);
		        msleep(20);
		        //gpio_set_value(SYSTEM_RESET_PIN_SR, 1);
//			gpio_write_one_pin_value(gpio_reset_hdle , 1,  "ctp_reset");
			__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
		        msleep(100);

			break;
		case IOCTL_IAP_MODE_LOCK:
			if(work_lock==0)
			{
				work_lock=1;
//				private_ts->client->irq = SW_INT_IRQNO_PIO;
//				disable_irq(private_ts->client->irq);
				input_set_int_enable(&(config_info.input_type), 0);
				cancel_work_sync(&private_ts->work);
			}
			break;
		case IOCTL_IAP_MODE_UNLOCK:
			if(work_lock==1)
			{			
				work_lock=0;
//				private_ts->client->irq = SW_INT_IRQNO_PIO;
//				enable_irq(private_ts->client->irq);
				input_set_int_enable(&(config_info.input_type), 1);

			}
			break;
		case IOCTL_CHECK_RECOVERY_MODE:
			return RECOVERY;
			break;
		case IOCTL_FW_VER:
			__fw_packet_handler(private_ts->client);
			return FW_VERSION;
			break;
		case IOCTL_X_RESOLUTION:
			__fw_packet_handler(private_ts->client);
			return X_RESOLUTION;
			break;
		case IOCTL_Y_RESOLUTION:
			__fw_packet_handler(private_ts->client);
			return Y_RESOLUTION;
			break;
		case IOCTL_FW_ID:
			__fw_packet_handler(private_ts->client);
			return FW_ID;
			break;
		case IOCTL_ROUGH_CALIBRATE:
			return elan_ktf2k_ts_rough_calibrate(private_ts->client);
		case IOCTL_I2C_INT:
			//put_user(gpio_get_value(private_ts->intr_gpio), ip);
//			put_user( gpio_read_one_pin_value(gpio_reset_hdle, "ctp_reset"), ip);
			put_user( gpio_get_value(config_info.wakeup_gpio.gpio), ip);
			break;	
		case IOCTL_RESUME:
			elan_ktf2k_ts_resume(private_ts->client);
			break;	
		case IOCTL_POWER_LOCK:
			power_lock=1;
			break;
		case IOCTL_POWER_UNLOCK:
			power_lock=0;
			break;
#if IAP_PORTION		
		case IOCTL_GET_UPDATE_PROGREE:
			update_progree=(int __user)arg;
			break; 
		case IOCTL_FW_UPDATE:
			Update_FW_One(private_ts->client, 0);
			break;
		case IOCTL_2WIREICE:
//			TWO_WIRE_ICE(private_ts->client);
			break;		
#endif
		case IOCTL_CIRCUIT_CHECK:
			return circuit_ver;
			break;
		default:      
			ERR("[elan] Un-known IOCTL Command %d\n", cmd);      
			break;   
	}       
	return 0;
}

struct file_operations elan_touch_fops = {    
        .open =         elan_iap_open,    
        .write =        elan_iap_write,    
        .read = 	elan_iap_read,    
        .release =	elan_iap_release,    
	.unlocked_ioctl=elan_iap_ioctl, 
 };

int EnterISPMode(struct i2c_client *client, uint8_t  *isp_cmd)
{
	char buff[4] = {0};
	int len = 0;
	
	len = i2c_master_send(private_ts->client, isp_cmd,  sizeof(isp_cmd));
	if (len != sizeof(buff)) {
		ERR("[ELAN] ERROR: EnterISPMode fail! len=%d\r\n", len);
		return -1;
	}
	else
		DBG("[ELAN] IAPMode write data successfully! cmd = [%2x, %2x, %2x, %2x]\n", isp_cmd[0], isp_cmd[1], isp_cmd[2], isp_cmd[3]);
	return 0;
}

int ExtractPage(struct file *filp, uint8_t * szPage, int byte)
{
	int len = 0;

	len = filp->f_op->read(filp, szPage,byte, &filp->f_pos);
	if (len != byte) 
	{
		ERR("[ELAN] ExtractPage ERROR: read page error, read error. len=%d\r\n", len);
		return -1;
	}

	return 0;
}

int WritePage(uint8_t * szPage, int byte)
{
	int len = 0;

	len = i2c_master_send(private_ts->client, szPage,  byte);
	if (len != byte) 
	{
		ERR("[ELAN] ERROR: write page error, write error. len=%d\r\n", len);
		return -1;
	}

	return 0;
}

int GetAckData(struct i2c_client *client)
{
	int len = 0;

	char buff[2] = {0};
	
	len=i2c_master_recv(private_ts->client, buff, sizeof(buff));
	if (len != sizeof(buff)) {
		ERR("[ELAN] ERROR: read data error, write 50 times error. len=%d\r\n", len);
		return -1;
	}

	pr_info("[ELAN] GetAckData:%x,%x",buff[0],buff[1]);
	if (buff[0] == 0xaa/* && buff[1] == 0xaa*/) 
		return ACK_OK;
	else if (buff[0] == 0x55 && buff[1] == 0x55)
		return ACK_REWRITE;
	else
		return ACK_Fail;

	return 0;
}

void print_progress(int page, int ic_num, int j)
{
	int i, percent,page_tatol,percent_tatol;
	char str[256];
	str[0] = '\0';
	for (i=0; i<((page)/10); i++) {
		str[i] = '#';
		str[i+1] = '\0';
	}
	
	page_tatol=page+249*(ic_num-j);
	percent = ((100*page)/(249));
	percent_tatol = ((100*page_tatol)/(249*ic_num));

	if ((page) == (249))
		percent = 100;

	if ((page_tatol) == (249*ic_num))
		percent_tatol = 100;		

	INF("\rprogress %s| %d%%", str, percent);
	
	if (page == (249))
		INF("\n");
}
/* 
* Restet and (Send normal_command ?)
* Get Hello Packet
*/
int IAPReset(struct i2c_client *client)
{
	int res;
	//reset
//	gpio_set_value(SYSTEM_RESET_PIN_SR, 0);
	__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
	msleep(20);
//	gpio_set_value(SYSTEM_RESET_PIN_SR, 1);
	__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
	msleep(100);


	DBG("[ELAN] read Hello packet data!\n"); 	  
	res= __hello_packet_handler(client);
	return res;
}


int Update_FW_One(struct i2c_client *client, int recovery)
{
	int res = 0,ic_num = 1;
	int iPage = 0, rewriteCnt = 0; //rewriteCnt for PAGE_REWRITE
	int i = 0;
	uint8_t data;
	//struct timeval tv1, tv2;
	int restartCnt = 0; // For IAP_RESTART
	
	uint8_t recovery_buffer[4] = {0};
	int byte_count;
	uint8_t *szBuff = NULL;
	int curIndex = 0;
	uint8_t isp_cmd[] = {0x54, 0x00, 0x12, 0x34}; //{0x45, 0x49, 0x41, 0x50};

	dev_dbg(&client->dev, "[ELAN] %s:  ic_num=%d\n", __func__, ic_num);
IAP_RESTART:	
	//reset
// modify    


	data=I2C_DATA[0];//Master
	dev_dbg(&client->dev, "[ELAN] %s: address data=0x%x \r\n", __func__, data);

	if(recovery != 0x80)
	{
        INF("[ELAN] Firmware upgrade normal mode !\n");
//		gpio_set_value(SYSTEM_RESET_PIN_SR,0);
		__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
		mdelay(20);
//		gpio_set_value(SYSTEM_RESET_PIN_SR,1);
		__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
		mdelay(150);
		res = EnterISPMode(private_ts->client, isp_cmd);	 //enter ISP mode
	} else
        INF("[ELAN] Firmware upgrade recovery mode !\n");
	//res = i2c_master_recv(private_ts->client, recovery_buffer, 4);   //55 aa 33 cc 
	INF("[ELAN] recovery byte data:%x,%x,%x,%x \n",recovery_buffer[0],recovery_buffer[1],recovery_buffer[2],recovery_buffer[3]);		

	// Send Dummy Byte	
	DBG("[ELAN] send one byte data:%x,%x",private_ts->client->addr,data);
	res = i2c_master_send(private_ts->client, &data,  sizeof(data));
	if(res!=sizeof(data))
	{
		DBG("[ELAN] dummy error code = %d\n",res);
	}	
	mdelay(10);


	// Start IAP
	for( iPage = 1; iPage <= PageNum; iPage++ ) 
	{
PAGE_REWRITE:
#if 1 // 8byte mode
		// 8 bytes
		//szBuff = fw_data + ((iPage-1) * PageSize); 
		for(byte_count=1;byte_count<=17;byte_count++)
		{
			if(byte_count!=17)
			{		
	//			printk("[ELAN] byte %d\n",byte_count);	
	//			printk("curIndex =%d\n",curIndex);
				szBuff = firmware_file + curIndex;
				curIndex =  curIndex + 8;

				//ioctl(fd, IOCTL_IAP_MODE_LOCK, data);
				res = WritePage(szBuff, 8);
			}
			else
			{
	//			printk("byte %d\n",byte_count);
	//			printk("curIndex =%d\n",curIndex);
				szBuff = firmware_file + curIndex;
				curIndex =  curIndex + 4;
				//ioctl(fd, IOCTL_IAP_MODE_LOCK, data);
				res = WritePage(szBuff, 4); 
			}
		} // end of for(byte_count=1;byte_count<=17;byte_count++)
#endif 
#if 0 // 132byte mode		
		szBuff = file_fw_data + curIndex;
		curIndex =  curIndex + PageSize;
		res = WritePage(szBuff, PageSize);
#endif
//#if 0
		if(iPage==249 || iPage==1)
		{
			mdelay(600); 			 
		}
		else
		{
			mdelay(50); 			 
		}	
		res = GetAckData(private_ts->client);

		if (ACK_OK != res) 
		{
			mdelay(50); 
			ERR("[ELAN] ERROR: GetAckData fail! res=%d\r\n", res);
			if ( res == ACK_REWRITE ) 
			{
				rewriteCnt = rewriteCnt + 1;
				if (rewriteCnt == PAGERETRY)
				{
					INF("[ELAN] ID 0x%02x %dth page ReWrite %d times fails!\n", data, iPage, PAGERETRY);
					return E_FD;
				}
				else
				{
					INF("[ELAN] ---%d--- page ReWrite %d times!\n",  iPage, rewriteCnt);
					goto PAGE_REWRITE;
				}
			}
			else
			{
				restartCnt = restartCnt + 1;
				if (restartCnt >= 5)
				{
					ERR("[ELAN] ID 0x%02x ReStart %d times fails!\n", data, IAPRESTART);
					return E_FD;
				}
				else
				{
					INF("[ELAN] ===%d=== page ReStart %d times!\n",  iPage, restartCnt);
					goto IAP_RESTART;
				}
			}
		}
		else
		{       DBG("  data : 0x%02x ",  data);  
			rewriteCnt=0;
			print_progress(iPage,ic_num,i);
		}

		mdelay(10);
	} // end of for(iPage = 1; iPage <= PageNum; iPage++)

	if (IAPReset(client) > 0) INF("[ELAN] Update ALL Firmware successfully!\n");
	return 0;
}

// End Firmware Update

// Start sysfs
static ssize_t elan_ktf2k_gpio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct elan_ktf2k_ts_data *ts = private_ts;
//       ret = gpio_read_one_pin_value(gpio_reset_hdle, "ctp_reset");
	ret =  gpio_get_value(config_info.wakeup_gpio.gpio);
	//ret = gpio_get_value(ts->intr_gpio);
	sprintf(buf, "GPIO_TP_INT_N=%d\n", ret);
	ret = strlen(buf) + 1;
	return ret;
}

static DEVICE_ATTR(gpio, S_IRUGO, elan_ktf2k_gpio_show, NULL);

static ssize_t elan_ktf2k_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct elan_ktf2k_ts_data *ts = private_ts;

	sprintf(buf, "%s_x%4.4x\n", "ELAN_KTF2K", ts->fw_ver);
	ret = strlen(buf) + 1;
	return ret;
}

static DEVICE_ATTR(vendor, S_IRUGO, elan_ktf2k_vendor_show, NULL);

static ssize_t elan_ktf2k_id_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct elan_ktf2k_ts_data *ts = private_ts;
        return sprintf(buf, "%04x\n", ts->fw_id);
}
static DEVICE_ATTR(id, 0444, elan_ktf2k_id_show, NULL);

static ssize_t elan_ktf2k_version_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct elan_ktf2k_ts_data *ts = private_ts;
        return sprintf(buf, "%04x\n", ts->fw_ver);
}
static DEVICE_ATTR(version, 0444, elan_ktf2k_version_show, NULL);

static ssize_t elan_ktf2k_bootcode_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct elan_ktf2k_ts_data *ts = private_ts;
        return sprintf(buf, "%04x\n", ts->bc_ver);
}
static DEVICE_ATTR(bootcode, 0444, elan_ktf2k_bootcode_show, NULL);

static ssize_t elan_ktf2k_calibrate_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
        struct elan_ktf2k_ts_data *ts = private_ts;
        int ret;

        ret = elan_ktf2k_ts_rough_calibrate(private_ts->client);
        return ret==0 ?  count : ret;
}
static DEVICE_ATTR(calibrate, 0222, NULL, elan_ktf2k_calibrate_store);

static ssize_t ektf2k_trail_read(struct file *f, struct kobject *kobj,
                struct bin_attribute *attr, char *buf, loff_t off, size_t size)
{
        while (trail_head == trail_tail) {
                int rc = wait_event_interruptible(trail_wq, trail_head != trail_tail);
                if (rc) return rc;
        }
        struct trail_record *tr = &trail_data[trail_tail];
        trail_tail = (trail_tail + 1) % TRAIL_SIZE;
        return snprintf(buf, size, "%c%d  %4d %4d  %4d %4d\n",
                tr->mask & 0x80 ? '*' : ' ', tr->mask & 0x7f, tr->x1, tr->y1, tr->x2, tr->y2);
}

static ssize_t elan_ktf2k_run_update_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
        struct elan_ktf2k_ts_data *ts = private_ts;
        int ret;

        work_lock=1;
        cancel_work_sync(&ts->work);
        power_lock = 1;
        ret = Update_FW_One(ts->client, RECOVERY);
        power_lock = 0;
        work_lock=0;
        return ret==0 ? count : ret;
}
static DEVICE_ATTR(run_update, 0200, NULL, elan_ktf2k_run_update_store);


static ssize_t firmware_binary_read(struct file *f, struct kobject *kobj,
                struct bin_attribute *attr, char *buf, loff_t off, size_t size)
{
        if (off >= firmware_size) return 0;
        if (off + size > firmware_size) size = firmware_size - off;
        memcpy(buf, firmware_file + off, size);
        return size;
}

static ssize_t firmware_binary_write(struct file *f, struct kobject *kobj,
                struct bin_attribute *attr, char *buf, loff_t off, size_t size)
{
        if (! firmware_file) firmware_file = kzalloc(MAX_FIRMWARE_SIZE, GFP_KERNEL);
        if (! firmware_file) return -ENOMEM;
        if (off + size > MAX_FIRMWARE_SIZE) return -EFBIG;
        memcpy(firmware_file+off, buf, size);
        firmware_size = off + size;
        return size;
}

static struct bin_attribute firmware_attribute = {
    .attr = { .name = "firmware", .mode = 0600 },
    .read = firmware_binary_read,
    .write = firmware_binary_write,
    .size = 0,
};

static struct bin_attribute trail_attribute = {
    .attr = { .name = "trail", .mode = 0444 },
    .read = ektf2k_trail_read,
    .size = 0,
};

static struct kobject *android_touch_kobj;

static int elan_ktf2k_touch_sysfs_init(void)
{
	int ret = -1;

	android_touch_kobj = kobject_create_and_add("android_touch", NULL) ;
	if (android_touch_kobj) {
		ret = sysfs_create_file(android_touch_kobj, &dev_attr_gpio.attr);
		ret |= sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
		ret |= sysfs_create_file(android_touch_kobj, &dev_attr_id.attr);
		ret |= sysfs_create_file(android_touch_kobj, &dev_attr_version.attr);
		ret |= sysfs_create_file(android_touch_kobj, &dev_attr_bootcode.attr);
		ret |= sysfs_create_file(android_touch_kobj, &dev_attr_calibrate.attr);
		ret |= sysfs_create_file(android_touch_kobj, &dev_attr_run_update.attr);
		ret |= sysfs_create_bin_file(android_touch_kobj, &firmware_attribute);
		ret |= sysfs_create_bin_file(android_touch_kobj, &trail_attribute);
	}
	if (ret) {
		ERR("[elan]%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	return 0 ;
}


static void elan_touch_sysfs_deinit(void)
{
#if 1
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_gpio.attr);
	kobject_del(android_touch_kobj);
#endif	
}	

// end sysfs

static int __elan_ktf2k_ts_poll(struct i2c_client *client)
{	
	struct elan_ktf2k_ts_data *ts = private_ts;
	int status = 0, retry = 10;
 
	do {
		status =  gpio_get_value(config_info.irq_gpio.gpio);
#if 0		
	#if 1	
        	if(PRESS_DOWN == ctp_ops.get_pendown_state())
			status = 0;
		else
			status = 1;
	#else
		status = gpio_get_value(ts->intr_gpio);
	#endif
#endif	
		dev_dbg(&client->dev, "%s: status = %d\n", __func__, status);
		retry--;
		mdelay(20);
	} while (status == 1 && retry > 0);

	dev_dbg(&client->dev, "[elan]%s: poll interrupt status %s\n",
			__func__, status == 1 ? "high" : "low");
	return (status == 0 ? 0 : -ETIMEDOUT);
}

static int elan_ktf2k_ts_poll(struct i2c_client *client)
{	
	return 0;
	//return __elan_ktf2k_ts_poll(client);
}

static int elan_ktf2k_ts_get_data(struct i2c_client *client, uint8_t *cmd,
			uint8_t *buf, size_t size)
{
	int rc;

	dev_dbg(&client->dev, "[elan]%s: enter\n", __func__);

	if (buf == NULL)
		return -EINVAL;

	if ((i2c_master_send(client, cmd, 4)) != 4) {
		dev_err(&client->dev,
			"[elan]%s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0)
		return -EINVAL;
	else {
		if (i2c_master_recv(client, buf, size) != size ||
		    buf[0] != CMD_S_PKT)
			return -EINVAL;
	}

	return 0;
}

static int __hello_packet_handler(struct i2c_client *client)
{
	int rc;
	uint8_t buf_recv[8] = { 0 };

	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0) {
		ERR( "[elan] %s: Int poll failed!\n", __func__);
		RECOVERY=0x80;
		return RECOVERY;
		//return -EINVAL;
	}

	rc = i2c_master_recv(client, buf_recv, 8);
	DBG("[elan] %s: hello packet %2x:%2X:%2x:%2x:%2x:%2X:%2x:%2x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3] , buf_recv[4], buf_recv[5], buf_recv[6], buf_recv[7]);

	if(buf_recv[0]==0x55 && buf_recv[1]==0x55 && buf_recv[2]==0x80 && buf_recv[3]==0x80)
	{
             RECOVERY=0x80;
	     return RECOVERY; 
	}
	return 0;
}

static int __fw_packet_handler(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = private_ts;
	int rc;
	int major, minor;
	uint8_t cmd[] = {CMD_R_PKT, 0x00, 0x00, 0x01};/* Get Firmware Version*/
	uint8_t cmd_x[] = {0x53, 0x60, 0x00, 0x00}; /*Get x resolution*/
	uint8_t cmd_y[] = {0x53, 0x63, 0x00, 0x00}; /*Get y resolution*/
	uint8_t cmd_id[] = {0x53, 0xf0, 0x00, 0x01}; /*Get firmware ID*/
    uint8_t cmd_bc[] = {CMD_R_PKT, 0x01, 0x00, 0x01};/* Get BootCode Version*/
	uint8_t buf_recv[4] = {0};
// Firmware version
	rc = elan_ktf2k_ts_get_data(client, cmd, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	ts->fw_ver = major << 8 | minor;
	FW_VERSION = ts->fw_ver;
// Firmware ID
	rc = elan_ktf2k_ts_get_data(client, cmd_id, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	ts->fw_id = major << 8 | minor;
	FW_ID = ts->fw_id;
// Bootcode version
        rc = elan_ktf2k_ts_get_data(client, cmd_bc, buf_recv, 4);
        if (rc < 0)
                return rc;
        major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
        minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
        ts->bc_ver = major << 8 | minor;

// X Resolution
	rc = elan_ktf2k_ts_get_data(client, cmd_x, buf_recv, 4);
	if (rc < 0)
		return rc;
	minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
	ts->x_resolution =minor;
//#ifndef ELAN_TEN_FINGERS
	//X_RESOLUTION = ts->x_resolution;
	X_NATIVE = ts->x_resolution;
//#endif
	
// Y Resolution	
	rc = elan_ktf2k_ts_get_data(client, cmd_y, buf_recv, 4);
	if (rc < 0)
		return rc;
	minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
	ts->y_resolution =minor;
//#ifndef ELAN_TEN_FINGERS
	//Y_RESOLUTION = ts->y_resolution;
	Y_NATIVE = ts->y_resolution;
//#endif
	
	INF("[elan] %s: Firmware version: 0x%4.4x\n",
			__func__, ts->fw_ver);
	INF("[elan] %s: Firmware ID: 0x%4.4x\n",
			__func__, ts->fw_id);
	INF("[elan] %s: Bootcode Version: 0x%4.4x\n",
			__func__, ts->bc_ver);
	INF("[elan] %s: x resolution: %d, y resolution: %d\n",
			__func__, X_NATIVE, Y_NATIVE);

	
	return 0;
}

static inline int elan_ktf2k_ts_parse_xy(uint8_t *data,
			uint16_t *x, uint16_t *y)
{
	*x = *y = 0;	

	*x = (data[0] & 0xf0);
	*x <<= 4;	
	*x |= data[1];

	*y = (data[0] & 0x0f);
	*y <<= 8;	
	*y |= data[2];

	return 0;
}



static int elan_ktf2k_ts_setup(struct i2c_client *client)
{
	int rc;
	/* ��鴥�����������Ƿ�ɹ���������ͨ��IIC �ӿ�*/
	rc = __hello_packet_handler(client);

	mdelay(10);
	if (rc != 0x80){
	    /* ��ȡ�̼���Ϣ�����̼��汾ID��XY�ֱ��ʺ��������� */
	    rc = __fw_packet_handler(client);
	    if (rc < 0)
		    ERR("[elan] %s, fw_packet_handler fail, rc = %d", __func__, rc);
	    dev_dbg(&client->dev, "[elan] %s: firmware checking done.\n", __func__);
//Check for FW_VERSION, if 0x0000 means FW update fail!
	    if ( FW_VERSION == 0x00)
	    {
			rc = 0x80;
			INF("[elan] FW_VERSION = %d, last FW update fail\n", FW_VERSION);
	    }
      }
	return rc;
}

static int elan_ktf2k_ts_rough_calibrate(struct i2c_client *client){
      uint8_t cmd[] = {CMD_W_PKT, 0x29, 0x00, 0x01};

	//dev_info(&client->dev, "[elan] %s: enter\n", __func__);
	INF("[elan] %s: enter\n", __func__);
	dev_info(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);

	if ((i2c_master_send(client, cmd, sizeof(cmd))) != sizeof(cmd)) {
			ERR("[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf2k_ts_set_power_state(struct i2c_client *client, int state)
{
	uint8_t cmd[] = {CMD_W_PKT, 0x50, 0x00, 0x01};

	dev_dbg(&client->dev, "[elan] %s: enter\n", __func__);

	cmd[1] |= (state << 3);

	dev_dbg(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);

	if ((i2c_master_send(client, cmd, sizeof(cmd))) != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf2k_ts_get_power_state(struct i2c_client *client)
{
	int rc = 0;
	uint8_t cmd[] = {CMD_R_PKT, 0x50, 0x00, 0x01};
	uint8_t buf[4], power_state;

	rc = elan_ktf2k_ts_get_data(client, cmd, buf, 4);
	if (rc)
		return rc;

	power_state = buf[1];
	dev_dbg(&client->dev, "[elan] dump repsponse: %0x\n", power_state);
	power_state = (power_state & PWR_STATE_MASK) >> 3;
	dev_dbg(&client->dev, "[elan] power state = %s\n",
		power_state == PWR_STATE_DEEP_SLEEP ?
		"Deep Sleep" : "Normal/Idle");

	return power_state;
}

extern int register_touchpower_callback(int (*touch_enable_cb)(void *data, bool onOff), int (*touch_state_cb)(void *data), void *touch_context);
static int ektf2k_tp_enabled = 1;

static int tp_enable(void *data, bool onOff)
{
	int rc, retry = 3;
	struct elan_ktf2k_ts_data *ts = (struct elan_ktf2k_ts_data *) data;

	if (onOff) {
		#ifdef CONFIG_PB_PLATFORM
		//SetTouchPower(1);
		#endif
		ektf2k_tp_enabled = 1;
		input_set_power_enable(&(config_info.input_type), 1);
		/*end add */
		do {
			rc = elan_ktf2k_ts_set_power_state(ts->client, PWR_STATE_NORMAL);
			mdelay(200);
			rc = elan_ktf2k_ts_get_power_state(ts->client);
			if (rc != PWR_STATE_NORMAL)
				ERR("[elan] %s: wake up tp failed! err = %d\n",
					__func__, rc);
			else
				break;
		} while (--retry);

		//enable_irq(SW_INT_IRQNO_PIO);
		input_set_int_enable(&(config_info.input_type), 1);
	} else {
		input_set_int_enable(&(config_info.input_type), 0);

		rc = cancel_work_sync(&ts->work);
		rc = elan_ktf2k_ts_set_power_state(ts->client, PWR_STATE_DEEP_SLEEP);

		/*add by A-GAN  When super standby turn off */
		input_set_power_enable(&(config_info.input_type), 0);
		/*end add */

		#ifdef CONFIG_PB_PLATFORM
		//SetTouchPower(0);
		#endif

		ektf2k_tp_enabled = 0;
	}
	return 0;
}

static int is_tp_enabled(void *data)
{
	return ektf2k_tp_enabled;
}

static int elan_ktf2k_ts_recv_data(struct i2c_client *client, uint8_t *buf, int bytes_to_recv)
{
	int rc;
	if (buf == NULL)
		return -EINVAL;

	memset(buf, 0, bytes_to_recv);

	rc = i2c_master_recv(client, buf, bytes_to_recv);
	if (rc == bytes_to_recv) {
    DBG("[elan] %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
	} else {
		dev_err(&client->dev, "[elan] %s: i2c_master_recv error?! \n", __func__);
		return -1;
	}
	return rc;
}

static void elan_ktf2k_ts_report_data(struct i2c_client *client, uint8_t *buf)
{
	struct elan_ktf2k_ts_data *ts = private_ts;
	struct input_dev *idev = ts->input_dev;
	uint16_t x, y;
	uint16_t fbits=0;
	uint8_t i, num, reported = 0;
	uint8_t idx, btn_idx;
	int finger_num;

	/* for 2 fingers */
	finger_num = 2;
	//		num = buf[7] & 0x06;		// for elan old 5D protocol the finger ID is 0x06
	//		fbits = (buf[7] & 0x06) >> 1;	// for elan old 5D protocol the finger ID is 0x06

	// DmitryZ: 5a [xy xx yy] [xy xx yy] mask
	num = (buf[7] & 1) + ((buf[7] >> 1) & 1);
	fbits = buf[7] & 3;
	idx=1;
	btn_idx=7;

    memset(&trail_data[trail_head], 0, sizeof(struct trail_record));
    trail_data[trail_head].mask = fbits;

    for (i = 0; i < finger_num; i++) {
      if ((fbits & 0x01)) {
        elan_ktf2k_ts_parse_xy(&buf[idx], &x, &y);
        DBG("[elan_debug] %s, x=%d, y=%d\n",__func__, x , y);
        if (X_REVERT) x = X_NATIVE-1-x;
        if (Y_REVERT) y = Y_NATIVE-1-y;
        x = (x * X_RESOLUTION) / X_NATIVE;
        y = (y * Y_RESOLUTION) / Y_NATIVE;
        if (XY_EXCHANGE) swap(x, y);
        DBG("[elan_debug] %s, x=%d, y=%d\n",__func__, x , y);
        input_report_abs(idev, ABS_MT_TRACKING_ID, i);
        //input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
        input_report_abs(idev, ABS_MT_POSITION_X, x);
        input_report_abs(idev, ABS_MT_POSITION_Y, y);
        input_mt_sync(idev);
        if (i == 0) { trail_data[trail_head].x1 = x; trail_data[trail_head].y1 = y; }
        if (i == 1) { trail_data[trail_head].x2 = x; trail_data[trail_head].y2 = y; }
        reported++;
      } // end if finger status
      fbits = fbits >> 1;
      idx += 3;
    } // end for

    if (reported) {
      input_sync(idev);
    }else {
      input_mt_sync(idev);
      input_sync(idev);
    }

    trail_head = (trail_head + 1) % TRAIL_SIZE;
    if (trail_tail == trail_head) trail_tail = (trail_tail + 1) % TRAIL_SIZE;
    wake_up(&trail_wq);
}

static void elan_ktf2k_ts_work_func(struct work_struct *work)
{	
	int rc;
	struct elan_ktf2k_ts_data *ts = private_ts;
	container_of(work, struct elan_ktf2k_ts_data, work);
	uint8_t buf[PACKET_SIZE] = { 0 };
	uint8_t buf1[40] = { 0 };
	

	DBG("[elan_debug] %s\n",__func__);
		//if (gpio_read_one_pin_value(gpio_reset_hdle, "ctp_reset"))
		//{
		//	printk("[elan] Detected the jitter on INT pin");
		//	enable_irq(SW_INT_IRQNO_PIO);
		//	return;
		//}
	      /* ��ȡ�������� */
		rc = elan_ktf2k_ts_recv_data(ts->client, buf,PACKET_SIZE);
		if (rc < 0)
		{
			ERR("[elan] Received the packet Error.\n");
//			enable_irq(SW_INT_IRQNO_PIO);
			input_set_int_enable(&(config_info.input_type), 1);

			return;
		}
             /* ����ȥ�� */

#ifndef ELAN_BUFFER_MODE
		elan_ktf2k_ts_report_data(ts->client, buf);
#else

	if ((buf[0] == 0x63) && ((buf[1] == 2) || (buf[1] == 3))) {
		rc = elan_ktf2k_ts_recv_data(ts->client, buf1, 40);
		if (rc < 0){
//			enable_irq(SW_INT_IRQNO_PIO);
			input_set_int_enable(&(config_info.input_type), 1);
			return;
		}
		
		elan_ktf2k_ts_report_data(ts->client, buf1);
		if (buf[1] == 3) {
			rc = elan_ktf2k_ts_recv_data(ts->client, buf1, 40);
			if (rc < 0){
//				enable_irq(SW_INT_IRQNO_PIO);
				input_set_int_enable(&(config_info.input_type), 1);

				return;
			}
			elan_ktf2k_ts_report_data(ts->client, buf1);
		}
	}
#endif
schedule:

//		enable_irq(SW_INT_IRQNO_PIO);
		input_set_int_enable(&(config_info.input_type), 1);


	return;
}

/* �жϴ����� */
irqreturn_t elan_ktf2k_ts_irq_handler(int irq, void *dev_id)
{
	struct elan_ktf2k_ts_data *ts = private_ts;
	struct i2c_client *client = ts->client;

	input_set_int_enable(&(config_info.input_type), 0);
	queue_work(ts->elan_wq, &ts->work);
	return IRQ_HANDLED;
}

static int elan_ktf2k_ts_register_interrupt(struct i2c_client *client)
{
	int ret;
	struct elan_ktf2k_ts_data *ts = private_ts;
	int err = 0;
	DBG("**CTP*** %s %d ,ts = %x, private_ts = %x ,input_type = %d, dev=%x\n", __FUNCTION__, __LINE__,
		ts,private_ts,config_info.input_type,config_info.dev);	
	
	ret = input_request_int(&(config_info.input_type), elan_ktf2k_ts_irq_handler,
				CTP_IRQ_MODE, private_ts);
	if (ret) {
		ERR( "ekt2k_init_events: request irq failed\n");
	}
	return err;
}

static int elan_ktf2k_ts_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	DBG(" %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);	
	int err = 0;
	int fw_err = 0;
	struct elan_ktf2k_i2c_platform_data *pdata;
	struct elan_ktf2k_ts_data *ts;
	int New_FW_ID;	
	int New_FW_VER;	
		
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ERR("[elan] %s: i2c check functionality error\n", __func__);
		err = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct elan_ktf2k_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		ERR("[elan] %s: allocate elan_ktf2k_ts_data failed\n", __func__);
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}
	private_ts = ts;

	ts->elan_wq = create_singlethread_workqueue("elan_wq");
	if (!ts->elan_wq) {
		ERR("[elan] %s: create workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_wq_failed;
	}

	INIT_WORK(&ts->work, elan_ktf2k_ts_work_func);
	ts->client = client;
	i2c_set_clientdata(client, ts);
#if 0	
// james: maybe remove	
	pdata = client->dev.platform_data;
	if (likely(pdata != NULL)) {
		ts->intr_gpio = pdata->intr_gpio;
	}
#endif
	fw_err = elan_ktf2k_ts_setup(client);
	if (fw_err < 0) {
		ERR("No Elan chip inside\n");
//		fw_err = -ENODEV;  
	}
	DBG("input_allocate_device %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev, "[elan] Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = ELAN_KTF2K_NAME;//"elan-touchscreen";   // for andorid2.2 Froyo  
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->dev.parent = &client->dev;
	input_set_drvdata(ts->input_dev, ts);


	input_set_abs_params(ts->input_dev,ABS_MT_TRACKING_ID, 0, (MAX_CONTACTS+1), 0, 0);
	set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	set_bit(ABS_MT_POSITION_X, ts->input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, ts->input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, ts->input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, ts->input_dev->absbit);

	input_set_abs_params(ts->input_dev,ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(ts->input_dev,ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(ts->input_dev,ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(ts->input_dev,ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	
	err = input_register_device(ts->input_dev);
	if (err) {
			ERR("[elan]%s: unable to register %s input device\n",
			__func__, ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	config_info.dev = &ts->input_dev->dev;
	
	elan_ktf2k_ts_register_interrupt(ts->client);

	elan_ktf2k_touch_sysfs_init();

	dev_info(&client->dev, "[elan] Start touchscreen %s in interrupt mode\n",
		ts->input_dev->name);

// Firmware Update
	ts->firmware.minor = MISC_DYNAMIC_MINOR;
	ts->firmware.name = "elan-iap";
	ts->firmware.fops = &elan_touch_fops;
	ts->firmware.mode = S_IFREG|S_IRWXUGO; 

	if (misc_register(&ts->firmware) < 0)
  		ERR("[ELAN]misc_register failed!!");
  	else
		DBG("[ELAN]misc_register finished!!");

	input_set_int_enable(&(config_info.input_type), 1);
	device_init_wakeup(&client->dev, 1);
	//register keylock interface;
	register_touchpower_callback(tp_enable, is_tp_enabled, ts);
	DBG("[ELAN]probe finished!!");
	return 0;

err_input_register_device_failed:
	if (ts->input_dev)
		input_free_device(ts->input_dev);

err_input_dev_alloc_failed: 
	if (ts->elan_wq)
		destroy_workqueue(ts->elan_wq);

err_create_wq_failed:
	kfree(ts);

err_alloc_data_failed:
err_check_functionality_failed:

	return err;
}

static int elan_ktf2k_ts_remove(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);

	elan_touch_sysfs_deinit();
	//unregister keylock interface
	register_touchpower_callback(NULL, NULL, NULL);

	input_set_int_enable(&(config_info.input_type), 0);
	input_set_power_enable(&(config_info.input_type), 0);

//	unregister_early_suspend(&ts->early_suspend);
	//free_irq(SW_INT_IRQNO_PIO, ts);
	input_free_int(&(config_info.input_type), ts);
	input_free_platform_resource(&(config_info.input_type));

	if (ts->elan_wq)
		destroy_workqueue(ts->elan_wq);
	input_unregister_device(ts->input_dev);
	kfree(ts);

//    ctp_ops.free_platform_resource();

	return 0;
}


static int elan_ktf2k_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int rc = 0;

	
	//add by ffz
	INF("[elan] %s: enter\n", __func__);
	if ((check_scene_locked(SCENE_WLAN_STANDBY) == 0) || (check_scene_locked(SCENE_NORMAL_STANDBY) == 0)){
		//enable ctp wake up source
		if(enable_wakeup_src(CPUS_GPIO_SRC, config_info.irq_gpio.gpio) != 0){
			ERR("[elan] enable ctp wakeup src failed\n");
		}else{
			
		} 
		return 0;
	} 
	//end 
//	if(power_lock==0) /* The power_lock can be removed when firmware upgrade procedure will not be enter into suspend mode.  */
//	{
//		INF("[elan] %s: enter\n", __func__);
//
////		disable_irq(SW_INT_IRQNO_PIO);
//		input_set_int_enable(&(config_info.input_type), 0);
//
//		rc = cancel_work_sync(&ts->work);
//		if (rc)
//		{
//			input_set_int_enable(&(config_info.input_type), 1);
////			enable_irq(SW_INT_IRQNO_PIO);
//		}
//
//		rc = elan_ktf2k_ts_set_power_state(client, PWR_STATE_DEEP_SLEEP);
//
//		/*add by A-GAN  When super standby turn off */
//		input_set_power_enable(&(config_info.input_type), 0);
//		/*end add */
//	}
	return 0;
}

static int elan_ktf2k_ts_resume(struct i2c_client *client)
{
	int rc = 0, retry = 3;
	INF("[elan] %s: enter\n", __func__);
	//add by ffz
	if ((check_scene_locked(SCENE_WLAN_STANDBY) == 0) || (check_scene_locked(SCENE_NORMAL_STANDBY) == 0)){
		
		return 0;
	}
	//end
//	if(power_lock==0)   /* The power_lock can be removed when firmware upgrade procedure will not be enter into suspend mode.  */
//	{
//		INF("[elan] %s: enter\n", __func__);
//		/*add by A-GAN  When super standby turn off */
//		input_set_power_enable(&(config_info.input_type), 1);
//		/*end add */
//		do {
//			rc = elan_ktf2k_ts_set_power_state(client, PWR_STATE_NORMAL);
//			mdelay(200);
//			rc = elan_ktf2k_ts_get_power_state(client);
//			if (rc != PWR_STATE_NORMAL)
//				ERR("[elan] %s: wake up tp failed! err = %d\n",
//					__func__, rc);
//			else
//				break;
//		} while (--retry);
//
//		//enable_irq(SW_INT_IRQNO_PIO);
//		input_set_int_enable(&(config_info.input_type), 1);
//	}
	return 0;
}


/* #define ELAN_KTF2K_NAME "elan-ktf2k" */
static const struct i2c_device_id elan_ktf2k_ts_id[] = {
	{ ELAN_KTF2K_NAME, 0 },
	{ }
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

static void __exit elan_ktf2k_ts_exit(void)
{
	i2c_del_driver(&ektf2k_ts_driver);
	input_free_platform_resource(&(config_info.input_type));
	return;
}

/**
 * ctp_wakeup - function
 *
 */
int ctp_wakeup(int status,int ms)
{
	DBG(" %s:status:%d,ms = %d\n",__func__,status,ms);

	if (status == 0) {

		if(ms == 0) {
			__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
		}else {
			__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
			msleep(ms);
			__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
		}
	}
	if (status == 1) {
		if(ms == 0) {
			__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
		}else {
			__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
			msleep(ms);
			__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
		}
	}
	msleep(5);

	return 0;
}

/**
 * ctp_print_info - sysconfig print function
 * return value:
 *
 */
void ctp_print_info(struct ctp_config_info info)
{
		INF("info.ctp_used:%d\n",info.ctp_used);
		INF("info.ctp_name:%s\n",info.name);
		INF("info.twi_id:%d\n",info.twi_id);
		INF("info.screen_max_x:%d\n",info.screen_max_x);
		INF("info.screen_max_y:%d\n",info.screen_max_y);
		INF("info.revert_x_flag:%d\n",info.revert_x_flag);
		INF("info.revert_y_flag:%d\n",info.revert_y_flag);
		INF("info.exchange_x_y_flag:%d\n",info.exchange_x_y_flag);
		INF("info.irq_gpio_number:%d\n",info.irq_gpio.gpio);
		INF("info.wakeup_gpio_number:%d\n",info.wakeup_gpio.gpio);
}

static int ctp_get_system_config(void)
{
	fwname = config_info.name;
	INF("%s:fwname:%s\n",__func__,fwname);

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
	if((screen_max_x == 0) || (screen_max_y == 0)){
           ERR("%s:read config error!\n",__func__);
           return 0;
    }
    return 1;
}


static int __init elan_ktf2k_ts_init(void)
{
	int ret = -1;     

	INF("---elan_ktf2k_ts_init--------\n");
	//input_set_power_enable(&(config_info.input_type), 1);
	if (input_fetch_sysconfig_para(&(config_info.input_type))) {
		ERR("%s: ctp_fetch_sysconfig_para err.\n", __func__);
		return 0;
	} else {
		ret = input_init_platform_resource(&(config_info.input_type));
		if (0 != ret) {
			ERR("%s:ctp_ops.init_platform_resource err. \n", __func__);    
		}
	}
	if (config_info.ctp_used == 0) {
		ERR("*** ctp_used set to 0 !\n");
		ERR("*** if use ctp,please put the sys_config.fex ctp_used set to 1. \n");
		return 0;
	}
	if (!ctp_get_system_config()) {
		ERR("%s:read config fail!\n",__func__);
		return ret;
	}
	ctp_print_info(config_info);
	input_set_power_enable(&(config_info.input_type), 1);
    msleep(20);
	ctp_wakeup(1,0);
	ret = i2c_add_driver(&ektf2k_ts_driver);
	return ret;
}

module_init(elan_ktf2k_ts_init);
module_exit(elan_ktf2k_ts_exit);

MODULE_DESCRIPTION("ELAN KTF2K Touchscreen Driver");
MODULE_LICENSE("GPL");


