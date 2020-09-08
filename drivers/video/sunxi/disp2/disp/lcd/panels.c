#include "panels.h"

struct sunxi_lcd_drv g_lcd_drv;

extern __lcd_panel_t default_eink;
extern __lcd_panel_t default_panel;

__lcd_panel_t* panel_array[] = {
	&default_eink,
	&default_panel,
	/* add new panel below */

	NULL,
};

static void lcd_set_panel_funs(void)
{
	int i;

	for (i=0; panel_array[i] != NULL; i++) {
		sunxi_lcd_set_panel_funs(panel_array[i]->name, &panel_array[i]->func);
	}

	return ;
}

int lcd_init(void)
{
	sunxi_disp_get_source_ops(&g_lcd_drv.src_ops);
	lcd_set_panel_funs();

	return 0;
}
