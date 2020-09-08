#include "../eink.h"

#define NC  0x7fffffff
#define ANY 0x7fffffff

#define EPDC_VDD   -1
#define EPDC_VEE   -2
#define EPDC_VGG   -3
#define EPDC_VPOS  -4
#define EPDC_VNEG  -5
#define EPDC_VCOM  -6
#define EPDC_VHIGH -7

typedef struct {

	char *names;
	int types[];

} amepd_list;

static short UP1[16] = { 1, EPDC_VNEG, 6, EPDC_VEE, 6, EPDC_VPOS, 6, EPDC_VGG, 4, 0 };
static short DN1[16] = { EPDC_VGG, 6, EPDC_VPOS, 6, EPDC_VNEG, 48, EPDC_VEE, 0 };
                                                                        
static short UP2[16] = { 1, EPDC_VNEG, 12, EPDC_VEE, 6, EPDC_VPOS, 6, EPDC_VGG, 4, 0 };
static short DN2[16] = { EPDC_VGG, 6, EPDC_VPOS, 12, EPDC_VEE, 48, EPDC_VNEG, 4, 0 };

static amepd_list am_seq2 = { NULL, { 0x31, 0x2F, 0x32, 0x24, 0x2E, 0x1E, 0x12, 0x3D, 0xAF, 0xCD, 0xCE, 0x1B, 0x30,
                         0x2b, 0x08, 0x09, 0x41, 0x14,
                         0x59, 0x7d, // only for GPIO PM
                         0 } };
static amepd_list am_scg = { NULL, { 0x38, 0x39, 0x3a, 0x3B, 0x3C, 0 } };
static amepd_list am_sc3 = { NULL, { 0x08, 0 } };
static amepd_list am_xd4 = { "ED060XD4", { 0x02, 0x04, 0x06, 0x08, 0x0a, 0x12, 0x13, 0x0e, 0 } };
static amepd_list am_xh7 = { "ED060XH7", { 0x50, 0x92, 0 } };
static amepd_list am_xh8 = { "ED060XH8", { 0x77, 0x84, 0x94, 0 } };
static amepd_list am_sct = { "ED060SCT", { 0x4D, 0x85, 0x9b, 0 } };
static amepd_list am_xcd = { "ED060XCD", { 0x3D, 0x3E, 0 } };
static amepd_list am_tc1 = { "ED080TC1", { 0xa0, 0xa1, 0 } };
static amepd_list am_sd1 = { "ED060SD1", { 0x4E, 0 } };
static amepd_list am_kh6 = { "ED060KH6", { 0 } };


static const struct display_param {

    char *name;                 // description to show in sysfs
    int xres;                   // panel width
    int yres;                   // panel height
    int wftype;                 // waveform type: 0=(up to A9) 1=(AA...AD)
    amepd_list *parts;          // AMEPD part numbers

    int fsl;                    // FSL
    int fbl;                    // FBL
    int fdl;                    // FDL
    int fel;                    // FEL
    int lsl;                    // LSL
    int lbl;                    // LBL
    int ldl;                    // LDL
    int lel;                    // LEL
    int gdck_sta;               // GDCK start (relative to first LBL), range -LSL...LBL+LDL+LEL
    int gdck_high;              // how many clocks hold GDCK high
    int gdoe_sta;               // GDOE start (relative to first FSL), range 0...FSL+FBL
    int sdoe_sta;               // SDOE start (relative to first LBL), range -LSL...LBL
    int sdoe_toggle;            // 0=line 1=frame
    int gdsp_offset;            // SPV edge offset from GDCK edge (0-100%)
    int vmirror;                // mirror image vertically
    int powerup_vcom_delay;     // delay after VCOM up to first frame
    int powerdown_delay;        // delay from end of update to start of power-down procedure 
    int powerdown_vcom_delay;   // delay from last frame to VCOM down
    int powerdown_vhigh_delay;  // delay from VCOM down to VHIGH down (-1 = don't turn off VHIGH)
    int powerdown_vdd_delay;    // delay from VHIGH down to VDD down (-1 = don't turn off VDD)
    int vhigh_level;            // +-15 voltage level adjustment
    short *powerup_sequence;    // power-up sequence for VHIGH (set to NULL if don't change)
    short *powerdown_sequence;  // power-down sequence for VHIGH (set to NULL if don't change)
    int temp_offset;            // temperature offset (in ranges, not degrees)
    int hqfilter;               // HQ update filter (0=none 1=outline 2=shadow)
    int vtouch;                 // touchpanel voltage (mV)
    int vbus;                   // data bus voltage (mV)

} param[] = {

// NAME          XRES YRES WF  AMEPD   FSL FBL FDL  FEL LSL LBL LDL LEL GDCK GDCK GDO SDO SDO SPV MIR UP   DOWN DOWN DOWN DOWN VHIGH  UP   DOWN TEMP HQ   V     V
//                         TYP LIST                             4BIT     STA HIGH STA STA TOG OFF ROR VCOM DELY VCOM VHIG VDD  LEVEL  SEQ  SEQ  OFFS FILT TOUCH BUS
                                                                                   
 { "800x600",   800, 600, ANY, NULL,     1,  4, 600,  9,  2,  4, 200, 44,  1, 204,  0, -1,  1, 80,  0,  1,   12,  1,   10, 500, 14250, UP1, DN1,  0,  1,  3300, 3300 },
 { "1024x760",  1024,758, ANY, NULL,     2,  4, 758,  8,  6,  6, 256, 38,  4, 200,  0, -1,  1, 80,  0,  1,   12,  5,   10, 500, 15000, UP1, DN1,  0,  1,  3300, 3300 },
 { "1440x1080", 1440,1080,ANY, NULL,     2,  4, 1080, 5,  8,  8, 360,100,  4, 320,  0, -1,  1, 80,  1,  1,   12,  5,   10, 500, 15000, UP1, DN1,  0,  1,  3300, 3300 },
 { "1448x1072", 1448,1072,ANY, NULL,     2,  4, 1072, 4, 14,  8, 362, 51,100, 280,  0, -1,  1, 80,  0,  1,   12,  5,   10, 500, 15000, UP1, DN1,  0,  1,  3300, 3300 },
 { "1600x1200", 1600,1200,ANY, NULL,     2,  4, 1200,14,  4,  4, 400, 74,  2, 400,  0, -1,  1, 80,  0,  1,   12,  5,   10,  -1, 15000, UP1, DN1,  0,  1,  3300, 3300 },
 { "1872x1404", 1872,1404,ANY, NULL,     1,  4, 1404, 3, 18, 17, 468,  3, 17, 234,  0, -1,  1, 80,  0,  1,   12,  5,   10,  -1, 15000, UP1, DN1,  0,  1,  3300, 3300 },
 { "800x480",   800, 480, ANY, NULL,     2,  4, 480,  4,  4,  2, 200,114,  4, 264,  0, -1,  1, 80,  0,  1,   12,  5,   10, 500, 15000, UP1, DN1,  0,  1,  3300, 3300 },
 { "TC1",       1600,1200,ANY, &am_tc1, NC, NC,  NC, NC, NC, NC,  NC, NC, NC, 375, NC, NC, NC, NC, NC, NC,   NC, NC,   NC,  NC,    NC, UP1, DN1,  0,  0,  3300, 3300 },
 { "SEQ2",      ANY, ANY, 0,   &am_seq2,NC, NC,  NC, NC, NC, NC,  NC, NC, NC,  NC, NC, NC, NC, NC, NC, NC,   NC, NC,   NC,  NC, 14250, UP2, DN2,  0,  1,  3300, 3300 },
 { "SCG",       800, 600, 0,   &am_scg,  1,  4, 600,  8,  2,  4, 200, 44,  1, 204,  0, -1,  1, NC, NC,  1,   12,  5,   10, 500, 15000, UP1, DN1,  0,  1,  3300, 3300 },
 { "SC3",       800, 600, 0,   &am_sc3, NC, NC,  NC, NC, NC, NC,  NC, NC, NC,  NC, NC, NC,  0, NC, NC, NC,   NC, NC,   NC,  NC, 14250, UP2, DN2,  0,  1,  3300, 3300 },
 { "SD1",       800, 600, 1,   &am_sd1,  4,  4, 600, 10,  4,  4, 200, 30,  8, 219, NC, NC,  1, NC, NC, NC,   NC, NC,   NC,  -1, 15000, UP1, DN1,  0,  0,  3300, 3300 },
 { "SCT",       800, 600, 1,   &am_sct,  4,  4, 600, 10,  4,  4, 200, 30,  8, 219, NC, NC,  1, NC, NC, NC,   NC, NC,   NC,  -1, 15000, UP1, DN1,  0,  0,  3300, 3300 },
 { "XD4",       1024,758, 1,   &am_xd4, NC, NC,  NC, NC, NC, NC,  NC, NC, NC,  NC, NC, NC,  0, NC, NC, NC,   NC, NC,   NC,  NC, 15000, UP1, DN1,  0,  0,  1800, 3300 },
 { "XH7",       1024,758, 1,   &am_xh7, NC, NC,  NC, NC, NC, NC,  NC, NC, NC,  NC, NC, NC,  1, NC, NC, NC,   NC, NC,   NC,  -1, 15000, UP1, DN1,  0,  0,  3300, 3300 },
 { "XH8",       1024,758, 1,   &am_xh8, NC, NC,  NC, NC, NC, NC,  NC, NC, NC,  NC, NC, NC,  1, NC, NC, NC,   NC, NC,   NC,  -1, 15000, UP1, DN1,  0,  0,  3300, 3300 },
 { "XCD",       1024,758, 1,   &am_xcd,  2,  4, 758,  5,  6,  6, 256, 38,  4, 200,  0, -1,  1, 80,  0,  1,   12,  5,   10,  -1, 15000, UP1, DN1,  0,  0,  3300, 3300 },
 { "KH6",       1448,1072,ANY, &am_kh6,  2,  4, 1072, 4, 14,  8, 362, 51,100, 280, NC, NC, NC, NC, NC, NC,   NC, NC,   NC,  NC, 15000, UP1, DN1,  0,  1,  3300, 1800 },
};

#define SET_PARAM(var, x) if ((x) != NC) var = (x)

static inline int check_amepd(amepd_list *parts, char *name, int amepd)
{
	char *pn = parts->names;
	int *pv = parts->types, len;

	if (name && *name && pn && *pn) {
		if (strstr(name, pn)) {
			INF("found AMEPD: '%s'\n", pn);
			return 1;
		}
	}
	while (amepd && pv && *pv) {
		if (amepd == *pv) {
			return 1;
			INF("found AMEPD: 0x%x\n", *pv);
		}
		pv++;
	}
	return 0;
}

int epdc_set_display_parameters(int xres, int yres, int wftype, char *name, int amepd, int bus_width)
{
    int i, ret=-1;
    int ldl8 = 0;

    DBG("epdc_set_display_parameters(%dx%d type=%x name='%s' amepd=%x bus=%d)\n",
        xres, yres, wftype, name, amepd, bus_width ? 16 : 8);
    wfdata.amepd_applied[0] = 0;
    for (i=0; i<sizeof(param)/sizeof(struct display_param); i++) {
        if (param[i].xres != ANY && param[i].xres != xres) continue;
        if (param[i].yres != ANY && param[i].yres != yres) continue;
        if (param[i].wftype != ANY) {
		if (param[i].wftype == 0 && !(wftype < 0xaa)) continue;
		if (param[i].wftype == 1 && !(wftype == 0x3c || wftype == 0x4b || wftype == 0x4d || wftype == 0x4f)) continue;
	}
        if (param[i].parts != NULL && ! check_amepd(param[i].parts, name, amepd)) continue;
        SET_PARAM(eink_param.timing.fsl, param[i].fsl);
        SET_PARAM(eink_param.timing.fbl, param[i].fbl);
        SET_PARAM(eink_param.timing.fdl, param[i].fdl);
        SET_PARAM(eink_param.timing.fel, param[i].fel);
        SET_PARAM(eink_param.timing.lsl, param[i].lsl);
        SET_PARAM(eink_param.timing.lbl, param[i].lbl);
        SET_PARAM(ldl8, param[i].ldl);
        SET_PARAM(eink_param.timing.lel, param[i].lel);
        SET_PARAM(eink_param.timing.gdck_sta, param[i].gdck_sta);
        SET_PARAM(eink_param.timing.gdck_high, param[i].gdck_high);
        SET_PARAM(eink_param.timing.gdoe_sta, param[i].gdoe_sta);
        SET_PARAM(eink_param.timing.sdoe_sta, param[i].sdoe_sta);
        SET_PARAM(eink_param.timing.sdoe_toggle, param[i].sdoe_toggle);
        SET_PARAM(eink_param.timing.gdsp_offset, param[i].gdsp_offset);
        //SET_PARAM(eink_param.timing.xdelay, param[i].powerdown_delay);
        //SET_PARAM(eink_param.vmirror, param[i].vmirror);
        //SET_PARAM(eink_param.hqfilter, param[i].hqfilter);
        SET_PARAM(eink_param.powerup_vcom_delay, param[i].powerup_vcom_delay);
        SET_PARAM(eink_param.powerdown_vcom_delay, param[i].powerdown_vcom_delay);
        SET_PARAM(eink_param.powerdown_vhigh_delay, param[i].powerdown_vhigh_delay);
        SET_PARAM(eink_param.powerdown_vdd_delay, param[i].powerdown_vdd_delay);
        SET_PARAM(eink_param.vhigh_level, param[i].vhigh_level);
        //SET_PARAM(eink_param.temp_offset, param[i].temp_offset);
        SET_PARAM(eink_param.vtouch, param[i].vtouch);
        SET_PARAM(eink_param.vbus, param[i].vbus);
        if (param[i].powerup_sequence)
            memcpy(eink_param.powerup_sequence, param[i].powerup_sequence, 16 * sizeof(short));
        if (param[i].powerdown_sequence)
            memcpy(eink_param.powerdown_sequence, param[i].powerdown_sequence, 16 * sizeof(short));
        sprintf(wfdata.amepd_applied+strlen(wfdata.amepd_applied), "%s ", param[i].name);
        ret = 0;
    }
    eink_param.timing.ldl = bus_width ? ldl8 / 2 : ldl8;
		eink_param.timing.vblank = 3;

    DBG("Applied params: %s\n", wfdata.amepd_applied);
    DBG("  FSL:%d FBL:%d FDL:%d FEL:%d\n", eink_param.timing.fsl,eink_param.timing.fbl,eink_param.timing.fdl,eink_param.timing.fel);
    DBG("  LSL:%d LBL:%d LDL:%d LEL:%d\n", eink_param.timing.lsl,eink_param.timing.lbl,eink_param.timing.ldl,eink_param.timing.lel);
    DBG("  GDCKSTA:%d GDCKHIGH:%d\n", eink_param.timing.gdck_sta,eink_param.timing.gdck_high);
    DBG("  GDOESTA:%d SDOESTA:%d TOG:%d\n", eink_param.timing.gdoe_sta,eink_param.timing.sdoe_sta,eink_param.timing.sdoe_toggle);
    return ret;
}

