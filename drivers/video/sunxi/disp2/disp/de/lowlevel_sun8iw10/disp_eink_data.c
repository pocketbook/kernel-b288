/*
**********************************************************************************************************************
*											        Melis
*						           the Easy Portable/Player Operation System
*
*						         (c) Copyright 2012-2020
*											All	Rights Reserved
*
* File    :
* By      : libaiao
* Version : v1.00
* Data    :
* Note    : eink 设备控制时序数据和waveform数据
**********************************************************************************************************************
*/

#include "disp_eink_data.h"
#include "disp_waveform.h"

                                 //  G G S S  G S
                                 //  D D D D  D D
                                 //  S C C O  O L
                                 //  P K E E  E E
#define cB (0xB08000|0xff030303) //  1 0 1 1  1 0
#define cX (0xD08000|0xff030303) //  1 1 0 1  1 0
#define cW (0xF08000|0xff030303) //  1 1 1 1  1 0
#define cG (0xA08000|0xff030303) //  1 0 1 0  1 0
#define cD (0x208000|0xff030303) //  0 0 1 0  1 0
#define cH (0x608000|0xff030303) //  0 1 1 0  1 0
#define cT (0xE0A000|0xFF030303) //  1 1 1 0  1 1
#define cA (0xA00000|0xFF030303) //  1 0 1 0  0 0
#define cE (0xE08000|0xff030303) //  1 1 1 0  1 0

//      1-5         6 FSL        7 *FBL     8 FBL       LSL    |LBL|  DATA                    FEL       n  
//                                                                      
//       AAA.AAA  EEEHHH.HHHD  HHHEEE.EEEG EEE.EEEG   TTTTTTTTTTEEEWXXXXWWWWWW.WWWWWBBBBG   EEE.EEEG  GGG.GGG
// GDSP  ###.###  ###___.____  ___###.#### ###.####   ########################.##########   ###.####  ###.###
// GDCK  ___.___  ######.###_  ######.###_ ###.###_   ########################.#####_____   ###.###_  ___.___
// SDCE  ###.###  ######.####  ######.#### ###.####   ##############____######.##########   ###.####  ###.###
// SDOE  ___.___  ______.____  ______.____ ___.____   _____________###########.#########_   ___.____  ___.___
// GDOE  ___.___  ######.####  ######.#### ###.####   ########################.##########   ###.####  ###.###
// SDLE  ___.___  ______.____  ______.____ ___.____   ##########______________.__________   ___.____  ___.___


const __u32 eink_ctrl_line_index_tbl[EINK_LCD_H+1] =
{
    0,  0,  0,  0,  0,                      //line1--line5
    1,                                      //line6
    2,                                      //line7
    3,                                      //line8
    3,                                      //2012.8.26添加line9(解决右边少显示一行的问题)
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //line9--line608,共600行
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 100

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 200

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 300

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 400

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 500

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 600

    #if (EINK_PANEL_H >= 758)
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 700

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5

    4,  4,  4,  4,  4,  4,  4,  4,          //6 , 758
    #endif

    #if (EINK_PANEL_H >= 1072)
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 858

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 958

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //2
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //3
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //4
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //5
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //6
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //7
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //8
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //9
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //10, 1058

    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  //1068
    4,  4,  4,  4,  						//1072
    #endif
	
	#if(EINK_PANEL_H >= 1080)
    4,  4,  4,  4,  4,  4,  4,  4,      //1080
    #endif
    4, 5,                                      	   //line609

    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  //line610--line620
};

//NEW_CTRL_DATA
const __u32 eink_ctrl_tbl_GC16_COMMON[8][EINK_WF_WIDTH] =
{
//line1--line5; 共5行
// 1         2       3           4       5           6       7       8       9          10
{cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//1
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//2
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//3
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//4
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//6
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//7
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//8
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//9
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//10

 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//1
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//2
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//3
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//4

#if (EINK_PANEL_W >= 1024)
 //256 data
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5

cA,cA,cA, cA, cA,cA,//5

#endif

#if (EINK_PANEL_W >= 1440)
 //360 data
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5

cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5

cA,cA,cA, cA,
#endif

#if(EINK_PANEL_W >= 1448)
//362 data
cA,cA,
#endif

 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//6
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//7
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//8
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//9
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//10

 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//1
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//2
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//3
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//4
 cA,cA,cA, cA, cA,cA,cA, cA,cA,  cA,//5

 cA,cA,cA, cA, cA,cA,cA, cA
},


//line6
// 1         2       3           4        5           6       7       8       9          10
{cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1
                                                              //$14
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//10

// 1         2       3           4       5           6       7       $108       9          10
 cE,cE,cE, cE, cE,cE,cE, cE,cH,  cH,//1
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//2
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//3
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//4


#if (EINK_PANEL_W >= 1024)
 //256 data
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//6
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//7
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//8
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//9
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//10

 cH,cH,cH, cH, cH,cH,
#endif

#if (EINK_PANEL_W >= 1440)
 //360 data
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//6
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//7
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//8
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//9
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//10

cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//6
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//7
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//8
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//9
cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//10

cH,cH,cH, cH,

#endif

#if(EINK_PANEL_W >= 1448)
//362 data
cH,cH,
#endif


 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//5
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//6
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//7
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//8
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//9
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//10

// 1         2       3           4       5           6       7       8       9          10
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//1

// 1        2       3           4       $215           6       7       8       9          10
 cH,cH,cH, cH, cH,cD,cD, cD,cD,  cD,//2
 cD,cD,cD, cD, cD,cD,cD, cD,cD,  cD,//3
 cD,cD,cD, cD, cD,cD,cD, cD,cD,  cD,//4
 cD,cD,cD, cD, cD,cD,cD, cD,cD,  cD,//5

 cD,cD,cD, cD, cD,cD,cD, cD
},

//line7
// 1         2       3           4       5           6       7       8       9          10
{cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//1
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//2
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//3
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//4
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//5
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//6
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//7
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//8
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//9
 cH,cH,cH, cH, cH,cH,cH, cH,cH,  cH,//10


 cH,cH,cH, cH, cH,cH,cH, cH,cE,  cE,//1
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4

#if (EINK_PANEL_W >= 1024)
 //256 data
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9

 cE,cE,cE, cE, cE,cE,
#endif

#if (EINK_PANEL_W >= 1440)
 //360 data
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9

cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9

cE,cE,cE, cE,

#endif

#if(EINK_PANEL_W >= 1448)
//362 data
cE,cE,
#endif

 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//10


// 1         2       3           2          4        2         5          2           6        2         7         2        8          2       9            2        10         2                     2
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1

// 1         2       3           2          4        2         $215       2              6     2            7      2        8          2       9            2         10        2                     2
 cE,cE,cE, cE, cE,cG,cG, cG,cG,  cG,//2
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//3
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//4
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5

 cG,cG,cG, cG, cG,cG,cG, cG
},


//line8
// 1         2       3           4       5           6       7       8       9          10
{cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//10


 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4

#if (EINK_PANEL_W >= 1024)
 //256 data
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1//
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2//
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3//
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4//
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5//
 cE,cE,cE, cE, cE,cE,
#endif

#if (EINK_PANEL_W >= 1440)
 //360 data
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1//
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2//
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3//
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4//
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5//

cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1//
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2//
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3//
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4//
cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5//

cE,cE,cE, cE,

#endif

#if(EINK_PANEL_W >= 1448)
//362 data
cE,cE,
#endif

 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//10


// 1         2       3           2          4        2         5          2           6        2         7         2        8          2       9            2        10         2                     2
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1

// 1         2       3           2          4        2         $215       2              6     2            7      2           8       2          9         2           10      2                     2
 cE,cE,cE, cE, cE,cG,cG, cG,cG,  cG,//2
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//3
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//4
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5

 cG,cG,cG, cG, cG,cG,cG, cG

},

//line9-line608, 共600行
// 1         2       3           4       5           6       7       8       9          10
{cT,cT,cT, cT, cT,cT,cT, cT,cT,  cT,//1

// 1         2       $13         2          $14      2           $15      2           6        2         7         2        8          2       $19          2          10       2                     2
 cE,cE,cE, cW, cX,cX,cX, cX,cW,  cW,//2
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//3
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//4
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//5
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//6
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//7
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//8
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//9
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//10


 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//1
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//2
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//3
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//4

#if (EINK_PANEL_W >= 1024)
 //256 data
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//1//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//2//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//3//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//4//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//5
 cW,cW,cW, cW, cW,cW,
#endif

#if (EINK_PANEL_W >= 1440)
 //360 data
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//1//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//2//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//3//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//4//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//5

 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//1//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//2//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//3//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//4//
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//5

 cW,cW,cW, cW,
#endif

#if(EINK_PANEL_W >= 1448)
//362 data
cW,cW,
#endif



 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//5
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//6
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//7
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//8
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//9
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//10


// 1         2       3           2          4        2         5          2           6        2         7         2        8          2       9            2        10         2                     2
 cW,cW,cW, cW, cW,cW,cW, cW,cW,  cW,//1
//211       212     213         212        214      212       215        212        216       212      217        212      218        212     219          212        210      212                   212
 cW,cW,cW, cW, cW,cB,cB, cB,cB,  cB,//2
 cB,cB,cB, cB, cB,cB,cB, cB,cB,  cB,//3
 cB,cB,cB, cB, cB,cB,cB, cB,cB,  cB,//4
 cB,cB,cB, cB, cB,cB,cB, cB,cB,  cB,//5

//251       252     253         252        254      252       255        252        $256      252       257       252       258       252                  252                 252                   252
 cB,cB,cB, cB, cB,cG,cG, cG

},

//以上为有效数据行


//以下为场结束部分
//line609
//1         2       3           4       $5           6       7       8       9          10
{cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//10


 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//2
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//3
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//4

#if (EINK_PANEL_W >= 1024)
	//256 data
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9

	cE,cE,cE, cE, cE,cE,
#endif

#if (EINK_PANEL_W >= 1440)
	//360 data
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9

	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
	cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9

	cE,cE,cE, cE,
#endif

#if(EINK_PANEL_W >= 1448)
//362 data
	cE,cE,
#endif

 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//5
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//6
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//7
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//8
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//9
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//10


//1        2       3           2       3  4        2       3 5          2       3   6        2       3 $7        2       3 8         2       39           2       3 10        2       3             2       3
 cE,cE,cE, cE, cE,cE,cE, cE,cE,  cE,//1

//21        212     213         212        214      212       215        212          216     212        217      212       218       212       219        212        210      212                   212
 cE,cE,cE, cE, cE,cG,cG, cG,cG,  cG,//2
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//3
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//4
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5

 cG,cG,cG, cG, cG,cG,cG, cG
},

//line610-620
//1         2       3           4       $5           6       $7       8       9          10
{cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//1
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//2
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//3
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//4
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//6
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//7
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//8
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//9
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//10


 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//1
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//2
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//3
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//4

#if (EINK_PANEL_W >= 1024)
 //256 data
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//6
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//7
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//8
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//9

 cG,cG,cG, cG, cG,cG,
#endif

#if (EINK_PANEL_W >= 1440)
 //360 data
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//6
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//7
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//8
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//9

cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//6
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//7
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//8
cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//9

cG,cG,cG, cG,

#endif

#if(EINK_PANEL_W >= 1448)
//362 data
cG,cG,
#endif


 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//6
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//7
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//8
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//9
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//10


//1         2       3           2          4        2         5          2           6        2         $7        2         8         2        9           2         10        2                     2
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//1
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//2
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//3
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//4
 cG,cG,cG, cG, cG,cG,cG, cG,cG,  cG,//5

 cG,cG,cG, cG, cG,cG,cG, cG
},

};

//#define TEST_EDMA

#ifdef TEST_EDMA
static __s32 load_wavedata(const char *path, char *buf)
{
	struct file *fp = NULL;
	__s32 file_len = 0;
	__s32 read_len = 0;
	mm_segment_t fs;
	loff_t pos;
	__s32 ret = -EINVAL;

	if ((NULL == path) || (NULL == buf)) {
		pr_info("path is null\n");
		return -EINVAL;
	}

	fp = filp_open(path, O_RDONLY, 0);
	if(IS_ERR(fp))	{
		pr_info("fail to open waveform file(%s)", path);
		return -EBADF;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	file_len = fp->f_dentry->d_inode->i_size;

	//pr_info("%s: file-len=%d\n", __func__, file_len);

	read_len = vfs_read(fp, (char *)buf, file_len, &pos);
	if(read_len != file_len) {
		printk(KERN_ERR"read file(%s) error(read=%d byte, file=%d byte)\n", path, read_len, file_len);
		ret = -EAGAIN;
		goto error;
	}

	if(fp) {
		filp_close(fp, NULL);
		set_fs(fs);
	}

	//pr_info("load wavedata file(%s) successfully\n", path);
	return 0;

error:
	if(fp) {
		filp_close(fp, NULL);
		set_fs(fs);
	}

	return ret;
}


/*
Description	: wavedata buffer must call this function to init conctrl data, this interface is for 8-bits data TCON
Input 		: eink_timing_info -- specify the eink panel timing parameter
Ouput		: wavedata_buf -- wavedata buffer address(virturl address)
Return		: 0 -- success, others -- fail
*/
int init_eink_ctrl_data_8(void *wavedata_buf, struct eink_timing_param *eink_timing_info)
{
	u8 *point = NULL;
	//u8 *dest_point = NULL;
	//u32 buf_size = 258*620*2;

	char path[128] = {0};
	int ret = -EINVAL;

	if ((0 == wavedata_buf) || (NULL == eink_timing_info)) {
		printk(KERN_ERR "%s: input param is null\n", __func__);
		return -EINVAL;
	}

	point = (u8 *)wavedata_buf;
	memset(path, 0, sizeof(path));
	sprintf(path, "%s","/test/wave_data_frame0.bin");
	ret = load_wavedata(path, (char *)point);
	if (0 != ret) {
		printk(KERN_ERR"%s: load wavedata fail, ret = %d\n", __func__, ret);
	}

	return ret;
}


#else



/*
Description	: wavedata buffer must call this function to init conctrl data, this interface is for 8-bits data TCON
Input 		: eink_timing_info -- specify the eink panel timing parameter
Ouput		: wavedata_buf -- wavedata buffer address(virturl address)
Return		: 0 -- success, others -- fail
*/
int init_eink_ctrl_data_8(void *wavedata_buf, struct eink_timing_param *eink_timing_info)
{
	u32 row, col = 0, row_temp = 0;

	const u32* p_ctrl_gc_tbl = NULL;
	u32* p_ctrl_line = NULL;

	u32 wav_width = 0, wav_height = 0;
	u32 hync_len = 0, vync_len = 0;
	u32 h_data_len = 0, v_data_len = 0;

	u32 *point = NULL;
	A13_WAVEDATA *global_ctrl_buffer = NULL;

	A13_WAVEDATA *src = NULL;
	B100_WAVEDATA_8 *dest = NULL;

	if ((NULL == wavedata_buf) || (NULL == eink_timing_info)) {
		printk(KERN_ERR "%s: input param is null\n", __func__);
		return -EINVAL;
	}

	hync_len = eink_timing_info->lsl + eink_timing_info->lbl + eink_timing_info->lel;
	vync_len = eink_timing_info->fsl + eink_timing_info->fbl + eink_timing_info->fel;
	h_data_len = eink_timing_info->width >> 2;
	v_data_len = eink_timing_info->height;
	wav_width = h_data_len + hync_len;
	wav_height = v_data_len + vync_len;

	//pr_info("%s: wav_width=%d, wav_height=%d\n", __func__, wav_width, wav_height);

	global_ctrl_buffer = (A13_WAVEDATA *)kzalloc(wav_width*wav_height*sizeof(A13_WAVEDATA), GFP_KERNEL);
	if (NULL == global_ctrl_buffer) {
		printk(KERN_ERR "%s: alloc memory for global control buffer fail, size=0x%x\n", __func__, wav_width*wav_height*sizeof(A13_WAVEDATA));
		return -ENOMEM;
	}

	point = (u32 *)global_ctrl_buffer;
	p_ctrl_gc_tbl = (u32*)eink_ctrl_tbl_GC16_COMMON[4];
	p_ctrl_line = (u32*)eink_ctrl_line_index_tbl;

	//line1--line8
	for(row = 0; row < (eink_timing_info->fsl + eink_timing_info->fbl); row++)
	{
		row_temp = *p_ctrl_line++;
		for(col = 0; col < wav_width; col++)
		{
			*point = eink_ctrl_tbl_GC16_COMMON[row_temp][col];
			point++;
		}
	}

	for(; row < (wav_height - eink_timing_info->fel); row++)
	{
		row_temp = *p_ctrl_line++;
		for(col = 0; col<wav_width; col++)
		{
			*point = (*(p_ctrl_gc_tbl + col));
			point++;
		}
	}

	for(; row < wav_height; row++)
	{
		row_temp = *p_ctrl_line++;
		for(col = 0; col < wav_width; col++)
		{
			*point = eink_ctrl_tbl_GC16_COMMON[row_temp][col];
			point++;
		}
	}

	src = global_ctrl_buffer;
	dest = (B100_WAVEDATA_8 *)wavedata_buf;

	for (row = 0; row < wav_height; row++) {
		for (col = 0; col < wav_width; col++) {
			dest->bits.mode = src->bits.mode;
			dest->bits.oeh = src->bits.oeh;
			dest->bits.leh = src->bits.leh;
			dest->bits.sth = src->bits.sth;
			dest->bits.ckv = src->bits.ckv;
			dest->bits.stv = src->bits.stv;
			dest->bits.res0 = 3;
			dest++;
			src++;
		}
	}

	if (NULL != global_ctrl_buffer) {
		kfree(global_ctrl_buffer);
		global_ctrl_buffer = NULL;
	}

	return 0;
}
#endif


/*
Description	: wavedata buffer must call this function to init conctrl data, this interface is for 16-bits data TCON
Input 		: eink_timing_info -- specify the eink panel timing parameter
Ouput		: wavedata_buf -- wavedata buffer address(virturl address)
Return		: 0 -- success, others -- fail
*/
int init_eink_ctrl_data_16(void *wavedata_buf, struct eink_timing_param *eink_timing_info)
{
	u32 row, col = 0, row_temp = 0;

	const u32* p_ctrl_gc_tbl = NULL;
	u32* p_ctrl_line = NULL;

	u32 wav_width = 0, wav_height = 0;
	u32 hync_len = 0, vync_len = 0;
	u32 h_data_len = 0, v_data_len = 0;

	u32 *point = NULL;
	A13_WAVEDATA *global_ctrl_buffer = NULL;

	A13_WAVEDATA *src = NULL;
	B100_WAVEDATA_16 *dest = NULL;

	if ((NULL == wavedata_buf) || (NULL == eink_timing_info)) {
		printk(KERN_ERR "%s: input param is null\n", __func__);
		return -EINVAL;
	}

	hync_len = eink_timing_info->lsl + eink_timing_info->lbl + eink_timing_info->lel;
	vync_len = eink_timing_info->fsl + eink_timing_info->fbl + eink_timing_info->fel;
	h_data_len = eink_timing_info->width >> 2;
	v_data_len = eink_timing_info->height;
	wav_width = h_data_len + hync_len;
	wav_height = v_data_len + vync_len;

	pr_info("%s: wav_width=%d, wav_height=%d\n", __func__, wav_width, wav_height);

	global_ctrl_buffer = (A13_WAVEDATA *)kzalloc(wav_width*wav_height*sizeof(A13_WAVEDATA), GFP_KERNEL);
	if (NULL == global_ctrl_buffer) {
		printk(KERN_ERR "%s: alloc memory for global control buffer fail, size=0x%x\n", __func__, wav_width*wav_height*sizeof(A13_WAVEDATA));
		return -ENOMEM;
	}

	point = (u32 *)global_ctrl_buffer;
	p_ctrl_gc_tbl = (u32*)eink_ctrl_tbl_GC16_COMMON[4];
	p_ctrl_line = (u32*)eink_ctrl_line_index_tbl;

	//line1--line8
	for(row = 0; row < (eink_timing_info->fsl + eink_timing_info->fbl); row++)
	{
		row_temp = *p_ctrl_line++;
		for(col = 0; col < wav_width; col++)
		{
			*point = eink_ctrl_tbl_GC16_COMMON[row_temp][col];
			point++;
		}
	}

/*	  p_ctrl_line += 600;*/
/*	  point += 600*258;*/

	for(; row < (wav_height - eink_timing_info->fel); row++)
	{
		row_temp = *p_ctrl_line++;
		for(col = 0; col<(eink_timing_info->lsl + eink_timing_info->lbl); col++)
		{
			*point = (*(p_ctrl_gc_tbl + col));
			point++;
		}

		point+=h_data_len;
		col +=h_data_len;

		for(; col<wav_width; col++)
		{
			*point = (*(p_ctrl_gc_tbl + col));
			point++;
		}
	}

	for(; row < wav_height; row++)
	{
		row_temp = *p_ctrl_line++;
		for(col = 0; col < wav_width; col++)
		{
			*point = eink_ctrl_tbl_GC16_COMMON[row_temp][col];
			point++;
		}
	}

	//convert to new format
	src = global_ctrl_buffer;
	dest = (B100_WAVEDATA_16 *)wavedata_buf;
	for (row = 0; row < wav_height; row++) {
		for (col = 0; col < wav_width; col++) {
			dest->bits.mode = src->bits.mode;
			dest->bits.oeh = src->bits.oeh;
			dest->bits.leh = src->bits.leh;
			dest->bits.sth = src->bits.sth;
			dest->bits.ckv = src->bits.ckv;
			dest->bits.stv = src->bits.stv;
		}
	}

	if (NULL != global_ctrl_buffer) {
		kfree(global_ctrl_buffer);
		global_ctrl_buffer = NULL;
	}

	return 0;

}



