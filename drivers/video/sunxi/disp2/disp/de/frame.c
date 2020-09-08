#include "../eink.h"

#define PIX_CE                   (SDCE                                  | DMASK8)
#define PIX_CE_SP                (SDCE        | SPV                     | DMASK8)
#define PIX_CE_GD_SP_CK          (SDCE | GDOE | SPV               | CKV | DMASK8)
#define PIX_CE_GD_SP             (SDCE | GDOE | SPV                     | DMASK8)
#define PIX_CE_GD_CK             (SDCE | GDOE                     | CKV | DMASK8)
#define PIX_CE_GD                (SDCE | GDOE                           | DMASK8)
#define PIX_CE_GD_SD_SP_LE       (SDCE | GDOE | SPV | SDOE | SDLE       | DMASK8)
#define PIX_CE_GD_SD_SP_CK_LE    (SDCE | GDOE | SPV | SDOE | SDLE | CKV | DMASK8)
#define PIX_CE_GD_SD_SP          (SDCE | GDOE | SPV | SDOE              | DMASK8)
#define PIX_CE_GD_SD_SP_CK       (SDCE | GDOE | SPV | SDOE        | CKV | DMASK8)
#define PIX_CE_SP_CK             (SDCE        | SPV               | CKV | DMASK8)
#define PIX_CE_GD_SP_LE          (SDCE | GDOE | SPV        | SDLE       | DMASK8)
#define PIX_CE_GD_SP_CK_LE       (SDCE | GDOE | SPV        | SDLE | CKV | DMASK8)
#define PIX_CE_CK                (SDCE                            | CKV | DMASK8)
#define PIX_GD_SP_CK             (       GDOE | SPV               | CKV | DMASK8)
#define PIX_GD_SP                (       GDOE | SPV                     | DMASK8)
#define PIX_LDL1                 (       GDOE | SPV | SDOE        | CKV | DMASK8)
#define PIX_LDL2                 (SDCE | GDOE | SPV | SDOE        | CKV | DMASK8)
#define PIX_LDL3                 (       GDOE | SPV | SDOE              | DMASK8)

struct wavedata_queue {
  unsigned int     head;
  unsigned int     tail;
  void            *vaddr[WAVE_DATA_BUF_NUM+1];
  phys_addr_t      paddr[WAVE_DATA_BUF_NUM+1]; // frame control data
  bool             used[WAVE_DATA_BUF_NUM+1];
  phys_addr_t      empty_paddr;
  size_t           buffer_size;
  spinlock_t       slock;
};


static struct wavedata_queue wavering;      // wave data buffer queue

static int init_frame(void *wavedata_buf, int m16, struct eink_timing_param *eink_timing_info)
{
	int width = eink_timing_info->width;
	int height = eink_timing_info->height;

	int line_toggle = (eink_timing_info->sdoe_toggle == 0);

	int fsl = eink_timing_info->fsl;
	int fbl = eink_timing_info->fbl;
	int fel = eink_timing_info->fel;

	int ldl = width / 4;
	int lsl = m16 ? eink_timing_info->lsl * 2 : eink_timing_info->lsl;
	int lbl = m16 ? eink_timing_info->lbl * 2 : eink_timing_info->lbl;
	int lel = m16 ? eink_timing_info->lel * 2 : eink_timing_info->lel;
	int sdoe_sta = m16 ? eink_timing_info->sdoe_sta * 2: eink_timing_info->sdoe_sta;
	int gdoe_sta = m16 ? eink_timing_info->gdoe_sta * 2: eink_timing_info->gdoe_sta;
	int gdck_sta = m16 ? eink_timing_info->gdck_sta * 2: eink_timing_info->gdck_sta;
	int gdck_high = m16 ? eink_timing_info->gdck_high * 2: eink_timing_info->gdck_high;
	int gdsp_offset = eink_timing_info->gdsp_offset;

	int w = lsl + lbl + ldl + lel;
	int pad = 0;
	int i, line, igdck, isdoe;
	int igdoe = fsl-gdoe_sta;
	uint16_t spv1, spv2, spv3, spv4;

	uint16_t *p = (uint16_t *) wavedata_buf;
	uint16_t *pp;

	// empty lines
	for (line=0; line<eink_timing_info->vblank; line++) {
		for (i=0; i<w; i++) *p++ = PIX_CE_SP;
		p += pad;
	}

	// fsl
	for (line=0; line<fsl-1; line++) {
		igdck = -(lsl + gdck_sta);
		spv1 = (igdoe < 0) ? PIX_CE_SP_CK : PIX_CE_GD_SP_CK;
		spv2 = (igdoe < 0) ? PIX_CE_SP : PIX_CE_GD_SP;
		for (i=0; i<w; i++) {
			*p++ = (igdck >= 0 && igdck < gdck_high) ? spv1 : spv2;
			igdck++;
		}
		igdoe++;
		p += pad;
	}

	// start spv pulse
	spv1 = (igdoe < 0) ? PIX_CE_SP : PIX_CE_GD_SP;
	spv2 = (igdoe < 0) ? PIX_CE_SP_CK : PIX_CE_GD_SP_CK;
	spv3 = (igdoe < 0) ? PIX_CE_CK : PIX_CE_GD_CK;
	spv4 = (igdoe < 0) ? PIX_CE : PIX_CE_GD;
	igdck = -(lsl + gdck_sta);
	for (i=0; i<w; i++) {
		*p++ = (igdck >= 0 && igdck < gdck_high) ?
			((igdck < (gdck_high * gdsp_offset) / 100) ? spv2 : spv3) :
			((igdck < 0) ? spv1 : spv4);
		igdck++;
	}
	igdoe++;
	p += pad;

	// end spv pulse
	spv1 = (igdoe < 0) ? PIX_CE : PIX_CE_GD;
	spv2 = (igdoe < 0) ? PIX_CE_CK : PIX_CE_GD_CK;
	spv3 = (igdoe < 0) ? PIX_CE_SP_CK : PIX_CE_GD_SP_CK;
	spv4 = (igdoe < 0) ? PIX_CE_SP: PIX_CE_GD_SP;
	igdck = -(lsl + gdck_sta);
	for (i=0; i<w; i++) {
		*p++ = (igdck >= 0 && igdck < gdck_high) ?
			((igdck < (gdck_high * gdsp_offset) / 100) ? spv2 : spv3) :
			((igdck < 0) ? spv1 : spv4);
		igdck++;
	}
	igdoe++;
	p += pad;

	// fbl
	for (line=0; line<fbl-2; line++) {
		igdck = -(lsl + gdck_sta);
		spv1 = (igdoe < 0) ? PIX_CE_SP_CK : PIX_CE_GD_SP_CK;
		spv2 = (igdoe < 0) ? PIX_CE_SP : PIX_CE_GD_SP;
		for (i=0; i<w; i++) {
			*p++ = (igdck >= 0 && igdck < gdck_high) ? spv1 : spv2;
			igdck++;
		}
		igdoe++;
		p += pad;
	}

	// last fbl pulse with first data line

	igdck = -(lsl + gdck_sta);
	for (i=0; i<lsl+lbl; i++) {
		*p++ = (igdck >= 0 && igdck < gdck_high) ? PIX_CE_GD_SP_CK : PIX_CE_GD_SP;
		igdck++;
	}
	for (i=0; i<ldl; i++) {
		*p++ = (igdck >= 0 && igdck < gdck_high) ? PIX_GD_SP_CK : PIX_GD_SP;
		igdck++;
	}
	for (i=0; i<lel; i++) {
		*p++ = (igdck >= 0 && igdck < gdck_high) ? PIX_CE_GD_SP_CK : PIX_CE_GD_SP;
		igdck++;
	}
	p += pad;


	// data

	igdck = -(lsl + gdck_sta);
	isdoe = -(lsl + sdoe_sta);

	pp = p;
	for (i=0; i<lsl; i++) {
		*p++ = (line_toggle && isdoe < 0) ?
			((igdck >= 0 && igdck < gdck_high) ? PIX_CE_GD_SP_CK_LE : PIX_CE_GD_SP_LE) :
			((igdck >= 0 && igdck < gdck_high) ? PIX_CE_GD_SD_SP_CK_LE : PIX_CE_GD_SD_SP_LE) ;
		igdck++;
		isdoe++;
	}
	for (i=0; i<lbl; i++) {
		*p++ = (line_toggle && isdoe < 0) ?
			((igdck >= 0 && igdck < gdck_high) ? PIX_CE_GD_SP_CK : PIX_CE_GD_SP) :
			((igdck >= 0 && igdck < gdck_high) ? PIX_CE_GD_SD_SP_CK : PIX_CE_GD_SD_SP) ;
		igdck++;
		isdoe++;
	}
	for (i=0; i<ldl; i++) {
		*p++ = (igdck >= 0 && igdck < gdck_high) ? PIX_LDL1 : PIX_LDL3;
		igdck++;
	}
	while (p-pp < w) {
		*p++ = line_toggle ?
			((igdck >= 0 && igdck < gdck_high) ? PIX_CE_GD_SP_CK : PIX_CE_GD_SP) :
			((igdck >= 0 && igdck < gdck_high) ? PIX_CE_GD_SD_SP_CK : PIX_CE_GD_SD_SP) ;
		igdck++;
	}
	p = pp + (w + pad);

	for (line=1; line<height; line++) {
		memcpy(p, pp, w * sizeof(uint16_t));
		p += (w + pad);
	}

	// last line without SDCE
	pp = p - (w + pad);
	while (pp < p - pad) {
		if (*pp == PIX_LDL1) *pp = PIX_CE_GD_SD_SP_CK;
		if (*pp == PIX_LDL3) *pp = PIX_CE_GD_SD_SP;
		pp++;
	}

	// fel
	for (line=0; line<fel+1; line++) {
		for (i=0; i<w; i++) *p++ = PIX_CE_SP;
		p += pad;
	}
	return 0;
}

#define check_bounds(par, min, max) { if (par < (min)) par = (min); if (par > (max)) par = (max); }

static void setup_ee_params(void)
{
	int vblank = eink_param.timing.vblank;
	int fbl = eink_param.timing.fbl;
	int fsl = eink_param.timing.fsl;
	int fel = eink_param.timing.fel;
	int lsl = eink_param.timing.lsl;
	int lbl = eink_param.timing.lbl;
	int lel = eink_param.timing.lel;
	int hsync = lbl + lsl + lel;
	int vsync = vblank + fbl + fsl + fel;

	eink_param.ee.img_width = eink_param.timing.width;
	eink_param.ee.img_height = eink_param.timing.height;
	eink_param.ee.wav_pitch = ((lbl+lsl+lel) << (eink_param.bus_width ? 2 : 1)) + (eink_param.timing.width>>1);
	eink_param.ee.wav_offset = (vblank + fbl + fsl - 1) * eink_param.ee.wav_pitch + ((lbl + lsl) << (eink_param.bus_width ? 2 : 1));
	eink_param.ee.edma_wav_width = eink_param.timing.width + (eink_param.bus_width ? (hsync<<3) : (hsync<<2));
	eink_param.ee.edma_wav_height = eink_param.timing.height + vsync;
	eink_param.ee.edma_img_x = (lbl + lsl) << (eink_param.bus_width ? 3 : 2);
	eink_param.ee.edma_img_y = vblank + fbl + fsl - 1;
	eink_param.ee.edma_img_w = eink_param.timing.width;
	eink_param.ee.edma_img_h = eink_param.timing.height;
}

static void setup_tcon_params(void)
{
	eink_param.tcon.width = eink_param.timing.lbl + eink_param.timing.lsl + eink_param.timing.ldl + eink_param.timing.lel;
	eink_param.tcon.height =  eink_param.timing.vblank + eink_param.timing.fbl + eink_param.timing.fsl + eink_param.timing.fdl + eink_param.timing.fel;
	eink_param.tcon.ht = eink_param.tcon.width + 4;
	eink_param.tcon.hbp = 2;
	eink_param.tcon.hspw = 4;
	eink_param.tcon.vt = eink_param.tcon.height + 6;
	eink_param.tcon.vbp = 2;
	eink_param.tcon.vspw = 2;
}

void epdc_update_timings(void)
{
	int i;

	check_bounds(eink_param.timing.fsl, 1, EPDC_MAX_FSL);
	check_bounds(eink_param.timing.fbl, 1, EPDC_MAX_FBL);
	check_bounds(eink_param.timing.fel, 1, EPDC_MAX_FEL);
	check_bounds(eink_param.timing.lsl, 1, EPDC_MAX_LSL);
	check_bounds(eink_param.timing.lbl, 1, EPDC_MAX_LBL);
	check_bounds(eink_param.timing.lel, 1, EPDC_MAX_LEL);
	check_bounds(eink_param.timing.gdck_sta, -eink_param.timing.lsl, eink_param.timing.ldl-1);
	check_bounds(eink_param.timing.gdck_high, 1, eink_param.timing.ldl+EPDC_MAX_LEL-eink_param.timing.gdck_sta-1);
	check_bounds(eink_param.timing.gdsp_offset, 0, 100);
	check_bounds(eink_param.timing.sdoe_toggle, 0, 1);
	check_bounds(eink_param.timing.gdoe_sta, 0, eink_param.timing.fsl+eink_param.timing.fbl);
	check_bounds(eink_param.timing.sdoe_sta, -eink_param.timing.lsl, eink_param.timing.lbl);

	init_frame(wavering.vaddr[0], eink_param.bus_width, &eink_param.timing);
	for (i = 1; i <= WAVE_DATA_BUF_NUM; i++) {
		memcpy(wavering.vaddr[i], wavering.vaddr[0], wavering.buffer_size);
	}

	setup_ee_params();
	setup_tcon_params();
}

int epdc_init_wavering(void)
{
	int hsyncmax = EPDC_MAX_LBL + EPDC_MAX_LSL + EPDC_MAX_LEL;
	int vsyncmax = 4 + EPDC_MAX_FBL + EPDC_MAX_FSL + EPDC_MAX_FEL;
	int i;

	if (eink_param.bus_width)
		wavering.buffer_size = 4 * (eink_param.timing.width/8 + hsyncmax) * (eink_param.timing.height + vsyncmax);  // 16 bit bus
	else
		wavering.buffer_size = 2 * (eink_param.timing.width/4 + hsyncmax) * (eink_param.timing.height + vsyncmax);  // 8 bit bus

	spin_lock_init(&wavering.slock);
	wavering.head = 0;
	wavering.tail = 0;

	for (i=0; i<=WAVE_DATA_BUF_NUM; i++) {
		wavering.vaddr[i] = (void*) disp_malloc(wavering.buffer_size, &wavering.paddr[i]);
		if (! wavering.vaddr[i]) {
			ERR("malloc eink wavedata memory fail, size=%d, id=%d\n", wavering.buffer_size, i);
			return -ENOMEM;
		}
		memset((void*)wavering.vaddr[i], 0, wavering.buffer_size);
		DBG("wavering[%d]=%p/%x size=%d\n", i, wavering.vaddr[i], wavering.paddr[i], wavering.buffer_size);
	}
	wavering.empty_paddr = wavering.paddr[WAVE_DATA_BUF_NUM];
	return 0;
}

void clear_wavedata_buffer(void)
{
        int i = 0;
        unsigned long flags = 0;

        DBG("clear_wavedata_buffer\n");
        spin_lock_irqsave(&wavering.slock, flags);
        wavering.head = wavering.tail = 0;
        for (i = 0; i < WAVE_DATA_BUF_NUM; i++) wavering.used[i] = false;
        spin_unlock_irqrestore(&wavering.slock, flags);
}

/* return a physic address for tcon used to display wavedata,then dequeue wavedata buffer. */
phys_addr_t request_buffer_for_display(void)
{
        unsigned long flags = 0;
        phys_addr_t ret = 0;

        spin_lock_irqsave(&wavering.slock, flags);
        if (wavering.head != wavering.tail) {
                ret = wavering.paddr[wavering.tail];
        }
        spin_unlock_irqrestore(&wavering.slock, flags);
        //DBG("request_buffer_for_display=%x\n", ret);
        return ret;
}

phys_addr_t epdc_wavering_get_empty(void)
{
	return wavering.empty_paddr;
}

/* return a physic address for eink engine used to decode one frame, then queue wavedata buffer. */
phys_addr_t request_buffer_for_decode(void)
{
        unsigned long flags = 0;
        phys_addr_t ret;

        spin_lock_irqsave(&wavering.slock, flags);
        ret = ((wavering.head + 2)%WAVE_DATA_BUF_NUM == wavering.tail || wavering.used[wavering.head]) ? 0 : wavering.paddr[wavering.head];
        spin_unlock_irqrestore(&wavering.slock, flags);
        //DBG("request_buffer_for_decode=%x\n", ret);
        return ret;
}

int queue_wavedata_buffer(void)
{
        int ret = 0;
        unsigned long flags = 0;

        spin_lock_irqsave(&wavering.slock, flags);
        if ((wavering.head + 2)%WAVE_DATA_BUF_NUM == wavering.tail) {
                /* queue full */
                ret =  -EBUSY;
        } else {
                /* set used status true */
                wavering.used[wavering.head] = true;
                wavering.head = (wavering.head + 1) % WAVE_DATA_BUF_NUM;
        }
        spin_unlock_irqrestore(&wavering.slock, flags);
        //DBG("queue_wavedata_buffer=%d\n", ret);
        return ret;
}

int dequeue_wavedata_buffer(void)
{
        int ret = 0;
        unsigned long flags = 0;

        spin_lock_irqsave(&wavering.slock, flags);
        if (wavering.head == wavering.tail) {
                /* queue empty */
                ret =  -EBUSY;
        } else {
                wavering.tail = (wavering.tail + 1) % WAVE_DATA_BUF_NUM;
        }
        spin_unlock_irqrestore(&wavering.slock, flags);
        //DBG("dequeue_wavedata_buffer=%d\n", ret);
        return ret;
}

int clean_used_wavedata_buffer(void)
{
        int ret = 0;
        unsigned long flags = 0;

        spin_lock_irqsave(&wavering.slock, flags);
        if (wavering.tail >= 2) {
                wavering.used[wavering.tail - 2] = false;
        } else {
                wavering.used[wavering.tail + WAVE_DATA_BUF_NUM - 2] = false;
        }
        spin_unlock_irqrestore(&wavering.slock, flags);
        //DBG("clean_used_wavedata_buffer\n");
        return ret;
}

