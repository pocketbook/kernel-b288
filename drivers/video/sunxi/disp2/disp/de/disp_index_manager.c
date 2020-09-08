/*
 * Copyright (C) 2015 Allwinnertech, z.q <zengqi@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "disp_index_manager.h"
#include "include.h"

/* index format: [ OLD:4 NEW:4 ] */

static unsigned int          irq_num;
static volatile unsigned int index_calc_finish;
static wait_queue_head_t     index_calc_queue;
static struct clk*           clk;
static struct mutex          mlock;

/*
start to calc index or update area
1. index will be stored in new_index_data_paddr buffer.
2. if current_image->window_calc_enable == TRUE, area will update to current_image->update_area
*/
s32 index_calc_start(struct area_info *update_area, int isfull,
						phys_addr_t old_index_data_paddr, phys_addr_t new_index_data_paddr,
						struct eink_image_slot *last_image, struct eink_image_slot *current_image)
{
	long timerout = (200 * HZ)/1000;		/*200ms*/
	int ret = -1;
	int i;

	mutex_lock(&mlock);
	for (i=0; i<3; i++) {
		disp_al_eink_start_calculate_index(update_area, isfull, old_index_data_paddr, new_index_data_paddr, last_image, current_image);

		timerout = wait_event_interruptible_timeout(index_calc_queue, (1 == index_calc_finish), timerout);
		index_calc_finish = 0;
		if (timerout != 0) {
			ret = 0;
			break;
		}
	}
	mutex_unlock(&mlock);
	return ret;
}

int disp_index_irq_handler(void)
{
	index_calc_finish = 1;
	wake_up_interruptible(&index_calc_queue);
	return 0;
}

s32 index_manager_init(disp_bsp_init_para * para)
{
	s32 ret = -1;

	mutex_init(&mlock);
	init_waitqueue_head(&index_calc_queue);
	index_calc_finish = 0;
	irq_num = para->irq_no[DISP_MOD_EINK];
	clk = para->mclk[DISP_MOD_EINK];

	/* ee_base has been set by disp_eink_manager, index modules use the same address */
	//disp_al_set_eink_base(i, para->reg_base[DISP_MOD_EINK]);

	return ret;
}


