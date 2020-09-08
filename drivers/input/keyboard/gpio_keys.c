/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 * Copyright 2010, 2011 David Jander <david@protonic.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/power/scenelock.h>

#if 1
#define gpioKey_printk(fmt, args...) printk(KERN_INFO "[gpioKey:] " "%s(%d): " fmt, __FUNCTION__, __LINE__, ##args)
#else
#define gpioKey_printk(fmt, args...) 
#endif

static unsigned long deep_standby = 0;

struct gpio_button_data {
	const struct gpio_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
	struct delayed_work work;
	unsigned int timer_debounce;	/* in msecs */
	unsigned int irq;
	spinlock_t lock;
	bool disabled;
	bool key_pressed;
	int state;
};

struct gpio_keys_drvdata {
	const struct gpio_keys_platform_data *pdata;
	struct input_dev *input;
	struct mutex disable_lock;
	struct gpio_button_data data[0];
};

/*
 * SYSFS interface for enabling/disabling keys and switches:
 *
 * There are 4 attributes under /sys/devices/platform/gpio-keys/
 *	keys [ro]              - bitmap of keys (EV_KEY) which can be
 *	                         disabled
 *	switches [ro]          - bitmap of switches (EV_SW) which can be
 *	                         disabled
 *	disabled_keys [rw]     - bitmap of keys currently disabled
 *	disabled_switches [rw] - bitmap of switches currently disabled
 *
 * Userland can change these values and hence disable event generation
 * for each key (or switch). Disabling a key means its interrupt line
 * is disabled.
 *
 * For example, if we have following switches set up as gpio-keys:
 *	SW_DOCK = 5
 *	SW_CAMERA_LENS_COVER = 9
 *	SW_KEYPAD_SLIDE = 10
 *	SW_FRONT_PROXIMITY = 11
 * This is read from switches:
 *	11-9,5
 * Next we want to disable proximity (11) and dock (5), we write:
 *	11,5
 * to file disabled_switches. Now proximity and dock IRQs are disabled.
 * This can be verified by reading the file disabled_switches:
 *	11,5
 * If we now want to enable proximity (11) switch we write:
 *	5
 * to disabled_switches.
 *
 * We can disable only those keys which don't allow sharing the irq.
 */

/**
 * get_n_events_by_type() - returns maximum number of events per @type
 * @type: type of button (%EV_KEY, %EV_SW)
 *
 * Return value of this function can be used to allocate bitmap
 * large enough to hold all bits for given type.
 */
static inline int get_n_events_by_type(int type)
{
	BUG_ON(type != EV_SW && type != EV_KEY);

	return (type == EV_KEY) ? KEY_CNT : SW_CNT;
}

/**
 * gpio_keys_disable_button() - disables given GPIO button
 * @bdata: button data for button to be disabled
 *
 * Disables button pointed by @bdata. This is done by masking
 * IRQ line. After this function is called, button won't generate
 * input events anymore. Note that one can only disable buttons
 * that don't share IRQs.
 *
 * Make sure that @bdata->disable_lock is locked when entering
 * this function to avoid races when concurrent threads are
 * disabling buttons at the same time.
 */
static void gpio_keys_disable_button(struct gpio_button_data *bdata)
{
	if (!bdata->disabled) {
		/*
		 * Disable IRQ and possible debouncing timer.
		 */
		disable_irq(bdata->irq);
		if (bdata->timer_debounce)
			del_timer_sync(&bdata->timer);

		bdata->disabled = true;
	}
}

/**
 * gpio_keys_enable_button() - enables given GPIO button
 * @bdata: button data for button to be disabled
 *
 * Enables given button pointed by @bdata.
 *
 * Make sure that @bdata->disable_lock is locked when entering
 * this function to avoid races with concurrent threads trying
 * to enable the same button at the same time.
 */
static void gpio_keys_enable_button(struct gpio_button_data *bdata)
{
	if (bdata->disabled) {
		enable_irq(bdata->irq);
		bdata->disabled = false;
	}
}

/**
 * gpio_keys_attr_show_helper() - fill in stringified bitmap of buttons
 * @ddata: pointer to drvdata
 * @buf: buffer where stringified bitmap is written
 * @type: button type (%EV_KEY, %EV_SW)
 * @only_disabled: does caller want only those buttons that are
 *                 currently disabled or all buttons that can be
 *                 disabled
 *
 * This function writes buttons that can be disabled to @buf. If
 * @only_disabled is true, then @buf contains only those buttons
 * that are currently disabled. Returns 0 on success or negative
 * errno on failure.
 */
static ssize_t gpio_keys_attr_show_helper(struct gpio_keys_drvdata *ddata,
					  char *buf, unsigned int type,
					  bool only_disabled)
{
	int n_events = get_n_events_by_type(type);
	unsigned long *bits;
	ssize_t ret;
	int i;

	bits = kcalloc(BITS_TO_LONGS(n_events), sizeof(*bits), GFP_KERNEL);
	if (!bits)
		return -ENOMEM;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (only_disabled && !bdata->disabled)
			continue;

		__set_bit(bdata->button->code, bits);
	}

	ret = bitmap_scnlistprintf(buf, PAGE_SIZE - 2, bits, n_events);
	buf[ret++] = '\n';
	buf[ret] = '\0';

	kfree(bits);

	return ret;
}

/**
 * gpio_keys_attr_store_helper() - enable/disable buttons based on given bitmap
 * @ddata: pointer to drvdata
 * @buf: buffer from userspace that contains stringified bitmap
 * @type: button type (%EV_KEY, %EV_SW)
 *
 * This function parses stringified bitmap from @buf and disables/enables
 * GPIO buttons accordingly. Returns 0 on success and negative error
 * on failure.
 */
static ssize_t gpio_keys_attr_store_helper(struct gpio_keys_drvdata *ddata,
					   const char *buf, unsigned int type)
{
	int n_events = get_n_events_by_type(type);
	unsigned long *bits;
	ssize_t error;
	int i;

	bits = kcalloc(BITS_TO_LONGS(n_events), sizeof(*bits), GFP_KERNEL);
	if (!bits)
		return -ENOMEM;

	error = bitmap_parselist(buf, bits, n_events);
	if (error)
		goto out;

	/* First validate */
	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (test_bit(bdata->button->code, bits) &&
		    !bdata->button->can_disable) {
			error = -EINVAL;
			goto out;
		}
	}

	mutex_lock(&ddata->disable_lock);

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (test_bit(bdata->button->code, bits))
			gpio_keys_disable_button(bdata);
		else
			gpio_keys_enable_button(bdata);
	}

	mutex_unlock(&ddata->disable_lock);

out:
	kfree(bits);
	return error;
}

#define ATTR_SHOW_FN(name, type, only_disabled)				\
static ssize_t gpio_keys_show_##name(struct device *dev,		\
				     struct device_attribute *attr,	\
				     char *buf)				\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);	\
									\
	return gpio_keys_attr_show_helper(ddata, buf,			\
					  type, only_disabled);		\
}

ATTR_SHOW_FN(keys, EV_KEY, false);
ATTR_SHOW_FN(switches, EV_SW, false);
ATTR_SHOW_FN(disabled_keys, EV_KEY, true);
ATTR_SHOW_FN(disabled_switches, EV_SW, true);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/gpio-keys/keys [ro]
 * /sys/devices/platform/gpio-keys/switches [ro]
 */
static DEVICE_ATTR(keys, S_IRUGO, gpio_keys_show_keys, NULL);
static DEVICE_ATTR(switches, S_IRUGO, gpio_keys_show_switches, NULL);

#define ATTR_STORE_FN(name, type)					\
static ssize_t gpio_keys_store_##name(struct device *dev,		\
				      struct device_attribute *attr,	\
				      const char *buf,			\
				      size_t count)			\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);	\
	ssize_t error;							\
									\
	error = gpio_keys_attr_store_helper(ddata, buf, type);		\
	if (error)							\
		return error;						\
									\
	return count;							\
}

ATTR_STORE_FN(disabled_keys, EV_KEY);
ATTR_STORE_FN(disabled_switches, EV_SW);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/gpio-keys/disabled_keys [rw]
 * /sys/devices/platform/gpio-keys/disables_switches [rw]
 */
static DEVICE_ATTR(disabled_keys, S_IWUSR | S_IRUGO,
		   gpio_keys_show_disabled_keys,
		   gpio_keys_store_disabled_keys);
static DEVICE_ATTR(disabled_switches, S_IWUSR | S_IRUGO,
		   gpio_keys_show_disabled_switches,
		   gpio_keys_store_disabled_switches);

static struct attribute *gpio_keys_attrs[] = {
	&dev_attr_keys.attr,
	&dev_attr_switches.attr,
	&dev_attr_disabled_keys.attr,
	&dev_attr_disabled_switches.attr,
	NULL,
};

static struct attribute_group gpio_keys_attr_group = {
	.attrs = gpio_keys_attrs,
};

static void gpio_keys_gpio_report_event(struct gpio_button_data *bdata)
{
	const struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned int type = button->type ?: EV_KEY;
	int last_state = bdata->state;
	
	gpioKey_printk("gpio=%d value=%d\n",__gpio_get_value(button->gpio_key_gpio.gpio),
		__gpio_get_value(button->gpio_key_gpio.gpio));
	bdata->state = (__gpio_get_value(button->gpio_key_gpio.gpio) ? 1 : 0) ^ button->active_low;

	if (type == EV_ABS) {
		if (bdata->state)
		{
			gpioKey_printk("input_event: type=%d code=%d value=%d\n",type,button->code,button->value);
			input_event(input, type, button->code, button->value);
		}
	} else {
		if (last_state == 0 && bdata->state == 0) {
			gpioKey_printk("input_event: type=%d code=%d value=%d\n",type,button->code,1);
			input_event(input, type, button->code, 1);
		}
		gpioKey_printk("input_event: type=%d code=%d value=%d\n",type,button->code,bdata->state);
		input_event(input, type, button->code, bdata->state);
	}
	gpioKey_printk("input_sync\n");
	input_sync(input);
}

static void gpio_keys_gpio_work_func(struct work_struct *_work)
{
	struct gpio_button_data *bdata =
		container_of(to_delayed_work(_work), struct gpio_button_data, work);
	gpioKey_printk("gpio_keys_gpio_work_func\n");
	gpio_keys_gpio_report_event(bdata);

	if (bdata->button->wakeup)
		pm_relax(bdata->input->dev.parent);
}

static irqreturn_t gpio_keys_gpio_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;

	BUG_ON(irq != bdata->irq);
	gpioKey_printk(" gpio_irq=%d\n",irq);

	if (! deep_standby) {
		if (bdata->button->wakeup)
			pm_stay_awake(bdata->input->dev.parent);
		schedule_delayed_work(&bdata->work, msecs_to_jiffies(50));
	}
	return IRQ_HANDLED;
}

static void gpio_keys_irq_timer(unsigned long _data)
{
	struct gpio_button_data *bdata = (struct gpio_button_data *)_data;
	struct input_dev *input = bdata->input;
	unsigned long flags;
	gpioKey_printk(" \n");
	spin_lock_irqsave(&bdata->lock, flags);
	if (bdata->key_pressed) {
		input_event(input, EV_KEY, bdata->button->code, 0);
		input_sync(input);
		bdata->key_pressed = false;
	}
	spin_unlock_irqrestore(&bdata->lock, flags);
}

static irqreturn_t gpio_keys_irq_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;
	const struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned long flags;
	gpioKey_printk(" irq=%d\n",irq);
	BUG_ON(irq != bdata->irq);
	gpioKey_printk(" irq=%d\n",irq);

	if (deep_standby) goto out;

	spin_lock_irqsave(&bdata->lock, flags);

	if (!bdata->key_pressed) {
		if (bdata->button->wakeup)
			pm_wakeup_event(bdata->input->dev.parent, 0);

		gpioKey_printk("isr input_event: type=%d code=%d value=%d\n",EV_KEY,button->code, 1);
		input_event(input, EV_KEY, button->code, 1);
		input_sync(input);

		//if (!bdata->timer_debounce) {
		//	input_event(input, EV_KEY, button->code, 0);
		//	input_sync(input);
		//	goto out;
		//}

		bdata->key_pressed = true;
	}

	if (bdata->timer_debounce)
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(bdata->timer_debounce));
out:
	spin_unlock_irqrestore(&bdata->lock, flags);
	return IRQ_HANDLED;
}

static int gpio_keys_setup_key(struct platform_device *pdev,
				struct input_dev *input,
				struct gpio_button_data *bdata,
				const struct gpio_keys_button *button)
{
	const char *desc = button->desc ? button->desc : "gpio_keys";
	struct device *dev = &pdev->dev;
	irq_handler_t isr;
	unsigned long irqflags;
	int irq, error;

	bdata->input = input;
	bdata->button = button;
	spin_lock_init(&bdata->lock);

		if (gpio_is_valid(button->gpio_key_gpio.gpio)) 
		{
			gpioKey_printk("\n");

			error = gpio_request_one(button->gpio_key_gpio.gpio, GPIOF_IN, desc);
			gpioKey_printk("gpio_request_one error =%d\n",error);
			if (error < 0) {
				dev_err(dev, "Failed to request GPIO %d, error %d\n",
					button->gpio_key_gpio.gpio, error);
				return error;
			}
			gpioKey_printk("\n");

			if (button->debounce_interval) {
				error = gpio_set_debounce(button->gpio_key_gpio.gpio,
						button->debounce_interval * 1000);
				/* use timer if gpiolib doesn't provide debounce */
				if (error < 0)
					bdata->timer_debounce =
							button->debounce_interval;
			}

			irq = gpio_to_irq(button->gpio_key_gpio.gpio);
			gpioKey_printk("irq=%d\n",irq);
			if (irq < 0) {
				error = irq;
				dev_err(dev,
					"Unable to get irq number for GPIO %d, error %d\n",
					button->gpio_key_gpio.gpio, error);
				goto fail;
			}
			bdata->irq = irq;

			INIT_DELAYED_WORK(&bdata->work, gpio_keys_gpio_work_func);

			isr = gpio_keys_gpio_isr;
			irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND;
			gpioKey_printk("\n");

		} else {
			if (!button->irq) {
				dev_err(dev, "No IRQ specified\n");
				return -EINVAL;
			}
			bdata->irq = button->irq;

			if (button->type && button->type != EV_KEY) {
				dev_err(dev, "Only EV_KEY allowed for IRQ buttons.\n");
				return -EINVAL;
			}

			bdata->timer_debounce = button->debounce_interval;
			setup_timer(&bdata->timer,
				    gpio_keys_irq_timer, (unsigned long)bdata);

			isr = gpio_keys_irq_isr;
			irqflags = 0;
		}

	input_set_capability(input, button->type ?: EV_KEY, button->code);
	gpioKey_printk("button->code =%d\n",button->code);
	/*
	 * If platform has specified that the button can be disabled,
	 * we don't want it to share the interrupt line.
	 */
	if (!button->can_disable)
		irqflags |= IRQF_SHARED;
	gpioKey_printk("\n");

	error = request_any_context_irq(bdata->irq, isr, irqflags, desc, bdata);
	gpioKey_printk("request_any_context_irq error=%d\n",error);
	if (error < 0) {
		dev_err(dev, "Unable to claim irq %d; error %d\n",
			bdata->irq, error);
		goto fail;
	}

	return 0;

fail:
	gpioKey_printk("\n");
	if (gpio_is_valid(button->gpio_key_gpio.gpio))
		gpio_free(button->gpio_key_gpio.gpio);

	return error;
}

static void gpio_keys_report_state(struct gpio_keys_drvdata *ddata)
{
	struct input_dev *input = ddata->input;
	int i;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];
		if (gpio_is_valid(bdata->button->gpio_key_gpio.gpio))
			gpio_keys_gpio_report_event(bdata);
	}
	input_sync(input);
}

static int gpio_keys_open(struct input_dev *input)
{
	struct gpio_keys_drvdata *ddata = input_get_drvdata(input);
	const struct gpio_keys_platform_data *pdata = ddata->pdata;
	int error;

	if (pdata->enable) {
		error = pdata->enable(input->dev.parent);
		if (error)
			return error;
	}

	/* Report current state of buttons that are connected to GPIOs */
	//gpio_keys_report_state(ddata);

	return 0;
}

static void gpio_keys_close(struct input_dev *input)
{
	struct gpio_keys_drvdata *ddata = input_get_drvdata(input);
	const struct gpio_keys_platform_data *pdata = ddata->pdata;

	if (pdata->disable)
		pdata->disable(input->dev.parent);
}

/*
 * Handlers for alternative sources of platform_data
 */

#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static struct gpio_keys_platform_data *
gpio_keys_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *pp;
	struct gpio_keys_platform_data *pdata;
	struct gpio_keys_button *button;
	int error;
	int nbuttons;
	int i;
	gpioKey_printk("\n");
	node = dev->of_node;
	if (!node) {
		error = -ENODEV;
		goto err_out;
	}

	nbuttons = of_get_child_count(node);
	gpioKey_printk("nbuttons =%d \n",nbuttons);
	if (nbuttons == 0) {
		error = -ENODEV;
		goto err_out;
	}
	gpioKey_printk("\n");
	pdata = kzalloc(sizeof(*pdata) + nbuttons * (sizeof *button),
			GFP_KERNEL);
	if (!pdata) {
		error = -ENOMEM;
		goto err_out;
	}

	pdata->buttons = (struct gpio_keys_button *)(pdata + 1);
	pdata->nbuttons = nbuttons;
	gpioKey_printk("nbuttons=%d\n",nbuttons);
	pdata->rep = !!of_get_property(node, "autorepeat", NULL);

	i = 0;
	for_each_child_of_node(node, pp) {
		int gpio;
		enum of_gpio_flags flags;

		if (!of_find_property(pp, "gpios", NULL)) {
			pdata->nbuttons--;
			dev_warn(dev, "Found button without gpios\n");
			continue;
		}
		gpioKey_printk("\n");

		gpio = of_get_gpio_flags(pp, 0, &flags);
		if (gpio < 0) {
			error = gpio;
			if (error != -EPROBE_DEFER)
				dev_err(dev,
					"Failed to get gpio flags, error: %d\n",
					error);
			goto err_free_pdata;
		}

		button = &pdata->buttons[i++];

		button->gpio = gpio;
		button->active_low = flags & OF_GPIO_ACTIVE_LOW;

		if (of_property_read_u32(pp, "linux,code", &button->code)) {
			dev_err(dev, "Button without keycode: 0x%x\n",
				button->gpio);
			error = -EINVAL;
			goto err_free_pdata;
		}

		button->desc = of_get_property(pp, "label", NULL);

		if (of_property_read_u32(pp, "linux,input-type", &button->type))
			button->type = EV_KEY;

		button->wakeup = !!of_get_property(pp, "gpio-key,wakeup", NULL);

		if (of_property_read_u32(pp, "debounce-interval",
					 &button->debounce_interval))
			button->debounce_interval = 5;
	}

	if (pdata->nbuttons == 0) {
		error = -EINVAL;
		goto err_free_pdata;
	}

	return pdata;

err_free_pdata:
	kfree(pdata);
err_out:
	gpioKey_printk("\n");
	return ERR_PTR(error);
}

static struct of_device_id gpio_keys_of_match[] = {
	{ .compatible = "gpio-keys", },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_keys_of_match);

#else

static inline struct gpio_keys_platform_data *
gpio_keys_get_devtree_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}

#endif

static void gpio_remove_key(struct gpio_button_data *bdata)
{
	free_irq(bdata->irq, bdata);
	if (bdata->timer_debounce)
		del_timer_sync(&bdata->timer);
	cancel_delayed_work_sync(&bdata->work);
	if (gpio_is_valid(bdata->button->gpio))
		gpio_free(bdata->button->gpio);
}

static int gpio_keys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct gpio_keys_platform_data *pdata = dev_get_platdata(dev);
	struct gpio_keys_drvdata *ddata;
	struct input_dev *input;
	int i, error,ret;
	
	struct device_node *np = NULL;


	int wakeup = 0;
	gpioKey_printk("\n");
	//gpioKey_printk("pdata->nbuttons=%d\n",pdata->nbuttons);
	if (!pdata) {
		gpioKey_printk("platform data is null, get from device tree\n");
		pdata = gpio_keys_get_devtree_pdata(dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	} else {
		gpioKey_printk("used default platform data\n");
	}

	gpioKey_printk("\n");

	ddata = kzalloc(sizeof(struct gpio_keys_drvdata) +
			pdata->nbuttons * sizeof(struct gpio_button_data),
			GFP_KERNEL);
	input = input_allocate_device();
	if (!ddata || !input) {
		dev_err(dev, "failed to allocate state\n");
		error = -ENOMEM;
		goto fail1;
	}

	ddata->pdata = pdata;
	ddata->input = input;
	mutex_init(&ddata->disable_lock);
	
	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	input->name = pdata->name ? : pdev->name;
	input->phys = "gpio-keys/input0";
	input->dev.parent = &pdev->dev;
	input->open = gpio_keys_open;
	input->close = gpio_keys_close;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);
	gpioKey_printk("pdata->nbuttons=%d\n",pdata->nbuttons);

	//static struct tps65185_config_info *tps65185_data ;
	
	//tps65185_data = kzalloc(sizeof(struct tps65185_config_info),GFP_ATOMIC);


	np = of_find_node_by_name(NULL,"gpio_keys");
	if (!np) {
		 pr_err("ERROR! get TPS65185_PARA failed, func:%s, line:%d\n",__FUNCTION__, __LINE__);
	}
	ret = of_property_read_u32(np, "gpio_key_used", &pdata->gpio_key_used);
	if (ret) {
		 pr_err("get gpio_key_used is fail, %d\n", ret);
		// goto devicetree_get_item_err;
	}
	gpioKey_printk(" gpio_key_used=%d \n",pdata->gpio_key_used);


	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_keys_button *button = &pdata->buttons[i];
		gpioKey_printk("gpio_key_gpio=%d desc=%s\n",button->code,button->desc);
		if(i ==0)
		{
			button->gpio_key_gpio.gpio =of_get_named_gpio_flags(np, "gpio_key_gpio_0", 0, 
				(enum of_gpio_flags *)(&(button->gpio_key_gpio)));
			if (!gpio_is_valid(button->gpio_key_gpio.gpio))
				pr_err("%s: gpio_key_gpio_one is invalid. \n",__func__ );
			gpioKey_printk("gpio_key_gpio_one=%d gpio=%d\n",button->gpio_key_gpio,button->gpio_key_gpio.gpio);
			struct gpio_button_data *bdata = &ddata->data[i];

			error = gpio_keys_setup_key(pdev, input, bdata, button);
			gpioKey_printk("gpio_keys_setup_key  error=%d \n",error);
			if (error)
				goto fail2;

			
		}
		else if (i ==1)
		{
			button->gpio_key_gpio.gpio =of_get_named_gpio_flags(np, "gpio_key_gpio_1", 0, 
				(enum of_gpio_flags *)(&(button->gpio_key_gpio)));
			if (!gpio_is_valid(button->gpio_key_gpio.gpio))
				pr_err("%s: gpio_key_gpio_two is invalid. \n",__func__ );
			gpioKey_printk("gpio_key_gpio_two=%d gpio=%d\n",button->gpio_key_gpio,button->gpio_key_gpio.gpio);

			struct gpio_button_data *bdata = &ddata->data[i];

			error = gpio_keys_setup_key(pdev, input, bdata, button);
			gpioKey_printk("gpio_keys_setup_key  error=%d gpio=%s\n",error);
			if (error)
				goto fail2;

			
		}

		if (button->wakeup)
			wakeup = 1;
	}

	error = sysfs_create_group(&pdev->dev.kobj, &gpio_keys_attr_group);
	if (error) {
		dev_err(dev, "Unable to export keys/switches, error: %d\n",
			error);
		goto fail2;
	}

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			error);
		goto fail3;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	return 0;

 fail3:
	sysfs_remove_group(&pdev->dev.kobj, &gpio_keys_attr_group);
 fail2:
	gpioKey_printk("\n");
	while (--i >= 0)
		gpio_remove_key(&ddata->data[i]);

	platform_set_drvdata(pdev, NULL);
 fail1:
	input_free_device(input);
	kfree(ddata);
	/* If we have no platform data, we allocated pdata dynamically. */
	if (!dev_get_platdata(&pdev->dev))
		kfree(pdata);

	return error;
}

static int gpio_keys_remove(struct platform_device *pdev)
{
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

	sysfs_remove_group(&pdev->dev.kobj, &gpio_keys_attr_group);

	device_init_wakeup(&pdev->dev, 0);

	for (i = 0; i < ddata->pdata->nbuttons; i++)
		gpio_remove_key(&ddata->data[i]);

	input_unregister_device(input);

	/* If we have no platform data, we allocated pdata dynamically. */
	if (!dev_get_platdata(&pdev->dev))
		kfree(ddata->pdata);

	kfree(ddata);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gpio_keys_suspend(struct device *dev)
{
	struct gpio_keys_drvdata *ddata = dev_get_drvdata(dev);
	const struct gpio_keys_button *button = NULL;
	struct input_dev *input = ddata->input;
	int i;

	if (device_may_wakeup(dev)) {
		for (i = 0; i < ddata->pdata->nbuttons; i++) {
			struct gpio_button_data *bdata = &ddata->data[i];
			if (bdata->state) return -EBUSY;
			button = bdata->button;
			gpioKey_printk("button%d, enable irq wake, irq=%d, gpio=%d\n", i, bdata->irq, button->gpio_key_gpio.gpio);
			if ((check_scene_locked(SCENE_WLAN_STANDBY) == 0) || (check_scene_locked(SCENE_NORMAL_STANDBY) == 0)){
				//configure gpio key as wakeup src
				gpioKey_printk("configure gpio key as wakeup src\n");
				if(enable_wakeup_src(CPUS_GPIO_SRC, button->gpio_key_gpio.gpio) != 0){
					gpioKey_printk("configure gpio key as wakeup src fail\n");
				}else{
					gpioKey_printk("configure gpio key as wakeup src success\n");
				}
			} else {
				deep_standby = 1;
			}
			if (bdata->button->wakeup)
				enable_irq_wake(bdata->irq);
		}
	} else {
		mutex_lock(&input->mutex);
		if (input->users)
			gpio_keys_close(input);
		mutex_unlock(&input->mutex);
	}

	return 0;
}

static int gpio_keys_resume(struct device *dev)
{
	struct gpio_keys_drvdata *ddata = dev_get_drvdata(dev);
	struct input_dev *input = ddata->input;
	int error = 0;
	int i;
	if (device_may_wakeup(dev)) {
		for (i = 0; i < ddata->pdata->nbuttons; i++) {
			struct gpio_button_data *bdata = &ddata->data[i];
			if ((check_scene_locked(SCENE_WLAN_STANDBY) == 0) || (check_scene_locked(SCENE_NORMAL_STANDBY) == 0)){
				//do nothing
			}
			
			if (bdata->button->wakeup)
				disable_irq_wake(bdata->irq);
		}
	} else {
		mutex_lock(&input->mutex);
		if (input->users)
			error = gpio_keys_open(input);
		mutex_unlock(&input->mutex);
	}

	deep_standby = 0;

	if (error)
		return error;

	//gpio_keys_report_state(ddata);
	return 0;
}
#endif

static const struct of_device_id gpio_key_dt_ids[] = {
	{ .compatible = "gpio_keys", },
	{},
};
MODULE_DEVICE_TABLE(of, gpio_key_dt_ids);

static SIMPLE_DEV_PM_OPS(gpio_keys_pm_ops, gpio_keys_suspend, gpio_keys_resume);

static struct platform_driver gpio_keys_device_driver = {
	.probe		= gpio_keys_probe,
	.remove		= gpio_keys_remove,
	.driver		= {
		.name	= "gpio_keys",
		.owner	= THIS_MODULE,
		.pm	= &gpio_keys_pm_ops,
		.of_match_table = of_match_ptr(gpio_keys_of_match),
	}
};

static int __init gpio_keys_init(void)
{
    int ret ;

	ret = platform_driver_register(&gpio_keys_device_driver);
	gpioKey_printk("platform_driver_register ret=%d\n",ret);
	return ret;
}
#if 1

static struct gpio_keys_button sun8i_gpio_keys[] = {
	{
		.desc			= "key_pagedown",
		.code			= KEY_PAGEDOWN,
		.type = EV_KEY,
		.wakeup = 1,
		.can_disable = true,
	//	.gpio_key_gpio_one	= {
	//			.data           = 32,
	//			.gpio			= 1,
	//			.mul_sel		= 1,
	//			.pull			= 1,
	//			.drv_level		= 1,
	//	},
		.active_low		= 1,
	},
	{
		.desc			= "key_pageup",
		.code			= KEY_PAGEUP,
	//	.gpio_key_gpio_two	= {
	//			.data           = 33,
	//			.gpio			= 1,
	//			.mul_sel		= 1,
	////			.pull			= 1,
	//			.drv_level		= 1,
	//	},
		.active_low		= 1,
		.type = EV_KEY,
		.wakeup = 1,
		.can_disable = true,
	}
};

static struct gpio_keys_platform_data sun8i_gpio_keys_data = {
	.buttons	= sun8i_gpio_keys,
	.nbuttons	= ARRAY_SIZE(sun8i_gpio_keys),
};
#endif
static struct platform_device gpio_key_gpio_keys_device = {
	.name	= "gpio_keys",
	.id	= -1,
	.dev	= {
		.platform_data	= &sun8i_gpio_keys_data,
	},
};

static int __init gpio_keys_regist_init(void)
{
	int ret;
	int i;
	ret =platform_device_register(&gpio_key_gpio_keys_device);
	gpioKey_printk("platform_device_register ret =%d\n",ret);
	return ret;
}

static void __exit gpio_keys_exit(void)
{
	platform_driver_unregister(&gpio_keys_device_driver);
}

late_initcall(gpio_keys_init);
module_exit(gpio_keys_exit);


module_init(gpio_keys_regist_init);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>");
MODULE_DESCRIPTION("Keyboard driver for GPIOs");
MODULE_ALIAS("platform:gpio-keys");
