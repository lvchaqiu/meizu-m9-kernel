/*
 * linux/drivers/power/m9w_bat.h
 *
 * Battery measurement code for S3C6410 platform.
 *
 * Copyright (C) 2009 Samsung Electronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _M9W_BATTERY_H_
#define _M9W_BATTERY_H_

#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/mfd/ltc3577.h>
#include <linux/android_alarm.h>

#define	VDD_ADC_REF		(int)(800*(80.6+226)/80.6)	//mV

/*
 * Spica Rev00 board ADC channel
 */
typedef enum s3c_adc_channel {
	S3C_ADC_CHARG = 0,
	S3C_ADC_TEMPERATURE,
	S3C_ADC_VOLTAGE,
	S3C_ADC_EAR_MIC = 4,
	ENDOFADC
} adc_channel_type;

typedef enum {
	OTG_CUR_10mA,
	OTG_CUR_100mA = 100000,
	OTG_CUR_500mA = 500000,
	OTG_CUR_1000mA = 1000000,
} m9w_otg_cur;

struct battery_info {
	u32 batt_id;		    		/* Battery ID from ADC */
	u32 batt_vol;		    		/* Battery voltage from ADC */
	u32 batt_vol_adc;	    		/* Battery ADC value */
	u32 batt_vol_adc_cal;		/* Battery ADC value (calibrated)*/
	s32 batt_temp;		    	/* Battery Temperature (C) from ADC */
	u32 batt_temp_adc;	    	/* Battery Temperature ADC value */
	u32 batt_temp_adc_cal;	/* Battery Temperature ADC value (calibrated) */
	u32 batt_current;	    		/* Battery current from ADC */
	u32 batt_chr_cur_adc;		/* Battert charging current from ADC*/
	u32 batt_charging_current;	/* Battery charging current value */
	u32 batt_charging_time;	/* Battery charging time, unit: s */
	u32 level;		        	/* formula */
	u32 charging_enabled;		/* 0: Disable, 1: Enable */
	u32 batt_health;	    		/* Battery Health (Authority) */
	u32 batt_is_full;       		/* false : Not full; true: Full */
	u32 charge_health;	    	/* Charger Health  */
	u32 charging_status;
};

struct m9w_bat_info {
	struct device *dev;
	struct ltc3577_dev	*iodev;
	struct ltc3577_charger_data *pdata;
	int present;
	unsigned long polling_interval;
	ktime_t                 last_poll;

	struct battery_info bat_info;
	struct mutex		mutex;

	struct wake_lock usb_wake_lock;
	struct wake_lock vbus_wake_lock;
	struct wake_lock bat_wake_lock;

	enum cable_type_t	cable_status;
	bool	charging;
	int	timestamp;
	int	slow_poll;

	struct delayed_work m9w_low_work;
	struct delayed_work m9w_bat_work;
	struct delayed_work m9w_usb_work;

	struct alarm		alarm;
	struct workqueue_struct *monitor_wqueue;

	struct power_supply	psy_bat;
	struct power_supply	psy_usb;
	struct power_supply	psy_ac;

	int debug;

	struct regulator *charger_cur;

	unsigned int usb_irq_type;
	bool	usb_pin_low;
};

extern int s3c_adc_get_adc_data(int);
extern atomic_t attatched_usb;

extern int  register_m9w_bat_notifier(struct notifier_block *nb);
extern void unregister_m9w_bat_notifier(struct notifier_block *nb);
#ifdef CONFIG_EARLYSUSPEND
extern suspend_state_t get_suspend_state(void);
#endif

#endif /*  _M9W_BATTERY_H_ */
