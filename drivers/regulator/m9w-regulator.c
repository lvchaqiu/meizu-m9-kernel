/*
 * m9w-regulator.c - Voltage regulator driver for the Linear ltc3577-3
 *
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 * Author: 	lvcha qiu	<lvcha@meizu.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/ltc3577.h>
#include <linux/mfd/ltc3577-private.h>
#include <mach/gpio-m9w.h>
#include <mach/regs-gpio.h>

struct ltc3577_data {
	struct device		*dev;
	struct ltc3577_dev	*iodev;
	int			num_regulators;
	struct regulator_dev	**rdev;
};

/**
 * struct voltage_map_desc - regulator operating constraints.
 */
struct voltage_map_desc {
	int min_mV;
	int max_mV;
	int step_mV;
};

/*1=>input; 0==>output 0*/
static const unsigned int arm_voltage_table[][3] = {
    /* GPJ0(5)		  GPJ0(2)		   GPJ0(4)*/
    /*ARM_ON0	ARM_ON1		ARM_ON2*/
	{   1,		   	0,			1 },		/*ARM_VOUT  0_95*/
	{   0,		   	1,			1 },		/*ARM_VOUT 1_00*/
	{   1,		   	0,			0 },		/*ARM_VOUT 1_05*/
	{   0,		   	1,			0 },		/*ARM_VOUT 1_10*/
	{   0,		   	0,			1 },		/*ARM_VOUT 1_15*/
	{   0,		   	0,			1 },		/*ARM_VOUT 1_20*/
	{   0,		   	0,			0 },	    	/*ARM_VOUT 1_25*/
};

/*1=>input; 0==>output 0*/
static const unsigned int int_voltage_table[2] = {
    1,  /*INT 1.0v*/
    0   /*INT 1.1v*/
}; 

/* 1.25v ARM CORE */
static const struct voltage_map_desc buck3_voltage_map_desc = {
	.min_mV = 950,	.step_mV = 50,	.max_mV = 1250,
};
/* 1.10v ARM INT */
static const struct voltage_map_desc buck4_voltage_map_desc = {
	.min_mV = 1000,	.step_mV = 100,	.max_mV = 1100,
};

static const struct voltage_map_desc *ltc3577_voltage_map[] = {
	&buck3_voltage_map_desc,	/* BUCK3 ARM CORE */
	&buck4_voltage_map_desc,	/* BUCK4 ARM INT */
};

static inline int ltc3577_get_ldo(struct regulator_dev *rdev)
{
	return rdev_get_id(rdev);
}

static int ltc3577_list_voltage(struct regulator_dev *rdev,
				unsigned int selector)
{
	const struct voltage_map_desc *desc;
	int ldo = ltc3577_get_ldo(rdev);
	int val;

	if (ldo >= ARRAY_SIZE(ltc3577_voltage_map))
		return -EINVAL;

	desc = ltc3577_voltage_map[ldo];
	if (desc == NULL)
		return -EINVAL;

	val = desc->min_mV + desc->step_mV* selector;
	if (val > desc->max_mV)
		return -EINVAL;

	return val * 1000;
}

static int ltc3577_buck_get_voltage(struct regulator_dev *rdev)
{
	int ldo = ltc3577_get_ldo(rdev);
	int regs;
	u8 val = 0;

	switch (ldo) {
	case LTC3577_BUCK3:	/* ARM CORE */
		regs = __raw_readl(S5PV210_GPJ0_BASE);
		regs &= ((0xf << 8) | (0xf << 16) | (0xf << 20));
		if (regs == 0x000100) {			//ARM_VOUT 0_95
			val = 0;
		} else if(regs == 0x100000) {	//ARM_VOUT 1_00
			val = 1;
		} else if(regs == 0x010100) {	//ARM_VOUT 1_05
			val = 2;
		} else if (regs == 0x110000) {	//ARM_VOUT_1_10
			val = 3;
		} else if (regs == 0x100100) {	//ARM_VOUT_1_20
			val = 5;
		} else if (regs == 0x110100) {	//ARM_VOUT_1_25
			val = 6;
		} else {
			WARN(1, "arm core voltage set no correct, regs = 0x%06x\n", regs);
		}
		break;
	case LTC3577_BUCK4:	/* ARM INT */
		regs = __raw_readl(S5PV210_GPJ0_BASE);
		regs &= (0xf << 24);
		if (regs == 0x1000000) {		//INT 1.1V
			val = 1;
		} else if(regs == 0) {
			val = 0;
		} else {
			WARN(1, "arm int voltage set no correct, regs = 0x%06x\n", regs);
		}
		break;
	default:
		val = 0;
		break;
	}

	return ltc3577_list_voltage(rdev, val);
}

static int ltc3577_buck_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	const struct voltage_map_desc *desc;
	int ldo = ltc3577_get_ldo(rdev);
	int reg_con = __raw_readl(S5PV210_GPJ0_BASE);
	int i = 0;

	if (ldo >= ARRAY_SIZE(ltc3577_voltage_map))
		return -EINVAL;

	desc = ltc3577_voltage_map[ldo];
	if (desc == NULL)
		return -EINVAL;

	if (max_vol < desc->min_mV || min_vol > desc->max_mV)
		return -EINVAL;

	while (desc->min_mV+ desc->step_mV*i < min_vol &&
	       desc->min_mV+ desc->step_mV*i < desc->max_mV)
		i++;

	if (desc->min_mV+ desc->step_mV*i > max_vol)
		return -EINVAL;

	switch (ldo) {
	case LTC3577_BUCK3:	/* ARM CORE */
		if (arm_voltage_table[i][0]) {	/*ARM_ON0: S5PV210_GPJ0(5)*/
			reg_con &= ~(0xf<<20);
		} else {
			reg_con |= 0x1<<20;
		}

		if (arm_voltage_table[i][1]) {	/*ARM_ON1: S5PV210_GPJ0(2)*/
			reg_con &= ~(0xf<<8);
		} else {
			reg_con |= 0x1<<8;
		}

		if (arm_voltage_table[i][2]) {	/*ARM_ON2: S5PV210_GPJ0(4)*/
			reg_con &= ~(0xf<<16);
		} else {
			reg_con |= 0x1<<16;
		}
		__raw_writel(reg_con, S5PV210_GPJ0_BASE);
		reg_con = __raw_readl(S5PV210_GPJ0_BASE);
		break;
	case LTC3577_BUCK4:
		if(int_voltage_table[i]) {	/*S5PV210_GPJ0(6)*/
			reg_con &= ~(0xf<<24);
		} else {
			reg_con |= 0x1<<24;
		}
		__raw_writel(reg_con, S5PV210_GPJ0_BASE);
		reg_con = __raw_readl(S5PV210_GPJ0_BASE);
		break;
	}

	return 0;
}

static int ltc3577_buck_set_suspend_voltage(struct regulator_dev *rdev,
				int apply_uV)
{
	int dummy;
	return ltc3577_buck_set_voltage(rdev, apply_uV, apply_uV, &dummy);
}

static int ltc3577_buck_suspend_enable(struct regulator_dev *rdev)
{
	return 0;
}

static int ltc3577_buck_suspend_disable(struct regulator_dev *rdev)
{
	return 0;
}

static int ltc3577_isink_set_current(struct regulator_dev *rdev, int min_uA,
	int max_uA)
{
//	struct ltc3577_data *ltc577 = rdev_get_drvdata(rdev);
	int set_cur = max_uA / 1000;
	int ret = -EINVAL;

	if (set_cur != 0 && set_cur != 100 &&
	    set_cur != 500 && set_cur != 1000)
		return ret;

	ret = gpio_request(LIM0, "ILIM0");
	if (ret)
		return -EINVAL;
	ret = gpio_request(LIM1, "ILIM1");
	if (ret) {
		gpio_free(LIM0);
		return -EINVAL;
	}

	if (set_cur == 1000) {
		gpio_direction_output(LIM0, 0);
		gpio_direction_output(LIM1, 1);
	} else if (set_cur == 500) {
		gpio_direction_output(LIM0, 0);
		gpio_direction_output(LIM1, 0);
	} else if (set_cur == 100) {
		gpio_direction_output(LIM0, 1);
		gpio_direction_output(LIM1, 1);
	} else {
		gpio_direction_output(LIM0, 1);
		gpio_direction_output(LIM1, 0);
	}

	gpio_free(LIM0);
	gpio_free(LIM1);

	return 0;
}

static int ltc3577_isink_get_current(struct regulator_dev *rdev)
{
	//	struct ltc3577_data *ltc577 = rdev_get_drvdata(rdev);
	int ret = -EINVAL;
	int ilim0, ilim1;

	ret = gpio_request(LIM0, "ILIM0");
	if (ret)
		return -EINVAL;
	ret = gpio_request(LIM1, "ILIM1");
	if (ret) {
		gpio_free(LIM0);
		return -EINVAL;
	}

	ilim0 = gpio_get_value(LIM0);
	ilim1 = gpio_get_value(LIM1);

	if (ilim0 == 0 && ilim1 == 1) {
		ret = 1000;
	} else if (ilim0 == 0 && ilim1 == 0) {
		ret = 500;
	} else if (ilim0 == 1 && ilim1 == 1) {
		ret = 100;
	} else {
		ret = 0;
	}

	gpio_free(LIM0);
	gpio_free(LIM1);

	return ret * 1000;
}

static int ltc3577_isink_is_enabled(struct regulator_dev *rdev)
{
	int ret = -EINVAL;
	int ilim0, ilim1;

	ret = gpio_request(LIM0, "ILIM0");
	if (ret)
		return -EINVAL;
	ret = gpio_request(LIM1, "ILIM1");
	if (ret) {
		gpio_free(LIM0);
		return -EINVAL;
	}

	ilim0 = gpio_get_value(LIM0);
	ilim1 = gpio_get_value(LIM1);

	gpio_free(LIM0);
	gpio_free(LIM1);

	if (ilim0 == 1 && ilim1 == 0)
		ret = 0;
	else
		ret = 1;

	return ret;
}

static int ltc3577_isink_suspend_enable(struct regulator_dev *rdev)
{
	return 0;
}

static int ltc3577_isink_suspend_disable(struct regulator_dev *rdev)
{
	return ltc3577_isink_set_current(rdev, 0, 0);
}

static struct regulator_ops ltc3577_buck_ops = {
	.list_voltage		= ltc3577_list_voltage,
	.get_voltage		= ltc3577_buck_get_voltage,
	.set_voltage		= ltc3577_buck_set_voltage,
	.set_suspend_voltage = ltc3577_buck_set_suspend_voltage,
	.set_suspend_enable	= ltc3577_buck_suspend_enable,
	.set_suspend_disable	= ltc3577_buck_suspend_disable,
};

static struct regulator_ops ltc3577_isink_ops = {
	.set_current_limit = ltc3577_isink_set_current,
	.get_current_limit = ltc3577_isink_get_current,
	.is_enabled = ltc3577_isink_is_enabled,
	.set_suspend_enable	= ltc3577_isink_suspend_enable,
	.set_suspend_disable	= ltc3577_isink_suspend_disable,
};

static struct regulator_desc ltc3577_regulators[] = {
	{
		.name		= "ARM CORE",		/* 1.25v ARM CORE */
		.id			= LTC3577_BUCK3,
		.ops			= &ltc3577_buck_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "ARM INT",			/* 1.10v ARM INT */
		.id			= LTC3577_BUCK4,
		.ops			= &ltc3577_buck_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "CHARGER CURRENT",
		.id			= LTC3577_ISINK1,
		.ops			= &ltc3577_isink_ops,
		.type		= REGULATOR_CURRENT,
		.owner		= THIS_MODULE,
	},
};

static __devinit int ltc3577_pmic_probe(struct platform_device *pdev)
{
	struct ltc3577_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct ltc3577_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_dev **rdev;
	struct ltc3577_data *ltc3577;
	int i, ret, size;

	if (!pdata) {
		dev_err(pdev->dev.parent, "No platform init data supplied\n");
		return -ENODEV;
	}

	ltc3577 = kzalloc(sizeof(struct ltc3577_data), GFP_KERNEL);
	if (!ltc3577)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	ltc3577->rdev = kzalloc(size, GFP_KERNEL);
	if (!ltc3577->rdev) {
		kfree(ltc3577);
		return -ENOMEM;
	}

	rdev = ltc3577->rdev;
	ltc3577->dev = &pdev->dev;
	ltc3577->iodev = iodev;
	ltc3577->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, ltc3577);

	for (i = 0; i < pdata->num_regulators; i++) {
		const struct voltage_map_desc *desc;
		int id = pdata->regulators[i].id;
		int index = id - LTC3577_BUCK3;

		desc = ltc3577_voltage_map[id];
		/*enable adjust voltage group*/
		if (desc && ltc3577_regulators[index].ops == &ltc3577_buck_ops) {
			int count;
			if (desc->step_mV)
				count = (desc->max_mV- desc->min_mV) / desc->step_mV+ 1;
			else
				count = 1;
			ltc3577_regulators[index].n_voltages = count;
		}
		rdev[i] = regulator_register(&ltc3577_regulators[index], ltc3577->dev,
				pdata->regulators[i].initdata, ltc3577);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(ltc3577->dev, "regulator init failed\n");
			rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
err:
	for (i = 0; i < ltc3577->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(ltc3577->rdev);
	kfree(ltc3577);

	return ret;
}

static int __devexit ltc3577_pmic_remove(struct platform_device *pdev)
{
	struct ltc3577_data *ltc3577 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = ltc3577->rdev;
	int i;

	for (i = 0; i < ltc3577->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(ltc3577->rdev);
	kfree(ltc3577);

	return 0;
}

static struct platform_driver ltc3577_pmic_driver = {
	.driver = {
		.name = "ltc3577-pmic",
		.owner = THIS_MODULE,
	},
	.probe = ltc3577_pmic_probe,
	.remove = __devexit_p(ltc3577_pmic_remove),
};

static int __init ltc3577_pmic_init(void)
{
	return platform_driver_register(&ltc3577_pmic_driver);
}
subsys_initcall(ltc3577_pmic_init);

static void __exit ltc3577_pmic_cleanup(void)
{
	platform_driver_unregister(&ltc3577_pmic_driver);
}
module_exit(ltc3577_pmic_cleanup);

MODULE_DESCRIPTION("LINEAR LTC3577-3 voltage regulator driver");
MODULE_AUTHOR("Lvcha Qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPL");
