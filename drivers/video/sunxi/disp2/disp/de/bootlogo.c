#include "../eink.h"

static inline int read4(u8 *p, int off)
{
    return p[off] + (p[off+1] << 8) + (p[off+2] << 16) + (p[off+3] << 24);
}

static int check_parameter(u8 **ptr, char *name, int u, int *param)
{
    char *p = (char *) *ptr;
    if (strncmp(p, name, strlen(name)) != 0) return 0;
    p += strlen(name);
    *param = u ? simple_strtoul(p, &p, 0) : simple_strtol(p, &p, 0);
    *ptr = (u8 *) p;
    DBG("** %s=%d\n", name, *param);
    return 1;
}

static void epdc_read_logo(char *name, u8 *addr, size_t maxsize)
{
	struct file *fp;
	mm_segment_t old_fs;
	loff_t pos = 0;
	int i;
	ssize_t len;

	for (i=0; i<15; i++) {
		fp = filp_open(name, O_RDONLY, 0);
		if(IS_ERR(fp)) {
			msleep(100);
			continue;
		}
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		len = vfs_read(fp, addr, maxsize-1, &pos);
		filp_close(fp, NULL);
		set_fs(old_fs);
		if (len > 0) addr[len] = 0;
		INF("read %s (%d bytes)\n", name, len);
		return;
	}
	ERR("cannot read %s\n", name);
}

int epdc_bootlogo_thread(void *p_arg)
{
	u8 *logo_addr = (u8 *) phys_to_virt(EPDC_BOOTLOGO_ADDR);
	int logo_size = EPDC_BOOTLOGO_SIZE;

	struct area_info area;
	u8 *data = logo_addr;
	u8 *loopentry = NULL;
	int loop = 0;
	int timeout = 250;
	int waveform = EPDC_WFTYPE_GC16;
	u32 flags;
	int full = 1;
	int waitlut = 1;
	int xoffset = -1;
	int yoffset = -1;
	int fsize, start, hsize, width, height, bpp, comp, scanlen, npal, i, xx, x, yy, y;
	int filler = 0xff;
	int first = 1;
	u32 ref, pattern;
	u8 *src, *dest, c, *p1, *p2;
	u8 palette[256];

	int type = *((int *)p_arg);
	if (! type) return 0;

	msleep(50);

	INF("EPDC bootlogo start (%d)\n", type);

	if (type == 2) {
		epdc_read_logo("/boot/charge.bmp", logo_addr, logo_size);
	} else if (type == 3) {
		epdc_read_logo("/boot/battery_low.bmp", logo_addr, logo_size);
	}

	while (1) {

	if (data >= &logo_addr[logo_size]) {
		if (! loopentry) break;
		data = loopentry;
	}

	if (data[0] == '\r' || data[0] == '\n') {
		data++;
		continue;
	}

	if (data[0] == '%') {
		data++;
		check_parameter(&data, "waveform=", 0, &waveform);
		check_parameter(&data, "full=", 0, &full);
		check_parameter(&data, "wait=", 0, &waitlut);
		check_parameter(&data, "timeout=", 0, &timeout);
		check_parameter(&data, "xoffset=", 0, &xoffset);
		check_parameter(&data, "yoffset=", 0, &yoffset);
		if (check_parameter(&data, "loop", 0, &loop)) {
			loopentry = data;
		}
		if (check_parameter(&data, "filler=", 1, &filler)) {
			memset(shadow_slot.vaddr, filler, shadow_slot.memsize);
		}
		if (check_parameter(&data, "pattern=", 1, &pattern)) {
			for (y=0; y<shadow_slot.height; y+=2) {
				p1 = shadow_slot.vaddr + y * shadow_slot.scanline;
				p2 = p1 + shadow_slot.scanline;
				for (x=0; x<shadow_slot.width; x+=2) {
					*p1++ = (pattern & 0xff);
					*p1++ = ((pattern >> 8) & 0xff);
					*p2++ = ((pattern >> 16) & 0xff);
					*p2++ = ((pattern >> 24) & 0xff);
				}
			}
		}
		if (check_parameter(&data, "reference=", 1, &ref)) {
			//epdc_init_indexmap(ref);
		}
		continue;
	} else	if (data[0] != 'B' || data[1] != 'M') {
		if (! loopentry) break;
		data = loopentry;
	}

		fsize = read4(data,0x02);
		start = read4(data,0x0a);
		hsize = read4(data,0x0e);
		width = read4(data,0x12);
		height = read4(data,0x16);
		bpp = *((short *)(data+0x1c));
		comp = read4(data,0x1e);
		scanlen = ((width * bpp + 7) / 8 + 3) & ~0x3;
		DBG("** bitmap %dx%d bpp%d\n", width, height, bpp);
		if (width < 4 || width > shadow_slot.width) break;
		if (height < 4 || height > shadow_slot.height) break;
		if (fsize < 40 || data+fsize > logo_addr+logo_size) break;
		if (start < 40 || start > fsize - height*scanlen) break;
		if (hsize != 40) break;
		if (comp != 0) break;
		if (bpp != 8 && bpp != 4) break;

		src = data + 14 + hsize;
		npal = (bpp == 4) ? 16 : 256;
		for (i=0; i<npal; i++) {
			palette[i] = (src[0] + src[1]*6 + src[2]*3) / 10;
			src += 4;
		}

		xx = (xoffset >= 0) ? xoffset : (shadow_slot.width - width) / 2;
		yy = (yoffset >= 0) ? yoffset : (shadow_slot.height - height) / 2;
		if (xx < 0) xx = 0;
		if (yy < 0) yy = 0;

		for (y=0; y<height && y<shadow_slot.height; y++) {
			dest = shadow_slot.vaddr + (yy + y) * shadow_slot.scanline + xx;
			src = data + start + (height - y - 1) * scanlen;
			for (x=0; x<width && x<shadow_slot.width; x++) {
				c = (bpp == 4) ? ((src[x/2] << ((x&1)*4)) >> 4) & 0xf : src[x];
				*dest++ = palette[c];
			}
		}

		flags = full ? (EPDC_FULL | EPDC_LOGO) : EPDC_LOGO;
		if (full || first) {
			area.x_top = 0;
			area.y_top = 0;
			area.x_bottom = shadow_slot.width-1;
			area.y_bottom = shadow_slot.height-1;
		} else {
			area.x_top = xx;
			area.y_top = yy;
			area.x_bottom = xx+width-1;
			area.y_bottom = yy+height-1;
		}
		area.wf = waveform;
		area.mode = full ? 1 : 0;
		if (eink_update_image(&area, &flags) != 0) break;
		first = 0;
		msleep(timeout);
		if (waitlut) eink_wait_complete();

		data += fsize;
	}

	INF("EPDC bootlogo complete (%p)\n", data);

	memblock_free(EPDC_BOOTLOGO_ADDR, EPDC_BOOTLOGO_SIZE);
	return 0;
}

