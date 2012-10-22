/* linux/arch/arm/mach-s5pv210/cpufreq-m9w.c
 *
 * Copyright (c) 2012 Meizu Tech Co., Ltd.
 *		http://www.meizu.com
 *
 * CPU frequency scaling for S5PC110/S5PV210
 *
 * Author: Cao Ziqiang <caoziqiang@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include<linux/delay.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/cpu-freq-v210.h>

static struct clk *cpu_clk;
static struct clk *dmc0_clk;
static struct clk *dmc1_clk;
static struct cpufreq_freqs freqs;
static DEFINE_MUTEX(set_freq_lock);
static DEFINE_MUTEX(set_level_lock);

static struct cpufreq_driver s5pv210_driver;
/* APLL M,P,S values for 1G/800Mhz */
#define APLL_VAL_1200	((1 << 31) | (150 << 16) | (3 << 8) | 1)
#define APLL_VAL_1000	((1 << 31) | (125 << 16) | (3 << 8) | 1)
#define APLL_VAL_800	((1 << 31) | (100 << 16) | (3 << 8) | 1)
#define APLL_VAL_600	((1 << 31) | (75 << 16) | (3 << 8) | 1)

#define SLEEP_FREQ	(800 * 1000) /* Use 800MHz when entering sleep */

/*
 * relation has an additional symantics other than the standard of cpufreq
 *	DISALBE_FURTHER_CPUFREQ: disable further access to target until being re-enabled.
 *	ENABLE_FURTUER_CPUFREQ: re-enable access to target
*/
enum cpufreq_access {
	DISABLE_FURTHER_CPUFREQ = 0x10,
	ENABLE_FURTHER_CPUFREQ = 0x20,
};
static bool no_cpufreq_access;

/*
 * DRAM configurations to calculate refresh counter for changing
 * frequency of memory.
 */
struct dram_conf {
	unsigned long freq;	/* HZ */
	unsigned long refresh;	/* DRAM refresh counter * 1000 */
};

/* DRAM configuration (DMC0 and DMC1) */
static struct dram_conf s5pv210_dram_conf[2];

enum perf_level {
	L0, L1, L2, L3, L4, L5, L6
};

enum s5pv210_mem_type {
	LPDDR	= 0x1,
	LPDDR2	= 0x2,
	DDR2	= 0x4,
};

enum s5pv210_dmc_port {
	DMC0 = 0,
	DMC1,
};
/* here we show all of frequency level supported. Just for showing.*/
static struct cpufreq_policy policy_full = {
	.min = 100*1000,
	.max = 1200*1000,
};
static struct cpufreq_frequency_table s5pv210_freq_table_full[] = {
	{L0, 1200*1000},
	{L1, 1000*1000},
	{L2, 800*1000},
	{L3, 600*1000},
	{L4, 400*1000},
	{L5, 200*1000},
	{L6, 100*1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_frequency_table s5pv210_freq_table_high[] = {
	{L0, 1200*1000},
	{L1, 1000*1000},
	{L2, 800*1000},
	{L4, 400*1000},
//	{L5, 200*1000},
	{L6, 100*1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_frequency_table s5pv210_freq_table_med[] = {
	//{L1, 1000*1000},
	{L2, 800*1000},
	{L3, 600*1000},
	{L4, 400*1000},
//	{L5, 200*1000},
	{L6, 100*1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_frequency_table s5pv210_freq_table_low[] = {
	{L3, 600*1000},
	{L4, 400*1000},
	{L5, 200*1000},
	{L6, 100*1000},
	{0, CPUFREQ_TABLE_END},
};

static struct regulator *arm_regulator;
static struct regulator *internal_regulator;

struct s5pv210_dvs_conf {
	unsigned long	arm_volt; /* uV */
	unsigned long	int_volt; /* uV */
};

const unsigned long arm_volt_max = 1350000;
const unsigned long int_volt_max = 1250000;

static struct s5pv210_dvs_conf dvs_conf[] = {
	[L0] = {
		.arm_volt   = 1250000,
		.int_volt   = 1100000,
	},
	[L1] = {
		.arm_volt   = 1200000,
		.int_volt   = 1100000,
	},
	[L2] = {
		.arm_volt   = 1100000,
		.int_volt   = 1100000,
	},
	[L3] = {
		.arm_volt   = 1050000,
		.int_volt   = 1100000,
	},
	[L4] = {
		.arm_volt   = 1050000,
		.int_volt   = 1100000,
	},
	[L5] = {
		.arm_volt   = 950000,
		.int_volt   = 1100000,
	},
	[L6] = {
		.arm_volt   = 950000,
		.int_volt   = 1000000,
	},
};
static const u32 clkdiv_val[][11] = {
	/*{ APLL, A2M, HCLK_MSYS, PCLK_MSYS,
	 * HCLK_DSYS, PCLK_DSYS, HCLK_PSYS, PCLK_PSYS, ONEDRAM,
	 * MFC, G3D }
	 */
	/* L0 : [1200/200/200/100][166/83][133/66][200/200] */
	{0, 5, 5, 1, 3, 1, 4, 1, 3, 0, 0},
	/* L1 : [1000/200/200/100][166/83][133/66][200/200] */
	{0, 4, 4, 1, 3, 1, 4, 1, 3, 0, 0},
	/* L2 : [800/200/200/100][166/83][133/66][200/200] */
	{0, 3, 3, 1, 3, 1, 4, 1, 3, 0, 0},
	/* L3 : [600/200/200/100][166/83][133/66][200/200] */
	{0, 2, 2, 1, 3, 1, 4, 1, 3, 0, 0},
	/* L4 : [400/200/200/100][166/83][133/66][200/200] */
	{1, 3, 1, 1, 3, 1, 4, 1, 3, 0, 0},
	/* L5 : [200/200/200/100][166/83][133/66][200/200] */
	{3, 3, 0, 1, 3, 1, 4, 1, 3, 0, 0},
	/* L6 : [100/100/100/100][83/83][66/66][100/100] */
	{7, 7, 0, 0, 7, 0, 9, 0, 7, 0, 0},
};

static struct cpufreq_frequency_table * m9w_get_table(void)
{
	struct cpufreq_frequency_table *tab;

	switch (atomic_read(&s5pv210_driver.level)) {
	case CPU_FREQ_LEVEL_HIGH:
		tab = s5pv210_freq_table_high;
		break;
	case CPU_FREQ_LEVEL_LOW:
		tab = s5pv210_freq_table_low;
		break;
	case CPU_FREQ_LEVEL_MED:
	default:
		tab = s5pv210_freq_table_med;
		break;
	}
	return tab;
}

int s5pv210_verify_speed(struct cpufreq_policy *policy)
{
	if (policy->cpu)
		return -EINVAL;

	return cpufreq_frequency_table_verify(policy, m9w_get_table());
}

unsigned int s5pv210_getspeed(unsigned int cpu)
{
	if (cpu)
		return 0;

	return clk_get_rate(cpu_clk) / 1000;
}

static int s5pv210_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_frequency_table * cur_tab = m9w_get_table();
	unsigned long reg;
	unsigned int index, priv_index;
	unsigned int pll_changing = 0;
	unsigned int bus_speed_changing = 0;
	unsigned int arm_volt, int_volt;
	int ret = 0, index_tmp = 0;
	bool down_case = true;

	mutex_lock(&set_freq_lock);

	if (relation & ENABLE_FURTHER_CPUFREQ)
		no_cpufreq_access = false;
	if (no_cpufreq_access) {
#ifdef CONFIG_PM_VERBOSE
		pr_err("%s:%d denied access to %s as it is disabled"
				"temporarily\n", __FILE__, __LINE__, __func__);
#endif
		ret = -EINVAL;
		goto out;
	}
	if (relation & DISABLE_FURTHER_CPUFREQ)
		no_cpufreq_access = true;

	relation &= ~(ENABLE_FURTHER_CPUFREQ | DISABLE_FURTHER_CPUFREQ);

	freqs.old = s5pv210_getspeed(0);

	if (cpufreq_frequency_table_target(policy, cur_tab,
					   target_freq, relation, &index_tmp)) {
		ret = -EINVAL;
		goto out;
	}

	freqs.new = cur_tab[index_tmp].frequency;
	freqs.cpu = 0;

	if (freqs.new == freqs.old)
		goto out;

	index = cur_tab[index_tmp].index;
//	pr_info("%s change from %d to %d\n", __func__, freqs.old, freqs.new);
	/* Finding current running level index */
	policy_full.cpu = policy->cpu;
	if (cpufreq_frequency_table_target(&policy_full, s5pv210_freq_table_full,
					   freqs.old, relation, &priv_index)) {
		ret = -EINVAL;
		goto out;
	}

	arm_volt = dvs_conf[index].arm_volt;
	int_volt = dvs_conf[index].int_volt;

	if (freqs.new > freqs.old) {
		/* Voltage up code: increase ARM first */
		if (!IS_ERR_OR_NULL(arm_regulator) &&
				!IS_ERR_OR_NULL(internal_regulator)) {
			ret = regulator_set_voltage(arm_regulator,
						    arm_volt, arm_volt_max);
			if (ret)
				goto out;
			ret = regulator_set_voltage(internal_regulator,
						    int_volt, int_volt_max);
			if (ret)
				goto out;
		}
		down_case = false;
	} else
		down_case = true;

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	/* Check if there need to change PLL */
	/* If the performance level is LOW, then we should consider the 600MHz
	 * settings, because, it is not multiple of 200MHz.*/
	if (index != priv_index)
		pll_changing = 1;

	/* Check if there need to change System bus clock */
	/* Here we only change the bus frequency at or from L6(100MHz).*/
	if ((index == L6) || (priv_index == L6))
		bus_speed_changing = 1;

	if (bus_speed_changing) {
		/*
		 * Reconfigure DRAM refresh counter value for minimum
		 * temporary clock while changing divider.
		 * expected clock is 83Mhz : 7.8usec/(1/83Mhz) = 0x287
		 */
		if (pll_changing)
			__raw_writel(0x30c, S5P_VA_DMC1 + 0x30);
		else
			__raw_writel(0x287, S5P_VA_DMC1 + 0x30);

		__raw_writel(0x287, S5P_VA_DMC0 + 0x30);
	}

	/*
	 * APLL should be changed in this level
	 * APLL -> MPLL(for stable transition) -> APLL
	 * Some clock source's clock API are not prepared.
	 * Do not use clock API in below code.
	 */
	if (pll_changing) {

#ifndef CONFIG_FIXED_MEDIA_FREQ
		/*
		 * 1. Temporary Change divider for MFC and G3D
		 * SCLKA2M(200/1=200)->(200/4=50)Mhz
		 */
		reg = __raw_readl(S5P_CLK_DIV2);
		reg &= ~(S5P_CLKDIV2_G3D_MASK | S5P_CLKDIV2_MFC_MASK);
		reg |= (3 << S5P_CLKDIV2_G3D_SHIFT) |
			(3 << S5P_CLKDIV2_MFC_SHIFT);
		__raw_writel(reg, S5P_CLK_DIV2);

		/* For MFC, G3D dividing */
		do {
			reg = __raw_readl(S5P_CLKDIV_STAT0);
		} while (reg & ((1 << 16) | (1 << 17)));

		/*
		 * 2. Change SCLKA2M(200Mhz)to SCLKMPLL in MFC_MUX, G3D MUX
		 * (200/4=50)->(667/4=166)Mhz
		 */
		reg = __raw_readl(S5P_CLK_SRC2);
		reg &= ~(S5P_CLKSRC2_G3D_MASK | S5P_CLKSRC2_MFC_MASK);
		reg |= (1 << S5P_CLKSRC2_G3D_SHIFT) |
			(1 << S5P_CLKSRC2_MFC_SHIFT);
		__raw_writel(reg, S5P_CLK_SRC2);

		do {
			reg = __raw_readl(S5P_CLKMUX_STAT1);
		} while (reg & ((1 << 7) | (1 << 3)));
#endif
		/*
		 * 3. DMC1 refresh count for 133Mhz if (index == L4) is
		 * true refresh counter is already programed in upper
		 * code. 0x287@83Mhz
		 */
		if (!bus_speed_changing)
			__raw_writel(0x40D,S5P_VA_DMC0+0x30);

		/* 4. SCLKAPLL -> SCLKMPLL */
		reg = __raw_readl(S5P_CLK_SRC0);
		reg &= ~(S5P_CLKSRC0_MUX200_MASK);
		reg |= (0x1 << S5P_CLKSRC0_MUX200_SHIFT);
		__raw_writel(reg, S5P_CLK_SRC0);

		do {
			reg = __raw_readl(S5P_CLKMUX_STAT0);
		} while (reg & (0x1 << 18));

	}
	if ((!down_case) && (index < L4)) {
		reg = __raw_readl(S5P_ARM_MCS_CON);
		reg &= ~0x3;
		reg |= 0x1;
		__raw_writel(reg, S5P_ARM_MCS_CON);
	}
	/* Change divider */
	reg = __raw_readl(S5P_CLK_DIV0);

	reg &= ~(S5P_CLKDIV0_APLL_MASK | S5P_CLKDIV0_A2M_MASK |
		S5P_CLKDIV0_HCLK200_MASK | S5P_CLKDIV0_PCLK100_MASK |
		S5P_CLKDIV0_HCLK166_MASK | S5P_CLKDIV0_PCLK83_MASK |
		S5P_CLKDIV0_HCLK133_MASK | S5P_CLKDIV0_PCLK66_MASK);

	reg |= ((clkdiv_val[index][0] << S5P_CLKDIV0_APLL_SHIFT) |
		(clkdiv_val[index][1] << S5P_CLKDIV0_A2M_SHIFT) |
		(clkdiv_val[index][2] << S5P_CLKDIV0_HCLK200_SHIFT) |
		(clkdiv_val[index][3] << S5P_CLKDIV0_PCLK100_SHIFT) |
		(clkdiv_val[index][4] << S5P_CLKDIV0_HCLK166_SHIFT) |
		(clkdiv_val[index][5] << S5P_CLKDIV0_PCLK83_SHIFT) |
		(clkdiv_val[index][6] << S5P_CLKDIV0_HCLK133_SHIFT) |
		(clkdiv_val[index][7] << S5P_CLKDIV0_PCLK66_SHIFT));

	__raw_writel(reg, S5P_CLK_DIV0);

	do {
		reg = __raw_readl(S5P_CLKDIV_STAT0);
	} while (reg & 0xff);

	if (pll_changing) {
		/* 5. Set Lock time = 30us*24Mhz = 0x2cf */
		__raw_writel(0x2cf, S5P_APLL_LOCK);

		/*
		 * 6. Turn on APLL
		 * 6-1. Set PMS values
		 * 6-2. Wait untile the PLL is locked
		 */
		if (index == L0)
			__raw_writel(APLL_VAL_1200, S5P_APLL_CON);
		else if (index == L1)
			__raw_writel(APLL_VAL_1000, S5P_APLL_CON);
		else if (index == L3)
			__raw_writel(APLL_VAL_600, S5P_APLL_CON);
		else
			__raw_writel(APLL_VAL_800, S5P_APLL_CON);


		do {
			reg = __raw_readl(S5P_APLL_CON);
		} while (!(reg & (0x1 << 29)));
#ifndef CONFIG_FIXED_MEDIA_FREQ
		/*
		 * 7. Change souce clock from SCLKMPLL(667Mhz)
		 * to SCLKA2M(200Mhz) in MFC_MUX and G3D MUX
		 * (667/4=166)->(200/4=50)Mhz
		 */
		reg = __raw_readl(S5P_CLK_SRC2);
		reg &= ~(S5P_CLKSRC2_G3D_MASK | S5P_CLKSRC2_MFC_MASK);
		reg |= (0 << S5P_CLKSRC2_G3D_SHIFT) |
			(0 << S5P_CLKSRC2_MFC_SHIFT);
		__raw_writel(reg, S5P_CLK_SRC2);

		do {
			reg = __raw_readl(S5P_CLKMUX_STAT1);
		} while (reg & ((1 << 7) | (1 << 3)));

		/*
		 * 8. Change divider for MFC and G3D
		 * (200/4=50)->(200/1=200)Mhz
		 */
		reg = __raw_readl(S5P_CLK_DIV2);
		reg &= ~(S5P_CLKDIV2_G3D_MASK | S5P_CLKDIV2_MFC_MASK);
		reg |= (clkdiv_val[index][10] << S5P_CLKDIV2_G3D_SHIFT) |
			(clkdiv_val[index][9] << S5P_CLKDIV2_MFC_SHIFT);
		__raw_writel(reg, S5P_CLK_DIV2);

		/* For MFC, G3D dividing */
		do {
			reg = __raw_readl(S5P_CLKDIV_STAT0);
		} while (reg & ((1 << 16) | (1 << 17)));
		/* 9. Change MPLL to APLL in MSYS_MUX */
		reg = __raw_readl(S5P_CLK_SRC0);
		reg &= ~(S5P_CLKSRC0_MUX200_MASK);
		reg |= (0x0 << S5P_CLKSRC0_MUX200_SHIFT);
		__raw_writel(reg, S5P_CLK_SRC0);

		do {
			reg = __raw_readl(S5P_CLKMUX_STAT0);
		} while (reg & (0x1 << 18));

#endif
		/*
		 * 10. DMC1 refresh counter
		 * L4 : DMC1 = 100Mhz 7.8us/(1/100) = 0x30c
		 * Others : DMC1 = 200Mhz 7.8us/(1/200) = 0x618
		 */
		if (!bus_speed_changing)
			__raw_writel(0x618, S5P_VA_DMC1 + 0x30);
	}

	/*
	 * L6 level need to change memory bus speed, hence onedram clock divier
	 * and memory refresh parameter should be changed
	 */
	if (bus_speed_changing) {
		reg = __raw_readl(S5P_CLK_DIV6);
		reg &= ~S5P_CLKDIV6_ONEDRAM_MASK;
		reg |= (clkdiv_val[index][8] << S5P_CLKDIV6_ONEDRAM_SHIFT);
		__raw_writel(reg, S5P_CLK_DIV6);

		do {
			reg = __raw_readl(S5P_CLKDIV_STAT1);
		} while (reg & (1 << 15));

		/* Reconfigure DRAM refresh counter value */
		if (index != L6) {
			/*
			 * DMC0 : 166Mhz
			 * DMC1 : 200Mhz
			 */
			__raw_writel(0x514, S5P_VA_DMC0 + 0x30);
			__raw_writel(0x618, S5P_VA_DMC1 + 0x30);
		} else {
			/*
			 * DMC0 : 83Mhz
			 * DMC1 : 100Mhz
			 */
			__raw_writel(0x287, S5P_VA_DMC0 + 0x30);
			__raw_writel(0x30c, S5P_VA_DMC1 + 0x30);
		}
	}

	/* ARM MCS value changed */
	
	if ((down_case) && (index >= L4)) {
		reg = __raw_readl(S5P_ARM_MCS_CON);
		reg &= ~0x3;
		reg |= 0x3;
		__raw_writel(reg, S5P_ARM_MCS_CON);
	}


	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	if (freqs.new < freqs.old) {
		/* Voltage down: decrease INT first */
		if (!IS_ERR_OR_NULL(arm_regulator) &&
				!IS_ERR_OR_NULL(internal_regulator)) {
			regulator_set_voltage(internal_regulator,
					int_volt, int_volt_max);
			regulator_set_voltage(arm_regulator,
					arm_volt, arm_volt_max);
		}
	}
	pr_debug("Perf changed[L%d]\n", index);
out:
	mutex_unlock(&set_freq_lock);
	return ret;
}
#ifdef CONFIG_CPU_PERF_LEVEL
#ifdef CONFIG_FIXED_MEDIA_FREQ
static int s5pv210_fixed_media_freq(unsigned int level)
{
	struct clk *mfc_clk = NULL;
	struct clk *g3d_clk = NULL;
	unsigned int g3d_freq, mfc_freq;

	mfc_clk = clk_get(NULL, "sclk_mfc");
	if (!mfc_clk) {
		pr_err("can not get mfc clk!\n");
		return -ENOENT;
	}

	g3d_clk = clk_get(NULL, "sclk_g3d");
	if (!g3d_clk) {
		pr_err("can not get g3d clk!\n");
		clk_put(mfc_clk);
		return -ENOENT;
	}

	switch (level) {
		case CPU_FREQ_LEVEL_MED:
			g3d_freq = 166750000;
			mfc_freq = 166750000;
			break;
		case CPU_FREQ_LEVEL_LOW:
			mfc_freq = 133400000;
			g3d_freq = 133400000;
			break;
		case CPU_FREQ_LEVEL_HIGH:
			mfc_freq = 166750000;
			g3d_freq = 250000000;
			break;
		default:
			g3d_freq = 166750000;
			mfc_freq = 166750000;
			break;
	}

	clk_set_rate(mfc_clk, mfc_freq);
	clk_set_rate(g3d_clk, g3d_freq);

	clk_put(mfc_clk);
	clk_put(g3d_clk);
	return 0;
}
#endif
static int s5pv210_setlevel(struct cpufreq_policy *policy, unsigned int level)
{
	struct cpufreq_frequency_table *tab;
	int ret = 0;

	mutex_lock(&set_level_lock);

	if (level == atomic_read(&s5pv210_driver.level)) {
		mutex_unlock(&set_level_lock);
		return 0;
	}

	atomic_set(&s5pv210_driver.level, level);

	tab = m9w_get_table();
	ret = cpufreq_frequency_table_cpuinfo(policy, tab);
	if (!ret) {
		cpufreq_frequency_table_get_attr(tab, policy->cpu);
		policy->user_policy.max = policy->cpuinfo.max_freq;
		policy->user_policy.min = policy->cpuinfo.min_freq;
#ifdef CONFIG_CPU_FREQ_GOV_SMOOTH
		/* For any level ,use the second index frequency.
		 */
		policy->proper = tab[1].frequency;
#endif

	} else {
		pr_err("%s set frequency of cpuinfo failure!\n", __func__);
	}

#ifdef CONFIG_FIXED_MEDIA_FREQ
	if (s5pv210_fixed_media_freq(level))
		pr_err("%s set fixed media freq!\n", __func__);
#endif
	mutex_unlock(&set_level_lock);
	return ret;

}

static int s5pv210_getlevel(struct cpufreq_policy *policy, unsigned int cpu)
{
	return atomic_read(&s5pv210_driver.level);
}
#endif
#ifdef CONFIG_PM
static int s5pv210_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int s5pv210_cpufreq_resume(struct cpufreq_policy *policy)
{
	return 0;
}
#endif

static int check_mem_type(void __iomem *dmc_reg)
{
	unsigned long val;

	val = __raw_readl(dmc_reg + 0x4);
	val = (val & (0xf << 8));

	return val >> 8;
}

static int s5pv210_cpu_init(struct cpufreq_policy *policy)
{
	unsigned long mem_type;

	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	dmc0_clk = clk_get(NULL, "sclk_dmc0");
	if (IS_ERR(dmc0_clk)) {
		clk_put(cpu_clk);
		return PTR_ERR(dmc0_clk);
	}

	dmc1_clk = clk_get(NULL, "hclk_msys");
	if (IS_ERR(dmc1_clk)) {
		clk_put(dmc0_clk);
		clk_put(cpu_clk);
		return PTR_ERR(dmc1_clk);
	}

	if (policy->cpu != 0)
		return -EINVAL;

	/*
	 * check_mem_type : This driver only support LPDDR & LPDDR2.
	 * other memory type is not supported.
	 */
	mem_type = check_mem_type(S5P_VA_DMC0);

	if ((mem_type != LPDDR) && (mem_type != LPDDR2)) {
		printk(KERN_ERR "CPUFreq doesn't support this memory type\n");
		return -EINVAL;
	}

	/* Find current refresh counter and frequency each DMC */
	s5pv210_dram_conf[0].refresh = (__raw_readl(S5P_VA_DMC0 + 0x30) * 1000);
	s5pv210_dram_conf[0].freq = clk_get_rate(dmc0_clk);

	s5pv210_dram_conf[1].refresh = (__raw_readl(S5P_VA_DMC1 + 0x30) * 1000);
	s5pv210_dram_conf[1].freq = clk_get_rate(dmc1_clk);

	policy->cur = policy->min = policy->max = s5pv210_getspeed(0);
#ifdef CONFIG_CPU_FREQ_GOV_SMOOTH
	policy->proper = policy->max; /*for temporarily testing*/
#endif
	cpufreq_frequency_table_get_attr(m9w_get_table(), policy->cpu);

	policy->cpuinfo.transition_latency = 400000;

	return cpufreq_frequency_table_cpuinfo(policy, m9w_get_table());
}

static int s5pv210_cpufreq_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	int ret;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		ret = cpufreq_driver_target(cpufreq_cpu_get(0), SLEEP_FREQ,
				DISABLE_FURTHER_CPUFREQ);
		if (ret < 0)
			return NOTIFY_BAD;
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		cpufreq_driver_target(cpufreq_cpu_get(0), SLEEP_FREQ,
				ENABLE_FURTHER_CPUFREQ);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static int s5pv210_cpufreq_reboot_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	int ret = 0;

	ret = cpufreq_driver_target(cpufreq_cpu_get(0), SLEEP_FREQ,
			DISABLE_FURTHER_CPUFREQ);
	if (ret < 0)
		return NOTIFY_BAD;

	return NOTIFY_DONE;
}

static struct cpufreq_driver s5pv210_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= s5pv210_verify_speed,
	.target		= s5pv210_target,
	.get		= s5pv210_getspeed,
	.init		= s5pv210_cpu_init,
	.name		= "s5pv210",
#ifdef CONFIG_CPU_PERF_LEVEL
	.setlevel	= s5pv210_setlevel,
	.getlevel	= s5pv210_getlevel,
#endif
#ifdef CONFIG_PM
	.suspend	= s5pv210_cpufreq_suspend,
	.resume		= s5pv210_cpufreq_resume,
#endif
};

static struct notifier_block s5pv210_cpufreq_notifier = {
	.notifier_call = s5pv210_cpufreq_notifier_event,
};

static struct notifier_block s5pv210_cpufreq_reboot_notifier = {
	.notifier_call	= s5pv210_cpufreq_reboot_notifier_event,
};

static int s5pv210_cpufreq_probe(struct platform_device *pdev)
{
	struct s5pv210_cpufreq_data *pdata = dev_get_platdata(&pdev->dev);
	struct cpufreq_frequency_table * cur_tab = m9w_get_table();
	int i, j;

	if (pdata && pdata->size) {
		for (i = 0; i < pdata->size; i++) {
			j = 0;
			while (cur_tab[j].frequency != CPUFREQ_TABLE_END) {
				if (cur_tab[j].frequency == pdata->volt[i].freq) {
					dvs_conf[j].arm_volt = pdata->volt[i].varm;
					dvs_conf[j].int_volt = pdata->volt[i].vint;
					break;
				}
				j++;
			}
		}
	}

	arm_regulator = regulator_get(NULL, "vddarm");
	if (IS_ERR(arm_regulator)) {
		pr_err("failed to get regulater resource vddarm\n");
		goto error;
	}
	internal_regulator = regulator_get(NULL, "vddint");
	if (IS_ERR(internal_regulator)) {
		pr_err("failed to get regulater resource vddint\n");
		goto error;
	}
	goto finish;
error:
	pr_warn("Cannot get vddarm or vddint. CPUFREQ Will not"
		       " change the voltage.\n");
finish:
	atomic_set(&s5pv210_driver.level, CPU_FREQ_LEVEL_MED);

	register_pm_notifier(&s5pv210_cpufreq_notifier);
	register_reboot_notifier(&s5pv210_cpufreq_reboot_notifier);

#ifdef CONFIG_FIXED_MEDIA_FREQ
	if (s5pv210_fixed_media_freq(CPU_FREQ_LEVEL_MED))
		pr_err("%s set fixed media freq!\n", __func__);
#endif

	return cpufreq_register_driver(&s5pv210_driver);
}

static struct platform_driver s5pv210_cpufreq_drv = {
	.probe		= s5pv210_cpufreq_probe,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s5pv210-cpufreq",
	},
};

static int __init m9w_cpufreq_init(void)
{
	int ret;

	ret = platform_driver_register(&s5pv210_cpufreq_drv);
	if (!ret)
		pr_info("%s: Meizu M9W cpu-freq driver\n", __func__);

	return ret;
}

late_initcall(m9w_cpufreq_init);
