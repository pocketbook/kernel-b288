/*
* Simple driver for Texas Instruments LM3630 LED Flash driver chip
* Copyright (C) 2012 Texas Instruments
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/

#ifndef __LINUX_LM3630_H
#define __LINUX_LM3630_H

#define LM3630_NAME "lm3630_bl"

enum lm3630_pwm_ctrl {
	PWM_CTRL_DISABLE = 0,
	PWM_CTRL_BANK_A,
	PWM_CTRL_BANK_B,
	PWM_CTRL_BANK_ALL,
};

enum lm3630_pwm_active {
	PWM_ACTIVE_HIGH = 0,
	PWM_ACTIVE_LOW,
};

#define BANK_A_CTRL_MASK 0x15
#define BANK_B_CTRL_MASK 0xA

enum lm3630_bank_a_ctrl {
	BANK_A_CTRL_DISABLE = 0x0,
	BANK_A_CTRL_LED1 = 0x4,
	BANK_A_CTRL_LED1_LINEAR = BANK_A_CTRL_LED1 | (1<<4),
	BANK_A_CTRL_LED2 = 0x2,
	BANK_A_CTRL_LED2_LINEAR = BANK_A_CTRL_LED2 | (1<<3),
	BANK_A_CTRL_ALL = 0x5,
};

enum lm3630_bank_b_ctrl {
	BANK_B_CTRL_DISABLE = 0,
	BANK_B_CTRL_LED2 = 0x2,
	BANK_B_CTRL_LED2_LINEAR = BANK_B_CTRL_LED2 | (1<<3),
};

struct lm3630_platform_data {

	/* maximum brightness */
	int max_brt_led1;
	int max_brt_led2;

	/* initial on brightness */
	int init_brt_led1;
	int init_brt_led2;

	/* max led current */
	int max_current_led1;
	int max_current_led2;

	enum lm3630_pwm_ctrl pwm_ctrl;
	enum lm3630_pwm_active pwm_active;
	enum lm3630_bank_a_ctrl bank_a_ctrl;
	enum lm3630_bank_b_ctrl bank_b_ctrl;
	unsigned int pwm_period;
	void (*pwm_set_intensity) (int brightness, int max_brightness);
};

#endif /* __LINUX_LM3630_H */
