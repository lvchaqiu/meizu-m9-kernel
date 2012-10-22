/* linux/arch/arm/plat-s5pc11x/ltc3577.h
 *
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: lvcha qiu <lvcha@meizu.com>
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
 * Inital code : Apr 16 , 2010 : lvcha@meizu.com
 *
*/
#ifndef __MEIZU_M9_LC3577_H
#define __MEIZU_M9_LC3577_H __FILE__

#include <linux/regulator/machine.h>

/* ltc 3577-3 regulator ids */
enum {
	LTC3577_BUCK3,		/* 1.25v ARM CORE */
	LTC3577_BUCK4,		/* 1.10v ARM INT */
	LTC3577_ISINK1,		/* USB CHARGER */
};

/**
 * ltc3577_regulator_data - regulator data
 * @id: regulator id
 * @initdata: regulator init data (contraints, supplies, ...)
 */
struct ltc3577_regulator_data {
	int				id;
	struct regulator_init_data	*initdata;
};

enum cable_type_t {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
};

/**
 * ltc3577_charger_data - charger data
 */
struct ltc3577_charger_data {
	int usb_int;
	int charger_int;
	int low_bat_int;
	int (*usb_attach) (bool enable);
};

/**
 * struct ltc3577_platform_data - packages regulator init data
 * @num_regulators: number of regultors used
 * @regulators: array of defined regulators
 */

struct ltc3577_platform_data {
	int	num_regulators;
	struct ltc3577_regulator_data	*regulators;
	struct ltc3577_charger_data	*charger;
};

#endif //__MEIZU_M9_LC3577_H
