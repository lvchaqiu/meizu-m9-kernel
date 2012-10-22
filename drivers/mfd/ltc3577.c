/*
 * ltc3577.c - mfd core driver for the ltc 3577-3
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/ltc3577.h>
#include <linux/mfd/ltc3577-private.h>

static struct mfd_cell ltc3577_devs[] = {
	[0] = {.name = "ltc3577-pmic",},
	[1] = {.name = "ltc3577-backlight",},
	[2] = {.name = "ltc3577-charger",},
};

static int ltc3577_i2c_device_read(struct ltc3577_dev *ltc3577, u8 reg)
{
	struct i2c_client *client = ltc3577->i2c_client;
	int ret = 0;

	mutex_lock(&ltc3577->iolock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&ltc3577->iolock);
	return ret;
}

static int ltc3577_i2c_device_write(struct ltc3577_dev *ltc3577, u8 reg, u8 value)
{
	struct i2c_client *client = ltc3577->i2c_client;
	int ret;

	mutex_lock(&ltc3577->iolock);
	ret = i2c_smbus_write_byte_data(client, reg, value);
	mutex_unlock(&ltc3577->iolock);

	if (ret<0)
		dev_err(&client->dev, "failed to transmit instructions to ltc3577.ret = %d\n", ret);
	else
		ltc3577->data[reg] = value;

	return ret;
}

static int ltc3577_i2c_device_update(struct ltc3577_dev *ltc3577, u8 reg,
				     u8 val, u8 mask)
{
	struct i2c_client *client = ltc3577->i2c_client;
	int ret;

	mutex_lock(&ltc3577->iolock);
	ret = ltc3577->data[reg];
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(client, reg, new_val);
		if (ret >= 0)
			ret = 0;
	}
	mutex_unlock(&ltc3577->iolock);
	return ret;
}

static int ltc3577_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct ltc3577_dev *ltc3577;
	int ret = 0;

	if (!i2c_check_functionality(i2c->adapter,
		I2C_FUNC_SMBUS_READ_BYTE |
		I2C_FUNC_SMBUS_WRITE_BYTE)) {
		dev_err(&i2c->dev, "i2c bus does not support the ltc3577\n");
		ret = -ENODEV;
		return ret;
	}

	ltc3577 = kzalloc(sizeof(struct ltc3577_dev), GFP_KERNEL);
	if (ltc3577 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, ltc3577);
	ltc3577->dev = &i2c->dev;
	ltc3577->i2c_client = i2c;
	ltc3577->dev_read = ltc3577_i2c_device_read;
	ltc3577->dev_write = ltc3577_i2c_device_write;
	ltc3577->dev_update = ltc3577_i2c_device_update;
	mutex_init(&ltc3577->iolock);

	ret = ltc3577->dev_read(ltc3577, 0);
	if (ret < 0) {
		printk(KERN_ERR "%s: read ltc3577 regs failed\n", __func__);
		goto free;
	} else
		ltc3577->status = ret;

	if (likely(ltc3577->status & LTC3577_POWER_GOOD)) {
		printk(KERN_INFO "Power supply is all good\n");
	} else {
		printk(KERN_ERR "Power supply no good\n");
		if (pm_power_off)
			pm_power_off();
		while(1);
	}

	ret = mfd_add_devices(ltc3577->dev, -1,
			      ltc3577_devs, ARRAY_SIZE(ltc3577_devs),
			      NULL, 0);
	if (ret < 0)
		goto err;

	return ret;

err:
	mfd_remove_devices(ltc3577->dev);
free:
	kfree(ltc3577);
	return ret;
}

static int ltc3577_i2c_remove(struct i2c_client *i2c)
{
	struct ltc3577_dev *ltc3577 = i2c_get_clientdata(i2c);

	mfd_remove_devices(ltc3577->dev);
	kfree(ltc3577);

	return 0;
}

static const struct i2c_device_id ltc3577_i2c_id[] = {
       { "ltc3577-3", 0 },
       { }
};
MODULE_DEVICE_TABLE(i2c, ltc3577_i2c_id);

static struct i2c_driver ltc3577_i2c_driver = {
	.driver = {
		   .name = "ltc3577-3",
		   .owner = THIS_MODULE,
	},
	.probe = ltc3577_i2c_probe,
	.remove = ltc3577_i2c_remove,
	.id_table = ltc3577_i2c_id,
};

static int __init ltc3577_i2c_init(void)
{
	return i2c_add_driver(&ltc3577_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(ltc3577_i2c_init);

static void __exit ltc3577_i2c_exit(void)
{
	i2c_del_driver(&ltc3577_i2c_driver);
}
module_exit(ltc3577_i2c_exit);

MODULE_DESCRIPTION("LTC 3577-3 multi-function core driver");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPL");
