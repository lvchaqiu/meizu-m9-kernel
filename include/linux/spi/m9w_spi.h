/*
 * m9w_spi.h -- Serial peheripheral interface framing layer for IFX modem.
 *
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: 	lvcha qiu   <lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef M9W_SPI_H_
#define M9W_SPI_H_

#define IFX_SPI_MAJOR			153	/* assigned */
#define IFX_N_SPI_MINORS		4	/* ... up to 256 */

#define IFX_SPI_MAX_BUF_SIZE		2044	/* Max buffer size */
#define IFX_SPI_DEFAULT_BUF_SIZE	2044 	/* Default buffer size*/

#define IFX_SPI_HEADER_SIZE		4

/* Structure used to store private data */
struct ifx_spi_data {
	struct spi_device   *spi;
	struct list_head    device_entry;
	struct completion   ifx_read_write_completion;
	struct tty_struct   *ifx_tty;
	struct device *tty_dev[IFX_N_SPI_MINORS];

	/* buffer is NULL unless this device is open (open_count > 0) */
	int open_count;
	
	struct delayed_work ifx_delay_wq;
	struct delayed_work ifx_mdelay_wq;

	struct tty_driver   *ifx_spi_tty_driver;
	

	unsigned char   *ifx_tx_buffer;
	unsigned char   *ifx_rx_buffer;

	unsigned int    ifx_master_more;
	unsigned int    ifx_slave_more;
	unsigned int    ifx_first_transfer:1;
	unsigned int	ifx_master_initiated_transfer:1;
	unsigned int	ifx_spi_count;
	unsigned int	ifx_sender_buf_size;
	unsigned int	ifx_receiver_buf_size;
	unsigned int	ifx_valid_frame_size;
	unsigned int	ifx_ret_count;
	const unsigned char *ifx_spi_buf;

	unsigned int ifx_rx_valid_buf_size;		/* valid data size form modem to ap*/

	int ifx_spi_status;					/* return value of transmit from s5pc110 spi */
	unsigned int modem_irq;
	unsigned int irq_type;
	unsigned int reset_irq;
	struct completion   *ifx_reset_completion;
	struct m9w_spi_info *info;
#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock ifx_wake_lock;
#endif

	unsigned long timeout;
	int ifx_debug;

	struct mutex 		ifx_spi_mutex;		// Mutex between user write and SPI handle work thread

#define IFX_SUSPND     (1<<0)
#define IFX_XFERBUSY   (1<<1)
#define IFX_IRQERR     (1<<2)
#define IFX_TOUTERR    (1<<3)
#define IFX_INTERR     (1<<4)
#define IFX_IOERR      (1<<5)
#define IFX_RESUME     (1<<6)
	atomic_t	state;
};

union ifx_spi_frame_header{
    struct{
        unsigned int curr_data_size:12;
        unsigned int more:1;
        unsigned int res1:1;
        unsigned int res2:2;
        unsigned int next_data_size:12;
        unsigned int ri:1;
        unsigned int dcd:1;
        unsigned int cts_rts:1;
        unsigned int dsr_dtr:1;
    }ifx_spi_header;
    unsigned char framesbytes[IFX_SPI_HEADER_SIZE];
};

#ifdef CONFIG_EARLYSUSPEND
extern suspend_state_t get_suspend_state(void);
#endif

#endif /* M9W_SPI_H_ */
