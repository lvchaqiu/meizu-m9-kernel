/* linux/arm/arch/plat-s5p/include/plat/mipi_ddi.h
 *
 * definitions for DDI based MIPI-DSI.
 *
 * Copyright (c) 2009 Samsung Electronics
 * InKi Dae <inki.dae@xxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _MIPI_DDI_H
#define _MIPI_DDI_H

#include <linux/lcd.h>

enum mipi_ddi_interface {
	RGB_IF = 0x4000,
	I80_IF = 0x8000,
	YUV_601 = 0x10000,
	YUV_656 = 0x20000,
	MIPI_VIDEO = 0x1000,
	MIPI_COMMAND = 0x2000,
};

enum mipi_ddi_panel_select {
	DDI_MAIN_LCD = 0,
	DDI_SUB_LCD = 1,
};

enum mipi_ddi_parameter {
	/* DSIM video interface parameter */
	DSI_VIRTUAL_CH_ID = 0,
	DSI_FORMAT = 1,
	DSI_VIDEO_MODE_SEL = 2,
};

struct mipi_ddi_platform_data {
	/*
	 * it is used for command mode lcd panel and
 	 * when all contents of framebuffer in panel module are transfered
	 * to lcd panel it occurs te signal.
	 *
	 * note:
	 * - in case of command mode(cpu mode), it should be triggered only
 	 *   when TE signal of lcd panel and frame done interrupt of display
	 *   controller or mipi controller occurs.
	 */
	unsigned int te_irq;

	int (*lcd_reset) (struct lcd_device *ld);
	int (*lcd_power_on) (struct lcd_device *ld, int enable);
	int (*backlight_on) (struct lcd_device *ld, int enable);

	unsigned int reset_delay;
	unsigned int power_on_delay;
	unsigned int power_off_delay;
};

#endif /* _MIPI_DDI_H */
