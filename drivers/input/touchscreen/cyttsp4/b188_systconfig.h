
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>

#include <linux/init-input.h>

#define CTP_IRQ_MODE			(IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND)

#define CYTTSP4_I2C_ADDR 0x24

#define CYTTSP4_I2C_NAME "cyttsp4_i2c_adapter"




extern int screen_max_x ;
extern int screen_max_y ;
extern int revert_x_flag ;
extern int revert_y_flag ;
extern int exchange_x_y_flag;

extern const char* fwname;;


//int	int_cfg_addr[]={PIO_INT_CFG0_OFFSET,PIO_INT_CFG1_OFFSET,
//			PIO_INT_CFG2_OFFSET, PIO_INT_CFG3_OFFSET};

/* Addresses to scan 
 union{
	unsigned short dirty_addr_buf[2];
	const unsigned short normal_i2c[2];
}u_i2c_addr = {{CYTTSP4_I2C_ADDR,I2C_CLIENT_END},};				

		*/
extern __u32 twi_id;

extern struct ctp_config_info config_info;

extern void ctp_print_info(struct ctp_config_info info,int debug_level);

extern int ctp_wakeup(int status,int ms);

extern int ctp_get_system_config(void);
extern int ctp_detect(struct i2c_client *client, struct i2c_board_info *info);
extern int setctp18V(void);





