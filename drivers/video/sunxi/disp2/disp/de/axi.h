#ifndef _AXI_H_
#define _AXI_H_

#include "../eink.h"

#define axi_rect_empty(a) (((a)->x_top | (a)->x_bottom | (a)->y_top | (a)->y_bottom) == 0)
#define axi_rect_size(a) (((a)->x_bottom + 1 - (a)->x_top) * ((a)->y_bottom + 1 - (a)->y_top))

void axi_init(void);

/* zero rectangle */
void axi_rect_zero(struct area_info *area);

/* merge two rectangles into first one */
void axi_rect_merge(struct area_info *main_area, struct area_info *area);

/* chack whether two rectangles overlap */
bool axi_rect_overlaps(struct area_info *area1, struct area_info *area2);

/* chack whether first area fully covers the second one */
bool axi_rect_covers(struct area_info *area1, struct area_info *area2);

/* chack whether two rectangles can be combined into one */
bool axi_rect_combinable(struct area_info *area1, struct area_info *area2);

/* transform fb coordinates to native coordinates */
void axi_normalize_area(struct eink_image_slot *fb, struct area_info *src_area, struct area_info *dest_area);

/* copy update area (current-last, fb-(rotate)-current), returns !0 if area is changed */
//uint32_t axi_update_area(struct eink_image_slot *fb, struct eink_image_slot *curr, struct eink_image_slot *last, struct area_info *update_area);

uint32_t axi_update_index(struct eink_image_slot *fb, struct index_buffer *idx, struct area_info *area);

/* revert area (last-current) */
//void axi_revert_area(struct eink_image_slot *curr, struct eink_image_slot *last, struct area_info *update_area);

void axi_revert_index(struct index_buffer *idx, struct area_info *area);

/* split area into number of update areas and colliding areas */
void axi_split_area(struct area_info *src_area, struct area_info *upd_areas, int *nupd, struct area_info *coll_areas, int *ncoll);

/* register pipeline in AXI for collision checks */
void axi_new_pipeline(int num, struct area_info *area);

/* remove completed pipeline from list */
void axi_remove_pipeline(int num);

/* store new collision area */
void axi_collision_add(struct eink_image_slot *fb, struct eink_image_slot *shadow, struct area_info *area);

/* exclude already updated area from pending collisions */
void axi_collision_exclude(struct area_info *area);

/* get next collision that can be applied now */
bool axi_collision_fetch(struct area_info *area);

/* forget all pipelines and collision areas */
void axi_cleanup(void);

#endif

