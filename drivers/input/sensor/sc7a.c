#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>

#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/ioctl.h>

#include <linux/init-input.h>
#include <linux/sys_config.h>

#include <linux/gpio.h>

#include <linux/power/scenelock.h>

//#define DBG(s...)
#define DBG(s...) printk(s)
#define INF(s...) printk(s)
#define ERR(s...) printk(s)

#define SC7A_NAME "sc7a"

#define SC7A_I2C_ADDR 0x18

#define SC7A_SWAP_XY

#define SC7A_DEFAULT_RATE            100
#define SC7A_DEFAULT_INT_THRESHOLD   7
#define SC7A_DEFAULT_INT_DELAY       25
#define SC7A_DEFAULT_CLICK_THRESHOLD 8

#define SC7A_COMPARE_MATCH           6
#define SC7A_COMPARE_TIMEOUT         15

#define SC7A_ID_REG      0x0f
#define SC7A_CTRL_REG1   0x20
#define SC7A_CTRL_REG2   0x21
#define SC7A_CTRL_REG3   0x22
#define SC7A_CTRL_REG4   0x23
#define SC7A_CTRL_REG5   0x24
#define SC7A_CTRL_REG6   0x25
#define SC7A_REFERENCE   0x26
#define SC7A_STATUS      0x27
#define SC7A_OUT_XL      0x28
#define SC7A_OUT_XH      0x29
#define SC7A_OUT_YL      0x2a
#define SC7A_OUT_YH      0x2b
#define SC7A_OUT_ZL      0x2c
#define SC7A_OUT_ZH      0x2d
#define SC7A_INT1_CFG    0x30
#define SC7A_INT1_SRC    0x31
#define SC7A_INT1_THS    0x32
#define SC7A_INT1_DUR    0x33
#define SC7A_INT2_CFG    0x34
#define SC7A_INT2_SRC    0x35
#define SC7A_INT2_THS    0x36
#define SC7A_INT2_DUR    0x37
#define SC7A_CLICK_CFG   0x38
#define SC7A_CLICK_SRC   0x39
#define SC7A_CLICK_THS   0x3a
#define SC7A_ACT_THS     0x3e
#define SC7A_ACT_DUR     0x3f

struct sc7a_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *wq;
	struct delayed_work work;
	int intr_gpio;
	int x;
	int y;
	int z;
	int enable;
	int orientation;
	int rate;
	int threshold;
	int delay;
	int sensitivity;
	int intmask;
	int counter;
	int timeout;
	int irq_pending;
};

static struct sc7a_data *sc7a;

static uint32_t btn_m1, btn_m2, btn_m4, btn_m8;

/* Addresses to scan */
 union{
	unsigned short dirty_addr_buf[2];
	const unsigned short normal_i2c[2];
}u_i2c_addr = {{SC7A_I2C_ADDR,I2C_CLIENT_END},};				

struct sensor_config_info config_info = {
	.input_type = GSENSOR_TYPE,
	.int_number = 0,
};

int sc7a_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if(config_info.twi_id == adapter->nr)
	{
		INF("%s: Detected chip %s at adapter %d, address 0x%02x\n",
			 __func__, SC7A_NAME, i2c_adapter_id(adapter), client->addr);

		strlcpy(info->type, SC7A_NAME, I2C_NAME_SIZE);
		return 0;
	}else{
		return -ENODEV;
	}
}

static void sc7a_setup(void)
{
	uint8_t dr;

	if (! sc7a->enable) {
		i2c_smbus_write_byte_data(sc7a->client, SC7A_CTRL_REG1, 0);
		sc7a->x = sc7a->y = sc7a->z = 0;
		return;
	}

	if (sc7a->rate < 1) {
		sc7a->rate = 0; dr = 0;
	} else if (sc7a->rate < 10) {
		sc7a->rate = 1; dr = 1;
	} else if (sc7a->rate < 25) {
		sc7a->rate = 10; dr = 2;
	} else if (sc7a->rate < 50) {
		sc7a->rate = 25; dr = 3;
	} else if (sc7a->rate < 100) {
		sc7a->rate = 50; dr = 4;
	} else if (sc7a->rate < 200) {
		sc7a->rate = 100; dr = 5;
	} else if (sc7a->rate < 400) {
		sc7a->rate = 200; dr = 6;
	} else if (sc7a->rate < 1600) {
		sc7a->rate = 400; dr = 7;
	} else {
		sc7a->rate = 1600; dr = 8;
	}
	if (sc7a->threshold > 127) sc7a->threshold = 127;
	if (sc7a->delay > 127) sc7a->delay = 127;

	i2c_smbus_write_byte_data(sc7a->client, SC7A_CTRL_REG1, (dr << 4) | (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0));
	i2c_smbus_write_byte_data(sc7a->client, SC7A_CTRL_REG2, 0);
	// click, AOI1 interrupts
	i2c_smbus_write_byte_data(sc7a->client, SC7A_CTRL_REG3, (0 << 7) | (1 << 6));
	// continuous update, little-endian, +-8G, hires enable
	i2c_smbus_write_byte_data(sc7a->client, SC7A_CTRL_REG4, (0 << 7) | (0 << 6) | (2 << 4) | (1 << 3));
	// fifo disable, 4D disable
	i2c_smbus_write_byte_data(sc7a->client, SC7A_CTRL_REG5, 0);
	// int active low
	i2c_smbus_write_byte_data(sc7a->client, SC7A_CTRL_REG6, (1 << 1));
	// interrupt on x,y threshold
	i2c_smbus_write_byte_data(sc7a->client, SC7A_INT1_CFG, (3 << 6) | (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0));
	i2c_smbus_write_byte_data(sc7a->client, SC7A_INT1_THS, sc7a->threshold);
	i2c_smbus_write_byte_data(sc7a->client, SC7A_INT1_DUR, sc7a->delay);
	// interrupt on single click by Z axis
	i2c_smbus_write_byte_data(sc7a->client, SC7A_CLICK_CFG, (1 << 4));
	i2c_smbus_write_byte_data(sc7a->client, SC7A_CLICK_SRC, (1 << 4));
	i2c_smbus_write_byte_data(sc7a->client, SC7A_CLICK_THS, sc7a->sensitivity);
}

static ssize_t sc7a_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "SC7A20");
}

static ssize_t sc7a_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sc7a->enable);
}

static ssize_t sc7a_enable_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;

	sc7a->enable = value ? 1 : 0;
	sc7a_setup();
	return count;
}

static int sc7a_read_value(int regl, int regh)
{
	int vl = i2c_smbus_read_byte_data(sc7a->client, regl);
	int vh = i2c_smbus_read_byte_data(sc7a->client, regh);
	if (vl < 0 || vh < 0) return 0;
	if (vh & 0x80) vh |= ~0xff;
	return (vh << 8) | vl;
}

static void sc7a_read_position(void)
{
	sc7a->x = sc7a_read_value(SC7A_OUT_XL, SC7A_OUT_XH);
	sc7a->y = sc7a_read_value(SC7A_OUT_YL, SC7A_OUT_YH);
	sc7a->z = sc7a_read_value(SC7A_OUT_ZL, SC7A_OUT_ZH);
}

static ssize_t sc7a_position_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	sc7a_read_position();
	#ifndef SC7A_SWAP_XY
	return sprintf(buf, "%d %d %d\n", sc7a->x, sc7a->y, -sc7a->z);
	#else
	return sprintf(buf, "%d %d %d\n", sc7a->y, sc7a->x, -sc7a->z);
	#endif
}

static ssize_t sc7a_orientation_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sc7a->orientation);
}

static ssize_t sc7a_rate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sc7a->rate);
}

static ssize_t sc7a_rate_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;

	sc7a->rate = value;
	sc7a_setup();
	return count;
}

static ssize_t sc7a_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sc7a->threshold);
}

static ssize_t sc7a_threshold_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;

	sc7a->threshold = value;
	sc7a_setup();
	return count;
}

static ssize_t sc7a_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sc7a->delay);
}

static ssize_t sc7a_delay_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;

	sc7a->delay = value;
	sc7a_setup();
	return count;
}

static ssize_t sc7a_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sc7a->sensitivity);
}

static ssize_t sc7a_sensitivity_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;

	sc7a->delay = value;
	sc7a_setup();
	return count;
}

static ssize_t sc7a_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int reg, len=0;
	for (reg=0; reg<0x40; reg++) {
		int value = i2c_smbus_read_byte_data(sc7a->client, reg);
		len += snprintf(buf+len, 4096-len, "%02x: %02x\n", reg, value);
	}
	return len;
}

static ssize_t sc7a_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val;
	int ret;

  if (sscanf(buf, "%x %x", &reg, &val) < 2) return -EINVAL;
	ret = i2c_smbus_write_byte_data(sc7a->client, reg, val);
	return (ret == 0) ? count : ret;
}

static DEVICE_ATTR(vendor, S_IRUGO, sc7a_vendor_show, NULL);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUGO, sc7a_enable_show, sc7a_enable_store);
static DEVICE_ATTR(position, S_IRUGO, sc7a_position_show, NULL);
static DEVICE_ATTR(orientation, S_IRUGO, sc7a_orientation_show, NULL);
static DEVICE_ATTR(rate, S_IRUGO|S_IWUGO, sc7a_rate_show, sc7a_rate_store);
static DEVICE_ATTR(threshold, S_IRUGO|S_IWUGO, sc7a_threshold_show, sc7a_threshold_store);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUGO, sc7a_delay_show, sc7a_delay_store);
static DEVICE_ATTR(sensitivity, S_IRUGO|S_IWUGO, sc7a_sensitivity_show, sc7a_sensitivity_store);
static DEVICE_ATTR(reg, S_IRUGO|S_IWUGO, sc7a_reg_show, sc7a_reg_store);

static struct kobject *gsensor_kobj;

static int sc7a_sysfs_init(void)
{
	int ret=0;

	gsensor_kobj = kobject_create_and_add("gsensor", NULL) ;
	if (gsensor_kobj == NULL) {
		ERR("%s: subsystem_register failed\n", __func__);
		return -ENOMEM;
	}

	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_vendor.attr);
	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_enable.attr);
	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_position.attr);
	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_orientation.attr);
	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_rate.attr);
	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_threshold.attr);
	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_delay.attr);
	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_sensitivity.attr);
	ret |= sysfs_create_file(gsensor_kobj, &dev_attr_reg.attr);
	if (ret) ERR("%s: sysfs_create_file failed\n", __func__);
	return 0 ;
}

static void sc7a_sysfs_deinit(void)
{
	sysfs_remove_file(gsensor_kobj, &dev_attr_vendor.attr);
	kobject_del(gsensor_kobj);
}	

static int button_to_orientation(int btn)
{
	switch (btn) {
		default:        return 0;
		case BTN_NORTH: return 3;
		case BTN_WEST:  return 1;
		case BTN_EAST:  return 2;
	}
}

static void sc7a_report(int key)
{
	printk("SC7A: report (%d)\n", key);
	input_report_key(sc7a->input_dev, key, 1);
	input_sync(sc7a->input_dev);
	input_report_key(sc7a->input_dev, key, 0);
	input_sync(sc7a->input_dev);
}

static void sc7a_work_func(struct work_struct *work)
{
	// set lower threshold after interrupt, but require higher angle without it
	int thfactor = sc7a->irq_pending ? 200 : 400;
	int intmask = 0;
	int button = 0;
	sc7a_read_position();
	if (sc7a->x < -sc7a->threshold * thfactor && abs(sc7a->x) > abs(sc7a->y)) {
		intmask = 1;
		button = btn_m1;
	} else if (sc7a->x > +sc7a->threshold * thfactor && abs(sc7a->x) > abs(sc7a->y)) {
		intmask = 2;
		button = btn_m2;
	} else if (sc7a->y < -sc7a->threshold * thfactor && abs(sc7a->y) > abs(sc7a->x)) {
		intmask = 4;
		button = btn_m4;
	} else if (sc7a->y > +sc7a->threshold * thfactor && abs(sc7a->y) > abs(sc7a->x)) {
		intmask = 8;
		button = btn_m8;
	} else {
		intmask = 0;
		button = 0;
	}

	if (intmask == sc7a->intmask) {
		sc7a->counter++;
	} else {
		sc7a->intmask = intmask;
		sc7a->counter = 0;
	}

	printk("SC7A: x=%d y=%d intmask %d counter %d\n", sc7a->x, sc7a->y, intmask, sc7a->counter);

	if (sc7a->counter >= SC7A_COMPARE_MATCH) {
		if (button) {
			sc7a_report(button);
			sc7a->orientation = button_to_orientation(button);
			printk("SC7A: orientation %d\n", sc7a->orientation);
		} else {
			printk("SC7A: orientation not changed\n");
		}
		i2c_smbus_write_byte_data(sc7a->client, SC7A_INT1_CFG, (3 << 6) | (0xf & ~intmask));
		input_set_int_enable(&(config_info.input_type), 1);
	} else if (++sc7a->timeout >= SC7A_COMPARE_TIMEOUT) {
		i2c_smbus_write_byte_data(sc7a->client, SC7A_INT1_CFG, (3 << 6) | 0xf);
		printk("SC7A: timeout detecting orientation\n");
		input_set_int_enable(&(config_info.input_type), 1);
	} else {
		queue_delayed_work(sc7a->wq, &sc7a->work, HZ/20);
	}
	return;
}

irqreturn_t sc7a_irq_handler(int irq, void *dev_id)
{
	printk("SC7A: *GS.IRQ*\n");
	input_set_int_enable(&(config_info.input_type), 0);
	sc7a->timeout = 0;
	sc7a->counter = 0;
	sc7a->intmask = 0;
	sc7a->irq_pending = 1;
	queue_delayed_work(sc7a->wq, &sc7a->work, HZ/20);
	return IRQ_HANDLED;
}

static int sc7a_register_interrupt(struct i2c_client *client)
{
	int ret = input_request_int(&(config_info.input_type), sc7a_irq_handler, (IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND), sc7a);
	if (ret) {
		ERR( "sc7a: request irq failed\n");
	}
	return ret;
}

static int sc7a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int devid;
	int err = 0;

	DBG("sc7a_probe\n");	

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ERR("%s: i2c check functionality error\n", __func__);
		err = -ENODEV;
		goto err_check_functionality_failed;
	}

	devid = i2c_smbus_read_byte_data(client, SC7A_ID_REG);
	if (devid != 0x11) {
		ERR("%s: unknown device id %x\n", __func__, devid);
		err = -ENODEV;
		goto err_input_dev_alloc_failed;
	}

	sc7a = kzalloc(sizeof(struct sc7a_data), GFP_KERNEL);
	if (sc7a == NULL) {
		ERR("%s: allocate data failed\n", __func__);
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}

	sc7a->wq = create_singlethread_workqueue("sc7a_wq");
	if (!sc7a->wq) {
		ERR("%s: create workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_wq_failed;
	}

	INIT_DELAYED_WORK(&sc7a->work, sc7a_work_func);
	sc7a->client = client;
	i2c_set_clientdata(client, sc7a);

	sc7a->input_dev = input_allocate_device();
	if (sc7a->input_dev == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev, "%s: Failed to allocate input device\n", __func__);
		goto err_input_dev_alloc_failed;
	}
	sc7a->input_dev->name = SC7A_NAME;
	sc7a->input_dev->id.bustype = BUS_I2C;
	sc7a->input_dev->dev.parent = &client->dev;
	input_set_drvdata(sc7a->input_dev, sc7a);

	set_bit(EV_KEY, sc7a->input_dev->evbit);
	sc7a->input_dev->keybit[BIT_WORD(BTN_NORTH)] |= BIT_MASK(BTN_NORTH);
	sc7a->input_dev->keybit[BIT_WORD(BTN_SOUTH)] |= BIT_MASK(BTN_SOUTH);
	sc7a->input_dev->keybit[BIT_WORD(BTN_EAST)] |= BIT_MASK(BTN_EAST);
	sc7a->input_dev->keybit[BIT_WORD(BTN_WEST)] |= BIT_MASK(BTN_WEST);

	#if (CONFIG_PB_DEVICE == 740) || (CONFIG_PB_DEVICE == 1040)
		btn_m1 = BTN_SOUTH;
		btn_m2 = BTN_NORTH;
		btn_m4 = BTN_WEST;
		btn_m8 = BTN_EAST;
	#elif (CONFIG_PB_DEVICE == 632) || (CONFIG_PB_DEVICE == 633)
		btn_m1 = BTN_WEST;
		btn_m2 = BTN_EAST;
		btn_m4 = BTN_NORTH;
		btn_m8 = BTN_SOUTH;
	#else
		#error sc7a orientation not defined for this device
	#endif

	err = input_register_device(sc7a->input_dev);
	if (err) {
			ERR("%s: unable to register %s input device\n", __func__, sc7a->input_dev->name);
		goto err_input_register_device_failed;
	}

	config_info.dev = &sc7a->input_dev->dev;
	
	sc7a_register_interrupt(sc7a->client);

	sc7a_sysfs_init();

	sc7a->enable = 1;
	sc7a->rate = SC7A_DEFAULT_RATE;
	sc7a->threshold = SC7A_DEFAULT_INT_THRESHOLD;
	sc7a->delay = SC7A_DEFAULT_INT_DELAY;
	sc7a->sensitivity = SC7A_DEFAULT_CLICK_THRESHOLD;
	sc7a_setup();

	dev_info(&client->dev, "Start g-sensor %s in interrupt mode\n", sc7a->input_dev->name);

	device_init_wakeup(&client->dev, 1);

	sc7a->irq_pending = 0;
	queue_delayed_work(sc7a->wq, &sc7a->work, HZ/10);
	return 0;

err_input_register_device_failed:
	if (sc7a->input_dev)
		input_free_device(sc7a->input_dev);

err_input_dev_alloc_failed: 
	if (sc7a->wq)
		destroy_workqueue(sc7a->wq);

err_create_wq_failed:
	kfree(sc7a);

err_alloc_data_failed:
err_check_functionality_failed:

	return err;
}

static int sc7a_remove(struct i2c_client *client)
{
	sc7a_sysfs_deinit();

	input_free_int(&(config_info.input_type), sc7a);

	if (sc7a->wq) destroy_workqueue(sc7a->wq);
	input_unregister_device(sc7a->input_dev);
	kfree(sc7a);

	return 0;
}


static int sc7a_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct sc7a_data *data;
	data = i2c_get_clientdata(client);

	pr_debug("[%s] 1\n", __func__);
	if (check_scene_locked(SCENE_NORMAL_STANDBY) == 0)
		return 0;

	pr_debug("[%s] 2\n", __func__);
	input_set_int_enable(&(config_info.input_type), 0);

	printk("SC7A: suspend\n");
	return 0;
}

static int sc7a_resume(struct i2c_client *client)
{
	struct sc7a_data *data;
	data = i2c_get_clientdata(client);

	pr_debug("[%s] 1\n", __func__);

	printk("SC7A: wakeup\n");

	if (check_scene_locked(SCENE_NORMAL_STANDBY) == 0)
		return 0;

	pr_debug("[%s] 2\n", __func__);
	sc7a_setup();
	sc7a->irq_pending = 0;
	queue_delayed_work(sc7a->wq, &sc7a->work, HZ/10);

	return 0;
}

static const struct i2c_device_id sc7a_id[] = {
	{ SC7A_NAME, 0 },
	{ }
};

static struct i2c_driver sc7a_driver = {
  .class = I2C_CLASS_HWMON,
	.probe		= sc7a_probe,
	.remove		= sc7a_remove,
	.suspend	= sc7a_suspend,
	.resume		= sc7a_resume,
	.id_table	= sc7a_id,
	.detect   = sc7a_detect,
	.driver		= {
		.name   = SC7A_NAME,
		.owner  = THIS_MODULE,
	},
	.address_list   = u_i2c_addr.normal_i2c,
};

static void __exit sc7a_exit(void)
{
	i2c_del_driver(&sc7a_driver);
	input_free_platform_resource(&(config_info.input_type));
	return;
}

static int __init sc7a_init(void)
{
	int ret = -1;     

	INF("sc7a g-sensor driver\n");

	if (input_fetch_sysconfig_para(&(config_info.input_type))) {
		ERR("%s: input_fetch_sysconfig_para err.\n", __func__);
		return 0;
	} else {
		ret = input_init_platform_resource(&(config_info.input_type));
		if (0 != ret) {
			ERR("%s: input_init_platform_resource err. \n", __func__);    
		}
	}
	if (config_info.sensor_used == 0) {
		ERR("*** gsensor_used set to 0 !\n");
		return 0;
	}

	ret = i2c_add_driver(&sc7a_driver);
	return ret;
}

module_init(sc7a_init);
module_exit(sc7a_exit);

MODULE_DESCRIPTION("SC7A g-sensor driver");
MODULE_LICENSE("GPL");


