/* drivers/input/keyboard/m9w_i2c_rmi.c
 *
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author:  zhengkl <zhengkl@meizu.com> lvcha qiu <lvcha@meizu.com>
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
 * Inital code : Mar 10 , 2010
 *
 */
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/m9w_i2c_rmi.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/firmware.h>

#define  M9W_MAX_FINGERS   10
#define  M9W_DETECT_DELTA  4		/*defualt:4*/

#define BTN_F19 BTN_0
#define BTN_F30 BTN_0
#define SCROLL_ORIENTATION REL_Y

/* Register: EGR_0 */
#define EGR_PINCH_REG   0
#define EGR_PINCH     (1 << 6)
#define EGR_PRESS_REG     0
#define EGR_PRESS     (1 << 5)
#define EGR_FLICK_REG     0
#define EGR_FLICK     (1 << 4)
#define EGR_EARLY_TAP_REG 0
#define EGR_EARLY_TAP   (1 << 3)
#define EGR_DOUBLE_TAP_REG  0
#define EGR_DOUBLE_TAP    (1 << 2)
#define EGR_TAP_AND_HOLD_REG  0
#define EGR_TAP_AND_HOLD  (1 << 1)
#define EGR_SINGLE_TAP_REG  0
#define EGR_SINGLE_TAP    (1 << 0)
/* Register: EGR_1 */
#define EGR_PALM_DETECT_REG 1
#define EGR_PALM_DETECT   (1 << 0)

const static int touch_button_key_code[] = { 158, 139};   /* 158 for back, and 139 for menu */

struct synaptics_function_descriptor {
	__u8 queryBase;
	__u8 commandBase;
	__u8 controlBase;
	__u8 dataBase;
	__u8 intSrc;
#define FUNCTION_VERSION(x) ((x >> 5) & 3)
#define INTERRUPT_SOURCE_COUNT(x) (x & 7)

	__u8 functionNumber;
};
#define FD_ADDR_MAX 0xE9
#define FD_ADDR_MIN 0x05
#define FD_BYTE_COUNT 6

#define MIN_ACTIVE_SPEED 5

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h);
static void synaptics_rmi4_late_resume(struct early_suspend *h);
#endif

#define DEV_ATTR(_pre, _name, _mode) \
    DEVICE_ATTR(_pre##_##_name, _mode, _pre##_##_name##_show, _pre##_##_name##_store)

static BLOCKING_NOTIFIER_HEAD(notifier_list);	//added by lvcha

static inline void synaptic_reliable_read_reg(struct synaptics_rmi4 *ts, __u8 addr, __u8 *value)
{
	int retry = 3;
	int ret;
	while (retry-- > 0) {  
		ret = i2c_smbus_read_byte_data(ts->client, addr);		
		if (ret >= 0) {
			*value = (u8)ret;
			break;
		} else {            
			*value = (u8)ret;
			printk("%s %d, read addr error! get ret value:%d\n", __func__, __LINE__, ret);
			msleep(5);
		}
	}
	return;
}

static inline int synaptic_reliable_write_reg( struct synaptics_rmi4 *ts, int addr, __u8 value)
{
	int ret = 0;
	int retry = 3;
	while (retry-- > 0) {  
		ret = i2c_smbus_write_byte_data(ts->client, addr, value); 
		if (ret >= 0) {
			ret = i2c_smbus_read_byte_data(ts->client, addr);
			if (ret == value) {
				break;
			} else {
				printk("synaptic_reliable_write_reg at addr:%d, value:%d, but read value:%d!\n", addr, value, ret);
				continue;
			}
		} else
			msleep(5);
	}
	if (retry < 0) {
		printk("synaptic_reliable_write_reg at addr:%d, value:%d error!\n", addr, value);
	}
	return ret;
}

static ssize_t synaptics_rmi4_reg_address_show(struct device *dev,
                                         struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4 *ts = dev_get_drvdata(dev);  
	__u8 value = 0;
	if ((ts->dump_reg_address>=0) && ts->dump_reg_address<=255) {    
		synaptic_reliable_read_reg(ts, ts->dump_reg_address, &value);    
		return sprintf(buf, "dump_reg_address:0x%x, value:%d\n", ts->dump_reg_address, value);
	} else {
		return sprintf(buf, "dump_reg_address not set!\n");
	}
}

static ssize_t synaptics_rmi4_reg_address_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count)
{
	struct synaptics_rmi4 *ts = dev_get_drvdata(dev);    
	char addr_str[33];
	char value_str[33];
	unsigned long addr;    
	unsigned long value;    
	int error;

	sscanf(buf, "%s %s", addr_str, value_str);
	printk("addr_str:%s;value_str:%s\n", addr_str, value_str);

	error = strict_strtoul(addr_str, 10, &addr);
	if (error) {
		printk("strict_strtoul addr error\n");    
		return error;
	}    
	error = strict_strtoul(value_str, 10, &value);    
	if (error) {
		printk("strict_strtoul value error\n");    
		return error;
	}

	printk("addr:%ld, value:%ld\n", addr, value);
	if (addr>=0 && addr<=255) {
		ts->dump_reg_address = addr;    
		if (value >= 0 && value <=255)
			synaptic_reliable_write_reg(ts, addr, value);
	}

	return count;
}
DEV_ATTR(synaptics_rmi4, reg_address, 0664);

static void synaptics_report_buttons(struct synaptics_rmi4 *ts,
                                     __u8 *data,
                                     __u8 points_supported,
                                     int base)
{
	int b;
	struct input_dev *dev = ts->input_dev;
	ts->is_button_touched = 0;
	for (b = 0; b < points_supported; ++b) {
		int button = (data[b>>3] >> (b & 0x7)) & 1;
		ts->is_button_touched |= button;
		input_report_key(dev, touch_button_key_code[b], button);
	}
}

static int synaptics_rmi4_work_handle(struct synaptics_rmi4 *ts)
{
	int ret;
	int pressed_cnt;
	int is_report_again = 0;
	static int last_pressed_cnt = 0;
	unsigned char *interrupt = NULL;

	ret = i2c_transfer(ts->client->adapter, ts->data_i2c_msg, 2);  

	if (ret < 0) {
		printk(KERN_ERR "%s: i2c_transfer failed\n", __func__);
		return -EDEADLK;
	}
	
	interrupt = &ts->data[ts->f01.data_offset + 1];
	
report_press_finger_again:
	if ( !(ts->is_button_touched) && ts->hasF11 &&
		(interrupt[ts->f11.interrupt_offset] & ts->f11.interrupt_mask)) {

		__u8 *f11_data = &ts->data[ts->f11.data_offset];
		int f;
		__u8 finger_status_reg = 0;
		__u8 fsr_len = (ts->f11.points_supported + 3)>>2;
		ts->is_abs_touched = 0;

		pressed_cnt = 0;
		for (f = 0; f < ts->f11.points_supported  &&  f < ts->finger_count; ++f) {
			__u8 finger_status;
			if (!(f % 4))
				finger_status_reg = f11_data[f>>2];

			finger_status = (finger_status_reg >> ((f &0x3)<<1)) & 3;

			if (finger_status == 1 || finger_status == 2) {
				__u8 reg = fsr_len + 5 * f;
				__u8 *finger_reg = &f11_data[reg];
				u12 x = (finger_reg[0] << 4) | (finger_reg[2] & 0xf);
				u12 y = (finger_reg[1] << 4) | (finger_reg[2] >> 4);
				u4 wx = finger_reg[3] & 0xf;
				u4 wy = finger_reg[3] >> 4;
				u4 z = finger_reg[4];
				u32 max_value = max(max(wx, wy), 1);
				ts->is_abs_touched = 1;
				++pressed_cnt;

				y = ts->f11_max_y - y;

				/* Linux 2.6.31 multi-touch */
				input_report_key(ts->input_dev, BTN_TOUCH, 1);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, max_value);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, z);
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, f);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
				
				input_mt_sync(ts->input_dev);
				ts->finger_status[f] = 1;
			} else if (finger_status == 0 && !is_report_again) {
				if(ts->finger_status[f]) {
					__u8 reg = fsr_len + 5 * f;
					__u8 *finger_reg = &f11_data[reg];
					u12 x = (finger_reg[0] * 0x10) | (finger_reg[2] % 0x10);
					u12 y = (finger_reg[1] * 0x10) | (finger_reg[2] / 0x10);
					//u4 wx = finger_reg[3] % 0x10;
					//u4 wy = finger_reg[3] / 0x10;

					y = ts->f11_max_y - y;
					input_report_key(ts->input_dev, BTN_TOUCH, 0);
					
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
					
					input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, f);
					input_mt_sync(ts->input_dev);
					ts->finger_status[f] = 0;
				}
			}

			ts->f11_fingers[f].status = finger_status;
		}

		/* f == ts->f11.points_supported */
		/* set f to offset after all absolute data */
		f = ((f + 3) >> 2) + f * 5;
		if (ts->f11_has_relative) {
			/* NOTE: not reporting relative data, even if available */
			/* just skipping over relative data registers */
			f += 2;
		}

		if (ts->hasEgrPalmDetect) {
			input_report_key(ts->input_dev,
			BTN_DEAD,
			f11_data[f + EGR_PALM_DETECT_REG] & EGR_PALM_DETECT);
		}
	
		if (ts->hasEgrFlick) {
			if (f11_data[f + EGR_FLICK_REG] & EGR_FLICK) {
				input_report_rel(ts->input_dev, REL_X, f11_data[f + 2]);
				input_report_rel(ts->input_dev, REL_Y, f11_data[f + 3]);
			}
		}
		
		if (ts->hasEgrSingleTap) {
				printk("%s %d, button touch\n", __func__, __LINE__);
			input_report_key(ts->input_dev, BTN_TOUCH,
				f11_data[f + EGR_SINGLE_TAP_REG] & EGR_SINGLE_TAP);
		}
		
		if (ts->hasEgrDoubleTap) {
			input_report_key(ts->input_dev, BTN_TOOL_DOUBLETAP,
				f11_data[f + EGR_DOUBLE_TAP_REG] & EGR_DOUBLE_TAP);
		}
		input_sync(ts->input_dev);
	
		if (pressed_cnt < last_pressed_cnt) {
			last_pressed_cnt = pressed_cnt;
			is_report_again = 1;
			goto report_press_finger_again;
		} else {
			last_pressed_cnt = pressed_cnt;
		}
	}

	if (!(ts->is_abs_touched) &&
		ts->hasF19 &&
		(interrupt[ts->f19.interrupt_offset] & ts->f19.interrupt_mask)) {
		int reg;
		int touch = 0;
		
		for (reg = 0; reg < ((ts->f19.points_supported + 7) / 8); reg++) {
			if (ts->data[ts->f19.data_offset + reg]) {
				touch = 1;
				break;
			}
		}
		//input_report_key(ts->input_dev, BTN_DEAD, touch);

		synaptics_report_buttons(ts, &ts->data[ts->f19.data_offset],
		ts->f19.points_supported, BTN_F19);
		input_sync(ts->input_dev);    
	}

	if (ts->hasF30 && interrupt[ts->f30.interrupt_offset] & ts->f30.interrupt_mask) {
		synaptics_report_buttons(ts, &ts->data[ts->f30.data_offset],
			ts->f30.points_supported, BTN_F30);
		input_sync(ts->input_dev);
	}

	return blocking_notifier_call_chain(&notifier_list, EV_LED, ts);
}

static irqreturn_t synaptics_rmi4_irq_handler(int irq, void *dev_id)
{
	struct synaptics_rmi4 *ts = dev_id;
	int ret;
	ret = synaptics_rmi4_work_handle(ts);
	//ack_irq_handle(irq);	/* clear irq pending */
	return IRQ_HANDLED;
}

static int synaptics_rmi4_read_pdt(struct synaptics_rmi4 *ts)
{
	int ret = 0;
	int nFd = 0;
	int interruptCount = 0;
	__u8 data_length = 0;

	struct i2c_msg fd_i2c_msg[2];
	__u8 fd_reg;
	struct synaptics_function_descriptor fd;

	struct i2c_msg query_i2c_msg[2];
	__u8 query[14];
	__u8 *egr;

	fd_i2c_msg[0].addr = ts->client->addr;
	fd_i2c_msg[0].flags = 0;
	fd_i2c_msg[0].buf = &fd_reg;
	fd_i2c_msg[0].len = 1;

	fd_i2c_msg[1].addr = ts->client->addr;
	fd_i2c_msg[1].flags = I2C_M_RD;
	fd_i2c_msg[1].buf = (__u8 *)(&fd);
	fd_i2c_msg[1].len = FD_BYTE_COUNT;

	query_i2c_msg[0].addr = ts->client->addr;
	query_i2c_msg[0].flags = 0;
	query_i2c_msg[0].buf = &fd.queryBase;
	query_i2c_msg[0].len = 1;

	query_i2c_msg[1].addr = ts->client->addr;
	query_i2c_msg[1].flags = I2C_M_RD;
	query_i2c_msg[1].buf = query;
	query_i2c_msg[1].len = sizeof(query);

	ts->hasF11 = false;
	ts->hasF19 = false;
	ts->hasF30 = false;
	ts->data_reg = 0xff;
	ts->data_length = 0;

	for (fd_reg = FD_ADDR_MAX; fd_reg >= FD_ADDR_MIN; fd_reg -= FD_BYTE_COUNT) {
		ret = i2c_transfer(ts->client->adapter, fd_i2c_msg, 2);
		if (ret < 0) {
			printk(KERN_ERR "I2C read failed querying RMI4 $%02X capabilities\n", ts->client->addr);
			return ret;
		}

		if (!fd.functionNumber) {
			/* End of PDT */
			ret = nFd;
			printk("Read %d functions from PDT\n", nFd);
			break;
		}

		++nFd;
		printk("functionNumber:0x%2x, queryBase:0x%2x, commandBase:0x%2x, controlBase:0x%2x, dataBase:0x%2x\n", \
		fd.functionNumber, fd.queryBase, fd.commandBase, fd.controlBase, fd.dataBase);

		switch (fd.functionNumber) {
			case 0x01: /* Interrupt */
				ts->f01.data_offset = fd.dataBase;
				ts->f01.control_base = fd.controlBase;   /* add by karlzheng for clear interrupt status */
				ts->f01.command_base = fd.commandBase;   /* add by karlzheng for clear interrupt status */
                ts->f01.query_base   = fd.queryBase;    
				/*
				* Can't determine data_length
				* until whole PDT has been read to count interrupt sources
				* and calculate number of interrupt status registers.
				* Setting to 0 safely "ignores" for now.
				*/
				data_length = 0;
				break;
			case 0x11: /* 2D */
				ts->hasF11 = true;

				ts->f11.control_base = fd.controlBase;

				ts->f11.data_offset = fd.dataBase;
				ts->f11.interrupt_offset = interruptCount / 8;
				ts->f11.interrupt_mask = ((1 << INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount & 0x7);

				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F11 query registers\n");

				ts->f11.points_supported = (query[1] & 7) + 1;
				if (ts->f11.points_supported == 6)
					ts->f11.points_supported = 10;

				ts->f11_fingers = kcalloc(ts->f11.points_supported,
				          sizeof(*ts->f11_fingers), 0);

				printk("%d fingers\n", ts->f11.points_supported);

				ts->f11_has_gestures = (query[1] >> 5) & 1;
				ts->f11_has_relative = (query[1] >> 3) & 1;

				egr = &query[7];

				printk(KERN_DEBUG "EGR features:\n");
				ts->hasEgrPinch = egr[EGR_PINCH_REG] & EGR_PINCH;
				printk(KERN_DEBUG "\tpinch: %u\n", ts->hasEgrPinch);
				ts->hasEgrPress = egr[EGR_PRESS_REG] & EGR_PRESS;
				printk(KERN_DEBUG "\tpress: %u\n", ts->hasEgrPress);
				ts->hasEgrFlick = egr[EGR_FLICK_REG] & EGR_FLICK;
				printk(KERN_DEBUG "\tflick: %u\n", ts->hasEgrFlick);
				ts->hasEgrEarlyTap = egr[EGR_EARLY_TAP_REG] & EGR_EARLY_TAP;
				printk(KERN_DEBUG"\tearly tap: %u\n", ts->hasEgrEarlyTap);
				ts->hasEgrDoubleTap = egr[EGR_DOUBLE_TAP_REG] & EGR_DOUBLE_TAP;
				printk(KERN_DEBUG "\tdouble tap: %u\n", ts->hasEgrDoubleTap);
				ts->hasEgrTapAndHold = egr[EGR_TAP_AND_HOLD_REG] & EGR_TAP_AND_HOLD;
				printk(KERN_DEBUG "\ttap and hold: %u\n", ts->hasEgrTapAndHold);
				ts->hasEgrSingleTap = egr[EGR_SINGLE_TAP_REG] & EGR_SINGLE_TAP;
				printk(KERN_DEBUG "\tsingle tap: %u\n", ts->hasEgrSingleTap);
				ts->hasEgrPalmDetect = egr[EGR_PALM_DETECT_REG] & EGR_PALM_DETECT;
				printk(KERN_DEBUG "\tpalm detect: %u\n", ts->hasEgrPalmDetect);

				query_i2c_msg[0].buf = &fd.controlBase;
				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F11 control registers\n");

				query_i2c_msg[0].buf = &fd.queryBase;

				ts->f11_max_x = ((query[7] & 0x0f) * 0x100) | query[6];
				ts->f11_max_y = ((query[9] & 0x0f) * 0x100) | query[8];

				printk("max X: %d; max Y: %d\n", ts->f11_max_x, ts->f11_max_y);

				ts->f11.data_length = data_length =
					/* finger status, four fingers per register */
					((ts->f11.points_supported + 3) / 4)
					/* absolute data, 5 per finger */
					+ 5 * ts->f11.points_supported
					/* two relative registers */
					+ (ts->f11_has_relative ? 2 : 0)
					/* F11_2D_Data8 is only present if the egr_0 register is non-zero. */
					+ (egr[0] ? 1 : 0)
					/* F11_2D_Data9 is only present if either egr_0 or egr_1 registers are non-zero. */
					+ ((egr[0] || egr[1]) ? 1 : 0)
					/* F11_2D_Data10 is only present if EGR_PINCH or EGR_FLICK of egr_0 reports as 1. */
					+ ((ts->hasEgrPinch || ts->hasEgrFlick) ? 1 : 0)
					/* F11_2D_Data11 and F11_2D_Data12 are only present if EGR_FLICK of egr_0 reports as 1. */
					+ (ts->hasEgrFlick ? 2 : 0);
				break;
			case 0x19: /* Cap Buttons */
				ts->hasF19 = true;

				ts->f19.data_offset = fd.dataBase;
				ts->f19.control_base = fd.controlBase;  
				ts->f19.interrupt_offset = interruptCount / 8;
				ts->f19.interrupt_mask = ((1 < INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount % 8);

				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F19 query registers\n");

				ts->f19.points_supported = query[1] & 0x1F;
				ts->f19.data_length = data_length = (ts->f19.points_supported + 7) / 8;

				printk(KERN_NOTICE "$%02X F19 has %d buttons\n", ts->client->addr, ts->f19.points_supported);
				break;
			case 0x30: /* GPIO */
				ts->hasF30 = true;

				ts->f30.data_offset = fd.dataBase;
				ts->f30.interrupt_offset = interruptCount / 8;
				ts->f30.interrupt_mask = ((1 < INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount % 8);

				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F30 query registers\n");


				ts->f30.points_supported = query[1] & 0x1F;
				ts->f30.data_length = data_length = (ts->f30.points_supported + 7) / 8;

				break;
			default:
				goto pdt_next_iter;
		}

		// Change to end address for comparison
		// NOTE: make sure final value of ts->data_reg is subtracted
		data_length += fd.dataBase;
		if (data_length > ts->data_length) {
			ts->data_length = data_length;
		}

		if (fd.dataBase < ts->data_reg) {
			ts->data_reg = fd.dataBase;
		}

		pdt_next_iter:
		interruptCount += INTERRUPT_SOURCE_COUNT(fd.intSrc);
	}

	// Now that PDT has been read, interrupt count determined, F01 data length can be determined.
	ts->f01.data_length = data_length = 1 + ((interruptCount + 7) / 8);
	// Change to end address for comparison
	// NOTE: make sure final value of ts->data_reg is subtracted
	data_length += ts->f01.data_offset;
	if (data_length > ts->data_length) {
		ts->data_length = data_length;
	}

	// Change data_length back from end address to length
	// NOTE: make sure this was an address
	ts->data_length -= ts->data_reg;

	//  Change all data offsets to be relative to first register read
	//  TODO: add __u8 *data (= &ts->data[ts->f##.data_offset]) to struct rmi_function_info?
	ts->f01.data_offset -= ts->data_reg;
	ts->f11.data_offset -= ts->data_reg;
	ts->f19.data_offset -= ts->data_reg;
	ts->f30.data_offset -= ts->data_reg;

	ts->data = kcalloc(ts->data_length, sizeof(*ts->data), 0);
	if (ts->data == NULL) {
		printk(KERN_ERR "Not enough memory to allocate space for RMI4 data\n");
		ret = -ENOMEM;
	}

	ts->data_i2c_msg[0].addr = ts->client->addr;
	ts->data_i2c_msg[0].flags = 0;
	ts->data_i2c_msg[0].len = 1;
	ts->data_i2c_msg[0].buf = &ts->data_reg;

	ts->data_i2c_msg[1].addr = ts->client->addr;
	ts->data_i2c_msg[1].flags = I2C_M_RD;
	ts->data_i2c_msg[1].len = ts->data_length;
	ts->data_i2c_msg[1].buf = ts->data;

	printk(KERN_ERR "RMI4 $%02X data read: $%02X + %d\n",
	ts->client->addr, ts->data_reg, ts->data_length);

	return ret;
}

int synaptics_rmi4_panel_init(struct synaptics_rmi4 *ts)
{
	int i;
	int ret;

	for(i=0; i<10; i++) {
		ts->finger_status[i] = 0;
	}

	ts->is_abs_touched    = 0;
	ts->is_button_touched = 0;

	ret = i2c_smbus_read_byte_data(ts->client, ts->f01.query_base + 3);
	if (ret < 0xA && ts->hasF11) {
		synaptic_reliable_write_reg(ts, ts->f11.control_base + 2, ts->delta);   // set reporting DeltaX      
		synaptic_reliable_write_reg(ts, ts->f11.control_base + 3, ts->delta);   // set reporting DeltaY
	}

	ret = i2c_smbus_read_byte_data(ts->client, ts->data_reg + 1);  /*clear interrupt status*/
	if (ret < 0) {
		printk(KERN_ERR "%s:i2c_smbus_read_byte_data failed\n", __func__);
	} 

	return ret;
}

extern int RMI4Reflash(struct synaptics_rmi4* ts, const unsigned char* img_buf, int img_len);
static int synaptics_rmi4_reflash(struct synaptics_rmi4 *ts, const char *buf, size_t count)
{
	unsigned char reg_data = 0;
	int ret = 0;

	ts->is_in_reflash = 1;

	disable_irq(ts->client->irq);
	flush_work(&ts->work);

	// disable Button and Abs0 interrupt
	synaptic_reliable_read_reg(ts, ts->f01.control_base + 1, &reg_data);
	reg_data &= 0xf3;    
	synaptic_reliable_write_reg(ts, ts->f01.control_base + 1, reg_data);

	// clear interrupt pending
	synaptic_reliable_read_reg(ts, ts->f01.data_offset + 1, &reg_data);

	ret = 0;

	do {
		ret = RMI4Reflash(ts, buf, count);
		if (ret == -6) {  // reflash error; restart again.
			pr_err("Reflash failed!!\n");
			arm_machine_restart(0, NULL);
		}
	} while (ret == -6);

	ts->is_in_reflash = 0;
	return 0;
}

static void m9w_rmi_firmware_handler(const struct firmware *fw,
        void *context)
{
	unsigned char builtin_fw_ver;
	struct synaptics_rmi4 *ts = NULL;
	
	if(!fw)
		return;

	if (context) {
		ts = (struct synaptics_rmi4 *)context;
		builtin_fw_ver = i2c_smbus_read_byte_data(ts->client, ts->f01.query_base + 3);
		pr_info("%s builtin_fw_ver:%d, fw_ver:%d\n", 
				__func__, builtin_fw_ver, fw->data[0x1f]);
		if (builtin_fw_ver != fw->data[0x1f]) {
			synaptics_rmi4_reflash(ts, fw->data, fw->size);
			synaptics_rmi4_read_pdt(ts);
			if (ts->hasF11) {
				int i;
				for (i = 0; i < ts->f11.points_supported; ++i) {
					input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->f11.points_supported, -1, 0);
					input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->f11_max_x, 0, 0);
					input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->f11_max_y, 0, 0);
					input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
					input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 15,  0, 0); 
					input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 0xF, 0, 0);
					input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);
				}
			}
			synaptic_reliable_write_reg(ts, ts->f01.control_base + 1, 0x0E);
			enable_irq(ts->client->irq);
		} else {
			int tmp;
			tmp = i2c_smbus_read_byte_data(ts->client, ts->f11.control_base + 2);
			printk("before read delta = %d\n", tmp);
		}
	}
	release_firmware(fw);
}

static int synaptics_rmi4_probe(
  struct i2c_client *client, const struct i2c_device_id *id)
{
	int i;
	int ret = 0;
	struct synaptics_rmi4 *ts = NULL;

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL)
		return -ENOMEM;
	
	ts->regulator = regulator_get(NULL, "vcc_touch");
	if (IS_ERR(ts->regulator)) {
		printk("regulator_get vcc_touch failed\n");
		ret = -EINVAL;
		goto err_get_regulaor_failed;
	}
	ret = regulator_enable(ts->regulator);
	if (ret) {
		printk("regulator_enable vcc_touch failed\n");
	}

	printk(KERN_INFO "probing for Synaptics RMI4 device %s at $%02X...\n", client->name, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	} 

	ts->client = client;
	i2c_set_clientdata(client, ts);

	ret = synaptics_rmi4_read_pdt(ts);
	if (ret <= 0) {
		if (ret == 0)
			printk(KERN_ERR "Empty PDT\n");

		printk(KERN_ERR "Error identifying device (%d)\n", ret);
		ret = -ENODEV;
		goto err_pdt_read_failed;
	}

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		printk(KERN_ERR "failed to allocate input device.\n");
		ret = -EBUSY;
		goto err_alloc_dev_failed;
	}

	ts->input_dev->name = "m9w-rmi-touchscreen";
	ts->input_dev->phys = client->name;
	__set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(ABS_MT_POSITION_X, ts->input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, ts->input_dev->absbit);
	__set_bit(ABS_MT_PRESSURE, ts->input_dev->absbit);
	__set_bit(EV_SYN, ts->input_dev->evbit);
	__set_bit(EV_KEY, ts->input_dev->evbit);
	__set_bit(BTN_TOUCH, ts->input_dev->keybit);
	if (ts->hasF11) {
/*	for (i = 0; i < ts->f11.points_supported; ++i) {
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->f11.points_supported, -1, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->f11_max_x, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->f11_max_y, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 15,  0, 0); 
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 0xF, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);
	}*/
	for (i = 0; i < ts->f11.points_supported; ++i) {
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->f11.points_supported, -1, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->f11_max_x, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->f11_max_y, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 15,  0, 0); 
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 0xF, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);
	}



	if (ts->hasEgrPalmDetect)
		set_bit(BTN_DEAD, ts->input_dev->keybit);
	if (ts->hasEgrFlick) {
		set_bit(REL_X, ts->input_dev->keybit);
		set_bit(REL_Y, ts->input_dev->keybit);
	}
	if (ts->hasEgrSingleTap)
		set_bit(BTN_TOUCH, ts->input_dev->keybit);
	if (ts->hasEgrDoubleTap)
		set_bit(BTN_TOOL_DOUBLETAP, ts->input_dev->keybit);
	}
	if (ts->hasF19) {
		set_bit(BTN_DEAD, ts->input_dev->keybit);
		
		/* F19 does not (currently) report ABS_X but setting maximum X is a convenient way to indicate number of buttons */
		input_set_abs_params(ts->input_dev, ABS_X, 0, ts->f19.points_supported, 0, 0);
		for (i = 0; i < ts->f19.points_supported; ++i) {
			set_bit(touch_button_key_code[i], ts->input_dev->keybit);
		}
	}
	
	if (ts->hasF30) {
		for (i = 0; i < ts->f30.points_supported; ++i)
			set_bit(BTN_F30 + i, ts->input_dev->keybit);
	}

	ts->finger_count = M9W_MAX_FINGERS;

	ts->delta = M9W_DETECT_DELTA;

	synaptics_rmi4_panel_init(ts);

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL, synaptics_rmi4_irq_handler,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts);
	}

	/*
	* Device will be /dev/input/event#
	* For named device files, use udev
	*/
	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk(KERN_ERR "synaptics_rmi4_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	} else {
		printk("synaptics input device registered\n");
	}

	dev_set_drvdata(&ts->input_dev->dev, ts);

	if (sysfs_create_file(&ts->input_dev->dev.kobj, &dev_attr_synaptics_rmi4_reg_address.attr) < 0)
		printk("failed to create sysfs file dev_attr_synaptics_rmi4_reg_address for input device\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 3;
	ts->early_suspend.suspend = synaptics_rmi4_early_suspend;
	ts->early_suspend.resume = synaptics_rmi4_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	BLOCKING_INIT_NOTIFIER_HEAD(&notifier_list);
	ret = request_firmware_nowait(THIS_MODULE,
		        FW_ACTION_HOTPLUG,
		        M9W_RMI_FW,
		        &client->dev,
		        GFP_KERNEL | __GFP_ZERO,
		        ts,
		        m9w_rmi_firmware_handler);

	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_alloc_dev_failed:
err_pdt_read_failed:
err_check_functionality_failed:
	regulator_disable(ts->regulator);
err_get_regulaor_failed:
	regulator_put(ts->regulator);
	kfree(ts);
	return ret;
}


static int synaptics_rmi4_remove(struct i2c_client *client)
{
	struct synaptics_rmi4 *ts = i2c_get_clientdata(client);

	regulator_disable(ts->regulator);
	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	
	return 0;
}

static inline void synaptics_rmi4_wait_reset_finish(int msec)
{
	unsigned long timeout;

	timeout = jiffies + msecs_to_jiffies(msec);
	while (time_before(jiffies, timeout)) {
		if(!gpio_get_value(TOUCH_PANNEL_INT))
			break;
	}
	while (time_before(jiffies, timeout)) {
		if (gpio_get_value(TOUCH_PANNEL_INT))
			break;
	}
	while (time_before(jiffies, timeout)) {
		if (!gpio_get_value(TOUCH_PANNEL_INT))
			break;
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h)
{
	struct synaptics_rmi4 *ts = container_of(h, struct synaptics_rmi4, early_suspend);
	u8 data = 0;
    
	while (ts->is_in_reflash)
		msleep(10);

	disable_irq(ts->client->irq);

	synaptic_reliable_read_reg(ts, ts->f01.control_base + 1, &data);
	data &= 0xf3;    // disable Button and Abs0 interrupt
	synaptic_reliable_write_reg(ts, ts->f01.control_base + 1, data);

	synaptic_reliable_read_reg(ts, ts->f01.control_base, &data);
	data &= 0xf8;    // set sensor to sleep mode
	data |= 0x01;    // set sensor to sleep mode
	synaptic_reliable_write_reg(ts, ts->f01.control_base, data);

	if (ts->hasF11) {
		int f;
		for (f = 0; f < ts->f11.points_supported  &&  f < ts->finger_count; ++f) {
			if(ts->finger_status[f]) {
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, f);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, 0);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, 0);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				//input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR, 0);
				ts->finger_status[f] = 0;
			}
		}
		input_mt_sync(ts->input_dev);
		input_sync(ts->input_dev);    
	}

	input_report_key(ts->input_dev, touch_button_key_code[0], 0);
	input_report_key(ts->input_dev, touch_button_key_code[1], 0);
	input_sync(ts->input_dev);
}

static void synaptics_rmi4_late_resume(struct early_suspend *h)
{
	struct synaptics_rmi4 *ts = container_of(h, struct synaptics_rmi4, early_suspend);
	u8 data = 1;

 	while (ts->is_in_reflash)
		msleep(10);

	/* reset touch pannel  */
	i2c_smbus_write_byte_data(ts->client, ts->f01.command_base, data); 

	synaptics_rmi4_wait_reset_finish(40); 

	synaptics_rmi4_panel_init(ts);

	enable_irq(ts->client->irq);
}
#endif

static const struct i2c_device_id synaptics_ts_id[] = {
    {"m9w-rmi-ts", 0},
    { }  
};

static struct i2c_driver synaptics_ts_driver = {
	.probe    = synaptics_rmi4_probe,
	.remove   = synaptics_rmi4_remove,
	.id_table = synaptics_ts_id,
	.driver = {
		.name = "m9w-rmi-ts",
	},
};

static int __devinit m9w_ts_init(void)
{
	return i2c_add_driver(&synaptics_ts_driver);
}

static void __exit  m9w_ts_exit(void)
{
	i2c_del_driver(&synaptics_ts_driver);
}

module_init(m9w_ts_init);
module_exit(m9w_ts_exit);

int register_m9w_rmi_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&notifier_list,
			 nb);
}
EXPORT_SYMBOL_GPL(register_m9w_rmi_notifier);

int unregister_m9w_rmi_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&notifier_list,
			 nb);
}
EXPORT_SYMBOL_GPL(unregister_m9w_rmi_notifier);

MODULE_DESCRIPTION("Meizu M9W Touchscreen Driver");
MODULE_AUTHOR("zhengkl <zhengkl@meizu.com> lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
