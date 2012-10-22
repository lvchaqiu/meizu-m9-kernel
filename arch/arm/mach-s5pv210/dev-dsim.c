/* linux/arch/arm/mach-s5pv210/dev-dsim.c
 *
 * Copyright 2009 Samsung Electronics
 *	InKi Dae <inki.dae@xxxxxxxxxxx>
 *
 * S5PC1XX series device definition for MIPI-DSIM
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <mach/map.h>
#include <mach/irqs.h>

#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>

#include <plat/mipi-ddi.h>
#include <plat/mipi-dsi.h>

static int ls035b3sx01_reset(struct lcd_device *ld)
{
	s3c_gpio_setpin(LCD_RESET_GPIO, 0);
	s3c_gpio_cfgpin(LCD_RESET_GPIO, S3C_GPIO_OUTPUT);
	mdelay(10);
	s3c_gpio_setpin(LCD_RESET_GPIO, 1);
	return 0;
}

/* define ddi platform data based on MIPI-DSI. */
static struct mipi_ddi_platform_data mipi_ddi_pd1 = {
	.lcd_reset 		= ls035b3sx01_reset,

	.reset_delay = 40,
	.power_on_delay = 1,
	.power_off_delay = 120,
};

static struct dsim_config dsim_info = {
	/* only DSIM_1_03 */
	/* main frame fifo auto flush at VSYNC pulse
	 *  0 = Enable (defalut)  1 = Disable
	 */
	.auto_flush = true,

	/*  Disables EoT packet in HS mode.
	 *  0 = Enables EoT packet generation for V1.01r11
	 *  1 = Disables EoT packet generation for V1.01r03
	 */
	.eot_disable = false,

	/*  Specifies auto vertical count mode.
	 *  In Video mode, the vertical line transition uses line counter
	 *  configured by VSA, VBP, and Vertical resolution. If this bit
	 *  is set to ¡®1¡¯, the line counter does not use VSA and VBP
	 *  registers.
	 *  0 = Configuration mode
	 *  1 = Auto mode
	 *  In command mode, this bit is ignored.
	 */
	.auto_vertical_cnt = false,

	/*  In Vsync pulse and Vporch area, MIPI DSI master transfers
	 *  only Hsync start packet to MIPI DSI slave at MIPI DSI spec
	 *  1.1r02. This bit transfers Hsync end packet in Vsync pulse
	 *  and Vporch area (optional).
	 *  0 = Disables transfer
	 *  1 = Enables transfer
	 *  In command mode, this bit is ignored.
	 */
	.hse = true,

	/*  Specifies HFP disable mode. If this bit set, DSI master
	 *  ignores HFP area in Video mode.
	 *  0 = Enables
	 *  1 = Disables
	 *  In command mode, this bit is ignored.
	 */
	.hfp = false,

	/*  Specifies HBP disable mode. If this bit set, DSI master
	 *  ignores HBP area in Video mode.
	 *  0 = Enables
	 *  1 = Disables
	 *  In command mode, this bit is ignored.
	 */
	.hbp = true,

	/*  Specifies HSA disable mode. If this bit set, DSI master
	 *  ignores HSA area in Video mode.
	 *  0 = Enables
	 *  1 = Disables
	 *  In command mode, this bit is ignored.
	 */
	.hsa = false,

	.e_interface = DSIM_VIDEO,
	.e_virtual_ch =DSIM_VIRTUAL_CH_0,
	.e_pixel_format = DSIM_24BPP_888,
	.e_burst_mode =DSIM_NON_BURST_SYNC_EVENT ,

	.e_no_data_lane = DSIM_DATA_LANE_2,
	.e_byte_clk = DSIM_PLL_OUT_DIV8,

	/*
	 * ===========================================
	 * |    P    |    M    |    S    |    MHz    |
	 * -------------------------------------------
	 * |    3    |   100   |    3    |    100    |
	 * |    3    |   100   |    2    |    200    |
	 * |    3    |    63   |    1    |    252    |
	 * |    4    |   100   |    1    |    300    |
	 * |    4    |   110   |    1    |    330    |
	 * |   12    |   350   |    1    |    350    |
	 * |    3    |   100   |    1    |    400    |
	 * |    4    |   150   |    1    |    450    |
	 * |    3    |   118   |    1    |    472    |
	 * |   12    |   250   |    0    |    500    |
	 * |    4    |   100   |    0    |    600    |
	 * |    3    |    81   |    0    |    648    |
	 * |    3    |    88   |    0    |    704    |
	 * |    3    |    90   |    0    |    720    |
	 * |    3    |   100   |    0    |    800    |
	 * |   12    |   425   |    0    |    850    |
	 * |    4    |   150   |    0    |    900    |
	 * |   12    |   475   |    0    |    950    |
	 * |    6    |   250   |    0    |   1000    |
	 * -------------------------------------------
	 */

	/* 500 MHz: Sharp Recommended */
	.p = 3,
	.m = 125,
	.s = 1,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 400,

	.esc_clk = 20 * 1000000,	/* escape clk : 10MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x07ff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.e_lane_swap = DSIM_NO_CHANGE,

	.mipi_ddi_pd	= (void *)&mipi_ddi_pd1,
};

static struct resource s5p_dsim_resource[] = {
	[0] = {
		.start = S5PV210_PA_DSI,
		.end   = S5PV210_PA_DSI + S5PV210_SZ_DSI - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_MIPIDSI,
		.end   = IRQ_MIPIDSI,
		.flags = IORESOURCE_IRQ,
	},
};

static struct s5p_platform_dsim dsim_def_platform_data = {
	.dsim_info			= &dsim_info,

	.mipi_power			= s5p_mipi_power,
	.part_reset			= s5p_mipi_part_reset,
	.init_d_phy			= s5p_mipi_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger				= NULL,

	/*
	 * the stable time of needing to write data on SFR
	 * when the mipi mode becomes LP mode.
	 */
	.delay_for_stabilization = 50,
};

struct platform_device s5p_device_dsim = {
	.name				= "s5p-dsim",
	.id					= -1,
	.num_resources		= ARRAY_SIZE(s5p_dsim_resource),
	.resource				= s5p_dsim_resource,
};

void __init s5p_dsi_set_platdata(struct s5p_platform_dsim *pd)
{
	struct s5p_platform_dsim *npd = &dsim_def_platform_data;

	if (!pd) {
		printk(KERN_ERR "%s: no platform data\n", __func__);
		return;
	}

	if (pd->lcd_panel_name)
		strcpy(npd->lcd_panel_name, pd->lcd_panel_name);
	if (pd->dsim_info->lcd_panel_info)
		npd->dsim_info->lcd_panel_info = pd->dsim_info->lcd_panel_info;

	s5p_device_dsim.dev.platform_data = npd;
}

