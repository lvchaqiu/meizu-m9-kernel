/*
 * m9w_spi.c -- Serial peheripheral interface framing layer for IFX modem.
 * 
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: 	lvcha qiu   <lvcha@meizu.com>
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
 * Inital code : Apr 16 , 2010 : Lvcha qiu <lvcha@meizu.com>
 *
 */
//#define DEBUG
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
/*#include <linux/smp_lock.h>*/
#include <asm/uaccess.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#endif
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/io.h>

#include <mach/m9w_xgold_spi.h>
#include <mach/regs-gpio.h>

#include <linux/spi/m9w_spi.h>
#include "linux/ctype.h"

#define Modem_debug(buf,count) do{\
	int i;\
	pr_debug("\t%s(line:%d) \t Buffer(size:%d):\n",\
			__FUNCTION__, __LINE__,count);\
	for(i=0;i<count;i++){\
		pr_debug("[%d]:0x%02x ",i,buf[i]);\
		if(i%8 == 0)\
			pr_debug("\n");\
	}\
	pr_debug("\n");\
}while(0)

#define SPI_PACKET_SIZE 256
#define SPI_WAIT_TIMEOUT 5

static ssize_t ifx_timeout_show(struct device *dev,
     struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);
	struct ifx_spi_data *spi_data = spi_get_drvdata(spi);
	
	return sprintf(buf, "timeout = %lu\n",  spi_data->timeout);
}

static ssize_t ifx_timeout_store(struct device *dev,
      struct device_attribute *attr,
      const char *buf, size_t count)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);
	struct ifx_spi_data *spi_data = spi_get_drvdata(spi);
	unsigned long timeout = simple_strtoul(buf, NULL, 10);

	spi_data->timeout = timeout;
	
	return count;
}

static DEVICE_ATTR(timeout, S_IRUGO|S_IWUSR|S_IWGRP,
   ifx_timeout_show, ifx_timeout_store);

static ssize_t ifx_debug_store(struct device *dev,
      struct device_attribute *attr,
      const char *buf, size_t count)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);
	struct ifx_spi_data *spi_data = spi_get_drvdata(spi);
	int ifx_debug = simple_strtoul(buf, NULL, 10);

	spi_data->ifx_debug = ifx_debug;
	return count;

}

static DEVICE_ATTR(ifx_debug, S_IRUGO|S_IWUSR|S_IWGRP,
   NULL, ifx_debug_store);

static char* format_hex_string(const unsigned char *buf, int count) 
{
#define LINE_CHAR_NUM	1024
	const static char hexchar_table[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	static char line[3 * LINE_CHAR_NUM + 1];
	int actcount = (count < LINE_CHAR_NUM) ? count : LINE_CHAR_NUM;
	int i;
	for (i = 0; i < actcount; i++) {
		line[i * 3 + 0] = hexchar_table[buf[i] >> 4]; 
		line[i * 3 + 1] = hexchar_table[buf[i] & 0x0f]; 
		line[i * 3 + 2] = ' ';
	}
	line[i * 3] = 0;
	return line;
}

static char* format_ascii_string(const unsigned char *buf, int count) 
{
#define LINE_CHAR_NUM	1024
	static char line[LINE_CHAR_NUM + 1];
	int actcount = (count < LINE_CHAR_NUM) ? count : LINE_CHAR_NUM;
	int i;

	for (i = 0; i < actcount; i++) {
		if (isprint(buf[i])) {
			line[i] = buf[i];
		} else {
			line[i] = '.';
		}
	}
	line[i] = 0;
	return line;
}

/*
 * Function to set/reset MRDY signal
 */
static int ifx_spi_set_mrdy_signal(struct ifx_spi_data *spi_data, bool value)
{
	int mrdy_gpio = spi_data->info->mrdy_gpio;

	if (value != gpio_get_value(mrdy_gpio))
		gpio_set_value(mrdy_gpio, value);

	if (spi_data->ifx_debug) {
		bool srdy_value = gpio_get_value(spi_data->info->srdy_gpio);
  		printk(KERN_DEBUG "%s: mrdy = %d, srdy = %d, is_tx = %d\n", __func__, value, srdy_value, spi_data->ifx_master_initiated_transfer);
	}
 
	return 0;
}

/*
 * Intialize frame sizes as "IFX_SPI_DEFAULT_BUF_SIZE"(128) bytes for first SPI frame transfer
 */
static void ifx_spi_buffer_initialization(struct ifx_spi_data *spi_data)
{
	spi_data->ifx_sender_buf_size = IFX_SPI_DEFAULT_BUF_SIZE;
	spi_data->ifx_receiver_buf_size = IFX_SPI_DEFAULT_BUF_SIZE;
}


/*
 * Function opens a tty device when called from user space
 */
static int ifx_spi_open(struct tty_struct *tty, struct file *filp)
{
	int status = 0;
	struct ifx_spi_data *spi_data = tty->driver->driver_state;
	int port_num = tty->index;

	if (spi_data->ifx_debug)
		printk(KERN_DEBUG "%s: tty = 0x%p; num = %d\n", __func__, tty, tty->index);
	
	if(port_num == 0) {
		mutex_lock(&spi_data->ifx_spi_mutex);
		if(spi_data->open_count) {
			mutex_unlock(&spi_data->ifx_spi_mutex);
			pr_err("the spi_tty is already open\n");
			return -1;
		}
		spi_data->open_count++;
		spi_data->ifx_tty = tty;
		tty->driver_data = spi_data;
		
		ifx_spi_buffer_initialization(spi_data);
		
		mutex_unlock(&spi_data->ifx_spi_mutex);
	} else if(port_num == 1) {
		tty->driver_data = spi_data;
		irq_set_irq_type(spi_data->reset_irq, IRQF_TRIGGER_FALLING);
	} else if(port_num == 2) {
		tty->driver_data = spi_data;
	} else
		status = -1;

	return status;
}

/*
 * Function closes a opened a tty device when called from user space
 */
static void ifx_spi_close(struct tty_struct *tty, struct file *filp)
{
	struct ifx_spi_data *spi_data = tty->driver->driver_state;
	int port_num = tty->index;

	if (spi_data->ifx_debug)
		printk(KERN_DEBUG "%s: tty = 0x%p; num = %d\n", __func__, tty, tty->index);

	if(port_num == 0) {
		mutex_lock(&spi_data->ifx_spi_mutex);
		spi_data->ifx_tty = NULL;
		tty->driver_data = NULL;
		spi_data->open_count--;
		mutex_unlock(&spi_data->ifx_spi_mutex);
	} else if(port_num == 1) {
		irq_set_irq_type(spi_data->reset_irq, IRQF_TRIGGER_NONE);
	}
}

/*
 * Function is called from user space to send data to MODEM, it setups the transmission, enable MRDY signal and
 * waits for SRDY signal HIGH from MDOEM. Then starts transmission and reception of data to and from MODEM.
 * Once data read from MODEM is transferred to TTY core flip buffers, then "ifx_read_write_completion" is set
 * and this function returns number of bytes sent to MODEM
 */
static int ifx_spi_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	struct ifx_spi_data *spi_data = tty->driver_data;
	int status;
	int tick = 0;

	if (tty->index != 0 || spi_data == NULL) {
		WARN_ON(1);
		return -1;
	}

	if( !buf ){
		pr_err("File: %s\tFunction: %s\t Buffer NULL\n", __FILE__, __func__);
		return spi_data->ifx_ret_count;
	}
	if(count <= 0){
		pr_err("File: %s\tFunction: %s\t Count is ZERO, count = %d\n",
			__FILE__, __func__, count);
		return spi_data->ifx_ret_count;
	}
#if 0
	if (spi_data->ifx_debug) {
		printk(KERN_DEBUG "MzPhone, to send buf[%d] = [%s]\n", 
			count, format_hex_string(buf, (count < 32) ? count : 32));
		printk(KERN_DEBUG "MzPhone, to send buf[%d] = [%s]\n", 
			count, format_ascii_string(buf, (count < 32) ? count : 32));
	}
#endif

	spi_data->ifx_tty = tty;
	spi_data->ifx_tty->low_latency = 1;
	spi_data->ifx_spi_buf = buf;
	
	mutex_lock(&spi_data->ifx_spi_mutex);
	spi_data->ifx_ret_count = 0;
	spi_data->ifx_master_initiated_transfer = true;
	spi_data->ifx_spi_count = count;
	mutex_unlock(&spi_data->ifx_spi_mutex);

	ifx_spi_set_mrdy_signal(spi_data, 1);

	tick = spi_data->timeout ? (spi_data->timeout * HZ) : MAX_SCHEDULE_TIMEOUT;

	INIT_COMPLETION(spi_data->ifx_read_write_completion);
	status = wait_for_completion_timeout(&spi_data->ifx_read_write_completion, tick);
	if(!status) {
		if (!spi_data->ifx_slave_more)
			ifx_spi_set_mrdy_signal(spi_data, 0);
		pr_warn("ifx_spi_write: ifx_spi_write timeout, time = %luS\n", spi_data->timeout);
	} 

	/* Number of bytes sent to the device */
	return spi_data->ifx_ret_count;
}

/* This function should return number of free bytes left in the write buffer in this case always return 2044 */

static int ifx_spi_write_room(struct tty_struct *tty)
{
	int port_num = tty->index;
	int room = 0;

	if(port_num == 0) {
		room = IFX_SPI_DEFAULT_BUF_SIZE;
	}

	return room;
}

/*
 * Service an IOCTL request
 *
 * Arguments
 *
 * 	tty	pointer to tty instance data
 * 	file	pointer to associated file object for device
 * 	cmd	IOCTL command code
 * 	arg	command argument/context
 *
 * Return 0 if success, otherwise error code
 */
static int ifx_spi_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int port_num = tty->index;
	int ret = -ENOIOCTLCMD;

	if (port_num == 1) {

		struct ifx_spi_data *spi_data = (struct ifx_spi_data *)tty->driver_data;

		switch(cmd) {
		case TIOCMIWAIT:
			do {
				DECLARE_COMPLETION_ONSTACK(ifx_reset_complete);
				spi_data->ifx_reset_completion = &ifx_reset_complete;
				ret = wait_for_completion_interruptible(&ifx_reset_complete);
				spi_data->ifx_reset_completion = NULL;
				WARN(!ret, "%s: modem reset abnormaly\n", __func__);
			} while(0);
			break;
		default:
			printk(KERN_DEBUG "ifx_spi_ioctl: cmd = 0x%x\n", cmd);
			break;
		}
	} else if (port_num == 2) {
		struct ifx_spi_data *spi_data = (struct ifx_spi_data *)tty->driver_data;
		switch(cmd) {
		case TCSETSW:	//reset modem if need
			do {
			unsigned long reset_normal;
			if (copy_from_user(&reset_normal, argp, sizeof(reset_normal)))
				return -EFAULT;

			if(spi_data->info->reset_modem)
				spi_data->info->reset_modem(spi_data->info, reset_normal);
			} while(0);
			ret = 0;
			break;
		case TCSETSF:
			do {
				unsigned long write_timeout;
				if (copy_from_user(&write_timeout, argp, sizeof(write_timeout)))
					return -EFAULT;
				spi_data->timeout = write_timeout;
			} while(0);
			ret = 0;
			break;
		default:
			printk(KERN_DEBUG "ifx_spi_ioctl: cmd = 0x%x\n", cmd);
			break;
		}
	} else {
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

/*
 * Structure to specify tty core about tty driver operations supported in TTY SPI driver.
 */
static const struct tty_operations ifx_spi_ops = {
	.open 		= ifx_spi_open,
   	.close 		= ifx_spi_close,
    	.write 		= ifx_spi_write,
    	.ioctl 		= ifx_spi_ioctl,
    	.write_room	= ifx_spi_write_room,
};

/*
 * Function to set header information according to IFX SPI framing protocol specification
 */
static void ifx_spi_set_header_info(struct ifx_spi_data *spi_data)
{
	int i;
	union ifx_spi_frame_header header;
	
	for(i=0; i<4; i++){
		header.framesbytes[i] = 0;
	}

	header.ifx_spi_header.curr_data_size = spi_data->ifx_valid_frame_size;
	header.ifx_spi_header.more = spi_data->ifx_master_more;
	
	if (header.ifx_spi_header.more)
		header.ifx_spi_header.next_data_size = IFX_SPI_DEFAULT_BUF_SIZE;

	for(i=3; i>=0; i--){
		spi_data->ifx_tx_buffer[i] = header.framesbytes[i];
	}
}

/*
 * Function to setup transmission and reception. It implements a logic to find out the ifx_current_frame_size,
 * valid_frame_size and sender_next_frame_size to set in SPI header frame. Copys the data to be transferred from
 * user space to TX buffer and set MRDY signal to HIGH to indicate Master is ready to transfer data.
 */
static void ifx_spi_setup_transmission(struct ifx_spi_data *spi_data)
{
	/* Initial transfer - start with default transmitter and receiver frame size */
	if (spi_data->ifx_first_transfer)
		ifx_spi_buffer_initialization(spi_data);

	if ((spi_data->ifx_sender_buf_size != 0) ||
	     (spi_data->ifx_receiver_buf_size != 0)) {

		if (spi_data->ifx_spi_count > 0) {
			if (spi_data->ifx_spi_count > IFX_SPI_DEFAULT_BUF_SIZE) {
				spi_data->ifx_valid_frame_size = IFX_SPI_DEFAULT_BUF_SIZE;
				spi_data->ifx_spi_count -= IFX_SPI_DEFAULT_BUF_SIZE;
			} else {
				spi_data->ifx_valid_frame_size = spi_data->ifx_spi_count;
				spi_data->ifx_spi_count = 0;
			}
		} else {
			spi_data->ifx_valid_frame_size = 0;
			spi_data->ifx_sender_buf_size = 0;
		}

		if (spi_data->ifx_spi_count > 0) {
			spi_data->ifx_master_more = 1;
		} else {
			spi_data->ifx_master_more = 0;
		}

		if(spi_data->ifx_master_more)
			spi_data->ifx_sender_buf_size = IFX_SPI_DEFAULT_BUF_SIZE;
		else
			spi_data->ifx_sender_buf_size = 0;
#if 0
		/*added by lvcha 2010-08-21 to init buf*/
		if(spi_data->ifx_sender_buf_size)
			memset(spi_data->ifx_tx_buffer, 0x00, IFX_SPI_DEFAULT_BUF_SIZE + IFX_SPI_HEADER_SIZE);
		
		if(spi_data->ifx_receiver_buf_size)
			memset(spi_data->ifx_rx_buffer, 0x00, IFX_SPI_DEFAULT_BUF_SIZE + IFX_SPI_HEADER_SIZE);
		/*added end*/
#endif
		
		/* Set header information */
		ifx_spi_set_header_info(spi_data);
		
		if(spi_data->ifx_valid_frame_size > 0) {
			memcpy(spi_data->ifx_tx_buffer + IFX_SPI_HEADER_SIZE,
				spi_data->ifx_spi_buf, spi_data->ifx_valid_frame_size);
			spi_data->ifx_spi_buf += spi_data->ifx_valid_frame_size;
		}
	} else {
		pr_err("Transmission aborted\n");
	}
}

/*
 * push spi data to tty buffer
 */
static int ifx_spi_push_tty(struct ifx_spi_data *spi_data)
{
	unsigned int rx_valid_buf_size = spi_data->ifx_rx_valid_buf_size;
	
	if (spi_data->ifx_tty && (rx_valid_buf_size != 0)) {
		int count;
		char *data_ptr = spi_data->ifx_rx_buffer + IFX_SPI_HEADER_SIZE;
		
		/*push tty buffer size under 256 byte*/
		for (count=0; count<(rx_valid_buf_size/SPI_PACKET_SIZE); count++)
			tty_insert_flip_string(spi_data->ifx_tty, data_ptr + count * SPI_PACKET_SIZE, SPI_PACKET_SIZE);
		
		tty_insert_flip_string(spi_data->ifx_tty, 
			data_ptr + count * SPI_PACKET_SIZE, rx_valid_buf_size % SPI_PACKET_SIZE);
		
		tty_flip_buffer_push(spi_data->ifx_tty);

	} else {
		/*handle RTS and CTS in SPI flow control Reject the packet as of now*/
		dev_dbg(spi_data->tty_dev[0], 
			"Warning!!Handle RTS and CTS in SPI flow control Reject the packet as of now\n");
	}

	return 0;
}

static int ifx_spi_post_handle(struct ifx_spi_data *spi_data)
{
	/* End of transfer session detection */
	if (!(spi_data->ifx_master_more || spi_data->ifx_slave_more)) {
		/* End of transfer condition signaled */
		ifx_spi_set_mrdy_signal(spi_data, 0);
		spi_data->ifx_first_transfer = true;
#ifdef CONFIG_HAS_WAKELOCK
		/* lock pm 2*HZ to let system can read spi data in time */
		wake_lock_timeout(&spi_data->ifx_wake_lock, msecs_to_jiffies(2000));
#endif
	} else {
		spi_data->ifx_first_transfer = false;
	}

	/* End of TTY write operation */
	if ((spi_data->ifx_master_initiated_transfer) && (spi_data->ifx_spi_count == 0)) {
		spi_data->ifx_master_initiated_transfer = false;
		complete(&spi_data->ifx_read_write_completion);
	}

	/* Restart if data received from TTY and end of transfer negociated */
	if (spi_data->ifx_first_transfer && spi_data->ifx_spi_count > 0)
		ifx_spi_set_mrdy_signal(spi_data, 1);

	return 0;
}

/*
 * Function starts Read and write operation and transfers received data to TTY core. It pulls down MRDY signal
 * in case of single frame transfer then sets "ifx_read_write_completion" to indicate transfer complete.
 */
static int ifx_spi_analyze_data(struct ifx_spi_data *spi_data)
{
	int i;
	union ifx_spi_frame_header header;
	int status = spi_data->ifx_spi_status;
#if 0
	if (spi_data->ifx_debug) {
		printk(KERN_DEBUG "MzPhone, sent buf is [%s]\n", 
				format_hex_string(spi_data->ifx_tx_buffer, 32));
		printk(KERN_DEBUG "MzPhone, sent buf is [%s]\n", 
				format_ascii_string(spi_data->ifx_tx_buffer, 32));
		printk(KERN_DEBUG "MzPhone, read buf is [%s]\n",
				format_hex_string(spi_data->ifx_rx_buffer, 32));
		printk(KERN_DEBUG "MzPhone, read buf is [%s]\n",
				format_ascii_string(spi_data->ifx_rx_buffer, 32));
	}
#endif

	if (status > 0) {
		spi_data->ifx_ret_count += spi_data->ifx_valid_frame_size;
	} else {
		*(u32*)spi_data->ifx_rx_buffer = 0;//don't push data to tty when error occur
	}

	dev_dbg(spi_data->tty_dev[0], 
		"\nspidrv::ifx_spi_send_and_receive_data status = %d\n",status);

	/* Handling Received data */
	/* Invalidated frame detection */
	if ((*(u32*)spi_data->ifx_rx_buffer) == 0xffffffff) {
		*(u32*)spi_data->ifx_rx_buffer = 0;
		dev_dbg(spi_data->tty_dev[0], "Invalidated Frame detected!\n");
	}

	for(i=3; i>=0; i--)
		header.framesbytes[i] = spi_data->ifx_rx_buffer[i];

	dev_dbg(spi_data->tty_dev[0],
			"receive nlen = %d, dlen=%d, cts=%d, dsr=%d\n\n", 
			header.ifx_spi_header.next_data_size, 
			header.ifx_spi_header.curr_data_size, 
			header.ifx_spi_header.cts_rts, 
			header.ifx_spi_header.dsr_dtr);

	if(header.ifx_spi_header.next_data_size != IFX_SPI_DEFAULT_BUF_SIZE) {
		for(i=0; i<4; i++)
			header.framesbytes[i] = 0;
	}
	
	spi_data->ifx_rx_valid_buf_size = header.ifx_spi_header.curr_data_size;
	spi_data->ifx_slave_more = header.ifx_spi_header.more;

	dev_dbg(spi_data->tty_dev[0], 
		"spidrv::spidrv::ifx_spi_send_and_receive_data: more=%d \n",spi_data->ifx_slave_more);

	if (spi_data->ifx_slave_more)
		spi_data->ifx_receiver_buf_size = IFX_SPI_DEFAULT_BUF_SIZE;
	else
		spi_data->ifx_receiver_buf_size = 0;

	return status;
}

/*
 * Function copies the TX_BUFFER and RX_BUFFER pointer to a spi_transfer structure and add it to SPI tasks.
 * And calls SPI Driver function "spi_sync" to start data transmission and reception to from MODEM
 */
static unsigned int ifx_spi_sync_read_write(struct ifx_spi_data *spi_data, unsigned int len)
{
	int status;
	struct spi_message m;
	struct spi_transfer	t = {
		.tx_buf = spi_data->ifx_tx_buffer,
		.rx_buf = spi_data->ifx_rx_buffer,
		.len    = len,
	};
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	
	if (spi_data->spi == NULL) {
		status = -ESHUTDOWN;
	} else {
		status = spi_sync(spi_data->spi, &m);
	}

	if (status == 0){
		if (m.status == 0)
			status = m.actual_length;
		else
			status = m.status;
	} else {
		pr_err("ifx_spi_sync Transmission failed, status = %d\n", status);
	}
	
	if (spi_data->ifx_debug) {
		printk(KERN_DEBUG "MzPhone, tx_buf[%d] = [%s]\n", 
			len, format_hex_string(spi_data->ifx_tx_buffer, (len < 32) ? len : 32));
		printk(KERN_DEBUG "MzPhone, tx_buf[%d] = [%s]\n", 
			len, format_ascii_string(spi_data->ifx_tx_buffer, (len < 32) ? len : 32));
		printk(KERN_DEBUG "MzPhone, rx_buf[%d] = [%s]\n", 
			len, format_hex_string(spi_data->ifx_rx_buffer, (len < 32) ? len : 32));
		printk(KERN_DEBUG "MzPhone, rx_buf[%d] = [%s]\n", 
			len, format_ascii_string(spi_data->ifx_rx_buffer, (len < 32) ? len : 32));
	}

	return status;
}

static int handle_queue_work(struct ifx_spi_data *spi_data)
{
	mutex_lock(&spi_data->ifx_spi_mutex);

	/* Ensure MRDY is set (slave initiated case) */
	if (!spi_data->ifx_master_initiated_transfer) {
		ifx_spi_set_mrdy_signal(spi_data, 1);
#ifdef CONFIG_HAS_WAKELOCK	
		wake_lock_timeout(&spi_data->ifx_wake_lock, SPI_WAIT_TIMEOUT * HZ);
#endif
	}

	ifx_spi_setup_transmission(spi_data);

	spi_data->ifx_spi_status = ifx_spi_sync_read_write(spi_data, 
		IFX_SPI_DEFAULT_BUF_SIZE + IFX_SPI_HEADER_SIZE); /* 4 bytes for header */

	ifx_spi_analyze_data(spi_data);

	ifx_spi_push_tty(spi_data);

	ifx_spi_post_handle(spi_data);

	mutex_unlock(&spi_data->ifx_spi_mutex);

	return 0;
}

/*
 * Function is a Interrupt service routine, is called when SRDY signal goes HIGH. 
 * It set up transmission and reception if it is a Slave initiated data transfer. 
 * For both the cases Master intiated/Slave intiated
 * transfer it starts data transfer.
 */
static irqreturn_t ifx_srdy_irq_thread(int irq, void *handle)
{
	struct ifx_spi_data *spi_data = (struct ifx_spi_data *)handle;

	ktime_t delta, rettime;
	long long delta_ms;

	rettime = ktime_get();

	atomic_set(&spi_data->state, atomic_read(&spi_data->state) & ~IFX_IOERR); /* Clear every ERROR flag */
	atomic_set(&spi_data->state, atomic_read(&spi_data->state) | IFX_XFERBUSY); /* Set Xfer busy flag */

	handle_queue_work(spi_data);

	atomic_set(&spi_data->state, atomic_read(&spi_data->state) & ~IFX_XFERBUSY);

	delta = ktime_sub(ktime_get(),rettime);
	delta_ms = ktime_to_ms(delta);
	if (delta_ms > 50)	// 50ms timeout
		pr_debug("ifx_spi_handle_work timeout %Ld mS\n", delta_ms);

	return IRQ_HANDLED;
}

static irqreturn_t ifx_srdy_irq_handle(int irq, void *handle)
{
	struct ifx_spi_data *spi_data = (struct ifx_spi_data *)handle;

	if (spi_data->irq_type & IRQF_TRIGGER_HIGH) {
		spi_data->irq_type = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
		irq_set_irq_type(irq, spi_data->irq_type);
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ifx_spi_handle_reset_irq(int irq, void *handle)
{
	struct ifx_spi_data *spi_data = handle;

	if (spi_data->ifx_reset_completion)
		complete(spi_data->ifx_reset_completion);

	return IRQ_HANDLED;
}

static int __devinit ifx_spi_probe(struct spi_device *spi)
{
	int status,i;
	struct m9w_spi_info *pdata = spi->dev.platform_data;
	struct ifx_spi_data *spi_data;

	/* Allocate SPI driver data */
	spi_data = kzalloc(sizeof(struct ifx_spi_data), GFP_KERNEL);
	if (!spi_data) {
		return -ENOMEM;
	}

	/* Allocate and Register a TTY device */
	spi_data->ifx_spi_tty_driver = alloc_tty_driver(IFX_N_SPI_MINORS);
	if (!spi_data->ifx_spi_tty_driver) {
		pr_err("Fail to allocate TTY Driver\n");
		status = -ENOMEM;
		goto error1;
	}

	/*config modem gpio pin*/
	pdata->cfg_gpio(pdata);

	/*power on modem*/
	pdata->power_modem(pdata, 1);

	/*reset modem*/
//	pdata->reset_modem(pdata, 1);

	spi_data->info = kmemdup(pdata, sizeof(struct m9w_spi_info), GFP_KERNEL);

	spi_data->ifx_first_transfer = true;
	spi_data->timeout = 0; //SPI_WAIT_TIMEOUT
	spi_data->ifx_debug = 0;

	/* initialize the tty driver */
	spi_data->ifx_spi_tty_driver->owner = THIS_MODULE;
	spi_data->ifx_spi_tty_driver->driver_name = "ifx_m9w";
	spi_data->ifx_spi_tty_driver->name = "ttyspi";
	spi_data->ifx_spi_tty_driver->major = IFX_SPI_MAJOR;
	spi_data->ifx_spi_tty_driver->minor_start = 0;
	spi_data->ifx_spi_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	spi_data->ifx_spi_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	spi_data->ifx_spi_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	spi_data->ifx_spi_tty_driver->init_termios = tty_std_termios;
	spi_data->ifx_spi_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	spi_data->ifx_spi_tty_driver->driver_state = spi_data;
	tty_set_operations(spi_data->ifx_spi_tty_driver, &ifx_spi_ops);

	status = tty_register_driver(spi_data->ifx_spi_tty_driver);
	if (status) {
		pr_err("Failed to register IFX SPI tty driver");
		goto error2;
	}

	/* . and sysfs class devices, so mdev/udev make /dev/ttyGS* */
	for (i = 0; i < IFX_N_SPI_MINORS; i++) {
		struct device *tty_dev;
		tty_dev = tty_register_device(spi_data->ifx_spi_tty_driver, i, &spi->dev);
		if (IS_ERR(tty_dev)) {
			pr_warning("%s: no classdev for port %d, err %ld\n",
				__func__, i, PTR_ERR(tty_dev));
		} else {
			spi_data->tty_dev[i] = tty_dev;
			dev_set_drvdata(tty_dev, &spi->dev);
		}
	}

	/* Allocate memeory for TX_BUFFER and RX_BUFFER */
	spi_data->ifx_rx_buffer = kzalloc(IFX_SPI_MAX_BUF_SIZE+IFX_SPI_HEADER_SIZE, GFP_KERNEL);
	if (!spi_data->ifx_rx_buffer) {
		pr_err("Open Failed ENOMEM\n");
		status = -ENOMEM;
		goto error3;
	}

	spi_data->ifx_tx_buffer = kzalloc(IFX_SPI_MAX_BUF_SIZE+IFX_SPI_HEADER_SIZE, GFP_KERNEL);
	if (!spi_data->ifx_tx_buffer) {
		pr_err("Open Failed ENOMEM\n");
		status = -ENOMEM;
		goto error4;
	}

	spi_set_drvdata(spi, spi_data);

	mutex_init(&spi_data->ifx_spi_mutex);

	init_completion(&spi_data->ifx_read_write_completion);

	/* Configure SPI */
	spi_data->spi = spi;
	spi->mode = SPI_MODE_1;
	spi->bits_per_word = 8;

	/* This version of the protocol supports up to 24 MHz */
	spi->max_speed_hz = 12000000;

	status = spi_setup(spi);
	if(status < 0) {
		pr_err("Failed to setup SPI \n");
		goto error5;
	}

	spi_data->reset_irq = gpio_to_irq(pdata->bb_reset_gpio);
	status = request_threaded_irq(spi_data->reset_irq, NULL,
								ifx_spi_handle_reset_irq, 
								IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
								spi->dev.driver->name, spi_data);
	if (status != 0) {
		pr_err("Failed to request IRQ for reset\n");
		goto error5;
	}

	status = device_create_file(&spi->dev, &dev_attr_timeout);
	if (status) {
		pr_err("Failed to device_create_file : dev_attr_timeout\n");
		goto error6;
	}
	status = device_create_file(&spi->dev, &dev_attr_ifx_debug);
	if (status) {
		pr_err("Failed to device_create_file : dev_attr_ifx_debug\n");
		goto error7;
	}

	device_init_wakeup(&spi->dev, 1);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&spi_data->ifx_wake_lock, WAKE_LOCK_SUSPEND, "ifx_working");
#endif

	/* Enable SRDY Interrupt request - If the SRDY signal is high 
	  * then ifx_spi_handle_srdy_irq() is called 
	  */
	spi_data->modem_irq = gpio_to_irq(pdata->srdy_gpio);
	spi_data->irq_type = IRQF_TRIGGER_HIGH | IRQF_ONESHOT;
	status = request_threaded_irq(spi_data->modem_irq, ifx_srdy_irq_handle,
								ifx_srdy_irq_thread, 
								spi_data->irq_type, 
								spi->dev.driver->name, spi_data);
	if (status) {
		pr_err("Failed to request IRQ for SRDY\n");
		goto error8;
	}

	pr_info("m9w ifx_spi_probe success!\n");

	return 0;

error8:
	device_init_wakeup(&spi->dev, 0);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&spi_data->ifx_wake_lock);
#endif
	device_remove_file(&spi->dev, &dev_attr_ifx_debug);
error7:
	device_remove_file(&spi->dev, &dev_attr_timeout);
 error6:
	free_irq(spi_data->reset_irq, spi_data);
error5:
	kfree(spi_data->ifx_tx_buffer);
 error4:
	kfree(spi_data->ifx_rx_buffer);
 error3:    
	tty_unregister_driver(spi_data->ifx_spi_tty_driver);
 error2:
	put_tty_driver(spi_data->ifx_spi_tty_driver);
 error1:
	kfree(spi_data);

	return status;
}

static int __devexit ifx_spi_remove(struct spi_device *spi)
{
	struct ifx_spi_data *spi_data = spi_get_drvdata(spi);

	device_init_wakeup(&spi->dev, 0);

	device_remove_file(&spi->dev, &dev_attr_timeout);
	device_remove_file(&spi->dev, &dev_attr_ifx_debug);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&spi_data->ifx_wake_lock);
#endif

	spi_data->spi = NULL;
	spi_set_drvdata(spi, NULL);

	free_irq(spi_data->modem_irq, spi_data);

	tty_unregister_driver(spi_data->ifx_spi_tty_driver);
	put_tty_driver(spi_data->ifx_spi_tty_driver);

	if (spi_data->ifx_tx_buffer)
		kfree(spi_data->ifx_tx_buffer);
	if (spi_data->ifx_rx_buffer)
		kfree(spi_data->ifx_rx_buffer);

	if (spi_data)
		kfree(spi_data);

	return 0;
}

#ifdef CONFIG_PM
static int ifx_spi_suspend(struct spi_device *spi, pm_message_t state)
{
	struct ifx_spi_data *spi_data = spi_get_drvdata(spi);

	dev_dbg(&spi->dev, "%s begin\n", __func__);

	disable_irq(spi_data->modem_irq);

	atomic_set(&spi_data->state, atomic_read(&spi_data->state) | IFX_SUSPND);

	enable_irq_wake(spi_data->modem_irq);
	enable_irq_wake(spi_data->reset_irq);

	while (atomic_read(&spi_data->state) & IFX_XFERBUSY) {
		WARN_ON(1);
		msleep(10);
	}

	dev_dbg(&spi->dev, "%s post\n", __func__);

	return 0;
}

static int ifx_spi_resume(struct spi_device *spi)
{
	struct ifx_spi_data *spi_data = spi_get_drvdata(spi);
	int spi_state;

	dev_dbg(&spi->dev, "%s begin\n", __func__);

	spi_state = atomic_read(&spi_data->state) & ~IFX_SUSPND;
	atomic_set(&spi_data->state, spi_state);

	disable_irq_wake(spi_data->modem_irq);
	disable_irq_wake(spi_data->reset_irq);

	/* lock system for waiting modem transform data */
	if(gpio_get_value(spi_data->info->srdy_gpio)) {
		atomic_set(&spi_data->state, (spi_state | IFX_RESUME));
		wake_lock(&spi_data->ifx_wake_lock);
	}

	enable_irq(spi_data->modem_irq);

	dev_dbg(&spi->dev, "%s post\n", __func__);

	return 0;
}
#else
#define ifx_spi_suspend NULL
#define ifx_spi_resume NULL
#endif /* CONFIG_PM */

static struct spi_driver ifx_spi_driver = {
	.driver = {
	    .name = "m9w-spi",
	    .bus = &spi_bus_type,
	    .owner = THIS_MODULE,
	},
	.probe = ifx_spi_probe,
	.remove = __devexit_p(ifx_spi_remove),
	.suspend = ifx_spi_suspend,
	.resume = ifx_spi_resume,
};

/*
 * Initialization function which allocates and set different parameters for TTY SPI driver. Registers the tty driver
 * with TTY core and register SPI driver with the Kernel. It validates the GPIO pins for MRDY and then request an IRQ
 * on SRDY GPIO pin for SRDY signal going HIGH. In case of failure of SPI driver register cases it unregister tty driver
 * from tty core.
 */
static int __init ifx_spi_init(void)
{
	/* Register SPI Driver */
	return spi_register_driver(&ifx_spi_driver);
}
module_init(ifx_spi_init);

/*
 * Exit function to unregister SPI driver and tty SPI driver
 */
static void __exit ifx_spi_exit(void)
{
	spi_unregister_driver(&ifx_spi_driver);
}
module_exit(ifx_spi_exit);

MODULE_AUTHOR("Lvcha Qiu <lvcha@meizu.com>");
MODULE_DESCRIPTION("M9W IFX SPI Framework Layer");
MODULE_LICENSE("GPLV2");
