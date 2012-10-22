/*
 * arch/arm/mach/include/m9w_xgold_spi.h
 *
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __M9W_SPI_H
#define __M9W_SPI_H __FILE__

struct m9w_spi_info{
    int srdy_gpio;
    int mrdy_gpio;
    int reset_gpio;
    int power_gpio;
    int bb_reset_gpio;
    int (*cfg_gpio)(struct m9w_spi_info *info);
    int (*reset_modem)(struct m9w_spi_info *info, int normal);
    int (*power_modem)(struct m9w_spi_info *info, int);
};

#endif
