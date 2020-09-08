#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#ifdef DEBUG
#define DBG(msg, ...) do { \
	printk("[%s] "msg"\n",__func__, ## __VA_ARGS__); \
} while (0)
#else
#define DBG(msg, ...) {}
#endif

#define INF(msg, ...) do { \
		printk("[%s] "msg"\n",__func__, ## __VA_ARGS__); \
} while (0)

#define ERR(msg, ...) do { \
		printk("[%s] ERROR "msg"\n",__func__, ## __VA_ARGS__); \
} while (0)


extern int Get_ADC_mVoltage(void);

/*
 * Detects PCB version by ADC value
 */
const char *GetPCBVersion(void) {
	int r = -1;
	static const char *ret = "unknown";
	r = Get_ADC_mVoltage();
	pr_debug("adc voltage = %i\n", r);
	switch (r) {
/*
0в - 0.25в / v.1.0
0.3в - 0.55в / v.1.1
0.6в - 0.85в / v.1.2
0.9в - 1.15в / v.1.3
1.2в - 1.45в / v.1.4
1.5в - 1.75в / v.1.5
1.8в - 2.05в / v.1.6
2.1в - 2.35в / v.1.7
2.4в - 2.65в / v.1.8
2.7в - 3.00в / v.1.9
*/
	case 0 ... 250:
	pr_debug("[%s] PCB version v1.0\n", __func__);
	ret = "1.0";
	break;
	case 300 ... 550:
	pr_debug("[%s] PCB version v1.1\n", __func__);
	ret = "1.1";
	break;
	case 600 ... 850:
	pr_debug("[%s] PCB version v1.2\n", __func__);
	ret = "1.2";
	break;
	case 900 ... 1150:
	pr_debug("[%s] PCB version v1.3\n", __func__);
	ret = "1.3";
	break;
	case 1200 ... 1450:
	pr_debug("[%s] PCB version v1.4\n", __func__);
	ret = "1.4";
	break;
	case 1500 ... 1750:
	pr_debug("[%s] PCB version v1.5\n", __func__);
	ret = "1.5";
	break;
	case 1800 ... 2050:
	pr_debug("[%s] PCB version v1.6\n", __func__);
	ret = "1.6";
	break;
	case 2100 ... 2350:
	pr_debug("[%s] PCB version v1.7\n", __func__);
	ret = "1.7";
	break;
	case 2400 ... 2650:
	pr_debug("[%s] PCB version v1.8\n", __func__);
	ret = "1.8";
	break;
	case 2700 ... 3000:
	pr_debug("[%s] PCB version v1.9\n", __func__);
	ret = "1.9";
	break;

	default:
		pr_warn("[%s] Detection fails! ADC info is out of ranges.",__func__);
		break;
	}
	pr_debug("[%s] pcb_version = %s\n",__func__,ret);
	return ret;
}
EXPORT_SYMBOL(GetPCBVersion);

static ssize_t proc_pcb_read(struct file *filp, char *buf, size_t count, loff_t *offp ) {
	int err;

	//exit if buffer is not empty
	if ((int)*offp > 0)
		return 0;

	const char *ver = GetPCBVersion();
	int len = strlen(ver);

	if (count < len)
		return 0;

	err = copy_to_user(buf, ver, len);
	if (err)
		return -EFAULT;
	*offp = len;
	return len;
}

static struct file_operations proc_fops = {
    .read = proc_pcb_read,
};

int pcb_version_init()
{
	struct proc_dir_entry *file;

	file = proc_create_data("pcb_version", 0444, NULL, &proc_fops, NULL);
	if (!file) {
		ERR("could not create /proc/pcb_version\n");
		return -1;
	}

	pr_debug("[%s] registered\n",__func__);
	return 0;
}

late_initcall(pcb_version_init);

/*
 * touch screen power (enable\disable) api
 */
struct hwpower_s {
	struct mutex mut;
	struct proc_dir_entry *file;
	struct file_operations proc_fops;
	void *data_cb; //context
	int (*state)(void *data_cb); //read power state
	int (*enable)(void *data_cb, bool onOff); //set power state
};
static struct hwpower_s *touch;

static ssize_t proc_touch_write(struct file *filp, const char __user * buffer, size_t count, loff_t *pos) {
	char buf[16];
	int current_state = -1;

	struct hwpower_s *d = PDE_DATA(file_inode(filp));

	if (count > sizeof(buf) -1)
		return -EINVAL;

	if (!count)
		return 0;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';

	mutex_lock(&d->mut);
	if(d->state)
		current_state = (d->state)(d->data_cb);

	switch (buf[0]) {
	case '0':
		if (current_state != 0 && (d->enable))
			(d->enable)(d->data_cb,false);

		break;
	case '1':
		if (current_state != 1 && (d->enable))
			(d->enable)(d->data_cb,true);

		break;
	default:
		ERR("wrong input");
		break;
	}
	mutex_unlock(&d->mut);
	return count;
}

static ssize_t proc_touch_read(struct file *filp, char *buf, size_t count, loff_t *offp ) {
	int err;
	int state = -1;
	char tbuf[16];
	int len;

	//exit if buffer is not empty
	if ((int)*offp > 0)
		return 0;

	struct hwpower_s *d = PDE_DATA(file_inode(filp));

	mutex_lock(&d->mut);
	if(d->state)
		state = (d->state)(d->data_cb);

	len = snprintf(tbuf, sizeof(tbuf), "%i\n", state);
	mutex_unlock(&d->mut);

	if (count < len)
		return 0;

	err = copy_to_user(buf, tbuf, len);
	if (err)
		return -EFAULT;

	*offp = len;
	return len;
}

/*
 * register touch screen callbacks set power state and get power state
 * unregister callbacks make call register_keylock_callback(NULL,NULL,NULL)
 */
int register_touchpower_callback(int (*touch_enable_cb)(void *data, bool onOff), int (*touch_state_cb)(void *data), void *touch_context) {
	if (touch == NULL)
		return -ENOENT;

	mutex_lock(&touch->mut);
	touch->data_cb = touch_context;
	touch->enable = touch_enable_cb;
	touch->state = touch_state_cb;
	mutex_unlock(&touch->mut);
	return 0;
}
EXPORT_SYMBOL(register_touchpower_callback);

static int proc_hwpower_init(void) {
	struct proc_dir_entry *dir, *file;

	dir = proc_mkdir("power", NULL);
	if (!dir) {
		ERR("could not create /proc/power");
		return -1;
	}

	touch = kmalloc(sizeof(struct hwpower_s), __GFP_ZERO);
	if (!touch) {
		remove_proc_entry("power",NULL);
		return -2;
	}
	touch->proc_fops.read = proc_touch_read;
	touch->proc_fops.write = proc_touch_write;

	file = proc_create_data("touch", 0666, dir, &(touch->proc_fops), touch);
	if (!file) {
		ERR("could not create /proc/power/touch\n");
		remove_proc_entry("touch",dir);
		remove_proc_entry("power",NULL);
		return -3;
	}

	mutex_init(&(touch->mut));
	touch->file = file;

	return 0;
}

late_initcall(proc_hwpower_init);
