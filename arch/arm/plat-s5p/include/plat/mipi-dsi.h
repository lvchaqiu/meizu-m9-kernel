/* linux/arm/arch/plat-s5p/include/plat/mipi_dsi.h
 *
 * Platform data header for Samsung MIPI-DSI.
 *
 * Copyright (c) 2009 Samsung Electronics
 * InKi Dae <inki.dae@xxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _MIPI_DSI_H
#define _MIPI_DSI_H

#include <linux/device.h>
#include <linux/fb.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#endif

#define PANEL_NAME_SIZE		(32)

enum dsim_interface_type {
	DSIM_COMMAND = 0,
	DSIM_VIDEO = 1,
};

enum dsim_state {
	DSIM_STATE_RESET = 0,
	DSIM_STATE_INIT = 1,
	DSIM_STATE_STOP = 2,
	DSIM_STATE_HSCLKEN = 3,
	DSIM_STATE_ULPS = 4,
};

enum dsim_virtual_ch_no {
	DSIM_VIRTUAL_CH_0 = 0,
	DSIM_VIRTUAL_CH_1 = 1,
	DSIM_VIRTUAL_CH_2 = 2,
	DSIM_VIRTUAL_CH_3 = 3,
};

enum dsim_burst_mode_type {
	DSIM_NON_BURST_SYNC_EVENT = 0,
	DSIM_NON_BURST_SYNC_PULSE = 2,
	DSIM_BURST = 1,	/*lvcha*/
	DSIM_NON_VIDEO_MODE = 4,
};

enum dsim_no_of_data_lane {
	DSIM_DATA_LANE_1 = 0,
	DSIM_DATA_LANE_2 = 1,
	DSIM_DATA_LANE_3 = 2,
	DSIM_DATA_LANE_4 = 3,
};

enum dsim_byte_clk_src {
	DSIM_PLL_OUT_DIV8 = 0,
	DSIM_EXT_CLK_DIV8 = 1,
	DSIM_EXT_CLK_BYPASS = 2,
};

enum dsim_lane {
	DSIM_LANE_DATA0 = (1 << 0),
	DSIM_LANE_DATA1 = (1 << 1),
	DSIM_LANE_DATA2 = (1 << 2),
	DSIM_LANE_DATA3 = (1 << 3),
	DSIM_LANE_DATA_ALL = 0xf,
	DSIM_LANE_CLOCK = (1 << 4),
	DSIM_LANE_ALL = DSIM_LANE_CLOCK | DSIM_LANE_DATA_ALL,
};

enum dsim_pixel_format {
	DSIM_CMD_3BPP = 0,
	DSIM_CMD_8BPP = 1,
	DSIM_CMD_12BPP = 2,
	DSIM_CMD_16BPP = 3,
	DSIM_VID_16BPP_565 = 4,
	DSIM_VID_18BPP_666PACKED = 5,
	DSIM_18BPP_666LOOSELYPACKED = 6,
	DSIM_24BPP_888 = 7,
};

enum dsim_lane_state {
	DSIM_LANE_STATE_HS_READY,
	DSIM_LANE_STATE_ULPS,
	DSIM_LANE_STATE_STOP,
	DSIM_LANE_STATE_LPDT,
};

enum dsim_transfer {
	DSIM_TRANSFER_NEITHER	= 0,
	DSIM_TRANSFER_BYCPU	= (1 << 7),
	DSIM_TRANSFER_BYLCDC	= (1 << 6),
	DSIM_TRANSFER_BOTH	= (0x3 << 6)
};

enum dsim_lane_change {
	DSIM_NO_CHANGE = 0,
	DSIM_DATA_LANE_CHANGE = 1,
	DSIM_CLOCK_NALE_CHANGE = 2,
	DSIM_ALL_LANE_CHANGE = 3,
};

enum dsim_int_src {
	DSIM_ALL_OF_INTR = 0xffffffff,
	DSIM_PLL_STABLE = (1 << 31),
	DSIM_SW_RESET_RELEASE = (1 << 30),
	DSIM_SFR_FIFO_EMPTY = (1 << 29),
	DSIM_BUS_TURN_OVER = (1 << 25),
	DSIM_FRAME_DONE_INT = (1 << 24),
	DSIM_LPDR_TIMEOUT = (1 << 21),
	DSIM_BTA_ACK_TIMEOUT = (1 << 20),
	DSIM_RX_DATA_DONE = (1 << 18),
	DSIM_RX_TE = (1 << 17),
	DSIM_RX_ACK = (1 << 16),
	DSIM_ERR_TX_ECC = (1 << 15),
	DSIM_ERR_RX_CRC = (1 << 14),
	DSIM_ERR_ESC_LANE3 = (1 << 13),
	DSIM_ERR_ESC_LANE2 = (1 << 12),
	DSIM_ERR_ESC_LANE1 = (1 << 11),
	DSIM_ERR_ESC_LANE0  = (1 << 10),
	DSIM_ERR_SYNC3 = (1 << 9),
	DSIM_ERR_SYNC2 = (1 << 8),
	DSIM_ERR_SYNC1 = (1 << 7),
	DSIM_ERR_SYNC0 = (1 << 6),
	DSIM_ERR_CTRL3 = (1 << 5),
	DSIM_ERR_CTRL2 = (1 << 4),
	DSIM_ERR_CTRL1 = (1 << 3),
	DSIM_ERR_CTRL0 = (1 << 2),
	DSIM_ERR_CONTENT_LP0 = (1 << 1),
	DSIM_ERR_CONTENT_LP1= (1 << 0),
};

/**
 * struct dsim_config - interface for configuring mipi-dsi controller.
 *
 * @auto_flush: enable or disable Auto flush of MD FIFO using VSYNC pulse.
 * @eot_disable: enable or disable EoT packet in HS mode.
 * @auto_vertical_cnt: specifies auto vertical count mode.
 *	in Video mode, the vertical line transition uses line counter
 *	configured by VSA, VBP, and Vertical resolution.
 *	If this bit is set to '1', the line counter does not use VSA and VBP
 *	registers.(in command mode, this variable is ignored)
 * @hse: set horizontal sync event mode.
 *	In VSYNC pulse and Vporch area, MIPI DSI master transfers only HSYNC
 *	start packet to MIPI DSI slave at MIPI DSI spec1.1r02.
 *	this bit transfers HSYNC end packet in VSYNC pulse and Vporch area
 *	(in mommand mode, this variable is ignored)
 * @hfp: specifies HFP disable mode.
 *	if this variable is set, DSI master ignores HFP area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @hbp: specifies HBP disable mode.
 *	if this variable is set, DSI master ignores HBP area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @hsa: specifies HSA disable mode.
 *	if this variable is set, DSI master ignores HSA area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @e_interface: specifies interface to be used.(CPU or RGB interface)
 * @e_virtual_ch: specifies virtual channel number that main or
 *	sub diaplsy uses.
 * @e_pixel_format: specifies pixel stream format for main or sub display.
 * @e_burst_mode: selects Burst mode in Video mode.
 *	in Non-burst mode, RGB data area is filled with RGB data and NULL
 *	packets, according to input bandwidth of RGB interface.
 *	In Burst mode, RGB data area is filled with RGB data only.
 * @e_no_data_lane: specifies data lane count to be used by Master.
 * @e_byte_clk: select byte clock source. (it must be DSIM_PLL_OUT_DIV8)
 *	DSIM_EXT_CLK_DIV8 and DSIM_EXT_CLK_BYPASSS are not supported.
 * @pll_stable_time: specifies the PLL Timer for stability of the ganerated
 *	clock(System clock cycle base)
 *	if the timer value goes to 0x00000000, the clock stable bit of status
 *	and interrupt register is set.
 * @esc_clk: specifies escape clock frequency for getting the escape clock
 *	prescaler value.
 * @stop_holding_cnt: specifies the interval value between transmitting
 *	read packet(or write "set_tear_on" command) and BTA request.
 *	after transmitting read packet or write "set_tear_on" command,
 *	BTA requests to D-PHY automatically. this counter value specifies
 *	the interval between them.
 * @bta_timeout: specifies the timer for BTA.
 *	this register specifies time out from BTA request to change
 *	the direction with respect to Tx escape clock.
 * @rx_timeout: specifies the timer for LP Rx mode timeout.
 *	this register specifies time out on how long RxValid deasserts,
 *	after RxLpdt asserts with respect to Tx escape clock.
 *	- RxValid specifies Rx data valid indicator.
 *	- RxLpdt specifies an indicator that D-PHY is under RxLpdt mode.
 *	- RxValid and RxLpdt specifies signal from D-PHY.
 * @e_lane_swap: swaps Dp/Dn channel of Clock lane or Data lane.
 *	if this bit is set, Dp and Dn channel would be swapped each other.
 * @lcd_panel_info: pointer for lcd panel specific structure.
 *	this structure specifies width, height, timing and polarity and so on.
 * @mipi_ddi_pd: pointer to lcd panel platform data.
 */
struct dsim_config {
	unsigned char auto_flush;
	unsigned char eot_disable;

	unsigned char auto_vertical_cnt;
	unsigned char hse;
	unsigned char hfp;
	unsigned char hbp;
	unsigned char hsa;

	enum dsim_interface_type e_interface;
	enum dsim_virtual_ch_no	e_virtual_ch;
	enum dsim_pixel_format e_pixel_format;
	enum dsim_burst_mode_type e_burst_mode;
	enum dsim_no_of_data_lane e_no_data_lane;
	enum dsim_byte_clk_src e_byte_clk;

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
	unsigned char p;
	unsigned short m;
	unsigned char s;

	unsigned int pll_stable_time;
	unsigned long esc_clk;

	unsigned short stop_holding_cnt;
	unsigned char bta_timeout;
	unsigned short rx_timeout;
	enum dsim_lane_change e_lane_swap;

	void *lcd_panel_info;
	void *mipi_ddi_pd;
};

/**
 * struct dsim_device - global interface for mipi-dsi driver.
 *
 * @dev: driver model representation of the device.
 * @clock: pointer to MIPI-DSI clock of clock framework.
 * @irq: interrupt number to MIPI-DSI controller.
 * @reg_base: base address to memory mapped SRF of MIPI-DSI controller.
 *	(virtual address)
 * @pd: pointer to MIPI-DSI driver platform data.
 * @dsim_info: infomation for configuring mipi-dsi controller.
 * @master_ops: callbacks to mipi-dsi operations.
 * @lcd_info: pointer to mipi_lcd_info structure.
 * @state: specifies status of MIPI-DSI controller.
 *	the status could be RESET, INIT, STOP, HSCLKEN and ULPS.
 * @resume_complete: indicates whether resume operation is completed or not.
 * @data_lane: specifiec enabled data lane number.
 *	this variable would be set by driver according to e_no_data_lane
 *	automatically.
 * @e_clk_src: select byte clock source.
 *	this variable would be set by driver according to e_byte_clock
 *	automatically.
 * @hs_clk: HS clock rate.
 *	this variable would be set by driver automatically.
 * @byte_clk: Byte clock rate.
 *	this variable would be set by driver automatically.
 * @escape_clk: ESCAPE clock rate.
 *	this variable would be set by driver automatically.
 * @freq_band: indicates Bitclk frequency band for D-PHY global timing.
 *	Serial Clock(=ByteClk X 8)		FreqBand[3:0]
 *		~ 99.99 MHz				0000
 *		100 ~ 119.99 MHz			0001
 *		120 ~ 159.99 MHz			0010
 *		160 ~ 199.99 MHz			0011
 *		200 ~ 239.99 MHz			0100
 *		140 ~ 319.99 MHz			0101
 *		320 ~ 389.99 MHz			0110
 *		390 ~ 449.99 MHz			0111
 *		450 ~ 509.99 MHz			1000
 *		510 ~ 559.99 MHz			1001
 *		560 ~ 639.99 MHz			1010
 *		640 ~ 689.99 MHz			1011
 *		690 ~ 769.99 MHz			1100
 *		770 ~ 869.99 MHz			1101
 *		870 ~ 949.99 MHz			1110
 *		950 ~ 1000 MHz				1111
 *	this variable would be calculated by driver automatically.
 */
struct dsim_device {
	struct device *dev;
	struct resource *res;
	struct clk *clock;
	struct regulator	*regulator;
	struct regulator	*regulator_pd;
	unsigned int irq;
	void __iomem *reg_base;

	struct s5p_platform_dsim *pd;
	struct dsim_config *dsim_info;
	struct dsim_master_ops *master_ops;
	struct mipi_lcd_info *lcd_info;

	unsigned char state;
	unsigned int resume_complete;
	unsigned int data_lane;
	enum dsim_byte_clk_src e_clk_src;
	unsigned long hs_clk;
	unsigned long byte_clk;
	unsigned long escape_clk;
	unsigned char freq_band;
#ifdef CONFIG_HAS_WAKELOCK
	struct early_suspend	early_suspend;
	struct early_suspend	earler_suspend;
#endif
};

/**
 * struct s5p_platform_dsim - interface to platform data for mipi-dsi driver.
 *
 * @lcd_panel_name: specifies lcd panel name registered to mipi-dsi driver.
 *	lcd panel driver searched would be actived.
 * @dsim_config: pointer of structure for configuring mipi-dsi controller.
 * @dsim_lcd_info: pointer to structure for configuring
 *	mipi-dsi based lcd panel.
 * @mipi_power: callback pointer for enabling or disabling mipi power.
 * @part_reset: callback pointer for reseting mipi phy.
 * @init_d_phy: callback pointer for enabing d_phy of dsi master.
 * @get_fb_frame_done: callback pointer for getting frame done status of the
 *	display controller(FIMD).
 * @trigger: callback pointer for triggering display controller(FIMD)
 *	in case of CPU mode.
 * @delay_for_stabilization: specifies stable time.
 *	this delay needs when writing data on SFR
 *	after mipi mode became LP mode.
 */
struct s5p_platform_dsim {
	char	lcd_panel_name[PANEL_NAME_SIZE];

	struct dsim_config *dsim_info;

	unsigned int delay_for_stabilization;

	int (*mipi_power) (struct dsim_device *dsim, unsigned int enable);
	int (*part_reset) (struct dsim_device *dsim);
	int (*init_d_phy) (struct dsim_device *dsim, unsigned int enable);
	int (*get_fb_frame_done) (struct fb_info *info);
	void (*trigger) (struct fb_info *info);
};

/**
 * struct dsim_master_ops - callbacks to mipi-dsi operations.
 *
 * @cmd_write: transfer command to lcd panel at LP mode.
 * @cmd_read: read command from rx register.
 * @get_dsim_frame_done: get the status that all screen data have been
 *	transferred to mipi-dsi.
 * @clear_dsim_frame_done: clear frame done status.
 * @change_dsim_transfer_mode: change transfer mode to LP or HS mode.
 *	- LP mode is used when commands data ard transferred to lcd panel.
 * @get_fb_frame_done: get frame done status of display controller.
 * @trigger: trigger display controller.
 *	- this one would be used only in case of CPU mode.
 */

struct dsim_master_ops {
	int (*cmd_write) (struct dsim_device *dsim, unsigned int data_id,
		unsigned int data0, unsigned int data1);
	int (*cmd_read) (struct dsim_device *dsim, unsigned int data_id,
		unsigned int data0, unsigned int data1);
	int (*get_dsim_frame_done) (struct dsim_device *dsim);
	int (*clear_dsim_frame_done) (struct dsim_device *dsim);

	int (*change_dsim_transfer_mode) (struct dsim_device *dsim,
						unsigned int mode);

	int (*get_fb_frame_done) (struct fb_info *info);
	void (*trigger) (struct fb_info *info);
};

/**
 * device structure for mipi-dsi based lcd panel.
 *
 * @dev: driver model representation of the device.
 * @id: id of device registered and when device is registered
 *	id would be counted.
 * @modalias: name of the driver to use with this device, or an
 *	alias for that name.
 * @mipi_lcd_drv: pointer of mipi_lcd_driver.
 * @master: pointer to dsim_device.
 */
struct mipi_lcd_device {
	struct	device	dev;
	int	id;
	char	modalias[64];

	struct mipi_lcd_driver	*mipi_drv;
	struct dsim_device	*master;
};

/**
 * driver structure for mipi-dsi based lcd panel.
 *
 * this structure should be registered by lcd panel driver.
 * mipi-dsi driver seeks lcd panel registered through name field
 * and calls these callback functions in appropriate time.
 */
struct mipi_lcd_driver {
	char		*name;

	int	(*probe)(struct mipi_lcd_device *mipi_dev);
	int	(*init_lcd)(struct mipi_lcd_device *mipi_dev);
	int	(*remove)(struct mipi_lcd_device *mipi_dev);
	void	(*shutdown)(struct mipi_lcd_device *mipi_dev);
	int	(*suspend)(struct mipi_lcd_device *mipi_dev);
	int	(*resume)(struct mipi_lcd_device *mipi_dev);
	int	(*reset_lcd)(struct mipi_lcd_device *mipi_dev);
};

/*Backward Transmission Type*/
enum dsim_response_id {
	DSIM_RESP_ACK 				= 0x02,	/*Acknowledge with Error Report*/
	DSIM_RESP_EOTP			= 0x08,	/*end of trasmission packet Response*/
	DSIM_RESP_GEN_SHORT_1B 	= 0x11,	/*Generic Short READ Response, 1 byte returned*/
	DSIM_RESP_GEN_SHORT_2B 	= 0x12,	/*Generic Short READ Response, 2 bytes returned*/
	DSIM_RESP_GEN_LONG 		= 0x1a,	/*Generic Long READ Response*/
	DSIM_RESP_DCS_LONG 		= 0x1c,	/*DCS Long READ Response*/
	DSIM_RESP_DCS_SHORT_1B	= 0x21,	/*DCS Short READ Response, 1 byte returned*/
	DSIM_RESP_DCS_SHORT_2B	= 0x22,	/*DCS Short READ Response, 2 bytes returned*/
};

struct dsim_header {
	unsigned char id;
	union {
		unsigned short word_count;
		unsigned short err_flag;
		struct {
			unsigned char data0;
			unsigned char data1;
		} data;
	} _inter;
	u8 ecc ;
} __attribute__ ((__packed__));

struct dsim_header_fifo {
	struct dsim_header m_oHeader;
	unsigned char* m_pPayLoadAddress;
	struct dsim_header_fifo * m_pNextFifoAddress;
};

/**
 * register mipi_lcd_driver object defined by lcd panel driver
 * to mipi-dsi driver.
 */
extern int s5p_mipi_register_lcd_driver(struct mipi_lcd_driver *lcd_drv);

/**
 * reset MIPI PHY through MIPI PHY CONTROL REGISTER
 */
extern int s5p_mipi_part_reset(struct dsim_device *dsim);

/**
 * enable MIPI D-PHY and DSI Master block.
 */
extern int s5p_mipi_init_d_phy(struct dsim_device *dsim, unsigned int enable);

/**
 * enable regulators to MIPI-DSI dphy power.
 */
extern int s5p_mipi_dphy_power(struct dsim_device *dsim, unsigned int enable);

/**
 * enable regulators to MIPI-DSI power.
 */
extern int s5p_mipi_power(struct dsim_device *dsim, unsigned int enable);


/**
 * send commands to mipi based lcd panel.
 *
 * @dsim_data: point to struct dsim_device.
 * @data_id: MIPI-DSI Command ID.
 * @data0: address to command array.
 * @data1: size of command array.
 */
extern int s5p_mipi_wr_data(struct dsim_device *dsim, unsigned int data_id,
	unsigned int data0, unsigned int data1);

/**
 * get framedone status of mipi-dsi controller.
 *
 * @dsim_data: point to struct dsim_device.
 */
extern int s5p_mipi_get_frame_done_status(struct dsim_device *dsim);

/**
 * clear framedone interrupt of mipi-dsi controller.
 *
 * @dsim_data: point to struct dsim_device.
*/
extern int s5p_mipi_clear_frame_done(struct dsim_device *dsim);

/**
 * wrapper function for changing transfer mode.
 *
 * @mode: it could be 0(LP MODE) or 1(HS MODE).
 */
extern int s5p_mipi_change_transfer_mode(struct dsim_device *dsim,
						unsigned int mode);

extern void  s5p_dsi_set_platdata(struct s5p_platform_dsim *pd);

#endif /* _MIPI_DSI_H */
