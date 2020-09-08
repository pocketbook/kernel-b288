/* linux/drivers/video/sunxi/disp/dev_disp.c
 *
 * Copyright (c) 2013 Allwinnertech Co., Ltd.
 * Author: Tyle <tyle@allwinnertech.com>
 *
 * Display driver for sunxi platform
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "dev_disp.h"
#include <linux/pm_runtime.h>
#if defined(CONFIG_DEVFREQ_DRAM_FREQ_WITH_SOFT_NOTIFY)
#include <linux/sunxi_dramfreq.h>
#endif
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include "disp_debug.h"

disp_drv_info g_disp_drv;

#define MY_BYTE_ALIGN(x) ( ( (x + (4*1024-1)) >> 12) << 12)             /* alloc based on 4K byte */

static u32 suspend_output_type[2] = {0,0};
static u32 suspend_status = 0;//0:normal; suspend_status&1 != 0:in early_suspend; suspend_status&2 != 0:in suspend;
static u32 suspend_prestep = 3; //0:after early suspend; 1:after suspend; 2:after resume; 3 :after late resume
static u32 power_status_init = 0;

//static unsigned int gbuffer[4096];
static struct info_mm  g_disp_mm[10];

static struct cdev *my_cdev;
static dev_t devid ;
static struct class *disp_class;
static struct device *display_dev;

static unsigned int g_disp = 0, g_enhance_mode = 0, g_cvbs_enhance_mode = 0;
static u32 DISP_print = 0xffff;   //print cmd which eq DISP_print
static bool g_pm_runtime_enable = 0; //when open the CONFIG_PM_RUNTIME,this bool can also control if use the PM_RUNTIME.
#ifndef CONFIG_OF
static struct sunxi_disp_mod disp_mod[] = {
	{DISP_MOD_DE      ,    "de"   },
	{DISP_MOD_LCD0    ,    "lcd0" },
	{DISP_MOD_DSI0    ,    "dsi0" },
#ifdef DISP_SCREEN_NUM
	#if DISP_SCREEN_NUM == 2
	{DISP_MOD_LCD1    ,    "lcd1" }
	#endif
#else
#	error "DISP_SCREEN_NUM undefined!"
#endif
};

static struct resource disp_resource[] =
{
};
#endif


static struct attribute *disp_attributes[] = {
    NULL
};

static struct attribute_group disp_attribute_group = {
  .name = "attr",
  .attrs = disp_attributes
};

unsigned int disp_boot_para_parse(const char *name)
{
	unsigned int value = 0;
	if (of_property_read_u32(g_disp_drv.dev->of_node, name, &value) < 0)
		DDBG("  disp.%s read fail\n", name);

	DDBG("  disp.%s=0x%x (%d)\n", name, value, value);
	return value;
}

EXPORT_SYMBOL(disp_boot_para_parse);

const char *disp_boot_para_parse_str(const char *name)
{
	const char *str;

	if (!of_property_read_string(g_disp_drv.dev->of_node, name, &str)) {
		DDBG("  disp.%s='%s'\n", name, str);
		return str;
	} else {
		DDBG("  disp.%s read fail\n", name);
		return NULL;
	}
}
EXPORT_SYMBOL(disp_boot_para_parse_str);

static s32 parser_disp_init_para(const struct device_node *np, disp_init_para * init_para)
{
	int  value;

	memset(init_para, 0, sizeof(disp_init_para));

	if (of_property_read_u32(np, "disp_init_enable", &value) < 0) {
		__wrn("of_property_read disp_init.disp_init_enable fail\n");
		return -1;
	}
	init_para->b_init = value;

	if (of_property_read_u32(np, "disp_mode", &value) < 0)	{
		__wrn("of_property_read disp_init.disp_mode fail\n");
		return -1;
	}
	init_para->disp_mode= value;

	//screen0
	if (of_property_read_u32(np, "screen0_output_type", &value) < 0)	{
	DDBG("of_property_read disp_init.screen0_output_type fail\n");
		return -1;
	}
	if (value == 0) {
		init_para->output_type[0] = DISP_OUTPUT_TYPE_NONE;
		DDBG("  output_type[0]=NONE\n");
	}	else if (value == 1) {
		init_para->output_type[0] = DISP_OUTPUT_TYPE_LCD;
		DDBG("  output_type[0]=LCD\n");
	}	else if (value == 2)	{
		init_para->output_type[0] = DISP_OUTPUT_TYPE_TV;
		DDBG("  output_type[0]=TV\n");
	}	else if (value == 3)	{
		init_para->output_type[0] = DISP_OUTPUT_TYPE_HDMI;
		DDBG("  output_type[0]=HDMI\n");
	}	else if (value == 4)	{
		init_para->output_type[0] = DISP_OUTPUT_TYPE_VGA;
		DDBG("  output_type[0]=VGA\n");
	}	else {
		DDBG("invalid screen0_output_type %d\n", init_para->output_type[0]);
		return -1;
	}

	if (of_property_read_u32(np, "screen0_output_mode", &value) < 0)	{
		DDBG("of_property_read disp_init.screen0_output_mode fail\n");
		return -1;
	}
	DDBG("  output_mode[0]=%d\n", value);
	if (init_para->output_type[0] == DISP_OUTPUT_TYPE_TV || init_para->output_type[0] == DISP_OUTPUT_TYPE_HDMI
	    || init_para->output_type[0] == DISP_OUTPUT_TYPE_VGA) {
		init_para->output_mode[0]= value;
	}
	if (DISP_SCREEN_NUM > 1) {
	//screen1
		if (of_property_read_u32(np, "screen1_output_type", &value) < 0)	{
			DDBG("  disp_init.screen1_output_type read fail\n");
			return -1;
		}
		if (value == 0) {
			init_para->output_type[1] = DISP_OUTPUT_TYPE_NONE;
			DDBG("  output_type[1]=NONE\n");
		}	else if (value == 1)	{
			init_para->output_type[1] = DISP_OUTPUT_TYPE_LCD;
			DDBG("  output_type[1]=LCD\n");
		}	else if (value == 2)	{
			init_para->output_type[1] = DISP_OUTPUT_TYPE_TV;
			DDBG("  output_type[1]=TV\n");
		}	else if (value == 3)	{
			init_para->output_type[1] = DISP_OUTPUT_TYPE_HDMI;
			DDBG("  output_type[1]=HDMI\n");
		}	else if (value == 4)	{
			init_para->output_type[1] = DISP_OUTPUT_TYPE_VGA;
			DDBG("  output_type[1]=VGA\n");
		}	else {
			DDBG("invalid screen1_output_type %d\n", init_para->output_type[1]);
			return -1;
		}

		if (of_property_read_u32(np, "screen1_output_mode", &value) < 0)	{
			DDBG("of_property_read disp_init.screen1_output_mode fail\n");
			return -1;
		}
		DDBG("  output_mode[1]=%d\n", value);
		if (init_para->output_type[1] == DISP_OUTPUT_TYPE_TV || init_para->output_type[1] == DISP_OUTPUT_TYPE_HDMI
		    || init_para->output_type[1] == DISP_OUTPUT_TYPE_VGA) {
			init_para->output_mode[1]= value;
		}
	}
#if 0
	//screen2
	if (of_property_read_u32(np, "screen2_output_type", &value) < 0)	{
		__inf("of_property_read disp_init.screen2_output_type fail\n");
	}
	if (value == 0) {
		init_para->output_type[2] = DISP_OUTPUT_TYPE_NONE;
	}	else if (value == 1) {
		init_para->output_type[2] = DISP_OUTPUT_TYPE_LCD;
	}	else if (value == 2)	{
		init_para->output_type[2] = DISP_OUTPUT_TYPE_TV;
	}	else if (value == 3)	{
		init_para->output_type[2] = DISP_OUTPUT_TYPE_HDMI;
	}	else if (value == 4)	{
		init_para->output_type[2] = DISP_OUTPUT_TYPE_VGA;
	}	else {
		__inf("invalid screen0_output_type %d\n", init_para->output_type[2]);
	}

	if (of_property_read_u32(np, "screen2_output_mode", &value) < 0)	{
		__inf("of_property_read disp_init.screen2_output_mode fail\n");
	}
	if (init_para->output_type[2] == DISP_OUTPUT_TYPE_TV || init_para->output_type[2] == DISP_OUTPUT_TYPE_HDMI
	    || init_para->output_type[2] == DISP_OUTPUT_TYPE_VGA) {
		init_para->output_mode[2]= value;
	}
#endif
	//fb0
	init_para->buffer_num[0]= 2;

	if (of_property_read_u32(np, "fb0_format", &value) < 0) {
		DDBG("of_property_read disp_init.fb0_format fail\n");
		return -1;
	}
	init_para->format[0]= value;
	DDBG("  format[0]=%d\n", value);

	if (of_property_read_u32(np, "fb0_width", &value) < 0)	{
		DDBG("of_property_read disp_init.fb0_width fail\n");
		return -1;
	}
	init_para->fb_width[0]= value;
	DDBG("  fb_width[0]=%d\n", value);

	if (of_property_read_u32(np, "fb0_height", &value) < 0)	{
		DDBG("of_property_read disp_init.fb0_height fail\n");
		return -1;
	}
	init_para->fb_height[0]= value;
	DDBG("  fb_height[0]=%d\n", value);

	//fb1
	if (DISP_SCREEN_NUM > 1) {
		init_para->buffer_num[1]= 2;

		if (of_property_read_u32(np, "fb1_format", &value) < 0) {
			DDBG("of_property_read disp_init.fb1_format fail\n");
		}
		init_para->format[1]= value;
		DDBG("  format[1]=%d\n", value);

		if (of_property_read_u32(np, "fb1_width", &value) < 0) {
			DDBG("of_property_read disp_init.fb1_width fail\n");
		}
		init_para->fb_width[1]= value;
		DDBG("  fb_width[1]=%d\n", value);

		if (of_property_read_u32(np, "fb1_height", &value) < 0) {
			DDBG("of_property_read disp_init.fb1_height fail\n");
		}
		init_para->fb_height[1]= value;
		DDBG("  fb_height[1]=%d\n", value);
	}
#if 0
	//fb2
	init_para->buffer_num[2]= 2;

	if (of_property_read_u32(np, "fb2_format", &value) < 0) {
		__inf("of_property_read disp_init.fb2_format fail\n");
	}
	init_para->format[2]= value;

	if (of_property_read_u32(np, "fb2_width", &value) < 0) {
		__inf("of_property_read disp_init.fb2_width fail\n");
	}
	init_para->fb_width[2]= value;

	if (of_property_read_u32(np, "fb2_height", &value) < 0) {
		__inf("of_property_read disp_init.fb2_height fail\n");
	}
	init_para->fb_height[2]= value;
#endif

	return 0;
}

void *disp_malloc(u32 num_bytes, void *p_phys_addr)
{
	uint32_t actual_bytes = PAGE_ALIGN(num_bytes);
	void *address = alloc_pages_exact(actual_bytes, __GFP_DMA | __GFP_ZERO);
	if (address && p_phys_addr) *((uint32_t *)p_phys_addr) = virt_to_phys(address);
	return address;
}

void  disp_free(void *virt_addr, void* phys_addr, u32 num_bytes)
{
	u32 actual_bytes;

	actual_bytes = MY_BYTE_ALIGN(num_bytes);
	if (phys_addr && virt_addr)
		dma_free_coherent(g_disp_drv.dev, actual_bytes, virt_addr, (dma_addr_t)phys_addr);

	return ;
}

static void resume_proc(unsigned disp)
{
	struct disp_manager *mgr = NULL;

	mgr = g_disp_drv.mgr[disp];
	if (!mgr || !mgr->device)
		return ;

	if (DISP_OUTPUT_TYPE_LCD == mgr->device->type) {
		mgr->device->fake_enable(mgr->device);
	}
}

static void resume_work_0(struct work_struct *work)
{
	resume_proc(0);
}

static void resume_work_1(struct work_struct *work)
{
	resume_proc(1);
}

static void start_work(struct work_struct *work)
{
	int num_screens;
	int screen_id;
	int count = 0;

	num_screens = bsp_disp_feat_get_num_screens();
	while ((g_disp_drv.inited == 0) && (count < 5)) {
		count ++;
		msleep(10);
	}
	if (count >= 5)
		ERR("%s, timeout\n", __func__);
	if (g_disp_drv.para.boot_info.sync == 0) {
		for (screen_id = 0; screen_id<num_screens; screen_id++) {
			int disp_mode = g_disp_drv.disp_init.disp_mode;
			int output_type = g_disp_drv.disp_init.output_type[screen_id];
			int output_mode = g_disp_drv.disp_init.output_mode[screen_id];
			int lcd_registered = bsp_disp_get_lcd_registered(screen_id);
			int hdmi_registered = bsp_disp_get_hdmi_registered();

			DDBG("start_work: sel=%d, output_type=%d, lcd_reg=%d, hdmi_reg=%d\n",
				screen_id, output_type, lcd_registered, hdmi_registered);
			if (((disp_mode	== DISP_INIT_MODE_SCREEN0) && (screen_id == 0))
				|| ((disp_mode	== DISP_INIT_MODE_SCREEN1) && (screen_id == 1))) {
				if ((output_type == DISP_OUTPUT_TYPE_LCD)) {
					if (lcd_registered	&& bsp_disp_get_output_type(screen_id) != DISP_OUTPUT_TYPE_LCD) {
						bsp_disp_device_switch(screen_id, output_type, output_mode);
						suspend_output_type[screen_id] = output_type;
					}
				}
				else if (output_type == DISP_OUTPUT_TYPE_HDMI) {
					if (hdmi_registered	&& bsp_disp_get_output_type(screen_id) != DISP_OUTPUT_TYPE_HDMI) {
						msleep(600);
						bsp_disp_device_switch(screen_id, output_type, output_mode);
						suspend_output_type[screen_id] = output_type;
					}
				} else {
					bsp_disp_device_switch(screen_id, output_type, output_mode);
					suspend_output_type[screen_id] = output_type;
				}
			}
		}
	}else {
		if ((g_disp_drv.para.boot_info.type == DISP_OUTPUT_TYPE_HDMI) && !bsp_disp_get_hdmi_registered())
			return;
		if (bsp_disp_get_output_type(g_disp_drv.para.boot_info.disp) != g_disp_drv.para.boot_info.type) {
			bsp_disp_sync_with_hw(&g_disp_drv.para);
			suspend_output_type[g_disp_drv.para.boot_info.disp] = g_disp_drv.para.boot_info.type;
		}
	}
}

static s32 start_process(void)
{
	flush_work(&g_disp_drv.start_work);
	schedule_work(&g_disp_drv.start_work);
	return 0;
}

s32 disp_register_sync_proc(void (*proc)(u32))
{
	struct proc_list *new_proc;

	DDBG("disp_register_sync_proc (%p)\n", proc);
	new_proc = (struct proc_list*)disp_sys_malloc(sizeof(struct proc_list));
	if (new_proc) {
		new_proc->proc = proc;
		list_add_tail(&(new_proc->list), &(g_disp_drv.sync_proc_list.list));
	} else {
		pr_warn("malloc fail in %s\n", __func__);
	}

	return 0;
}

s32 disp_unregister_sync_proc(void (*proc)(u32))
{
	struct proc_list *ptr;

	DDBG("disp_unregister_sync_proc (%p)\n", proc);
	if ((NULL == proc)) {
		pr_warn("hdl is NULL in %s\n", __func__);
		return -1;
	}
	list_for_each_entry(ptr, &g_disp_drv.sync_proc_list.list, list) {
		if (ptr->proc == proc) {
			list_del(&ptr->list);
			disp_sys_free((void*)ptr);
			return 0;
		}
	}

	return -1;
}

s32 disp_register_sync_finish_proc(void (*proc)(u32))
{
	struct proc_list *new_proc;

	DDBG("disp_register_sync_finish_proc (%p)\n", proc);
	new_proc = (struct proc_list*)disp_sys_malloc(sizeof(struct proc_list));
	if (new_proc) {
		new_proc->proc = proc;
		list_add_tail(&(new_proc->list), &(g_disp_drv.sync_finish_proc_list.list));
	} else {
		pr_warn("malloc fail in %s\n", __func__);
	}

	return 0;
}

s32 disp_unregister_sync_finish_proc(void (*proc)(u32))
{
	struct proc_list *ptr;

	DDBG("disp_unregister_sync_finish_proc (%p)\n", proc);
	if ((NULL == proc)) {
		pr_warn("hdl is NULL in %s\n", __func__);
		return -1;
	}
	list_for_each_entry(ptr, &g_disp_drv.sync_finish_proc_list.list, list) {
		if (ptr->proc == proc) {
			list_del(&ptr->list);
			disp_sys_free((void*)ptr);
			return 0;
		}
	}

	return -1;
}

static s32 disp_sync_finish_process(u32 screen_id)
{
	struct proc_list *ptr;

	list_for_each_entry(ptr, &g_disp_drv.sync_finish_proc_list.list, list) {
		if (ptr->proc)
			ptr->proc(screen_id);
	}

	return 0;
}

s32 disp_register_standby_func(int (*suspend)(void), int (*resume)(void))
{
	struct standby_cb_list *new_proc;

	new_proc = (struct standby_cb_list*)disp_sys_malloc(sizeof(struct standby_cb_list));
	if (new_proc) {
		new_proc->suspend = suspend;
		new_proc->resume = resume;
		list_add_tail(&(new_proc->list), &(g_disp_drv.stb_cb_list.list));
	} else {
		pr_warn("malloc fail in %s\n", __func__);
	}

	return 0;
}

s32 disp_unregister_standby_func(int (*suspend)(void), int (*resume)(void))
{
	struct standby_cb_list *ptr;

	list_for_each_entry(ptr, &g_disp_drv.stb_cb_list.list, list) {
		if ((ptr->suspend == suspend) && (ptr->resume == resume)) {
			list_del(&ptr->list);
			disp_sys_free((void*)ptr);
			return 0;
		}
	}

	return -1;
}

static s32 disp_suspend_cb(void)
{
	struct standby_cb_list *ptr;

	list_for_each_entry(ptr, &g_disp_drv.stb_cb_list.list, list) {
		if (ptr->suspend)
			return ptr->suspend();
	}

	return -1;
}

static s32 disp_resume_cb(void)
{
	struct standby_cb_list *ptr;

	list_for_each_entry(ptr, &g_disp_drv.stb_cb_list.list, list) {
		if (ptr->resume)
			return ptr->resume();
	}

	return -1;
}

static s32 disp_init(struct platform_device *pdev)
{
	disp_bsp_init_para *para;
	int i, disp, num_screens;
	unsigned int value, output_type, output_mode;

	DDBG("disp_init\n");

	INIT_WORK(&g_disp_drv.resume_work[0], resume_work_0);
	if (DISP_SCREEN_NUM > 1)
		INIT_WORK(&g_disp_drv.resume_work[1], resume_work_1);
	//INIT_WORK(&g_disp_drv.resume_work[2], resume_work_2);
	INIT_WORK(&g_disp_drv.start_work, start_work);
	INIT_LIST_HEAD(&g_disp_drv.sync_proc_list.list);
	INIT_LIST_HEAD(&g_disp_drv.sync_finish_proc_list.list);
	INIT_LIST_HEAD(&g_disp_drv.ioctl_extend_list.list);
	INIT_LIST_HEAD(&g_disp_drv.compat_ioctl_extend_list.list);
	INIT_LIST_HEAD(&g_disp_drv.stb_cb_list.list);
	mutex_init(&g_disp_drv.mlock);
	parser_disp_init_para(pdev->dev.of_node, &g_disp_drv.disp_init);
	para = &g_disp_drv.para;

	memset(para, 0, sizeof(disp_bsp_init_para));
	for (i=0; i<DISP_MOD_NUM; i++)	{
		para->reg_base[i] = g_disp_drv.reg_base[i];
		para->irq_no[i]   = g_disp_drv.irq_no[i];
        para->mclk[i] = g_disp_drv.mclk[i];
		DDBG("disp_init: mod %d, base=0x%lx, irq=%d, mclk=0x%p\n", i, para->reg_base[i], para->irq_no[i], para->mclk[i]);
	}

	para->disp_int_process       = disp_sync_finish_process;
	para->vsync_event            = NULL; //drv_disp_vsync_event;
	para->start_process          = start_process;

	value = disp_boot_para_parse("boot_disp");
	output_type = (value >> 8) & 0xff;
	output_mode = (value) & 0xff;
	if (output_type != (int)DISP_OUTPUT_TYPE_NONE) {
		para->boot_info.sync = 1;
		para->boot_info.disp = 0;//disp0
		para->boot_info.type = output_type;
		para->boot_info.mode = output_mode;
	} else {
		output_type = (value >> 24)& 0xff;
		output_mode = (value >> 16) & 0xff;
		if (output_type != (int)DISP_OUTPUT_TYPE_NONE) {
			para->boot_info.sync = 1;
			para->boot_info.disp = 1;//disp1
			para->boot_info.type = output_type;
			para->boot_info.mode = output_mode;
		}
	}

	if (1 == para->boot_info.sync) {
		g_disp_drv.disp_init.disp_mode = para->boot_info.disp;
		g_disp_drv.disp_init.output_type[para->boot_info.disp] = output_type;
		g_disp_drv.disp_init.output_mode[para->boot_info.disp] = output_mode;
	}

	bsp_disp_init(para);
	num_screens = bsp_disp_feat_get_num_screens();
	for (disp=0; disp<num_screens; disp++) {
		g_disp_drv.mgr[disp] = disp_get_layer_manager(disp);
	}
	lcd_init();
	bsp_disp_open();

	//fb_init(pdev);
	//composer_init(&g_disp_drv);
	g_disp_drv.inited = true;
	start_process();


	DDBG("disp_init finish\n");
	return 0;
}

static s32 disp_exit(void)
{
	//fb_exit();
	bsp_disp_close();
	bsp_disp_exit(g_disp_drv.exit_mode);
	return 0;
}

static int disp_mem_request(int sel,u32 size)
{
//#ifndef FB_RESERVED_MEM
#if 0
	unsigned map_size = 0;
	struct page *page;

	if (NULL != g_disp_mm[sel].info_base)
		return -EINVAL;

	g_disp_mm[sel].mem_len = size;
	map_size = PAGE_ALIGN(g_disp_mm[sel].mem_len);

	page = alloc_pages(GFP_KERNEL,get_order(map_size));
	if (page != NULL) {
		g_disp_mm[sel].info_base = page_address(page);
		if (NULL == g_disp_mm[sel].info_base)	{
			free_pages((unsigned long)(page),get_order(map_size));
			__wrn("page_address fail!\n");
			return -ENOMEM;
		}
		g_disp_mm[sel].mem_start = virt_to_phys(g_disp_mm[sel].info_base);
		memset(g_disp_mm[sel].info_base,0,size);

		__inf("pa=0x%p va=0x%p size:0x%x\n",(void*)g_disp_mm[sel].mem_start, g_disp_mm[sel].info_base, size);
		return 0;
	}	else {
		__wrn("alloc_pages fail!\n");
		return -ENOMEM;
	}
#else
	uintptr_t phy_addr;

	g_disp_mm[sel].info_base = disp_malloc(size, (void*)&phy_addr);
	if (g_disp_mm[sel].info_base) {
		g_disp_mm[sel].mem_start = phy_addr;
		g_disp_mm[sel].mem_len = size;
		memset(g_disp_mm[sel].info_base,0,size);
		__inf("pa=0x%p va=0x%p size:0x%x\n", (void*)g_disp_mm[sel].mem_start, g_disp_mm[sel].info_base, size);

		return 0;
	}	else {
		__wrn("disp_malloc fail!\n");
		return -ENOMEM;
	}
#endif
}

static int disp_mem_release(int sel)
{
//#ifndef FB_RESERVED_MEM
#if 0
	unsigned map_size = PAGE_ALIGN(g_disp_mm[sel].mem_len);
	unsigned page_size = map_size;

	if (NULL == g_disp_mm[sel].info_base)
		return -EINVAL;

	free_pages((unsigned long)(g_disp_mm[sel].info_base),get_order(page_size));
	memset(&g_disp_mm[sel],0,sizeof(struct info_mm));
#else
	if (g_disp_mm[sel].info_base == NULL)
		return -EINVAL;

	__inf("disp_mem_release, mem_id=%d, phy_addr=0x%p\n", sel, (void*)g_disp_mm[sel].mem_start);
	disp_free((void *)g_disp_mm[sel].info_base, (void*)g_disp_mm[sel].mem_start, g_disp_mm[sel].mem_len);
	memset(&g_disp_mm[sel],0,sizeof(struct info_mm));
#endif
  return 0;
}

int sunxi_disp_get_source_ops(struct sunxi_disp_source_ops *src_ops)
{
	memset((void *)src_ops, 0, sizeof(struct sunxi_disp_source_ops));

	src_ops->sunxi_lcd_set_panel_funs = bsp_disp_lcd_set_panel_funs;
	src_ops->sunxi_lcd_delay_ms = disp_delay_ms;
	src_ops->sunxi_lcd_delay_us = disp_delay_us;
	src_ops->sunxi_lcd_backlight_enable = bsp_disp_lcd_backlight_enable;
	src_ops->sunxi_lcd_backlight_disable = bsp_disp_lcd_backlight_disable;
	src_ops->sunxi_lcd_pwm_enable = bsp_disp_lcd_pwm_enable;
	src_ops->sunxi_lcd_pwm_disable = bsp_disp_lcd_pwm_disable;
	src_ops->sunxi_lcd_power_enable = bsp_disp_lcd_power_enable;
	src_ops->sunxi_lcd_power_disable = bsp_disp_lcd_power_disable;
	src_ops->sunxi_lcd_tcon_enable = bsp_disp_lcd_tcon_enable;
	src_ops->sunxi_lcd_tcon_disable = bsp_disp_lcd_tcon_disable;
	src_ops->sunxi_lcd_pin_cfg = bsp_disp_lcd_pin_cfg;
	src_ops->sunxi_lcd_gpio_set_value = bsp_disp_lcd_gpio_set_value;
	src_ops->sunxi_lcd_gpio_set_direction = bsp_disp_lcd_gpio_set_direction;
	src_ops->sunxi_lcd_cpu_write = tcon0_cpu_wr_16b;
	src_ops->sunxi_lcd_cpu_write_data = tcon0_cpu_wr_16b_data;
	src_ops->sunxi_lcd_cpu_write_index = tcon0_cpu_wr_16b_index;
	src_ops->sunxi_lcd_cpu_set_auto_mode = tcon0_cpu_set_auto_mode;

	return 0;
}

int disp_mmap(struct file *file, struct vm_area_struct * vma)
{
	unsigned long mypfn = vma->vm_pgoff;
	unsigned long vmsize = vma->vm_end-vma->vm_start;
	vma->vm_pgoff = 0;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (remap_pfn_range(vma,vma->vm_start,mypfn,vmsize,vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

int disp_open(struct inode *inode, struct file *file)
{
	return 0;
}

int disp_release(struct inode *inode, struct file *file)
{
	return 0;
}
ssize_t disp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

ssize_t disp_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static void disp_mclk_info(char *name, struct clk *xclk)
{
	struct clk *parent = __clk_get_parent(xclk);
	INF("[DISP] g_disp_drv.mclk[%s]={'%s' EN=%d P='%s' NP=%d rate=%lu}\n",
		name, __clk_get_name(xclk), __clk_is_enabled(xclk), 
		parent ? __clk_get_name(parent) : "NULL", __clk_get_num_parents(xclk), __clk_get_rate(xclk));
}

static int disp_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	int counter = 0;
	struct clk *xclk;

	INF("[DISP] disp_probe\n");
	memset(&g_disp_drv, 0, sizeof(disp_drv_info));

	g_disp_drv.dev = &pdev->dev;

	/* iomap */
	/* de - [device(tcon-top)] - lcd0/1/2.. - dsi */
	counter = 0;
	g_disp_drv.reg_base[DISP_MOD_DE] = (uintptr_t __force)of_iomap(pdev->dev.of_node, counter);
	if (!g_disp_drv.reg_base[DISP_MOD_DE]) {
		dev_err(&pdev->dev, "unable to map de registers\n");
		ret = -EINVAL;
		goto err_iomap;
	}
	DDBG("[DISP] g_disp_drv.reg_base[DISP_MOD_DE]=%p\n", (void *)g_disp_drv.reg_base[DISP_MOD_DE]);
	counter ++;

#if defined(HAVE_DEVICE_COMMON_MODULE)
	g_disp_drv.reg_base[DISP_MOD_DEVICE] = (uintptr_t __force)of_iomap(pdev->dev.of_node, counter);
	if (!g_disp_drv.reg_base[DISP_MOD_DEVICE]) {
		dev_err(&pdev->dev, "unable to map device common module registers\n");
		ret = -EINVAL;
		goto err_iomap;
	}
	DDBG("[DISP] g_disp_drv.reg_base[DISP_MOD_DEVICE]=%p\n", (void *)g_disp_drv.reg_base[DISP_MOD_DEVICE]);
	counter ++;
#endif

	for (i=0; i<DISP_DEVICE_NUM; i++) {
		g_disp_drv.reg_base[DISP_MOD_LCD0 + i] = (uintptr_t __force)of_iomap(pdev->dev.of_node, counter);
		if (!g_disp_drv.reg_base[DISP_MOD_LCD0 + i]) {
			dev_err(&pdev->dev, "unable to map timing controller %d registers\n", i);
			ret = -EINVAL;
			goto err_iomap;
		}
	  DDBG("[DISP] g_disp_drv.reg_base[DISP_MOD_LCD%d]=%p\n", i, (void *)g_disp_drv.reg_base[DISP_MOD_LCD0 + i]);
		counter ++;
	}


	g_disp_drv.reg_base[DISP_MOD_EINK] = (uintptr_t __force)of_iomap(pdev->dev.of_node, counter);
	if (!g_disp_drv.reg_base[DISP_MOD_EINK]) {
		dev_err(&pdev->dev, "unable to map eink registers\n");
		ret = -EINVAL;
		goto err_iomap;
	}
	DDBG("[DISP] g_disp_drv.reg_base[DISP_MOD_EINK]=%p\n", (void *)g_disp_drv.reg_base[DISP_MOD_EINK]);
	counter ++;

	/* parse and map irq */
	/* lcd0/1/2.. - dsi */
	counter = 0;
	for (i=0; i<DISP_DEVICE_NUM; i++) {
		g_disp_drv.irq_no[DISP_MOD_LCD0 + i] = irq_of_parse_and_map(pdev->dev.of_node, counter);
		if (!g_disp_drv.irq_no[DISP_MOD_LCD0 + i]) {
			dev_err(&pdev->dev, "irq_of_parse_and_map irq %d fail for timing controller%d\n", counter, i);
		}
		DDBG("[DISP] g_disp_drv.irq_no[DISP_MOD_LCD%d]=%d\n", i, g_disp_drv.irq_no[DISP_MOD_LCD0 + i]);
		counter ++;
	}

	g_disp_drv.irq_no[DISP_MOD_DE] = irq_of_parse_and_map(pdev->dev.of_node, counter);
	if (!g_disp_drv.irq_no[DISP_MOD_DE]) {
		dev_err(&pdev->dev, "irq_of_parse_and_map de irq %d fail for dsi\n", i);
	}
	DDBG("[DISP] g_disp_drv.irq_no[DISP_MOD_DE]=%d\n", g_disp_drv.irq_no[DISP_MOD_DE]);
	counter ++;

	g_disp_drv.irq_no[DISP_MOD_EINK] = irq_of_parse_and_map(pdev->dev.of_node, counter);
	if (!g_disp_drv.irq_no[DISP_MOD_EINK]) {
		dev_err(&pdev->dev, "irq_of_parse_and_map eink irq %d fail for dsi\n", i);
	}
	DDBG("[DISP] g_disp_drv.irq_no[DISP_MOD_EINK]=%d\n", g_disp_drv.irq_no[DISP_MOD_EINK]);
	counter ++;

	/* get clk */
	/* de - [device(tcon-top)] - lcd0/1/2.. - lvds - dsi */
	counter = 0;
	xclk = g_disp_drv.mclk[DISP_MOD_DE] = of_clk_get(pdev->dev.of_node, counter);
	if (IS_ERR(g_disp_drv.mclk[DISP_MOD_DE])) {
		dev_err(&pdev->dev, "fail to get clk for de\n");
	}
	disp_mclk_info("DISP_MOD_DE", xclk);
	counter ++;

#if defined(HAVE_DEVICE_COMMON_MODULE)
	xclk = g_disp_drv.mclk[DISP_MOD_DEVICE] = of_clk_get(pdev->dev.of_node, counter);
	if (IS_ERR(g_disp_drv.mclk[DISP_MOD_DEVICE])) {
		dev_err(&pdev->dev, "fail to get clk for device common module\n");
	}
	disp_mclk_info("DISP_MOD_DEVICE", xclk);
	counter ++;
#endif

	for (i=0; i<DISP_DEVICE_NUM; i++) {
		xclk = g_disp_drv.mclk[DISP_MOD_LCD0 + i] = of_clk_get(pdev->dev.of_node, counter);
		if (IS_ERR(g_disp_drv.mclk[DISP_MOD_LCD0 + i])) {
			dev_err(&pdev->dev, "fail to get clk for timing controller%d\n", i);
		}
	disp_mclk_info("DISP_MOD_LCD", xclk);
		counter ++;
	}

	xclk = g_disp_drv.mclk[DISP_MOD_EINK] = of_clk_get(pdev->dev.of_node, counter);
	if (IS_ERR(g_disp_drv.mclk[DISP_MOD_EINK])) {
		dev_err(&pdev->dev, "fail to get clk for eink\n");
	}
	disp_mclk_info("DISP_MOD_EINK", xclk);
	counter ++;

	xclk = g_disp_drv.mclk[DISP_MOD_EDMA] = of_clk_get(pdev->dev.of_node, counter);
	if (IS_ERR(g_disp_drv.mclk[DISP_MOD_EDMA])) {
		dev_err(&pdev->dev, "fail to get clk for edma\n");
	}
	disp_mclk_info("DISP_MOD_EDMA", xclk);
	counter ++;

	disp_init(pdev);
	ret = sysfs_create_group(&display_dev->kobj,
                             &disp_attribute_group);
	if (ret)
		ERR("sysfs_create_group fail!\n");

	power_status_init = 1;
#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 5000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
#endif
	INF("[DISP]disp_probe finish\n");

	return ret;

err_iomap:
	for (i=0; i<DISP_DEVICE_NUM; i++) {
		if (g_disp_drv.reg_base[i])
			iounmap((char __iomem *)g_disp_drv.reg_base[i]);
	}

	return ret;
}

static int disp_remove(struct platform_device *pdev)
{
	pr_info("disp_remove call\n");

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int disp_blank(bool blank)
{
	u32 screen_id = 0;
	int num_screens;
	struct disp_manager *mgr = NULL;

#if defined(CONFIG_DEVFREQ_DRAM_FREQ_WITH_SOFT_NOTIFY)
	/* notify dramfreq module that DE will access DRAM in a short time */
	if (!blank) {
		dramfreq_master_access(MASTER_DE, true);
	}
#endif
	num_screens = bsp_disp_feat_get_num_screens();

	for (screen_id=0; screen_id<num_screens; screen_id++) {
		mgr = g_disp_drv.mgr[screen_id];
		/* Currently remove !mgr->device condition,
		 * avoiding problem in the following case:
		 *
		 *   attach manager and device -> disp blank --> blank success
		 *   deattach manager and device -> disp unblank --> unblank fail (don't satisfy !mgr->device)
		 *   attach manager and device --> problem arises(manager is on unblank state)
		 *
		 * The real scenario is: hdmi plug in -> press power key to standy -> hdmi plug out
		 *  -> press power key to resume -> hdmi plug in -> display blank on hdmi screen
		 */
		if (!mgr)
			continue;

		if (mgr->blank)
			mgr->blank(mgr, blank);
	}

#if defined(CONFIG_DEVFREQ_DRAM_FREQ_WITH_SOFT_NOTIFY)
	/* notify dramfreq module that DE will not access DRAM any more */
	if (blank) {
		dramfreq_master_access(MASTER_DE, false);
	}
#endif

	return 0;
}

#if defined(CONFIG_PM_RUNTIME)
static int disp_runtime_suspend(struct device *dev)
{
	u32 screen_id = 0;
	int num_screens;
	struct disp_manager *mgr = NULL;
	struct disp_device* dispdev_suspend = NULL;
	struct list_head* disp_list= NULL;

	printk("%s\n", __func__);

	if (!g_pm_runtime_enable)
		return 0;

	num_screens = bsp_disp_feat_get_num_screens();

	disp_suspend_cb();
	for (screen_id=0; screen_id<num_screens; screen_id++) {
		mgr = g_disp_drv.mgr[screen_id];
		if (mgr && mgr->device) {
			struct disp_device *dispdev = mgr->device;

			if (suspend_output_type[screen_id] == DISP_OUTPUT_TYPE_LCD)
				flush_work(&g_disp_drv.resume_work[screen_id]);

			if (dispdev->is_enabled(dispdev))
				dispdev->disable(dispdev);
		}
	}

	disp_list = disp_device_get_list_head();
	list_for_each_entry(dispdev_suspend, disp_list, list) {
		if (dispdev_suspend->suspend) {
			dispdev_suspend->suspend(dispdev_suspend);
		}
	}

	suspend_status |= DISPLAY_LIGHT_SLEEP;
	suspend_prestep = 0;

	pr_info("%s finish\n", __func__);

	return 0;
}

static int disp_runtime_resume(struct device *dev)
{
	u32 screen_id = 0;
	int num_screens;
	struct disp_manager *mgr = NULL;
	struct disp_device* dispdev = NULL;
	struct list_head* disp_list= NULL;

	printk("%s\n", __func__);
	if (!g_pm_runtime_enable)
		return 0;

	num_screens = bsp_disp_feat_get_num_screens();

	disp_list = disp_device_get_list_head();
	list_for_each_entry(dispdev, disp_list, list) {
		if (dispdev->resume) {
			dispdev->resume(dispdev);
		}
	}

	for (screen_id=0; screen_id<num_screens; screen_id++) {
		mgr = g_disp_drv.mgr[screen_id];
		if (!mgr || !mgr->device)
			continue;

		if (suspend_output_type[screen_id] == DISP_OUTPUT_TYPE_LCD) {
			flush_work(&g_disp_drv.resume_work[screen_id]);
			if (!mgr->device->is_enabled(mgr->device)) {
				mgr->device->enable(mgr->device);
			} else {
				mgr->device->pwm_enable(mgr->device);
				mgr->device->backlight_enable(mgr->device);
			}
		} else if (suspend_output_type[screen_id] != DISP_OUTPUT_TYPE_NONE) {
			if (mgr->device->set_mode && mgr->device->get_mode) {
					u32 mode = mgr->device->get_mode(mgr->device);

					mgr->device->set_mode(mgr->device, mode);
			}
			if (!mgr->device->is_enabled(mgr->device))
				mgr->device->enable(mgr->device);
		}
	}

	suspend_status &= (~DISPLAY_LIGHT_SLEEP);
	suspend_prestep = 3;

	disp_resume_cb();

	pr_info("%s finish\n", __func__);

	return 0;
}

static int disp_runtime_idle(struct device *dev)
{
	pr_info("%s\n", __func__);

	if (g_disp_drv.dev) {
		pm_runtime_mark_last_busy(g_disp_drv.dev);
		pm_request_autosuspend(g_disp_drv.dev);
	} else {
		pr_warn("%s, display device is null\n", __func__);
	}

	/* return 0: for framework to request enter suspend.
		return non-zero: do susupend for myself;
	*/
	return -1;
}
#endif

static int disp_suspend(struct device *dev)
{
	u32 screen_id = 0;
	int num_screens;
	struct disp_manager *mgr = NULL;
	struct disp_device* dispdev_suspend = NULL;
	struct list_head* disp_list= NULL;

	pr_info("%s\n", __func__);

	if (!g_disp_drv.dev) {
		pr_warn("display device is null!\n");
		return 0;
	}
#if defined(CONFIG_PM_RUNTIME)
	if (!pm_runtime_status_suspended(g_disp_drv.dev))
#endif
	{
		num_screens = bsp_disp_feat_get_num_screens();
		disp_suspend_cb();
		if (g_pm_runtime_enable) {

			for (screen_id=0; screen_id<num_screens; screen_id++) {
				mgr = g_disp_drv.mgr[screen_id];
				if (!mgr || !mgr->device)
					continue;

				if (suspend_output_type[screen_id] == DISP_OUTPUT_TYPE_LCD)
					flush_work(&g_disp_drv.resume_work[screen_id]);
				if (suspend_output_type[screen_id] != DISP_OUTPUT_TYPE_NONE) {
						if (mgr->device->is_enabled(mgr->device))
							mgr->device->disable(mgr->device);
				}
			}
		}
		else {
			for (screen_id=0; screen_id<num_screens; screen_id++) {
				mgr = g_disp_drv.mgr[screen_id];
				if (!mgr || !mgr->device)
					continue;

				if (suspend_output_type[screen_id] != DISP_OUTPUT_TYPE_NONE) {
					if (mgr->device->is_enabled(mgr->device))
						mgr->device->disable(mgr->device);
				}
			}
		}

		/*suspend for all display device*/
		disp_list = disp_device_get_list_head();
		list_for_each_entry(dispdev_suspend, disp_list, list) {
			if (dispdev_suspend->suspend) {
				dispdev_suspend->suspend(dispdev_suspend);
			}
		}
	}

	//FIXME: hdmi suspend
	suspend_status |= DISPLAY_DEEP_SLEEP;
	suspend_prestep = 1;
#if defined(CONFIG_PM_RUNTIME)
	if (g_pm_runtime_enable) {
		pm_runtime_disable(g_disp_drv.dev);
		pm_runtime_set_suspended(g_disp_drv.dev);
		pm_runtime_enable(g_disp_drv.dev);
	}
#endif
	pr_info("%s finish\n", __func__);

	//return epdc_suspend();
	return 0;
}

static int disp_resume(struct device *dev)
{
	u32 screen_id = 0;
	int num_screens = bsp_disp_feat_get_num_screens();
	struct disp_manager *mgr = NULL;

#if defined(CONFIG_PM_RUNTIME)
	struct disp_device* dispdev = NULL;
	struct list_head* disp_list= NULL;

	disp_list = disp_device_get_list_head();
	list_for_each_entry(dispdev, disp_list, list) {
		if (dispdev->resume) {
			dispdev->resume(dispdev);
		}
	}
	if (g_pm_runtime_enable) {
		for (screen_id=0; screen_id<num_screens; screen_id++) {
			mgr = g_disp_drv.mgr[screen_id];
			if (!mgr || !mgr->device)
				continue;

			if (suspend_output_type[screen_id] == DISP_OUTPUT_TYPE_LCD) {
				schedule_work(&g_disp_drv.resume_work[screen_id]);
			}
		}
		if (g_pm_runtime_enable) {
			if (g_disp_drv.dev) {
				pm_runtime_disable(g_disp_drv.dev);
				pm_runtime_set_active(g_disp_drv.dev);
				pm_runtime_enable(g_disp_drv.dev);
			} else {
				pr_warn("%s, display device is null\n", __func__);
			}
		}
	}
	else {
		for (screen_id=0; screen_id<num_screens; screen_id++) {
			mgr = g_disp_drv.mgr[screen_id];
			if (!mgr || !mgr->device)
				continue;

			if (suspend_output_type[screen_id] != DISP_OUTPUT_TYPE_NONE) {
				if (mgr->device->set_mode && mgr->device->get_mode) {
						u32 mode = mgr->device->get_mode(mgr->device);

						mgr->device->set_mode(mgr->device, mode);
				}
				if (!mgr->device->is_enabled(mgr->device)) {
					mgr->device->enable(mgr->device);
				}
			}
		}
		disp_resume_cb();
	}
#else
	struct disp_device* dispdev = NULL;
	struct list_head* disp_list= NULL;

	disp_list = disp_device_get_list_head();
	list_for_each_entry(dispdev, disp_list, list) {
		if (dispdev->resume) {
			dispdev->resume(dispdev);
		}
	}

	for (screen_id=0; screen_id<num_screens; screen_id++) {
		mgr = g_disp_drv.mgr[screen_id];
		if (!mgr || !mgr->device)
			continue;

		if (suspend_output_type[screen_id] != DISP_OUTPUT_TYPE_NONE) {
			if (mgr->device->set_mode && mgr->device->get_mode) {
					u32 mode = mgr->device->get_mode(mgr->device);

					mgr->device->set_mode(mgr->device, mode);
			}
			
			mgr->device->enable(mgr->device);
		}
	}
	disp_resume_cb();
#endif

	suspend_status &= (~DISPLAY_DEEP_SLEEP);
	suspend_prestep = 2;
	//epdc_resume();
	pr_info("%s finish\n", __func__);

	return 0;
}

static const struct dev_pm_ops disp_runtime_pm_ops =
{
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = disp_runtime_suspend,
	.runtime_resume  = disp_runtime_resume,
	.runtime_idle    = disp_runtime_idle,
#endif
	.suspend  = disp_suspend,
	.resume   = disp_resume,
};

static void disp_shutdown(struct platform_device *pdev)
{
	u32 screen_id = 0;
	int num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	for (screen_id=0; screen_id<num_screens; screen_id++) {
		struct disp_manager *mgr = g_disp_drv.mgr[screen_id];

		if (mgr && mgr->device && mgr->device->is_enabled && mgr->device->disable) {
			if (mgr->device->is_enabled(mgr->device)) {
				mgr->device->disable(mgr->device);
			}
		}
	}

	return ;
}


static const struct file_operations disp_fops = {
	.owner    = THIS_MODULE,
	.open     = disp_open,
	.release  = disp_release,
	.write    = disp_write,
	.read     = disp_read,
	.mmap     = disp_mmap,
};

#ifndef CONFIG_OF
static struct platform_device disp_device = {
	.name           = "disp",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(disp_resource),
	.resource       = disp_resource,
	.dev            =
	{
		.power        =
		{
			.async_suspend = 1,
		}
	}
};
#else
static const struct of_device_id sunxi_disp_match[] = {
	{ .compatible = "allwinner,sun8iw10p1-disp", },
	{ .compatible = "allwinner,sun50i-disp", },
	{ .compatible = "allwinner,sunxi-disp", },
	{},
};
#endif

static struct platform_driver disp_driver = {
	.probe    = disp_probe,
	.remove   = disp_remove,
	.shutdown = disp_shutdown,
	.driver   =
	{
		.name   = "disp",
		.owner  = THIS_MODULE,
		.pm	= &disp_runtime_pm_ops,
		.of_match_table = sunxi_disp_match,
	},
};

#ifdef CONFIG_DEVFREQ_DRAM_FREQ_IN_VSYNC
struct dramfreq_vb_time_ops
{
    int (*get_vb_time) (void);
    int (*get_next_vb_time) (void);
    int (*is_in_vb)(void);
};
extern s32 bsp_disp_get_vb_time(void);
extern s32 bsp_disp_get_next_vb_time(void);
extern int dramfreq_set_vb_time_ops(struct dramfreq_vb_time_ops *ops);
static struct dramfreq_vb_time_ops dramfreq_ops =
{
	.get_vb_time = bsp_disp_get_vb_time,
	.get_next_vb_time = bsp_disp_get_next_vb_time,
	.is_in_vb = bsp_disp_is_in_vb,
};
#endif

extern int disp_attr_node_init(void);
extern int capture_module_init(void);
extern void  capture_module_exit(void);
static int __init disp_module_init(void)
{
	int ret = 0, err;

	DDBG("[DISP] %s\n", __func__);

	alloc_chrdev_region(&devid, 0, 1, "disp");
	my_cdev = cdev_alloc();
	cdev_init(my_cdev, &disp_fops);
	my_cdev->owner = THIS_MODULE;
	err = cdev_add(my_cdev, devid, 1);
	if (err) {
		__wrn("cdev_add fail\n");
		return -1;
	}

	disp_class = class_create(THIS_MODULE, "disp");
	if (IS_ERR(disp_class))	{
		__wrn("class_create fail\n");
		return -1;
	}

	display_dev = device_create(disp_class, NULL, devid, NULL, "disp");

#ifndef CONFIG_OF
	ret = platform_device_register(&disp_device);
#endif
	if (ret == 0) {
		ret = platform_driver_register(&disp_driver);
	}
#ifdef CONFIG_DISP2_SUNXI_DEBUG
	dispdbg_init();
#endif

#ifdef CONFIG_DEVFREQ_DRAM_FREQ_IN_VSYNC
	dramfreq_set_vb_time_ops(&dramfreq_ops);
#endif

	pr_info("[DISP]%s finish\n", __func__);

	return ret;
}

static void __exit disp_module_exit(void)
{
	__inf("disp_module_exit\n");

#ifdef CONFIG_DISP2_SUNXI_DEBUG
	dispdbg_exit();
#endif

	disp_exit();

	platform_driver_unregister(&disp_driver);
#ifndef CONFIG_OF
	platform_device_unregister(&disp_device);
#endif

	device_destroy(disp_class,  devid);
	class_destroy(disp_class);

	cdev_del(my_cdev);
}

//EXPORT_SYMBOL(sunxi_disp_get_source_ops);
EXPORT_SYMBOL(disp_module_init);

module_init(disp_module_init);
module_exit(disp_module_exit);

MODULE_AUTHOR("tyle");
MODULE_DESCRIPTION("display driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:disp");


