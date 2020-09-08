#ifndef _B288_H_
#define _B288_H_

#define TCON_BASE       0xf1c0c000
#define PORT_BASE       0xf1c20800

#define TCON_GCTL       *((volatile u32 *)(TCON_BASE+0x000))
#define TCON_GINT0      *((volatile u32 *)(TCON_BASE+0x004))
#define TCON_GINT1      *((volatile u32 *)(TCON_BASE+0x008))
#define TCON_FRM_CTL    *((volatile u32 *)(TCON_BASE+0x010))
#define TCON_FRM_SEED0  *((volatile u32 *)(TCON_BASE+0x014))
#define TCON_FRM_SEED1  *((volatile u32 *)(TCON_BASE+0x018))
#define TCON_FRM_SEED2  *((volatile u32 *)(TCON_BASE+0x01c))
#define TCON_FRM_SEED3  *((volatile u32 *)(TCON_BASE+0x020))
#define TCON_FRM_SEED4  *((volatile u32 *)(TCON_BASE+0x024))
#define TCON_FRM_SEED5  *((volatile u32 *)(TCON_BASE+0x028))
#define TCON_FRM_TAB0   *((volatile u32 *)(TCON_BASE+0x02c))
#define TCON_FRM_TAB1   *((volatile u32 *)(TCON_BASE+0x030))
#define TCON_FRM_TAB2   *((volatile u32 *)(TCON_BASE+0x034))
#define TCON_FRM_TAB3   *((volatile u32 *)(TCON_BASE+0x038))
#define TCON0_CTL       *((volatile u32 *)(TCON_BASE+0x040))
#define TCON0_DCLK      *((volatile u32 *)(TCON_BASE+0x044))
#define TCON0_BASIC0    *((volatile u32 *)(TCON_BASE+0x048))
#define TCON0_BASIC1    *((volatile u32 *)(TCON_BASE+0x04c))
#define TCON0_BASIC2    *((volatile u32 *)(TCON_BASE+0x050))
#define TCON0_BASIC3    *((volatile u32 *)(TCON_BASE+0x054))
#define TCON0_HV_IF     *((volatile u32 *)(TCON_BASE+0x058))
#define TCON0_CPU_IF    *((volatile u32 *)(TCON_BASE+0x060))
#define TCON0_IO_POL    *((volatile u32 *)(TCON_BASE+0x088))
#define TCON0_IO_TRI    *((volatile u32 *)(TCON_BASE+0x08c))

#define EPDC_VBUS_REGULATOR "axp22_dldo3"

#define epdc_clk_enable(clk) clk_prepare_enable(clk)
#define clk_reset(hclk,state)
#define EPDC_CLK_VIDEO_PLL   "pll_video0"
#define EPDC_CLK_DEBE0       "de"
#define EPDC_CLK_LCD0_CH0    "tcon0"
#define LCDC0_IRQ            118

#define LCD_PORT 3 // PD

#define SPV  (1 << 5)
#define CKV  (1 << 4)
#define SDCE (1 << 3)
#define SDOE (1 << 1)
#define GDOE (1 << 0)
#define SDLE (1 << 2)

#define PIN_CLK (1 << 24)
#define PINS_DATA8  (0xff << 8)
#define PINS_DATA16 (0xffff << 8)
#define PINS_SYNC0 (GDOE | SDOE | SDLE | CKV)
#define PINS_SYNC1 (SPV | SDCE)

#define PIN_INPUT  0
#define PIN_OUTPUT 1
#define PIN_LCD    2

#define DMASK8  0xc0
#define DMASK16 0

#endif


