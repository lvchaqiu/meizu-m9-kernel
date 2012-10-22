/* linux/drivers/video/samsung/s3cfb_ls035b3sx01_dsi.c
 *
 *
 * Driver for meizu m9w mipi dsim lcd. D54E6PA8963
 * 
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: 	lvcha qiu	<lvcha@meizu.com>
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
 * Inital code : Oct 20 , 2010 : lvcha@meizu.com
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <plat/mipi-dsi.h>
#include <plat/mipi-ddi.h>

#define MIN_BRIGHTNESS		(0)
#define MAX_BRIGHTNESS		(10)

#define lcd_to_master(a)	(a->mipi_dev->master)
#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)
#define device_to_ddi_pd(a)	(a->master->dsim_info->mipi_ddi_pd)

typedef enum {
    LCD_DISPLAY_SLEEP_IN=0,
    LCD_DISPLAY_DEEP_STAND_BY,
    LCD_DISPLAY_POWER_OFF,
}dsim_lcd_state_t;

struct ls035b3sx01_info {
	struct device			*dev;
	struct regulator	*regulator;
	struct lcd_device		*ld;

	struct mipi_lcd_device		*mipi_dev;
	struct mipi_ddi_platform_data	*ddi_pd;
	dsim_lcd_state_t  state;
};

static int ls035b3sx01_panel_init(struct ls035b3sx01_info *lcd)
{
	struct dsim_master_ops *ops = lcd_to_master_ops(lcd);
	int ret;
/*
    ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xc6, 0x05);
	if (ret)
		return ret;
*/
	/*sleep out*/
	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_DCS_SHORT_WRITE, MIPI_DCS_EXIT_SLEEP_MODE, 0x00);
	if (ret)
		return ret;

	msleep(10);

    ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xb0, 0x00);
	if (ret)
		return ret;
	
	do {
		const char tmp[] = {0x99, 0x2b, 0x51};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

    	do{
		const char tmp[] = {0x98, 0x01, 0x05, 0x06, 0x0a, 0x18, 0x0e, 0x22, 0x23,0x24};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

	do{
		const char tmp[] = {0x9b, 0x02, 0x06, 0x08, 0x0a, 0x0c, 0x01};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

    	do{
		const char tmp[] = {0xa2, 0x00, 0x28, 0x0c, 0x05, 0xe9, 0x87,0x66,0x05};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

   	do{
        const char tmp[] = {0xa3, 0x00, 0x28, 0x0c, 0x05, 0xe9, 0x87,0x66,0x05};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

    	do{
        const char tmp[] = {0xa4, 0x04, 0x28, 0x0c, 0x05, 0xe9, 0x87,0x66,0x05};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

    	do{
		const char tmp[] = {0xa5, 0x04, 0x28, 0x0c, 0x05, 0xe9, 0x87,0x66,0x05};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

    	do{
		const char tmp[] = {0xa6, 0x02, 0x2b, 0x11, 0x46, 0x1c, 0xa9,0x76,0x06};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

    	do{
		const char tmp[] = {0xa7, 0x02, 0x2b, 0x11, 0x46, 0x1c, 0xa9,0x76,0x06};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xb4, 0x68);
	if (ret)
		return ret;

	do {
		const char tmp[] = {0xb5, 0x34, 0x03};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xb6, 0x02);
	if (ret)
		return ret;

	do{
		const char tmp[] = {0xb7, 0x08, 0x44, 0x06, 0x2e, 0x00, 0x00, 0x30, 0x33};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

	do{
		const char tmp[] = {0xb8, 0x1f, 0x44, 0x10, 0x2e, 0x1f, 0x00, 0x30, 0x33};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

	do {
		const char tmp[] = {0xb9, 0x46, 0x11, 0x01, 0x00, 0x30};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

	do{
		const char tmp[] = {0xba, 0x4f, 0x11, 0x00, 0x00, 0x30};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

	do{
		const char tmp[] = {0xbb, 0x11, 0x01, 0x00, 0x30};
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_LONG_WRITE, (unsigned int)tmp, ARRAY_SIZE(tmp));
		if (ret)
			return ret;
	} while(0);

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xbc, 0x06);
	if (ret)
		return ret;
	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xbf, 0x80);
	if (ret)
		return ret;
	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xb0, 0x01);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xc0, 0xc8);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xc2, 0x00);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xc3, 0x00);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xc4, 0x10);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xc5, 0x20);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xc8, 0x00);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xca, 0x10);
	if (ret)
		return ret;
	
    ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xcb, 0x44);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xcc, 0x10);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xd4, 0x00);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0xdc, 0x20);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0x96, 0x01);
	if (ret)
		return ret;

	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_DCS_SHORT_WRITE, MIPI_DCS_SET_DISPLAY_ON, 0);

	return ret;
}

static int __maybe_unused ls035b3sx01_gamma_ctrl(struct ls035b3sx01_info *lcd, int gamma)
{
	struct dsim_master_ops *ops = lcd_to_master_ops(lcd);

	/* change transfer mode to LP mode */
	if (ops->change_dsim_transfer_mode)
		ops->change_dsim_transfer_mode(lcd_to_master(lcd), 0);

	/* update gamma table. */

	/* change transfer mode to HS mode */
	if (ops->change_dsim_transfer_mode)
		ops->change_dsim_transfer_mode(lcd_to_master(lcd), 1);

	return 0;
}

static int ls035b3sx01_lcd_init(struct mipi_lcd_device *mipi_dev)
{
	struct ls035b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	return ls035b3sx01_panel_init(lcd);
}

static int ls035b3sx01_power_on(struct mipi_lcd_device *mipi_dev, int enable)
{
	struct ls035b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	int ret = 0;

	if (enable) {
		ret = regulator_enable(lcd->regulator);
		msleep(lcd->ddi_pd->power_on_delay);
	} else
		ret = regulator_disable(lcd->regulator);

	if (ret) {
		dev_err(lcd->dev, "failed to en/disable regulator\n");
		return -EINVAL;;
	}

	return 0;
}

static int ls035b3sx01_remove(struct mipi_lcd_device *mipi_dev)
{
	struct ls035b3sx01_info *lcd = NULL;

	ls035b3sx01_power_on(mipi_dev, 0);

	lcd = (struct ls035b3sx01_info *)dev_get_drvdata(&mipi_dev->dev);

	regulator_put(lcd->regulator);

	kfree(lcd);

	dev_set_drvdata(&mipi_dev->dev,NULL);

	return 0;
}

static int ls035b3sx01_probe(struct mipi_lcd_device *mipi_dev)
{
	struct ls035b3sx01_info *lcd = NULL;

	lcd = kzalloc(sizeof(struct ls035b3sx01_info), GFP_KERNEL);
	if (!lcd) {
		dev_err(&mipi_dev->dev, "failed to allocate ls035b3sx01 structure.\n");
		return -ENOMEM;
	}

	lcd->state = LCD_DISPLAY_POWER_OFF;
	lcd->mipi_dev = mipi_dev;
	lcd->ddi_pd =
		(struct mipi_ddi_platform_data *)device_to_ddi_pd(mipi_dev);

	lcd->dev = &mipi_dev->dev;

	dev_set_drvdata(&mipi_dev->dev, lcd);

	lcd->regulator = regulator_get(lcd->dev, "vcc_lcd");
	if (IS_ERR(lcd->regulator)) {
		dev_err(lcd->dev, "failed to get regulator\n");
		return -EINVAL;;
	}

	/* lcd power on */
	ls035b3sx01_power_on(mipi_dev, 1);

	/* lcd reset */
	if (lcd->ddi_pd->lcd_reset)
		lcd->ddi_pd->lcd_reset(NULL);

//	msleep(lcd->ddi_pd->reset_delay);

	return 0;
}

static int ls035b3sx01_reset(struct mipi_lcd_device *mipi_dev)
{
	struct ls035b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);

	/* lcd reset */
	if (lcd->ddi_pd->lcd_reset)
		lcd->ddi_pd->lcd_reset(NULL);

//	msleep(lcd->ddi_pd->reset_delay);

	return 0;
}

static void ls035b3sx01_shutdown(struct mipi_lcd_device *mipi_dev)
{
	struct ls035b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	struct dsim_master_ops *ops = lcd_to_master_ops(lcd);
	int ret;

	lcd->state = LCD_DISPLAY_POWER_OFF;

	/* deep-standby */
	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0x70, 1);

	msleep(lcd->ddi_pd->power_off_delay);	/*120ms*/

	/* lcd power off */
	ls035b3sx01_power_on(mipi_dev, 0);

	return;
}

static int ls035b3sx01_suspend(struct mipi_lcd_device *mipi_dev)
{
	struct ls035b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	struct dsim_master_ops *ops = lcd_to_master_ops(lcd);
	int ret;

	/* display off */
	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_DCS_SHORT_WRITE, MIPI_DCS_SET_DISPLAY_OFF, 0);
	if (ret)
		return ret;

	/* sleep in */
	ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_DCS_SHORT_WRITE, MIPI_DCS_ENTER_SLEEP_MODE, 0);
	if (ret)
		return ret;

	msleep(120);

	lcd->state = LCD_DISPLAY_SLEEP_IN;

	return 0;
}

static int ls035b3sx01_resume(struct mipi_lcd_device *mipi_dev)
{
	struct ls035b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);

	printk(KERN_DEBUG "%s: lcd->state = %d\n", __func__, lcd->state);

	switch(lcd->state) {
	case LCD_DISPLAY_POWER_OFF:
		/* lcd power on */
		if (!regulator_is_enabled(lcd->regulator)) {
			ls035b3sx01_power_on(mipi_dev, 1);
		}
		/* lcd reset */
		if (lcd->ddi_pd->lcd_reset)
			lcd->ddi_pd->lcd_reset(NULL);
//		msleep(lcd->ddi_pd->reset_delay);
		break;

	case LCD_DISPLAY_DEEP_STAND_BY:
		/* lcd reset */
		if (lcd->ddi_pd->lcd_reset)
			lcd->ddi_pd->lcd_reset(NULL);
//		msleep(lcd->ddi_pd->reset_delay);
		break;

	default:
		break;
	}

	return 0;
}

static struct mipi_lcd_driver ls035b3sx01_mipi_driver = {
	.name = "m9w-ls035b3sx01",
	.probe = ls035b3sx01_probe,
	.init_lcd	= ls035b3sx01_lcd_init,
	.suspend = ls035b3sx01_suspend,
	.resume = ls035b3sx01_resume,
	.shutdown = ls035b3sx01_shutdown,
	.remove = ls035b3sx01_remove,
	.reset_lcd = ls035b3sx01_reset,
};

static int __init ls035b3sx01_init(void)
{
	s5p_mipi_register_lcd_driver(&ls035b3sx01_mipi_driver);

	return 0;
}

arch_initcall(ls035b3sx01_init);

MODULE_AUTHOR("Lvcha Qiu <lvcha@meizu.com>");
MODULE_DESCRIPTION("MIPI-DSI based ls035b3sx01 LCD Panel Driver");
MODULE_LICENSE("GPL");
