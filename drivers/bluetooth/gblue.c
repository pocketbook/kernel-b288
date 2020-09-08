#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sys_config.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "gblue.h"

#define DEVICE_NAME "gblue"
#define DEVICE_MODEL "WB-8606D-30"
#define DEVICE_COUNT 1

static int major,minor;
static dev_t dev_number;
static struct class *gblue_class = NULL;
struct timer_list bt_timer;


struct bt_config_info{
	int bt_used;
	const char * bt_model;
	struct gpio_config power_gpio;
	struct gpio_config next_gpio;
	struct gpio_config play_pause_gpio;
	struct gpio_config prev_gpio;
	struct gpio_config led0_gpio;
}config_info;

static int status = STATUS_DISABLED;

enum{
	DEBUG_INIT = 1U << 0,
	DEBUG_SUSPEND = 1U << 1,
	DEBUG_TIMER = 1U << 2,
	DEBUG_CMD = 1U << 4,
};
static u32 debug_mask = DEBUG_INIT|DEBUG_TIMER|DEBUG_CMD|DEBUG_SUSPEND;

#define dprintk(level_mask,fmt,arg...)    if(unlikely(debug_mask & level_mask)) \
        pr_notice("Gblue:"fmt, ## arg)

const char* status_to_str(int sta){
	const char* str="";
	switch(sta){
		case STATUS_DISABLED:
			str="disabled";
			break;
		case STATUS_ENABLED:
			str="enabled";
			break;
		case STATUS_DISCONNECTED:
			str="disconnected";
			break;
		case STATUS_CONNECTED:
			str="connected";
			break;
	}
	return str;
}

static void bt_change_status(int new){
	if(status != new){
		status = new;
		switch(status){
			case STATUS_DISABLED:
				break;
			case STATUS_ENABLED:
				break;
			case STATUS_DISCONNECTED:
				break;
			case STATUS_CONNECTED:
				break;
		}
		dprintk(DEBUG_TIMER,"bt status change to:%s\n",status_to_str(status));
	}
}

static void bt_timer_handle(unsigned long data)
{	
	int val;
	static unsigned int num = 0;

	//dprintk(DEBUG_TIMER,"----------------bt_timer_handle-----------------\n");
	if(status==STATUS_DISABLED){
		num = 0;
		goto exit;
	}
	
	val = __gpio_get_value(config_info.led0_gpio.gpio);
	if(val){
		num = 0;
		bt_change_status(STATUS_DISCONNECTED);
	}else{
		if(num>20){
			bt_change_status(STATUS_CONNECTED);
		}else{
			bt_change_status(STATUS_DISCONNECTED);
			num++;
		}
	}
	
	//dprintk(DEBUG_TIMER,"val=%d,num=%d\n",val,num);
exit:
	mod_timer(&bt_timer,jiffies + HZ/20);
}


int bt_enable(int val)
{
    int ret = 0;

    if (0 != gpio_direction_output(config_info.power_gpio.gpio, val)){
		pr_err("power gpio set err!");
		ret = -1;
		return ret;
	}
	if(val){
		bt_change_status(STATUS_ENABLED);
	}else{
		bt_change_status(STATUS_DISABLED);
	}
    return ret;
}

int bt_next(void)
{
    int ret = 0;
	
	if (0 != gpio_direction_output(config_info.next_gpio.gpio, 1)){
		pr_err("next gpio set err!");
		ret = -1;
	}
	msleep(120);
    if (0 != gpio_direction_output(config_info.next_gpio.gpio, 0)){
		pr_err("next gpio set err!");
		ret = -1;
	}
    return ret;
}

int bt_play_pause(void)
{
    int ret = 0;
	if (0 != gpio_direction_output(config_info.play_pause_gpio.gpio, 1)){
		pr_err("play_pause gpio set err!");
		ret = -1;
	}
	msleep(120);
    if (0 != gpio_direction_output(config_info.play_pause_gpio.gpio, 0)){
		pr_err("play_pause gpio set err!");
		ret = -1;
	}
    return ret;
}

int bt_prev(void)
{
    int ret = 0;
	if (0 != gpio_direction_output(config_info.prev_gpio.gpio, 1)){
			pr_err("prev gpio set err!");
			ret = -1;
	}
	msleep(120);
    if (0 != gpio_direction_output(config_info.prev_gpio.gpio, 0)){
		pr_err("prev gpio set err!");
		ret = -1;
	}
    return ret;
}

int bt_clear_pair(void)
{
  
	int ret = 0;
  	if (0 != gpio_direction_output(config_info.next_gpio.gpio, 1)){
		pr_err("next gpio set err!");
		ret = -1;
	}
  	if (0 != gpio_direction_output(config_info.prev_gpio.gpio, 1)){
		pr_err("prev gpio set err!");
		ret = -1;
	}
	ssleep(4);
   	msleep(50);
	if (0 != gpio_direction_output(config_info.prev_gpio.gpio, 0)){
		pr_err("prev gpio set err!");
		ret = -1;
	}
	if (0 != gpio_direction_output(config_info.next_gpio.gpio, 0)){
		pr_err("next gpio set err!");
		ret = -1;
	}
	return ret;
}


static long gblue_ioctrl(struct file *filp,unsigned int cmd,unsigned long arg){
	int err = 0;
	
	if (_IOC_TYPE(cmd) != GBLUE_IOCTRL_MAGIC) 
        return -EINVAL;
    if (_IOC_NR(cmd) > GBLUE_IOCTRL_MAXNR) 
        return -EINVAL;

	if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
    if (err) 
        return -EFAULT;
	
	switch(cmd)
	{
		case GBLUE_IOCTRL_ENABLE:
			dprintk(DEBUG_CMD,"GBLUE_IOCTRL_ENABLE:%ld\n",arg);
			bt_enable((int)arg);
			break;
		case GBLUE_IOCTRL_NEXT:
			dprintk(DEBUG_CMD,"GBLUE_IOCTRL_NEXT\n");
			bt_next();
			break;
		case GBLUE_IOCTRL_PLAY_PAUSE:
			dprintk(DEBUG_CMD,"GBLUE_IOCTRL_PLAY_PAUSE\n");
			bt_play_pause();
			break;
		case GBLUE_IOCTRL_PREV:
			dprintk(DEBUG_CMD,"GBLUE_IOCTRL_PREV\n");
			bt_prev();
			break;
		case GBLUE_IOCTRL_CLEAR_PAIR:
			dprintk(DEBUG_CMD,"GBLUE_IOCTRL_CLEAR_PAIR\n");
			bt_clear_pair();
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static ssize_t gblue_write(struct file *file,const char __user *buf,size_t count,loff_t *ppos){
	char cmd[128];
	dprintk(DEBUG_CMD,"===gblue_write===cmd:%d,count:%d\n",sizeof(cmd),count);
	if(count>sizeof(cmd)){
		pr_err("cmd is to long!");
		goto write_exit;
	}

	if(copy_from_user(cmd,buf,count)){
		goto write_exit;
	}

	cmd[count-1] = '\0';

	if(!strcmp(cmd,"on")){
		dprintk(DEBUG_CMD,"on\n");
		bt_enable(1);
	}else if(!strcmp(cmd,"off")){
		dprintk(DEBUG_CMD,"off\n");
		bt_enable(0);
	}else if(!strcmp(cmd,"next")){
		dprintk(DEBUG_CMD,"next\n");
		bt_next();
	}else if(!strcmp(cmd,"pp")){
		dprintk(DEBUG_CMD,"play/pause\n");
		bt_play_pause();
	}else if(!strcmp(cmd,"prev")){
		dprintk(DEBUG_CMD,"prev\n");
		bt_prev();
	}else if(!strcmp(cmd,"clear")){
		dprintk(DEBUG_CMD,"clear pair\n");
		bt_clear_pair();
	}else{
		pr_notice("cmd:[%s] not suport!(cmd:on/off/next/pp/prev/clear)\n",cmd);
		goto write_exit;
	}
	return count;
write_exit:
	return -EINVAL;
}

static ssize_t gblue_read(struct file *file,char __user *buf,size_t count,loff_t *ppos){
	const char *sta_str;
	//dprintk(DEBUG_CMD,"===gblue_read===\n",);
	sta_str=status_to_str(status);
	if(copy_to_user(buf,(void*)sta_str,strlen(sta_str)+1)){
		return -EINVAL;
	}

	return strlen(sta_str);
}

void bt_print_info(struct bt_config_info info)
{
	dprintk(DEBUG_INIT,"info.bt_used:%d\n",info.bt_used);
	dprintk(DEBUG_INIT,"info.bt_model:%s\n",info.bt_model);
	dprintk(DEBUG_INIT,"info.power_gpio:%d\n",info.power_gpio.gpio);
	dprintk(DEBUG_INIT,"info.next_gpio:%d\n",info.next_gpio.gpio);
	dprintk(DEBUG_INIT,"info.play_pause_gpio:%d\n",info.play_pause_gpio.gpio);
	dprintk(DEBUG_INIT,"info.prev_gpio:%d\n",info.prev_gpio.gpio);
	dprintk(DEBUG_INIT,"info.int_status_gpio:%d\n",info.led0_gpio.gpio);
}


static int gblue_fetch_sysconfig_para(void)
{
        int ret = -1;

		struct device_node *np = NULL; 

		np = of_find_node_by_name(NULL,"bt_para");
		if (!np) {
			pr_err("ERROR! get bluetooth para failed, func:%s, line:%d\n",__FUNCTION__, __LINE__);
			goto script_get_err;
		}

		if (!of_device_is_available(np)) {
			pr_err("%s: bluetooth is not available\n", __func__);
			goto script_get_err;
		}

		ret = of_property_read_u32(np, "bt_used", &(config_info.bt_used));
		if (ret) {
			pr_err("%s:get bt_used is fail, %d\n",__func__, ret);
			goto script_get_err;
		}
		
		if(!config_info.bt_used){
			pr_err("%s: bluetooth is not used\n", __func__);
			return -1;
		}

				
		ret = of_property_read_string(np,"bt_model", &(config_info.bt_model));
		if (ret) {
		 	pr_err("get bt_model is fail, %d\n", ret);
			goto script_get_err;
		}

		config_info.power_gpio.gpio = of_get_named_gpio_flags(np, "bt_power", 0, (enum of_gpio_flags *)(&(config_info.power_gpio)));
		if (!gpio_is_valid(config_info.power_gpio.gpio))
			pr_err("%s: power_gpio is invalid. \n",__func__ );

		config_info.next_gpio.gpio = of_get_named_gpio_flags(np, "bt_next", 0, (enum of_gpio_flags *)(&(config_info.next_gpio)));
		if (!gpio_is_valid(config_info.next_gpio.gpio))
			pr_err("%s: next_gpio is invalid. \n",__func__ );

		config_info.play_pause_gpio.gpio = of_get_named_gpio_flags(np, "bt_pp", 0, (enum of_gpio_flags *)(&(config_info.play_pause_gpio)));
		if (!gpio_is_valid(config_info.play_pause_gpio.gpio))
			pr_err("%s: play_pause_gpio is invalid. \n",__func__ );

		config_info.prev_gpio.gpio = of_get_named_gpio_flags(np, "bt_prev", 0, (enum of_gpio_flags *)(&(config_info.prev_gpio)));
		if (!gpio_is_valid(config_info.prev_gpio.gpio))
			pr_err("%s: prev_gpio is invalid. \n",__func__ );

		config_info.led0_gpio.gpio = of_get_named_gpio_flags(np, "bt_led0", 0, (enum of_gpio_flags *)(&(config_info.led0_gpio)));
		if (!gpio_is_valid(config_info.led0_gpio.gpio))
			pr_err("%s: led0_gpio is invalid. \n",__func__ );
		bt_print_info(config_info);
        return 0;
script_get_err:
        pr_err(DEVICE_NAME":script_get_err\n");
        return -1;
}

static int bt_init_platform_resource(void)
{
	int ret = -1;

	if(0 != gpio_request(config_info.power_gpio.gpio, NULL)){
		pr_err("power gpio request is failed");
		return ret;
	}
	
	if (0 != gpio_direction_output(config_info.power_gpio.gpio, config_info.power_gpio.data)){
		pr_err("power gpio set err!");
		return ret;
	}	

	if(0 != gpio_request(config_info.next_gpio.gpio, NULL)) {
		pr_err("next gpio request is failed\n");
		return ret;
	}
	
	if (0 != gpio_direction_output(config_info.next_gpio.gpio, config_info.next_gpio.data)) {
		pr_err("next gpio set err!");
		return ret;
	}

	if(0 != gpio_request(config_info.play_pause_gpio.gpio, NULL)){
		pr_err("play_pause gpio request is failed");
		return ret;
	}
	
	if (0 != gpio_direction_output(config_info.play_pause_gpio.gpio, config_info.play_pause_gpio.data)){
		pr_err("play_pause gpio set err!");
		return ret;
	}	

	if(0 != gpio_request(config_info.prev_gpio.gpio, NULL)) {
		pr_err("prev gpio request is failed\n");
		return ret;
	}
	
	if (0 != gpio_direction_output(config_info.prev_gpio.gpio, config_info.prev_gpio.data)) {
		pr_err("prev gpio set err!");
		return ret;
	}

	if(0 != gpio_request(config_info.led0_gpio.gpio, NULL)) {
		pr_err("led0 gpio request is failed\n");
		return ret;
	}
	
	if (0 != gpio_direction_input(config_info.led0_gpio.gpio)) {
		pr_err("led0 gpio set err!");
		return ret;
	}

	ret = 0;
	return ret;
}

static void bt_free_platform_resource(void)
{
	gpio_free(config_info.power_gpio.gpio);
	gpio_free(config_info.next_gpio.gpio);
	gpio_free(config_info.play_pause_gpio.gpio);
	gpio_free(config_info.prev_gpio.gpio);
	gpio_free(config_info.led0_gpio.gpio);
	return;
}

#ifdef CONFIG_PM
static int gblue_suspend(struct device *dev,pm_message_t state)
{
	dprintk(DEBUG_SUSPEND,"%s,start\n",__func__);
	if (0 != gpio_direction_output(config_info.power_gpio.gpio, 0)){
		pr_err("gblue power gpio set err!");
		return -1;
	}

    return 0;
}

static int gblue_resume(struct device *dev)
{
	dprintk(DEBUG_SUSPEND,"%s,start\n",__func__);
	if(status != STATUS_DISABLED){
		if (0 != gpio_direction_output(config_info.power_gpio.gpio, 1)){
			pr_err("gblue power gpio set err!");
			return -1;
		}
	}
	return 0;
}
#endif


static struct file_operations dev_fops=
{
	.owner = THIS_MODULE,
	.unlocked_ioctl = gblue_ioctrl,
	.read = gblue_read,
	.write = gblue_write
};

static struct cdev gblue_cdev;

static int gblue_create_device(void){
	int ret = 0;
	int err = 0;
	cdev_init(&gblue_cdev,&dev_fops);
	gblue_cdev.owner = THIS_MODULE;

	err = alloc_chrdev_region(&gblue_cdev.dev,10,DEVICE_COUNT,DEVICE_NAME);
	if(err){
		pr_err("alloc_chrdev_region() failed\n");
		return err;
	}

	major = MAJOR(gblue_cdev.dev);
	minor = MINOR(gblue_cdev.dev);
	dev_number = gblue_cdev.dev;

	ret = cdev_add(&gblue_cdev,dev_number,DEVICE_COUNT);
	gblue_class = class_create(THIS_MODULE,DEVICE_NAME);
	gblue_class->suspend = gblue_suspend;
	gblue_class->resume = gblue_resume;
	device_create(gblue_class,NULL,dev_number,NULL,DEVICE_NAME);
	return ret;
}

static int gblue_destroy_device(void){
	device_destroy(gblue_class,dev_number);
	if(gblue_class)
		class_destroy(gblue_class);
	unregister_chrdev_region(dev_number,DEVICE_COUNT);

	return 0;
}

static int bt_timer_start(void){
	dprintk(DEBUG_TIMER, "bt_timer start\n");
	init_timer(&bt_timer);
	bt_timer.expires = jiffies + HZ/10;
	bt_timer.function = &bt_timer_handle;
	add_timer(&bt_timer);
	return 0;
}

static int bt_timer_stop(void){
	dprintk(DEBUG_TIMER, "bt_timer stop\n");	
	del_timer(&bt_timer);
	return 0;
}

static int gblue_init(void){
	int ret = 0;
	ret = gblue_create_device();
	if(ret){
		goto init_failed;
	}
	ret = gblue_fetch_sysconfig_para();
	if(ret){
		goto init_failed;
	}
	
	ret = bt_init_platform_resource();
	if(ret){
		goto init_failed;
	}

	bt_timer_start();
	dprintk(DEBUG_INIT,":initialized\n");
	return ret;
init_failed:
	pr_err(DEVICE_NAME"init failed! ret=%d\n",ret);	
	return ret;	
}

static void gblue_exit(void){
	bt_timer_stop();
	bt_free_platform_resource();
	gblue_destroy_device();
	dprintk(DEBUG_INIT,":exit\n");
}

module_init(gblue_init);
module_exit(gblue_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WB-8606D-30 BlueTooth module controller driver");
MODULE_AUTHOR("GuoGuo,guoguo@allwinnertech.com");
