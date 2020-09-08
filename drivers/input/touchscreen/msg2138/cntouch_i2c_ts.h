#ifndef __LINUX_CNTOUCH_TS_H__
#define __LINUX_CNTOUCH_TS_H__

//#define CONFIG_SUPPORT_CNT_CTP_UPG
//#define TP_FIRMWARE_UPDATE
//#define SUPPORT_VIRTUAL_KEY
//#define CNT_PROXIMITY
#define TP_FIRMWARE_UPDATE 1

#ifdef TP_FIRMWARE_UPDATE
#define U8 unsigned char
#define S8 signed char
#define U16 unsigned short
#define S16 signed short
#define U32 unsigned int
#define S32 signed int

#define TOUCH_ADDR_MSG20XX   0x26

#define CHIP_TYPE_MSG21XXA_DBI2C 0x62
#define CHIP_TYPE_MSG22XX_DBI2C 0x59

int i2c_dbbus_addr = CHIP_TYPE_MSG22XX_DBI2C;
int i2c_work_addr = TOUCH_ADDR_MSG20XX;

#define MSG2133_FW_UPDATE_ADDR      0x26 

static  char *fw_version;
#define DWIIC_MODE_ISP    0
#define DWIIC_MODE_DBBUS  1
static U8 temp[94][1024];
static int FwDataCnt;
struct class *firmware_class;
struct device *firmware_cmd_dev;
#endif

#define CHIP_TYPE_UNKNOWN	(0)
#define CHIP_TYPE_MSG21XX   (0x01) // EX. MSG2133
#define CHIP_TYPE_MSG21XXA  (0x02) // EX. MSG2133A/MSG2138A(Besides, use version to distinguish MSG2133A/MSG2138A, you may refer to _DrvFwCtrlUpdateFirmwareCash())
#define CHIP_TYPE_MSG26XXM  (0x03) // EX. MSG2633M
#define CHIP_TYPE_MSG22XX   (0x7A) // EX. MSG2238/MSG2256

#define MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE (48)  //48K
#define MSG22XX_FIRMWARE_INFO_BLOCK_SIZE (512) //512Byte

int g_ChipType = CHIP_TYPE_UNKNOWN;

#define CFG_DBG_DUMMY_INFO_SUPPORT   1     //output touch point information
#define CFG_DBG_FUCTION_INFO_SUPPORT 0     //output fouction name
#define CFG_DBG_INPUT_EVENT                   0     //debug input event


#define CFG_MAX_POINT_NUM            0x2    //max touch points supported
#ifdef CNT_PROXIMITY
#define CFG_NUMOFKEYS                    0x5    //number of touch keys + proximity support
#else
#define CFG_NUMOFKEYS                    0x4    //number of touch keys
#endif

#ifdef CONFIG_CNTouch_CUSTOME_ENV  
#define SCREEN_MAX_X           1024
#define SCREEN_MAX_Y           600
#else
//#define SCREEN_MAX_X           800
//#define SCREEN_MAX_Y           480
#define SCREEN_MAX_X           600
#define SCREEN_MAX_Y           800
#endif
#define CNT_RESOLUTION_X  2048
#define CNT_RESOLUTION_Y  2048
#define PRESS_MAX                 255

#define KEY_PRESS                 0x1
#define KEY_RELEASE              0x0

#define CNT_NAME    "ft5x_ts" //"cnt_ts"  

#define CNT_NULL                    0x0
#define CNT_TRUE                    0x1
#define CNT_FALSE                   0x0
#define I2C_CNT_ADDRESS    0x60

#define Touch_State_No_Touch   0x0
#define Touch_State_One_Finger 0x1
#define Touch_State_Two_Finger 0x2
#define Touch_State_VK 0x3

typedef unsigned char         CNT_BYTE;    
typedef unsigned short        CNT_WORD;   
typedef unsigned int          CNT_DWRD;    
typedef unsigned char         CNT_BOOL;  



 typedef struct _REPORT_FINGER_INFO_T
 {
     short   ui2_id;               /* ID information, from 0 to  CFG_MAX_POINT_NUM - 1*/
     short    u2_pressure;    /* ***pressure information, valid from 0 -63 **********/
     short    i2_x;                /*********** X coordinate, 0 - 2047 ****************/
     short    i2_y;                /* **********Y coordinate, 0 - 2047 ****************/
 } REPORT_FINGER_INFO_T;


typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

#ifdef CNT_PROXIMITY
enum {
	PROX_NORMAL = 0,
	PROX_CLOSE,
	PROX_OFF,
};
#endif

struct CNT_TS_DATA_T {
    struct input_dev    *input_dev;
    struct delayed_work     pen_event_work;
    struct workqueue_struct *ts_workqueue;
//    struct early_suspend	    early_suspend;
    int in_suspend;
};


#ifndef ABS_MT_TOUCH_MAJOR
#define ABS_MT_TOUCH_MAJOR    0x30    /* touching ellipse */
#define ABS_MT_TOUCH_MINOR    0x31    /* (omit if circular) */
#define ABS_MT_WIDTH_MAJOR    0x32    /* approaching ellipse */
#define ABS_MT_WIDTH_MINOR    0x33    /* (omit if circular) */
#define ABS_MT_ORIENTATION     0x34    /* Ellipse orientation */
#define ABS_MT_POSITION_X       0x35    /* Center X ellipse position */
#define ABS_MT_POSITION_Y       0x36    /* Center Y ellipse position */
#define ABS_MT_TOOL_TYPE        0x37    /* Type of touching device */
#define ABS_MT_BLOB_ID             0x38    /* Group set of pkts as blob */
#endif 

#define MSG2133_UPDATE              1

#ifdef MSG2133_UPDATE
#define MSG2133_SOFTWARE_UPDATE     1
#define MSG2133_TURN_ON_AUTO_UPDATE     0

#define _FW_UPDATE_C3_
#endif

#define TPD_OK                  0
#define TPD_REG_BASE            0x00
#define TPD_SOFT_RESET_MODE     0x01
#define TPD_OP_MODE             0x00
#define TPD_LOW_PWR_MODE        0x04
#define TPD_SYSINFO_MODE        0x10

#define GET_HSTMODE(reg)  ((reg & 0x70) >> 4)  // in op mode or not 
#define GET_BOOTLOADERMODE(reg) ((reg & 0x10) >> 4)  // in bl mode 

struct fw_version {
    unsigned short major;
    unsigned short minor;
    unsigned short VenderID;
};

enum {
    TP_HUANGZE = 1, //huangze
    TP_JUNDA,
    TP_MUDOND,
};

typedef enum
{

    EMEM_ALL = 0,
    EMEM_MAIN,
    EMEM_INFO,

}EMEM_TYPE_t;

typedef enum bool_t
{
    FALSE = 0,
    TRUE = 1
}bool_t;

#endif

#define BIT0  (1<<0)  // 0x0001
#define BIT1  (1<<1)  // 0x0002
#define BIT2  (1<<2)  // 0x0004
#define BIT3  (1<<3)  // 0x0008
#define BIT4  (1<<4)  // 0x0010
#define BIT5  (1<<5)  // 0x0020
#define BIT6  (1<<6)  // 0x0040
#define BIT7  (1<<7)  // 0x0080
#define BIT8  (1<<8)  // 0x0100
#define BIT9  (1<<9)  // 0x0200
#define BIT10 (1<<10) // 0x0400
#define BIT11 (1<<11) // 0x0800
#define BIT12 (1<<12) // 0x1000
#define BIT13 (1<<13) // 0x2000
#define BIT14 (1<<14) // 0x4000
#define BIT15 (1<<15) // 0x8000

