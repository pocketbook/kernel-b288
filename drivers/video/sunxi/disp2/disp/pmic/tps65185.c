#include "../eink.h"
#include <linux/pm.h>
#include <linux/suspend.h>

#define TPS65185_I2C_NAME     "tps65185"
#define TPS65185_I2C_BUS      1
#define TPS65185_I2C_ADDRESS  0x68

#define REG_TMST        0x00
#define REG_ENABLE      0x01
#define REG_VADJ        0x02
#define REG_VCOM1       0x03
#define REG_VCOM2       0x04
#define REG_UPSEQ0      0x09
#define REG_UPSEQ1      0x0a
#define REG_DOWNSEQ0    0x0b
#define REG_DOWNSEQ1    0x0c
#define REG_TMST1       0x0d
#define REG_TMST2       0x0e
#define REG_REVID       0x10

#define V3P3_EN_MASK    0x20
#define VCOM_EN_MASK    0x10
#define VGG_EN_MASK     0x08
#define VPOS_EN_MASK    0x04
#define VEE_EN_MASK     0x02
#define VNEG_EN_MASK    0x01

static struct i2c_client *client = NULL;

static int up_delay_before=1, up_delay_after=4;
static int down_delay_before=0, down_delay_after=0;
static u32 upseq0=0xe4, upseq1=0x55, downseq0=0x1e, downseq1=0xe0;
static int vcom_enabled = 1;
static int vadj = 3;
static int tps_locked = 0;

static uint32_t TPS65185_WAKEUP_GPIO;
static uint32_t TPS65185_VCOM_GPIO;
static uint32_t TPS65185_PWRUP_GPIO;
static uint32_t TPS65185_PWRGOOD_GPIO;

static void set_wakeup(int state)
{
	//int curstate = epdc_hw_gpio_read(TPS65185_WAKEUP_GPIO);
	//if (curstate == state) return;
	DBG("WAKEUP:%d\n", state);
	epdc_hw_gpio_configure(TPS65185_WAKEUP_GPIO,  EPDC_GPIO_OUT, 1);
	epdc_hw_gpio_set(TPS65185_WAKEUP_GPIO, state);
	if (state) msleep(2);
}

static void tps_write_registers(void)
{
    int res = 0;
    res |= i2c_smbus_write_byte_data(client, REG_VADJ, vadj);
    res |= i2c_smbus_write_byte_data(client, REG_UPSEQ0, upseq0);
    res |= i2c_smbus_write_byte_data(client, REG_UPSEQ1, upseq1);
    res |= i2c_smbus_write_byte_data(client, REG_DOWNSEQ0, downseq0);
    res |= i2c_smbus_write_byte_data(client, REG_DOWNSEQ1, downseq1);
    if (res) {
        ERR("tps_write_registers: i2c error\n");
    }
}

static const struct i2c_board_info tps65185_info = { 
    .type = TPS65185_I2C_NAME,
    .addr = TPS65185_I2C_ADDRESS,
};

static int gpio_from_sysconfig(struct device_node *np, char *name, uint32_t *gpio)
{
	struct gpio_config cfg;

	cfg.gpio = of_get_named_gpio_flags(np, name, 0, (enum of_gpio_flags *)&cfg);
	if (gpio_is_valid(cfg.gpio)) {
		*gpio = (((cfg.gpio >> 5) & 15) << 8) | (cfg.gpio & 31);
		printk("gpio_from_sysconfig: [%s] %04x M%d P%d D%d V%d = %08x\n",
			name, cfg.gpio, cfg.mul_sel, cfg.pull, cfg.drv_level, cfg.data, *gpio);
		return 0;
	} else {
		printk("gpio_from_sysconfig: [%s] error\n", name);
		return -1;
	}
}

static int epdc_pmic_init_(void)
{
	struct i2c_adapter *adap;
	int revid=0;
	struct device_node *np = NULL;

	DBG("epdc_pmic_init\n");

	np = of_find_node_by_name(NULL, "tps65185");
	if (!np) {
		ERR("epdc_pmic_init_: no pmic config found\n");
		return -1;
  }
	if (!of_device_is_available(np)) {
		ERR("epdc_pmic_init_: pmic is disabled\n");
		return -1;
  }

	if (gpio_from_sysconfig(np, "tps65185_vcom",      &TPS65185_VCOM_GPIO) != 0) return -1;
	if (gpio_from_sysconfig(np, "tps65185_powerup",   &TPS65185_PWRUP_GPIO) != 0) return -1;
	if (gpio_from_sysconfig(np, "tps65185_wakeup",    &TPS65185_WAKEUP_GPIO) != 0) return -1;
	if (gpio_from_sysconfig(np, "tps65185_powergood", &TPS65185_PWRGOOD_GPIO) != 0) return -1;

	epdc_hw_gpio_configure(TPS65185_VCOM_GPIO,    EPDC_GPIO_OUT, 0);
	epdc_hw_gpio_configure(TPS65185_PWRUP_GPIO,   EPDC_GPIO_OUT, 0);
	epdc_hw_gpio_configure(TPS65185_WAKEUP_GPIO,  EPDC_GPIO_OUT, 1);
	epdc_hw_gpio_configure(TPS65185_PWRGOOD_GPIO, EPDC_GPIO_IN,  0);

	set_wakeup(1);

	adap = i2c_get_adapter(TPS65185_I2C_BUS);
	if (! adap) {
		ERR("epdc_pmic_init: could not get i2c adapter\n");
		return -1;
	}
	client = i2c_new_device(adap, &tps65185_info);
	if (! client) {
		ERR("epdc_pmic_init: could not register i2c device\n");
		return -1;
	}
	revid = i2c_smbus_read_byte_data(client, REG_REVID);
	if (revid < 0) {
		ERR("epdc_pmic_init: could not read PMIC revision\n");
		return -1;
	}
	DBG("TPS65185: revid=%x\n", revid);
	return 0;	
}

static void epdc_pmic_shutdown_(void)
{
}

static int epdc_pmic_suspend_(void)
{
#ifdef CONFIG_ARCH_SUN5I
    if (g_suspend_state != PM_SUSPEND_PARTIAL) {
        set_wakeup(0);
    }
#endif
    return 0;
}

static void epdc_pmic_resume_(void)
{
#ifdef CONFIG_ARCH_SUN5I
    if (g_suspend_state != PM_SUSPEND_PARTIAL) {
        set_wakeup(1);
        tps_write_registers();
    }
#endif
}

static void epdc_pmic_pwrseq_(short *upseq, short *downseq, int vhigh_level)
{
    int vgg_s, vpos_s, vee_s, vneg_s, ns, dly[4];
    short  seq[16], *sp;

		memcpy(seq, upseq, 16 * sizeof(short));
    ns = up_delay_before = up_delay_after = 0;
    for (sp=seq; *sp; sp++) ;
    while (sp > 0 && *(sp-1) > 0) up_delay_after += *(--sp);
    *sp = 0;
    sp = seq;
    while (*sp > 0) up_delay_before += *(sp++);
    vgg_s = vpos_s = vee_s = vneg_s = 3;
    dly[0] = dly[1] = dly[2] = dly[3] = 0;
    while (*sp) {
        switch (*sp) {
            case EPDC_VGG:  vgg_s  = ns; break;
            case EPDC_VPOS: vpos_s = ns; break;
            case EPDC_VEE:  vee_s  = ns; break;
            case EPDC_VNEG: vneg_s = ns; break;
            default:
                if (*sp > 0 && ns < 3) {
                    dly[ns] = 0;
                    if (*sp > 3) dly[ns] = 1;
                    if (*sp > 6) dly[ns] = 2;
                    if (*sp > 9) dly[ns] = 3;
                    ns++;
                }
                break;
        }
        sp++;
    }
    upseq0 = (vgg_s << 6) | (vpos_s << 4) | (vee_s << 2) | vneg_s;
    upseq1 = (dly[2] << 6) | (dly[1] << 4) | (dly[0] << 2);

		memcpy(seq, downseq, 16 * sizeof(short));
    ns = down_delay_before = down_delay_after = 0;
    for (sp=seq; *sp; sp++) ;
    while (sp > 0 && *(sp-1) > 0) down_delay_after += *(--sp);
    *sp = 0;
    sp = seq;
    while (*sp > 0) down_delay_before += *(sp++);
    vgg_s = vpos_s = vee_s = vneg_s = 0;
    dly[0] = dly[1] = dly[2] = dly[3] = 0;
    while (*sp) {
        switch (*sp) {
            case EPDC_VGG:  vgg_s  = ns; break;
            case EPDC_VPOS: vpos_s = ns; break;
            case EPDC_VEE:  vee_s  = ns; break;
            case EPDC_VNEG: vneg_s = ns; break;
            default:
                if (*sp > 0 && ns < 3) {
                    dly[ns] = 0;
                    if (*sp > 6) dly[ns] = 1;
                    if (*sp > 12) dly[ns] = 2;
                    if (*sp > 24) dly[ns] = 3;
                    ns++;
                }
                break;
        }
        sp++;
    }
    downseq0 = (vgg_s << 6) | (vpos_s << 4) | (vee_s << 2) | vneg_s;
    downseq1 = (dly[2] << 6) | (dly[1] << 4) | (dly[0] << 2);

    if (vhigh_level >= 15000) {
        vadj = 3;
    } else if (vhigh_level >= 14750) {
        vadj = 4;
    } else if (vhigh_level >= 14500) {
        vadj = 5;
    } else if (vhigh_level >= 14250) {
        vadj = 6;
    } else {
        vadj = 7;
    }

    DBG("epdc_pmic_pwrseq: UPSEQ0=%x UPSEQ1=%x DOWNSEQ0=%x DOWNSEQ1=%x VADJ=%d\n",
        upseq0, upseq1, downseq0, downseq1, vadj
    );
    tps_write_registers();
}

static void epdc_pmic_wakeup_(int state)
{
		set_wakeup(1);
}

static void epdc_pmic_vdd_(int state)
{
    int v;
    if (tps_locked && state == 0) return;
    DBG("VDD %s\n", state ? "on": "off");
    //epdc_log_update('P', -EPDC_VDD, state, NULL);
		set_wakeup(1);
    v = i2c_smbus_read_byte_data(client, REG_ENABLE);
    if (v < 0) {
        ERR("epdc_pmic_vdd(%d): read failed\n", state);
        return;
    }
    v = (state == 0) ? (v & ~V3P3_EN_MASK) : (v | V3P3_EN_MASK);
    if (i2c_smbus_write_byte_data(client, REG_ENABLE, v) < 0) {
        ERR("epdc_pmic_vdd(%d): write failed\n", state);
    }
}

static void epdc_pmic_vhigh_(int state)
{
    int i;

    DBG("VHIGH %s\n", state ? "on": "off");
    //epdc_log_update('P', -EPDC_VHIGH, state, NULL);
    mdelay(state ? up_delay_before : down_delay_before);
    epdc_hw_gpio_set(TPS65185_PWRUP_GPIO, state);
    for (i=120; i>0; i--) {
        mdelay(1);
        if (epdc_hw_gpio_read(TPS65185_PWRGOOD_GPIO) == state) break;
    }
    if (i == 0) {
        ERR("epdc_pmic_vhigh(%d): timeout waiting for pwrgood\n", state);
    }
    mdelay(state ? up_delay_after : down_delay_after);
}

static void epdc_pmic_vcom_(int state)
{
    DBG("VCOM %s %s\n", state ? "on" : "off", vcom_enabled ? "" : "(disabled)");
    //epdc_log_update('P', -EPDC_VCOM, state, NULL);
    epdc_hw_gpio_set(TPS65185_VCOM_GPIO, vcom_enabled ? state : 0);
}

static void epdc_pmic_vcom_set_(int vcom, int vcomoffset, int permanent)
{
    int i, vcom1, vcom2;

    DBG("VCOM %d+(%d)\n", vcom, vcomoffset);
    vcom = -(vcom + vcomoffset);
    //epdc_log_update('V', 0, vcom, NULL);
    if (vcom == 0) {
        vcom_enabled = 0;
    } else {
        vcom_enabled = 1;
        vcom1 = (vcom / 10) & 0xff;
        vcom2 = ((vcom / 10) >> 8) & 1;
				set_wakeup(1);
        if (i2c_smbus_write_byte_data(client, REG_VCOM1, vcom1) != 0 ||
            i2c_smbus_write_byte_data(client, REG_VCOM2, vcom2) != 0
        ) {
            ERR("epdc_pmic_vcom_set: could not write vcom value\n");
        }
        if (permanent) {
            vcom2 |= (1 << 6); // PROG
            i2c_smbus_write_byte_data(client, REG_VCOM2, vcom2);
            for (i=0; i<20; i++) {
                vcom2 = i2c_smbus_read_byte_data(client, REG_VCOM2);
                if (vcom2 >= 0 && (vcom2 & (1 << 6)) == 0) return;
                msleep(5);
            }
            ERR("epdc_pmic_vcom_set: timeout\n");
        }
    }
}

static int epdc_pmic_vcom_read_(void)
{
		set_wakeup(1);
    int vcom1 = i2c_smbus_read_byte_data(client, REG_VCOM1);
    int vcom2 = i2c_smbus_read_byte_data(client, REG_VCOM2);
    if (vcom1 < 0 || vcom2 < 0) {
        ERR("epdc_pmic_vcom_read failed\n");
        return 0;
    }
    return -(((vcom1 & 0xff) + ((vcom2 & 1) << 8)) * 10);
}

static void epdc_pmic_vcom_hiz_(int state)
{
		set_wakeup(1);
    int vcom2 = i2c_smbus_read_byte_data(client, REG_VCOM2);
    if (vcom2 < 0) {
        ERR("epdc_pmic_vcom_hiz: vcom2 read failed\n");
        return;
    }
    vcom2 = state ? (vcom2 | (1 << 5)) : (vcom2 & ~(1 << 5));
    if (i2c_smbus_write_byte_data(client, REG_VCOM2, vcom2) != 0)
    {
        ERR("epdc_pmic_vcom_hiz: could not write vcom2 value\n");
    }
}

static void epdc_pmic_vcom_acq_(void)
{
    int i;
		set_wakeup(1);
    int vcom2 = i2c_smbus_read_byte_data(client, REG_VCOM2);
    if (vcom2 < 0) {
        ERR("epdc_pmic_vcom_acq: vcom2 read failed\n");
        return;
    }
    vcom2 &= ~(3 << 3);
    vcom2 |= (2 << 3);
    i2c_smbus_write_byte_data(client, REG_VCOM2, vcom2);
    vcom2 |= (1 << 7);
    i2c_smbus_write_byte_data(client, REG_VCOM2, vcom2);
    for (i=0; i<20; i++) {
        vcom2 = i2c_smbus_read_byte_data(client, REG_VCOM2);
        if (vcom2 >= 0 && (vcom2 & (1 << 7)) == 0) return;
	msleep(5);
    }
    ERR("epdc_pmic_vcom_acq: timeout\n");
}

static int epdc_pmic_temperature_(int *temp)
{
    int i, v, stat;

		set_wakeup(1);
    i2c_smbus_write_byte_data(client, REG_TMST1, 0x80);
    for (i=50; i>0; i--) {
        mdelay(1);
        stat = i2c_smbus_read_byte_data(client, REG_TMST1);
        if (stat < 0) return -1;
        if (stat & 0x20) break;
    }
    if (i == 0) {
        ERR("epdc_pmic_temperature: timeout\n");
        return -1;
    }
    v = i2c_smbus_read_byte_data(client, REG_TMST);
    if (v < 0) return -1;
    *temp = (v < 128) ? v : v - 256;
    return 0;
}

struct epdc_pmic_driver epdc_pmic_driver_tps65185 = {
	.name = "TPS65185",
	.epdc_pmic_init = epdc_pmic_init_,
	.epdc_pmic_shutdown = epdc_pmic_shutdown_,
	.epdc_pmic_suspend = epdc_pmic_suspend_,
	.epdc_pmic_resume = epdc_pmic_resume_,
	.epdc_pmic_wakeup = epdc_pmic_wakeup_,
	.epdc_pmic_pwrseq = epdc_pmic_pwrseq_,
	.epdc_pmic_vdd = epdc_pmic_vdd_,
	.epdc_pmic_vhigh = epdc_pmic_vhigh_,
	.epdc_pmic_vcom = epdc_pmic_vcom_,
	.epdc_pmic_vcom_set = epdc_pmic_vcom_set_,
	.epdc_pmic_vcom_read = epdc_pmic_vcom_read_,
	.epdc_pmic_vcom_hiz = epdc_pmic_vcom_hiz_,
	.epdc_pmic_vcom_acq = epdc_pmic_vcom_acq_,
	.epdc_pmic_temperature = epdc_pmic_temperature_,
};

EXPORT_SYMBOL(epdc_pmic_driver_tps65185);

