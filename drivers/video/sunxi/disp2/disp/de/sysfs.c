#include "../eink.h"

#define EPDC_RO_ATTRIBUTE(name, mode) \
    static struct kobj_attribute name##_attribute = __ATTR(name, mode, name##_show, NULL);

#define EPDC_WO_ATTRIBUTE(name, mode) \
    static struct kobj_attribute name##_attribute = __ATTR(name, mode, NULL, name##_store);

#define EPDC_RW_ATTRIBUTE(name, mode) \
    static struct kobj_attribute name##_attribute = __ATTR(name, mode, name##_show, name##_store);

#define EPDC_TIMING_ATTRIBUTE(name, mode, var) \
    static ssize_t name##_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) \
    { \
        return scnprintf(buf, PAGE_SIZE, "%d\n", var); \
    } \
    static ssize_t name##_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) \
    { \
        return update_timing_value(&(var), buf, count); \
    } \
    static struct kobj_attribute name##_attribute = __ATTR(name, mode, name##_show, name##_store);

#define EPDC_TIMING_RO_ATTRIBUTE(name, mode, var) \
    static ssize_t name##_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) \
    { \
        return scnprintf(buf, PAGE_SIZE, "%d\n", var); \
    } \
    static struct kobj_attribute name##_attribute = __ATTR(name, mode, name##_show, NULL);

#define EPDC_REG_ATTRIBUTE(name, mode, var) \
    static ssize_t name##_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) \
    { \
        return scnprintf(buf, PAGE_SIZE, "%d\n", var); \
    } \
    static ssize_t name##_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) \
    { \
        return update_ee_value(&(var), buf, count); \
    } \
    static struct kobj_attribute name##_attribute = __ATTR(name, mode, name##_show, name##_store);

static ssize_t update_timing_value(int *var, const char *buf, size_t count)
{
    long value;
    //epdc_lock_driver(1);
    if (strict_strtol(buf, 0, &value)) {
        //epdc_lock_driver(0);
        return -EINVAL;
    }
    *var = value;
    //epdc_lock_driver(0);
    epdc_update_timings();
    return count;
}

static ssize_t update_ee_value(int *var, const char *buf, size_t count)
{
    long value;
    if (strict_strtol(buf, 0, &value)) {
        return -EINVAL;
    }
    *var = value;
    return count;
}

static ssize_t framerate_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return  scnprintf(buf, PAGE_SIZE, "%d\n", wfdata.framerate);
}

static ssize_t sdoe_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return  scnprintf(buf, PAGE_SIZE, "%s\n",
        (eink_param.timing.sdoe_toggle == 0) ? "line" : "frame");
}

static ssize_t sdoe_mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == 'l' || buf[0] == 'L') update_timing_value(&(eink_param.timing.sdoe_toggle), "0", 1);
    if (buf[0] == 'f' || buf[0] == 'F') update_timing_value(&(eink_param.timing.sdoe_toggle), "1", 1);
    return count;
}

static ssize_t vcom_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int vcom = -eink_param.vcom;
    return  scnprintf(buf, PAGE_SIZE, "-%d.%02d\n", vcom / 1000, (vcom / 10) % 100);
}

static ssize_t vcom_setup(const char *buf, size_t count, int perm)
{
    int vcom;
    const char *p = buf;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '-' || *(p+1) < '0' || *(p+1) > '3') return -EINVAL;
    vcom = (*(p+1) - '0') * 1000;
    p += 2;
    if (*p) {
        if (*(p++) != '.') return -EINVAL;
        if (*p >= '0' && *p <= '9') vcom += (*p - '0') * 100;
        if (*(p+1) >= '0' && *(p+1) <= '9') vcom += (*(p+1) - '0') * 10;
    }
    eink_param.vcom = -vcom;
    eink_param.vcom_offset = 0;
    if (perm) eink_param.vcom_permanent = -vcom;
    pmic->epdc_pmic_vcom_set(eink_param.vcom, 0, perm);
    return count;
}

static ssize_t vcom_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    return vcom_setup(buf, count, 0);
}

static ssize_t vcom_permanent_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", eink_param.vcom_permanent);
}

static ssize_t vcom_permanent_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    return vcom_setup(buf, count, 1);
}

static ssize_t vtouch_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", eink_param.vtouch);
}

static ssize_t vbus_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", eink_param.vbus);
}

static ssize_t temperature_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", eink_param.temperature);
}

static ssize_t temperature_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    eink_param.temperature = simple_strtoul(buf, NULL, 10);
    eink_param.temp_fixed = true;
    return count;
}

static ssize_t tempsensor_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%s\n", eink_param.temp_fixed ? "fixed" : "ambient");
}

static ssize_t tempsensor_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == 'f' || buf[0] == 'F') { eink_param.temp_fixed = true; return count; }
    if (buf[0] == 'a' || buf[0] == 'A') { eink_param.temp_fixed = false; return count; }
    return -EINVAL;
}

static ssize_t strength_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return  scnprintf(buf, PAGE_SIZE, "%d\n", eink_param.strength);
}

static ssize_t strength_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    eink_param.strength = simple_strtoul(buf, NULL, 10);
    if (eink_param.strength < 0 || eink_param.strength > 3) eink_param.strength = 3;
    return count;
}

static ssize_t debuglevel_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", epdc_debuglevel);
}

static ssize_t debuglevel_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    epdc_debuglevel = simple_strtoul(buf, NULL, 10);
    return count;
}

static ssize_t dump_updates_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    eink_param.dump_updates = simple_strtoul(buf, NULL, 10);
    return count;
}

static ssize_t waveform_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", wfdata.name);
}

static ssize_t waveform_info_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, len=0;

	len += scnprintf(buf+len, PAGE_SIZE-len, "Waveform name:  %s\n", wfdata.name);
	len += scnprintf(buf+len, PAGE_SIZE-len, "Waveform type:  0x%02x\n", wfdata.type);
	len += scnprintf(buf+len, PAGE_SIZE-len, "Panel size:     %d.%d\n",
		wfdata.panel_size / 10, wfdata.panel_size % 10);
	len += scnprintf(buf+len, PAGE_SIZE-len, "AMEPD part no:  0x%02x\n", wfdata.amepd_part);
	len += scnprintf(buf+len, PAGE_SIZE-len, "Applied params: %s\n", wfdata.amepd_applied);
	len += scnprintf(buf+len, PAGE_SIZE-len, "Frame rate:     %d Hz\n", wfdata.framerate);
	len += scnprintf(buf+len, PAGE_SIZE-len, "Timing mode:    %d\n", wfdata.timing_mode);
  len += scnprintf(buf+len, PAGE_SIZE-len, "Mode version:   0x%02x\n", wfdata.mode_ver);
	len += scnprintf(buf+len, PAGE_SIZE-len, "Waveforms:      ");
	for (i=0; i<MAX_WF_MODES; i++) {
		if (wfdata.ready[i]) len += scnprintf(buf+len, PAGE_SIZE-len, "%d ", i);
	}
	len += scnprintf(buf+len, PAGE_SIZE-len, "\nTemp ranges:   ");
	for (i=0; i<wfdata.ntemp+1; i++) {
		len += scnprintf(buf+len, PAGE_SIZE-len, "%d ", wfdata.tempindex[i]);
	}
	len += scnprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}

static void read_next_value(const char **s, unsigned long *ptr, int radix, unsigned long deflt)
{
	const char *p = *s;
	*ptr = deflt;
	while (*p != 0 && *p <= ' ') p++;
	if (*p != 0) *ptr = simple_strtoul(p, (char **)&p, radix);
	*s = p;
}

static ssize_t fill_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long x, y, w, h, i;
	unsigned long pattern;
	const char *p = buf;
	u8 *ptr;

	read_next_value(&p, &x, 10, 0);
	read_next_value(&p, &y, 10, 0);
	read_next_value(&p, &w, 10, 0);
	read_next_value(&p, &h, 10, 0);
	read_next_value(&p, &pattern, 16, 0xffffffff);
	if (x < 0 || y < 0 || w <= 0 || h <= 0 || x+w > framebuffer_slot.width || y+h > framebuffer_slot.height) return -EINVAL;
	while (h-- > 0) {
		ptr = (u8 *)framebuffer_slot.vaddr + y * framebuffer_slot.scanline + x;
		for (i=0; i<w; i++) {
			ptr[i] = (u8)((i&1) ? (pattern >> 8) : pattern);
		}
		y++;
		pattern = ((pattern >> 16) & 0xffff) | (pattern << 16);
	}
	return count;
}

static ssize_t update_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct area_info area;
	unsigned long x, y, w, h, wf, mode;
	uint32_t flags = 0;
	const char *p = buf;

	read_next_value(&p, &x, 0, 0);
	read_next_value(&p, &y, 0, 0);
	read_next_value(&p, &w, 0, 0);
	read_next_value(&p, &h, 0, 0);
	read_next_value(&p, &wf, 0, 3);
	read_next_value(&p, &mode, 0, EPDC_FULL);
	area.x_top = x;
	area.y_top = y;
	area.x_bottom = x + w - 1;
	area.y_bottom = y + h - 1;
	area.wf = wf;
	area.mode = mode;
	eink_update_image(&area, &flags);
	return count;
}

static ssize_t register_dump_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long addr, cnt;
	int i;
	const char *p = buf;

	read_next_value(&p, &addr, 16, 0);
	read_next_value(&p, &cnt, 16, 1);
	if (addr == 0 || cnt > 256) return -EINVAL;
	for (i=0; i<cnt; i++) {
		printk(KERN_ERR "%08x: %08x\n", addr, *((volatile unsigned long *)addr));
		addr += 4;
	}
	return count;
}


static ssize_t register_write_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long addr, value;
	const char *p = buf;

	read_next_value(&p, &addr, 16, 0);
	read_next_value(&p, &value, 16, 0);
	if (addr == 0) return -EINVAL;
	*((volatile unsigned long *)addr) = value;
	return count;
}

static ssize_t wf_binary_read(struct file *f, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t size)
{
	if (! wfdata.binary) return 0;
	if (off >= wfdata.binary_length) return 0;
	if (off + size > wfdata.binary_length) size = wfdata.binary_length - off;
	memcpy(buf, wfdata.binary + off, size);
	return size;
}

static u8 *wf_update_buffer = NULL;
static int wf_update_size = 0;

static ssize_t wf_binary_write(struct file *f, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t size)
{
	if (off == 0) {
		if (size < 8) goto err;
		wf_update_size = *((u32 *)(buf + 4));
		if (wf_update_size < 64 || wf_update_size > 1024000) goto err;
		if (wf_update_buffer) vfree(wf_update_buffer);
		wf_update_buffer = (u8 *) vmalloc(wf_update_size);
		if (! wf_update_buffer) goto err;
	} else {
		if (!wf_update_buffer) goto err;
	}
	if (off >= wf_update_size) return 0;
	if (off + size > wf_update_size) size = wf_update_size - off;
	memcpy(wf_update_buffer + off, buf, size);
	if (off + size == wf_update_size) {
		int ret = epdc_waveform_parse(wf_update_buffer, wf_update_size);
		if (ret != 0) vfree(wf_update_buffer);
		wf_update_buffer = NULL;
		wf_update_size = 0;
		//wfdata.status = (ret == 0) ? EPDC_WFSTATUS_UPDATED : EPDC_WFSTATUS_ERROR;
                //epdc_pmic_pwrseq();
	}
	return size;
err:
	if (wf_update_buffer) vfree(wf_update_buffer);
	wf_update_buffer = NULL;
	wf_update_size = 0;
	return -EIO;
}


EPDC_TIMING_ATTRIBUTE(fsl, 0644, eink_param.timing.fsl);
EPDC_TIMING_ATTRIBUTE(fbl, 0644, eink_param.timing.fbl);
EPDC_TIMING_RO_ATTRIBUTE(fdl, 0444, eink_param.timing.fdl);
EPDC_TIMING_ATTRIBUTE(fel, 0644, eink_param.timing.fel);
EPDC_TIMING_ATTRIBUTE(lsl, 0644, eink_param.timing.lsl);
EPDC_TIMING_ATTRIBUTE(lbl, 0644, eink_param.timing.lbl);
EPDC_TIMING_RO_ATTRIBUTE(ldl, 0444, eink_param.timing.ldl);
EPDC_TIMING_ATTRIBUTE(lel, 0644, eink_param.timing.lel);
EPDC_TIMING_ATTRIBUTE(gdoe_sta, 0644, eink_param.timing.gdoe_sta);
EPDC_TIMING_ATTRIBUTE(sdoe_sta, 0644, eink_param.timing.sdoe_sta);
EPDC_TIMING_ATTRIBUTE(gdck_sta, 0644, eink_param.timing.gdck_sta);
EPDC_TIMING_ATTRIBUTE(gdck_high, 0644, eink_param.timing.gdck_high);
EPDC_TIMING_ATTRIBUTE(gdsp_offset, 0644, eink_param.timing.gdsp_offset);
EPDC_TIMING_ATTRIBUTE(pixclk, 0644, eink_param.timing.pixclk);
EPDC_TIMING_ATTRIBUTE(slow_motion, 0644, eink_param.timing.slow_motion);
EPDC_RO_ATTRIBUTE(framerate, 0444);
EPDC_RW_ATTRIBUTE(sdoe_mode, 0644);

EPDC_REG_ATTRIBUTE(img_width, 0644, eink_param.ee.img_width);
EPDC_REG_ATTRIBUTE(img_height, 0644, eink_param.ee.img_height);
EPDC_REG_ATTRIBUTE(wav_pitch, 0644, eink_param.ee.wav_pitch);
EPDC_REG_ATTRIBUTE(wav_offset, 0644, eink_param.ee.wav_offset);
EPDC_REG_ATTRIBUTE(edma_wav_width, 0644, eink_param.ee.edma_wav_width);
EPDC_REG_ATTRIBUTE(edma_wav_height, 0644, eink_param.ee.edma_wav_height);
EPDC_REG_ATTRIBUTE(edma_img_x, 0644, eink_param.ee.edma_img_x);
EPDC_REG_ATTRIBUTE(edma_img_y, 0644, eink_param.ee.edma_img_y);
EPDC_REG_ATTRIBUTE(edma_img_w, 0644, eink_param.ee.edma_img_w);
EPDC_REG_ATTRIBUTE(edma_img_h, 0644, eink_param.ee.edma_img_h);

EPDC_REG_ATTRIBUTE(tconx, 0644, eink_param.tcon.width);
EPDC_REG_ATTRIBUTE(tcony, 0644, eink_param.tcon.height);
EPDC_REG_ATTRIBUTE(ht, 0644, eink_param.tcon.ht);
EPDC_REG_ATTRIBUTE(hbp, 0644, eink_param.tcon.hbp);
EPDC_REG_ATTRIBUTE(hspw, 0644, eink_param.tcon.hspw);
EPDC_REG_ATTRIBUTE(vt, 0644, eink_param.tcon.vt);
EPDC_REG_ATTRIBUTE(vbp, 0644, eink_param.tcon.vbp);
EPDC_REG_ATTRIBUTE(vspw, 0644, eink_param.tcon.vspw);

EPDC_RO_ATTRIBUTE(waveform_name, 0444);
EPDC_RO_ATTRIBUTE(waveform_info, 0444);
EPDC_RW_ATTRIBUTE(vcom, 0666);
EPDC_RW_ATTRIBUTE(vcom_permanent, 0644);
EPDC_RO_ATTRIBUTE(vtouch, 0444);
EPDC_RO_ATTRIBUTE(vbus, 0444);
EPDC_RW_ATTRIBUTE(temperature, 0644);
EPDC_RW_ATTRIBUTE(tempsensor, 0644);
EPDC_RW_ATTRIBUTE(strength, 0644);
EPDC_RW_ATTRIBUTE(debuglevel, 0644);
EPDC_WO_ATTRIBUTE(fill, 0222);
EPDC_WO_ATTRIBUTE(update, 0222);
EPDC_WO_ATTRIBUTE(dump_updates, 0222);

EPDC_WO_ATTRIBUTE(register_dump, 0200);
EPDC_WO_ATTRIBUTE(register_write, 0200);

static struct bin_attribute waveform_binary_attribute = {
    .attr = { .name = "waveform_binary", .mode = 0644 },
    .read = wf_binary_read,
    .write = wf_binary_write,
    .size = 0,
};

static struct attribute *global_attrs[] = {

    &waveform_name_attribute.attr,
    &waveform_info_attribute.attr,
    &vcom_attribute.attr,
    &vcom_permanent_attribute.attr,
    &vtouch_attribute.attr,
    &vbus_attribute.attr,
    &temperature_attribute.attr,
    &tempsensor_attribute.attr,
    &strength_attribute.attr,
    &debuglevel_attribute.attr,
    &fill_attribute.attr,
    &update_attribute.attr,
    &dump_updates_attribute.attr,
#if 1
    &register_dump_attribute.attr,
    &register_write_attribute.attr,
#endif
    NULL

};

static struct attribute *timings_attrs[] = {

    &framerate_attribute.attr,
    &fsl_attribute.attr,
    &fbl_attribute.attr,
    &fdl_attribute.attr,
    &fel_attribute.attr,
    &lsl_attribute.attr,
    &lbl_attribute.attr,
    &ldl_attribute.attr,
    &lel_attribute.attr,
    &gdck_sta_attribute.attr,
    &gdck_high_attribute.attr,
    &gdoe_sta_attribute.attr,
    &sdoe_sta_attribute.attr,
    &sdoe_mode_attribute.attr,
    &gdsp_offset_attribute.attr,
    &pixclk_attribute.attr,
    &slow_motion_attribute.attr,
    NULL

};

static struct attribute *ee_attrs[] = {

	&img_width_attribute.attr,
	&img_height_attribute.attr,
	&wav_pitch_attribute.attr,
	&wav_offset_attribute.attr,
	&edma_wav_width_attribute.attr,
	&edma_wav_height_attribute.attr,
	&edma_img_x_attribute.attr,
	&edma_img_y_attribute.attr,
	&edma_img_w_attribute.attr,
	&edma_img_h_attribute.attr,
	NULL

};

static struct attribute *tcon_attrs[] = {

	&tconx_attribute.attr,
	&tcony_attribute.attr,
	&ht_attribute.attr,
	&hbp_attribute.attr,
	&hspw_attribute.attr,
	&vt_attribute.attr,
	&vbp_attribute.attr,
	&vspw_attribute.attr,
	NULL

};

static struct attribute_group epdc_sysfs_global_group = {

    .attrs = global_attrs,

};

static struct attribute_group epdc_sysfs_timings_group = {

    .name = "timings",
    .attrs = timings_attrs,

};

static struct attribute_group epdc_sysfs_ee_group = {

    .name = "ee",
    .attrs = ee_attrs,

};

static struct attribute_group epdc_sysfs_tcon_group = {

    .name = "tcon",
    .attrs = tcon_attrs,

};

int epdc_sysfs_create_groups(struct kobject *kobj)
{
    int err = 0;
    err |= sysfs_create_group(kobj, &epdc_sysfs_global_group);
    err |= sysfs_create_group(kobj, &epdc_sysfs_timings_group);
    err |= sysfs_create_group(kobj, &epdc_sysfs_ee_group);
    err |= sysfs_create_group(kobj, &epdc_sysfs_tcon_group);
    err |= sysfs_create_bin_file(kobj, &waveform_binary_attribute);
    return err;
}

