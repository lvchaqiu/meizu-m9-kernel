/* linux/drivers/video/samsung/s5p_mipi_dsi_lowlevel.h
 *
 * Header file for Samsung MIPI-DSI lowlevel driver.
 *
 * Copyright (c) 2009 Samsung Electronics
 * InKi Dae <inki.dae@xxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _S5P_MIPI_LOWLEVEL_H
#define _S5P_MIPI_LOWLEVEL_H

void s5p_mipi_func_reset(struct dsim_device *dsim);
void s5p_mipi_sw_reset(struct dsim_device *dsim);
void s5p_mipi_set_interrupt_mask(struct dsim_device *dsim,
	unsigned int mode, unsigned int mask);
void s5p_mipi_set_data_lane_number(struct dsim_device *dsim,
				unsigned int count);
void s5p_mipi_init_fifo_pointer(struct dsim_device *dsim,
					unsigned int cfg);
void s5p_mipi_set_phy_tunning(struct dsim_device *dsim,
				unsigned int value);
void s5p_mipi_set_phy_tunning(struct dsim_device *dsim,
				unsigned int value);
void s5p_mipi_set_main_disp_resol(struct dsim_device *dsim,
		unsigned int vert_resol, unsigned int hori_resol);
void s5p_mipi_set_main_disp_vporch(struct dsim_device *dsim,
	unsigned int cmd_allow, unsigned int vfront, unsigned int vback);
void s5p_mipi_set_main_disp_hporch(struct dsim_device *dsim,
			unsigned int front, unsigned int back);
void s5p_mipi_set_main_disp_sync_area(struct dsim_device *dsim,
				unsigned int vert, unsigned int hori);
void s5p_mipi_set_sub_disp_resol(struct dsim_device *dsim,
				unsigned int vert, unsigned int hori);
void s5p_mipi_init_config(struct dsim_device *dsim);
void s5p_mipi_display_config(struct dsim_device *dsim,
				struct dsim_config *dsim_info);
void s5p_mipi_set_data_lane_number(struct dsim_device *dsim,
			unsigned int count);
void s5p_mipi_enable_lane(struct dsim_device *dsim, unsigned int lane,
				unsigned int enable);
void s5p_mipi_enable_afc(struct dsim_device *dsim, unsigned int enable,
				unsigned int afc_code);
void s5p_mipi_enable_pll_bypass(struct dsim_device *dsim,
				unsigned int enable);
void s5p_mipi_set_pll_pms(struct dsim_device *dsim, unsigned int p,
				unsigned int m, unsigned int s);
void s5p_mipi_pll_freq_band(struct dsim_device *dsim,
				unsigned int freq_band);
void s5p_mipi_pll_freq(struct dsim_device *dsim,
			unsigned int pre_divider, unsigned int main_divider,
			unsigned int scaler);
void s5p_mipi_pll_stable_time(struct dsim_device *dsim,
			unsigned int lock_time);
void s5p_mipi_enable_pll(struct dsim_device *dsim, unsigned int enable);
void s5p_mipi_set_byte_clock_src(struct dsim_device *dsim, unsigned int src);
void s5p_mipi_enable_byte_clock(struct dsim_device *dsim, unsigned int enable);
void s5p_mipi_set_esc_clk_prs(struct dsim_device *dsim,
				unsigned int enable, unsigned int prs_val);
void s5p_mipi_enable_esc_clk_on_lane(struct dsim_device *dsim,
				unsigned int lane_sel, unsigned int enable);
void s5p_mipi_force_dphy_stop_state(struct dsim_device *dsim,
				unsigned int enable);
unsigned int s5p_mipi_is_lane_state(struct dsim_device *dsim,
				unsigned int lane);
void s5p_mipi_set_stop_state_counter(struct dsim_device *dsim,
				unsigned int cnt_val);
void s5p_mipi_set_bta_timeout(struct dsim_device *dsim,
				unsigned int timeout);
void s5p_mipi_set_lpdr_timeout(struct dsim_device *dsim,
				unsigned int timeout);
void s5p_mipi_set_data_mode(struct dsim_device *dsim,
				unsigned int data, unsigned int state);
void s5p_mipi_enable_hs_clock(struct dsim_device *dsim,
				unsigned int enable);
void s5p_mipi_dp_dn_swap(struct dsim_device *dsim,
				unsigned int swap_en);
void s5p_mipi_hs_zero_ctrl(struct dsim_device *dsim,
				unsigned int hs_zero);
void s5p_mipi_prep_ctrl(struct dsim_device *dsim, unsigned int prep);
void s5p_mipi_clear_interrupt(struct dsim_device *dsim, unsigned int int_src);
unsigned int s5p_mipi_is_pll_stable(struct dsim_device *dsim);
unsigned int s5p_mipi_get_fifo_state(struct dsim_device *dsim);
unsigned int _s5p_mipi_get_frame_done_status(struct dsim_device *dsim);
void _s5p_mipi_clear_frame_done(struct dsim_device *dsim);
void s5p_mipi_wr_tx_header(struct dsim_device *dsim, unsigned int di,
				unsigned int data0, unsigned int data1);
void s5p_mipi_wr_tx_data(struct dsim_device *dsim, unsigned int tx_data);

void s5p_mipi_force_bta(struct dsim_device *dsim);

int s5p_mipi_wait_state(struct dsim_device *dsim,
	enum dsim_int_src intsrc);

#endif /* _S5P_MIPI_LOWLEVEL_H */
