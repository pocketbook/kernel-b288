#include "default_eink.h"

static void EINK_power_on(u32 sel);
static void EINK_power_off(u32 sel);

static void EINK_cfg_panel_info(panel_extend_para * info)
{
	u32 i = 0, j=0;
	u32 items;
	u8 lcd_gamma_tbl[][2] =
	{
		//{input value, corrected value}
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
	{
		{LCD_CMAP_G0,LCD_CMAP_B1,LCD_CMAP_G2,LCD_CMAP_B3},
		{LCD_CMAP_B0,LCD_CMAP_R1,LCD_CMAP_B2,LCD_CMAP_R3},
		{LCD_CMAP_R0,LCD_CMAP_G1,LCD_CMAP_R2,LCD_CMAP_G3},
		},
		{
		{LCD_CMAP_B3,LCD_CMAP_G2,LCD_CMAP_B1,LCD_CMAP_G0},
		{LCD_CMAP_R3,LCD_CMAP_B2,LCD_CMAP_R1,LCD_CMAP_B0},
		{LCD_CMAP_G3,LCD_CMAP_R2,LCD_CMAP_G1,LCD_CMAP_R0},
		},
	};

	items = sizeof(lcd_gamma_tbl)/2;
	for (i=0; i<items-1; i++) {
		u32 num = lcd_gamma_tbl[i+1][0] - lcd_gamma_tbl[i][0];

		for (j=0; j<num; j++) {
			u32 value = 0;

			value = lcd_gamma_tbl[i][1] + ((lcd_gamma_tbl[i+1][1] - lcd_gamma_tbl[i][1]) * j)/num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] = (value<<16) + (value<<8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items-1][1]<<16) + (lcd_gamma_tbl[items-1][1]<<8) + lcd_gamma_tbl[items-1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 Eink_int_panel(u32 sel)
{
	return 0;
}

static s32 Eink_uninit_panel(u32 sel)
{
	return 0;
}

static s32 EINK_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, Eink_int_panel, 0);
	LCD_OPEN_FUNC(sel, EINK_power_on, 0);
	//LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 0);
	return 0;
}

static s32 EINK_close_flow(u32 sel)
{
	//LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, EINK_power_off, 0);
	LCD_OPEN_FUNC(sel, Eink_uninit_panel, 0);

	return 0;
}

static void LCD_power_on(u32 sel)
{
	sunxi_lcd_power_enable(sel, 0);//config lcd_power pin to open lcd power0
	//sunxi_lcd_pin_cfg(sel, 1);
}

static void LCD_power_off(u32 sel)
{
	//sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_power_disable(sel, 0);//config lcd_power pin to close lcd power0
}

static void EINK_power_on(u32 sel)
{
	LCD_power_on(sel);

	//sunxi_lcd_pin_cfg(sel, 1);
}

static void EINK_power_off(u32 sel)
{
	//sunxi_lcd_pin_cfg(sel, 0);
	LCD_power_off(sel);
}

//sel: 0:lcd0; 1:lcd1
static s32 EINK_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

__lcd_panel_t default_eink = {
	/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex */
	.name = "default_eink",
	.func = {
		.cfg_panel_info = EINK_cfg_panel_info,
		.cfg_open_flow = EINK_open_flow,
		.cfg_close_flow = EINK_close_flow,
		.lcd_user_defined_func = EINK_user_defined_func,
	},
};
