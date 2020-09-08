#ifndef __EINK_H__
#define __EINK_H__

#define SUPPORT_EINK

#include "linux/clk-provider.h"
#include "linux/memblock.h"

extern int epdc_debuglevel;

#define ERR(x...)  { if (epdc_debuglevel >= 1) printk(x); }
#define INF(x...)  { if (epdc_debuglevel >= 2) printk(x); }
#define DBG(x...)  { if (epdc_debuglevel >= 3) printk(x); }
#define DDBG(x...) { if (epdc_debuglevel >= 4) printk(x); }
#define FDBG(x...) { if (epdc_debuglevel >= 5) printk(x); }

#define AXIDBG(x...) { if (epdc_debuglevel >= 3) printk(x); }
#define PDBG(x...)   { if (epdc_debuglevel >= 3) printk(x); }

#define XLOG1(x...)

#include "de/include.h"

#define WAVEFORM_PATH "/boot/default.wbf"
#define VCOM_PATH     "/boot/vcom.override"

#define EPDC_WAVEFORM_ADDR 0x42000000
#define EPDC_WAVEFORM_SIZE 2048000

#define EPDC_BOOTLOGO_ADDR 0x42200000
#define EPDC_BOOTLOGO_SIZE 2048000

#define DEFAULT_VCOM -1500

#define BATTERY_MIN_VOLTAGE      3600
#define BATTERY_CRITICAL_VOLTAGE 3500

//#define DEFAULT_DEBUGLEVEL 2
//#define DEFAULT_DEBUGLEVEL 5

#define MAX_WF_MODES 16
#define MAX_TEMP_RANGES 32
#define MAX_WF_SIZE (1024*1024)

#define MAX_PIPELINES 16
#define MAX_COLLISIONS 8

#define EPDC_SEND_UPDATE               _IOW('F', 0x2E, struct epdc_update_data)
#define EPDC_WAIT_FOR_UPDATE_COMPLETE  _IOW('F', 0x2F, __u32)
#define EPDC_FLUSH_UPDATES             _IO ('F', 0x3A)
#define EPDC_GET_WAVEFORM_HEADER       _IOW('F', 0x50, void *)
#define EPDC_SET_FEATURES              _IOW('F', 0x51, __u32)
#define EPDC_ADJUST_VCOM               _IOW('F', 0x54, __u32)

#define EPDC_GET_UPDATE_STATE          _IOR('F', 0x55, __u32)

#define EPDC_FEATUREMASK  0x003f0000

#define EPDC_PIXELCHANGED 0x80000000
#define EPDC_GRAYSCALE    0x40000000
#define EPDC_FULL         0x10000000
#define EPDC_PARTIALHQ    0x02000000
#define EPDC_FORCEBW      0x01000000
#define EPDC_DEFERRED     0x00800000

#define EPDC_FORCELUT     0x00000080
#define EPDC_LOGO         0x00000040
#define EPDC_CC           0x00000020

#define EPDC_COLLISION_APPLY    0x00010000
#define EPDC_COLLISION_WAIT     0x00020000
#define EPDC_COLLISION_ABORT    0x00040000
#define EPDC_COLLISION_COMPLETE 0x00080000

#define EPDC_DEFAULT_FEATURES (EPDC_COLLISION_APPLY | EPDC_COLLISION_COMPLETE)

#define EPDC_WFTYPE_INIT         0  // not used
#define EPDC_WFTYPE_DU           1  // bw update
#define EPDC_WFTYPE_GC16         2  // flashing 16-grayscale update
#define EPDC_WFTYPE_GC4          3  // non-flashing 4-grayscale update
#define EPDC_WFTYPE_A2           4  // fast bw update, low quality
#define EPDC_WFTYPE_GL16         5  // low-flashing 16-grayscale update
#define EPDC_WFTYPE_A2IN         6  // enter A2
#define EPDC_WFTYPE_A2OUT        7  // exit A2
#define EPDC_WFTYPE_GS16        14  // non-flashing 16-grayscale update
#define EPDC_WFTYPE_GC16HQ      15  // not used

// waveform priority:  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
#define EPDC_WF_RANGES 0, 4, 8, 6, 1, 7, 2, 3, 0, 0, 0, 0, 0, 0, 5, 9

#define EINK_POWER_NUM        4
#define WAVE_DATA_BUF_NUM     4
#define INDEX_BUFFER_NUM      2

#define EE_INDEX  (1 << 0)
#define EE_DECODE (1 << 1)

#define PIPE_MANAGER_FREE 0
#define PIPE_MANAGER_BUSY 1

#define PPM_INSERT (1 << 0)
#define PPM_REMOVE (1 << 1)

#define WAIT_FRAMES 4

#define EPDC_MAX_FSL 16
#define EPDC_MAX_FBL 16
#define EPDC_MAX_FEL 24
#define EPDC_MAX_LSL 30
#define EPDC_MAX_LBL 30
#define EPDC_MAX_LEL 120

#define EPDC_VDD   -1
#define EPDC_VEE   -2
#define EPDC_VGG   -3
#define EPDC_VPOS  -4
#define EPDC_VNEG  -5
#define EPDC_VCOM  -6
#define EPDC_VHIGH -7

#define EPDC_GPIO(port,pin,inv) (((inv)<<15)|((port)<<8)|(pin))
#define EPDC_GPIO_UNUSED        0xffff
#define EPDC_GPIO_IN            0
#define EPDC_GPIO_OUT           1

#define LCD_PIN_IDLE    0
#define LCD_PIN_ACTIVE  1
#define LCD_PIN_SUSPEND 2

#define FB_ALIGN(x)  (((x) + 7) & ~7)

#include "platform/b288.h"

struct area_info
{
	uint16_t x_top;
	uint16_t y_top;
	uint16_t x_bottom;
	uint16_t y_bottom;
	uint8_t  wf;
	uint8_t  mode;
};

struct waveform_data
{
	char name[64];
	char amepd_applied[120];
	u8 type;
	u8 mode_ver;
	u8 panel_size;
	u8 amepd_part;
	u8 framerate;
	u8 timing_mode;
	u8 awv;
	u8 ntemp;
	u8 tempindex[MAX_TEMP_RANGES];
	int pagesize;

	u8 ready[MAX_WF_MODES];
	u8 *vaddr[MAX_WF_MODES][MAX_TEMP_RANGES];
	phys_addr_t paddr[MAX_WF_MODES][MAX_TEMP_RANGES];
	int total[MAX_WF_MODES][MAX_TEMP_RANGES];
	int vcomoffset[MAX_WF_MODES][MAX_TEMP_RANGES];

	void *binary;
	int binary_length;
};

struct eink_timing_param
{
	int vblank; //
	int fbl; //
	int fsl; //
	int fdl; //
	int fel; //
	int lbl; //
	int lsl;
	int ldl; //
	int lel; //
	int width;  //image width
	int height;  //image height

	int sdoe_sta;
	int sdoe_toggle;
	int gdoe_sta;
	int gdck_high;
	int gdck_sta;
	int gdsp_offset;

	int pixclk;
	int slow_motion;
};

struct eink_ee_param
{
	int img_width;
	int img_height;
	int wav_pitch;
	int wav_offset;
	int edma_wav_width;
	int edma_wav_height;
	int edma_img_x;
	int edma_img_y;
	int edma_img_w;
	int edma_img_h;
};

struct eink_tcon_param
{
  int width;
  int height;
  int ht;
  int hbp;
  int hspw;
  int vt;
  int vbp;
  int vspw;
};

enum eink_flash_mode
{
	FLASH_LOCAL,
	FLASH_GLOBAL,
	FLASH_INIT
};

struct eink_image_slot
{
	uint8_t    *vaddr;
	phys_addr_t paddr;
	int         memsize;
	int         width;
	int         height;
	int         scanline;
	int         orientation;
};

struct index_buffer
{
	void       *vaddr;
	phys_addr_t paddr;
	int         size;
	int         width;
	int         height;
	int         scanline;
	struct area_info  area;  // last changed area
};

struct eink_init_param
{
	bool                     used;
	u8                       eink_moudule_type;
	u8                       eink_version_type;
	u8                       eink_ctrl_data_type;
	u8                       bus_width;   /*0->8data,1->16data*/
  int                      temperature;
  int                      temp_offset;
  bool                     temp_fixed;
  int                      strength;

  short                    powerup_sequence[16];
  short                    powerdown_sequence[16];
  int                      powerup_vcom_delay;
  int                      powerdown_vcom_delay;
  int                      powerdown_vhigh_delay;
  int                      powerdown_vdd_delay;
  int                      vhigh_level;
	int                      vcom_permanent;
	int                      vcom;
	int                      vcom_offset;
	int                      vtouch;
	int                      vbus;
	bool                     timing_updated;
	struct eink_timing_param timing;
	struct eink_ee_param     ee;
	struct eink_tcon_param   tcon;

  bool                     dump_updates;
};

struct epdc_update_data
{
	struct {
		u32 top;
		u32 left;
		u32 width;
		u32 height;
	} update_region;
	u32 waveform_mode;
	u32 update_mode;
	u32 update_marker;
	int temp;
	uint flags;
	u32 _alt_buffer_data[7];
};

struct epdc_pmic_driver
{
  char  *name;
  int  (*epdc_pmic_init)(void);
  void (*epdc_pmic_shutdown)(void);
  int  (*epdc_pmic_suspend)(void);
  void (*epdc_pmic_resume)(void);
  void (*epdc_pmic_wakeup)(void);
  void (*epdc_pmic_pwrseq)(short *upseq, short *downseq, int vhigh_level);
  void (*epdc_pmic_vdd)(int state);
  void (*epdc_pmic_vhigh)(int state);
  void (*epdc_pmic_vcom)(int state);
  void (*epdc_pmic_vcom_set)(int vcom, int vcomoffset, int permanent);
  int  (*epdc_pmic_vcom_read)(void);
  void (*epdc_pmic_vcom_hiz)(int state);
  void (*epdc_pmic_vcom_acq)(void);
  int  (*epdc_pmic_temperature)(int *temp);
};

extern struct eink_init_param eink_param;   // parameters read from sysconfig
extern struct waveform_data   wfdata;
extern struct epdc_pmic_driver *pmic;

extern struct eink_image_slot framebuffer_slot;
extern struct eink_image_slot shadow_slot;
extern struct index_buffer indexmap;

/* inline debug */

static inline void IDUMP(char *s, void *ptr, int count)
{
	char tmpbuf[256];
	int ii;
	for (ii=0; ii<count && 3*ii<sizeof(tmpbuf)-4; ii++) sprintf(tmpbuf+3*ii, "%02x.", ((uint8_t *)ptr)[ii]);
	FDBG("%s: %s\n", s, tmpbuf);
}

static inline void FDUMP(char *s, void *ptr, int count)
{
	static const char FSTATE[4] = "-@*?";
	char tmpbuf[256];
	int ii;
	for (ii=0; ii<count && ii<sizeof(tmpbuf)-1; ii++) {
		int idx = (ii / 8) * 4 + 1 + ((ii / 4) & 1);
		int sh = ((ii & 3) << 1);
		tmpbuf[ii] = FSTATE[(((uint8_t *)ptr)[idx] >> sh) & 3];
	}
	tmpbuf[ii] = 0;
	FDBG("%s: %s\n", s, tmpbuf);
}

static inline int x_clk_set_rate(struct clk *clk, unsigned long rate)
{
	DDBG("  clk_set_rate(%s,%lu)\n", __clk_get_name(clk), rate)
	return clk_set_rate(clk, rate);
}

static inline unsigned long x_clk_get_rate(struct clk *clk)
{
	unsigned long rate = __clk_get_rate(clk);
	DDBG("  clk_get_rate(%s)=%lu\n", __clk_get_name(clk), rate);
	return rate;
}

static inline struct clk *x_clk_get_parent(struct clk *clk)
{
	struct clk *parent = __clk_get_parent(clk);
	DDBG("  clk_get_parent(%s)=%s\n", __clk_get_name(clk), parent ?  __clk_get_name(parent) : "none");
	return parent;
}

/* hardware */

void epdc_hw_pin_setup(int setup);

void epdc_hw_gpio_configure(int pin, int mode, int state);
void epdc_hw_gpio_set(int pin, int state);
int  epdc_hw_gpio_read(int gpio);

int  epdc_hw_init(void);
void epdc_hw_suspend(void);
void epdc_hw_resume(void);
void epdc_hw_shutdown(void);

void epdc_hw_open(void);
void epdc_hw_close(void);

/* contiguous memory allocator */

void *disp_malloc(u32 num_bytes, void *phy_addr);
void  disp_free(void *virt_addr, void *phy_addr, u32 num_bytes);

/* waveform loader */

int epdc_waveform_load_from_memory(phys_addr_t addr, size_t size);
int  epdc_waveform_load(const char *filename);
void epdc_waveform_unload(void);
int  epdc_waveform_parse(u8 *data, int len);
bool epdc_waveform_supported(int wf);
phys_addr_t epdc_get_waveform_data(int wf, int updmode, u32 temp, u32 *total_frames);
int epdc_get_waveform_bits(void);
int epdc_vcom_read(int vcom_default);

/* amepd */

int epdc_set_display_parameters(int xres, int yres, int wftype, char *name, int amepd, int bus_width);

/* bootlogo */

int epdc_bootlogo_thread(void *p_arg);

/* wavering */

int epdc_init_wavering(void);
void epdc_update_timings(void);
void clear_wavedata_buffer(void);
phys_addr_t request_buffer_for_display(void);
phys_addr_t epdc_wavering_get_empty(void);
phys_addr_t request_buffer_for_decode(void);
int queue_wavedata_buffer(void);
int dequeue_wavedata_buffer(void);
int clean_used_wavedata_buffer(void);

/* eink manager */

int eink_manager_init(disp_bsp_init_para *para);
void eink_get_size(int *width, int *height);
int eink_update_image(struct area_info *update_area, uint32_t *flags);
int eink_wait_complete(void);
int eink_get_update_state(void);
s32 eink_suspend(void);
s32 eink_resume(void);

int eink_tcon_interrupt(void);

extern int eink_irq_query_index(void);

/* index manager */

s32 index_manager_init(disp_bsp_init_para *para);
s32 index_calc_start(struct area_info *update_area, int isfull,
	    phys_addr_t old_index_data_paddr, phys_addr_t new_index_data_paddr,
	    struct eink_image_slot *last_image, struct eink_image_slot *current_image);

int disp_index_irq_handler(void);

/* pipeline manager */

int pipeline_manager_init(void);
int ppm_free_list_status(void);
int ppm_used_list_status(void);
int ppm_is_active(void);
int ppm_clear_pipeline_list(void);
uint32_t ppm_update_pipeline_list(struct area_info *new_rect);
int ppm_config_one_pipeline(struct area_info *update_area, int temperature, int *tframes);
void ppm_combine_delayed(struct area_info *update_area);
void ppm_lock(void);
void ppm_unlock(void);

/* sysfs */

int epdc_sysfs_create_groups(struct kobject *kobj);

/* pmic */

extern int tps65185_vcom_set(int vcom_mv);
extern int tps65185_temperature_get(int *temp);

/* neon */

void     epdc_neon_init(void *ptr);
uint32_t epdc_neon_complete(void *ptr);
uint32_t epdc_neon_cw_step(void *fbptr, uint32_t fbscanline, void *imapptr, uint32_t imapscanline);
uint32_t epdc_neon_ccw_step(void *fbptr, uint32_t fbscanline, void *imapptr, uint32_t imapscanline);

#endif

