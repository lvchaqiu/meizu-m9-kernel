
/*
 * Driver for RJ64SC110 (5M camera) from Meizu Inc.
 * 
 * 1/4" 5Mp CMOS Image Sensor SoC with an Embedded Image Processor
 * supporting MIPI CSI-2
 *
 * Copyright (C) 2010, WenBin Wu<wenbinwu@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <media/rj64sc110_platform.h>
//#include <linux/smp_lock.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/gpio.h>

#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include "rj64sc110.h"

#define RJ64SC110_DRIVER_NAME	"RJ64SC110"

/* Default resolution & pixelformat. plz ref rj64sc110_platform.h */
#define DEFAULT_RES		WVGA	/* Index of resoultion */
#define DEFAUT_FPS_INDEX	RJ64SC110_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */
#define MAX_QUEUE_SIZE		256
#define MAX_MSG_LEN				256
#define MAX_MSG_BUFFER_SIZE		512


#define ACK_REG					0x0ffc		
#define STATUS_REG				0x0d0f
#define VERSION1_REG			0x0ff8
#define VERSION2_REG			0x0ffa
#define ZOOM_RATE_REG			0x0fcb


/**/
#define LEN_FOCUS_MOVE_STATUS_MASK		0x0060
#define LEN_FOCUS_CALU_STATUS_MASK		0x0100
#define LEN_STATUS_REG		0x0f80	


#define CAPTURE_HSIZE_REG			0x0fc1
#define CAPTURE_VSIZE_REG			0x0fc3

#define BRIGHTNESS_INFO_REG 		0x0fd2
#define WBMODE_INFO_REG 			0x0fd3
#define SCENE_INFO_REG				0x0fd5
#define LUMINNANCE_INFO_REG		0x0fd6
#define CONTRAST_INFO_REG			0x0fd7
#define SHARPPNESS_INFO_REG		0x0fd8
#define SATURATION_INFO_REG		0x0fd9
#define EFFECT_INFO_REG			0x0fdb
#define EXPOSURE_INFO_REG			0x0fec

#define EXPOSURE_POSITION_REG			0x0588
#define EXPOSURE_POSITION_MASK		0x0f

#define COMMAND_ACK			0x80	
#define COMMAND_NACK			0x81
#define COMMAND_BUSY			0x8f

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10 
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

/* Camera functional setting values configured by user concept */
struct rj64sc110_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int exposure_position;	/* V4L2_CID_EXPOSURE */	
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int brightness;
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int glamour;
	unsigned int scene;
	unsigned int focus_position;
	unsigned int zoom;
	unsigned int fast_shutter;	
};
enum state_type{
	STATE_CAPTURE_OFF=0,
	STATE_CAPTURE_ON,
};
enum shutter_type{
	STATE_SHUTTER_OFF=0,
	STATE_SHUTTER_ON,
};
enum init_type{
	STATE_UNINITIALIZED=0,
	STATE_INIT_PRIVEW,
	STATE_INIT_COMMAND,
};

struct rj64sc110_state {
	struct rj64sc110_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	//struct v4l2_format fmt;
	struct v4l2_mbus_framefmt fmt;
	struct v4l2_fract timeperframe;
	struct rj64sc110_userset userset;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;

	int state;
	int mode;
	int shutter;
	struct i2c_msg msg[MAX_MSG_LEN];

	int initialized;
	int stream_on;

	int ready_irq;
	struct completion   ready_completion;
	
	struct work_struct      rj64sc110_work;
	struct workqueue_struct *rj64sc110_wq;	
	spinlock_t		msg_lock;
};

static int rj64sc110_init(struct v4l2_subdev *sd, u32 val);
static int rj64sc110_init_vga(struct v4l2_subdev *sd);
static int rj64sc110_init_720p(struct v4l2_subdev *sd);
static int rj64sc110_check_focus(struct v4l2_subdev *sd);

static inline struct rj64sc110_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct rj64sc110_state, sd);
}

int rj64sc110_i2c_read_8bit(struct i2c_client *client, u16 addr)
{    
	struct i2c_msg msg[3];
	unsigned char reg1[2];
	unsigned char reg2[2];
	unsigned char val[2];
	int err = 0;
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

again:
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = reg1;

	reg1[0] = 0xff;
	reg1[1] = addr >> 8;


	msg[1].addr = client->addr;
	msg[1].flags = 0;
	msg[1].len = 1;
	msg[1].buf = reg2;

	reg2[0] = addr & 0xff;


	msg[2].addr = client->addr;
	msg[2].flags = I2C_M_RD;
	msg[2].len = 1;
	msg[2].buf = val;

	val[0] = 0;


	err = i2c_transfer(client->adapter, msg, 3);
	if (err >= 0)
	{
		return val[0];	/* Returns here on success */
	}
	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%04x\n", __func__, addr);
		retry++;
		goto again;
	}
	return -1;
}

int rj64sc110_i2c_read_16bit(struct i2c_client *client, u16 addr)
{
	int val0, val1, val;
	
	val0 = rj64sc110_i2c_read_8bit(client, addr);
	val1 = rj64sc110_i2c_read_8bit(client, addr+1);	

	if(val0<0 || val1<0)
		return -1;

	val = (val1<<8 | val0) & 0xffff;
	
	return val;
}
/*
 * RJ64SC110 register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static inline int rj64sc110_write(struct v4l2_subdev *sd, u16 addr, u16 val)
{
//	struct rj64sc110_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[1];
	unsigned char reg[4];
	int err = 0;
	int retry = 0;


	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 4;
	msg->buf = reg;

	reg[0] = addr >> 8;
	reg[1] = addr & 0xff;
	reg[2] = val >> 8;
	reg[3] = val & 0xff;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x%02x, " \
			"value: 0x%02x%02x\n", __func__, \
			reg[0], reg[1], reg[2], reg[3]);
		retry++;
		goto again;
	}
	return err;
}

#if 0
static int rj64sc110_i2c_write(struct v4l2_subdev *sd, unsigned char addr, unsigned char data)
{
//	struct rj64sc110_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2];
	struct i2c_msg msg[1] = {{client->addr, 0, 2, buf}};
	int retry_count = 5;
	int ret = 0;
	
	buf[0] = addr;
	buf[1] = data;

	while(retry_count-- > 0){
		ret = i2c_transfer(client->adapter, msg, 1);
		if(ret == 1)
			return 0;
	}

	return -EIO;
}
#endif

static int rj64sc110_write_regs_sync(struct v4l2_subdev *sd, const struct rj64sc110_reg regs[], int size)
{
	int err = -EINVAL, i,j;
	struct rj64sc110_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[100][2];

	if(size>100)
		return -EINVAL;
	for (i = 0,j=0; i < size; i++) {
		state->msg[j].addr = client->addr;
		state->msg[j].flags = client->flags & I2C_M_TEN;
		state->msg[j].len = 2;
		state->msg[j].buf = buf[i];

		buf[i][0] = regs[i].addr;
		buf[i][1] = regs[i].val;
		
		j++;	
	}
	if(j>0)
	{	
		err = i2c_transfer(client->adapter, state->msg, j);
		if (err < 0) {
			printk( "%s: write register failed\n", __func__);
			return -EIO;
		}
	}
	return 0;
}

static int rj64sc110_transfer_array_sync(struct v4l2_subdev *sd, unsigned char tx_array[][259], unsigned int tx_len)
{
	int err = -EINVAL, i,j;
	struct rj64sc110_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if(tx_len>MAX_MSG_LEN)
		return -EINVAL;
	
	for (i = 0,j=0; i < tx_len; i++) 
	{
		state->msg[j].addr = client->addr;
		state->msg[j].flags = client->flags & I2C_M_TEN;
		state->msg[j].len = tx_array[i][257]*0xff+tx_array[i][258];
		state->msg[j].buf = tx_array[i];
		j++;	
	}
	if(j>0)
	{	
		err = i2c_transfer(client->adapter, state->msg, j);
		if (err < 0) {
			printk( "%s: transfer failed\n", __func__);
			return -EIO;
		}
	}
	return 0;

}

static inline int rj64sc110_is_Initialized(struct v4l2_subdev *sd)
{
	struct rj64sc110_state *state = to_state(sd);

	return state->initialized;
}
static inline int rj64sc110_is_accept_command(struct v4l2_subdev *sd)
{
	struct rj64sc110_state *state = to_state(sd);
	if(state->initialized == STATE_INIT_COMMAND)
		return 1;
	return 0;
}
static inline int rj64sc110_wait_accept_command(struct v4l2_subdev *sd)
{
	int trycount = 20;
	
	while(!rj64sc110_is_accept_command(sd))
	{
		msleep(100);
		trycount --;
		if(trycount==0)
		{
			return 0;
		}
	}
	return 1;
}

static int rj64sc110_fps_to_step(int fps)
{
	int step;
	
	if(fps > 15)
		step = 7;//30fps
	else if(fps>10)
		step = 6;//15fps
	else if(fps>7)
		step = 5;//10fps
	else if(fps>5)
		step = 4;//7.5fps
	else if(fps > 3)
		step = 3;//5fps
	else if(fps>1)
		step = 2;//fps3
	else 
		step = 1;//fps1
		
	return step;
}

#if 0
static int rj64sc110_step_to_fps(int step)
{
	int fps;
	switch (step)
	{
		case 1:
			fps = 1;
			break;
		case 2:			
			fps = 3;
			break;
		case 3:
			fps = 5;
			break;			
		case 4:
			fps = 7;
			break;			
		case 5:
			fps = 10;
			break;			
		case 6:
			fps = 15;
			break;			
		case 7:
			fps = 30;
			break;
		default:
			fps = 15;
			break;			
	}
	return step;
}
#endif

static const char *rj64sc110_querymenu_wb_preset[] = {
	"WB auto", "WB daylight", "WB cloudy", "WB outdoor", 
	"WB incandescent light", "WB fluorescent light", "WB all light",NULL
};

static const char *rj64sc110_querymenu_effect_mode[] = {
	"Effect normal", "Effect B/W", "Effect sepia",
	"Effect nega", "Effect sepia nega", "sketch", "emboss", NULL
};

static const char *rj64sc110_querymenu_ev_bias_mode[] = {
	"-2EV", "-1EV", "0", "1EV", "2EV", NULL
};

static struct v4l2_queryctrl rj64sc110_controls[] = {
#if 	0
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 0,	/* FIXME */
	},
#endif
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(rj64sc110_regs_wb_preset)-1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = ARRAY_SIZE(rj64sc110_regs_ev_bias) - 1,
		.step = 1,
		.default_value = (ARRAY_SIZE(rj64sc110_regs_ev_bias) - 1) / 2,	/* 0 EV */
	},
	{
		.id = V4L2_CID_COLORFX,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(rj64sc110_regs_color_effect) - 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0,
		.maximum = ARRAY_SIZE(rj64sc110_contrast_map)-1,
		.step = 1,
		.default_value = (ARRAY_SIZE(rj64sc110_contrast_map) - 1) / 2,//0
	},
	{
		.id = V4L2_CID_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0,
		.maximum = ARRAY_SIZE(rj64sc110_saturation_map)-1,
		.step = 1,
		.default_value = (ARRAY_SIZE(rj64sc110_saturation_map) - 1) / 2,//0
	},
	{
		.id = V4L2_CID_BRIGHTNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Brightness",
		.minimum = 0,
		.maximum = ARRAY_SIZE(rj64sc110_brightness_map)-1,
		.step = 1,
		.default_value = (ARRAY_SIZE(rj64sc110_brightness_map) - 1) / 2,//0
	},
	{
		.id = V4L2_CID_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0,
		.maximum = ARRAY_SIZE(rj64sc110_sharpness_map)-1,
		.step = 1,
		.default_value = (ARRAY_SIZE(rj64sc110_sharpness_map) - 1) / 2,//0
	},
	{
		.id = V4L2_CID_ZOOM_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Zoom, Absolute",
		.minimum = 0,
		.maximum = ARRAY_SIZE(rj64sc110_zoom_scale_fmt)-1,
		.step = 1,
		.default_value = 0,
	},	
	{
		.id = V4L2_CID_FOCUS_AUTO,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Focus, Auto",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_FOCUS_POSITION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Focus, Position",
		.minimum = 1,
		.maximum = 49,
		.step = 1,
		.default_value = 25,
	},	
	
};

static int rj64sc110_stream_on(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct rj64sc110_state *state = to_state(sd);	
	int err = 0;
	int val = 0;
	int try_count = 5;

	v4l_info(client, "%s: stream on\n", __func__);

	if(state->stream_on == 1)
		return 0;
	err = rj64sc110_write_regs_sync(sd,  (struct rj64sc110_reg*)rj64sc110_streamon_reg, RJ64SC110_STREAM_ON_REGS);
	do{
		val = rj64sc110_i2c_read_8bit(client, 0x06FD);
		if(val == 0x00)
			break;
		msleep(20);
		try_count --;
	}while(try_count>0);

	err = rj64sc110_write_regs_sync(sd,  (struct rj64sc110_reg*)rj64sc110_streamcontinue_reg, RJ64SC110_STREAM_CONTINUE_REGS);
	state->stream_on = 1;

	return err;
}

static int rj64sc110_stream_off(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct rj64sc110_state *state = to_state(sd);	
	int err = 0;//-EINVAL;
	int val = 0;
	int try_count = 5;

	v4l_info(client, "%s:  stream off\n", __func__);
	if(state->stream_on == 0)
		return 0;

	err = rj64sc110_write_regs_sync(sd,  (struct rj64sc110_reg*)rj64sc110_streamoff_reg, RJ64SC110_STREAM_OFF_REGS);
	do{
		val = rj64sc110_i2c_read_8bit(client, 0x06FD);
		if(val & 0x01)
			break;
		msleep(20);
		try_count --;
	}while(try_count>0);

	state->stream_on = 0;
	
	return err;
}

unsigned int rj64sc110_convert_zoom_scale_rete(unsigned int zoom_val)
{
	int size = RJ64SC110_ZOOM_SCALE_COUNT;
	if(zoom_val >= size)
		return rj64sc110_zoom_scale_rate[RJ64SC110_ZOOM_SCALE_COUNT-1];
	return rj64sc110_zoom_scale_rate[zoom_val];
}
unsigned int rj64sc110_get_zoom_scale_rete(struct v4l2_subdev *sd)
{
	int zoom_rate;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	zoom_rate = rj64sc110_i2c_read_16bit(client, ZOOM_RATE_REG);
	if(zoom_rate<0)
		return rj64sc110_zoom_scale_rate[0];
	return zoom_rate;
}

int rj64sc110_set_focus_position(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	struct rj64sc110_state *state = to_state(sd);	
	
	if(ctrl->value < 1 || ctrl->value>49)
	{
		return -EINVAL;
	}
	rj64sc110_focus_position[1].val = 1;
	rj64sc110_focus_position[2].val = ctrl->value;
	rj64sc110_focus_position[3].val = ctrl->value;
	rj64sc110_focus_position[4].val = ctrl->value;
	rj64sc110_focus_position[5].val = ctrl->value;
	rj64sc110_focus_position[6].val = ctrl->value;
	err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *)  rj64sc110_focus_position, RJ64SC110_FOCUS_POSITION_REGS);
	
	if(err<0)
		return -EINVAL;
	state->userset.focus_position = ctrl->value;
	return 0;
}

int rj64sc110_set_exposure(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct rj64sc110_state *state = to_state(sd);
	int err = -EINVAL;
	
	if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_regs_ev_bias)-1))
		return -EINVAL;
	
	err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *)  rj64sc110_regs_ev_bias[ctrl->value], RJ64SC110_EV_REGS);
	if(err<0)
		return -EINVAL;
	state->userset.exposure_bias = ctrl->value;
	return 0;
}
static int rj64sc110_set_exposure_position(struct v4l2_subdev *sd,  struct v4l2_control *ctrl)
{
	struct rj64sc110_state *state = to_state(sd);
	int err = -EINVAL;
	
	if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_regs_exp_position)-1))
		return -EINVAL;
	
	err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *)  rj64sc110_regs_exp_position[ctrl->value], RJ64SC110_EXP_POSITION_REGS);
	if(err<0)
		return -EINVAL;
	state->userset.exposure_position = ctrl->value;
	return 0;
}

static int rj64sc110_wait_ack(struct v4l2_subdev *sd)
{
	int retry_count = 5;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 val;
	int err;
	
	while (retry_count--) {
		val = rj64sc110_i2c_read_8bit(client, ACK_REG);
		if (val < 0) {
			pr_err("%s:read ACK_REG fail\n", __func__);
			err = -EIO;
			break;
		} else if (val == 0x80) {
			err = 0;
			break;
		} else if (val == 0x81) {
			pr_debug("%s:fail\n", __func__);
			err = -EINVAL;
			break;
		} else {
			msleep(10);
		}	
	}

	if (!retry_count) err = -EINVAL;

	return err;
}

int rj64sc110_set_wb_preset(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct rj64sc110_state *state = to_state(sd);
	int err = -EINVAL;
	
	if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_regs_wb_preset)-1))
		return -EINVAL;

	rj64sc110_check_focus(sd);
	
	err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *)  rj64sc110_regs_wb_preset[ctrl->value], RJ64SC110_WB_REGS);
	if(err<0)
		return -EINVAL;
	state->userset.manual_wb = ctrl->value;
	
	err = rj64sc110_wait_ack(sd);
	
	return err;
}
int rj64sc110_set_image_quality(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct rj64sc110_state *state = to_state(sd);
	int err = -EINVAL;	

	switch(ctrl->id)
	{
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CAMERA_BRIGHTNESS:
	{
		dev_dbg(&client->dev, "%s: V4L2_CID_SHARPNESS, ctrl->value==%d\n", __func__,ctrl->value);
		if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_brightness_map)-1))
			return -EINVAL;
		rj64sc110_image_quality_auto[1].val = rj64sc110_brightness_map[ctrl->value];
		rj64sc110_image_quality_auto[2].val = rj64sc110_contrast_map[state->userset.contrast];
		rj64sc110_image_quality_auto[3].val = rj64sc110_sharpness_map[state->userset.sharpness];
		rj64sc110_image_quality_auto[4].val = rj64sc110_saturation_map[state->userset.saturation];
		rj64sc110_image_quality_auto[5].val = state->userset.fast_shutter;		
		break;	
	}
	case V4L2_CID_CONTRAST:
	{
		dev_dbg(&client->dev, "%s: V4L2_CID_CONTRAST, ctrl->value==%d\n", __func__,ctrl->value);
		if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_contrast_map)-1))
			return -EINVAL;
		rj64sc110_image_quality_auto[1].val = rj64sc110_brightness_map[state->userset.brightness];
		rj64sc110_image_quality_auto[2].val = rj64sc110_contrast_map[ctrl->value];
		rj64sc110_image_quality_auto[3].val = rj64sc110_sharpness_map[state->userset.sharpness];
		rj64sc110_image_quality_auto[4].val = rj64sc110_saturation_map[state->userset.saturation];
		rj64sc110_image_quality_auto[5].val = state->userset.fast_shutter;		
		break;
	}
	case V4L2_CID_SATURATION:
	{
		dev_dbg(&client->dev, "%s: V4L2_CID_SATURATION, ctrl->value==%d\n", __func__,ctrl->value);
		if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_saturation_map)-1))
			return -EINVAL;
		rj64sc110_image_quality_auto[1].val = rj64sc110_brightness_map[state->userset.brightness];
		rj64sc110_image_quality_auto[2].val = rj64sc110_contrast_map[state->userset.contrast];
		rj64sc110_image_quality_auto[3].val = rj64sc110_sharpness_map[state->userset.sharpness];
		rj64sc110_image_quality_auto[4].val = rj64sc110_saturation_map[ctrl->value];
		rj64sc110_image_quality_auto[5].val = state->userset.fast_shutter;		
		break;
	}
	case V4L2_CID_SHARPNESS:
	{
		dev_dbg(&client->dev, "%s: V4L2_CID_SHARPNESS, ctrl->value==%d\n", __func__,ctrl->value);
		if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_sharpness_map)-1))
			return -EINVAL;
		rj64sc110_image_quality_auto[1].val = rj64sc110_brightness_map[state->userset.brightness];
		rj64sc110_image_quality_auto[2].val = rj64sc110_contrast_map[state->userset.contrast];
		rj64sc110_image_quality_auto[3].val = rj64sc110_sharpness_map[ctrl->value];	
		rj64sc110_image_quality_auto[4].val = rj64sc110_saturation_map[state->userset.saturation];
		rj64sc110_image_quality_auto[5].val = state->userset.fast_shutter;	
		break;
	}
	default:
		return -EINVAL;
	}

	err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *)  rj64sc110_image_quality_auto, RJ64SC110_IMAGE_QUALITY_REGS);
	if(err < 0)
		return -EINVAL;
	switch(ctrl->id)
	{
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CAMERA_BRIGHTNESS:
		state->userset.brightness= ctrl->value;	
		break;	
	case V4L2_CID_CONTRAST:
		state->userset.contrast= ctrl->value;	
		break;
	case V4L2_CID_SATURATION:
		state->userset.saturation= ctrl->value;	
		break;
	case V4L2_CID_SHARPNESS:
		state->userset.sharpness= ctrl->value;	
		break;
	default:
		break;
	}
	return 0;
}
int rj64sc110_set_color_effect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct rj64sc110_state *state = to_state(sd);
	int err = -EINVAL;	

	if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_regs_color_effect)-1))
		return -EINVAL;

	err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *)  rj64sc110_regs_color_effect[ctrl->value], RJ64SC110_EFFECT_REGS);
	if(err < 0)
		return -EINVAL;

	state->userset.effect= ctrl->value;	
	return 0;
}
int rj64sc110_set_scenemode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	struct rj64sc110_state *state = to_state(sd);
	
	if(ctrl->value < 0 || ctrl->value > (ARRAY_SIZE(rj64sc110_regs_scene_mode)-1))
		return -EINVAL;
	
	err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *) rj64sc110_regs_scene_mode[ctrl->value], RJ64SC110_SCENE_MODE_REGS);
	if(err < 0)
		return -EINVAL;

	state->userset.scene= ctrl->value;
	return 0;
}

static int rj64sc110_check_focus(struct v4l2_subdev *sd)
{
	unsigned int state =0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	int trycount = 10;
	
	do{
		state = rj64sc110_i2c_read_16bit(client, LEN_STATUS_REG);
		if(state<0)
		{
			return -1;
		}
		if(!(state & LEN_FOCUS_MOVE_STATUS_MASK))
			break;
		trycount --;
		msleep(200);	
	}while(trycount);
	
	if(trycount <= 0){
		printk("%s: cancel auto focus!\n", __func__);
		//cancel auto focus
		rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *)rj64sc110_focus_auto_cancel, RJ64SC110_CANCEL_AUTO_FOCUS_REGS);
		msleep(100);
	}
	if(state & LEN_FOCUS_CALU_STATUS_MASK)
	{
		printk("%s: wait auto focus complete!\n", __func__);
		//wait fouce calulate finish
		msleep(100);
	}	
	return 0;
}

int rj64sc110_set_focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;

	/*check auto focus status*/
	rj64sc110_check_focus(sd);

	err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *) rj64sc110_focus_auto, RJ64SC110_AUTO_FOCUS_REGS);
	return err;
}

const char **rj64sc110_ctrl_get_menu(u32 id)
{
	switch (id) {
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return rj64sc110_querymenu_wb_preset;

	case V4L2_CID_COLORFX:
	case V4L2_CID_CAMERA_EFFECT:
		return rj64sc110_querymenu_effect_mode;

	case V4L2_CID_EXPOSURE:
		return rj64sc110_querymenu_ev_bias_mode;

	default:
		return (const char **)v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *rj64sc110_find_qctrl(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rj64sc110_controls); i++)
		if (rj64sc110_controls[i].id == id)
			return &rj64sc110_controls[i];

	return NULL;
}

static int rj64sc110_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rj64sc110_controls); i++) {
		if (rj64sc110_controls[i].id == qc->id) {
			memcpy(qc, &rj64sc110_controls[i], \
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int rj64sc110_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	rj64sc110_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, rj64sc110_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int rj64sc110_s_crystal_freq(struct v4l2_subdev *sd, u32  freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int rj64sc110_s_shutter(struct v4l2_subdev *sd, int on_off)
{
	int err = 0;
	int try_count=10;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct rj64sc110_state *state = to_state(sd);
	
	if(on_off)
	{
		if(state->shutter == STATE_SHUTTER_OFF)
		{	
			dev_info(&client->dev, "Shutter_ON\n\n");
			err = rj64sc110_write_regs_sync(sd,  rj64sc110_Shutter_ON, RJ64SC110_SHUTTER_ON_REGS);
			//check command result
			while(rj64sc110_i2c_read_8bit(client, ACK_REG)==COMMAND_NACK)
			{
				err = rj64sc110_write_regs_sync(sd,  rj64sc110_Shutter_ON, RJ64SC110_SHUTTER_ON_REGS);
				if(try_count<0)
					break;
				try_count--;
				msleep(100);
			}
			state->shutter = STATE_SHUTTER_ON;
		}
	}else{
		if(state->shutter == STATE_SHUTTER_ON)
		{
			dev_info(&client->dev, "Shutter_OFF\n\n");
			err = rj64sc110_write_regs_sync(sd,  rj64sc110_Shutter_OFF, RJ64SC110_SHUTTER_OFF_REGS);
			//check command result
			while(rj64sc110_i2c_read_8bit(client, ACK_REG)==COMMAND_NACK)
			{
				err = rj64sc110_write_regs_sync(sd,  rj64sc110_Shutter_OFF, RJ64SC110_SHUTTER_OFF_REGS);
				if(try_count<0)
					break;
				try_count--;
				msleep(100);
			}
			state->shutter = STATE_SHUTTER_OFF;
		}
	}
	msleep(50);
	return err;
}
static int rj64sc110_init_vga(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct rj64sc110_state *state = to_state(sd);
	int err = -EINVAL;
	int try_count=10;
	int fps_step = rj64sc110_fps_to_step(state->fps);
    
	rj64sc110_vga_size[6].val = fps_step;
	rj64sc110_vga_size[7].val = fps_step;
	
	do
	{
		err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *)  rj64sc110_vga_size, RJ64SC110_VGA_SIZE_REGS);
		if( rj64sc110_i2c_read_8bit(client, ACK_REG) == COMMAND_ACK )
			break;
		try_count--;
		msleep(20);
	}while(try_count);

	if (err < 0)
		goto out;

	msleep(50);
	state->state = STATE_CAPTURE_OFF;		
	return 0;
out:
	v4l_info(client, "%s: rj64sc110_init_vga failed\n", __func__);
	return err;
}
static int rj64sc110_init_720p(struct v4l2_subdev *sd)
{
	int err  = 0;
	int try_count=10;	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct rj64sc110_state *state = to_state(sd);
	int fps_step = rj64sc110_fps_to_step(state->fps);

	do
	{
			rj64sc110_init_720p_reg[6].val = fps_step;
			rj64sc110_init_720p_reg[7].val = fps_step;
			err = rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg*)rj64sc110_init_720p_reg, RJ64SC110_INIT_720P_REGS);
			if( rj64sc110_i2c_read_8bit(client, ACK_REG) == COMMAND_ACK )
				break;
			try_count--;
			msleep(20);		
	}while(try_count);

	msleep(50);
	return err;
}

//static int rj64sc110_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
static int rj64sc110_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int err = 0;
	struct rj64sc110_state *state = to_state(sd);

	state->fmt = *fmt;
	if (rj64sc110_is_accept_command(sd)) {
		switch (fmt->reserved[0]) {
			case V4L2_CAMERA_PREVIEW:
				/*shutter off*/
				switch(state->mode) {
					case V4L2_CAMERA_PREVIEW:
					case V4L2_CAMERA_RECORD:
						rj64sc110_init_vga(sd);
						break;
					case V4L2_CAMERA_CAPTURE:
						rj64sc110_s_shutter(sd, 0);
						break;
				}
				state->mode = V4L2_CAMERA_PREVIEW;
				state->pix.width = 960;
				state->pix.height = 720;				
				break;
			case V4L2_CAMERA_RECORD:
				/*shutter off*/
				switch(state->mode) {
					case V4L2_CAMERA_PREVIEW:
					case V4L2_CAMERA_RECORD:
						break;
					case V4L2_CAMERA_CAPTURE:
						rj64sc110_s_shutter(sd, 0);
						break;
				}				
				rj64sc110_init_720p(sd);
				state->mode = V4L2_CAMERA_RECORD;
				state->state = STATE_CAPTURE_ON;
				state->pix.width  = 1280;
				state->pix.height = 720;			
				break;
			case V4L2_CAMERA_CAPTURE:
			{
				/*check auto focus status*/
				rj64sc110_check_focus(sd);	
				switch(state->mode)
				{
					case V4L2_CAMERA_RECORD:
						rj64sc110_init_vga(sd);
					default:
						break;
				}
				/*shutter on*/
				rj64sc110_s_shutter(sd, 1);
				state->mode = V4L2_CAMERA_CAPTURE;
				state->state = STATE_CAPTURE_ON;
				state->pix.width = 2592;
				state->pix.height = 1944;	
				break;
			}
		}
	} else {
		switch(fmt->reserved[0]) {
			case V4L2_CAMERA_RECORD:
				state->mode = V4L2_CAMERA_RECORD;
				state->pix.width  = 1280;
				state->pix.height = 720;
				break;
			default:
				state->mode = V4L2_CAMERA_PREVIEW;
				state->pix.width = 960;
				state->pix.height = 720;
				break;				
		}
	}
	fmt->width = state->pix.width;
	fmt->height = state->pix.height;	
	return err;
}
static int rj64sc110_enum_framesizes(struct v4l2_subdev *sd, 
					struct v4l2_frmsizeenum *fsize)
{
	int err = 0;

	return err;
}

static int rj64sc110_enum_frameintervals(struct v4l2_subdev *sd, 
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int rj64sc110_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int err = 0;

	return err;
}

static int rj64sc110_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct rj64sc110_state *state = to_state(sd);
	
	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	param->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	param->parm.capture.capturemode = state->mode;
	param->parm.capture.timeperframe.numerator = 1;
	param->parm.capture.timeperframe.denominator = state->fps;

	return 0;
}

static int rj64sc110_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct rj64sc110_state *state = to_state(sd);
	int fps = param->parm.capture.timeperframe.denominator;

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;	
	if(state->fps == fps)
		return 0;
	state->fps = fps;
	if(rj64sc110_is_accept_command(sd))
	{
		if(state->mode == V4L2_CAMERA_PREVIEW)
		{
			rj64sc110_init_vga(sd);	
		}else if(state->mode == V4L2_CAMERA_RECORD){
			rj64sc110_init_720p(sd);
		}
	}
	return 0;
}

static int rj64sc110_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct rj64sc110_state *state = to_state(sd);
	struct rj64sc110_userset userset = state->userset;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
	case V4L2_CID_CAMERA_WHITE_BALANCE:		
		ctrl->value =userset.manual_wb;
		break;
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		ctrl->value = userset.wb_temp;
		break;
	case V4L2_CID_COLORFX:
	case V4L2_CID_CAMERA_EFFECT:
		ctrl->value = userset.effect;
		break;
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CAMERA_BRIGHTNESS:
		ctrl->value =  userset.brightness;
		break;		
	case V4L2_CID_CONTRAST:
		ctrl->value = userset.contrast;
		break;
	case V4L2_CID_SATURATION:
		ctrl->value =  userset.saturation;
		break;
	case V4L2_CID_SHARPNESS:
		ctrl->value =  userset.sharpness;
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		ctrl->value =  userset.zoom;
		break;
	case V4L2_CID_FOCUS_POSITION:
		ctrl->value =  userset.focus_position;
		break;
	case V4L2_CID_CAMERA_EXPOSURE_POSITION:
		ctrl->value =  userset.exposure_position;
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE:
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
	case V4L2_CID_CAMERA_VT_MODE:
	case V4L2_CID_CAMERA_VGA_BLUR:
	case V4L2_CID_CAMERA_RETURN_FOCUS:
	case V4L2_CID_ESD_INT:
	case V4L2_CID_CAMERA_GET_ISO:
	case V4L2_CID_CAMERA_GET_SHT_TIME:
	case V4L2_CID_CAMERA_SENSOR_MODE:
	case V4L2_CID_CAMERA_GET_FLASH_ONOFF:
	case V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK:
	case V4L2_CID_CAMERA_SET_GAMMA:
	case V4L2_CID_CAMERA_BATCH_REFLECTION:
		ctrl->value =  0;
		break;
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT_FIRST:
		ctrl->value =  1;//alway return success(0-fail, 1-success, 2-cancel)
		break;
	default:
		dev_err(&client->dev, "%s: no such control, ctrl->id=%x, ctrl->value==%d\n", __func__,ctrl->id,ctrl->value);
		return -EINVAL;
		break;
	}
	if(ctrl->value<0)
		return -EINVAL;
	return 0;
}

static int rj64sc110_s_stream(struct v4l2_subdev *sd, int enable)
{
	
	if(rj64sc110_wait_accept_command(sd))
	{
		if(enable)
		{
			rj64sc110_stream_on(sd);
		}else{
			rj64sc110_stream_off(sd);
		}
		return 0;
	}
	return -1;
}

static int rj64sc110_load_fw(struct v4l2_subdev *sd)
{
	struct rj64sc110_state *state = to_state(sd);

	queue_work(state->rj64sc110_wq, &state->rj64sc110_work);

	return 0;
}
static int rj64sc110_s_gpio(struct v4l2_subdev *sd, u32 val)
{

	return 0;
}
static int rj64sc110_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}
static long rj64sc110_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	return 0;
}
static int rj64sc110_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
#ifdef RJ64SC110_COMPLETE
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct rj64sc110_state *state = to_state(sd);
	int err = -EINVAL;

	if(!rj64sc110_wait_accept_command(sd))
	{
		return -EINVAL;
	}
	if(state->shutter == STATE_SHUTTER_ON)
	{
		return -EINVAL;	
	}
	switch (ctrl->id) {

	case V4L2_CID_EXPOSURE:
		dev_info(&client->dev, "%s: V4L2_CID_EXPOSURE, ctrl->value==%d\n", \
			__func__,ctrl->value);
		err = rj64sc110_set_exposure(sd, ctrl);
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		dev_info(&client->dev, "%s: V4L2_CID_WHITE_BALANCE_PRESET, ctrl->value==%d\n", \
			__func__,ctrl->value);
		err =rj64sc110_set_wb_preset(sd, ctrl);
		break;
	case V4L2_CID_COLORFX:
	case V4L2_CID_CAMERA_EFFECT:
		dev_info(&client->dev, "%s: V4L2_CID_COLORFX, ctrl->value==%d\n", __func__,ctrl->value);
		err = rj64sc110_set_color_effect(sd, ctrl);
		break;
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_SHARPNESS:
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CAMERA_BRIGHTNESS:
		err = rj64sc110_set_image_quality(sd, ctrl);
		break;
	case V4L2_CID_SCENEMODE:
	case V4L2_CID_CAMERA_SCENE_MODE:
		dev_dbg(&client->dev, "%s: V4L2_CID_SCENEMODE, ctrl->value==%d\n", __func__,ctrl->value);
		err = rj64sc110_set_scenemode(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		if(ctrl->value != AUTO_FOCUS_ON)
			return 0;
	case V4L2_CID_FOCUS_AUTO:
		dev_dbg(&client->dev, "%s: V4L2_CID_FOCUS_AUTO, ctrl->value==%d\n", __func__,ctrl->value);
		err = rj64sc110_set_focus(sd,ctrl);
		break;
	case V4L2_CID_FOCUS_POSITION:
		dev_dbg(&client->dev, "%s: V4L2_CID_FOCUS_POSITION, ctrl->value==%d\n", __func__,ctrl->value);
		err = rj64sc110_set_focus_position(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_EXPOSURE_POSITION:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_EXPOSURE_POSITION, ctrl->value==%d\n", __func__,ctrl->value);
		err = rj64sc110_set_exposure_position(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE:
	case V4L2_CID_CAMERA_VT_MODE:
	case V4L2_CID_CAMERA_VGA_BLUR:
	case V4L2_CID_CAMERA_RETURN_FOCUS:
	case V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK:
	case V4L2_CID_CAMERA_SET_GAMMA:
	case V4L2_CID_CAMERA_BATCH_REFLECTION:
	case V4L2_CID_CAM_PREVIEW_ONOFF:
		err = 0;
		break;
	default:
		dev_err(&client->dev, "%s: no such control, ctrl->id=%x, ctrl->value==%d\n", __func__,ctrl->id,ctrl->value);
		break;
	}
	if (err < 0)
		goto out;

	return 0;	
out:
	dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed, ctrl->value==%d\n", __func__,ctrl->value);
	return err;
#else
	return 0;
#endif
}

static int rj64sc110_init(struct v4l2_subdev *sd, u32 val)
{
	int err=0;

	struct rj64sc110_state *state = to_state(sd);	

	state->userset.exposure_bias = (ARRAY_SIZE(rj64sc110_regs_ev_bias) - 1) / 2;
	state->userset.ae_lock = 0;
	state->userset.awb_lock = 0;
	state->userset.auto_wb = 0;
	state->userset.manual_wb = 0;
	state->userset.wb_temp = 0;	
	state->userset.effect = 0;
	state->userset.brightness= (ARRAY_SIZE(rj64sc110_brightness_map) - 1) / 2;
	state->userset.contrast = (ARRAY_SIZE(rj64sc110_contrast_map) - 1) / 2;
	state->userset.saturation = (ARRAY_SIZE(rj64sc110_saturation_map) - 1) / 2;
	state->userset.sharpness = (ARRAY_SIZE(rj64sc110_sharpness_map) - 1) / 2;
	state->userset.focus_position = 25;
	state->userset.glamour = 0;	
	state->userset.zoom = 0;
	state->userset.scene = 0;
	state->userset.fast_shutter = 0;

	state->fps = 15;
	state->initialized = STATE_UNINITIALIZED;
	state->stream_on = 1;
	state->state = STATE_CAPTURE_OFF;
	state->mode = V4L2_CAMERA_PREVIEW;
	state->shutter = STATE_SHUTTER_OFF;

	err = rj64sc110_load_fw(sd);
	return err;
}

static const struct v4l2_subdev_core_ops rj64sc110_core_ops = {
	.init = rj64sc110_init,	/* initializing API */
	.queryctrl = rj64sc110_queryctrl,
	.querymenu = rj64sc110_querymenu,
	.g_ctrl = rj64sc110_g_ctrl,
	.s_ctrl = rj64sc110_s_ctrl,
	.load_fw = rj64sc110_load_fw,
	.s_gpio = rj64sc110_s_gpio,
	.reset = rj64sc110_reset,
	.ioctl = rj64sc110_ioctl,	
};

static const struct v4l2_subdev_video_ops rj64sc110_video_ops = {
	.s_crystal_freq = rj64sc110_s_crystal_freq,
	.s_mbus_fmt = rj64sc110_s_fmt,
	.enum_framesizes = rj64sc110_enum_framesizes,
	.enum_frameintervals = rj64sc110_enum_frameintervals,
	.try_mbus_fmt = rj64sc110_try_fmt,
	.g_parm = rj64sc110_g_parm,
	.s_parm = rj64sc110_s_parm,
	.s_stream = rj64sc110_s_stream,
};

static const struct v4l2_subdev_ops rj64sc110_ops = {
	.core = &rj64sc110_core_ops,
	.video = &rj64sc110_video_ops,
};

static void rj64sc110_handle_work(struct work_struct *work)
{
	int status;
	int try_count;
	struct rj64sc110_state *state = container_of(work, struct rj64sc110_state, rj64sc110_work);
	struct i2c_client *client = v4l2_get_subdevdata(&state->sd);
	struct v4l2_subdev *sd = &state->sd;

	v4l_info(client, "%s: camera initialization start\n", __func__);

	state->initialized = STATE_UNINITIALIZED;
	try_count = 5;
	do {
		if (rj64sc110_transfer_array_sync(sd, rj64sc110_init_reg_char, RJ64SC110_INIT_REGS_CHAR) == 0)
			break;
		try_count --;
		msleep(20);
	} while (try_count);

	state->initialized = STATE_INIT_PRIVEW;
	enable_irq(state->ready_irq);
	init_completion(&state->ready_completion);
	try_count = 5;
	do {	
		if (rj64sc110_transfer_array_sync(sd, rj64sc110_scs_reg_char, RJ64SC110_SCS_REGS_CHAR) == 0)
			break;
		try_count--;
		msleep(20);
	} while (try_count);

	try_count = 5;
	do {		
		if(rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *) rj64sc110_start_reg, RJ64SC110_START_REGS) == 0)
			break;
		try_count--;
		msleep(20);
	} while (try_count);

	v4l_info(client,"%s: wait for camera ready!\n", __func__);
	
	status = wait_for_completion_interruptible_timeout(&state->ready_completion, 5*HZ);
	disable_irq(state->ready_irq);
	
	if (status > 0) {
		struct v4l2_control ctrl;

		if (state->mode == V4L2_CAMERA_PREVIEW) {
			state->state = STATE_CAPTURE_ON;/*fouce initial to vga mode*/
			rj64sc110_init_vga(sd);	
		} else {
			rj64sc110_init_720p(sd);
		}
		/*set image quality*/
		ctrl.id = V4L2_CID_SATURATION;
		ctrl.value = state->userset.saturation + 1;//+1 saturation
		state->userset.fast_shutter = 0;//enable fast shutter
		rj64sc110_set_image_quality(sd, &ctrl);

		/*Color bar*/
		//rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *) rj64sc110_color_bar_reg, RJ64SC110_COLOR_BAR_REGS);

		/*set full focus mode*/
		rj64sc110_write_regs_sync(sd, (struct rj64sc110_reg *) rj64sc110_focus_mode, RJ64SC110_ZOOM_MODE_REGS);

		v4l_info(client, "%s: camera initialization finished\n", __func__);
		state->initialized = STATE_INIT_COMMAND;
	} else {
		v4l_info(client, "%s: camera initialization failed\n", __func__);
	}
}


static irqreturn_t rj64sc110_handle_irq(int irq, void *handle)
{
	struct rj64sc110_state *state = (struct rj64sc110_state *)handle;
	complete(&state->ready_completion);
	return IRQ_HANDLED;
}
/*
 * rj64sc110_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */

static int rj64sc110_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct rj64sc110_state *state;
	struct v4l2_subdev *sd;	
	int err;
	
	state = kzalloc(sizeof(struct rj64sc110_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, RJ64SC110_DRIVER_NAME);
	
	spin_lock_init(&state->msg_lock);
	INIT_WORK(&state->rj64sc110_work, rj64sc110_handle_work);

	state->rj64sc110_wq = create_singlethread_workqueue("rj64sc110_wq");
	if(!state->rj64sc110_wq){
		printk("Failed to setup workqueue - rj64sc110_wq \n");
		 goto error0;
	}
	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &rj64sc110_ops);

	init_completion(&state->ready_completion);
	/* Enable INT Interrupt request */
	state->ready_irq = gpio_to_irq( client->irq);
	err = request_irq(state->ready_irq, rj64sc110_handle_irq, 
	    IRQF_TRIGGER_RISING, client->name, state);
	if (err != 0) {
		printk(KERN_ERR "Failed to request IRQ for camera INT\n");
		goto error1;
	}
	disable_irq(state->ready_irq);
	
	dev_info(&client->dev, "rj64sc110 has been probed\n");
	return 0;
error1:
	destroy_workqueue(state->rj64sc110_wq);	
	v4l2_device_unregister_subdev(sd);
error0:
	kfree(state);
	return -ENOMEM;
}


static int rj64sc110_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct rj64sc110_state *state = to_state(sd);

	flush_workqueue(state->rj64sc110_wq);
	destroy_workqueue(state->rj64sc110_wq);
	free_irq(state->ready_irq, state);
	
	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	dev_info(&client->dev, "rj64sc110 has been removed\n");	
	return 0;
}

static const struct i2c_device_id rj64sc110_id[] = {
	{ RJ64SC110_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, rj64sc110_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = RJ64SC110_DRIVER_NAME,
	.probe = rj64sc110_probe,
	.remove = rj64sc110_remove,
	.id_table = rj64sc110_id,
};

MODULE_DESCRIPTION("Sharp Electronics RJ64SC110 5M camera driver");
MODULE_AUTHOR("Liu Yi Hui<LiuYiHui@meizu.com>");
MODULE_AUTHOR("WenBin Wu<wenbinwu@meizu.com>");
MODULE_LICENSE("GPL");

