/* linux/arch/arm/mach-s5pv210/pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * S5PV210 - Power Management support
 *
 * Based on arch/arm/mach-s3c2410/pm.c
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/regs-timer.h>

#include <mach/regs-gpio.h>
#include <mach/regs-irq.h>
#include <mach/regs-clock.h>
#include <mach/regs-mem.h>
#include <mach/power-domain.h>
#include <mach/gpio-m9w.h>

int (*s5p_config_sleep_gpio_table)(void);

static struct sleep_save core_save[] = {
	/* PLL Control */
	SAVE_ITEM(S5P_APLL_CON),
	SAVE_ITEM(S5P_MPLL_CON),
	SAVE_ITEM(S5P_EPLL_CON),
	SAVE_ITEM(S5P_VPLL_CON),

	/* Clock source */
	SAVE_ITEM(S5P_CLK_SRC0),
	SAVE_ITEM(S5P_CLK_SRC1),
	SAVE_ITEM(S5P_CLK_SRC2),
	SAVE_ITEM(S5P_CLK_SRC3),
	SAVE_ITEM(S5P_CLK_SRC4),
	SAVE_ITEM(S5P_CLK_SRC5),
	SAVE_ITEM(S5P_CLK_SRC6),

	/* Clock source Mask */
	SAVE_ITEM(S5P_CLK_SRC_MASK0),
	SAVE_ITEM(S5P_CLK_SRC_MASK1),

	/* Clock Divider */
	SAVE_ITEM(S5P_CLK_DIV0),
	SAVE_ITEM(S5P_CLK_DIV1),
	SAVE_ITEM(S5P_CLK_DIV2),
	SAVE_ITEM(S5P_CLK_DIV3),
	SAVE_ITEM(S5P_CLK_DIV4),
	SAVE_ITEM(S5P_CLK_DIV5),
	SAVE_ITEM(S5P_CLK_DIV6),
	SAVE_ITEM(S5P_CLK_DIV7),

	/* Clock Main Gate */
	SAVE_ITEM(S5P_CLKGATE_MAIN0),
	SAVE_ITEM(S5P_CLKGATE_MAIN1),
	SAVE_ITEM(S5P_CLKGATE_MAIN2),

	/* Clock source Peri Gate */
	SAVE_ITEM(S5P_CLKGATE_PERI0),
	SAVE_ITEM(S5P_CLKGATE_PERI1),

	/* Clock source SCLK Gate */
	SAVE_ITEM(S5P_CLKGATE_SCLK0),
	SAVE_ITEM(S5P_CLKGATE_SCLK1),

	/* Clock IP Clock gate */
	SAVE_ITEM(S5P_CLKGATE_IP0),
	SAVE_ITEM(S5P_CLKGATE_IP1),
	SAVE_ITEM(S5P_CLKGATE_IP2),
	SAVE_ITEM(S5P_CLKGATE_IP3),
	SAVE_ITEM(S5P_CLKGATE_IP4),

	/* Clock Blcok and Bus gate */
	SAVE_ITEM(S5P_CLKGATE_BLOCK),
	SAVE_ITEM(S5P_CLKGATE_IP5),

	/* Clock ETC */
	SAVE_ITEM(S5P_CLK_OUT),
	SAVE_ITEM(S5P_MDNIE_SEL),

	/* PWM Register */
	SAVE_ITEM(S3C2410_TCFG0),
	SAVE_ITEM(S3C2410_TCFG1),
	SAVE_ITEM(S3C64XX_TINT_CSTAT),
	SAVE_ITEM(S3C2410_TCON),
	SAVE_ITEM(S3C2410_TCNTB(0)),
	SAVE_ITEM(S3C2410_TCMPB(0)),
	SAVE_ITEM(S3C2410_TCNTO(0)),
};

#ifdef CONFIG_MACH_MEIZU_M9W
static unsigned long m9w_wakeup_type;
wake_type_t m9w_get_wakeup_type(void)
{
	return m9w_wakeup_type;
}

int m9w_set_wakeup_type(unsigned int mask)
{
	return mask ? (m9w_wakeup_type |= mask) :
				  (m9w_wakeup_type = mask);
}
#endif

#if 0
static struct sleep_save sromc_save[] = {
	SAVE_ITEM(S5P_SROM_BW),
	SAVE_ITEM(S5P_SROM_BC0),
	SAVE_ITEM(S5P_SROM_BC1),
	SAVE_ITEM(S5P_SROM_BC2),
	SAVE_ITEM(S5P_SROM_BC3),
	SAVE_ITEM(S5P_SROM_BC4),
	SAVE_ITEM(S5P_SROM_BC5),
};
#endif
void s5pv210_cpu_suspend(void)
{
	unsigned long tmp;

	/* issue the standby signal into the pm unit. Note, we
	 * issue a write-buffer drain just in case */

	tmp = 0;
/*
	asm("b 1f\n\t"
	    ".align 5\n\t"
	    "1:\n\t"
	    "mcr p15, 0, %0, c7, c10, 5\n\t"
	    "mcr p15, 0, %0, c7, c10, 4\n\t"
	    "wfi" : : "r" (tmp));
*/
	cpu_do_idle();
	/* we should never get past here */
	panic("sleep resumed to originator?");
}

static bool pm_wakeup_full = true;
void set_pm_wakeup_full(bool val)
{
	pm_wakeup_full = !!val;
}

bool get_pm_wakeup_full(void)
{
	return pm_wakeup_full;
}

static void s5pv210_pm_prepare(void)
{
	unsigned int tmp;
#ifdef CONFIG_MACH_MEIZU_M9W
	unsigned int mask, i;
#endif

	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), S5P_INFORM0);

	/* WFI for SLEEP mode configuration by SYSCON */
	tmp = __raw_readl(S5P_PWR_CFG);
	tmp &= S5P_CFG_WFI_CLEAN;
	tmp |= S5P_CFG_WFI_SLEEP;
	__raw_writel(tmp, S5P_PWR_CFG);

	/* SYSCON interrupt handling disable */
	tmp = __raw_readl(S5P_OTHERS);
	tmp |= S5P_OTHER_SYSC_INTOFF;
	__raw_writel(tmp, S5P_OTHERS);

	__raw_writel(0xffffffff, (VA_VIC0 + VIC_INT_ENABLE_CLEAR));
	__raw_writel(0xffffffff, (VA_VIC1 + VIC_INT_ENABLE_CLEAR));
	__raw_writel(0xffffffff, (VA_VIC2 + VIC_INT_ENABLE_CLEAR));
	__raw_writel(0xffffffff, (VA_VIC3 + VIC_INT_ENABLE_CLEAR));
	/* runtime pm sleep pin config */
	if (s5p_config_sleep_gpio_table)
		s5p_config_sleep_gpio_table();

#ifdef CONFIG_MACH_MEIZU_M9W
	/* to clear pending of unmasked eint */
	for (i=0; i<4; i++) {
		mask = (s3c_irqwake_eintmask>>(i*8)) & 0xff;
		__raw_writel(mask, S5P_EINT_PEND(i));
		mask = __raw_readl(S5P_EINT_PEND(i));
		pr_debug("S5PC11X_EINTPEND%d = 0x%02x\n", i, mask);
	}
#endif
	s3c_pm_do_save(core_save, ARRAY_SIZE(core_save));
}

static int s5pv210_pm_add(struct sys_device *sysdev)
{
	pm_cpu_prep = s5pv210_pm_prepare;
	pm_cpu_sleep = s5pv210_cpu_suspend;

	return 0;
}

static struct sysdev_driver s5pv210_pm_driver = {
	.add		= s5pv210_pm_add,
};

static __init int s5pv210_pm_drvinit(void)
{
	return sysdev_driver_register(&s5pv210_sysclass, &s5pv210_pm_driver);
}
arch_initcall(s5pv210_pm_drvinit);

static void s5pv210_pm_resume(void)
{
	u32 tmp, audiodomain_on;

	tmp = __raw_readl(S5P_NORMAL_CFG);
	if (tmp & S5PV210_PD_AUDIO)
		audiodomain_on = 0;
	else {
		tmp |= S5PV210_PD_AUDIO;
		__raw_writel(tmp , S5P_NORMAL_CFG);
		audiodomain_on = 1;
	}

	tmp = __raw_readl(S5P_OTHERS);
	tmp |= (S5P_OTHERS_RET_IO | S5P_OTHERS_RET_CF |\
		S5P_OTHERS_RET_MMC | S5P_OTHERS_RET_UART);
	__raw_writel(tmp , S5P_OTHERS);

	if (audiodomain_on) {
		tmp = __raw_readl(S5P_NORMAL_CFG);
		tmp &= ~S5PV210_PD_AUDIO;
		__raw_writel(tmp , S5P_NORMAL_CFG);
	}

	s3c_pm_do_restore_core(core_save, ARRAY_SIZE(core_save));
}

static struct syscore_ops s5pv210_pm_syscore_ops = {
	.resume		= s5pv210_pm_resume,
};

static __init int s5pv210_pm_syscore_init(void)
{
	register_syscore_ops(&s5pv210_pm_syscore_ops);
	return 0;
}
arch_initcall(s5pv210_pm_syscore_init);
