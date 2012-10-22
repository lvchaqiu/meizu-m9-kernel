/* linux/drivers/video/samsung/s5p_mipi_dsi_common.h
 *
 * Header file for Samsung MIPI-DSI common driver.
 *
 * Copyright (c) 2009 Samsung Electronics
 * InKi Dae <inki.dae@xxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S5P_MIPI_COMMON_H
#define _S5P_MIPI_COMMON_H

int s5p_mipi_pll_on(struct dsim_device *dsim, unsigned int enable);
unsigned long s5p_mipi_change_pll(struct dsim_device *dsim,
unsigned int pre_divider, unsigned int main_divider,
	unsigned int scaler);
int s5p_mipi_set_clock(struct dsim_device *dsim,
	unsigned int byte_clk_sel, unsigned int enable);
int s5p_mipi_init_dsim(struct dsim_device *dsim);
int s5p_mipi_set_display_mode(struct dsim_device *dsim,
			struct dsim_config *dsim_info);
int s5p_mipi_init_link(struct dsim_device *dsim);
int s5p_mipi_set_hs_enable(struct dsim_device *dsim);
int s5p_mipi_set_data_transfer_mode(struct dsim_device *dsim,
	unsigned int data_path, unsigned int hs_enable);
int s5p_mipi_enable_frame_done_int(struct dsim_device *dsim,
	unsigned int enable);

extern struct fb_info *registered_fb[FB_MAX] __read_mostly;

#endif /* _S5P_MIPI_COMMON_H */
