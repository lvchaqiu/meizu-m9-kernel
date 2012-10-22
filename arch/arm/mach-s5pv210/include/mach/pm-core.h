/* linux/arch/arm/mach-s5pv210/include/mach/pm-core.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Based on arch/arm/mach-s3c2410/include/mach/pm-core.h,
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * S5PV210 - PM core support for arch/arm/plat-s5p/pm.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <mach/regs-gpio.h>

static inline void s3c_pm_debug_init_uart(void)
{
	/* nothing here yet */
}

static inline void s3c_pm_arch_prepare_irqs(void)
{
	__raw_writel(s3c_irqwake_intmask, S5P_WAKEUP_MASK);
	__raw_writel(s3c_irqwake_eintmask, S5P_EINT_WAKEUP_MASK);
}

static inline void s3c_pm_arch_stop_clocks(void)
{
	/* nothing here yet */
}

#define WS_CEC	(1 << 15)
#define WS_ST	(1 << 14)
#define WS_I2S	(1 << 13)
#define WS_MMC3	(1 << 12)
#define WS_MMC2	(1 << 11)
#define WS_MMC1	(1 << 10)
#define WS_MMC0	(1 << 9)
#define WS_KEY	(1 << 5)
#define WS_TS0	(1 << 4)
#define WS_TS1	(1 << 3)
#define WS_RTC_TICK	(1 << 2)
#define WS_RTC_ALARM	(1 << 1)
#define WS_EINT	(1 << 0)

static inline void show_wakeup_src_name(int wakeup_src)
{
	pr_info("### waked up by: ");
	if (wakeup_src & WS_CEC)
		pr_info("CEC");
	else if (wakeup_src & WS_ST)
		pr_info("ST");
	else if (wakeup_src & WS_I2S)
		pr_info("I2S");
	else if (wakeup_src & (WS_MMC3 | WS_MMC2 | WS_MMC1))
		pr_info("MMC");
	else if (wakeup_src & WS_KEY)
		pr_info("KEY");
	else if (wakeup_src & (WS_TS0 | WS_TS1))
		pr_info("TS");
	else if (wakeup_src & WS_RTC_TICK)
		pr_info("RTC TICK");
	else if (wakeup_src & WS_RTC_ALARM)
		pr_info("RTC ALARM");
	else if (wakeup_src & WS_EINT)
		pr_info("EINT");
	else
		pr_info("Unknown");
	pr_info("\n");
}

static inline void s3c_pm_arch_show_resume_irqs(void)
{
	int wakeup_src = __raw_readl(S5P_WAKEUP_STAT);
	pr_info("S5P_WAKEUP_STAT 0x%X\n", wakeup_src);
	pr_info("EINT_PEND 0x%X, 0x%X, 0x%X, 0x%X\n",
		__raw_readl(S5P_EINT_PEND(0)), __raw_readl(S5P_EINT_PEND(1)),
		__raw_readl(S5P_EINT_PEND(2)), __raw_readl(S5P_EINT_PEND(3)));
	show_wakeup_src_name(wakeup_src);
}

static inline void s3c_pm_arch_update_uart(void __iomem *regs,
					   struct pm_uart_save *save)
{
	/* nothing here yet */
}

