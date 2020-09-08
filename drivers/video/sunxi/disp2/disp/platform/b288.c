#include "../eink.h"

static struct regulator *vbus_reg = NULL;
static u32 tcon_dclk_div;

static void configure_pins(u32 port, u32 pins, int value, int mode, int drive, int pull)
{
	volatile u32 *r;
	u32 dat, cfg[4], drv[2], pul[2];
	int i;

	if (port == -1) return;
	DBG("configure_pins: port=%d pins=%x value=%d mode=%d drive=%d pull=%d\n",
		port, pins, value, mode, drive, pull
	);
	r = (volatile u32 *) (PORT_BASE + port * 0x24);

	cfg[0] = r[0x0/4];
	cfg[1] = r[0x4/4];
	cfg[2] = r[0x8/4];
	cfg[3] = r[0xc/4];
	dat	= r[0x10/4];
	drv[0] = r[0x14/4];
	drv[1] = r[0x18/4];
	pul[0] = r[0x1c/4];
	pul[1] = r[0x20/4];

	for (i=0; i<32; i++) {
		if (! (pins & (1 << i))) continue;
		cfg[i/8] &= ~(0xf << ((i & 7) * 4));
		cfg[i/8] |= (mode << ((i & 7) * 4));
		dat &= ~(1 << i);
		dat |= (value << i);
		drv[i/16] &= ~(3 << ((i & 15) * 2));
		drv[i/16] |= (drive << ((i & 15) * 2));
		pul[i/16] &= ~(3 << ((i & 15) * 2));
		pul[i/16] |= (pull << ((i & 15) * 2));
	}

	r[0x10/4] = dat;
	r[0x0/4] = cfg[0];
	r[0x4/4] = cfg[1];
	r[0x8/4] = cfg[2];
	r[0xc/4] = cfg[3];
	r[0x14/4] = drv[0];
	r[0x18/4] = drv[1];
	r[0x1c/4] = pul[0];
	r[0x20/4] = pul[1];
}

static void set_pins(u32 port, u32 pins, int value)
{
	volatile u32 *r;
	u32 dat;

	if (port == -1) return;
	r = (volatile u32 *)(PORT_BASE + port * 0x24);
	dat = r[0x10/4];
	if (value) {
		dat |= pins;
	} else {
		dat &= ~pins;
	}
	r[0x10/4] = dat;
}

static int read_pins(u32 port)
{
	volatile u32 *r;
	if (port == -1) return 0;
	r = (volatile u32 *)(PORT_BASE + port * 0x24);
	return r[0x10/4];
}

void epdc_hw_pin_setup(int setup)
{
	int mode = (setup == LCD_PIN_ACTIVE) ? PIN_LCD : PIN_OUTPUT;
	int vsync1 = (setup == LCD_PIN_IDLE) ? 1 : 0;

	configure_pins(LCD_PORT, PIN_CLK, 0, mode, eink_param.strength, 0);
	configure_pins(LCD_PORT, eink_param.bus_width ? PINS_DATA16 : PINS_DATA8, 0, mode, eink_param.strength, 0);
	configure_pins(LCD_PORT, PINS_SYNC0, 0, mode, eink_param.strength, 0);
	configure_pins(LCD_PORT, PINS_SYNC1, vsync1, mode, eink_param.strength, 0);
}

void epdc_hw_gpio_configure(int gpio, int mode, int state)
{
	int port, pin;

	if (gpio == EPDC_GPIO_UNUSED) return;
	port = (gpio >> 8) & 31;
	pin = gpio & 31;
	state = ((gpio >> 15) ^ state) & 1;
	configure_pins(port, 1 << pin, state, (mode & 1), 1, 0);
}

void epdc_hw_gpio_set(int gpio, int state)
{
	int port, pin;

	if (gpio == EPDC_GPIO_UNUSED) return;
	port = (gpio >> 8) & 31;
	pin = gpio & 31;
	state = ((gpio >> 15) ^ state) & 1;
	DBG(" GPIO(%d,%d):%d\n", port, pin, state);
	set_pins(port, 1 << pin, state);
}

int epdc_hw_gpio_read(int gpio)
{
	int port, pin, state;

	if (gpio == EPDC_GPIO_UNUSED) return -1;
	port = (gpio >> 8) & 31;
	pin = gpio & 31;
	state = (read_pins(port) >> pin) & 1;
	DBG(" GPIO(%d,%d)=%d\n", port, pin, state);
	return state;
}

static int clock_reset(const char *name, int state)
{
	struct clk *hclk;
	int ret = 0;

	if (! name) return 0;
	hclk = clk_get(NULL, name);
	if (IS_ERR(hclk)) {
		ERR("could not get clock source '%s'\n", name);
		ret = -1;
	} else {
		clk_reset(hclk, state);
		DBG("clk_reset('%s',%d)\n", name, state);
		clk_put(hclk);
	}
	return ret;
}

static int clock_enable(const char *name)
{
	struct clk *hclk;
	int ret = 0;

	if (! name) return 0;
	hclk = clk_get(NULL, name);
	if (IS_ERR(hclk)) {
		ERR("could not get clock source '%s'\n", name);
		ret = -1;
	} else {
		epdc_clk_enable(hclk);
		DBG("clk_enable('%s')\n", name);
		clk_put(hclk);
	}
	return ret;
}

static int clock_disable(const char *name)
{
	struct clk *hclk;
	int ret = 0;

	if (! name) return 0;
	hclk = clk_get(NULL, name);
	if (IS_ERR(hclk)) {
		ERR("could not get clock source '%s'\n", name);
		ret = -1;
	} else {
		clk_disable(hclk);
		DBG("clk_disable('%s')\n", name);
		clk_put(hclk);
	}
	return ret;
}

static int get_clock_rate(const char *name)
{
	struct clk *hclk;
	int ret = 0;

	if (! name) {
  ERR("trying to get rate of inexisting clock\n");
  return 0;
	}
	hclk = clk_get(NULL, name);
	if (IS_ERR(hclk)) {
		ERR("could not get clock source '%s'\n", name);
	} else {
		ret = clk_get_rate(hclk);
		clk_put(hclk);
	}
	return ret;
}

static int set_clock_rate(const char *name, u32 freq)
{
	struct clk *hclk;
	int ret = 0;

	if (! name) return 0;
	hclk = clk_get(NULL, name);
	if (IS_ERR(hclk)) {
		ERR("could not get clock source '%s'\n", name);
		return -1;
	} else if (clk_get_rate(hclk) != freq) {
		if (clk_set_rate(hclk, freq) != 0) {
			ERR("could not set clock '%s' to %d\n", name, freq);
			ret = -1;
		} else {
			DBG("clk_set_rate('%s',%d)\n", name, freq);
		}
	}
	clk_put(hclk);
	return ret;
}

static int set_clock_source(const char *dname, const char *sname)
{
	struct clk *dclk, *sclk;
	int ret = 0;

	if (! sname) return 0;
	if (! dname) return 0;
	dclk = clk_get(NULL, dname);
	if (IS_ERR(dclk)) {
		ERR("could not get clock source '%s'\n", dname);
		return -1;
	}
	sclk = clk_get(NULL, sname);
	if (IS_ERR(sclk)) {
		ERR("could not get clock source '%s'\n", sname);
		return -1;
	}
	if (clk_get_parent(dclk) != sclk) {
		if (clk_set_parent(dclk, sclk) != 0) {
			ERR("could not set clock source '%s' for '%s'\n", sname, dname);
			ret = -1;
		} else {
			DBG("clk_set_parent('%s','%s')\n", dname, sname);
		}
	}
	clk_put(dclk);
	clk_put(sclk);
	return ret;
}

static int set_clock_divider(const char *dname, int d)
{
	struct clk *dclk, *pclk;
	u32 freq;
	int ret = 0;

	if (! dname) return 0;
	dclk = clk_get(NULL, dname);
	if (IS_ERR(dclk)) {
		ERR("could not get clock source '%s'\n", dname);
		return -1;
	}
	pclk = clk_get_parent(dclk);
	if (IS_ERR(pclk)) {
		ERR("could not get parent clock for '%s'\n", dname);
		clk_put(dclk);
		return -1;
	}
	freq = clk_get_rate(pclk);
	if (clk_set_rate(dclk, freq / d) != 0) {
		ERR("could not set divider for '%s' to %d\n", dname, d);
		ret = -1;
	} else {
		DBG("clk_set_rate('%s',%d/%d)\n", dname, freq, d);
	}
	clk_put(dclk);
	return ret;
}

static void clock_info(char *name)
{
	struct clk *hclk, *pclk;

	if (! name) return;
	hclk = clk_get(NULL, name);
	if (IS_ERR(hclk)) {
		DBG("%s: unavailable\n", name);
	} else {
		pclk = clk_get_parent(hclk);
		DBG("%s: %ld (%ld)\n", name, clk_get_rate(hclk), IS_ERR(pclk) ? 0 : clk_get_rate(pclk));
	}
	clk_put(hclk);
}

static void epdc_hw_power(int en)
{
#ifdef EPDC_VBUS_REGULATOR
	if (en) {
		if (! vbus_reg) vbus_reg  = regulator_get(NULL, EPDC_VBUS_REGULATOR);
		if (vbus_reg) {
			if (regulator_get_voltage(vbus_reg) != eink_param.vbus*1000) {
			  if (regulator_set_voltage(vbus_reg, eink_param.vbus*1000, eink_param.vbus*1000) == 0) {
					INF("EPDC_VBUS: %d mv\n", eink_param.vbus);
			  } else {
					ERR("epdc_hw_init: cannot set vbus_reg voltage\n");
				}
			}
		  regulator_enable(vbus_reg);
		} else {
				ERR("epdc_hw_init: cannot get vbus_reg\n");
		}
	} else {
	  if (vbus_reg) regulator_disable(vbus_reg);
	}
#endif
}

int epdc_hw_init(void)
{
	DBG("epdc_hw_init(pixclk=%d)\n", eink_param.timing.pixclk);

	//if (request_irq(LCDC0_IRQ, epdc_vbi_irqhandler, 0, MODULE_NAME, NULL) != 0) {
	//	ERR("could not register irq handler\n");
	//	return -1;
	//}

	// just for test
	//if (epdi.tim.pixclk != 0 && configure_clocks() != 0) return -1;

	epdc_hw_power(1);
	return 0;
}

void epdc_hw_shutdown(void)
{
	if (vbus_reg) regulator_put(vbus_reg);
	vbus_reg = NULL;
}

void epdc_hw_suspend(void)
{

}

void epdc_hw_resume(void)
{

}

void epdc_hw_open(void)
{
	//int vblank, start_delay;
	volatile u32 *p;

	DBG("epdc_hw_open\n");

	//configure_clocks();
	//clocks_on();
	//close_req = 0;

	// configure tcon

	TCON_GCTL |= (1 << 31);
	//vblank = vt - ysize;
	//start_delay = (vblank >= 32) ? 30 : vblank - 2;
	//TCON0_CTL = (start_delay << 4);
	//TCON0_DCLK = (1 << 31) | tcon_dclk_div;
	TCON0_DCLK |= (0xf << 28);
	TCON0_BASIC0 = ((eink_param.tcon.width-1) << 16) | (eink_param.tcon.height-1);
	TCON0_BASIC1 = ((eink_param.tcon.ht-1) << 16) | (eink_param.tcon.hbp-1);
	TCON0_BASIC2 = (eink_param.tcon.vt*2 << 16) | (eink_param.tcon.vbp-1);
	TCON0_BASIC3 = ((eink_param.tcon.hspw-1) << 16) | (eink_param.tcon.vspw-1);
	//TCON0_HV_IF = 0;
	//TCON0_IO_POL = 0x04000000;
	//TCON0_IO_TRI = 0;
	TCON_GINT0 = (1 << 31);
	//TCON_GINT0 = 0;
	//TCON_GINT1 = (line_int_num << 16);
	//TCON_GINT0 = (1 << 31) | (1 << 29);
	TCON0_CTL |= (1 << 31);

	//switch_mode_counter = 2;

	//dump_registers();

	//udelay(120);
	//configure_pins(DATA_CTL, PIN_LCD, epdi.strength, 0);
	//configure_pins(SYNC0_CTL ^ 0, PIN_LCD, epdi.strength, 0);
	//configure_pins(SYNC1_CTL ^ 1, PIN_LCD, epdi.strength, 0);

	epdc_hw_pin_setup(LCD_PIN_ACTIVE);
}


void epdc_hw_close(void)
{
	epdc_hw_pin_setup(LCD_PIN_IDLE);

	TCON_GINT0 &= ~(1 << 31);
	TCON0_CTL &= ~(1 << 31);
	//TCON_GCTL &= ~(1 << 31);
  TCON0_DCLK &= ~(0xf << 28);
	//clocks_off();

}

