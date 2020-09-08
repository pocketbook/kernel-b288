/* linux/drivers/video/sunxi/disp2/disp/dev_fb.c
 *
 * Copyright (c) 2013 Allwinnertech Co., Ltd.
 * Author: Tyle <tyle@allwinnertech.com>
 *
 * Framebuffer driver for sunxi platform
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "dev_disp.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include "eink.h"

typedef struct
{
	struct device           *dev;

	bool                    fb_enable[FB_MAX];
	enum disp_fb_mode       fb_mode[FB_MAX];
	u32                     layer_hdl[FB_MAX][2];//channel, layer_id
	struct fb_info *        fbinfo[FB_MAX];
	struct disp_fb_create_info     fb_para[FB_MAX];
	u32                     pseudo_palette [FB_MAX][16];
	wait_queue_head_t       wait[3];
	unsigned long           wait_count[3];
	struct task_struct      *vsync_task[3];
	ktime_t                 vsync_timestamp[3];

	int                     blank[3];
}fb_info_t;

static fb_info_t g_fbi;

extern disp_drv_info g_disp_drv;

static int epdc_panel_width, epdc_panel_height;

static void *framebuffer_memory, *shadow_memory;

#define FBHANDTOID(handle)  ((handle) - 100)
#define FBIDTOHAND(ID)  ((ID) + 100)

static struct __fb_addr_para g_fb_addr;

s32 sunxi_get_fb_addr_para(struct __fb_addr_para *fb_addr_para)
{
	if (fb_addr_para){
		fb_addr_para->fb_paddr = g_fb_addr.fb_paddr;
		fb_addr_para->fb_size  = g_fb_addr.fb_size;
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL(sunxi_get_fb_addr_para);

/*
static int fb_map_video_memory(struct fb_info *info)
{
	info->screen_base = (char __iomem *)disp_malloc(info->fix.smem_len, (u32 *)(&info->fix.smem_start));
	if (info->screen_base)	{
		DDBG("fb_map_video_memory: %p/%x size:%d\n", info->screen_base, info->fix.smem_start, info->fix.smem_len);
		memset((void* __force)info->screen_base, 0xff, info->fix.smem_len);

		g_fb_addr.fb_paddr = (uintptr_t)info->fix.smem_start;
		g_fb_addr.fb_size = info->fix.smem_len;

		return 0;
	} else {
		DDBG("fb_map_video_memory: malloc fail!\n");
		return -ENOMEM;
	}

	return 0;
}
*/

static inline void fb_unmap_video_memory(struct fb_info *info)
{
	disp_free((void * __force)info->screen_base, (void*)info->fix.smem_start, info->fix.smem_len);
}

static void update_fb_parameters(struct fb_info *fbinfo)
{
    fbinfo->fix.type = FB_TYPE_PACKED_PIXELS;
    fbinfo->fix.type_aux = 0;
    fbinfo->fix.visual = FB_VISUAL_TRUECOLOR;
    fbinfo->fix.xpanstep = 1;
    fbinfo->fix.ypanstep = 1;
    fbinfo->fix.ywrapstep = 0;
    fbinfo->fix.accel = FB_ACCEL_NONE;
    fbinfo->var.nonstd = 0;
    fbinfo->var.bits_per_pixel = 8;
    fbinfo->var.transp.length = 0;
    fbinfo->var.red.length = 3;
    fbinfo->var.green.length = 3;
    fbinfo->var.blue.length = 2;
    fbinfo->var.transp.offset = 0;
    fbinfo->var.red.offset = 5;
    fbinfo->var.green.offset = 2;
    fbinfo->var.blue.offset = 0;
    fbinfo->var.activate = FB_ACTIVATE_FORCE;

    switch (fbinfo->var.rotate) {
      case FB_ROTATE_UR:
      case FB_ROTATE_UD:
        fbinfo->var.xres = epdc_panel_width;
        fbinfo->var.yres = epdc_panel_height;
        fbinfo->var.xres_virtual = epdc_panel_width;
        fbinfo->var.yres_virtual = epdc_panel_height;
        break;
      case FB_ROTATE_CW:
      case FB_ROTATE_CCW:
        fbinfo->var.xres = epdc_panel_height;
        fbinfo->var.yres = epdc_panel_width;
        fbinfo->var.xres_virtual = epdc_panel_height;
        fbinfo->var.yres_virtual = epdc_panel_width;
        break;
    }
    fbinfo->fix.line_length = FB_ALIGN(fbinfo->var.xres_virtual);

    framebuffer_slot.width = shadow_slot.width = fbinfo->var.xres;
    framebuffer_slot.height = shadow_slot.height = fbinfo->var.yres;
    framebuffer_slot.scanline = shadow_slot.scanline = fbinfo->fix.line_length;
    framebuffer_slot.orientation = shadow_slot.orientation = fbinfo->var.rotate;
    framebuffer_slot.memsize = shadow_slot.memsize = fbinfo->fix.smem_len;

    framebuffer_slot.paddr = fbinfo->fix.smem_start = virt_to_phys(framebuffer_memory);
    framebuffer_slot.vaddr = fbinfo->screen_base = framebuffer_memory;

    shadow_slot.paddr = virt_to_phys(shadow_memory);
    shadow_slot.vaddr = shadow_memory;

		DBG("** framebuffer: %p/%x %dx%d scan=%d orientation=%d pid=%d\n",
			framebuffer_slot.vaddr, framebuffer_slot.paddr,
			framebuffer_slot.width, framebuffer_slot.height,
			framebuffer_slot.scanline, framebuffer_slot.orientation,
			current->pid);
}


static int sunxi_fb_open(struct fb_info *info, int user)
{
	return 0;
}
static int sunxi_fb_release(struct fb_info *info, int user)
{
	return 0;
}

static int sunxi_fb_pan_display(struct fb_var_screeninfo *var,struct fb_info *info)
{
	return 0;
}

static int sunxi_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	return 0;
}

static int sunxi_fb_set_par(struct fb_info *info)
{
	update_fb_parameters(info);
	return 0;
}

void DRV_disp_int_process(u32 sel)
{
	g_fbi.wait_count[sel]++;
	wake_up_interruptible(&g_fbi.wait[sel]);

	return ;
}

static inline u32 convert_bitfield(int val, struct fb_bitfield *bf)
{
	u32 mask = ((1 << bf->length) - 1) << bf->offset;
	return (val << bf->offset) & mask;
}

static int sunxi_fb_setcolreg(unsigned regno, unsigned red, unsigned green,
			    unsigned blue, unsigned transp, struct fb_info *info)
{
	u32 val;
	u32 ret = 0;
#if 0
	switch (info->fix.visual) {
	case FB_VISUAL_PSEUDOCOLOR:
		ret = -EINVAL;
		break;
	case FB_VISUAL_TRUECOLOR:
		if (regno < 16) {
			val = convert_bitfield(transp, &info->var.transp) |
				convert_bitfield(red, &info->var.red) |
				convert_bitfield(green, &info->var.green) |
				convert_bitfield(blue, &info->var.blue);
			__inf("fb_setcolreg,regno=%2d,a=%2X,r=%2X,g=%2X,b=%2X, "
					"result=%08X\n", regno, transp, red, green, blue,
					val);
			((u32 *) info->pseudo_palette)[regno] = val;
		} else {
			ret = 0;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
#endif
	return ret;
}

static int sunxi_fb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
#if 0
	unsigned int j, r = 0;
	unsigned char hred, hgreen, hblue, htransp = 0xff;
	unsigned short *red, *green, *blue, *transp;

	__inf("fb_setcmap, cmap start:%d len:%d, %dbpp\n", cmap->start,
			cmap->len, info->var.bits_per_pixel);

	red = cmap->red;
	green = cmap->green;
	blue = cmap->blue;
	transp = cmap->transp;

	for (j = 0; j < cmap->len; j++) {
		hred = *red++;
		hgreen = *green++;
		hblue = *blue++;
		if (transp)
			htransp = (*transp++) & 0xff;
		else
			htransp = 0xff;

		r = sunxi_fb_setcolreg(cmap->start + j, hred, hgreen, hblue, htransp,
				info);
		if (r)
			return r;
	}
#endif
	return 0;
}


static int sunxi_fb_ioctl(struct fb_info *info, unsigned int cmd,unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	long ret = 0;

	switch (cmd) {

	case EPDC_SEND_UPDATE:
	{
		struct epdc_update_data upd;
		struct area_info area;
		uint8_t *fb_addr;
		int wf, rotate;

		if (copy_from_user(&upd, argp, sizeof(upd))) {
			ret = -EFAULT;
			break;
		}
		fb_addr = g_fbi.fbinfo[0]->screen_base;
		rotate = g_fbi.fbinfo[0]->var.rotate;
		area.x_top = upd.update_region.left;
		area.y_top = upd.update_region.top;
		area.x_bottom = upd.update_region.left + upd.update_region.width - 1;
		area.y_bottom = upd.update_region.top + upd.update_region.height - 1;
		area.wf = upd.waveform_mode;
		area.mode = upd.update_mode;
		eink_update_image(&area, &upd.flags);
		if (copy_to_user(argp, &upd, sizeof(upd))) {
			ret = -EFAULT;
		}
		break;
	}

	case EPDC_GET_WAVEFORM_HEADER:
	{
		break;
	}

	case EPDC_WAIT_FOR_UPDATE_COMPLETE:
	{
		eink_wait_complete();
		break;
	}

	case EPDC_GET_UPDATE_STATE:
	{
		int state = eink_get_update_state();
		if (copy_to_user(argp,&state,sizeof(state))) {
        ret = -EFAULT;
		}
	}

	default:
		break;
	}
	return ret;
}

static struct fb_ops dispfb_ops =
{
	.owner		    = THIS_MODULE,
	.fb_open        = sunxi_fb_open,
	.fb_release     = sunxi_fb_release,
	.fb_pan_display	= sunxi_fb_pan_display,
	.fb_ioctl       = sunxi_fb_ioctl,
	.fb_check_var   = sunxi_fb_check_var,
	.fb_set_par     = sunxi_fb_set_par,
#if defined(CONFIG_FB_CONSOLE_SUNXI)
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
#endif
	.fb_setcmap     = sunxi_fb_setcmap,
	.fb_setcolreg	= sunxi_fb_setcolreg,

};

unsigned long fb_get_address_info(u32 fb_id, u32 phy_virt_flag)
{
	struct fb_info *info = NULL;
	unsigned long phy_addr = 0;
	unsigned long virt_addr = 0;

	if (fb_id >= FB_MAX) {
		return 0;
	}

	info = g_fbi.fbinfo[fb_id];
	phy_addr = info->fix.smem_start;
	virt_addr = (unsigned long)info->screen_base;

	if (0 == phy_virt_flag) {
		//get virtual address
		return virt_addr;
	} else {
		//get phy address
		return phy_addr;
	}
}

unsigned long fb_get_phyaddress_info(u32 fb_id) 
{
	struct fb_info *info = NULL;
	unsigned long phy_addr = 0;
	
	if (fb_id >= FB_MAX) {
		return 0;
	}

	info = g_fbi.fbinfo[fb_id];
	phy_addr = info->fix.smem_start + info->var.yoffset*info->fix.line_length;
	
	
	return phy_addr;

}

s32 fb_init(struct platform_device *pdev)
{
	struct disp_fb_create_info fb_para;
	u32 num_screens;
	int memsize;
	struct fb_info *info;
	//struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	g_fbi.dev = &pdev->dev;
	num_screens = bsp_disp_feat_get_num_screens();

	DDBG("fb_init: num_screens=%d\n", num_screens);

	init_waitqueue_head(&g_fbi.wait[0]);
	init_waitqueue_head(&g_fbi.wait[1]);
	init_waitqueue_head(&g_fbi.wait[2]);
	disp_register_sync_finish_proc(DRV_disp_int_process);

	eink_get_size(&epdc_panel_width, &epdc_panel_height);
	memsize = PAGE_ALIGN(FB_ALIGN(epdc_panel_width) * FB_ALIGN(epdc_panel_height));
	INF("allocating framebuffer(%d,%d) memory=%d\n", epdc_panel_width, epdc_panel_height, memsize);

	framebuffer_memory = (char __iomem *) alloc_pages_exact(memsize, __GFP_ZERO);
	shadow_memory = (char __iomem *) alloc_pages_exact(memsize, __GFP_ZERO);
	if (framebuffer_memory == NULL || shadow_memory == NULL)	{
		ERR("fb_init: malloc fail!\n");
		return -ENOMEM;
	}

	memset(framebuffer_memory, 0xff, memsize);
	memset(shadow_memory, 0xff, memsize);

	info = framebuffer_alloc(0, g_fbi.dev);
	info->fbops   = &dispfb_ops;
	info->flags   = 0;
	info->device  = g_fbi.dev;
	info->par     = &g_fbi;
	info->var.xoffset         = 0;
	info->var.yoffset         = 0;
	info->screen_base = NULL;
	info->pseudo_palette = g_fbi.pseudo_palette[0];
	info->fix.smem_start = 0x0;
	info->fix.smem_len = memsize;
	info->fix.mmio_start = 0;
	info->fix.mmio_len = 0;
	info->var.rotate = 1;

	g_fb_addr.fb_paddr = (uintptr_t)info->fix.smem_start;
	g_fb_addr.fb_size = info->fix.smem_len;

	if (fb_alloc_cmap(&info->cmap, 256, 1) < 0) {
		return -ENOMEM;
	}

	g_fbi.fbinfo[0] = info;
	update_fb_parameters(info);
	register_framebuffer(info);

	epdc_sysfs_create_groups(&pdev->dev.kobj);

	return 0;
}

s32 fb_exit(void)
{
	return 0;
}


