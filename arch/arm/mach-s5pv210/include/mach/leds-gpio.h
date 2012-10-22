/* arch/arm/mach-s5pv210/include/mach/leds-gpio.h
 *
 * Copyright (C) 2011 Meizu Technology Co.Ltd, Zhuhai, China
 *	http://www.meizu.com/
 *	lvcha qiu <lvcha@meizu.com>
 *
 * S5P - LEDs GPIO connector
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_LEDSGPIO_H
#define __ASM_ARCH_LEDSGPIO_H

#define M9W_LEDF_ACTLOW	(1<<0)		/* LED is on when GPIO low */
#define M9W_LEDF_TRISTATE	(1<<1)		/* tristate to turn off */

struct m9w_led_platdata {
#ifdef CONFIG_HAVE_PWM
	int pwm_id;
	unsigned int max_brightness;
	unsigned int pwm_period_ns;
#endif	
	unsigned int		 gpio;
	unsigned int		 flags;

	char			*name;
	char			*def_trigger;
};

#endif /* __ASM_ARCH_LEDSGPIO_H */
