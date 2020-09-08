#include "axi.h"

#include <asm/cp15.h>
#include <asm/cputype.h>
#include <asm/system_info.h>
#include <asm/thread_notify.h>
#include <asm/vfp.h>

void kernel_neon_begin(void);

#define axi_bit_set(var, num) (var) |= (1 << (num))
#define axi_bit_clear(var, num) (var) &= ~(1 << (num))
#define axi_bit_test(var, num) ((var) & (1 << (num)))

static const uint8_t wf_range[MAX_WF_MODES] = { EPDC_WF_RANGES };

static struct area_info pipelines[MAX_PIPELINES];
static struct area_info collisions[MAX_COLLISIONS];

static uint32_t active_mask = 0;
static uint32_t collision_mask = 0;

static spinlock_t axilock;

static inline uint32_t byte_rev(uint32_t x)
{
	return ((x >> 24) & 0xff) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | ((x << 24)  & 0xff000000);
}

void axi_rect_zero(struct area_info *area)
{
	memset(area, 0, sizeof(struct area_info));
}

static inline void axi_best_waveform(struct area_info *area1, struct area_info *area2)
{
	if (area1->mode == 1 || area2->mode == 1) area1->mode = area2->mode = 1;
	if (wf_range[area1->wf] > wf_range[area2->wf]) {
		area2->wf = area1->wf;
	} else {
		area1->wf = area2->wf;
	}
}

void axi_rect_merge(struct area_info *main_area, struct area_info *area)
{
	if (axi_rect_empty(area)) return;
	if (axi_rect_empty(main_area)) {
		*main_area = *area;
	} else {
		if (main_area->x_top > area->x_top) main_area->x_top = area->x_top;
		if (main_area->y_top > area->y_top) main_area->y_top = area->y_top;
		if (main_area->x_bottom < area->x_bottom) main_area->x_bottom = area->x_bottom;
		if (main_area->y_bottom < area->y_bottom) main_area->y_bottom = area->y_bottom;
		axi_best_waveform(main_area, area);
	}
}

int axi_rect_sum(struct area_info *area1, struct area_info *area2)
{
	struct area_info tmp = *area1;
	if (axi_rect_empty(&tmp)) tmp = *area2;
	if (axi_rect_empty(&tmp)) return 0;
	if (! axi_rect_empty(area2)) {
		if (tmp.x_top > area2->x_top) tmp.x_top = area2->x_top;
		if (tmp.y_top > area2->y_top) tmp.y_top = area2->y_top;
		if (tmp.x_bottom < area2->x_bottom) tmp.x_bottom = area2->x_bottom;
		if (tmp.y_bottom < area2->y_bottom) tmp.y_bottom = area2->y_bottom;
	}
	return axi_rect_size(&tmp);
}

bool axi_rect_overlaps(struct area_info *area1, struct area_info *area2)
{
	if (axi_rect_empty(area1) || axi_rect_empty(area2)) return false;
	if (area1->x_bottom < area2->x_top) return false;
	if (area1->x_top > area2->x_bottom) return false;
	if (area1->y_bottom < area2->y_top) return false;
	if (area1->y_top > area2->y_bottom) return false;
	return true;
}

bool axi_rect_covers(struct area_info *area1, struct area_info *area2)
{
	if (axi_rect_empty(area1) || axi_rect_empty(area2)) return false;
	if (area1->x_top > area2->x_top) return false;
	if (area1->x_bottom < area2->x_bottom) return false;
	if (area1->y_top > area2->y_top) return false;
	if (area1->y_bottom < area2->y_bottom) return false;
	return true;
}

bool axi_rect_combinable(struct area_info *area1, struct area_info *area2)
{
	if (axi_rect_empty(area1) || axi_rect_empty(area2)) return false;
	if (area1->x_bottom < area2->x_top - 4) return false;
	if (area1->x_top > area2->x_bottom + 4) return false;
	if (area1->y_bottom < area2->y_top - 4) return false;
	if (area1->y_top > area2->y_bottom + 4) return false;
	if (axi_rect_sum(area1, area2) > 160*160) return false;
	return true;
}

void axi_rect_break(struct area_info *areas, int *count, struct area_info *inpipe, bool update)
{
	struct area_info *area;
	int i;
	struct area_info tpipe;
	struct area_info *pipe = &tpipe;

#define ADUMP(a) (a).x_top,(a).y_top,(a).x_bottom+1-(a).x_top,(a).y_bottom+1-(a).y_top

	AXIDBG(" ** axi_rect_break:%c %d(%d,%d,%d,%d)(%d,%d,%d,%d) ^(%d,%d,%d,%d)\n",
						update?'U':'C', *count, ADUMP(areas[0]), ADUMP(areas[1]), ADUMP(*inpipe));

	for (i=0; i<*count;) {
		*pipe = *inpipe;     // don't touch original rect
		area = &areas[i];

		if (pipe->x_top > area->x_top && pipe->x_bottom < area->x_bottom && pipe->y_top > area->y_top && pipe->y_bottom < area->y_bottom) {
			// in the middle, extend pipe
			if (pipe->x_top - area->x_top > area->x_bottom - pipe->x_bottom) {
				pipe->x_bottom = area->x_bottom;
			} else {
				pipe->x_top = area->x_top;
			}
			if (pipe->y_top - area->y_top > area->y_bottom - pipe->y_bottom) {
				pipe->y_bottom = area->y_bottom;
			} else {
				pipe->y_top = area->y_top;
			}
		}

		if (! axi_rect_overlaps(area, pipe)) {
			// no collision, leave as is
		} else if (axi_rect_covers(pipe, area)) {
			// full collision, exclude
			areas[0] = areas[1];
			(*count)--;
			continue;
		} else if (pipe->x_top <= area->x_top && pipe->x_bottom >= area->x_top && pipe->x_bottom < area->x_bottom) {
			// enter left
			if (pipe->y_top <= area->y_top && pipe->y_bottom >= area->y_bottom) {
				// cut left part
				area->x_top = pipe->x_bottom + 1;
			} else if (pipe->y_top <= area->y_top && *count == 1 && update) {
				// top-left
				areas[1].x_top = area->x_top;
				areas[1].x_bottom = area->x_bottom;
				areas[1].y_top = pipe->y_bottom + 1;
				areas[1].y_bottom = area->y_bottom;
				area->x_top = pipe->x_bottom + 1;
				area->y_bottom = pipe->y_bottom;
				*count = 2;
			} else if (pipe->y_bottom >= area->y_bottom && *count == 1 && update) { 
				// bottom-left
				areas[1].x_top = area->x_top;
				areas[1].x_bottom = area->x_bottom;
				areas[1].y_top = area->y_top;
				areas[1].y_bottom = pipe->y_top - 1;
				area->x_top = pipe->x_bottom + 1;
				area->y_top = pipe->y_top;
				*count = 2;
			} else if (update) {
				// center-left
				area->x_top = pipe->x_bottom + 1;
			}
		} else if (pipe->x_top > area->x_top && pipe->x_top <= area->x_bottom && pipe->x_bottom >= area->x_bottom) {
			// enter right
			if (pipe->y_top <= area->y_top && pipe->y_bottom >= area->y_bottom) {
				// cut right part
				area->x_bottom = pipe->x_top - 1;
			} else if (pipe->y_top <= area->y_top && *count == 1 && update) {
				// top-right
				areas[1].x_top = area->x_top;
				areas[1].x_bottom = area->x_bottom;
				areas[1].y_top = pipe->y_bottom + 1;
				areas[1].y_bottom = area->y_bottom;
				area->x_bottom = pipe->x_top - 1;
				area->y_bottom = pipe->y_bottom;
				*count = 2;
			} else if (pipe->y_bottom >= area->y_bottom && *count == 1 && update) { 
				// bottom-right
				areas[1].x_top = area->x_top;
				areas[1].x_bottom = area->x_bottom;
				areas[1].y_top = area->y_top;
				areas[1].y_bottom = pipe->y_top - 1;
				area->x_bottom = pipe->x_top - 1;
				area->y_top = pipe->y_top;
				*count = 2;
			} else if (update) {
				// center-right
				area->x_bottom = pipe->x_top - 1;
			}
		} else if (pipe->y_top <= area->y_top && pipe->y_bottom >= area->y_top && pipe->y_bottom < area->y_bottom) {
			// enter top
			if (pipe->x_top <= area->x_top && pipe->x_bottom >= area->x_bottom) {
				// cut top part
				area->y_top = pipe->y_bottom + 1;
			} else if (pipe->x_top <= area->x_top && *count == 1 && update) {
				// top-left
				areas[1].x_top = area->x_top;
				areas[1].x_bottom = area->x_bottom;
				areas[1].y_top = pipe->y_bottom + 1;
				areas[1].y_bottom = area->y_bottom;
				area->x_top = pipe->x_bottom + 1;
				area->y_bottom = pipe->y_bottom;
				*count = 2;
			} else if (pipe->x_bottom >= area->x_bottom && *count == 1 && update) { 
				// top-right
				areas[1].x_top = area->x_top;
				areas[1].x_bottom = area->x_bottom;
				areas[1].y_top = pipe->y_bottom + 1;
				areas[1].y_bottom = area->y_bottom;
				area->x_bottom = pipe->x_top - 1;
				area->y_bottom = pipe->y_bottom;
				*count = 2;
			} else if (update) {
				// top-center
				area->y_top = pipe->y_bottom + 1;
			}
		} else if (pipe->y_top > area->y_top && pipe->y_top < area->y_bottom && pipe->y_bottom >= area->y_bottom) {
			// enter bottom
			if (pipe->x_top <= area->x_top && pipe->x_bottom >= area->x_bottom) {
				// cut bottom part
				area->y_bottom = pipe->y_top - 1;
			} else if (pipe->x_top <= area->x_top && *count == 1 && update) {
				// bottom-left
				areas[1].x_top = area->x_top;
				areas[1].x_bottom = area->x_bottom;
				areas[1].y_top = area->y_top;
				areas[1].y_bottom = pipe->y_top - 1;
				area->x_top = pipe->x_bottom + 1;
				area->y_top = pipe->y_top;
				*count = 2;
			} else if (pipe->x_bottom >= area->x_bottom && *count == 1 && update) { 
				// bottom-right
				areas[1].x_top = area->x_top;
				areas[1].x_bottom = area->x_bottom;
				areas[1].y_top = area->y_top;
				areas[1].y_bottom = pipe->y_top - 1;
				area->x_bottom = pipe->x_top - 1;
				area->y_top = pipe->y_top;
				*count = 2;
			} else if (update) {
				// bottom-center
				area->y_bottom = pipe->y_top - 1;
			}
		} else if (update) {
			areas[0] = areas[1];
			(*count)--;
			continue;
		}
		i++;
	}

	AXIDBG("   -> %d(%d,%d,%d,%d)(%d,%d,%d,%d)\n",
						*count, ADUMP(areas[0]), ADUMP(areas[1]));

}

void axi_normalize_area(struct eink_image_slot *fb, struct area_info *src_area, struct area_info *dest_area)
{
	switch (fb->orientation) {

		case FB_ROTATE_CW:
			dest_area->x_top = src_area->y_top;
			dest_area->y_top = fb->width - src_area->x_bottom - 1;
			dest_area->x_bottom = src_area->y_bottom;
			dest_area->y_bottom = fb->width - src_area->x_top - 1;
			break;

		case FB_ROTATE_UD:
			dest_area->x_top = src_area->x_top;
			dest_area->y_top = src_area->y_top;
			dest_area->x_bottom = src_area->x_bottom;
			dest_area->y_bottom = src_area->y_bottom;
			break;

		case FB_ROTATE_UR:
			dest_area->x_top = fb->width - src_area->x_bottom - 1;
			dest_area->y_top = fb->height - src_area->y_bottom - 1;
			dest_area->x_bottom = fb->width - src_area->x_top - 1;
			dest_area->y_bottom = fb->height - src_area->y_top - 1;
			break;

		case FB_ROTATE_CCW:
			dest_area->x_top = fb->height - src_area->y_bottom - 1;
			dest_area->y_top = src_area->x_top;
			dest_area->x_bottom = fb->height - src_area->y_top - 1;
			dest_area->y_bottom = src_area->x_bottom;
			break;

	}

	dest_area->wf = src_area->wf;
	dest_area->mode = src_area->mode;
}

void axi_revert_index(struct index_buffer *idx, struct area_info *area)
{
	uint32_t *dp;

	int xt = area->x_top;
	int yt = area->y_top;
	int xb = area->x_bottom;
	int yb = area->y_bottom;
	int x, y;

	for (y=yt; y<=yb; y++) {
		dp = (uint32_t *)(idx->vaddr + (y * idx->scanline + xt));
		for (x=xt; x<=xb; x+=4) {
			*dp >>= 4;
			dp++;
		}
	}
}

static uint32_t rotate_cw_4x4(struct eink_image_slot *fb, struct index_buffer *idx,
		struct area_info *uarea, int xt, int yt, int xb, int yb)
{
  int x, y;
  uint32_t *sp, *dp;

  int qfscan = fb->scanline / sizeof(uint32_t);
  int qiscan = idx->scanline / sizeof(uint32_t);

	uint32_t mi, mx[2];

  for (y=yt; y<=yb;) {
    sp = (uint32_t *)(fb->vaddr + xt * fb->scanline + (fb->width - y - 4));
    dp = (uint32_t *)(idx->vaddr + (y * idx->scanline + xt));
		mi = mx[0] = mx[1] = 0;
    for (x=xt; x<=xb; x+=4) {
			uint32_t c0, c1, c2, c3, d0, d1, d2, d3;
      c0 = *sp; sp += qfscan;
      c1 = *sp; sp += qfscan;
      c2 = *sp; sp += qfscan;
      c3 = *sp; sp += qfscan;
      d0 = (((c0 >> 24) & 0xff) | ((c1 >> 16) & 0xff00) | ((c2 >> 8)  & 0xff0000) | ((c3 >> 0)  & 0xff000000)) >> 4;
      d1 = (((c0 >> 16) & 0xff) | ((c1 >> 8)  & 0xff00) | ((c2 >> 0)  & 0xff0000) | ((c3 << 8)  & 0xff000000)) >> 4;
      d2 = (((c0 >> 8)  & 0xff) | ((c1 >> 0)  & 0xff00) | ((c2 << 8)  & 0xff0000) | ((c3 << 16) & 0xff000000)) >> 4;
      d3 = (((c0 >> 0)  & 0xff) | ((c1 << 8)  & 0xff00) | ((c2 << 16) & 0xff0000) | ((c3 << 24) & 0xff000000)) >> 4;
			if (((d0^dp[qiscan*0]) | (d1^dp[qiscan*1]) | (d2^dp[qiscan*2]) | (d3^dp[qiscan*3])) & 0x0f0f0f0f) { mx[mi] = x; mi = 1; }
      dp[qiscan*0] = ((dp[qiscan*0] << 4) & 0xf0f0f0f0) | (d0 & 0x0f0f0f0f);
      dp[qiscan*1] = ((dp[qiscan*1] << 4) & 0xf0f0f0f0) | (d1 & 0x0f0f0f0f);
      dp[qiscan*2] = ((dp[qiscan*2] << 4) & 0xf0f0f0f0) | (d2 & 0x0f0f0f0f);
      dp[qiscan*3] = ((dp[qiscan*3] << 4) & 0xf0f0f0f0) | (d3 & 0x0f0f0f0f);
      dp++;
    }
		if (mi) {
			if (mx[1] < mx[0]) mx[1] = mx[0];
			if (uarea->x_top > mx[0]) uarea->x_top = mx[0];
			if (uarea->x_bottom < mx[1] + (4-1)) uarea->x_bottom = mx[1] + (4-1);
			if (uarea->y_bottom == 0) uarea->y_top = y;
			uarea->y_bottom = y + (4-1);
		}
    y += 4;
  }
	return 0xffffffff; // assume GC
}

static uint32_t rotate_ccw_4x4(struct eink_image_slot *fb, struct index_buffer *idx,
		struct area_info *uarea, int xt, int yt, int xb, int yb)
{
  int x, y;
  uint32_t *sp, *dp;

  int qfscan = fb->scanline / sizeof(uint32_t);
  int qiscan = idx->scanline / sizeof(uint32_t);

	uint32_t mi, mx[2];

  for (y=yt; y<=yb;) {
    sp = (uint32_t *)(fb->vaddr + ((fb->height - xt - 4) * fb->scanline + y));
    dp = (uint32_t *)(idx->vaddr + (y * idx->scanline + xt));
		mi = mx[0] = mx[1] = 0;
    for (x=xt; x<=xb; x+=4) {
			uint32_t c0, c1, c2, c3, d0, d1, d2, d3;
      c0 = *sp; sp += qfscan;
      c1 = *sp; sp += qfscan;
      c2 = *sp; sp += qfscan;
      c3 = *sp; sp -= qfscan * 7;
      d0 = (((c3 >> 0)  & 0xff) | ((c2 << 8)  & 0xff00) | ((c1 << 16) & 0xff0000) | ((c0 << 24) & 0xff000000)) >> 4;
      d1 = (((c3 >> 8)  & 0xff) | ((c2 >> 0)  & 0xff00) | ((c1 << 8)  & 0xff0000) | ((c0 << 16) & 0xff000000)) >> 4;
      d2 = (((c3 >> 16) & 0xff) | ((c2 >> 8)  & 0xff00) | ((c1 >> 0)  & 0xff0000) | ((c0 << 8)  & 0xff000000)) >> 4;
      d3 = (((c3 >> 24) & 0xff) | ((c2 >> 16) & 0xff00) | ((c1 >> 8)  & 0xff0000) | ((c0 >> 0)  & 0xff000000)) >> 4;
			if (((d0^dp[qiscan*0]) | (d1^dp[qiscan*1]) | (d2^dp[qiscan*2]) | (d3^dp[qiscan*3])) & 0x0f0f0f0f) { mx[mi] = x; mi = 1; }
      dp[qiscan*0] = ((dp[qiscan*0] << 4) & 0xf0f0f0f0) | (d0 & 0x0f0f0f0f);
      dp[qiscan*1] = ((dp[qiscan*1] << 4) & 0xf0f0f0f0) | (d1 & 0x0f0f0f0f);
      dp[qiscan*2] = ((dp[qiscan*2] << 4) & 0xf0f0f0f0) | (d2 & 0x0f0f0f0f);
      dp[qiscan*3] = ((dp[qiscan*3] << 4) & 0xf0f0f0f0) | (d3 & 0x0f0f0f0f);
      dp++;
    }
		if (mi) {
			if (mx[1] < mx[0]) mx[1] = mx[0];
			if (uarea->x_top > mx[0]) uarea->x_top = mx[0];
			if (uarea->x_bottom < mx[1] + (4-1)) uarea->x_bottom = mx[1] + (4-1);
			if (uarea->y_bottom == 0) uarea->y_top = y;
			uarea->y_bottom = y + (4-1);
		}
    y += 4;
  }
	return 0xffffffff; // assume GC
}

uint32_t axi_update_index(struct eink_image_slot *fb, struct index_buffer *idx, struct area_info *area)
{
	uint32_t *sp, *dp;

	int xt = area->x_top;
	int yt = area->y_top;
	int xb = area->x_bottom;
	int yb = area->y_bottom;
	int x=xt, y=yt;

	struct area_info uarea = {
		.x_top = xb,
		.x_bottom = xt,
		.y_top = 0,
		.y_bottom = 0,
	};

	uint32_t mi, mx[2], hyst=0;

	AXIDBG("axi_update_index: +(%d,%d,%d,%d) %d\n",
			area->x_top, area->y_top,
			area->x_bottom+1-area->x_top, area->y_bottom+1-area->y_top,
			fb->orientation);

	switch (fb->orientation) {
		case FB_ROTATE_CW:
			hyst = rotate_cw_4x4(fb, idx, &uarea, xt, yt, xb, yb);
			break;
		case FB_ROTATE_CCW:
			hyst = rotate_ccw_4x4(fb, idx, &uarea, xt, yt, xb, yb);
			break;
		case FB_ROTATE_UD:
			for (y=yt; y<=yb; y++) {
				sp = (uint32_t *)(fb->vaddr + (y * fb->scanline + xt));
				dp = (uint32_t *)(idx->vaddr + (y * idx->scanline + xt));
				mi = mx[0] = mx[1] = 0;
				for (x=xt; x<=xb; x+=4) {
					uint32_t vc = ((*sp++) >> 4) & 0x0f0f0f0f;
					uint32_t vp = *dp & 0x0f0f0f0f;
					if ((vc ^ vp)) { mx[mi] = x; mi = 1; }
					*dp++ = (vp << 4) | vc;
					hyst |= ((vc + 0x01010101) & 0x0e0e0e0e);
				}
				if (mi) {
					if (mx[1] < mx[0]) mx[1] = mx[0];
					if (uarea.x_top > mx[0]) uarea.x_top = mx[0];
					if (uarea.x_bottom < mx[1] + (4-1)) uarea.x_bottom = mx[1] + (4-1);
					if (uarea.y_bottom == 0) uarea.y_top = y;
					uarea.y_bottom = y;
				}
			}
			break;
		case FB_ROTATE_UR:
			for (y=yt; y<=yb; y++) {
				sp = (uint32_t *)(fb->vaddr + ((fb->height - y - 1) * fb->scanline + (fb->width - xt - 4)));
				dp = (uint32_t *)(idx->vaddr + (y *idx->scanline + xt));
				mi = mx[0] = mx[1] = 0;
				for (x=xt; x<=xb; x+=4) {
					uint32_t vc = (byte_rev(*sp--) >> 4) & 0x0f0f0f0f;
					uint32_t vp = *dp & 0x0f0f0f0f;
					if ((vc ^ vp)) { mx[mi] = x; mi = 1; }
					*dp++ = (vp << 4) | vc;
					hyst |= ((vc + 0x01010101) & 0x0e0e0e0e);
				}
				if (mi) {
					if (mx[1] < mx[0]) mx[1] = mx[0];
					if (uarea.x_top > mx[0]) uarea.x_top = mx[0];
					if (uarea.x_bottom < mx[1] + (4-1)) uarea.x_bottom = mx[1] + (4-1);
					if (uarea.y_bottom == 0) uarea.y_top = y;
					uarea.y_bottom = y;
				}
			}
			break;
	}

	if (area->mode == 0) {
		if (uarea.y_bottom == 0) return 0;
		area->x_top = uarea.x_top & ~3;
		area->x_bottom = uarea.x_bottom | 3;
		area->y_top = uarea.y_top & ~3;
		area->y_bottom = uarea.y_bottom | 3;
	}

	AXIDBG("axi_update_index: =(%d,%d,%d,%d) h=%x\n",
			area->x_top, area->y_top,
			area->x_bottom+1-area->x_top, area->y_bottom+1-area->y_top,
			hyst
	);
	IDUMP("- idx", idx->vaddr, 16);

	axi_collision_exclude(area);
	return hyst ? 2 : 1;
}

static void axi_copy(struct eink_image_slot *src, struct eink_image_slot *dest, struct area_info *area)
{
	uint32_t *sp, *dp;
	int y;

	int xt = area->x_top;
	int yt = area->y_top;
	int xb = area->x_bottom;
	int yb = area->y_bottom;

	for (y=yt; y<=yb; y++) {
		sp = (uint32_t *)(src->vaddr + (y * src->scanline + xt));
		dp = (uint32_t *)(dest->vaddr + (y * dest->scanline + xt));
		memcpy(dp, sp, xb+1-xt);
	}
}

void axi_new_pipeline(int num, struct area_info *area)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&axilock, flags);
	pipelines[num] = *area;
	axi_bit_set(active_mask, num);
	spin_unlock_irqrestore(&axilock, flags);
}

void axi_remove_pipeline(int num)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&axilock, flags);
	axi_bit_clear(active_mask, num);
	spin_unlock_irqrestore(&axilock, flags);
}

void axi_collision_add(struct eink_image_slot *fb, struct eink_image_slot *shadow, struct area_info *area)
{
	int i, avl, s, smin;
	unsigned long flags = 0;

	if (fb == shadow) return;   // don't generate collision on collision
	if (fb->orientation != FB_ROTATE_UD) return; // do it only in native orientation

	spin_lock_irqsave(&axilock, flags);
	avl = -1;
	for (i=0; i<MAX_COLLISIONS; i++) {
		if (! axi_bit_test(collision_mask, i)) {
			if (avl < 0) avl = i;
			continue;
		}
		if (axi_rect_covers(&collisions[i], area)) {
			// previous collision covers a new one, don't add
			axi_best_waveform(area, &collisions[i]);
			AXIDBG("covered by collision %d\n", i);
			goto axa_out;
		} else if (axi_rect_covers(area, &collisions[i])) {
			// new collision covers previous one, fix wf and discard previous
			axi_best_waveform(area, &collisions[i]);
			AXIDBG("discard collision %d\n", i);
			axi_bit_clear(collision_mask, i);			
		} else if (axi_rect_overlaps(&collisions[i], area)) {
			// overlapped collision, just fix wf
			axi_best_waveform(area, &collisions[i]);
		}
	}

	// new collision
	if (avl >= 0) {
		AXIDBG("add collision %d\n", avl);
		collisions[avl] = *area;
		axi_bit_set(collision_mask, avl);
		goto axa_out;
	}

	// no free collisions, merge with closest one
	avl = -1;
	for (i=0; i<MAX_COLLISIONS; i++) {
		s = axi_rect_sum(&collisions[i], area);
		if (avl < 0 || s < smin) {
			avl = i;
			smin = s;
		}
	}
	AXIDBG("merge collision %d\n", avl);
	axi_rect_merge(&collisions[avl], area);

axa_out:
	//axi_copy(fb, shadow, &collisions[avl]);
	spin_unlock_irqrestore(&axilock, flags);
}

bool axi_collision_fetch(struct area_info *area)
{
	int i, j;
	unsigned long flags = 0;

	spin_lock_irqsave(&axilock, flags);
	for (i=0; i<MAX_COLLISIONS; i++) {
		if (! axi_bit_test(collision_mask, i)) continue;
		for (j=0; j<MAX_PIPELINES; j++) {
			if (! axi_bit_test(active_mask, j)) continue;
			if (axi_rect_overlaps(&collisions[i], &pipelines[j])) break;
		}
		if (j == MAX_PIPELINES) {
			*area = collisions[i];
			axi_bit_clear(collision_mask, i);
			spin_unlock_irqrestore(&axilock, flags);
			AXIDBG("fetch collision %d\n", i);
			return true;
		}
	}
	spin_unlock_irqrestore(&axilock, flags);
	return false;
}

void axi_collision_exclude(struct area_info *area)
{
	int i;
	unsigned long flags = 0;

	spin_lock_irqsave(&axilock, flags);
	for (i=0; i<MAX_COLLISIONS; i++) {
		if (! axi_bit_test(collision_mask, i)) continue;
		if (axi_rect_covers(area, &collisions[i])) axi_bit_clear(collision_mask, i);
	}
	spin_unlock_irqrestore(&axilock, flags);
}

static inline void print_area(char *buf, struct area_info *area)
{
	*buf = 0;
	if (area == NULL || axi_rect_empty(area)) return;
	sprintf(buf, "%d,%d,%d,%d", area->x_top,area->y_top,area->x_bottom+1-area->x_top,area->y_bottom+1-area->y_top);
}

void axi_split_area(struct area_info *src_area, struct area_info *upd_areas, int *nupd, struct area_info *coll_areas, int *ncoll)
{
	char buf0[24], buf1[24], buf2[24], buf3[24];
	int i, j;

	// up to 2 update areas, up to 1 collision area

	upd_areas[0] = upd_areas[1] = *src_area;
	coll_areas[0] = *src_area;
	*nupd = *ncoll = 1;

	// break areas by running pipelines

	for (i=0; i<MAX_PIPELINES; i++) {
		if (! axi_bit_test(active_mask, i)) continue;
		axi_rect_break(upd_areas, nupd, &pipelines[i], true);
		if (! *nupd) break;
	}

	// check if some area left to update later

	for (i=0; i<*nupd; i++) {
		axi_rect_break(coll_areas, ncoll, &upd_areas[i], false);
		if (! *ncoll) break;
	}

	print_area(buf0, src_area);
	print_area(buf1, *nupd > 0 ? &upd_areas[0] : NULL);
	print_area(buf2, *nupd > 1 ? &upd_areas[1] : NULL);
	print_area(buf3, *ncoll > 0 ? &coll_areas[0]: NULL);
	AXIDBG("axi_split_area (%s) -> U(%s)(%s) C(%s)\n", buf0, buf1, buf2, buf3);
	return;
}

void axi_init(void)
{
	AXIDBG("axi_init\n");
	spin_lock_init(&axilock);
}

void axi_cleanup(void)
{
	AXIDBG("axi_cleanup\n");
	memset(pipelines, 0, sizeof(pipelines));
	memset(collisions, 0, sizeof(collisions));
}

