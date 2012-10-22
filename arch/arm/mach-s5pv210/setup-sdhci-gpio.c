/* linux/arch/arm/plat-s5pc1xx/setup-sdhci-gpio.c
 *
 * Copyright (c) 2009-2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5PV210 - Helper functions for setting up SDHCI device(s) GPIO (HSMMC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-sdhci.h>
#include <plat/sdhci.h>

#include "herring.h"

void s5pv210_setup_sdhci0_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	switch (width) {
	/* Channel 0 supports 4 and 8-bit bus width */
	case 8:
		/* Set all the necessary GPIO function and pull up/down */
		for (gpio = S5PV210_GPG1(3); gpio <= S5PV210_GPG1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s3c_gpio_set_drvstrength(gpio, S3C_GPIO_DRVSTR_2X);
		}

	case 0:
	case 1:
	case 4:
		/* Set all the necessary GPIO function and pull up/down */
		for (gpio = S5PV210_GPG0(0); gpio <= S5PV210_GPG0(6); gpio++) {
			if (gpio != S5PV210_GPG0(2)) {
				s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
				s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			}
			s3c_gpio_set_drvstrength(gpio, S3C_GPIO_DRVSTR_2X);
		}
		break;
	default:
		printk(KERN_ERR "Wrong SD/MMC bus width : %d\n", width);
	}

	if (machine_is_herring()) {
		s3c_gpio_cfgpin(S5PV210_GPJ2(7), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S5PV210_GPJ2(7), S3C_GPIO_PULL_NONE);
		gpio_set_value(S5PV210_GPJ2(7), 1);
	}

	if (machine_is_herring()) {
		gpio_direction_output(S5PV210_GPJ2(7), 1);
		s3c_gpio_setpull(S5PV210_GPJ2(7), S3C_GPIO_PULL_NONE);
	}
}

#ifdef CONFIG_MACH_MEIZU_M9W
void s5pv210_setup_sdhci1_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	if(width > 0) {
		for (gpio = S5PV210_GPG1(0); gpio <= S5PV210_GPG1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
	} else {
		s3c_gpio_cfgpin(S5PV210_GPG1(0), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(S5PV210_GPG1(0), S3C_GPIO_PULL_NONE);

		for (gpio = S5PV210_GPG1(1); gpio <= S5PV210_GPG1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
	}
}
#else
void s5pv210_setup_sdhci1_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	switch (width) {
	/* Channel 1 supports 4-bit bus width */
	case 0:
	case 1:
	case 4:
		/* Set all the necessary GPIO function and pull up/down */
		for (gpio = S5PV210_GPG1(0); gpio <= S5PV210_GPG1(6); gpio++) {
			if (gpio != S5PV210_GPG1(2)) {
				s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
				s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			}
			s3c_gpio_set_drvstrength(gpio, S3C_GPIO_DRVSTR_2X);
		}
		break;
	default:
		printk(KERN_ERR "Wrong SD/MMC bus width : %d\n", width);
	}
}
#endif

#ifdef CONFIG_MACH_MEIZU_M9W
void s5pv210_setup_sdhci2_cfg_gpio(struct platform_device *pdev, int width)
{
	unsigned int gpio;
	unsigned int end;

	if(width != 1 && width !=4 && width !=8) {
		WARN(1, "Channel 2 supports 1, 4 and 8-bit bus width, width = %d\n", width);
		return;
	}

	/* Channel 2 supports 1 and 4-bit bus width */
	end = S5PV210_GPG2(3 + width);

	/* Set all the necessary GPG2 pins to special-function 2 */
	for (gpio = S5PV210_GPG2(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s3c_gpio_set_drvstrength(gpio, S3C_GPIO_DRVSTR_4X);
	}
}
#else
void s5pv210_setup_sdhci2_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	switch (width) {
	/* Channel 2 supports 4 and 8-bit bus width */
	case 8:
		/* Set all the necessary GPIO function and pull up/down */
		for (gpio = S5PV210_GPG3(3); gpio <= S5PV210_GPG3(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s3c_gpio_set_drvstrength(gpio, S3C_GPIO_DRVSTR_2X);
		}

	case 0:
	case 1:
	case 4:
		if (machine_is_herring() && herring_is_cdma_wimax_dev())
			break;
		/* Set all the necessary GPIO function and pull up/down */
		for (gpio = S5PV210_GPG2(0); gpio <= S5PV210_GPG2(6); gpio++) {
			if (gpio != S5PV210_GPG2(2)) {
				s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
				s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			}
			s3c_gpio_set_drvstrength(gpio, S3C_GPIO_DRVSTR_2X);
		}
		break;
	default:
		printk(KERN_ERR "Wrong SD/MMC bus width : %d\n", width);
	}
}
#endif

void s5pv210_setup_sdhci3_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	switch (width) {
	/* Channel 3 supports 4-bit bus width */
	case 0:
	case 1:
	case 4:
		/* Set all the necessary GPIO function and pull up/down */
		for (gpio = S5PV210_GPG3(0); gpio <= S5PV210_GPG3(6); gpio++) {
			if (gpio != S5PV210_GPG3(2)) {
				s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
				s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			}
			s3c_gpio_set_drvstrength(gpio, S3C_GPIO_DRVSTR_2X);
		}
		break;
	default:
		printk(KERN_ERR "Wrong SD/MMC bus width : %d\n", width);
	}
}
