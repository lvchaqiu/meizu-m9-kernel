/*
  *  linux/drivers/video/backlight/m9w_bl.c
  *
  *  PMIC LTC3577-3 for Meizu M9
  *
  *  Based on ltc3714.c
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
  * Inital code : Jan  6 , 2011 : lvcha@meizu.com
  *
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/reboot.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/mfd/ltc3577.h>
#include <linux/mfd/ltc3577-private.h>

//#define USE_SYNC_MODE

struct m9w_bl_msg {
	int brightness;
	int error;
	struct completion *work;
	struct list_head msg_list;
};

struct m9w_bl_data {
	struct device		*dev;
	struct ltc3577_dev	*iodev;
	struct backlight_device *bl_dev;
	struct delayed_work m9w_bl_work;
	int backlight_on;
	int brightness;
	int debug;
	struct mutex m9w_bl_mutex;
	struct list_head m9w_bl_list;
	struct kmem_cache *m9w_bl_slab;
};

static struct m9w_bl_data *m9w_bl_data_local = NULL;

static int m9w_bl_onoff(struct m9w_bl_data *m9w_bl, int onoff);
static int m9w_adjust_bl(struct m9w_bl_data *m9w_bl, int brightness);

static ssize_t bl_brt_show(struct device *dev,
     struct device_attribute *attr, char *buf)
{
	struct m9w_bl_data *m9w_bl = dev_get_drvdata(dev->parent);

	return sprintf(buf, "%d\n",  m9w_bl->brightness);
}

static ssize_t bl_brt_store(struct device *dev,
      struct device_attribute *attr,
      const char *buf, size_t count)
{
	struct m9w_bl_data *m9w_bl = dev_get_drvdata(dev->parent);
	unsigned long level = simple_strtoul(buf, NULL, 0);
	int ret = m9w_adjust_bl(m9w_bl, level);

	return ret ? 0 : count;
}

static ssize_t bl_onoff_show(struct device *dev,
     struct device_attribute *attr, char *buf)
{
	struct m9w_bl_data *m9w_bl = dev_get_drvdata(dev->parent);

	return sprintf(buf, "%d\n",  m9w_bl->backlight_on);
}

static ssize_t bl_onoff_store(struct device *dev,
      struct device_attribute *attr,
      const char *buf, size_t count)
{
	struct m9w_bl_data *m9w_bl = dev_get_drvdata(dev->parent);
	unsigned long onoff = simple_strtoul(buf, NULL, 0);
	int ret = m9w_bl_onoff(m9w_bl, onoff);

	return ret ? 0: count;
}

static ssize_t backlight_debug_show(struct device *dev,
     struct device_attribute *attr, char *buf)
{
	struct m9w_bl_data *m9w_bl = dev_get_drvdata(dev->parent);

	return sprintf(buf, "debug %s\n",  m9w_bl->debug ? "enable" : "disable");
}

static ssize_t backlight_debug_store(struct device *dev,
      struct device_attribute *attr,
      const char *buf, size_t count)
{
	struct m9w_bl_data *m9w_bl = dev_get_drvdata(dev->parent);
	unsigned long debug_enable = simple_strtoul(buf, NULL, 0);

	m9w_bl->debug = debug_enable;

	return count;
}

static DEVICE_ATTR(onoff, S_IRUGO|S_IWUSR|S_IWGRP,
   bl_onoff_show, bl_onoff_store);

static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR|S_IWGRP,
   backlight_debug_show, backlight_debug_store);

static DEVICE_ATTR(level, S_IRUGO|S_IWUSR|S_IWGRP,
		bl_brt_show, bl_brt_store);

static struct attribute * m9w_bl_attributes[] = {
	&dev_attr_onoff.attr,
	&dev_attr_debug.attr,
	&dev_attr_level.attr,
	NULL
};

static struct attribute_group m9w_bl_attribute_group = {
	.attrs = m9w_bl_attributes
};

static int m9w_bl_onoff(struct m9w_bl_data *m9w_bl, int onoff)
{
	int ret;

   	BUG_ON(!m9w_bl);	//dump_stack();

	ret = ltc3577_update_reg(m9w_bl->iodev, LED_CONTROL_REGISTER_ADDR,
			onoff, 0x1);

	if (!ret)
		m9w_bl->backlight_on = onoff;

	return ret;
}

int m9w_bl_on(int onoff)
{
	int ret = 0;

	if (m9w_bl_data_local) {
		ret = m9w_bl_onoff(m9w_bl_data_local, onoff);
		if (ret)
			pr_err("call %s function failed, ret = %d\n", __func__, ret);
	}
	return ret;
}
EXPORT_SYMBOL(m9w_bl_on);

static int m9w_adjust_bl(struct m9w_bl_data *m9w_bl, int brightness)
{
	int ret = -1;
	mutex_lock(&m9w_bl->m9w_bl_mutex);

	if (m9w_bl->brightness != brightness) {
		ret = ltc3577_write_reg(m9w_bl->iodev, LED_DAC_REGISTER_ADDR, (unsigned char)brightness);
		if (!ret)
			m9w_bl->brightness = brightness;
	}

	mutex_unlock(&m9w_bl->m9w_bl_mutex);

	return ret;
}

static void m9w_bl_delay_func(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct m9w_bl_data *m9w_bl = container_of(dw, struct m9w_bl_data, m9w_bl_work);

	while(!list_empty(&m9w_bl->m9w_bl_list)) {
		struct m9w_bl_msg *msg =
			container_of(m9w_bl->m9w_bl_list.next, struct m9w_bl_msg, msg_list);
		list_del_init(&msg->msg_list);
		msg->error = m9w_adjust_bl(m9w_bl, msg->brightness);
#ifdef USE_SYNC_MODE
		if (msg && msg->work) {
			complete(msg->work);
		}
#else
		kmem_cache_free(m9w_bl->m9w_bl_slab, msg);
#endif
	}
}

static int m9w_bl_get_brightness(struct backlight_device* bd)
{
	return bd->props.brightness;
}

static int m9w_bl_update_status(struct backlight_device* bd)
{
	struct m9w_bl_data *m9w_bl = dev_get_drvdata(bd->dev.parent);
	struct m9w_bl_msg *m9w_bl_msg;
	int bl = (bd->props.brightness >> 2) - 2;
	int ret = -1;

	m9w_bl_msg = kmem_cache_zalloc(m9w_bl->m9w_bl_slab, GFP_ATOMIC);
	if (m9w_bl_msg == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	if (m9w_bl->debug) {
		dev_info(&bd->dev, "%s: fb_blank = 0x%x, power = 0x%x, state = 0x%x\n",
			__func__, bd->props.fb_blank, bd->props.power, bd->props.state);
		dev_info(&bd->dev, "Update brightness=%d, level = %d\n",bd->props.brightness, bl);
	}

	if (bd->props.power != FB_BLANK_UNBLANK ||
	    bd->props.fb_blank != FB_BLANK_UNBLANK || bl < 0)
		bl = 0;

	m9w_bl_msg->brightness = bl;

	INIT_LIST_HEAD(&m9w_bl_msg->msg_list);
	list_add_tail(&m9w_bl_msg->msg_list, &m9w_bl->m9w_bl_list);
	schedule_delayed_work(&m9w_bl->m9w_bl_work, 1);

#ifdef USE_SYNC_MODE
	do {
		#define WAITTING_TIME 100	/*100mS*/
		DECLARE_COMPLETION_ONSTACK(m9w_bl_done);
		ktime_t delta, rettime;
		long long delta_time;

		rettime = ktime_get();
		m9w_bl_msg->work = &m9w_bl_done;
		ret = wait_for_completion_timeout(&m9w_bl_done, msecs_to_jiffies(WAITTING_TIME));
		if (ret) {
			ret = m9w_bl_msg->error;
		} else {
			printk(KERN_ERR "m9_bl_update_status timeout\n");
			ret = EIO;
		}
		m9w_bl_msg->work = NULL;
		kmem_cache_free(m9w_bl->m9w_bl_slab, m9w_bl_msg);
		m9w_bl_msg = NULL;

		delta = ktime_sub(ktime_get(), rettime);
		delta_time = ktime_to_ms(delta);
		if (delta_time > WAITTING_TIME/2)
			printk(KERN_INFO "%s: after : %Ld mS\n", __func__, ktime_to_ms(delta));

	} while(0);
#endif

	return ret;
}

static int m9w_bl_check_fb(struct backlight_device *dev, struct fb_info *fbinfo)
{
	/*only available on framebuffer window 0*/
	if(fbinfo->node == 0)
		return 1;
	return 0;
}

static struct backlight_ops m9w_bl_ops = {
	.update_status = m9w_bl_update_status,
	.get_brightness = m9w_bl_get_brightness,
	.check_fb = m9w_bl_check_fb,
};

static void  m9w_bl_boot(struct work_struct *work)
{
	struct m9w_bl_data *m9w_bl =
		container_of(work, struct m9w_bl_data, m9w_bl_work.work);

	m9w_adjust_bl(m9w_bl, 50);
	m9w_bl_onoff(m9w_bl, 1);

	INIT_WORK(&m9w_bl->m9w_bl_work.work, m9w_bl_delay_func);
}

static int m9w_bl_reboot_notifier_event(struct notifier_block *this,
						 unsigned long event, void *ptr)
{
	int ret;

	switch (event) {
	case SYS_POWER_OFF:
	case SYS_RESTART:
		ret = m9w_bl_on(0);	//turn off backlight before reset.
		break;
	default:
		ret = -1;
		break;
	}

	return ret ? NOTIFY_BAD : NOTIFY_DONE;
}

static struct notifier_block m9w_bl_reboot_notifier = {
	.notifier_call = m9w_bl_reboot_notifier_event,
};

static int __devinit m9w_bl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct ltc3577_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct m9w_bl_data *m9w_bl;

	dev_dbg(&pdev->dev,  "m9w_bl_probe\n");

	m9w_bl = kzalloc(sizeof(*m9w_bl), GFP_KERNEL);
	if (m9w_bl == NULL) {
		ret = -ENOMEM;
		goto error0;
	}

	m9w_bl->m9w_bl_slab = kmem_cache_create("m9w_bl",
					     sizeof(struct m9w_bl_msg), 0,
					     SLAB_HWCACHE_ALIGN, NULL);
	if (!m9w_bl->m9w_bl_slab) {
		printk(KERN_ERR "m9w_bl->m9w_bl_slab failed\n");
		ret = -ENOMEM;
		goto error1;
	}

	m9w_bl->dev = &pdev->dev;
	m9w_bl->iodev = iodev;

	mutex_init(&m9w_bl->m9w_bl_mutex);

	platform_set_drvdata(pdev, m9w_bl);

	ret = ltc3577_write_reg(iodev, LED_CONTROL_REGISTER_ADDR,
			LED_CONTROL_REGISTER_VAL_SLOW);
	if (ret) {
		ret = EIO;
		goto error2;
	}

	ret = m9w_bl_onoff(m9w_bl, 0);	//turn off backlight in first.
	if (ret) {
		ret = EIO;
		goto error2;
	}

	INIT_LIST_HEAD(&m9w_bl->m9w_bl_list);

	m9w_bl->bl_dev = backlight_device_register("pwm-backlight",
		&pdev->dev, NULL, &m9w_bl_ops, NULL);
	m9w_bl->bl_dev->props.max_brightness = 255;

	INIT_DELAYED_WORK(&m9w_bl->m9w_bl_work, m9w_bl_boot);

	/*turn off backlight if enter enginer test mode?*/
	if (!gpio_get_value(X_POWER_GPIO)) {
		m9w_bl_onoff(m9w_bl, 0);
	} else {
		schedule_delayed_work(&m9w_bl->m9w_bl_work, msecs_to_jiffies(100));
	}

	register_reboot_notifier(&m9w_bl_reboot_notifier);
	m9w_bl_data_local = m9w_bl;
	return sysfs_create_group(&m9w_bl->bl_dev->dev.kobj, &m9w_bl_attribute_group);

error2:
	kmem_cache_destroy(m9w_bl->m9w_bl_slab);
error1:
	kfree(m9w_bl);
error0:
	return ret;
}

static int __devexit m9w_bl_remove(struct platform_device *pdev)
{
	struct m9w_bl_data *m9w_bl = platform_get_drvdata(pdev);

	m9w_adjust_bl(m9w_bl, 0);
	m9w_bl_onoff(m9w_bl, 0);	//turn off backlight

	mutex_destroy(&m9w_bl->m9w_bl_mutex);

	backlight_device_unregister(m9w_bl->bl_dev );

	sysfs_remove_group(&m9w_bl->bl_dev->dev.kobj, &m9w_bl_attribute_group);

	unregister_reboot_notifier(&m9w_bl_reboot_notifier);

	kmem_cache_destroy(m9w_bl->m9w_bl_slab);
	kfree(m9w_bl);

	return 0;
}

#if defined(CONFIG_PM)
static int m9w_bl_suspend(struct platform_device *pdev, pm_message_t st)
{
	struct m9w_bl_data *m9w_bl = platform_get_drvdata(pdev);
	int ret;
	ret = cancel_delayed_work_sync(&m9w_bl->m9w_bl_work);
	ret = ltc3577_write_reg(m9w_bl->iodev,
			BUCK_CONTROL_REGISTER_ADDR, BUCK_CONTROL_ENABLE_VAL);
	return ret;
}
static int m9w_bl_resume(struct platform_device *pdev)
{
	struct m9w_bl_data *m9w_bl = platform_get_drvdata(pdev);
	return ltc3577_write_reg(m9w_bl->iodev,
			BUCK_CONTROL_REGISTER_ADDR, BUCK_CONTROL_DISABLE_VAL);
}
#else
#define m9w_bl_suspend NULL
#define m9w_bl_resume NULL
#endif

static struct platform_driver m9w_bl_driver = {
	.driver		= {
		.name	= "ltc3577-backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= m9w_bl_probe,
	.remove		= __devexit_p(m9w_bl_remove),
	.suspend		= m9w_bl_suspend,
	.resume		= m9w_bl_resume,
};

static int __init m9w_bl_init(void)
{
	return platform_driver_register(&m9w_bl_driver);
}

static void __exit m9w_bl_cleanup(void)
{
	platform_driver_unregister(&m9w_bl_driver);
}

module_init(m9w_bl_init);
module_exit(m9w_bl_cleanup);

/* Module information */
MODULE_DESCRIPTION("LTC3577-3 Backlight driver");
MODULE_AUTHOR("Lvcha Qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPL");
