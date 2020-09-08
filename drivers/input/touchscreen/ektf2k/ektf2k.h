#ifndef _LINUX_ELAN_KTF2K_H
#define _LINUX_ELAN_KTF2K_H
/*
#if (defined(CONFIG_7564C_V10))
#define ELAN_X_MAX 	2112	
#define ELAN_Y_MAX	1600	
#else
#define ELAN_X_MAX 	2624  	
#define ELAN_Y_MAX	1644	
#endif
*/





/* project SMB-b7006 use this function */
//#define   MODE_VALID_CHECK_FUNCTION

/* 触摸屏的名字 */
#define ELAN_KTF2K_NAME "elan-ktf2k"

/* 板级文件使用的平台数据 */
struct elan_ktf2k_i2c_platform_data {
	uint16_t version;
	int abs_x_min;
	int abs_x_max;
	int abs_y_min;
	int abs_y_max;
	int intr_gpio;
	int reset_gpio;
        int mode_check_gpio;
	int (*power)(int on);
};

#endif /* _LINUX_ELAN_KTF2K_H */
