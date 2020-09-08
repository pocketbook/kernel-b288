/*
 * Copyright (C) 2015 Allwinnertech, z.q <zengqi@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "../eink.h"
#include "axi.h"
#include "lowlevel_sun8iw10/disp_al.h"

struct pipeline_list
{
  struct list_head   free_list;
  struct list_head   used_list;
  struct mutex       mlock;
};

struct pipeline_info
{
  struct area_info      area;
  int          frame_index;
  int          total_frames;
  int          pipeline_no;
  int                   temperature;
  enum  eink_bit_num    bit_num;
  unsigned int          wave_file_addr;
  struct list_head      node;
};

static struct pipeline_list  list;

static int ppm_is_locked = 0;
/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:  the type * to use as a loop cursor.
 * @n:    another type * to use as temporary storage
 * @head: the head for your list.
 * @member: the name of the list_struct within the struct.

#define list_for_each_entry_safe(pos, n, head, member)      \
  for (pos = list_entry((head)->next, typeof(*pos), member),  \
    n = list_entry(pos->member.next, typeof(*pos), member); \
       &pos->member != (head);          \
       pos = n, n = list_entry(n->member.next, typeof(*n), member))

*/


/* return the number of free pipe*/
int ppm_free_list_status(void)
{
	struct pipeline_info* pipeline, *tpipeline;
	int num = 0;

	mutex_lock(&list.mlock);
	if (list_empty(&list.used_list)) {
		num = MAX_PIPELINES;
	} else {
		list_for_each_entry_safe(pipeline, tpipeline, &list.free_list, node) {
			num++;
		}
	}
	mutex_unlock(&list.mlock);
	return num;
}

/* return the number of used pipe*/
int ppm_used_list_status(void)
{
	struct pipeline_info *pipeline, *tpipeline;
	int num = 0;

	mutex_lock(&list.mlock);
	if (list_empty(&list.free_list)) {
		num = MAX_PIPELINES;
	} else {
		list_for_each_entry_safe(pipeline, tpipeline, &list.used_list, node) {
			num++;
		}
	}
	mutex_unlock(&list.mlock);
	return num;
}

int ppm_is_active(void)
{
	return ! list_empty(&list.used_list);
}

int ppm_clear_pipeline_list(void)
{
	struct pipeline_info *pipeline, *tpipeline;

	mutex_lock(&list.mlock);
	list_for_each_entry_safe(pipeline, tpipeline, &list.used_list, node) {
		list_move_tail(&pipeline->node, &list.free_list);
		axi_remove_pipeline(pipeline->pipeline_no);
	}
	mutex_unlock(&list.mlock);
	return 0;
}

int ppm_config_one_pipeline(struct area_info *update_area, int temperature, int *tframes)
{
	struct pipeline_info* pipeline, *tpipeline;
	int ret = 0;

	if (! epdc_waveform_supported(update_area->wf)) {
		ERR("waveform %d not available\n", update_area->wf);
		return -EINVAL;
	}

	mutex_lock(&list.mlock);
	list_for_each_entry_safe(pipeline, tpipeline, &list.free_list, node) {

		pipeline->total_frames = 0;
		pipeline->frame_index = -2;
		pipeline->temperature = temperature;
		pipeline->area = *update_area;
		pipeline->bit_num = epdc_get_waveform_bits();
		pipeline->wave_file_addr = epdc_get_waveform_data(pipeline->area.wf, pipeline->area.mode, pipeline->temperature, &pipeline->total_frames);
		PDBG("P%d: cfg wf=%d%c total=%d\n", pipeline->pipeline_no, pipeline->area.wf, pipeline->area.mode ? 'F' : 'P', pipeline->total_frames);

		if (*tframes < pipeline->total_frames) *tframes = pipeline->total_frames;

		/* no need config wave file addr,it will config when next decode task. */
		axi_new_pipeline(pipeline->pipeline_no, &pipeline->area);
		list_move(&pipeline->node, &list.used_list);
		break;

	}
	mutex_unlock(&list.mlock);
	return ret;
}

uint32_t ppm_update_pipeline_list(struct area_info *new_rect)
{
	int ret = 0;
	struct pipeline_info* pipeline, *tpipeline;
	unsigned int cur_wave_paddr;

	axi_rect_zero(new_rect);

	mutex_lock(&list.mlock);
	list_for_each_entry_safe(pipeline, tpipeline, &list.used_list, node) {

		if ((pipeline->frame_index == pipeline->total_frames) && (0 != pipeline->total_frames)) {
			disp_al_eink_pipe_disable(0, pipeline->pipeline_no);
			list_move_tail(&pipeline->node, &list.free_list);
			axi_remove_pipeline(pipeline->pipeline_no);
			PDBG("P%d: complete\n", pipeline->pipeline_no);
			ret |= PPM_REMOVE;
			continue;
		}

		if (pipeline->frame_index < 0 && ppm_is_locked > 0) continue;

		pipeline->frame_index++;

		if (pipeline->frame_index == 0) {
			disp_al_eink_pipe_config(0, pipeline->pipeline_no, &pipeline->area);
			disp_al_eink_pipe_enable(0, pipeline->pipeline_no);
			PDBG("P%d: enable\n", pipeline->pipeline_no);
			axi_rect_merge(new_rect, &pipeline->area);
			ret |= PPM_INSERT;
		}
		if (pipeline->frame_index >= 0) {
			cur_wave_paddr = pipeline->wave_file_addr + (1<< (pipeline->bit_num << 1)) * pipeline->frame_index; // 256 / 1024
			disp_al_eink_pipe_config_wavefile(0, cur_wave_paddr, pipeline->pipeline_no);
		}

	}
	mutex_unlock(&list.mlock);
	return ret;
}

void ppm_combine_delayed(struct area_info *update_area)
{
	struct pipeline_info* pipeline, *tpipeline;

	mutex_lock(&list.mlock);
	list_for_each_entry_safe(pipeline, tpipeline, &list.used_list, node) {
		if (pipeline->frame_index >= 0) continue;
		if (! axi_rect_combinable(update_area, &pipeline->area))continue;
		DBG("P%d: combined with new area\n", pipeline->pipeline_no);
		axi_revert_index(&indexmap, &pipeline->area);
		axi_rect_merge(update_area, &pipeline->area);
		list_move_tail(&pipeline->node, &list.free_list);
		axi_remove_pipeline(pipeline->pipeline_no);
	}
	mutex_unlock(&list.mlock);
}

void ppm_lock(void)
{
	ppm_is_locked++;
}

void ppm_unlock(void)
{
	ppm_is_locked--;
}

int pipeline_manager_init(void)
{
	int ret = 0;
	int i;
	struct pipeline_info* pipeline[MAX_PIPELINES];

	INIT_LIST_HEAD(&list.free_list);
	INIT_LIST_HEAD(&list.used_list);

	mutex_init(&list.mlock);

	for (i = 0; i < MAX_PIPELINES; i++) {
		pipeline[i] = (struct pipeline_info *)disp_sys_malloc(sizeof(struct pipeline_info));
		if (! pipeline[i]) {
			ERR("no memory for pipeline %d\n", i);
			goto err;
		}
		memset((void*)pipeline[i], 0, sizeof(struct pipeline_info));
		pipeline[i]->pipeline_no = i;
		list_add_tail(&pipeline[i]->node, &list.free_list);
	}

	return ret;

err:
	return -ENOMEM;
}

