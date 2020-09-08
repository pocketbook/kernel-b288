/*
Description: awf waveform file decoder
Version	: v0.1
Author	: oujunxi
Date	: 2015/07/10
*/

#include "../eink.h"

#define WF_IS5BIT (1 << 0)
#define WF_GSCONV (1 << 1)
#define WF_GLCONV (1 << 2)

struct wf_mode {
	u8 type;
	s8 idx[MAX_WF_MODES];
};

static const struct wf_mode default_wmodes =
	{ 0x00, { 0,   1,   2,   2,   2,   1,   -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } };

static const struct wf_mode wmodes[] = {
	//        INIT DU   GC16 GC4  A2   GL16 A2IN A2OUT
	{ 0x00, { 0,   1,   3,   3,   1,   3,   -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x01, { 0,   1,   2,   3,   1,   2,   -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x02, { 0,   1,   3,   3,   1,   3,   -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x03, { 0,   1,   2,   2,   4,   2,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x04, { 0,   1,   2,   2,   4,   5,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x05, { 0,   1,   2,   2,   4,   5,    6,   7,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x07, { 0,   1,   2,   3,   4,   5,    1,   3,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x09, { 0,   1,   2,   2,   4,   3,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x12, { 0,   1,   2,   7,   4,   5,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x13, { 0,   1,   2,   2,   4,   2,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x15, { 0,   1,   2,   5,   4,   3,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x18, { 0,   1,   2,   2,   6,   3,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x19, { 0,   1,   2,   2,   6,   3,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x20, { 0,   1,   2,   2,   6,   3,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
	{ 0x23, { 0,   1,   2,   2,   4,   3,    1,   2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 } },
  { 0xff }
};

struct waveform_data wfdata;

static u8 *empty_vaddr = NULL;
static phys_addr_t empty_paddr;

static void check_black_flashing(u8 *data, int len)
{
	int i, nb=0, nw=0, nwb=0;

	for (i=0; i<len; i+=256) {
		if (data[i] == 0 && nb+nw > 0) break;
		if (data[i] == 0x01) nb++;
		if (data[i] == 0x02) nw++;
		if (i > 0 && data[i-256] == 0x02 && data[i] == 0x01) nwb++;
	}
	if (nwb > 1) {
		i = 0;
		while (data[i * 256] == 0) i++;
		while (nw-- > 0) data[(i++) * 256] = 0x02;
		while (nb-- > 0) data[(i++) * 256] = 0x01;
	}
}

static int conv_5to4(u8 *data, int len)
{
	u8 *sp = data;
	u8 *dp = data;
	int nb = len;
	int x, y;

	while (nb >= 1024) {
		for (y=0; y<16; y++) {
			for (x=0; x<16; x++) {
				dp[x] = sp[x*2];
			}
			sp += 64;
			dp += 16;
		}
		nb -= 1024;
	}
	return len / 4;
}

static void mirror_pages(u8 *dest, u8 *src, int len, u32 flags)
{
	int n = (flags & WF_IS5BIT) ? 32 : 16;
	int pg = n * n;
	int x, y;

	while (len >= pg) {
		for (y=0; y<n; y++) {
			for (x=0; x<n; x++) {
				dest[y*n+x] = src[x*n+y];
			}
		}
		if (flags & WF_GSCONV) {
			for (x=0; x<n; x++) dest[x*n+x] = 0;
		} else if (flags & WF_GLCONV) {
			dest[pg-1] = 0;
		}
		src += pg;
		dest += pg;
		len -= pg;
	}
}

static inline int read3chk(u8 *p, int off)
{
	int v;
	u8 cksum;

	p += off;
	v = *p | (*(p+1)<<8) | (*(p+2)<<16);
	cksum = (*p + *(p+1) + *(p+2)) & 0xff;
	return (cksum == *(p+3)) ? v : -1;
}

static int voltage(unsigned char *wfdata, int off, int step)
{
	int vcdata = *((unsigned short *)(wfdata+off));
	if (vcdata & 0x8000) {
		vcdata &= 0x7fff;
		step = - step;
	}
	return (vcdata * step) / 1000;
}

static int read_wbf(unsigned char *ptr, int off, unsigned char *out, int maxlen)
{
	unsigned char *p, ec, mc, c;
	int mode, len, n;

	ec = ptr[0x28];
	mc = ptr[0x29];

	p = ptr + off;
	len = 0;
	mode = 0;

	while (1) {
		c = *p++;
		if (c == ec) break;
		if (c == mc) {
			mode = 1-mode;
			continue;
		}
		if (mode == 0) {
			n = *(p++) + 1;
		} else {
			n = 1;
		}
		len += n * 4;
		if (len >= maxlen) return 0;
		while (n-- > 0) {
			*out++ = c & 3;
			*out++ = (c >> 2) & 3;
			*out++ = (c >> 4) & 3;
			*out++ = (c >> 6) & 3;
		}
	}
	return len;
}

static u8 *allocate_waveform(int wf, int temp, int len, int vc)
{
	phys_addr_t paddr;
	int tf = len / wfdata.pagesize;

	u8 *vaddr = (u8 *)disp_malloc(len, &paddr);
	if (! vaddr) return NULL;
	wfdata.vaddr[wf][temp] = vaddr;
	wfdata.paddr[wf][temp] = paddr;
	wfdata.total[wf][temp] = tf;
	wfdata.vcomoffset[wf][temp] = vc;
	DBG("wf=%d temp=%d addr=%x frames=%d vcomof=%d\n", wf, temp, paddr, tf, vc);
	return vaddr;
}

static void free_waveform(int wf, int temp)
{
	if (wfdata.vaddr[wf][temp] && wfdata.vaddr[wf][temp] != empty_vaddr) {
		disp_free(wfdata.vaddr[wf][temp], (void *)wfdata.paddr[wf][temp], wfdata.total[wf][temp] * wfdata.pagesize);
	}
	wfdata.vaddr[wf][temp] = NULL;
	wfdata.paddr[wf][temp] = 0;
}

int epdc_waveform_parse(u8 *data, int len)
{
	u8 *tbuf=NULL, *vaddr;
	int wlen, wfcount, ntemp, offset, i, v, is5bit, vcoffs, hdrver;
	int wf, iwf, temp, xwia, nlen, vc;
	const s8 *wmm;

	epdc_waveform_unload();

	tbuf = vmalloc(MAX_WF_SIZE);
	if (! tbuf) goto err_nomem;

	hdrver = data[0x2a];
	is5bit = ((data[0x24] & 0xc) != 0);
	wfdata.pagesize = 256;

	wfcount = data[0x25] + 1;
	if (wfcount <= 1 || wfcount > MAX_WF_MODES) goto err_wf;

	ntemp = data[0x26] + 1;
	if (ntemp <= 1 || ntemp >= MAX_TEMP_RANGES-1) goto err_wf;

	wfdata.mode_ver = data[0x10];
	wfdata.type = data[0x13];
	wfdata.panel_size = data[0x14];
	wfdata.amepd_part = data[0x15];
	wfdata.awv = data[0x27];
	if (hdrver == 0) {
		v = data[0x17];
		wfdata.framerate = ((v >> 4) & 0x0f) * 10 + (v & 0x0f);
		if (wfdata.framerate == 0) wfdata.framerate = 50;
		wfdata.timing_mode = data[0x18];
	} else {
		wfdata.framerate = data[0x18];
		wfdata.timing_mode = 0;
	}

	wfdata.ntemp = ntemp;

	xwia = data[0x1c] + (data[0x1d] << 8) + (data[0x1e] << 16);
	nlen = xwia ? data[xwia] : 0;
	if (nlen >= 1 && data[xwia+1] != 0) {
		if (nlen > 63) nlen = 63;
		memcpy(wfdata.name, &data[xwia+1], nlen);
		wfdata.name[nlen] = 0;
	}

	for (i=0; i<ntemp+1; i++) wfdata.tempindex[i] = data[0x30 + i];

	// setup waveform modes for different wf types
	wmm = default_wmodes.idx;
	for (i=0; wmodes[i].type!=0xff; i++) {
		if (wmodes[i].type == data[0x10]) {
			wmm = wmodes[i].idx;
			break;
		}
	}

	INF("  waveform name:  %s\n", wfdata.name);
	INF("  waveform type:  %d\n", wfdata.type);
	INF("  bit count:      %d\n", is5bit ? 5 : 4);
	INF("  frame rate:     %d\n", wfdata.framerate);
	INF("  timing mode:    %d\n", wfdata.timing_mode);
	INF("  AMEPD part:     0x%02x\n", wfdata.amepd_part);
	INF("  mode count:     %d\n", wfcount);
	INF("  temp ranges:    %d\n", ntemp);
	INF("  AWV:            %d\n", wfdata.awv);
	INF("  mode version:   0x%x\n", wfdata.mode_ver);
	INF("  modes:          DU=%d GC16=%d GC4=%d GL16=%d A2=%d A2IN=%d A2OUT=%d\n",
		wmm[EPDC_WFTYPE_DU], wmm[EPDC_WFTYPE_GC16], wmm[EPDC_WFTYPE_GC4],
		wmm[EPDC_WFTYPE_GL16], wmm[EPDC_WFTYPE_A2], wmm[EPDC_WFTYPE_A2IN],
		wmm[EPDC_WFTYPE_A2OUT]
	);

	if (! empty_vaddr) {
		empty_vaddr = (u8 *)disp_malloc(wfdata.pagesize, &empty_paddr);
	}

	for (wf=1; wf<MAX_WF_MODES; wf++) {
		iwf = wmm[wf];
		if (iwf < 0 || iwf >= wfcount) continue;
		for (temp=0; temp<ntemp; temp++) {
			offset = data[0x20] + (data[0x21] << 8);
			if (offset < 0 || offset >= len-4) goto err_wf;
			offset = read3chk(data, offset + iwf * 4);
			if (offset < 0 || offset >= len-4) goto err_wf;
			offset = read3chk(data, offset + temp * 4);
			if (offset < 0 || offset >= len-4) goto err_wf;
			vcoffs = (wfdata.awv == 1 || wfdata.awv == 3) ? offset - 16 : 0;
			wlen = read_wbf(data, offset, tbuf, MAX_WF_SIZE);
			if (wlen <= 0 || (wlen & 0xff) != 0) goto err_wf;
			if (is5bit) wlen = conv_5to4(tbuf, wlen);
			//if (((wlen/wfdata.pagesize) & 1) != 0) {
			//	memset(tbuf+wlen, 0, wfdata.pagesize);
			//	wlen += wfdata.pagesize;
			//}
			//if (wf == EPDC_WFTYPE_GC16 || wf == EPDC_WFTYPE_GC4) check_black_flashing(tbuf, wlen);
			vc = vcoffs ? voltage(data, vcoffs+10, 3125) : 0;
			vaddr = allocate_waveform(wf, temp, wlen, vc);
			if (! vaddr) goto err_nomem;
			mirror_pages(vaddr, tbuf, wlen, (wf == EPDC_WFTYPE_GL16) ? WF_GLCONV : 0);
			if (wf == EPDC_WFTYPE_GC16) {
				vaddr = allocate_waveform(EPDC_WFTYPE_GS16, temp, wlen, vc);
				if (! vaddr) goto err_nomem;
				mirror_pages(vaddr, tbuf, wlen, WF_GSCONV);
			}
		}
		wfdata.ready[wf] = 1;
		if (wf == EPDC_WFTYPE_GC16) wfdata.ready[EPDC_WFTYPE_GS16] = 1;
	}

	wfdata.binary = data;
	wfdata.binary_length = len;

	INF("waveforms loaded\n");

	vfree(tbuf);
	return 0;

err_wf:
	ERR("error in waveform file\n");
	goto err_out;
err_nomem:
	ERR("could not allocate memory for waveform\n");
err_out:
	if (tbuf) vfree(tbuf);
	return -1;
}

int epdc_waveform_load_from_memory(phys_addr_t addr, size_t size)
{
	uint8_t *data = (uint8_t *) phys_to_virt(addr);

	INF("loading waveform from memory: 0x%08x\n", addr);
	return epdc_waveform_parse(data, size);
}

int epdc_waveform_load(const char *filename)
{
	mm_segment_t fs;
	struct file *fp;
	loff_t pos = 0;
	u8 header[512], *data=NULL;
	int len, n, ret=-1;

	INF("loading waveform file: %s\n", filename);

	fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		fp = NULL;
		ERR("Could not open waveform file\n");
		goto err1;
	}

	n = vfs_read(fp, header, 512, &pos);
	if (n != 512) {
		ERR("could not read waveform header\n");
		goto err2;
	}

	len = *((u32 *)(header+4));
	if (len < 1024 || len > 1024000) {
		ERR("wrong waveform size (%d)\n", len);
		goto err2;
	}
	INF("waveform size: %d\n", len);

	data = vmalloc(len);
	if (data == NULL) {
		ERR("could not allocate memory\n");
		goto err2;
	}

	memcpy(data, header, 512);
	n = vfs_read(fp, data+512, len-512, &pos);
	if (n != len-512) {
		ERR("could not read waveform file (%d)\n", n);
		goto err2;
	}

	ret = epdc_waveform_parse(data, len);

err2:
	filp_close(fp, NULL);
err1:
	set_fs(fs);
	if (data && ret) vfree(data);
	return ret;
}

void epdc_waveform_unload(void)
{
	int i, j;

	if (wfdata.binary) vfree(wfdata.binary);
	for (i=0; i<MAX_WF_MODES; i++) {
		for (j=0; j<wfdata.ntemp; j++) {
			free_waveform(i, j);
		}
	}
	memset(&wfdata, 0, sizeof(wfdata));
}

int epdc_waveform_tempindex(int t)
{
	int i;

	for (i=0; i<wfdata.ntemp-1; i++) {
		if (t < wfdata.tempindex[i+1]) break;
	}
	i += eink_param.temp_offset;
	if (i < 0) i = 0;
	if (i > wfdata.ntemp-1) i = wfdata.ntemp-1;
	return i;
}

int epdc_get_waveform_bits(void)
{
	// now all wf's converted to 4-bit
	return 4;
}

phys_addr_t epdc_get_waveform_data(int wf, int updmode, u32 temp, u32 *total_frames)
{
	int idx = epdc_waveform_tempindex(temp);
	if (wf == EPDC_WFTYPE_GC16 && updmode == 0) wf = EPDC_WFTYPE_GS16;
	if (wfdata.vaddr[wf][idx]) {
		*total_frames = wfdata.total[wf][idx];
		return wfdata.paddr[wf][idx];
	} else {
		*total_frames = 1;
		return empty_paddr;
	}
}

bool epdc_waveform_supported(int wf)
{
	return wfdata.ready[wf];
}

int epdc_vcom_read(int vcom_default)
{
	static char vcombuf[16];
	struct file *fp;
	mm_segment_t old_fs;
	loff_t pos = 0;
	int vcom;

	fp = filp_open(VCOM_PATH, O_RDONLY, 0);
	if(IS_ERR(fp)) {
		INF("Using internal vcom value %d\n", vcom_default);
		vcom = vcom_default;
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		vfs_read(fp, vcombuf, 15, &pos);
		vcom = simple_strtol(vcombuf, NULL, 10);
		filp_close(fp, NULL);
		set_fs(old_fs);
		if (vcom < -3500 || vcom > -500) {
			ERR("wrong VCOM value %d, using default value %d\n", vcom, vcom_default);
			vcom = vcom_default;
		} else {
			INF("Using VCOM value from %s: %d\n", VCOM_PATH, vcom);
		}
	}
	return vcom;
}

