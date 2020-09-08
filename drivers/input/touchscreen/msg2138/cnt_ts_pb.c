/* 
 * CNT touch driver based on
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x TouchScreen driver. 
 *
 * Copyright (c) 2010  Focal tech Ltd.
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
 *
 *	note: only support mulititouch	Wenfs 2010-10-01
 *  for this touchscreen to work, it's slave addr must be set to 0x7e | 0x70
 */
#define DEBUG

#include <linux/i2c.h>
#include <linux/input.h>

//#if defined(CONFIG_PM)
//	#include <linux/pm.h>
//	#include <linux/suspend.h>
//	#include <linux/power/aw_pm.h>
//#endif

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
//#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "ctp_platform_ops.h"
#include <linux/suspend.h>
#include <linux/proc_fs.h>

#include <linux/sys_config.h>
#include <linux/gpio.h>
#include <linux/power/scenelock.h>

#include "cntouch_i2c_ts.h"

#define I2C_MINORS 	256
#define I2C_MAJOR 	125

//suspend mode
#define SUSPEND_MODE 	0x1 //touch held in reset to reduce power consumption
#define LOCK_MODE 		0x2 //TS locked

//#define PRINT_INT_INFO
#define PRINT_POINT_INFO
#define DEBUG

struct i2c_dev{
struct list_head list;	
struct i2c_adapter *adap;
struct device *dev;
};

static struct class *i2c_dev_class;
static LIST_HEAD (i2c_dev_list);
static DEFINE_SPINLOCK(i2c_dev_list_lock);

static struct i2c_client *this_client;

#ifdef PRINT_POINT_INFO 
#define print_point_info(fmt, args...)   \
        do{                              \
                pr_info(fmt, ##args);     \
        }while(0)
#else
#define print_point_info(fmt, args...)   //
#endif

#ifdef PRINT_INT_INFO 
#define print_int_info(fmt, args...)     \
        do{                              \
                pr_info(fmt, ##args);     \
        }while(0)
#else
#define print_int_info(fmt, args...)   //
#endif
///////////////////////////////////////////////
//specific tp related macro: need be configured for specific tp
#define CTP_IRQ_NO			(gpio_int_info[0].port_num)

//#define CTP_IRQ_MODE			(NEGATIVE_EDGE)
#define CTP_IRQ_MODE			(POSITIVE_EDGE)
#define CTP_NAME			"cnt_ts"//"ft5x_ts"
#define TS_RESET_LOW_PERIOD		(10)
#define TS_INITIAL_HIGH_PERIOD		(30)
#define TS_WAKEUP_LOW_PERIOD	(20)
#define TS_WAKEUP_HIGH_PERIOD	(20)
#define TS_POLL_DELAY			(10)	/* ms delay between samples */
#define TS_POLL_PERIOD			(10)	/* ms delay between samples */
//#define SCREEN_MAX_X			(screen_max_x)
//#define SCREEN_MAX_Y			(screen_max_y)
//#define PRESS_MAX			(255)


static void* __iomem gpio_addr = NULL;
static int gpio_int_hdle = 0;
static int gpio_wakeup_hdle = 0;
static int gpio_reset_hdle = 0;
static int gpio_wakeup_enable = 1;
static int gpio_reset_enable = 1;

static int gpio_power_hdle = 0;

static int screen_max_x = 0;
static int screen_max_y = 0;
static int revert_x_flag = 0;
static int revert_y_flag = 0;
static int exchange_x_y_flag = 0;

static REPORT_FINGER_INFO_T _st_finger_infos[CFG_MAX_POINT_NUM];

static int force=0;
module_param(force, int, 0);
MODULE_PARM_DESC(force, "Do not check name in sys_config");

#define MSG2133_DEBUG
#ifdef MSG2133_DEBUG
#define MSG2133_DBG(format, ...)	printk(KERN_INFO "MSG2133 " format "\n", ## __VA_ARGS__)
#define DBG(x...) do { printk("[%s:%.5i] ",__func__,__LINE__); printk(x); } while(0);
#else
#define MSG2133_DBG(format, ...)
#endif


#if MSG2133_UPDATE

struct class *firmware_class;
struct device *firmware_cmd_dev;
static int tp_type = TP_HUANGZE;
static  unsigned char g_dwiic_info_data[1024];
static int FwDataCnt;
static struct fw_version fw_v;
//static unsigned char temp[94][1024] = {};

#define FW_SIZE (50*1024)
static unsigned char fw_buffer[FW_SIZE];

static u8 _gOneDimenFwData[FW_SIZE];
u8 g_FwData[94][1024];
u32 g_FwDataCount = 0;

void DrvFwCtrlGetCustomerFirmwareVersion(u16 *pMajor, u16 *pMinor, u8 **ppVersion);

static void msg2133_reset(void)
{
	MSG2133_DBG("..........msg2133_reset.........\n");

	if(gpio_reset_enable){
		pr_info("%s. \n", __func__);
		if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 0, "ctp_reset")){
			pr_info("%s: err when operate gpio. \n", __func__);
		}
		msleep(100);
		if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 1, "ctp_reset")){
			pr_info("%s: err when operate gpio. \n", __func__);
		}
		msleep(300);
	} else
		pr_err("%s reset not enabled!\n", __func__);
}



static bool msg2133_i2c_read(char *pbt_buf, int dw_lenth)
{
    int ret;
    MSG2133_DBG("The msg_i2c_client->addr=0x%x\n",this_client->addr);
    ret = i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret <= 0){
        MSG2133_DBG("msg_i2c_read_interface error\n");
        return false;
    }

    return true;
}

static bool msg2133_i2c_write(char *pbt_buf, int dw_lenth)
{
    int ret;
	MSG2133_DBG("hhh %s\n", __func__);
    MSG2133_DBG("The msg_i2c_client->addr=0x%x\n",this_client->addr);
    ret = i2c_master_send(this_client, pbt_buf, dw_lenth);

    if(ret <= 0){
        MSG2133_DBG("msg_i2c_read_interface error\n");
        return false;
    }

    return true;
}

static void i2c_read_msg2133(unsigned char *pbt_buf, int dw_lenth)
{
    this_client->addr = i2c_dbbus_addr;
	i2c_master_recv(this_client, pbt_buf, dw_lenth);	//0xC5_8bit
	this_client->addr = i2c_work_addr;

//	DBG("[%s] dw_lenth = %i\n", __func__, dw_lenth);
//	int i;
//	for (i = 0; i != dw_lenth; i++) {
//		DBG("%hhx ", pbt_buf[i]);
//	}
//	DBG("\n");
}

static void i2c_write_msg2133(unsigned char *pbt_buf, int dw_lenth)
{
//	DBG("[%s] dw_lenth = %i\n", __func__, dw_lenth);
//	int i;
//	for (i = 0; i != dw_lenth; i++) {
//		DBG("%hhx ", pbt_buf[i]);
//	}
//	DBG("\n");

	this_client->addr = i2c_dbbus_addr;
	i2c_master_send(this_client, pbt_buf, dw_lenth);		//0xC4_8bit
	this_client->addr = i2c_work_addr;
}

static void i2c_read_update_msg2133(unsigned char *pbt_buf, int dw_lenth)
{	
	MSG2133_DBG("hhh %s\n", __func__);

	this_client->addr = MSG2133_FW_UPDATE_ADDR;
	i2c_master_recv(this_client, pbt_buf, dw_lenth);	//0x93_8bit
	this_client->addr = i2c_work_addr;
}

static void i2c_write_update_msg2133(unsigned char *pbt_buf, int dw_lenth)
{	
	MSG2133_DBG("hhh %s\n", __func__);

    this_client->addr = MSG2133_FW_UPDATE_ADDR;
	i2c_master_send(this_client, pbt_buf, dw_lenth);	//0x92_8bit
	this_client->addr = i2c_work_addr;
}

static int msg2133_get_version(struct fw_version *fw)
{
	unsigned char dbbus_tx_data[3];
	unsigned char dbbus_rx_data[5] ;
	int i;
	
	MSG2133_DBG("%s\n", __func__);
/*	
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
    */
	MSG2133_DBG("\n");
	for ( i = 0; i < 10; i++)
	{
	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x74;
	msg2133_i2c_write(&dbbus_tx_data[0], 3);
	mdelay(50);
	msg2133_i2c_read(&dbbus_rx_data[0], 5);

    MSG2133_DBG("dbbus_rx_data[0] = %x\n",dbbus_rx_data[0]);
    MSG2133_DBG("dbbus_rx_data[1] = %x\n",dbbus_rx_data[1]);
    MSG2133_DBG("dbbus_rx_data[2] = %x\n",dbbus_rx_data[2]);
    MSG2133_DBG("dbbus_rx_data[3] = %x\n",dbbus_rx_data[3]);
    MSG2133_DBG("dbbus_rx_data[4] = %x\n",dbbus_rx_data[4]);

	fw->major = dbbus_rx_data[0];
	fw->minor = (dbbus_rx_data[3] << 8) + dbbus_rx_data[2];
	fw->VenderID= (dbbus_rx_data[4]);
	MSG2133_DBG("%s, major = 0x%x, minor = 0x%x\n,VenderID = 0x%x\n", __func__, fw->major, fw->minor,fw->VenderID);
	if( (fw->major & 0xff00 )== 0)
	break;
	}
	return 0;
}


void dbbusDWIICEnterSerialDebugMode(void)//Check
{
    unsigned char data[5];
	MSG2133_DBG("hhh %s\n", __func__);
    // Enter the Serial Debug Mode
    data[0] = 0x53;
    data[1] = 0x45;
    data[2] = 0x52;
    data[3] = 0x44;
    data[4] = 0x42;
    i2c_write_msg2133(data, 5);
}

void dbbusDWIICStopMCU(void)//Check
{
    unsigned char data[1];
	MSG2133_DBG("hhh %s\n", __func__);
    // Stop the MCU
    data[0] = 0x37;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICUseBus(void)//Check
{
    unsigned char data[1];
	MSG2133_DBG("hhh %s\n", __func__);
    // IIC Use Bus
    data[0] = 0x35;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICReshape(void)//Check
{
    unsigned char data[1];
	MSG2133_DBG("hhh %s\n", __func__);
    // IIC Re-shape
    data[0] = 0x71;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICNotUseBus(void)//Check
{
    unsigned char data[1];
	MSG2133_DBG("hhh %s\n", __func__);
    // IIC Not Use Bus
    data[0] = 0x34;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICNotStopMCU(void)//Check
{
    unsigned char data[1];
	MSG2133_DBG("hhh %s\n", __func__);
    // Not Stop the MCU
    data[0] = 0x36;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICExitSerialDebugMode(void)//Check
{
    unsigned char data[1];
	MSG2133_DBG("hhh %s\n", __func__);
    // Exit the Serial Debug Mode
    data[0] = 0x45;
    i2c_write_msg2133(data, 1);
    // Delay some interval to guard the next transaction
}

void drvISP_EntryIspMode(void)//Check
{
    unsigned char bWriteData[5] =
    {
        0x4D, 0x53, 0x54, 0x41, 0x52
    };
	MSG2133_DBG("hhh %s\n", __func__);
    i2c_write_update_msg2133(bWriteData, 5);
    msleep(10);           // delay about 10ms
}

void drvISP_WriteEnable(void)//Check
{
    unsigned char bWriteData[2] =
    {
        0x10, 0x06
    };
    unsigned char bWriteData1 = 0x12;
	MSG2133_DBG("hhh %s\n", __func__);
    i2c_write_update_msg2133(bWriteData, 2);
    i2c_write_update_msg2133(&bWriteData1, 1);
}

void drvISP_ExitIspMode(void)//Check
{
	unsigned char bWriteData = 0x24;
	MSG2133_DBG("hhh %s\n", __func__);
	i2c_write_update_msg2133(&bWriteData, 1);
}


#if 0//Old function
unsigned char drvISP_Read(unsigned char n, unsigned char *pDataToRead)    //First it needs send 0x11 to notify we want to get flash data back.
{
    unsigned char Read_cmd = 0x11;
    unsigned char i = 0;
    unsigned char dbbus_rx_data[16] = {0};
    i2c_write_update_msg2133(&Read_cmd, 1);
    //if (n == 1)
    {
        i2c_read_update_msg2133(&dbbus_rx_data[0], n + 1);

        for(i = 0; i < n; i++)
        {
            *(pDataToRead + i) = dbbus_rx_data[i + 1];
        }
    }
    //else
    {
        //     i2c_read_update_msg2133(pDataToRead, n);
    }
    return 0;
}
#endif
static unsigned char drvISP_Read(unsigned char n, unsigned char *pDataToRead) //First it needs send 0x11 to notify we want to get flash data back. //Check
{
    unsigned char Read_cmd = 0x11;
    unsigned char dbbus_rx_data[2] = {0xFF, 0xFF};
	MSG2133_DBG("hhh %s\n", __func__);
    i2c_write_update_msg2133(&Read_cmd, 1);
    msleep(10);        // delay about 1000us*****
    if ( n == 1 )
    {
        i2c_read_update_msg2133( &dbbus_rx_data[0], 2 );

        // Ideally, the obtained dbbus_rx_data[0~1] stands for the following meaning:
        //  dbbus_rx_data[0]  |  dbbus_rx_data[1]  | status
        // -------------------+--------------------+--------
        //       0x00         |       0x00         |  0x00
        // -------------------+--------------------+--------
        //       0x??         |       0x00         |  0x??
        // -------------------+--------------------+--------
        //       0x00         |       0x??         |  0x??
        //
        // Therefore, we build this field patch to return the status to *pDataToRead.
        *pDataToRead = ( ( dbbus_rx_data[0] >= dbbus_rx_data[1] ) ? \
                         dbbus_rx_data[0]  : dbbus_rx_data[1] );
    }
    else
    {
        i2c_read_update_msg2133 ( pDataToRead, n );
    }

    return 0;
}


unsigned char drvISP_ReadStatus(void)//Check
{
    unsigned char bReadData = 0;
    unsigned char bWriteData[2] =
    {
        0x10, 0x05
    };
    unsigned char bWriteData1 = 0x12;
	MSG2133_DBG("hhh %s\n", __func__);
//    msleep(1);           // delay about 100us
    i2c_write_update_msg2133(bWriteData, 2);
    msleep(1);           // delay about 100us
    drvISP_Read(1, &bReadData);
//    msleep(10);           // delay about 10ms
    i2c_write_update_msg2133(&bWriteData1, 1);
    return bReadData;
}


void drvISP_SectorErase(unsigned int addr)//This might remove
{
	unsigned char bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	unsigned char  bWriteData1 = 0x12;
	MSG2133_DBG("hhh %s\n", __func__);
	MSG2133_DBG("The drvISP_ReadStatus0=%d\n", drvISP_ReadStatus());
	drvISP_WriteEnable();
	MSG2133_DBG("The drvISP_ReadStatus1=%d\n", drvISP_ReadStatus());
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x50;
	i2c_write_update_msg2133(&bWriteData, 2);
	i2c_write_update_msg2133(&bWriteData1, 1);
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x01;
	bWriteData[2] = 0x00;
	i2c_write_update_msg2133(bWriteData, 3);
	i2c_write_update_msg2133(&bWriteData1, 1);
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x04;
	i2c_write_update_msg2133(bWriteData, 2);
	i2c_write_update_msg2133(&bWriteData1, 1); 
	while((drvISP_ReadStatus() & 0x01) == 0x01);
	
	drvISP_WriteEnable();
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x20; //Sector Erase
	bWriteData[2] = (( addr >> 16) & 0xFF);
	bWriteData[3] = (( addr >> 8 ) & 0xFF);
	bWriteData[4] = ( addr & 0xFF); 
	i2c_write_update_msg2133(&bWriteData, 5);
	i2c_write_update_msg2133(&bWriteData1, 1);
	while((drvISP_ReadStatus() & 0x01) == 0x01);
}

static void drvISP_BlockErase(unsigned int addr)//This might remove
{
    unsigned char bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    unsigned char bWriteData1 = 0x12;
    unsigned int timeOutCount=0;
	MSG2133_DBG("hhh %s\n", __func__);
    drvISP_WriteEnable();
    //Enable write status register
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x50;
    i2c_write_update_msg2133(bWriteData, 2);
    i2c_write_update_msg2133(&bWriteData1, 1);
    //Write Status
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x01;
    bWriteData[2] = 0x00;
    i2c_write_update_msg2133(bWriteData, 3);
    i2c_write_update_msg2133(&bWriteData1, 1);
    //Write disable
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x04;
    i2c_write_update_msg2133(bWriteData, 2);
    i2c_write_update_msg2133(&bWriteData1, 1);

    timeOutCount=0;
    msleep(1);           // delay about 100us
    while((drvISP_ReadStatus() & 0x01) == 0x01)
    {
        timeOutCount++;
	 if ( timeOutCount > 10000 ) 
            break; /* around 1 sec timeout */
    }

    //pr_ch("The drvISP_ReadStatus3=%d\n", drvISP_ReadStatus());
    drvISP_WriteEnable();
    //pr_ch("The drvISP_ReadStatus4=%d\n", drvISP_ReadStatus());
    bWriteData[0] = 0x10;
    bWriteData[1] = 0xC7;        //Block Erase
    //bWriteData[2] = ((addr >> 16) & 0xFF) ;
    //bWriteData[3] = ((addr >> 8) & 0xFF) ;
    // bWriteData[4] = (addr & 0xFF) ;
    i2c_write_update_msg2133(bWriteData, 2);
    //i2c_write_update_msg2133( &bWriteData, 5);
    i2c_write_update_msg2133(&bWriteData1, 1);

    timeOutCount=0;
    msleep(1);           // delay about 100us
    while((drvISP_ReadStatus() & 0x01) == 0x01)
    {
        timeOutCount++;
	 if ( timeOutCount > 10000 ) 
            break; /* around 1 sec timeout */
    }
}


void drvISP_Program(unsigned short k, unsigned char *pDataToWrite)//Check
{
    unsigned short i = 0;
    unsigned short j = 0;
    //U16 n = 0;
    unsigned char TX_data[133];
    unsigned char bWriteData1 = 0x12;
    unsigned int addr = k * 1024;
    unsigned int timeOutCount = 0; 
	MSG2133_DBG("hhh %s\n", __func__);

    for(j = 0; j < 8; j++)    //128*8 cycle
    {
        TX_data[0] = 0x10;
        TX_data[1] = 0x02;// Page Program CMD
        TX_data[2] = (addr + 128 * j) >> 16;
        TX_data[3] = (addr + 128 * j) >> 8;
        TX_data[4] = (addr + 128 * j);

        for(i = 0; i < 128; i++)
        {
            TX_data[5 + i] = pDataToWrite[j * 128 + i];
        }
        msleep(1);        // delay about 100us*****

        timeOutCount = 0;
        while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
        {
            timeOutCount++;
            if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
        }

        drvISP_WriteEnable();
        i2c_write_update_msg2133( TX_data, 133);   //write 133 byte per cycle
        i2c_write_update_msg2133(&bWriteData1, 1);
    }
}

static void _HalTscrHWReset ( void )//This function must implement by customer
{
    msg2133_reset();
}

static void drvDB_WriteReg ( unsigned char bank, unsigned char addr, unsigned short data )//New. Check
{
    unsigned char tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};
    MSG2133_DBG("hhh %s\n", __func__);
    i2c_write_msg2133 ( tx_data, 5 );
}

static void drvDB_WriteReg8Bit ( unsigned char bank, unsigned char addr, unsigned char data )//New. Check
{
    unsigned char tx_data[4] = {0x10, bank, addr, data};
    MSG2133_DBG("hhh %s\n", __func__);
    i2c_write_msg2133 ( tx_data, 4 );
}

static unsigned short drvDB_ReadReg ( unsigned char bank, unsigned char addr )//New. Check
{
    unsigned char tx_data[3] = {0x10, bank, addr};
    unsigned char rx_data[2] = {0};
    MSG2133_DBG("hhh %s\n", __func__);

    i2c_write_msg2133 ( tx_data, 3 );
    i2c_read_msg2133 ( &rx_data[0], 2 );
    return ( rx_data[1] << 8 | rx_data[0] );
}

void RegSet16BitValue(u16 nAddr, u16 nData)
{
    u8 tx_data[5] = {0x10, (nAddr >> 8) & 0xFF, nAddr & 0xFF, nData & 0xFF, nData >> 8};
    MSG2133_DBG("hhh %s\n", __func__);
    i2c_write_msg2133 ( tx_data, 5 );
    //IicWriteData(SLAVE_I2C_ID_DBBUS, &tx_data[0], 5);
}

void RegSetLByteValue(u16 nAddr, u8 nData)
{
    u8 tx_data[4] = {0x10, (nAddr >> 8) & 0xFF, nAddr & 0xFF, nData};
    MSG2133_DBG("hhh %s\n", __func__);
    i2c_write_msg2133 ( tx_data, 4 );
    //IicWriteData(SLAVE_I2C_ID_DBBUS, &tx_data[0], 4);
}

u16 RegGet16BitValue(u16 nAddr)
{
    u8 tx_data[3] = {0x10, (nAddr >> 8) & 0xFF, nAddr & 0xFF};
    u8 rx_data[2] = {0};

    MSG2133_DBG("hhh %s\n", __func__);

    i2c_write_msg2133 ( tx_data, 3 );
    i2c_read_msg2133 ( &rx_data[0], 2 );

    //IicWriteData(SLAVE_I2C_ID_DBBUS, &tx_data[0], 3);
    //IicReadData(SLAVE_I2C_ID_DBBUS, &rx_data[0], 2);

    return (rx_data[1] << 8 | rx_data[0]);
}

static unsigned int Reflect ( unsigned int ref, char ch )////New. Check
{
    unsigned int value = 0;
    unsigned int i = 0;
//    MSG2133_DBG("hhh %s\n", __func__);

    for ( i = 1; i < ( ch + 1 ); i++ )
    {
        if ( ref & 1 )
        {
            value |= 1 << ( ch - i );
        }
        ref >>= 1;
    }
    return value;
}

static void Init_CRC32_Table ( unsigned int *crc32_table )//New. Check
{
    unsigned int magicnumber = 0x04c11db7;
    unsigned int i = 0, j;
    MSG2133_DBG("hhh %s\n", __func__);

    for ( i = 0; i <= 0xFF; i++ )
    {
        crc32_table[i] = Reflect ( i, 8 ) << 24;
        for ( j = 0; j < 8; j++ )
        {
            crc32_table[i] = ( crc32_table[i] << 1 ) ^ ( crc32_table[i] & ( 0x80000000L ) ? magicnumber : 0 );
        }
        crc32_table[i] = Reflect ( crc32_table[i], 32 );
    }
}

unsigned int Get_CRC ( unsigned int text, unsigned int prevCRC, unsigned int *crc32_table )//New. Check
{
    unsigned int ulCRC = prevCRC;
//    MSG2133_DBG("hhh %s\n", __func__);
    {
        ulCRC = ( ulCRC >> 8 ) ^ crc32_table[ ( ulCRC & 0xFF ) ^ text];
    }
    return ulCRC ;
}

static ssize_t firmware_version_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)//Check
{
	u16 maj = 0, min = 0;
	unsigned char *bbuf = NULL;

	if (g_ChipType == CHIP_TYPE_UNKNOWN) {
		printk("[%s] TP chip type is UNKNOWN! Need to detect chip first.\n",__func__);
		return sprintf(buf,"Need to detect chip first!");
	}
	DrvFwCtrlGetCustomerFirmwareVersion(&maj,&min,&bbuf);
	DBG("FW maj=%hi min=%hi buf=%s\n",maj,min,bbuf)

    return sprintf(buf, "%d.%d\n",maj,min);
}

static ssize_t firmware_version_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t size)
{
	int i;
	MSG2133_DBG("hhh %s\n", __func__);


	unsigned char dbbus_tx_data[3];
	unsigned char dbbus_rx_data[5] ;
	
	MSG2133_DBG("%s\n", __func__);
/*	
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
    */
	MSG2133_DBG("\n");

	for (i = 0; i < 10; i++)
	{
	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x74;

	msg2133_i2c_write(&dbbus_tx_data[0], 3);
	mdelay(50);
	msg2133_i2c_read(&dbbus_rx_data[0], 5);
	MSG2133_DBG("dbbus_rx_data[0] = %x\n",dbbus_rx_data[0]);
	MSG2133_DBG("dbbus_rx_data[1] = %x\n",dbbus_rx_data[1]);
	MSG2133_DBG("dbbus_rx_data[2] = %x\n",dbbus_rx_data[2]);
	MSG2133_DBG("dbbus_rx_data[3] = %x\n",dbbus_rx_data[3]);
	MSG2133_DBG("dbbus_rx_data[4] = %x\n",dbbus_rx_data[4]);

	fw_v.major = (dbbus_rx_data[0] << 8) + dbbus_rx_data[1];
	fw_v.minor = (dbbus_rx_data[3] << 8) + dbbus_rx_data[2];
	fw_v.VenderID= (dbbus_rx_data[4]);
	MSG2133_DBG("fw_major = %x, fw_minor = %x\n",fw_v.major,fw_v.minor);
	if ((fw_v.major & 0xff00) == 0)
		break;
	
	}
	return size;
}


static ssize_t firmware_data_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)//Check
{
	MSG2133_DBG("tyd-tp: firmware_data_show\n");
	MSG2133_DBG("hhh %s\n", __func__);
    	return FwDataCnt;
}

static ssize_t firmware_data_store(struct device *dev,
                                   struct device_attribute *attr, const char *buf, size_t size)//Check
{
    	int i;
    	MSG2133_DBG("***FwDataCnt = %d ***\n", FwDataCnt);
		MSG2133_DBG("hhh %s\n", __func__);
	MSG2133_DBG("tyd-tp: firmware_data_store\n");
//    	for(i = 0; i < 1024; i++){
//        	memcpy(temp[FwDataCnt], buf, 1024);
//    	}

//    	FwDataCnt++;
    	return -ENODEV;
}

u8 DrvFwCtrlGetChipType(void)
{
    u8 nChipType = 0;

    DBG("*** %s() ***\n", __func__);

    //DrvPlatformLyrTouchDeviceResetHw();
    //disable_irq(SW_INT_IRQNO_PIO);
    _HalTscrHWReset();

    // Erase TP Flash first
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay(300);

    // Stop MCU
    RegSetLByteValue(0x0FE6, 0x01);

    // Disable watchdog
    RegSet16BitValue(0x3C60, 0xAA55);

    /////////////////////////
    // Difference between C2 and C3
    /////////////////////////
    // c2:MSG2133(1) c32:MSG2133A(2) c33:MSG2138A(2)
    // check ic type
    nChipType = RegGet16BitValue(0x1ECC) & 0xFF;

    if (nChipType != CHIP_TYPE_MSG21XX &&   // (0x01)
        nChipType != CHIP_TYPE_MSG21XXA &&  // (0x02)
        nChipType != CHIP_TYPE_MSG26XXM &&  // (0x03)
        nChipType != CHIP_TYPE_MSG22XX)     // (0x7A)
    {
        nChipType = 0;
    }

    DBG("*** Chip Type = 0x%x ***\n", nChipType);

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    _HalTscrHWReset();
    //DrvPlatformLyrTouchDeviceResetHw();
//  enable_irq(SW_INT_IRQNO_PIO);
    return nChipType;
}

void DrvFwCtrlGetPlatformFirmwareVersion(u8 **ppVersion)
{
    u32 i;
    u16 nRegData1, nRegData2;
    u8 szDbBusRxData[12] = {0};

    DBG("*** %s() ***\n", __func__);

    _HalTscrHWReset();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay(100);

    if (g_ChipType == CHIP_TYPE_MSG22XX) // Only MSG22XX support platform firmware version
    {
        // Stop mcu
        RegSetLByteValue(0x0FE6, 0x01);

        // Stop watchdog
        RegSet16BitValue(0x3C60, 0xAA55);

        // RIU password
        RegSet16BitValue(0x161A, 0xABBA);

        // Clear pce
        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x80));

        RegSet16BitValue(0x1600, 0xC1F2); // Set start address for platform firmware version on info block(Actually, start reading from 0xC1F0)

        // Enable burst mode
        RegSet16BitValue(0x160C, (RegGet16BitValue(0x160C) | 0x01));

        // Set pce
        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x40));

        for (i = 0; i < 3; i ++)
        {
            RegSetLByteValue(0x160E, 0x01);

            nRegData1 = RegGet16BitValue(0x1604);
            nRegData2 = RegGet16BitValue(0x1606);

            szDbBusRxData[i*4+0] = (nRegData1 & 0xFF);
            szDbBusRxData[i*4+1] = ((nRegData1 >> 8 ) & 0xFF);

//            DBG("szDbBusRxData[%d] = 0x%x , %c \n", i*4+0, szDbBusRxData[i*4+0], szDbBusRxData[i*4+0]); // add for debug
//            DBG("szDbBusRxData[%d] = 0x%x , %c \n", i*4+1, szDbBusRxData[i*4+1], szDbBusRxData[i*4+1]); // add for debug

            szDbBusRxData[i*4+2] = (nRegData2 & 0xFF);
            szDbBusRxData[i*4+3] = ((nRegData2 >> 8 ) & 0xFF);

//            DBG("szDbBusRxData[%d] = 0x%x , %c \n", i*4+2, szDbBusRxData[i*4+2], szDbBusRxData[i*4+2]); // add for debug
//            DBG("szDbBusRxData[%d] = 0x%x , %c \n", i*4+3, szDbBusRxData[i*4+3], szDbBusRxData[i*4+3]); // add for debug
        }

        // Clear burst mode
        RegSet16BitValue(0x160C, RegGet16BitValue(0x160C) & (~0x01));

        RegSet16BitValue(0x1600, 0x0000);

        // Clear RIU password
        RegSet16BitValue(0x161A, 0x0000);

        if (*ppVersion == NULL)
        {
            *ppVersion = kzalloc(sizeof(u8)*10, GFP_KERNEL);
        }

        sprintf(*ppVersion, "%c%c%c%c%c%c%c%c%c%c", szDbBusRxData[2], szDbBusRxData[3], szDbBusRxData[4],
            szDbBusRxData[5], szDbBusRxData[6], szDbBusRxData[7], szDbBusRxData[8], szDbBusRxData[9], szDbBusRxData[10], szDbBusRxData[11]);
    }
    else
    {
        if (*ppVersion == NULL)
        {
            *ppVersion = kzalloc(sizeof(u8)*10, GFP_KERNEL);
        }

        sprintf(*ppVersion, "%s", "N/A");
    }

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    _HalTscrHWReset();

    mdelay(100);

    DBG("*** platform firmware version = %s ***\n", *ppVersion);
}

void DrvFwCtrlGetCustomerFirmwareVersion(u16 *pMajor, u16 *pMinor, u8 **ppVersion)
{
    DBG("*** %s() ***\n", __func__);

    if (g_ChipType == CHIP_TYPE_MSG21XXA || g_ChipType == CHIP_TYPE_MSG21XX)
    {
        u8 szDbBusTxData[3] = {0};
        u8 szDbBusRxData[4] = {0};

        szDbBusTxData[0] = 0x53;
        szDbBusTxData[1] = 0x00;

//        if (g_ChipType == CHIP_TYPE_MSG21XXA)
//        {
//            szDbBusTxData[2] = 0x2A;
//        }
//        else if (g_ChipType == CHIP_TYPE_MSG21XX)
//        {
            szDbBusTxData[2] = 0x74;
//        }
//        else
//        {
//            szDbBusTxData[2] = 0x2A;
//        }
//        _HalTscrHWReset();
//     //DrvPlatformLyrTouchDeviceResetHw();
//        mdelay(100);

    	msg2133_i2c_write(&szDbBusTxData[0], 3);
    	mdelay(50);
    	msg2133_i2c_read(&szDbBusRxData[0], 4);

//        IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
//        IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);

        *pMajor = (szDbBusRxData[1]<<8) + szDbBusRxData[0];
        *pMinor = (szDbBusRxData[3]<<8) + szDbBusRxData[2];
    }
    else if (g_ChipType == CHIP_TYPE_MSG22XX)
    {
        u16 nRegData1, nRegData2;

        //DrvPlatformLyrTouchDeviceResetHw();
        _HalTscrHWReset();

        dbbusDWIICEnterSerialDebugMode();
        dbbusDWIICStopMCU();
        dbbusDWIICIICUseBus();
        dbbusDWIICIICReshape();
        mdelay(100);

        // Stop mcu
        RegSetLByteValue(0x0FE6, 0x01);

        // Stop watchdog
        RegSet16BitValue(0x3C60, 0xAA55);

        // RIU password
        RegSet16BitValue(0x161A, 0xABBA);

        // Clear pce
        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x80));

        RegSet16BitValue(0x1600, 0xBFF4); // Set start address for customer firmware version on main block

        // Enable burst mode
//        RegSet16BitValue(0x160C, (RegGet16BitValue(0x160C) | 0x01));

        // Set pce
        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x40));

        RegSetLByteValue(0x160E, 0x01);

        nRegData1 = RegGet16BitValue(0x1604);
        nRegData2 = RegGet16BitValue(0x1606);

        *pMajor = (((nRegData1 >> 8) & 0xFF) << 8) + (nRegData1 & 0xFF);
        *pMinor = (((nRegData2 >> 8) & 0xFF) << 8) + (nRegData2 & 0xFF);

        // Clear burst mode
//        RegSet16BitValue(0x160C, RegGet16BitValue(0x160C) & (~0x01));

        RegSet16BitValue(0x1600, 0x0000);

        // Clear RIU password
        RegSet16BitValue(0x161A, 0x0000);

        dbbusDWIICIICNotUseBus();
        dbbusDWIICNotStopMCU();
        dbbusDWIICExitSerialDebugMode();

        //DrvPlatformLyrTouchDeviceResetHw();
        _HalTscrHWReset();
        mdelay(100);

    }

    DBG("*** major = %d ***\n", *pMajor);
    DBG("*** minor = %d ***\n", *pMinor);

    if (*ppVersion == NULL)
    {
        *ppVersion = kzalloc(sizeof(u8)*6, GFP_KERNEL);
    }

    sprintf(*ppVersion, "%03d%03d", *pMajor, *pMinor);
}

static void _DrvFwCtrlStoreFirmwareData(u8 *pBuf, u32 nSize)
{
    u32 nCount = nSize / 1024;
    u32 i;

    DBG("*** %s() ***\n", __func__);

    if (nCount > 0) // nSize >= 1024
   	{
        for (i = 0; i < nCount; i ++)
        {
            memcpy(g_FwData[g_FwDataCount], pBuf+(i*1024), 1024);

            g_FwDataCount ++;
        }
    }
    else // nSize < 1024
    {
        if (nSize > 0)
        {
            memcpy(g_FwData[g_FwDataCount], pBuf, nSize);

            g_FwDataCount ++;
        }
    }

    DBG("*** g_FwDataCount = %d ***\n", g_FwDataCount);

    if (pBuf != NULL)
    {
        DBG("*** buf[0] = %c ***\n", pBuf[0]);
    }
}

static int detect_i2c_debug_bus_address(void);
static int firmware_update_c22 (EMEM_TYPE_t eEmemType);
static u32 _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_TYPE_t eEmemType);
static u32 _DrvFwCtrlMsg22xxRetrieveFrimwareCrcFromBinFile(u8 szTmpBuf[], EMEM_TYPE_t eEmemType);
static void _DrvFwCtrlMsg22xxConvertFwDataTwoDimenToOneDimen(u8 szTwoDimenFwData[][1024], u8* pOneDimenFwData);

static ssize_t debug_show(struct device *dev, struct device_attribute *attr, char *buf) {
    DBG("read finish\n");
    return sprintf(buf, "i2c_dbbus_addr=%x; g_ChipType=%x\nFW VER %03d.%03d\n ",i2c_dbbus_addr,g_ChipType,fw_v.major, fw_v.minor);
}

static ssize_t debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
	int t;
	char *bbuf = NULL;
	long int tt = 0;
	unsigned short maj,min;
	unsigned int crcm = 0, crci = 0, crcsm = 0, crcsi = 0;

	DBG("input size=%i; args=%s\n",size,buf);

	disable_irq(SW_INT_IRQNO_PIO);

	char c = buf[0];
	switch(c) {
	case 'c':
		DBG("Read chip ID\n");
		t = DrvFwCtrlGetChipType();
		g_ChipType = t;
		DBG("CHIP=%x\n",g_ChipType);
		break;
	case 'r':
		DBG("reset\n");
		_HalTscrHWReset();
		break;
	case 'a':
		kstrtol(&buf[2],0,&tt);
		i2c_dbbus_addr = (int)tt;
		DBG("i2c debug address was changed to %x\n",i2c_dbbus_addr);
		break;
	case 'p':
		DBG("Get platform fw version\n");
		DrvFwCtrlGetPlatformFirmwareVersion(&bbuf);
		DBG("PLATFORM FW VERSION=%s\n",bbuf);
		break;
	case 'v':
		DBG("Get customer firmware version\n");
		DrvFwCtrlGetCustomerFirmwareVersion(&maj,&min,&bbuf);
		DBG("FW maj=%hi min=%hi buf=%s\n",maj,min,bbuf);
		break;
	case 'd':
		detect_i2c_debug_bus_address();
		break;
	case 'u':
		if (buf[1] == 'i')
			firmware_update_c22(EMEM_INFO);
		if (buf[1] == 'm')
			firmware_update_c22(EMEM_MAIN);
		if (buf[1] == 'a')
			firmware_update_c22(EMEM_ALL);
		break;
	case 's':

		crcm = _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_MAIN);
		crci = _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_INFO);
		crcsm = _DrvFwCtrlMsg22xxRetrieveFrimwareCrcFromBinFile(_gOneDimenFwData,EMEM_MAIN);
		crcsi = _DrvFwCtrlMsg22xxRetrieveFrimwareCrcFromBinFile(_gOneDimenFwData,EMEM_INFO);
		DBG("HW Main block CRC = %x; Info block CRC = %x\nSW Main block CRC = %x; Info block CRC = %x\n",crcm,crci,crcsm,crcsi);

		break;

	case 'm':
		if (buf[1] == 'v')
			memcpy(_gOneDimenFwData,fw_buffer,FW_SIZE);
		else if (buf[1] == 'c') {
			_DrvFwCtrlStoreFirmwareData(fw_buffer,49664);
			_DrvFwCtrlMsg22xxConvertFwDataTwoDimenToOneDimen(g_FwData, _gOneDimenFwData);
		}/* else if (buf[1] == 's') {
			_DrvFwCtrlStoreFirmwareData(msg2xxx_xxxx_update_bin,49664);
			_DrvFwCtrlMsg22xxConvertFwDataTwoDimenToOneDimen(g_FwData, _gOneDimenFwData);
		}*/
		break;
	}

	enable_irq(SW_INT_IRQNO_PIO);
	DBG("OK\n");
	return size;
}

/*
 * Determine and set TP i2c debug bus. It is using for program tp mcu.
 */
static int detect_i2c_debug_bus_address(void) {
	int chip = 0;
	int i,retry;
	int query_address[] = { CHIP_TYPE_MSG22XX_DBI2C, CHIP_TYPE_MSG21XXA_DBI2C, 0, };

	int old_i2c_dbbus_add = i2c_dbbus_addr;

	for (i = 0; query_address[i]; i++) {
		i2c_dbbus_addr = query_address[i];
		retry = 3;
		while ((chip = DrvFwCtrlGetChipType()) == 0 && retry--);
		if (chip) break;
	}
	if (chip == 0) {
		DBG("TP chip has not found!\n");
		i2c_dbbus_addr = old_i2c_dbbus_add;
	} else {
		DBG("Found chip = %x; i2c_addr = %x\n", chip, i2c_dbbus_addr);
	}
	return i2c_dbbus_addr;
}

static ssize_t chipid_show(struct device *dev, struct device_attribute *attr, char *buf) {
	int addr = 0;
	u8 t = 0;
	addr = detect_i2c_debug_bus_address();
	if (addr == 0) {
		printk("[%s] error detect TP debug i2c address.\n",__func__);
		return sprintf(buf, "Error detection! ChipID=UNKNOWN\n");
	}

	t = DrvFwCtrlGetChipType();
	if (t == 0) {
		printk("[%s] error getting TP chip ID.\n",__func__);
		return sprintf(buf, "Error getting TP chip ID! ChipID=UNKNOWN\n");
	}
	g_ChipType = t;

    return sprintf(buf, "I2CDEBUGBUS=%x; ChipID=%x\n",i2c_dbbus_addr, g_ChipType);
}

static int drvTP_erase_emem_c33 ( EMEM_TYPE_t emem_type )//New, Check
{
    // stop mcu
    MSG2133_DBG("hhh %s\n", __func__);
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0001 );

    //disable watch dog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    // set PROGRAM password
    drvDB_WriteReg8Bit ( 0x16, 0x1A, 0xBA );
    drvDB_WriteReg8Bit ( 0x16, 0x1B, 0xAB );

    //proto.MstarWriteReg(F1.loopDevice, 0x1618, 0x80);
    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    if ( emem_type == EMEM_ALL )
    {
        drvDB_WriteReg8Bit ( 0x16, 0x08, 0x10 ); //mark
    }

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x40 );
    mdelay ( 10 );

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    // erase trigger
    if ( emem_type == EMEM_MAIN )
    {
        drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x04 ); //erase main
    }
    else
    {
        drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x08 ); //erase all block
    }

    return ( 1 );
}

static int drvTP_read_info_dwiic_c33 ( void )//New, Check
{
    unsigned char dwiic_tx_data[5];

    //drvDB_EnterDBBUS();
    MSG2133_DBG("hhh %s\n", __func__);
     _HalTscrHWReset();
     mdelay ( 300 );
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    // Stop Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    drvDB_WriteReg ( 0x3C, 0xE4, 0xA4AB );

    // TP SW reset
    drvDB_WriteReg ( 0x1E, 0x02, 0x829F );

    mdelay ( 50 );

    dwiic_tx_data[0] = 0x72;
    dwiic_tx_data[1] = 0x80;
    dwiic_tx_data[2] = 0x00;
    dwiic_tx_data[3] = 0x04;
    dwiic_tx_data[4] = 0x00;
    msg2133_i2c_write ( dwiic_tx_data, 5 );

    mdelay ( 50 );

    // recive info data
    msg2133_i2c_read ( &g_dwiic_info_data[0], 1024 );

    return ( 1 );
}

static int drvTP_info_updata_C33 ( unsigned short start_index, unsigned char *data, unsigned short size )//New, check
{
    // size != 0, start_index+size !> 1024
    unsigned short i;
    MSG2133_DBG("hhh %s\n", __func__);
    for ( i = 0;i < size; i++ )
    {
        g_dwiic_info_data[start_index] = * ( data + i );
        start_index++;
    }
    
    return ( 1 );
}

static void _DrvFwCtrlMsg22xxEraseEmem(EMEM_TYPE_t eEmemType)
{
    u32 i;
    u16 nRegData = 0;

    DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

    DBG("Erase start\n");

    // Stop mcu
    RegSet16BitValue(0x0FE6, 0x0001);

    // Disable watchdog
    RegSetLByteValue(0x3C60, 0x55);
    RegSetLByteValue(0x3C61, 0xAA);

    // Set PROGRAM password
    RegSetLByteValue(0x161A, 0xBA);
    RegSetLByteValue(0x161B, 0xAB);

    if (eEmemType == EMEM_ALL) // 48KB + 512Byte
    {
        DBG("Erase all block\n");

        // Clear pce
        RegSetLByteValue(0x1618, 0x80);
        mdelay(100);

        // Chip erase
        RegSet16BitValue(0x160E, 0x8);

        DBG("Wait erase done flag\n");

        do // Wait erase done flag
        {
            nRegData = RegGet16BitValue(0x1610); // Memory status
            mdelay(50);
        } while((nRegData & 0x2) != 0x2);
    }
    else if (eEmemType == EMEM_MAIN) // 48KB (32+8+8)
    {
        DBG("Erase main block\n");

        for (i = 0; i < 3; i ++)
        {
            // Clear pce
            RegSetLByteValue(0x1618, 0x80);
            mdelay(10);

            if (i == 0)
            {
                RegSet16BitValue(0x1600, 0x0000);
            }
            else if (i == 1)
            {
                RegSet16BitValue(0x1600, 0x8000);
            }
            else if (i == 2)
            {
                RegSet16BitValue(0x1600, 0xA000);
            }

            // Sector erase
            RegSet16BitValue(0x160E, (RegGet16BitValue(0x160E) | 0x4));

            DBG("Wait erase done flag\n");

            do // Wait erase done flag
            {
                nRegData = RegGet16BitValue(0x1610); // Memory status
                mdelay(50);
            } while((nRegData & 0x2) != 0x2);
        }
    }
    else if (eEmemType == EMEM_INFO) // 512Byte
    {
        DBG("Erase info block\n");

        // Clear pce
        RegSetLByteValue(0x1618, 0x80);
        mdelay(10);

        RegSet16BitValue(0x1600, 0xC000);

        // Sector erase
        RegSet16BitValue(0x160E, (RegGet16BitValue(0x160E) | 0x4));

        DBG("Wait erase done flag\n");

        do // Wait erase done flag
        {
            nRegData = RegGet16BitValue(0x1610); // Memory status
            mdelay(50);
        } while((nRegData & 0x2) != 0x2);
    }

    DBG("Erase end\n");

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

}

static void _DrvFwCtrlMsg22xxConvertFwDataTwoDimenToOneDimen(u8 szTwoDimenFwData[][1024], u8* pOneDimenFwData)
{
    u32 i, j;

    DBG("*** %s() ***\n", __func__);

    for (i = 0; i < (MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE+1); i ++)
    {
        if (i < MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE) // i < 48
        {
            for (j = 0; j < 1024; j ++)
            {
                pOneDimenFwData[i*1024+j] = szTwoDimenFwData[i][j];
            }
        }
        else // i == 48
        {
            for (j = 0; j < 512; j ++)
            {
                pOneDimenFwData[i*1024+j] = szTwoDimenFwData[i][j];
            }
        }
    }
}

static u32 _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_TYPE_t eEmemType) // For MSG22XX
{
    u16 nCrcDown = 0;
    u32 nRetVal = 0;

    DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay(100);

    // RIU password
    RegSet16BitValue(0x161A, 0xABBA);

    // Set PCE high
    RegSetLByteValue(0x1618, 0x40);

    if (eEmemType == EMEM_MAIN)
    {
        // Set start address and end address for main block
        RegSet16BitValue(0x1600, 0x0000);
        RegSet16BitValue(0x1640, 0xBFF8);
    }
    else if (eEmemType == EMEM_INFO)
    {
        // Set start address and end address for info block
        RegSet16BitValue(0x1600, 0xC000);
        RegSet16BitValue(0x1640, 0xC1F8);
    }

    // CRC reset
    RegSet16BitValue(0x164E, 0x0001);

    RegSet16BitValue(0x164E, 0x0000);

    // Trigger CRC check
    RegSetLByteValue(0x160E, 0x20);
    mdelay(10);

    nCrcDown = RegGet16BitValue(0x164E);

    while (nCrcDown != 2)
    {
        DBG("Wait CRC down\n");
        mdelay(10);
        nCrcDown = RegGet16BitValue(0x164E);
    }

    nRetVal = RegGet16BitValue(0x1652);
    nRetVal = (nRetVal << 16) | RegGet16BitValue(0x1650);

    DBG("Hardware CRC = 0x%x\n", nRetVal);

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    return nRetVal;
}

static s32 _DrvFwCtrlMsg22xxUpdateFirmware(/*u8 szFwData[][1024],*/ EMEM_TYPE_t eEmemType)
{
    u32 i, index;
    u32 nCrcMain, nCrcMainTp;
    u32 nCrcInfo, nCrcInfoTp;
    u32 nRemainSize, nBlockSize, nSize;
    u16 nRegData = 0;
    u8 szDbBusTxData[1024] = {0};
    u32 nSizePerWrite = 125;

    DBG("*** %s() ***\n", __func__);

    //_DrvFwCtrlMsg22xxConvertFwDataTwoDimenToOneDimen(szFwData, _gOneDimenFwData);

    _HalTscrHWReset();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

    DBG("Erase start\n");

    // Stop mcu
    RegSet16BitValue(0x0FE6, 0x0001);

    // Disable watchdog
    RegSetLByteValue(0x3C60, 0x55);
    RegSetLByteValue(0x3C61, 0xAA);

    // Set PROGRAM password
    RegSetLByteValue(0x161A, 0xBA);
    RegSetLByteValue(0x161B, 0xAB);

    if (eEmemType == EMEM_ALL) // 48KB + 512Byte
    {
        DBG("Erase all block\n");

        // Clear pce
        RegSetLByteValue(0x1618, 0x80);
        mdelay(100);

        // Chip erase
        RegSet16BitValue(0x160E, BIT3);

        DBG("Wait erase done flag\n");

        do // Wait erase done flag
        {
            nRegData = RegGet16BitValue(0x1610); // Memory status
            mdelay(50);
        } while((nRegData & BIT1) != BIT1);
    }
    else if (eEmemType == EMEM_MAIN) // 48KB (32+8+8)
    {
        DBG("Erase main block\n");

        for (i = 0; i < 3; i ++)
        {
            // Clear pce
            RegSetLByteValue(0x1618, 0x80);
            mdelay(10);

            if (i == 0)
            {
                RegSet16BitValue(0x1600, 0x0000);
            }
            else if (i == 1)
            {
                RegSet16BitValue(0x1600, 0x8000);
            }
            else if (i == 2)
            {
                RegSet16BitValue(0x1600, 0xA000);
            }

            // Sector erase
            RegSet16BitValue(0x160E, (RegGet16BitValue(0x160E) | BIT2));

            DBG("Wait erase done flag\n");

            do // Wait erase done flag
            {
                nRegData = RegGet16BitValue(0x1610); // Memory status
                mdelay(50);
            } while((nRegData & BIT1) != BIT1);
        }
    }
    else if (eEmemType == EMEM_INFO) // 512Byte
    {
        DBG("Erase info block\n");

        // Clear pce
        RegSetLByteValue(0x1618, 0x80);
        mdelay(10);

        RegSet16BitValue(0x1600, 0xC000);

        // Sector erase
        RegSet16BitValue(0x160E, (RegGet16BitValue(0x160E) | BIT2));

        DBG("Wait erase done flag\n");

        do // Wait erase done flag
        {
            nRegData = RegGet16BitValue(0x1610); // Memory status
            mdelay(50);
        } while((nRegData & BIT1) != BIT1);
    }

    DBG("Erase end\n");

    // Hold reset pin before program
    RegSetLByteValue(0x1E06, 0x00);

    /////////////////////////
    // Program
    /////////////////////////

    if (eEmemType == EMEM_ALL || eEmemType == EMEM_MAIN) // 48KB
    {
        DBG("Program main block start\n");

        // Program main block
        RegSet16BitValue(0x161A, 0xABBA);
        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x80));

        RegSet16BitValue(0x1600, 0x0000); // Set start address of main block
        RegSet16BitValue(0x160C, (RegGet16BitValue(0x160C) | 0x01)); // Enable burst mode

        // Program start
        szDbBusTxData[0] = 0x10;
        szDbBusTxData[1] = 0x16;
        szDbBusTxData[2] = 0x02;

        //IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3);
        i2c_write_msg2133(&szDbBusTxData[0], 3);

        szDbBusTxData[0] = 0x20;

        i2c_write_msg2133(&szDbBusTxData[0], 1);
        //IicWriteData(&szDbBusTxData[0], 3);

        nRemainSize = MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE * 1024; //48KB
        index = 0;

        while (nRemainSize > 0)
        {
            if (nRemainSize > nSizePerWrite)
            {
                nBlockSize = nSizePerWrite;
            }
            else
            {
                nBlockSize = nRemainSize;
            }

            szDbBusTxData[0] = 0x10;
            szDbBusTxData[1] = 0x16;
            szDbBusTxData[2] = 0x02;

            nSize = 3;

            for (i = 0; i < nBlockSize; i ++)
            {
                szDbBusTxData[3+i] = _gOneDimenFwData[index*nSizePerWrite+i];
                nSize ++;
            }
            index ++;
            i2c_write_msg2133(&szDbBusTxData[0], nSize);
            //IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], nSize);

            nRemainSize = nRemainSize - nBlockSize;
        }

        // Program end
        szDbBusTxData[0] = 0x21;
        i2c_write_msg2133(&szDbBusTxData[0], 1);
        //IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 1);

        nRegData = RegGet16BitValue(0x160C);
        RegSet16BitValue(0x160C, nRegData & (~0x01));

        DBG("Wait main block write done flag\n");

        // Polling 0x1610 is 0x0002
        do
        {
            nRegData = RegGet16BitValue(0x1610);
            nRegData = nRegData & BIT1;
            mdelay(10);

        } while (nRegData != BIT1); // Wait write done flag

        DBG("Program main block end\n");
    }

    if (eEmemType == EMEM_ALL || eEmemType == EMEM_INFO) // 512 Byte
    {
        DBG("Program info block start\n");

        // Program info block
        RegSet16BitValue(0x161A, 0xABBA);
        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x80));

        RegSet16BitValue(0x1600, 0xC000); // Set start address of info block
        RegSet16BitValue(0x160C, (RegGet16BitValue(0x160C) | 0x01)); // Enable burst mode

        // Program start
        szDbBusTxData[0] = 0x10;
        szDbBusTxData[1] = 0x16;
        szDbBusTxData[2] = 0x02;
        i2c_write_msg2133(&szDbBusTxData[0], 3);
        //IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3);

        szDbBusTxData[0] = 0x20;
        i2c_write_msg2133(&szDbBusTxData[0], 1);
        //IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 1);

        nRemainSize = MSG22XX_FIRMWARE_INFO_BLOCK_SIZE; //512Byte
        index = 0;

        while (nRemainSize > 0)
        {
            if (nRemainSize > nSizePerWrite)
            {
                nBlockSize = nSizePerWrite;
            }
            else
            {
                nBlockSize = nRemainSize;
            }

            szDbBusTxData[0] = 0x10;
            szDbBusTxData[1] = 0x16;
            szDbBusTxData[2] = 0x02;

            nSize = 3;

            for (i = 0; i < nBlockSize; i ++)
            {
                szDbBusTxData[3+i] = _gOneDimenFwData[(MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE*1024)+(index*nSizePerWrite)+i];
                nSize ++;
            }
            index ++;
            i2c_write_msg2133(&szDbBusTxData[0], nSize);
            //IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], nSize);

            nRemainSize = nRemainSize - nBlockSize;
        }

        // Program end
        szDbBusTxData[0] = 0x21;
        i2c_write_msg2133(&szDbBusTxData[0], 1);
        //IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 1);

        nRegData = RegGet16BitValue(0x160C);
        RegSet16BitValue(0x160C, nRegData & (~0x01));

        DBG("Wait info block write done flag\n");

        // Polling 0x1610 is 0x0002
        do
        {
            nRegData = RegGet16BitValue(0x1610);
            nRegData = nRegData & BIT1;
            mdelay(10);

        } while (nRegData != BIT1); // Wait write done flag

        DBG("Program info block end\n");
    }

    if (eEmemType == EMEM_ALL || eEmemType == EMEM_MAIN)
    {
        // Get CRC 32 from updated firmware bin file
        nCrcMain  = _gOneDimenFwData[0xBFFF] << 24;
        nCrcMain |= _gOneDimenFwData[0xBFFE] << 16;
        nCrcMain |= _gOneDimenFwData[0xBFFD] << 8;
        nCrcMain |= _gOneDimenFwData[0xBFFC];

        // CRC Main from TP
        DBG("Get Main CRC from TP\n");

        nCrcMainTp = _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_MAIN);

        DBG("nCrcMain=0x%x, nCrcMainTp=0x%x\n", nCrcMain, nCrcMainTp);
    }

    if (eEmemType == EMEM_ALL || eEmemType == EMEM_INFO)
    {
        nCrcInfo  = _gOneDimenFwData[0xC1FF] << 24;
        nCrcInfo |= _gOneDimenFwData[0xC1FE] << 16;
        nCrcInfo |= _gOneDimenFwData[0xC1FD] << 8;
        nCrcInfo |= _gOneDimenFwData[0xC1FC];

        // CRC Info from TP
        DBG("Get Info CRC from TP\n");

        nCrcInfoTp = _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_INFO);

        DBG("nCrcInfo=0x%x, nCrcInfoTp=0x%x\n", nCrcInfo, nCrcInfoTp);
    }

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    _HalTscrHWReset();

    if (eEmemType == EMEM_ALL)
    {
        if ((nCrcMainTp != nCrcMain) || (nCrcInfoTp != nCrcInfo))
        {
            DBG("Update FAILED\n");

            return -1;
        }
    }
    else if (eEmemType == EMEM_MAIN)
    {
        if (nCrcMainTp != nCrcMain)
        {
            DBG("Update FAILED\n");

            return -1;
        }
    }
    else if (eEmemType == EMEM_INFO)
    {
        if (nCrcInfoTp != nCrcInfo)
        {
            DBG("Update FAILED\n");

            return -1;
        }
    }

    DBG("Update SUCCESS\n");

    return 0;
}

static u32 _DrvFwCtrlMsg22xxRetrieveFrimwareCrcFromBinFile(u8 szTmpBuf[], EMEM_TYPE_t eEmemType) // For MSG22XX
{
    u32 nRetVal = 0;

    DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

    if (szTmpBuf != NULL)
    {
        if (eEmemType == EMEM_MAIN) // Read main block CRC(48KB-4) from bin file
        {
            nRetVal  = szTmpBuf[0xBFFF] << 24;
            nRetVal |= szTmpBuf[0xBFFE] << 16;
            nRetVal |= szTmpBuf[0xBFFD] << 8;
            nRetVal |= szTmpBuf[0xBFFC];
        }
        else if (eEmemType == EMEM_INFO) // Read info block CRC(512Byte-4) from bin file
        {
            nRetVal  = szTmpBuf[0xC1FF] << 24;
            nRetVal |= szTmpBuf[0xC1FE] << 16;
            nRetVal |= szTmpBuf[0xC1FD] << 8;
            nRetVal |= szTmpBuf[0xC1FC];
        }
    }

    return nRetVal;
}


static u16 _DrvFwCtrlMsg22xxGetSwId(EMEM_TYPE_t eEmemType) // For MSG22XX
{
    u16 nRetVal = 0;
    u16 nRegData1 = 0;

    DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay(100);

    // Stop mcu
    RegSetLByteValue(0x0FE6, 0x01);

    // Stop watchdog
    RegSet16BitValue(0x3C60, 0xAA55);

    // RIU password
    RegSet16BitValue(0x161A, 0xABBA);

    // Clear pce
    RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x80));

    if (eEmemType == EMEM_MAIN) // Read SW ID from main block
    {
        RegSet16BitValue(0x1600, 0xBFF4); // Set start address for main block SW ID
    }
    else if (eEmemType == EMEM_INFO) // Read SW ID from info block
    {
        RegSet16BitValue(0x1600, 0xC1EC); // Set start address for info block SW ID
    }

    /*
      Ex. SW ID in Main Block :
          Major low byte at address 0xBFF4
          Major high byte at address 0xBFF5

          SW ID in Info Block :
          Major low byte at address 0xC1EC
          Major high byte at address 0xC1ED
    */

    // Enable burst mode
//    RegSet16BitValue(0x160C, (RegGet16BitValue(0x160C) | 0x01));

    // Set pce
    RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x40));

    RegSetLByteValue(0x160E, 0x01);

    nRegData1 = RegGet16BitValue(0x1604);
//    nRegData2 = RegGet16BitValue(0x1606);

    nRetVal = ((nRegData1 >> 8) & 0xFF) << 8;
    nRetVal |= (nRegData1 & 0xFF);

    // Clear burst mode
//    RegSet16BitValue(0x160C, RegGet16BitValue(0x160C) & (~0x01));

    RegSet16BitValue(0x1600, 0x0000);

    // Clear RIU password
    RegSet16BitValue(0x161A, 0x0000);

    DBG("SW ID = 0x%x\n", nRetVal);

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    return nRetVal;
}

static int firmware_update_c22 (EMEM_TYPE_t eEmemType) { // For MSG22XX

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);
	unsigned int crcswi1 = 0, crcswm1 = 0;
	unsigned int crchwi1 = 0, crchwi2 = 0, crchwm1 = 0, crchwm2 = 0;

	int ret = 0;

	crcswi1 = _DrvFwCtrlMsg22xxRetrieveFrimwareCrcFromBinFile(_gOneDimenFwData,EMEM_INFO);
	crcswm1 = _DrvFwCtrlMsg22xxRetrieveFrimwareCrcFromBinFile(_gOneDimenFwData,EMEM_MAIN);

	crchwi1 = _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_INFO);
	crchwm1 = _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_MAIN);

	DBG("[%s] Before CRCSW info = %x; CRCSW main = %x; CRCHW info = %x; CRCHW main = %x\n",__func__,crcswi1,crcswm1,crchwi1,crchwm1);
	ret = _DrvFwCtrlMsg22xxUpdateFirmware(/*fw_buffer,*/eEmemType);
	crchwi2 = _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_INFO);
	crchwm2 = _DrvFwCtrlMsg22xxGetFirmwareCrcByHardware(EMEM_MAIN);

	DBG("[%s] \nCRCSW info = %x; CRCSW main = %x\n "
			"OLD CRCHW info = %x; CRCHW main = %x\n"
			"NEW CRCHW info = %x; CRCHW main = %x\n",__func__,crcswi1,crcswm1,crchwi1,crchwm1,crchwi2,crchwm2);
	return ret;
}

static int firmware_update_c33 (EMEM_TYPE_t emem_type )//New, check
{
//    unsigned char dbbus_tx_data[4];
//    unsigned char  dbbus_rx_data[2] = {0};
    unsigned char  life_counter[2];
    unsigned int i, j;
    unsigned int crc_main, crc_main_tp;
    unsigned int crc_info, crc_info_tp;
    unsigned int crc_tab[256];

    int update_pass = 1;
    unsigned short reg_data = 0;
	MSG2133_DBG("hhh %s\n", __func__);

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;

    drvTP_read_info_dwiic_c33();

    if ( g_dwiic_info_data[0] == 'M' && g_dwiic_info_data[1] == 'S' && g_dwiic_info_data[2] == 'T' && g_dwiic_info_data[3] == 'A' && g_dwiic_info_data[4] == 'R' && g_dwiic_info_data[5] == 'T' && g_dwiic_info_data[6] == 'P' && g_dwiic_info_data[7] == 'C' )
    {
        // updata FW Version
        pr_info("%s: read info\n", __func__);
        //drvTP_info_updata_C33 ( 8, &temp[32][8], 4 );

        // updata life counter
        life_counter[0] = ( ( g_dwiic_info_data[12] << 8 + g_dwiic_info_data[13] + 1 ) >> 8 ) & 0xFF;
        life_counter[1] = ( g_dwiic_info_data[12] << 8 + g_dwiic_info_data[13] + 1 ) & 0xFF;
        drvTP_info_updata_C33 ( 10, life_counter, 2 );

        drvDB_WriteReg ( 0x3C, 0xE4, 0x78C5 );

        // TP SW reset
        drvDB_WriteReg ( 0x1E, 0x02, 0x829F );

        mdelay ( 50 );

        //polling 0x3CE4 is 0x2F43
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x2F43 );

        // transmit lk info data
        msg2133_i2c_write ( g_dwiic_info_data, 1024 );

        //polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );

    }


    //erase main
    drvTP_erase_emem_c33 ( EMEM_MAIN );
    mdelay ( 1000 );

    //ResetSlave();
    _HalTscrHWReset();

    //drvDB_EnterDBBUS();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    /////////////////////////
    // Program
    /////////////////////////

    //polling 0x3CE4 is 0x1C70
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x1C70 );
    }

    switch ( emem_type )
    {
        case EMEM_ALL:
            drvDB_WriteReg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks
            break;
        case EMEM_MAIN:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for main block
            break;
        case EMEM_INFO:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for info block


            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x01 );

            drvDB_WriteReg8Bit ( 0x3C, 0xE4, 0xC5 ); //
            drvDB_WriteReg8Bit ( 0x3C, 0xE5, 0x78 ); //

            drvDB_WriteReg8Bit ( 0x1E, 0x04, 0x9F );
            drvDB_WriteReg8Bit ( 0x1E, 0x05, 0x82 );
            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x00 );
            mdelay ( 100 );
            break;
    }

    // polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );


    // calculate CRC 32
    Init_CRC32_Table ( &crc_tab[0] );


    for ( i = 0; i < 33; i++ ) // total  33 KB : 2 byte per R/W
    {
         if( emem_type == EMEM_INFO ) i = 32;

        if ( i < 32 )   //emem_main
        {
            if ( i == 31 )
            {
//                temp[i][1014] = 0x5A; //Fmr_Loader[1014]=0x5A;
//                temp[i][1015] = 0xA5; //Fmr_Loader[1015]=0xA5;
                fw_buffer[i * 1024 + 1014] = 0x5A;
                fw_buffer[i * 1024 + 1015] = 0xA5;

                for ( j = 0; j < 1016; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( fw_buffer[i * 1024 + j], crc_main, &crc_tab[0] );
                }
            }
            else
            {
                for ( j = 0; j < 1024; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( fw_buffer[i * 1024 + j], crc_main, &crc_tab[0] );
                }
            }
        }
        else  //emem_info
        {
            for ( j = 0; j < 1024; j++ )
            {
                //crc_info=Get_CRC(Fmr_Loader[j],crc_info,&crc_tab[0]);
                crc_info = Get_CRC ( g_dwiic_info_data[j], crc_info, &crc_tab[0] );
            }
            if ( emem_type == EMEM_MAIN ) break;
        }

        //drvDWIIC_MasterTransmit( DWIIC_MODE_DWIIC_ID, 1024, Fmr_Loader );
        msg2133_i2c_write ( &fw_buffer[i * 1024], 1024 );

        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );

        drvDB_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // write file done and check crc
        drvDB_WriteReg ( 0x3C, 0xE4, 0x1380 );
    }

    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // polling 0x3CE4 is 0x9432
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x9432 );
    }

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // CRC Main from TP
        crc_main_tp = drvDB_ReadReg ( 0x3C, 0x80 );
        crc_main_tp = ( crc_main_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );
        MSG2133_DBG ( "crc_main=0x%x, crc_main_tp=0x%x\n",
                   crc_main, crc_main_tp );
    }

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_INFO ) )
    {
        // CRC Info from TP
        crc_info_tp = drvDB_ReadReg ( 0x3C, 0xA0 );
        crc_info_tp = ( crc_info_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0xA2 );
        MSG2133_DBG ( "crc_info=0x%x, crc_info_tp=0x%x\n",
                   crc_info, crc_info_tp );
    }

    //drvDB_ExitDBBUS();

    update_pass = 1;
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        if ( crc_main_tp != crc_main )
            update_pass = 0;
    }

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_INFO ) )
    {
        if ( crc_info_tp != crc_info )
            update_pass = 0;
    }

    if ( !update_pass )
        MSG2133_DBG ( "update FAILED\n" );
    else
        MSG2133_DBG ( "update OK\n" );
    FwDataCnt = 0;
    _HalTscrHWReset();

    return update_pass;
}

static ssize_t firmware_update_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
	MSG2133_DBG("tyd-tp: firmware_update_show\n");
	MSG2133_DBG("hhh %s\n", __func__);
	return sprintf(buf, "%03d%03d\n", fw_v.major, fw_v.minor);
}


static int do_update_firmwareMSG21XXA(void) {
    unsigned char dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};
    int r;

	MSG2133_DBG("hhh %s\n", __func__);

    _HalTscrHWReset();

    // Erase TP Flash first
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    i2c_write_msg2133 ( dbbus_tx_data, 4 );
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    i2c_write_msg2133 ( dbbus_tx_data, 4 );

    // Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    i2c_write_msg2133 ( dbbus_tx_data, 4 );


    /////////////////////////
    // Difference between C2 and C3
    /////////////////////////

    //check id
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0xCC;
    i2c_write_msg2133 ( dbbus_tx_data, 3 );
    i2c_read_msg2133 ( &dbbus_rx_data[0], 2 );
    MSG2133_DBG ( "dbbus_rx id[0]=0x%x", dbbus_rx_data[0] );

    if ( dbbus_rx_data[0] == 2 )
    {
        // check version
        dbbus_tx_data[0] = 0x10;
        dbbus_tx_data[1] = 0x3C;
        dbbus_tx_data[2] = 0xEA;
        i2c_write_msg2133 ( dbbus_tx_data, 3 );
        i2c_read_msg2133 ( &dbbus_rx_data[0], 2 );
        MSG2133_DBG ( "dbbus_rx version[0]=0x%x", dbbus_rx_data[0] );

        if ( dbbus_rx_data[0] == 3 )
        {
            r = firmware_update_c33 ( EMEM_MAIN );
            _HalTscrHWReset();
            return r ? 1 : -EIO;
        }
        else
        {
            //return firmware_update_c32 ( dev, attr, buf, size, EMEM_ALL );
            pr_err("%s: unsupported version (c32)\n", __func__);
            _HalTscrHWReset();
            return -ENODEV;
        }
    }
    else
    {
        //return firmware_update_c2 ( dev, attr, buf, size );
        pr_err("%s: unsupported version (c2)\n", __func__);
        _HalTscrHWReset();
        return -ENODEV;
    }

}

static ssize_t firmware_update_store ( struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t size )//New, check
{
    int r;
    r = do_update_firmwareMSG21XXA();
    return (r < 0) ? r : size;
}

static DEVICE_ATTR(version, 0444, firmware_version_show, NULL);
static DEVICE_ATTR(debug, 0777, debug_show, debug_store);
static DEVICE_ATTR(detect_chip, 0444, chipid_show,NULL);

static DEVICE_ATTR(update, 0777, firmware_update_show, firmware_update_store);
static DEVICE_ATTR(data, 0777, firmware_data_show, firmware_data_store);


//    ssize_t (*write)(struct file *,struct kobject *, struct bin_attribute *,
//             char *, loff_t, size_t);


ssize_t firmware_updatefw(struct file *filp, struct kobject *kobj,
         struct bin_attribute *bin_attr,
         char *buf, loff_t off, size_t count) {
    // store firmware in buffer
    if (off >= FW_SIZE)
        return -ENOMEM;
    if ((off + count) > FW_SIZE) {
        count = FW_SIZE - off;
    }
    memcpy(fw_buffer + off, buf, count);
    printk("%s: pos %lld(%#llx) size %d(%#x)\n", __func__, off, off, count, count);

    return count; // consume all
}

static struct bin_attribute bin_attr_updatefw = {
    .attr = {.name = "updatefw", .mode = 0200 },
    .read   = NULL,
    .write  = firmware_updatefw,
};

ssize_t firmware_startupdate(struct file *filp, struct kobject *kobj,
         struct bin_attribute *bin_attr,
         char *buf, loff_t off, size_t count) {
    int r;
    struct CNT_TS_DATA_T *cnt_ts = (struct CNT_TS_DATA_T *)bin_attr->private;
    // store firmware in buffer
    if (off != 0)
        return -ENOMEM;

	if (g_ChipType == CHIP_TYPE_UNKNOWN) {
		printk("[%s] TP chip type is UNKNOWN! Need to detect chip first.\n",__func__);
		return sprintf(buf,"Need to detect chip first!");
	}

    // do update
    if (cnt_ts)
        cnt_ts->in_suspend |= LOCK_MODE;

    switch (g_ChipType) {
    case CHIP_TYPE_MSG21XXA:
    	r = do_update_firmwareMSG21XXA();
    	break;
    case CHIP_TYPE_MSG22XX:
    	memcpy(_gOneDimenFwData,fw_buffer,FW_SIZE);
    	r = firmware_update_c22(EMEM_ALL);
    	break;
    }

    if (cnt_ts)
        cnt_ts->in_suspend &= ~LOCK_MODE;
    return (r < 0) ? r : count;
}

static struct bin_attribute bin_attr_startupdate = {
    .attr = {.name = "startupdate", .mode = 0200 },
    .read   = NULL,
    .write  = firmware_startupdate,
};

static void msg2133_init_fw_class(struct CNT_TS_DATA_T *cnt_ts)
{
	firmware_class = class_create(THIS_MODULE,"mtk-tpd" );//client->name
	pr_err("hhh%s\n", __func__);

	if(IS_ERR(firmware_class))
		pr_err("Failed to create class(firmware)!\n");

	firmware_cmd_dev = device_create(firmware_class,
	                                     NULL, 0, NULL, "device");//device

	if(IS_ERR(firmware_cmd_dev))
		pr_err("Failed to create device(firmware_cmd_dev)!\n");
		
	// version /sys/class/mtk-tpd/device/version
	if(device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_version.attr.name);

	// update /sys/class/mtk-tpd/device/update
//	if(device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
//		pr_err("Failed to create device file(%s)!\n", dev_attr_update.attr.name);

	// data /sys/class/mtk-tpd/device/data
//	if(device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
//		pr_err("Failed to create device file(%s)!\n", dev_attr_data.attr.name);

//	 clear /sys/class/mtk-tpd/device/clear
	if(device_create_file(firmware_cmd_dev, &dev_attr_debug) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_debug.attr.name);

	if(device_create_file(firmware_cmd_dev, &dev_attr_detect_chip) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_detect_chip.attr.name);

    bin_attr_updatefw.private = cnt_ts;
    if(device_create_bin_file(firmware_cmd_dev, &bin_attr_updatefw) < 0)
        pr_err("Failed to create device file(%s)!\n", bin_attr_updatefw.attr.name);

    bin_attr_startupdate.private = cnt_ts;
    if(device_create_bin_file(firmware_cmd_dev, &bin_attr_startupdate) < 0)
        pr_err("Failed to create device file(%s)!\n", bin_attr_updatefw.attr.name);
}

#endif /* MSG2133_UPDATE */


/* Addresses to scan */
static union{
	unsigned short dirty_addr_buf[2];
	const unsigned short normal_i2c[2];
}u_i2c_addr = {{0x00},};
static __u32 twi_id = 0;

//use sw gpio interrupt trigger

/*
 * ctp_get_pendown_state  : get the int_line data state,
 *
 * return value:
 *             return PRESS_DOWN: if down
 *             return FREE_UP: if up,
 *             return 0: do not need process, equal free up.
 */
static int ctp_get_pendown_state(void) {

	//not supported
	return 0;
}

/**
 * ctp_clear_penirq - clear int pending
 *
 */
static void ctp_clear_penirq(void) {

	sw_gpio_eint_clr_irqpd_sta(gpio_int_hdle);
	return;
}

/**
 * ctp_set_irq_mode - according sysconfig's subkey "ctp_int_port" to config int port.
 *
 * return value:
 *              0:      success;
 *              others: fail;
 */
static int ctp_set_irq_mode(char *major_key , char *subkey, ext_int_mode int_mode) {

	pr_info("%s: config gpio to int mode. \n", __func__);

	if(gpio_int_hdle){
		sw_gpio_irq_free(gpio_int_hdle);
	}

	gpio_int_hdle = sw_gpio_irq_request(major_key,subkey,int_mode);

	return gpio_int_hdle ? 0 : -EIO;
}

/**
 * ctp_set_gpio_mode - according sysconfig's subkey "ctp_io_port" to config io port.
 *
 * return value:
 *              0:      success;
 *              others: fail;
 */
static int ctp_set_gpio_mode(void) {

	//not supported
	return -1;
}

/**
 * ctp_judge_int_occur - whether interrupt occur.
 *
 * return value:
 *              0:      int occur;
 *              others: no int occur;
 */
static int ctp_judge_int_occur(void) {

	return (sw_gpio_eint_get_irqpd_sta(gpio_int_hdle) == 1) ? 0 : 1;
}

/**
 * ctp_free_platform_resource - corresponding with ctp_init_platform_resource
 *
 */
static void ctp_free_platform_resource(void)
{
	if(gpio_addr){
		iounmap(gpio_addr);
	}
	
	if(gpio_int_hdle){
		gpio_release(gpio_int_hdle, 2);
	}
	
	if(gpio_wakeup_hdle){
		gpio_release(gpio_wakeup_hdle, 2);
	}
	
	if(gpio_reset_hdle){
		gpio_release(gpio_reset_hdle, 2);
	}

	if(gpio_power_hdle){
		gpio_release(gpio_power_hdle, 2);
	}

	return;
}


/**
 * ctp_init_platform_resource - initialize platform related resource
 * return value: 0 : success
 *               -EIO :  i/o err.
 *
 */
static int ctp_init_platform_resource(void)
{
	int ret = 0;

	gpio_addr = ioremap(PIO_BASE_ADDRESS, PIO_RANGE_SIZE);
	//pr_info("%s, gpio_addr = 0x%x. \n", __func__, gpio_addr);
	if(!gpio_addr) {
		ret = -EIO;
		goto exit_ioremap_failed;	
	}
	//    gpio_wakeup_enable = 1;
	gpio_wakeup_hdle = gpio_request_ex("ctp_para", "ctp_wakeup");
	if(!gpio_wakeup_hdle) {
		pr_warning("%s: tp_wakeup request gpio fail!\n", __func__);
		gpio_wakeup_enable = 0;
	}

	gpio_reset_hdle = gpio_request_ex("ctp_para", "ctp_reset");
	if(!gpio_reset_hdle) {
		pr_warning("%s: tp_reset request gpio fail!\n", __func__);
		gpio_reset_enable = 0;
	}

	gpio_power_hdle = gpio_request_ex("ctp_para", "ctp_powerpin");
	if(!gpio_reset_hdle) {
		pr_warning("%s: tp_power request gpio fail!\n", __func__);
		gpio_reset_enable = 0;
	}

	return ret;

exit_ioremap_failed:
	ctp_free_platform_resource();
	return ret;
}


/**
 * ctp_fetch_sysconfig_para - get config info from sysconfig.fex file.
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
static int ctp_fetch_sysconfig_para(void)
{
	int ret = -1;
	int ctp_used = -1;
	char name[I2C_NAME_SIZE];
	__u32 twi_addr = 0;
	//__u32 twi_id = 0;
	script_parser_value_type_t type = SCIRPT_PARSER_VALUE_TYPE_STRING;

	pr_info("%s. \n", __func__);

	if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_used", &ctp_used, 1)){
		pr_err("%s: script_parser_fetch err. \n", __func__);
		goto script_parser_fetch_err;
	}
	if(1 != ctp_used){
		pr_err("%s: ctp_unused. \n",  __func__);
		//ret = 1;
		return ret;
	}

	if(SCRIPT_PARSER_OK != script_parser_fetch_ex("ctp_para", "ctp_name", (int *)(&name), &type, sizeof(name)/sizeof(int))){
		pr_err("%s: script_parser_fetch err. \n", __func__);
		goto script_parser_fetch_err;
	}
	if(strcmp(CTP_NAME, name) && ! force){
		pr_err("%s: name %s does not match CTP_NAME. \n", __func__, name);
		pr_err(CTP_NAME);
		//ret = 1;
		return ret;
	}

	if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_twi_addr", &twi_addr, sizeof(twi_addr)/sizeof(__u32))){
		pr_err("%s: script_parser_fetch err. \n", name);
		goto script_parser_fetch_err;
	}
	//big-endian or small-endian?
	//pr_info("%s: before: ctp_twi_addr is 0x%x, dirty_addr_buf: 0x%hx. dirty_addr_buf[1]: 0x%hx \n", __func__, twi_addr, u_i2c_addr.dirty_addr_buf[0], u_i2c_addr.dirty_addr_buf[1]);
	u_i2c_addr.dirty_addr_buf[0] = twi_addr;
	u_i2c_addr.dirty_addr_buf[1] = I2C_CLIENT_END;
	pr_info("%s: after: ctp_twi_addr is 0x%x, dirty_addr_buf: 0x%hx. dirty_addr_buf[1]: 0x%hx \n", __func__, twi_addr, u_i2c_addr.dirty_addr_buf[0], u_i2c_addr.dirty_addr_buf[1]);
	//pr_info("%s: after: ctp_twi_addr is 0x%x, u32_dirty_addr_buf: 0x%hx. u32_dirty_addr_buf[1]: 0x%hx \n", __func__, twi_addr, u32_dirty_addr_buf[0],u32_dirty_addr_buf[1]);

	if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_twi_id", &twi_id, sizeof(twi_id)/sizeof(__u32))){
		pr_err("%s: script_parser_fetch err. \n", name);
		goto script_parser_fetch_err;
	}
	pr_info("%s: ctp_twi_id is %d. \n", __func__, twi_id);
	
	if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_screen_max_x", &screen_max_x, 1)){
		pr_err("%s: script_parser_fetch err. \n", __func__);
		goto script_parser_fetch_err;
	}
	pr_info("%s: screen_max_x = %d. \n", __func__, screen_max_x);

	if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_screen_max_y", &screen_max_y, 1)){
		pr_err("%s: script_parser_fetch err. \n", __func__);
		goto script_parser_fetch_err;
	}
	pr_info("%s: screen_max_y = %d. \n", __func__, screen_max_y);

	if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_revert_x_flag", &revert_x_flag, 1)){
		pr_err("%s: script_parser_fetch err. \n", __func__);
		goto script_parser_fetch_err;
	}
	pr_info("%s: revert_x_flag = %d. \n", __func__, revert_x_flag);

	if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_revert_y_flag", &revert_y_flag, 1)){
		pr_err("%s: script_parser_fetch err. \n", __func__);
		goto script_parser_fetch_err;
	}
	pr_info("%s: revert_y_flag = %d. \n", __func__, revert_y_flag);

	if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_exchange_x_y_flag", &exchange_x_y_flag, 1)){
		pr_err("cnt_ts: script_parser_fetch err. \n");
		goto script_parser_fetch_err;
	}
	pr_info("%s: exchange_x_y_flag = %d. \n", __func__, exchange_x_y_flag);

	return 0;

script_parser_fetch_err:
	pr_notice("=========script_parser_fetch_err============\n");
	return ret;
}

/**
 * ctp_reset - function
 *
 */
static void ctp_reset(void)
{
	if(gpio_reset_enable){
		pr_info("%s. \n", __func__);
		if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 0, "ctp_reset")){
			pr_info("%s: err when operate gpio. \n", __func__);
		}
		mdelay(TS_RESET_LOW_PERIOD);
		if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 1, "ctp_reset")){
			pr_info("%s: err when operate gpio. \n", __func__);
		}
		mdelay(TS_INITIAL_HIGH_PERIOD);
	}
}

/**
 * ctp_wakeup - function
 *
 */
static void ctp_wakeup(void)
{
	if(1 == gpio_wakeup_enable){  
		pr_info("%s. \n", __func__);
		if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_wakeup_hdle, 0, "ctp_wakeup")){
			pr_info("%s: err when operate gpio. \n", __func__);
		}
		mdelay(TS_WAKEUP_LOW_PERIOD);
		if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_wakeup_hdle, 1, "ctp_wakeup")){
			pr_info("%s: err when operate gpio. \n", __func__);
		}
		mdelay(TS_WAKEUP_HIGH_PERIOD);

	}
	return;
}

struct i2c_adapter *our_adapter;

/**
 * ctp_detect - Device detection callback for automatic device creation
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
static int ctp_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if(twi_id == adapter->nr)
	{
		pr_info("%s: Detected chip %s at adapter %d, address 0x%02x\n",
			 __func__, CTP_NAME, i2c_adapter_id(adapter), client->addr);

		our_adapter = adapter;
		strlcpy(info->type, CTP_NAME, I2C_NAME_SIZE);
		return 0;
	}else{
		return -ENODEV;
	}
}
////////////////////////////////////////////////////////////////

static struct ctp_platform_ops ctp_ops = {
	.get_pendown_state = ctp_get_pendown_state,
	.clear_penirq	   = ctp_clear_penirq,
	.set_irq_mode      = ctp_set_irq_mode,
	.set_gpio_mode     = ctp_set_gpio_mode,	
	.judge_int_occur   = ctp_judge_int_occur,
	.init_platform_resource = ctp_init_platform_resource,
	.free_platform_resource = ctp_free_platform_resource,
	.fetch_sysconfig_para = ctp_fetch_sysconfig_para,
	.ts_reset =          ctp_reset,
	.ts_wakeup =         ctp_wakeup,
	.ts_detect = ctp_detect,
};

static struct i2c_dev *get_free_i2c_dev(struct i2c_adapter *adap) 
{
	struct i2c_dev *i2c_dev;

	if (adap->nr >= I2C_MINORS){
		pr_info("i2c-dev:out of device minors (%d) \n",adap->nr);
		return ERR_PTR (-ENODEV);
	}

	i2c_dev = kzalloc(sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev){
		return ERR_PTR(-ENOMEM);
	}
	i2c_dev->adap = adap;

	spin_lock(&i2c_dev_list_lock);
	list_add_tail(&i2c_dev->list, &i2c_dev_list);
	spin_unlock(&i2c_dev_list_lock);
	
	return i2c_dev;
}

#if 0
static void cnt_ts_release(void)
{
	struct CNT_TS_DATA_T *data = i2c_get_clientdata(this_client);

	input_report_abs(data->input_dev, ABS_PRESSURE, 0);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
	
	input_sync(data->input_dev);
	return;

}
#endif

#define MAX_RETRY_READ 4


/***********************************************************************
    [function]: 
		      callback:								calculate data checksum
    [parameters]:
			    msg:             				data buffer which is used to store touch data;
			    s32Length:             	the length of the checksum ;
    [return]:
			    												checksum value;
************************************************************************/
static u8 Calculate_8BitsChecksum( u8 *msg, int s32Length )
{
	int s32Checksum = 0;
	int i;

	for ( i = 0 ; i < s32Length; i++ )
	{
		s32Checksum += msg[i];
	}

	return (u8)( ( -s32Checksum ) & 0xFF );
}

/***********************************************************************
    [function]: 
		      callback:								read touch  data ftom controller via i2c interface;
    [parameters]:
			    rxdata[in]:             data buffer which is used to store touch data;
			    length[in]:             the length of the data buffer;
    [return]:
			    CNT_TRUE:              	success;
			    CNT_FALSE:             	fail;
************************************************************************/
static int cnt_i2c_rxdata(u8 *rxdata, int length)
{
	int ret;
	struct i2c_msg msg;
	
  msg.addr = this_client->addr;
  msg.flags = I2C_M_RD;
  msg.len = length;
  msg.buf = rxdata;
  ret = i2c_transfer(this_client->adapter, &msg, 1);
	if (ret < 0)
		pr_err("msg %s i2c write error: %d\n", __func__, ret);
		
	return ret;
}

static int cnt_i2c_txdata(u8 *txdata, int length)
{
	int ret;
	struct i2c_msg msg;

  memset(&msg, 0, sizeof(msg));
  msg.addr = this_client->addr;
  msg.flags = 0;
  msg.len = length;
  msg.buf = txdata;
  ret = i2c_transfer(this_client->adapter, &msg, 1);
	if (ret < 0)
		pr_err("msg %s i2c write error: %d\n", __func__, ret);
		
	return ret;
}


/***********************************************************************
    [function]: 
		      callback:            		gather the finger information and calculate the X,Y
		                           		coordinate then report them to the input system;
    [parameters]:
          null;
    [return]:
          null;
************************************************************************/
int cnt_read_data(void)
{
#define READ_DATA_SIZE 14
    //struct CNT_TS_DATA_T *data = i2c_get_clientdata(this_client);
	struct CNT_TS_DATA_T *data = i2c_get_clientdata(this_client);
    u8 buf[READ_DATA_SIZE] = {0};
    int Touch_State = 0, temp_x0, temp_y0, dst_x, dst_y;
#ifdef SUPPORT_VIRTUAL_KEY
    int key_id = 0x0, have_VK = 0;
#endif
#ifdef CNT_PROXIMITY
	int change_state = PROX_NORMAL;
#endif
    int i,ret = -1;
    int changed;
    static int need_report_pointup = 1;

    ret = cnt_i2c_rxdata(buf, READ_DATA_SIZE);

	if (ret <= 0)
		printk("Error read\n");

	{
		static u8 copy_buf[READ_DATA_SIZE];
        changed = 0;
        for (i = 0; i < READ_DATA_SIZE; i++) {
            changed |= copy_buf[i] != buf[i];
            copy_buf[i] = buf[i];
        }
        //if (changed)
         {
//            pr_info("Touch data %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
        	pr_info("Touch data");
        	for (i = 0; i < READ_DATA_SIZE; i++) {
        		printk(" %#hhx", buf[i]);
        	}
        	printk("\n");
        }

	}

    if (ret > 0)
    {
#ifdef CNT_PROXIMITY
			if(prox_enable == 1)//if android phone is in call
			{
				if(buf[0] == 0x5A)
				{
					change_state = PROX_CLOSE;
				}
				else if (buf[0] == 0x5E)
				{
					change_state = PROX_OFF;
				}

				if(change_state == PROX_CLOSE)//chage backlight state
				{
					printk("[TSP]:%s, prox close\n", __func__);
					input_report_abs(prox_input, ABS_DISTANCE, 0);
				}
				else if(change_state == PROX_OFF)
				{
					printk("[TSP]:%s, prox off\n", __func__);
					input_report_abs(prox_input, ABS_DISTANCE, 1);
				}
			}
#endif

			//Judge Touch State
			if((buf[0] != 0x52) || (Calculate_8BitsChecksum(buf, 7) != buf[7])) //Check data packet ID & Check the data if valid
			{
				return 0;
			}
			else if((buf[1]== 0xFF) && (buf[2]== 0xFF) && (buf[3]== 0xFF) && (buf[4]== 0xFF) && (buf[6]== 0xFF))//Scan finger number on panel
			{

				if((buf[5]== 0x0) || (buf[5]== 0xFF))
				{
					Touch_State = Touch_State_No_Touch;
				}
#ifdef SUPPORT_VIRTUAL_KEY				
				else//VK 
				{
					Touch_State =  Touch_State_VK;
					key_id = buf[5] & 0x1F;
				}
#endif
			}
			else
			{
				if((buf[4]== 0x0) && (buf[5]== 0x0) && (buf[6]== 0x0))
				{
					Touch_State = Touch_State_One_Finger;
				}
				else
				{
					Touch_State = Touch_State_Two_Finger;	
				}

				
			} 

				temp_x0 = ((buf[1] & 0xF0) << 4) | buf[2];
				temp_y0 = ((buf[1] & 0x0F) << 8) | buf[3];
				dst_x = ((buf[4] & 0xF0) << 4) | buf[5];
				dst_y = ((buf[4] & 0x0F) <<8 ) | buf[6];
			//} 	
	}
	else
	{
		return 0;
	}

//    pr_info("Touch state %d\n", Touch_State);
        
    if(Touch_State == Touch_State_One_Finger)
	{
		/*Mapping CNT touch coordinate to Android coordinate*/

        // SWAP and mirror
  		_st_finger_infos[0].i2_y = SCREEN_MAX_X - (temp_x0*SCREEN_MAX_X) / CNT_RESOLUTION_X ;
  		_st_finger_infos[0].i2_x = (temp_y0*SCREEN_MAX_Y) / CNT_RESOLUTION_Y ;

        pr_info("Reporting one finger touch %d %d\n", _st_finger_infos[0].i2_x, _st_finger_infos[0].i2_y);

		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 1);        	
		if (revert_x_flag)
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,screen_max_x - _st_finger_infos[0].i2_x);
		else
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, _st_finger_infos[0].i2_x);

		if (revert_y_flag)
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, screen_max_y - _st_finger_infos[0].i2_y);
		else
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, _st_finger_infos[0].i2_y);
		input_mt_sync(data->input_dev);	
		input_sync(data->input_dev);
		need_report_pointup = 1;
	}
	else if(Touch_State == Touch_State_Two_Finger)
	{
		/*Mapping CNT touch coordinate to Android coordinate*/
		if (dst_x > 2048)
		{
			dst_x -= 4096;//transform the unsigh value to sign value
		}
		if (dst_y > 2048)
		{
			dst_y -= 4096;//transform the unsigh value to sign value
		}

        // SWAP and mirror
  		_st_finger_infos[0].i2_y = SCREEN_MAX_X - (temp_x0*SCREEN_MAX_X) / CNT_RESOLUTION_X ;
  		_st_finger_infos[0].i2_x = (temp_y0*SCREEN_MAX_Y) / CNT_RESOLUTION_Y ;

//  		_st_finger_infos[1].i2_x = ((temp_x0 + dst_x)*SCREEN_MAX_X) / CNT_RESOLUTION_X ;
//  		_st_finger_infos[1].i2_y = ((temp_y0 + dst_x)*SCREEN_MAX_Y) / CNT_RESOLUTION_Y ;

		_st_finger_infos[1].i2_y = SCREEN_MAX_X - ((temp_x0 + dst_x)*SCREEN_MAX_X) / CNT_RESOLUTION_X ;
		_st_finger_infos[1].i2_x = ((temp_y0 + dst_y)*SCREEN_MAX_Y) / CNT_RESOLUTION_Y ;


        pr_info("Reporting two finger touch %d %d %d %d\n", _st_finger_infos[0].i2_x, _st_finger_infos[0].i2_y, _st_finger_infos[1].i2_x, _st_finger_infos[1].i2_y);

		/*report first point*/
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 1);
		if (revert_x_flag)
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,screen_max_x - _st_finger_infos[0].i2_x);
		else
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, _st_finger_infos[0].i2_x);

		if (revert_y_flag)
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, screen_max_y - _st_finger_infos[0].i2_y);
		else
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, _st_finger_infos[0].i2_y);
		input_mt_sync(data->input_dev);

		/*report second point*/
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 1);
		if (revert_x_flag)
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,screen_max_x - _st_finger_infos[1].i2_x);
		else
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, _st_finger_infos[1].i2_x);

		if (revert_y_flag)
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, screen_max_y - _st_finger_infos[1].i2_y);
		else
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, _st_finger_infos[1].i2_y);
		input_mt_sync(data->input_dev);

		input_sync(data->input_dev);
        need_report_pointup = 1;
	}
#ifdef SUPPORT_VIRTUAL_KEY
	else if(Touch_State == Touch_State_VK)
	{
		/*you can implement your VK releated function here*/	
		if(key_id == 0x01)
		{
			input_report_key(data->input_dev, tsp_keycodes[0], 1);
		}
		else if(key_id = 0x02)
		{
			input_report_key(data->input_dev, tsp_keycodes[1], 1);
		}
		else if(key_id == 0x04)
		{
			input_report_key(data->input_dev, tsp_keycodes[2], 1);
		}
		else if(key_id == 0x08)
		{
			input_report_key(data->input_dev, tsp_keycodes[3], 1);
		}
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		have_VK=1;		
        need_report_pointup = 1;
	}
#endif
	else/*Finger up*/
	{
#ifdef SUPPORT_VIRTUAL_KEY						
		if(have_VK==1)
		{
			if(key_id == 0x01)
			{
				input_report_key(data->input_dev, tsp_keycodes[0], 0);
			}
			else if(key_id == 0x02)
			{
				input_report_key(data->input_dev, tsp_keycodes[1], 0);
			}
			else if(key_id == 0x04)
			{
				input_report_key(data->input_dev, tsp_keycodes[2], 0);
			}
			else if(key_id == 0x08)
			{
				input_report_key(data->input_dev, tsp_keycodes[3], 0);
			}
			input_report_key(data->input_dev, BTN_TOUCH, 0);
			have_VK=0;
            need_report_pointup = 1;
		}
#endif	
    
        if (need_report_pointup) {
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
            input_mt_sync(data->input_dev);
            input_sync(data->input_dev);
            need_report_pointup = 0;
            pr_info("Reported pointup\n");
        }
	}

	return 1;
}


static void cnt_ts_pen_irq_work(struct work_struct *work)
{
//	int ret = -1;
//	static int retry_counter = 0;
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct CNT_TS_DATA_T *cnt_ts = container_of(dw,struct CNT_TS_DATA_T,pen_event_work);

//	printk("Touch work\n");

	if (cnt_ts->in_suspend & LOCK_MODE)
		return;
//	if (this_client->dev.power.is_suspended) {
	//our_adapter
//	if (cnt_touch_suspend) {
//	if (our_adapter->dev.power.is_suspended) 
	if (cnt_ts->in_suspend & SUSPEND_MODE) 
	{
		// defer read till woken up
		queue_delayed_work(cnt_ts->ts_workqueue, &cnt_ts->pen_event_work,5);
	} else
    	cnt_read_data();    

}

static irqreturn_t cnt_ts_interrupt(int irq, void *dev_id)
{
	struct CNT_TS_DATA_T *cnt_ts = dev_id;

	print_int_info("==========------cnt_ts TS Interrupt-----============\n"); 
	if(!ctp_ops.judge_int_occur()){
		print_int_info("==IRQ_EINT%d=\n",CTP_IRQ_NO);
		ctp_ops.clear_penirq();
		if (!delayed_work_pending(&cnt_ts->pen_event_work))
		{
			print_int_info("Enter work %i\n",this_client->dev.power.is_suspended);
			queue_delayed_work(cnt_ts->ts_workqueue, &cnt_ts->pen_event_work,1);
		}
	}else{
		print_int_info("Other Interrupt\n");
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

#if defined(CONFIG_PM)

extern suspend_state_t g_suspend_state;
extern __u32 standby_wakeup_event;

static void cnt_ts_off(void) {
	// turn off touch
	if(gpio_reset_enable){
		pr_info("%s. \n", __func__);
		if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 0, "ctp_reset")){
			pr_info("%s: err when operate gpio. \n", __func__);
		}
	}
}

static int cnt_ts_suspend(struct i2c_client *client, pm_message_t mesg) {
	static uint8_t delay_mode_pkt[4] = {0xff, 0x11, 0xff, 0x01};
	int res;

	struct CNT_TS_DATA_T *cnt_ts = i2c_get_clientdata(client);
	printk("[%s] msg = %i\n",__func__,g_suspend_state);

	if ((g_suspend_state == PM_SUSPEND_PARTIAL) && !(cnt_ts->in_suspend & LOCK_MODE)) {
		standby_wakeup_event |= SUSPEND_WAKEUP_SRC_PIO;
		// inform touch about suspend
		pr_info("%s set touch delay mode\n", __func__);
		res = cnt_i2c_txdata(delay_mode_pkt, sizeof(delay_mode_pkt));
		pr_info("%s dleay mode result %d\n", __func__, res);

	} else {
		cnt_ts_off();

		cnt_ts->in_suspend |= SUSPEND_MODE;
	}
	return 0;
}

static int cnt_ts_resume(struct i2c_client *client) {

	struct CNT_TS_DATA_T *cnt_ts = i2c_get_clientdata(client);
	//printk("[%s] in suspend = %i\n",__func__,cnt_ts->in_suspend);
	if ((cnt_ts->in_suspend & SUSPEND_MODE) && !(cnt_ts->in_suspend & LOCK_MODE))	{
		ctp_ops.ts_reset();
	}
	cnt_ts->in_suspend = 0;

	return 0;
}
#endif

static int proc_keylock_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct CNT_TS_DATA_T *cnt_ts = (struct CNT_TS_DATA_T *)data;
	*eof = 1;
	if (cnt_ts)
		return snprintf(page, PAGE_SIZE, "%u\n", (cnt_ts->in_suspend & LOCK_MODE) ? 1 : 0);
	else
		return snprintf(page, PAGE_SIZE, "ERROR\n");
}

static int proc_keylock_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char buf[16];
	struct CNT_TS_DATA_T *cnt_ts = (struct CNT_TS_DATA_T *)data;

	if (count > sizeof(buf) -1 )
		return -EINVAL;

	if (!count)
		return 0;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';

	switch (buf[0]) {
		case '0':
			ctp_ops.ts_reset();
			if (cnt_ts)
				cnt_ts->in_suspend &= ~(SUSPEND_MODE | LOCK_MODE);
			break;
		default:
			cnt_ts_off();
			if (cnt_ts)
				cnt_ts->in_suspend |= SUSPEND_MODE | LOCK_MODE;
			break;
	}

	return count;
}

int cnt_ts_lock_init(struct CNT_TS_DATA_T *cnt_ts)
{
	struct proc_dir_entry *dir, *file;

	dir = proc_mkdir("keylock", NULL);
	if (!dir) {
		printk("could not create /proc/keylock\n");
		return -1;
	}

	file = create_proc_entry("lock", S_IRUGO | S_IWUGO, dir);
	if (!file) {
		printk("could not create /proc/keylock/lock\n");
		return -1;
	}

	file->data = cnt_ts;
	file->read_proc = proc_keylock_read;
	file->write_proc = proc_keylock_write;

	return 0;
}


static int 
cnt_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct CNT_TS_DATA_T *cnt_ts;
	struct input_dev *input_dev;
	struct device *dev;
	struct i2c_dev *i2c_dev;
	int err = 0;

	pr_info("====%s begin=====.  \n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	cnt_ts = kzalloc(sizeof(*cnt_ts), GFP_KERNEL);
	if (!cnt_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	//pr_info("touch panel gpio addr: = 0x%x", gpio_addr);
	this_client = client;
	
	//pr_info("cnt_ts_probe : client->addr = %d. \n", client->addr);
	this_client->addr = client->addr;
	//pr_info("cnt_ts_probe : client->addr = %d. \n", client->addr);
	i2c_set_clientdata(client, cnt_ts);

//	pr_info("==INIT_WORK=\n");
	INIT_DELAYED_WORK(&cnt_ts->pen_event_work, cnt_ts_pen_irq_work);
	cnt_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!cnt_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	cnt_ts->input_dev = input_dev;

	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);	
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TRACKING_ID, 0, 4, 0, 0);

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	input_dev->name		= CTP_NAME;		//dev_name(&client->dev)
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"cnt_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

	err = ctp_ops.set_irq_mode("ctp_para", "ctp_int_port", CTP_IRQ_MODE);
	if(0 != err){
		pr_info("%s:ctp_ops.set_irq_mode err. \n", __func__);
		goto exit_set_irq_mode;
	}

	err = request_irq(SW_INT_IRQNO_PIO, cnt_ts_interrupt, IRQF_SHARED, "cnt_ts", cnt_ts);
	if (err < 0) {
		dev_err(&client->dev, "cnt_ts_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}

	if (gpio_int_hdle) {
		//enable gpio eint trigger;
		sw_gpio_eint_set_enable(gpio_int_hdle,1);

		//clear trigger state
		sw_gpio_eint_clr_irqpd_sta(gpio_int_hdle);
	}

	i2c_dev = get_free_i2c_dev(client->adapter);
	if (IS_ERR(i2c_dev)){	
		err = PTR_ERR(i2c_dev);		
		return err;	
	}
	dev = device_create(i2c_dev_class, &client->adapter->dev, MKDEV(I2C_MAJOR,client->adapter->nr), NULL, "aw_i2c_ts%d", client->adapter->nr);	
	if (IS_ERR(dev))	{		
			err = PTR_ERR(dev);		
			return err;	
	}

	cnt_ts_lock_init(cnt_ts);

#if MSG2133_UPDATE
	msg2133_init_fw_class(cnt_ts);
#endif

	return 0;

exit_irq_request_failed:
exit_set_irq_mode:
	cancel_delayed_work_sync(&cnt_ts->pen_event_work);
	destroy_workqueue(cnt_ts->ts_workqueue);
	enable_irq(SW_INT_IRQNO_PIO);
exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	free_irq(SW_INT_IRQNO_PIO, cnt_ts);
exit_create_singlethread:
	pr_info("==singlethread error =\n");
	i2c_set_clientdata(client, NULL);
	kfree(cnt_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

static int __devexit cnt_ts_remove(struct i2c_client *client)
{

	struct CNT_TS_DATA_T *cnt_ts = i2c_get_clientdata(client);
	// power off touch
	if(gpio_reset_enable){
		pr_info("%s. \n", __func__);
		if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 0, "ctp_reset")){
			pr_info("%s: err when operate gpio. \n", __func__);
		}
	}
	
	pr_info("==cnt_ts_remove=\n");
	free_irq(SW_INT_IRQNO_PIO, cnt_ts);
	input_unregister_device(cnt_ts->input_dev);
	input_free_device(cnt_ts->input_dev);
	cancel_delayed_work_sync(&cnt_ts->pen_event_work);
	destroy_workqueue(cnt_ts->ts_workqueue);
	kfree(cnt_ts);
    
	i2c_set_clientdata(client, NULL);
	ctp_ops.free_platform_resource();

	return 0;

}

static const struct i2c_device_id cnt_ts_id[] = {
	{ CTP_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, cnt_ts_id);

static struct i2c_driver cnt_ts_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		= cnt_ts_probe,
	.remove		= __devexit_p(cnt_ts_remove),
	.id_table	= cnt_ts_id,
	.driver	= {
		.name	= CTP_NAME,
		.owner	= THIS_MODULE,
	},
	.address_list	= u_i2c_addr.normal_i2c,
#ifdef CONFIG_PM
	.suspend	= cnt_ts_suspend,
	.resume		= cnt_ts_resume,
#endif
};

static int __init cnt_ts_init(void)
{ 
	int ret = -1;
	int err = -1;

	pr_info("===========================%s=====================\n", __func__);

	if (ctp_ops.fetch_sysconfig_para)
	{
		if(ctp_ops.fetch_sysconfig_para()){
			pr_info("%s: err.\n", __func__);
			return -1;
		}
	}
	pr_info("%s: after fetch_sysconfig_para:  normal_i2c: 0x%hx. normal_i2c[1]: 0x%hx \n", \
	__func__, u_i2c_addr.normal_i2c[0], u_i2c_addr.normal_i2c[1]);

	err = ctp_ops.init_platform_resource();
	if(0 != err){
	    pr_info("%s:ctp_ops.init_platform_resource err. \n", __func__);    
	}

	//reset
	ctp_ops.ts_reset();
	//wakeup
	ctp_ops.ts_wakeup();  
	
	cnt_ts_driver.detect = ctp_ops.ts_detect;

	i2c_dev_class = class_create(THIS_MODULE,"aw_i2c_dev");
	if (IS_ERR(i2c_dev_class)) {		
		ret = PTR_ERR(i2c_dev_class);		
		class_destroy(i2c_dev_class);	
	}

	ret = i2c_add_driver(&cnt_ts_driver);

	return ret;
}

static void __exit cnt_ts_exit(void)
{
	pr_info("==cnt_ts_exit==\n");
	i2c_del_driver(&cnt_ts_driver);
}

late_initcall(cnt_ts_init);
module_exit(cnt_ts_exit);

MODULE_AUTHOR("Dushes");
MODULE_DESCRIPTION("CNT TouchScreen driver");
MODULE_LICENSE("GPL");

