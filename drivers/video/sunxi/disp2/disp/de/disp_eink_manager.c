/*
 *  Copyright (C) 2015 Allwinnertech, z.q <zengqi@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include "../eink.h"
#include "axi.h"

#include "disp_private.h"

#ifdef SUPPORT_EINK
#include <linux/sched.h>
#include <linux/power_supply.h>

extern struct epdc_pmic_driver epdc_pmic_driver_tps65185;

int epdc_debuglevel = DEFAULT_DEBUGLEVEL;

struct eink_init_param eink_param;   // parameters read from sysconfig
struct epdc_pmic_driver *pmic;

static bool                  enable_flag;
static struct tasklet_struct sync_tasklet;
static spinlock_t            eelock;
static spinlock_t            tconlock;
static struct mutex          update_lock;

static DECLARE_WAIT_QUEUE_HEAD(decode_wq);
static DECLARE_WAIT_QUEUE_HEAD(decode_finish_wq);


static struct task_struct    *decode_task;
static struct task_struct    *bootlogo_task;

static struct clk            *eink_clk;
static struct clk            *edma_clk;
static unsigned long         eink_base_addr;
static unsigned int          irq_no;

static int                   fresh_frame_index;
static int                   decode_frame_index;
static int                   total_frame_index;

static int                   first_enable = 1;
static int                   decode_finished = 0;
static int                   waveform_loaded = 0;
static int                   epdc_initialized = 0;
static int                   tcon_is_opened = 0;
static int                   sm_counter = 0;
static int                   in_update = false;
static int                   logo_showing = 0;
static int                   decode_thread_active = 0;

struct index_buffer indexmap;

static phys_addr_t edma_paddr = 0;

struct eink_image_slot framebuffer_slot; // framebuffer memory represented as image slot
struct eink_image_slot shadow_slot;      // framebuffer memory represented as image slot

static volatile int suspend;
volatile int diplay_pre_finish_flag;
static int wave_data_offset;

extern s32 tcon0_simple_close(u32 sel);
extern s32 tcon0_simple_open(u32 sel);

static int write_edma_first(void);
static int write_edma(void);

static void ee_request_release(u32 req, u32 rel);

#define ee_request(n) ee_request_release(n, 0)
#define ee_release(n) ee_request_release(0, n)

int eink_ee_interrupt(int irq, void *parg);
int eink_tcon_interrupt(void);

static int eink_clk_disable(void)
{
	if (eink_clk) clk_disable(eink_clk);
	if (edma_clk) clk_disable(edma_clk);
	DBG("eink_clk_disable\n");
	return 0;
}

static int eink_clk_enable(void)
{
	int ret = 0;
	if (eink_clk && ret == 0) ret = clk_prepare_enable(eink_clk);
	if (edma_clk && ret == 0) ret = clk_prepare_enable(edma_clk);
	DBG("eink_clk_enable: %d\n", ret);
	return ret;
}

static s32 eink_enable(void)
{
	int ret = 0;

	if (enable_flag) return 0;
	enable_flag = 1;
	suspend = 0;

	DBG("eink_enable\n");

	/* enable eink clk*/
	if (first_enable) eink_clk_enable();

	/* init eink and edma*/
	ret = disp_al_eink_config(0, &eink_param);
	ret = disp_al_edma_init(0, &eink_param);

	/*load waveform data,do only once.*/
	if (first_enable) {
		/* register eink irq */
		disp_sys_register_irq(irq_no, 0, eink_ee_interrupt, NULL, 0, 0);
		disp_sys_enable_irq(irq_no);
		first_enable = 0;
	}

	disp_al_eink_irq_enable(0);

	return ret;
}

static s32 eink_disable(void)
{
	if (! enable_flag) return 0;
	DBG("eink_disable\n");
	enable_flag = false;
	disp_al_eink_irq_disable(0);
	/* disable eink engine. */
	disp_al_eink_disable(0);
	/* disable clk move to when display finish. */
	/* ret = eink_clk_disable(manager); */
	return 0;
}

static void eink_tcon_open(void)
{
	unsigned long flags = 0;
	struct disp_device *plcd = NULL;

	spin_lock_irqsave(&tconlock, flags);
	if (! tcon_is_opened) {
		DBG("tcon:open\n");
		write_edma_first();
		/* use lcd_enable now, reserve lcd simple open method. */
		plcd = disp_device_find(0, DISP_OUTPUT_TYPE_LCD);
		diplay_pre_finish_flag = 0;
		plcd->enable(plcd);
		epdc_hw_open();
		tcon_is_opened = 1;
	}
	spin_unlock_irqrestore(&tconlock, flags);
}

static void eink_tcon_close(void)
{
	unsigned long flags = 0;
	struct disp_device *plcd = NULL;

	spin_lock_irqsave(&tconlock, flags);
	if (tcon_is_opened) {
		DBG("tcon:close\n");
		clear_wavedata_buffer();
		diplay_pre_finish_flag = 1;
		epdc_hw_close();
		plcd = disp_device_find(0, DISP_OUTPUT_TYPE_LCD);
		schedule_work(&plcd->close_eink_panel_work);
		tcon_is_opened = 0;
	}
	spin_unlock_irqrestore(&tconlock, flags);
}

int eink_ee_interrupt(int irq, void *parg)
{
	switch (disp_al_eink_irq_query(0)) {
		case 0:
			decode_finished = 1;
			wake_up(&decode_finish_wq);
			break;
		case 1:
			FDBG("*IRQ_INDEX*\n");
			break;
	}
	return DISP_IRQ_RETURN;
}

int eink_tcon_interrupt(void)
{
	int ret =0;

	FDBG("*TCON(%d/%d/%d)\n", fresh_frame_index, decode_frame_index, total_frame_index);
	if (fresh_frame_index < total_frame_index) {
		tasklet_schedule(&sync_tasklet);
	} else if (fresh_frame_index >= total_frame_index) {
		eink_tcon_close();
	}
	return ret;
}

static void eink_sync_task(unsigned long data)
{
	int cur_line = 0;
	static int start_delay = 0;

	start_delay = disp_al_lcd_get_start_delay(0, NULL);
	//tcon_flag = 0;
	cur_line = disp_al_lcd_get_cur_line(0, NULL);
	if (edma_paddr != epdc_wavering_get_empty()) clean_used_wavedata_buffer();

	//FDBG("sync: cur=%d start=%d\n", cur_line, start_delay);
	while (cur_line < start_delay && !diplay_pre_finish_flag) {
		cur_line = disp_al_lcd_get_cur_line(0, NULL);
	}
	write_edma();

	if (decode_frame_index < total_frame_index) wake_up(&decode_wq);
}

static void delay_wakeup_by_new_update(int ms)
{
	if (ms <= 0) return;
	if (ms < 10) {
			mdelay(ms);
	} else {
			wait_event_timeout(decode_wq, decode_frame_index < total_frame_index, msecs_to_jiffies(ms));
	}
}

static void dump_update(struct eink_image_slot *slot, struct area_info *update_area, uint32_t flags)
{
	#define BMHSIZE (14+40+4*256)
	#define U2(v) ((v)&0xff),(((v)>>8)&0xff)
	#define U4(v) ((v)&0xff),(((v)>>8)&0xff),(((v)>>16)&0xff),(((v)>>24)&0xff)

	static uint8_t bmheader[BMHSIZE] = {
		'B','M',U4(0),U4(0),U4(BMHSIZE),U4(40),U4(0),U4(0),
		U2(1),U2(8),U4(0),U4(0),U4(0),U4(0),U4(256),U4(256)
	};

	char filename[120];
	struct file *fp;
	mm_segment_t old_fs;
	struct timespec ts;
	loff_t pos = 0;
	int i, y;

	int w = update_area->x_bottom + 1 - update_area->x_top;
	int h = update_area->y_bottom + 1 - update_area->y_top;
	int scan = (w + 3) & ~3;
	int datasize = scan * h;

	get_monotonic_boottime(&ts);
	sprintf(filename, "/tmp/%04d.%03d_%d_%d.%d_%d.%d_%d%c_%08x.bmp",
		(int)ts.tv_sec, (int)ts.tv_nsec / 1000000,
		current->pid,
		update_area->x_top, update_area->y_top, w, h,
		update_area->wf, update_area->mode ? 'F' : 'P',
		flags
	);
	fp = filp_open(filename, O_WRONLY|O_CREAT, 0644);
	if(IS_ERR(fp)) return;
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	*((uint16_t *)(bmheader+0x2)) = BMHSIZE + datasize; 
	*((uint16_t *)(bmheader+0x12)) = w; 
	*((uint16_t *)(bmheader+0x16)) = h; 
	for (i=0; i<256; i++) {
		*(bmheader + 14 + 40 + i*4 + 0) = i;
		*(bmheader + 14 + 40 + i*4 + 1) = i;
		*(bmheader + 14 + 40 + i*4 + 2) = i;
	}
	vfs_write(fp, bmheader, BMHSIZE, &pos);
	for (y=update_area->y_bottom; y>=update_area->y_top; y--) {
		uint8_t *p = slot->vaddr + y * slot->scanline + update_area->x_top;
		vfs_write(fp, p, scan, &pos);
	}

	filp_close(fp, NULL);
	set_fs(old_fs);
}

static int epdc_decode_thread(void *p_arg)
{
	int vdd_on=0, vhigh_on=0, vcom_on=0;

	uint32_t status = 0;
	struct area_info area, upd_bounds;
	int poweroff_delay;

	phys_addr_t wavedata_paddr = 0;

	while (1) {

		if (wait_event_timeout(decode_wq, 
			(decode_frame_index < total_frame_index && (wavedata_paddr = request_buffer_for_decode()) != 0),
			HZ * 60
		) <= 0) {
			continue;
		}

		decode_thread_active = 1;

		if (! vdd_on) {
			pmic->epdc_pmic_wakeup();
			pmic->epdc_pmic_vdd(1);
			epdc_hw_pin_setup(LCD_PIN_IDLE);
			vdd_on = 1;
		}

		if (! vhigh_on) {
			pmic->epdc_pmic_temperature(&eink_param.temperature);
			DBG("temperature: %d\n", eink_param.temperature);
			pmic->epdc_pmic_vhigh(1);
			vhigh_on = 1;
		}

		if (! vcom_on) {

			eink_param.vcom_offset = 0; // wfdata.vcomoffset[1][eink_param.tempindex]; // TODO
			pmic->epdc_pmic_vcom_set(eink_param.vcom, eink_param.vcom_offset, 0);

			pmic->epdc_pmic_vcom(1);
			vcom_on = 1;
		}

		FDBG("+decode(%d/%d) i=%x w=%x\n", decode_frame_index, total_frame_index, indexmap.paddr, wavedata_paddr);
		eink_enable();
		decode_finished = 0;
		wave_data_offset = disp_al_eink_start_decode(0, indexmap.paddr, wavedata_paddr, &eink_param);
		if (wait_event_timeout(decode_finish_wq, decode_finished, HZ/10) <= 0) {
			ERR("!decode error!\n");
			continue;
		}
		queue_wavedata_buffer();
		status = ppm_update_pipeline_list(&upd_bounds);
		decode_frame_index++;

		if (status & PPM_REMOVE) {
			// process collisions
			while (ppm_free_list_status() >= 4 && axi_collision_fetch(&area)) {
				uint32_t epdcflags = EPDC_CC;
				eink_update_image(&area, &epdcflags);
			}
		}

		if (decode_frame_index - fresh_frame_index >= 2) {
			eink_tcon_open();
		}

		while ((ppm_is_active() || fresh_frame_index < total_frame_index) && decode_frame_index >= total_frame_index) {
			msleep(20);
		}
		if (decode_frame_index < total_frame_index) continue;

		mdelay(eink_param.powerdown_vcom_delay);
		pmic->epdc_pmic_vcom(0);
		vcom_on = 0;

		axi_cleanup();

		delay_wakeup_by_new_update(160 /* eink_param.powerdown_vhigh_delay */);
		if (decode_frame_index < total_frame_index) continue;

		if (eink_param.powerdown_vhigh_delay >= 0) {
			pmic->epdc_pmic_vhigh(0);
			vhigh_on = 0;
		}

		delay_wakeup_by_new_update(eink_param.powerdown_vdd_delay);
		if (decode_frame_index < total_frame_index) continue;

		if (vhigh_on == 0 && eink_param.powerdown_vdd_delay >= 0) {
			epdc_hw_pin_setup(LCD_PIN_SUSPEND);
			pmic->epdc_pmic_vdd(0);
			vdd_on = 0;
		}
		eink_disable();
		decode_thread_active = 0;
	}

	return 0;
}


int eink_update_image(struct area_info *update_area, uint32_t *flags)
{
	struct area_info native_area, upd_bounds;
	struct area_info upd_areas[2], coll_areas[2];
	int nupd, ncoll;
	int tframes = 0;
	uint32_t active_mask;
	int i, ret = 0;
	struct eink_image_slot *cslot = (*flags & (EPDC_LOGO /* | EPDC_CC */)) ? &shadow_slot : &framebuffer_slot;

	if (update_area->mode != 0) *flags |= EPDC_FULL;

	if (suspend) return -EBUSY;
	in_update = true;
	mutex_lock(&update_lock);

	if ((*flags & EPDC_LOGO) && ! logo_showing) {
		mutex_unlock(&update_lock);
		return -EINTR;
	}
	if (! (*flags & EPDC_LOGO)) {
		if (logo_showing) {
			if (axi_rect_size(update_area) >= 192*192) {
				logo_showing = 0;
				eink_wait_complete();
			} else {
				mutex_unlock(&update_lock);
				return 0;
			}
		}
	}

	INF("UPDATE: %d wf=%d mode=%d area=(%d,%d,%d,%d) %x\n",
		current->pid, update_area->wf, update_area->mode,
		update_area->x_top, update_area->y_top,
		update_area->x_bottom+1-update_area->x_top, update_area->y_bottom+1-update_area->y_top,
		*flags
	);

	if (eink_param.dump_updates) dump_update(cslot, update_area, *flags);

	if (! waveform_loaded) {
		ret = epdc_waveform_load_from_memory(EPDC_WAVEFORM_ADDR, EPDC_WAVEFORM_SIZE);
		if (ret) {
			ret = epdc_waveform_load(WAVEFORM_PATH);
			if (ret) {
				ERR("waveform could not be loaded\n");
				mutex_unlock(&update_lock);
				ret = -EIO;
				goto upd_exit;
			}
		}
		waveform_loaded = 1;
		memblock_free(EPDC_WAVEFORM_ADDR,EPDC_WAVEFORM_SIZE);
	}

	if (! epdc_initialized) {
		if (epdc_set_display_parameters(eink_param.timing.width, eink_param.timing.height, wfdata.type, wfdata.name, wfdata.amepd_part, eink_param.bus_width) != 0) {
			ERR("no timing parameters found for this display\n");
			mutex_unlock(&update_lock);
			ret = -ENODEV;
			goto upd_exit;
		}
		epdc_hw_init();
		epdc_update_timings();
		pmic->epdc_pmic_pwrseq(eink_param.powerup_sequence, eink_param.powerdown_sequence, eink_param.vhigh_level);
		eink_param.vcom = epdc_vcom_read(eink_param.vcom);
		epdc_initialized = 1;
	}

	/* update area should be in native (landscape) coordinates */
	if (*flags & EPDC_CC) {
		native_area = *update_area;
	} else {
		axi_normalize_area(cslot, update_area, &native_area);
	}

	/* align update area to 4 pixels */
	native_area.x_top &= ~3;
	native_area.y_top &= ~3;
	native_area.x_bottom |= 3;
	native_area.y_bottom |= 3;

	if (native_area.x_top < 0 || native_area.y_top < 0 ||
		native_area.x_bottom >= eink_param.timing.width || native_area.y_bottom >= eink_param.timing.height ||
		native_area.x_top > native_area.x_bottom || native_area.y_top > native_area.y_bottom) {
			ERR("wrong update parameters [%d %d %d %d]\n", native_area.x_top, native_area.y_top, native_area.x_bottom, native_area.y_bottom);
			ret = -EINVAL;
			goto upd_exit;
	}


	/* if possible, merge with last non-started pipelines */
	ppm_combine_delayed(&native_area);

	__cpuc_flush_dcache_area(cslot->vaddr, cslot->memsize);

	axi_split_area(&native_area, upd_areas, &nupd, coll_areas, &ncoll);

	/* if there is no free pipelines, store as collision */
	if (ppm_free_list_status() < nupd) {
		axi_collision_add(cslot, &shadow_slot, &native_area);
		goto upd_exit;
	}

	axi_rect_zero(&upd_bounds);
	active_mask = 0;
	for (i=0; i<nupd; i++) {
		struct area_info *ca = &upd_areas[i];
		// copy every update area: current->last, rotate(fb)->current
		uint32_t changes = axi_update_index(cslot, &indexmap, ca);
		if (changes == 2 && ca->mode == 0) {
			// should we upgrade waveform?
			if (ca->wf == EPDC_WFTYPE_DU) {
				DBG("wf changed: %d->%d\n", ca->wf, EPDC_WFTYPE_GC16);
				ca->wf = EPDC_WFTYPE_GC16;
			}
		}
		if (changes == 1 && ca->mode == 0) {
			// can we downgrade waveform?
			if (ca->wf == EPDC_WFTYPE_GC16 || ca->wf == EPDC_WFTYPE_GS16 || ca->wf == EPDC_WFTYPE_GC4) {
				DBG("wf changed: %d->%d\n", ca->wf, EPDC_WFTYPE_DU);
				ca->wf = EPDC_WFTYPE_DU;
			}
		}
		if (changes || ca->mode == 1) {
			axi_rect_merge(&upd_bounds, ca);
			active_mask |= (1 << i);
		}
	}
	for (i=0; i<ncoll; i++) {
		// add every collision area to collision list
		axi_collision_add(cslot, &shadow_slot, &coll_areas[i]);
	}
	if (active_mask) {

		// some updates are pending

		__cpuc_flush_dcache_area(indexmap.vaddr, indexmap.size);

		// add every update to decoder queue

		tframes = 0;

		for (i=0; i<nupd; i++) {
			struct area_info *ca = &upd_areas[i];
			if (! (active_mask & (1 << i))) continue;
			DBG("$ pipe (%d,%d,%d,%d) wf=%d%c temp=%d\n", 
				ca->x_top, ca->y_top, ca->x_bottom+1-ca->x_top, ca->y_bottom+1-ca->y_top,
				ca->wf, ca->mode ? 'F' : 'P', eink_param.temperature
			);
			ppm_config_one_pipeline(ca, eink_param.temperature, &tframes);
		}
		if (decode_frame_index + (tframes + WAIT_FRAMES) > total_frame_index) {
			total_frame_index = decode_frame_index + (tframes + WAIT_FRAMES);
		}
		wake_up(&decode_wq);
	}

upd_exit:
	mutex_unlock(&update_lock);
	in_update = false;
	return ret;
}

int eink_wait_complete(void)
{
	DBG("+ eink_wait_complete\n");
	while (ppm_is_active()) {
		usleep_range(10000, 15000);
	}
	DBG("- eink_wait_complete\n");
	return 0;
}

int eink_get_update_state(void)
{
	if (ppm_is_active()) return 1;
	if (decode_thread_active) return 2;
	return 0;
}

s32 eink_resume(void)
{
	if (!suspend) return 0;
	DBG("eink_resume\n");

	suspend = 0;
	epdc_hw_resume();
	/* register eink irq */
	if (!first_enable) {
		eink_clk_enable();
		disp_sys_register_irq(irq_no, 0, eink_ee_interrupt, NULL, 0, 0);
		disp_sys_enable_irq(irq_no);
	}
	return 0;
}

s32 eink_suspend(void)
{
	if (suspend) return 0;
	DBG("eink_suspend %d\n", enable_flag);

	if (enable_flag) return -EBUSY;

	suspend = 1;
	if (!first_enable) {
		eink_clk_disable();
		disp_sys_disable_irq(irq_no);
		disp_sys_unregister_irq(irq_no, eink_ee_interrupt, NULL);
	}
	epdc_hw_suspend();
	return 0;
}

int write_edma(void)
{
	int ret = 0, cur_line = 0, start_delay = 0;
	int sm = eink_param.timing.slow_motion;

	edma_paddr = (sm != 0 && (++sm_counter % sm) != 0) ? 0 : request_buffer_for_display();
	if (!edma_paddr) {
		FDBG("$ skip %d %d\n", sm, sm_counter);
		edma_paddr = epdc_wavering_get_empty();
	} else {
		//FDUMP("$ edma", edma_vaddr, 32);
		fresh_frame_index++;
	}
	
	start_delay = disp_al_lcd_get_start_delay(0, NULL);
	disp_al_eink_edma_cfg_addr(0, edma_paddr);

	cur_line = disp_al_lcd_get_cur_line(0, NULL);
	if (cur_line < start_delay)
		ERR("cfg edma too quicker.\n");
	disp_al_dbuf_rdy();

	ret = dequeue_wavedata_buffer();
	return ret;
}

static int write_edma_first(void)
{
	int ret = 0;

	edma_paddr = request_buffer_for_display();
	if (!edma_paddr) {
		FDBG("$ empty\n");
		edma_paddr = epdc_wavering_get_empty();
	} else {
		//FDUMP("$ edma1", edma_vaddr, 32);
		fresh_frame_index++;
	}
	sm_counter = 0;

	disp_al_edma_config(0, edma_paddr, &eink_param);
	disp_al_edma_write(0, 1);
	disp_al_dbuf_rdy();
	ret = dequeue_wavedata_buffer();
	return ret;
}

int eink_manager_init(disp_bsp_init_para * para)
{
	int  value = 1;
	int ret = 0;
	unsigned long slot_size;
	unsigned long indexdata_buf_size = 0;
	char *primary_key = "lcd0";

	struct power_supply *psy;
	union power_supply_propval voltage, status;

	INF("init_eink\n");

	if (disp_sys_script_get_item(primary_key, "lcd_used", &value, 1) == 1) eink_param.used = value;
	if (eink_param.used == 0)	return 0;
	if (disp_sys_script_get_item(primary_key, "eink_mode", &value, 1) == 1) eink_param.bus_width = value;
	if (disp_sys_script_get_item(primary_key, "eink_width", &value, 1) == 1) eink_param.timing.width = value;
	if (disp_sys_script_get_item(primary_key, "eink_height", &value, 1) == 1) eink_param.timing.height = value;

	eink_param.timing.width = (eink_param.timing.width + 3) & ~3;

	eink_param.temp_fixed = false;
	eink_param.temperature = 25;
	eink_param.vcom = -1870;
	eink_param.strength = 2;

	INF("size:%dx%d bus:%d\n", eink_param.timing.width, eink_param.timing.height, eink_param.bus_width ? 16 : 8);
	INF("waveform: %s\n", WAVEFORM_PATH);

	enable_flag = false;
	eink_clk = para->mclk[DISP_MOD_EINK];
	edma_clk = para->mclk[DISP_MOD_EDMA];
	eink_base_addr = (unsigned long)para->reg_base[DISP_MOD_EINK];
	irq_no = para->irq_no[DISP_MOD_EINK];

	DBG("eink_clk: 0x%p; edma_clk: 0x%p; base_addr: 0x%p; irq_no: 0x%x\n",
				eink_clk, edma_clk, (void *)eink_base_addr, irq_no);

	disp_al_set_eink_base(0, eink_base_addr);

	psy = power_supply_get_by_name("battery");
	if (psy && psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &voltage) == 0) {

		psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &status);
		INF("battery voltage: %d charge status: %d\n", voltage.intval/1000, status.intval);

		if (voltage.intval / 1000 <= BATTERY_CRITICAL_VOLTAGE) {
			// very low battery
			logo_showing = 0;
		} else if (voltage.intval / 1000 > BATTERY_MIN_VOLTAGE) {
			// normal charge
			logo_showing = 1;
		} else if (status.intval == POWER_SUPPLY_STATUS_CHARGING || status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			// low battery, charger connected
			logo_showing = 2;
		} else {
			// no charger, show low battery logo
			logo_showing = 3;
		}

	} else {
		ERR("could not read battery level\n");
		logo_showing = 1;
	}


	spin_lock_init(&eelock);
	spin_lock_init(&tconlock);

	axi_init();

	/* init index buffer */

	indexmap.size = eink_param.timing.width * eink_param.timing.height;
	indexmap.width = eink_param.timing.width;
	indexmap.height = eink_param.timing.height;
	indexmap.scanline = eink_param.timing.width;
	indexmap.vaddr = disp_malloc(indexmap.size, &indexmap.paddr);
	if (! indexmap.vaddr) {
		ERR("malloc index data memory fail!\n");
		ret =  -ENOMEM;
		goto private_init_malloc_err;
	}
	memset(indexmap.vaddr, 0xff, indexmap.size);
	DBG("indexmap=%p/%x size=%lu\n", indexmap.vaddr, indexmap.paddr, indexdata_buf_size);

  /* init current and last image buffer slots */

	epdc_init_wavering();
	pipeline_manager_init();
	tasklet_init(&sync_tasklet, eink_sync_task, (unsigned long)0);
	mutex_init(&update_lock);

	pmic = &epdc_pmic_driver_tps65185;
	if (pmic->epdc_pmic_init() != 0) {
		ERR("PMIC init failed\n");
		goto private_init_malloc_err;
	}

	eink_param.vcom = eink_param.vcom_permanent = pmic->epdc_pmic_vcom_read();
	INF("pmic internal vcom: %d\n", eink_param.vcom_permanent);

	decode_task = kthread_create(epdc_decode_thread, NULL, "epdc_decode");
	wake_up_process(decode_task);

	bootlogo_task = kthread_create(epdc_bootlogo_thread, &logo_showing, "epdc_bootlogo");
	wake_up_process(bootlogo_task);

	//index_test_main();

	return ret;

private_init_malloc_err:
	return ret;
}


#if 0

uint8_t *fb1_vaddr, *fb2_vaddr, *idx1_vaddr, *idx2_vaddr;
phys_addr_t fb1_paddr, fb2_paddr, idx1_paddr, idx2_paddr;

void index_test(int w, int h, int flash_mode, int win_en)
{
	struct ee_img img1, img2;
	img1.w = img2.w = w;
	img1.h = img2.h = h;
	img1.pitch = img2.pitch = w;
	img1.addr = fb1_paddr;
	img2.addr = fb2_paddr;

	memset(idx1_vaddr, 0xce, w*h);
	memset(idx2_vaddr, 0xab, w*h);
	memset(fb1_vaddr, 0x11, w*h);
	memset(fb2_vaddr, 0x11, w*h);

	for (y=4; y<=11; y++) {
		for (x=4; x<=11; x++) {
			fb2_vaddr[y*w+x] = (2 + ((x+y) & 7)) * 0x11;
			fb2_vaddr[(y+16)*w+(x+16)] = (2 + ((x+y) & 7)) * 0x11;
		}
	}

	__cpuc_flush_dcache_area(idx1_vaddr, w*h);
	__cpuc_flush_dcache_area(idx2_vaddr, w*h);
	__cpuc_flush_dcache_area(fb1_vaddr, w*h);
	__cpuc_flush_dcache_area(fb2_vaddr, w*h);

	area->x_top = area->y_top = 0;
	area->x_bottom = area->y_bottom = 15;
	eink_start_idx(&img1, &img2, flash_mode, win_en, idx1_paddr, idx2_paddr, &area);

	.....

}

void index_test_main(void)
{

	vaddr1 = disp_malloc(1600*1200, &paddr1);
	vaddr2 = disp_malloc(1600*1200, &paddr2);
	img1.addr = vaddr1;
	img2.addr = vaddr2;
	if (vaddr1 && vaddr2) {
		index_test(img1, img2, 80, 80, 0, 0);
		index_test(img1, img2, 80, 80, 1, 0);
		index_test(img1, img2, 120, 120, 1, 0);
		index_test(img1, img2, 240, 240, 1, 0);
		index_test(img1, img2, 640, 480, 1, 0);
		index_test(img1, img2, 800, 600, 1, 0);
		index_test(img1, img2, 1024, 768, 1, 0);
		index_test(img1, img2, 1600, 1200, 1, 0);
		index_test(img1, img2, 1600, 1200, 1, 1);
	}
}

#endif

void eink_get_size(int *width, int *height)
{
	*width = eink_param.timing.width;
	*height = eink_param.timing.height;
}

void epdc_reserve_memory(void)
{
	memblock_reserve(EPDC_WAVEFORM_ADDR, EPDC_WAVEFORM_SIZE);
	memblock_reserve(EPDC_BOOTLOGO_ADDR, EPDC_BOOTLOGO_SIZE);
}


#endif

