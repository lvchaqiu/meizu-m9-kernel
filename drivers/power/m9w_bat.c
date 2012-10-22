/*
 * linux/drivers/power/m9w_bat.c
 *
 * Battery measurement code for meizu m9w .
 *
 * based on s5pc110_battery.c
 * 
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: 	lvcha qiu	<lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 
 *
 * Revision History
 *
 * Inital code : Jan 10 , 2011 : lvcha@meizu.com
 *
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/mfd/ltc3577-private.h>
#include <linux/m9w_bat.h>

#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <plat/pm.h>

#ifdef CONFIG_BQ27541	
#include "m9w_bq27541.h"
#endif

#define FAST_POLL			(1 * 60)
#define SLOW_POLL			(10 * 60)
#define	BATTERY_OVERHEAT_TMP	(600)	
#define	BATTERY_COLD_TMP		(-200)	

#define CHARGE_OVERHEAT_TMP	(400)		/* modified by lvcha; default:450 */
#define	CHARGE_COLD_TMP		(000)	

#define	SHUTDOWN_LEVEL	3

extern atomic_t attatched_usb;
static struct m9w_bat_info *g_m9w_bat;
static atomic_t usb_bind;
static RAW_NOTIFIER_HEAD(m9w_bat_chain);
static int m9w_bat_notify(unsigned long val, void *v);

static enum power_supply_property m9w_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property m9w_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

static int m9w_bat_get_property(struct power_supply *bat_ps, 
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct m9w_bat_info *m9w_bat =
		container_of(bat_ps, struct m9w_bat_info, psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = m9w_bat->bat_info.charging_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = m9w_bat->bat_info.batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = m9w_bat->present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (m9w_bat->bat_info.batt_is_full)
			val->intval = 100;
		else
			val->intval = m9w_bat->bat_info.level;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = m9w_bat->bat_info.batt_temp;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = m9w_bat->bat_info.batt_current;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = m9w_bat->bat_info.batt_vol;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = 10;
#ifdef  CONFIG_BQ27541
		if( m9w_bat->bat_info.batt_id == BQ27541_ID)
			val->intval = bq27541_get_CycleCount();
#endif
		break;
	default:
		return -ENODEV;
	}
	return 0;
}

static int m9w_usb_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct m9w_bat_info *chg = container_of(ps, struct m9w_bat_info, psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = (chg->cable_status == CABLE_TYPE_USB);

	return 0;
}

static int m9w_ac_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct m9w_bat_info *chg = container_of(ps, struct m9w_bat_info, psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (chg->cable_status == CABLE_TYPE_AC);

	return 0;
}

/* Prototypes */
static ssize_t m9w_bat_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf);
static ssize_t m9w_bat_store(struct device *dev, 
			     struct device_attribute *attr,
			     const char *buf, size_t count);

#define SEC_BATTERY_ATTR(_name)\
{\
    .attr = { .name = #_name, .mode = S_IRUGO | S_IWUGO},\
    .show = m9w_bat_show_property,\
    .store = m9w_bat_store,\
}
#define SEC_BATTERY_ATTR_R(_name)\
{\
    .attr = { .name = #_name, .mode = S_IRUGO},\
    .show = m9w_bat_show_property,\
    .store = m9w_bat_store,\
}

static struct device_attribute m9w_battery_attrs[] = {
    SEC_BATTERY_ATTR(batt_vol),
    SEC_BATTERY_ATTR(batt_vol_adc),
    SEC_BATTERY_ATTR(batt_vol_adc_cal),
    SEC_BATTERY_ATTR(batt_temp),
    SEC_BATTERY_ATTR(batt_temp_adc),
    SEC_BATTERY_ATTR(batt_temp_adc_cal),
    SEC_BATTERY_ATTR(batt_chr_cur_adc),
    SEC_BATTERY_ATTR(batt_charging_current),
    SEC_BATTERY_ATTR(batt_charging_time),
#ifdef CONFIG_BQ27541	
    SEC_BATTERY_ATTR_R(RemainTime),
    SEC_BATTERY_ATTR_R(Flags),
    SEC_BATTERY_ATTR_R(NominalAvailableCapacity),
    SEC_BATTERY_ATTR_R(FullAvailableCapacity),
    SEC_BATTERY_ATTR_R(RemainingCapacity),
    SEC_BATTERY_ATTR_R(FullChargeCapacity),
    SEC_BATTERY_ATTR_R(AverageCurrent),
    SEC_BATTERY_ATTR_R(TimeToEmpty),
    SEC_BATTERY_ATTR_R(TimeToFull),
    SEC_BATTERY_ATTR_R(StandbyCurrent),
    SEC_BATTERY_ATTR_R(StandbyTimeToEmpty),
    SEC_BATTERY_ATTR_R(MaxLoadCurrent),
    SEC_BATTERY_ATTR_R(MaxLoadTimeToEmpty),
    SEC_BATTERY_ATTR_R(AvailableEnergy),
    SEC_BATTERY_ATTR_R(AveragePower),
    SEC_BATTERY_ATTR_R(TTEatConstantPower),
    SEC_BATTERY_ATTR_R(CycleCount),
    SEC_BATTERY_ATTR_R(StateOfCharge),
#endif
    SEC_BATTERY_ATTR(debug),
    SEC_BATTERY_ATTR(usb_state),
};

enum {
    BATT_VOL = 0,
    BATT_VOL_ADC,
    BATT_VOL_ADC_CAL,
    BATT_TEMP,
    BATT_TEMP_ADC,
    BATT_TEMP_ADC_CAL,
    BATT_CHR_CUR_ADC,
    BATT_CHR_CURRENT,
    BATT_CHR_TIME,
#ifdef CONFIG_BQ27541	
    BATT_REMAINTIME,
    BATT_FLAGS,
    BATT_NOMINALAVAILABLECAPACITY,
    BATT_FULLAVAILABLECAPACITY,
    BATT_REMAININGCAPACITY,
    BATT_FULLCHARGECAPACITY,
    BATT_AVERAGECURRENT,
    BATT_TIMETOEMPTY,
    BATT_TIMETOFULL,
    BATT_STANDBYCURRENT,
    BATT_STANDBYTIMETOEMPTY,
    BATT_MAXLOADCURRENT,
    BATT_MAXLOADTIMETOEMPTY,
    BATT_AVAILABLEENERGY,
    BATT_AVERAGEPOWER,
    BATT_TTEATCONSTANTPOWER,
    BATT_CYCLECOUNT,
    BATT_STATEOFCHARGE,
#endif
    BATT_DEBUG,
    USB_STATE,
};

static int m9w_bat_create_attrs(struct device * dev)
{
    int i, rc;

    for (i = 0; i < ARRAY_SIZE(m9w_battery_attrs); i++) {
        rc = device_create_file(dev, &m9w_battery_attrs[i]);
        if (rc)
        goto m9w_attrs_failed;
    }
    goto succeed;

m9w_attrs_failed:
    while (i--)
        device_remove_file(dev, &m9w_battery_attrs[i]);
succeed:        
    return rc;
}

static void m9w_bat_destroy_atts(struct device * dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(m9w_battery_attrs); i++)
		device_remove_file(dev, &m9w_battery_attrs[i]);
}

static ssize_t m9w_bat_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
	int i = 0;
	const ptrdiff_t off = attr - m9w_battery_attrs;
	struct m9w_bat_info *m9w_bat_info = dev_get_drvdata(dev->parent);

#ifdef CONFIG_BQ27541		
	if( m9w_bat_info->bat_info.batt_id != BQ27541_ID && off >= BATT_REMAINTIME) {
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",0);
		return i;
	}
#endif

	switch (off) {
	case BATT_VOL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->bat_info.batt_vol);
		break;
	case BATT_VOL_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->bat_info.batt_vol_adc);
		break;
	case BATT_VOL_ADC_CAL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->bat_info.batt_vol_adc_cal);
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->bat_info.batt_temp);
		break;
	case BATT_TEMP_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->bat_info.batt_temp_adc);
		break;	
	case BATT_TEMP_ADC_CAL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->bat_info.batt_temp_adc_cal);
		break;
	case BATT_CHR_CUR_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->bat_info.batt_chr_cur_adc);
		break;
	case BATT_CHR_CURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->bat_info.batt_charging_current);
		break;
	case BATT_CHR_TIME:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d S\n",m9w_bat_info->bat_info.batt_charging_time);
		break;

#ifdef CONFIG_BQ27541	
	case BATT_REMAINTIME:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d minutes\n",bq27541_get_AtRateTimeToEmpty());
		break;

	case BATT_FLAGS:
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%.4X\n",bq27541_get_Flags());
		break;

	case BATT_NOMINALAVAILABLECAPACITY:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mAH\n",bq27541_get_NominalAvailableCapacity());
		break;

	case BATT_FULLAVAILABLECAPACITY:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mAH\n",bq27541_get_FullAvailableCapacity());
		break;

	case BATT_REMAININGCAPACITY:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mAH\n",bq27541_get_RemainingCapacity());
		break;

	case BATT_FULLCHARGECAPACITY:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mAH\n",bq27541_get_FullChargeCapacity());
		break;

	case BATT_AVERAGECURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mA\n",bq27541_get_AverageCurrent());
		break;

	case BATT_TIMETOEMPTY:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d minutes\n",bq27541_get_TimeToEmpty());
		break;

	case BATT_TIMETOFULL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d minutes\n",bq27541_get_TimeToFull());
		break;

	case BATT_STANDBYCURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mA\n",bq27541_get_StandbyCurrent());
		break;

	case BATT_STANDBYTIMETOEMPTY:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d S\n",bq27541_get_StandbyTimeToEmpty());
		break;

	case BATT_MAXLOADCURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mA\n",bq27541_get_MaxLoadCurrent());
		break;

	case BATT_MAXLOADTIMETOEMPTY:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d S\n",bq27541_get_MaxLoadTimeToEmpty());
		break;

	case BATT_AVAILABLEENERGY:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mAH\n",bq27541_get_AvailableEnergy());
		break;

	case BATT_AVERAGEPOWER:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d mW\n",bq27541_get_AveragePower());
		break;

	case BATT_TTEATCONSTANTPOWER:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d minutes\n",bq27541_get_TTEatConstantPower());
		break;

	case BATT_CYCLECOUNT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d \n",bq27541_get_CycleCount());
		break;

	case BATT_STATEOFCHARGE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d%% \n",bq27541_get_StateOfCharge());
		break;		
#endif		
	case BATT_DEBUG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",m9w_bat_info->debug);
		break;
	case USB_STATE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",atomic_read(&attatched_usb));
		break;
	default:
		i = -EINVAL;
	}       

	return i;
}

static ssize_t m9w_bat_store(struct device *dev, 
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int x = 0;
	int ret = 0;
	const ptrdiff_t off = attr - m9w_battery_attrs;
	struct m9w_bat_info *m9w_bat_info = dev_get_drvdata(dev->parent);

	switch (off) {
	case BATT_VOL_ADC_CAL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			m9w_bat_info->bat_info.batt_vol_adc_cal = x;
			ret = count;
		}
		dev_dbg(dev, "%s : batt_vol_adc_cal = %d\n", __func__, x);
		break;
	case BATT_TEMP_ADC_CAL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			m9w_bat_info->bat_info.batt_temp_adc_cal = x;
			ret = count;
		}
		dev_dbg(dev, "%s : batt_temp_adc_cal = %d\n", __func__, x);
		break;
	case BATT_CHR_TIME:
		if (sscanf(buf, "%d\n", &x) == 1) {
			m9w_bat_info->bat_info.batt_charging_time = x;
			ret = count;
		}
		dev_dbg(dev, "%s : batt_charging_time = %d\n", __func__, x);
		break;		
		
#ifdef CONFIG_BQ27541	
	case BATT_REMAINTIME:
		break;
#endif
	case BATT_DEBUG:
		if (sscanf(buf, "%d\n", &x) == 1) {
			m9w_bat_info->debug = (!!x);
			ret = count;
		}
		dev_dbg(dev, "%s : debu = %d\n", __func__, x);
		break;		

	default:
	ret = -EINVAL;
	}       

	return ret;
}
static int m9w_get_bat_temp(struct m9w_bat_info *m9w_bat)
{
	int ret;
	int temp;

	ret = ltc3577_read_reg(m9w_bat->iodev, 0);
	if (ret < 0) // battery fault
		return ret;

#ifdef CONFIG_BQ27541
	if( m9w_bat->bat_info.batt_id == BQ27541_ID ) {
		temp = bq27541_get_Temperature();
		if (m9w_bat->bat_info.batt_temp != temp)
			m9w_bat_notify(POWER_BAT_TMP, m9w_bat);
	}
	else
#endif		
	temp = 260;	

	return temp;
}

static unsigned int m9w_get_bat_vol(struct m9w_bat_info *m9w_bat)
{	
#ifdef CONFIG_BQ27541
	if( m9w_bat->bat_info.batt_id == BQ27541_ID )
		m9w_bat->bat_info.batt_vol = bq27541_get_Voltage();
	else
#endif		
	m9w_bat->bat_info.batt_vol = 3700;

	return m9w_bat->bat_info.batt_vol*1000;
}

static unsigned int m9w_get_bat_cur(struct m9w_bat_info *m9w_bat)
{	
#ifdef CONFIG_BQ27541
	if( m9w_bat->bat_info.batt_id == BQ27541_ID )
		m9w_bat->bat_info.batt_current = bq27541_get_AverageCurrent();
	else
#endif		
	m9w_bat->bat_info.batt_current = 250;

	return m9w_bat->bat_info.batt_current;

}

static unsigned int m9w_get_bat_level(struct m9w_bat_info *m9w_bat)
{
	int report_level;

#ifdef CONFIG_BQ27541
	if( m9w_bat->bat_info.batt_id == BQ27541_ID ) {
		report_level = bq27541_get_StateOfCharge();
		
		if(report_level < 0 || report_level > 100)	// When the level is not in 0 ~ 100 
			report_level = m9w_bat->bat_info.level;

		if (m9w_bat->bat_info.level != report_level)
			m9w_bat_notify(POWER_LEVEL_CHANGE, m9w_bat);
	} else
#endif
	report_level = 15;

	return report_level;
}

static int m9w_bat_status_update(struct m9w_bat_info *m9w_bat)
{
	int old_level, old_temp;
	unsigned int batt_health = m9w_bat->bat_info.batt_health;
	unsigned int charger_type = m9w_bat->cable_status;
	int charge_cur = -1;

	old_temp = m9w_bat->bat_info.batt_temp;
	old_level = m9w_bat->bat_info.level; 

	m9w_bat->bat_info.batt_vol = m9w_get_bat_vol(m9w_bat);
	m9w_bat->bat_info.batt_temp = m9w_get_bat_temp(m9w_bat);
	m9w_bat->bat_info.level = m9w_get_bat_level(m9w_bat);	
	m9w_bat->bat_info.batt_current = m9w_get_bat_cur(m9w_bat);

	if(m9w_bat->bat_info.level <= SHUTDOWN_LEVEL)
		m9w_bat->bat_info.level = 0;

	if( charger_type == CABLE_TYPE_USB || charger_type == CABLE_TYPE_AC) {
		int charge_health = m9w_bat->bat_info.charge_health;
		if( m9w_bat->bat_info.batt_temp > CHARGE_OVERHEAT_TMP ) {
			if( m9w_bat->bat_info.charge_health == POWER_SUPPLY_HEALTH_GOOD ) {
				m9w_bat->bat_info.charge_health = POWER_SUPPLY_HEALTH_OVERHEAT;
				if (charger_type == CABLE_TYPE_AC)
					charge_cur = OTG_CUR_500mA;	//modified by lvcha default:OTG_CUR_100mA
			}
		} else if( m9w_bat->bat_info.batt_temp < CHARGE_COLD_TMP) {
			if( m9w_bat->bat_info.charge_health == POWER_SUPPLY_HEALTH_GOOD ) {
				m9w_bat->bat_info.charge_health = POWER_SUPPLY_HEALTH_COLD;
				charge_cur = OTG_CUR_100mA;
			}
		} else {
			if( m9w_bat->bat_info.charge_health != POWER_SUPPLY_HEALTH_GOOD ) {
				m9w_bat->bat_info.charge_health = POWER_SUPPLY_HEALTH_GOOD;
				/* resume to normal charge current */
				if(charger_type == CABLE_TYPE_USB)
					charge_cur = OTG_CUR_500mA;
				else
					charge_cur = OTG_CUR_1000mA;	
			}				
		}

		/* change charge current because of abnormal state */
		if (charge_cur > 0)
			regulator_set_current_limit(m9w_bat->charger_cur, charge_cur, charge_cur);
					
		if(charge_health != m9w_bat->bat_info.charge_health)
			pr_info("Charger changed 0x%X, batt_temp = %d, charger_type= %d \n", 
				m9w_bat->bat_info.charge_health,m9w_bat->bat_info.batt_temp,charger_type);
	} else {
		m9w_bat->bat_info.charge_health = POWER_SUPPLY_HEALTH_GOOD;

		if( m9w_bat->bat_info.batt_temp > BATTERY_OVERHEAT_TMP ) {
			if( batt_health == POWER_SUPPLY_HEALTH_GOOD)
				batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		} else if( m9w_bat->bat_info.batt_temp < BATTERY_COLD_TMP) {
			if( batt_health == POWER_SUPPLY_HEALTH_GOOD)
				batt_health = POWER_SUPPLY_HEALTH_COLD;		
		} else {
			if( batt_health == POWER_SUPPLY_HEALTH_OVERHEAT ||
				batt_health == POWER_SUPPLY_HEALTH_COLD)
				batt_health = POWER_SUPPLY_HEALTH_GOOD;
		}

		if( m9w_bat->bat_info.batt_health != batt_health ) {
			m9w_bat->bat_info.batt_health = batt_health;
			pr_info("battery health changed 0x%X, batt_temp = %d, charger_type= %d\n", 
				batt_health,m9w_bat->bat_info.batt_temp,charger_type);
		}
	}

	return 0;
}

static int m9w_charging_status_update(struct m9w_bat_info *m9w_bat)
{
	int ret = 0;
	struct battery_info *bat_info = &m9w_bat->bat_info;

	ret = ltc3577_read_reg(m9w_bat->iodev, 0);
	if (ret < 0 || ret & LTC3577_BAT_MASK) // battery fault
		goto err;

	if (m9w_bat->cable_status != CABLE_TYPE_NONE) {
		struct timeval delta_tm;
		static struct timeval first_tm;

		if (ret & LTC3577_CHARGING) {	/* charging */
			if (bat_info->charging_status != POWER_SUPPLY_STATUS_CHARGING)
				do_gettimeofday	(&first_tm);
			else
				do_gettimeofday	(&delta_tm);
			bat_info->batt_charging_time = delta_tm.tv_sec - first_tm.tv_sec;
			m9w_bat->charging = true;
			bat_info->charging_status = POWER_SUPPLY_STATUS_CHARGING;
		} else {
#ifdef CONFIG_BQ27541
			if(bat_info->level>95 && (0x0200&bq27541_get_Flags())) {
				bat_info->batt_is_full = true;
				bat_info->charging_status = POWER_SUPPLY_STATUS_FULL;
				if (m9w_bat->debug)
					pr_info("%s : the full charge detection\n", __func__);
			}
#else
			if(bat_info->charging_status == POWER_SUPPLY_STATUS_CHARGING) {				
				bat_info->batt_is_full = true;
				bat_info->charging_status = POWER_SUPPLY_STATUS_FULL;				
			}
#endif
		}
	} else  {
		m9w_bat->charging = false;
		bat_info->charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
		bat_info->batt_is_full = false;
		bat_info->batt_charging_time = 0;
	}

#ifndef CONFIG_ALLOW_M9W_USB_SLP
	if (m9w_bat->cable_status != CABLE_TYPE_NONE)
		wake_lock(&m9w_bat->vbus_wake_lock);
	else
#endif
	wake_lock_timeout(&m9w_bat->vbus_wake_lock, HZ/5);

	return 0;

err:
	pr_err("%s: excuted failed\n", __func__);
	bat_info->charging_status = POWER_SUPPLY_STATUS_UNKNOWN;
	return ret;
}

static void m9w_program_alarm(struct m9w_bat_info *m9w_bat, int seconds)
{
	ktime_t low_interval = ktime_set(seconds - 10, 0);
	ktime_t slack = ktime_set(20, 0);
	ktime_t next;

	next = ktime_add(m9w_bat->last_poll, low_interval);
	alarm_start_range(&m9w_bat->alarm, next, ktime_add(next, slack));
}

static void m9w_bat_work(struct work_struct *work)
{
	struct m9w_bat_info *m9w_bat = 
		container_of(work, struct m9w_bat_info, m9w_bat_work.work);
	struct timespec ts;
	unsigned long flags;

	mutex_lock(&m9w_bat->mutex);

	m9w_bat_status_update(m9w_bat);
	m9w_charging_status_update(m9w_bat);

	mutex_unlock(&m9w_bat->mutex);

	power_supply_changed(&m9w_bat->psy_bat);

	m9w_bat->last_poll = alarm_get_elapsed_realtime();
	ts = ktime_to_timespec(m9w_bat->last_poll);
	m9w_bat->timestamp = ts.tv_sec;

	/* prevent suspend before starting the alarm */
	local_irq_save(flags);
	wake_unlock(&m9w_bat->bat_wake_lock);
	m9w_program_alarm(m9w_bat, FAST_POLL);
	local_irq_restore(flags);
}

static void m9w_usb_work_func(struct work_struct *work)
{
#define  RETRY_CNT 5
	struct m9w_bat_info *m9w_bat =
		container_of(work, struct m9w_bat_info, m9w_usb_work.work);
	struct ltc3577_charger_data *pdata = m9w_bat->pdata;
	enum cable_type_t	cable_status = CABLE_TYPE_NONE;
	int usb_supply_cur = -1;
	u32 charge_health = m9w_bat->bat_info.charge_health;

	wake_lock(&m9w_bat->usb_wake_lock);

	if (m9w_bat->usb_pin_low) {
		if (atomic_read(&attatched_usb) == true) {
			cable_status = CABLE_TYPE_USB;
		} else {
			cable_status = CABLE_TYPE_AC;
		}
	}

	if (m9w_bat->debug)
		pr_info("%s: cable_status = %d",__func__, cable_status);

	if (cable_status == CABLE_TYPE_NONE) {
		if (pdata && pdata->usb_attach && !pdata->usb_attach(false))
			atomic_set(&attatched_usb, false);
	}

	if (m9w_bat->cable_status != cable_status) {
		power_supply_changed(&m9w_bat->psy_ac);
		power_supply_changed(&m9w_bat->psy_usb);

		m9w_bat_notify(POWER_STATE_CHANGE, m9w_bat);

		switch (cable_status) {
			case CABLE_TYPE_USB:
				usb_supply_cur = (charge_health == POWER_SUPPLY_HEALTH_GOOD) ?
					OTG_CUR_500mA : OTG_CUR_100mA;
				break;
			case CABLE_TYPE_AC:
				usb_supply_cur = (charge_health == POWER_SUPPLY_HEALTH_GOOD) ?
					OTG_CUR_1000mA : OTG_CUR_100mA;
				break;
			default:	/* CABLE_TYPE_NONE */
				usb_supply_cur = OTG_CUR_10mA;
				break;	
		}
		if (usb_supply_cur > 0) {
			regulator_set_current_limit(m9w_bat->charger_cur,
				usb_supply_cur, usb_supply_cur);
		}

		m9w_bat->cable_status = cable_status;

		queue_delayed_work(m9w_bat->monitor_wqueue,
					&m9w_bat->m9w_bat_work, 0);
	}

	wake_unlock(&m9w_bat->usb_wake_lock);
}

void check_usb(void)
{
	struct m9w_bat_info *m9w_bat = g_m9w_bat;
	struct ltc3577_charger_data *pdata = m9w_bat->pdata;
	unsigned long timeout = 0;

	printk("============%s call\n", __func__);
	m9w_bat->usb_pin_low = !gpio_get_value(USB_INT);
	if (m9w_bat->usb_pin_low) {
		if (pdata && pdata->usb_attach && !pdata->usb_attach(true)) {
			printk("usb_attach\n");
			timeout = 3*HZ;
		}
	} else {
		timeout = HZ/2;
	}
	
	if (timeout) {
		wake_lock(&m9w_bat->usb_wake_lock);

		if (work_pending(&m9w_bat->m9w_usb_work.work)) {
			cancel_delayed_work(&m9w_bat->m9w_usb_work);
		}

		queue_delayed_work(m9w_bat->monitor_wqueue,
						&m9w_bat->m9w_usb_work, timeout);
		atomic_set(&usb_bind, 1);
	}
}
EXPORT_SYMBOL(check_usb);

static irqreturn_t usb_int_work_func(int irq, void *dev_id)
{
	struct m9w_bat_info *m9w_bat = dev_id;
	struct ltc3577_charger_data *pdata = m9w_bat->pdata;
	unsigned long timeout = 0;

	if(atomic_read(&usb_bind) == 0) {
		pr_info("+++++++++++++++usb not bind, can not handle irq\n");
		return IRQ_HANDLED;
	}
	m9w_bat->usb_pin_low = !gpio_get_value(USB_INT);

	if (m9w_bat->debug)
		pr_info("%s: usb_pin_low = %s\n", __func__, 
			m9w_bat->usb_pin_low ? "true" : "false");

	if (m9w_bat->usb_pin_low) {
		if (pdata && pdata->usb_attach && !pdata->usb_attach(true)) {
			timeout = 3*HZ;
		}
	} else {
#ifdef CONFIG_EARLYSUSPEND
		if (get_suspend_state() != PM_SUSPEND_MEM)
			timeout = HZ/5;
		else
#endif
			timeout = HZ/2;
	}
	
	if (timeout) {
		wake_lock(&m9w_bat->usb_wake_lock);

		if (work_pending(&m9w_bat->m9w_usb_work.work)) {
			cancel_delayed_work(&m9w_bat->m9w_usb_work);
		}

		queue_delayed_work(m9w_bat->monitor_wqueue,
						&m9w_bat->m9w_usb_work, timeout);

		if (m9w_bat->debug) {
			pr_info("%s: schedule_delayed_work after %d ms",
				__func__, jiffies_to_msecs(timeout));
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t usb_int_irq_handle(int irq, void *dev_id)
{
	struct m9w_bat_info *m9w_bat = dev_id;

	if (m9w_bat->usb_irq_type & IRQF_TRIGGER_LOW) {
		m9w_bat->usb_irq_type =
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		irq_set_irq_type(irq, m9w_bat->usb_irq_type);
	} 

	return IRQ_WAKE_THREAD;
}

static void m9w_lowbat_func(struct work_struct *work)
{
	struct m9w_bat_info *m9w_bat = 
		container_of(work, struct m9w_bat_info, m9w_low_work.work);
	int low_bat_pin = gpio_get_value(LOW_BT_DETEC);

	if (!low_bat_pin) {
		m9w_bat->bat_info.batt_vol = m9w_get_bat_vol(m9w_bat);
		if (m9w_bat->bat_info.batt_vol <= 3500) {
			m9w_bat->bat_info.level = 0;
			m9w_bat->bat_info.batt_vol = 0;
			power_supply_changed(&m9w_bat->psy_bat);
		} else
			pr_info("%s: bat = %d\n", __func__, m9w_bat->bat_info.batt_vol);
	} else {
		pr_debug("status of low_bat is no stable\n");
	}
}

static irqreturn_t low_int_work_func(int irq, void *dev_id)
{
	struct m9w_bat_info *m9w_bat = dev_id;
	int low_bat_pin = gpio_get_value(LOW_BT_DETEC);

	if (m9w_bat->debug)
		pr_info("%s: low_bat_pin = %d\n", __func__, low_bat_pin);

	if (!low_bat_pin) {
		queue_delayed_work(m9w_bat->monitor_wqueue, 
			&m9w_bat->m9w_low_work, msecs_to_jiffies(200));
	}

	return IRQ_HANDLED;
}

static void m9w_bat_alarm(struct alarm *alarm)
{
	struct m9w_bat_info *m9w_bat =
			container_of(alarm, struct m9w_bat_info, alarm);

	wake_lock(&m9w_bat->bat_wake_lock);
	queue_delayed_work(m9w_bat->monitor_wqueue, &m9w_bat->m9w_bat_work, 0);
}

static int __devinit m9w_bat_probe(struct platform_device *pdev)
{
	struct ltc3577_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct ltc3577_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct m9w_bat_info *m9w_bat;
	int ret = 0;

	pr_info("%s : LTC3577 Charger Driver Loading\n", __func__);

	m9w_bat = kzalloc(sizeof(*m9w_bat), GFP_KERNEL);
	if (m9w_bat == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	atomic_set(&usb_bind, 0);
	g_m9w_bat = m9w_bat;

	m9w_bat->iodev = iodev;
	m9w_bat->pdata = pdata->charger;

	m9w_bat->psy_bat.name = "battery",
	m9w_bat->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY,
	m9w_bat->psy_bat.properties = m9w_battery_props,
	m9w_bat->psy_bat.num_properties = ARRAY_SIZE(m9w_battery_props),
	m9w_bat->psy_bat.get_property = m9w_bat_get_property,

	m9w_bat->psy_usb.name = "usb",
	m9w_bat->psy_usb.type = POWER_SUPPLY_TYPE_USB,
	m9w_bat->psy_usb.supplied_to = supply_list,
	m9w_bat->psy_usb.num_supplicants = ARRAY_SIZE(supply_list),
	m9w_bat->psy_usb.properties = m9w_power_properties,
	m9w_bat->psy_usb.num_properties = ARRAY_SIZE(m9w_power_properties),
	m9w_bat->psy_usb.get_property = m9w_usb_get_property,

	m9w_bat->psy_ac.name = "ac",
	m9w_bat->psy_ac.type = POWER_SUPPLY_TYPE_MAINS,
	m9w_bat->psy_ac.supplied_to = supply_list,
	m9w_bat->psy_ac.num_supplicants = ARRAY_SIZE(supply_list),
	m9w_bat->psy_ac.properties = m9w_power_properties,
	m9w_bat->psy_ac.num_properties = ARRAY_SIZE(m9w_power_properties),
	m9w_bat->psy_ac.get_property = m9w_ac_get_property,

	m9w_bat->present = 1;
	m9w_bat->bat_info.level = 90;	//default capacity
	m9w_bat->bat_info.charging_enabled = 0;
	m9w_bat->bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;
	m9w_bat->bat_info.charge_health = POWER_SUPPLY_HEALTH_GOOD;
	m9w_bat->debug = true;	//open debug msg ??

	m9w_bat->cable_status = CABLE_TYPE_NONE;

	mutex_init(&m9w_bat->mutex);

	platform_set_drvdata(pdev, m9w_bat);

#ifdef CONFIG_BQ27541	
	bq27541_i2c_init(pdev);
	m9w_bat->bat_info.batt_id = bq27541_checkid();
#endif

	wake_lock_init(&m9w_bat->vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
	wake_lock_init(&m9w_bat->bat_wake_lock, WAKE_LOCK_SUSPEND, "m9w_bat_work");
	wake_lock_init(&m9w_bat->usb_wake_lock, WAKE_LOCK_SUSPEND, "m9w_usb_work");

	m9w_bat->monitor_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if(m9w_bat->monitor_wqueue == NULL){
		pr_err("Failed to create freezeable workqueue\n");
		ret = -ENOMEM;
		goto err_wake_lock;
	}

	INIT_DELAYED_WORK(&m9w_bat->m9w_bat_work, m9w_bat_work);
	m9w_bat->last_poll = alarm_get_elapsed_realtime();
	alarm_init(&m9w_bat->alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
		m9w_bat_alarm);
	m9w_program_alarm(m9w_bat, FAST_POLL);

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &m9w_bat->psy_bat);
	if (ret) {
		pr_err("Failed to register power supply psy_bat\n");
		goto err_wqueue;
	}

	ret = power_supply_register(&pdev->dev, &m9w_bat->psy_usb);
	if (ret) {
		pr_err("Failed to register power supply psy_usb\n");
		goto err_supply_unreg_bat;
	}

	ret = power_supply_register(&pdev->dev, &m9w_bat->psy_ac);
	if (ret) {
		pr_err("Failed to register power supply psy_ac\n");
		goto err_supply_unreg_usb;
	}

	/* create sec detail attributes */
	ret = m9w_bat_create_attrs(m9w_bat->psy_bat.dev);
	if (ret) {
		pr_err("Failed to create attrs\n");
		goto err_create_attrs;
	}

	INIT_DELAYED_WORK(&m9w_bat->m9w_usb_work, m9w_usb_work_func);
	m9w_bat->usb_irq_type = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
	ret = request_threaded_irq(m9w_bat->pdata->usb_int, usb_int_irq_handle,
			usb_int_work_func, m9w_bat->usb_irq_type,
			"tlc3577-usb", m9w_bat);
	if(ret) {
		pr_err("%s : Failed to request ltc3577 usb irq\n", __func__);
		goto err_supply_unreg_ac;
	}
	
	INIT_DELAYED_WORK(&m9w_bat->m9w_low_work, m9w_lowbat_func);
	ret = request_threaded_irq(m9w_bat->pdata->low_bat_int, NULL,
			low_int_work_func, 
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"low battery isr", m9w_bat);
	if(ret) {
		pr_err("%s : Failed to request low battery irq\n", __func__);
		goto err_supply_low_bat;
	}

	m9w_bat->charger_cur = regulator_get(NULL, "charger_cur");
	if (IS_ERR(m9w_bat->charger_cur)) {
		pr_err("get regulator charger_cur failed\n");
		ret = PTR_ERR(m9w_bat->charger_cur);
		goto err_lowbat_irq;
	}
	regulator_set_current_limit(m9w_bat->charger_cur,
			OTG_CUR_10mA, OTG_CUR_10mA);

	m9w_bat->usb_pin_low = !gpio_get_value(USB_INT);

	/* do not to do if  m9w_bat->usb_pin_low == true 
	  * because function of usb_int_irq_handle will to do
	  */
	if (m9w_bat->usb_pin_low == false) {
		/* update battery info */
		queue_delayed_work(m9w_bat->monitor_wqueue, 
				&m9w_bat->m9w_bat_work, 0);

		/* update usb cable state */
		queue_delayed_work(m9w_bat->monitor_wqueue, 
			&m9w_bat->m9w_usb_work, 0);
	}
	
	/* update low bat level info */
	queue_delayed_work(m9w_bat->monitor_wqueue,
			&m9w_bat->m9w_low_work, msecs_to_jiffies(400));

	device_init_wakeup(&pdev->dev, 1);

	return 0;

err_lowbat_irq:
	free_irq(m9w_bat->pdata->low_bat_int, m9w_bat);
err_supply_low_bat:
	free_irq(m9w_bat->pdata->usb_int, m9w_bat);
err_supply_unreg_ac:
	cancel_delayed_work_sync(&m9w_bat->m9w_usb_work);
	m9w_bat_destroy_atts(m9w_bat->psy_bat.dev);
err_create_attrs:
	power_supply_unregister(&m9w_bat->psy_ac);
err_supply_unreg_usb:
	power_supply_unregister(&m9w_bat->psy_usb);
err_supply_unreg_bat:
	power_supply_unregister(&m9w_bat->psy_bat);
err_wqueue:
	destroy_workqueue(m9w_bat->monitor_wqueue);
	cancel_delayed_work_sync(&m9w_bat->m9w_bat_work);
	alarm_cancel(&m9w_bat->alarm);
err_wake_lock:
	wake_lock_destroy(&m9w_bat->usb_wake_lock);
	wake_lock_destroy(&m9w_bat->bat_wake_lock);
	wake_lock_destroy(&m9w_bat->vbus_wake_lock);
	mutex_destroy(&m9w_bat->mutex);
	kfree(m9w_bat);
	return ret;
}

static int __devexit m9w_bat_remove(struct platform_device *pdev)
{
	struct m9w_bat_info *m9w_bat = platform_get_drvdata(pdev);

	alarm_cancel(&m9w_bat->alarm);
	free_irq(m9w_bat->pdata->usb_int, m9w_bat);
	flush_workqueue(m9w_bat->monitor_wqueue);
	destroy_workqueue(m9w_bat->monitor_wqueue);
	power_supply_unregister(&m9w_bat->psy_bat);
	power_supply_unregister(&m9w_bat->psy_usb);
	power_supply_unregister(&m9w_bat->psy_ac);
	m9w_bat_destroy_atts(m9w_bat->psy_bat.dev);

	wake_lock_destroy(&m9w_bat->usb_wake_lock);
	wake_lock_destroy(&m9w_bat->bat_wake_lock);
	wake_lock_destroy(&m9w_bat->vbus_wake_lock);
	mutex_destroy(&m9w_bat->mutex);
	kfree(m9w_bat);

	return 0;
}

static int m9w_bat_suspend(struct device *dev)
{
	struct m9w_bat_info *m9w_bat = dev_get_drvdata(dev);

	disable_irq(m9w_bat->pdata->usb_int);

	m9w_bat_notify(POWER_STATE_SUSPEND, m9w_bat);

	if (work_pending(&m9w_bat->m9w_bat_work.work))
		WARN_ON(1);
	
	 if (work_pending(&m9w_bat->m9w_low_work.work))
	 	WARN_ON(1);
	 
	  if (work_pending(&m9w_bat->m9w_usb_work.work))
		WARN_ON(1);

	if (!m9w_bat->charging) {
		m9w_program_alarm(m9w_bat, SLOW_POLL);
		m9w_bat->slow_poll = 1;
	}

	enable_irq_wake(m9w_bat->pdata->low_bat_int);
#ifdef CONFIG_USB_SUPPORT
	enable_irq_wake(m9w_bat->pdata->usb_int);
#endif

	return 0;
}

static void m9w_bat_resume(struct device *dev)
{
	struct m9w_bat_info *m9w_bat = dev_get_drvdata(dev);
	
	/* We might be on a slow sample cycle.  If we're
	 * resuming we should resample the battery state
	 * if it's been over a minute since we last did
	 * so, and move back to sampling every minute until
	 * we suspend again.
	 */
	if (m9w_bat->slow_poll) {
		m9w_program_alarm(m9w_bat, FAST_POLL);
		m9w_bat->slow_poll = 0;
	}

	disable_irq_wake(m9w_bat->pdata->low_bat_int);
#ifdef CONFIG_USB_SUPPORT
	disable_irq_wake(m9w_bat->pdata->usb_int);
#endif

	enable_irq(m9w_bat->pdata->usb_int);

	m9w_bat_notify(POWER_STATE_RESUME, m9w_bat);
}

static const struct dev_pm_ops m9w_bat_pm_ops = {
	.prepare		= m9w_bat_suspend,
	.complete		= m9w_bat_resume,
};

static struct platform_driver m9w_bat_driver = {
	.driver = {
		.name = "ltc3577-charger",
		.owner = THIS_MODULE,
		.pm = &m9w_bat_pm_ops,
	},
	.probe		= m9w_bat_probe,
	.remove		= __devexit_p(m9w_bat_remove),
};

static int __init m9w_bat_init(void)
{
	return platform_driver_register(&m9w_bat_driver);
}

static void __exit m9w_bat_exit(void)
{
	platform_driver_unregister(&m9w_bat_driver);
}

late_initcall(m9w_bat_init);
module_exit(m9w_bat_exit);

static int __m9w_bat_notify(unsigned long val, void *v, int nr_to_call,
			int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&m9w_bat_chain, val, v, nr_to_call,
					nr_calls);

	return notifier_to_errno(ret);
}

static int m9w_bat_notify(unsigned long val, void *v)
{
	return __m9w_bat_notify(val, v, -1, NULL);
}

/* Need to know about CPUs going up/down? */
int  register_m9w_bat_notifier(struct notifier_block *nb)
{
	int ret;

	ret = raw_notifier_chain_register(&m9w_bat_chain, nb);

	return ret;
}
EXPORT_SYMBOL(register_m9w_bat_notifier);

void unregister_m9w_bat_notifier(struct notifier_block *nb)
{
	raw_notifier_chain_unregister(&m9w_bat_chain, nb);
}
EXPORT_SYMBOL(unregister_m9w_bat_notifier);

MODULE_AUTHOR("Lvcha Qiu <lvcha@meizu.com>");
MODULE_DESCRIPTION("battery driver for Meizu M9W");
MODULE_LICENSE("GPLV2");
