/*
 * gps driver
 *
 * Copyright (c) 2010 meizu Corporation
 * 
 */
 
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/wakelock.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/gpio.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

#include <mach/hardware.h>
//#include <plat/gpio-cfg.h>
#include <plat/irqs.h>
//#include <plat/regs-gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>

struct ublox_gps_info {
	struct delayed_work gps_delay_wq;
	int gps_enable;
	struct mutex ublox_gps_lock;
	struct device *dev;
#ifdef CONFIG_HAS_WAKELOCK
	struct early_suspend	early_suspend;
	struct early_suspend	earler_suspend;
#endif
};

static ssize_t gps_enable_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ublox_gps_info *gps_info = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",  gps_info->gps_enable);
}

static ssize_t gps_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long enable = simple_strtoul(buf, NULL, 10);
	struct ublox_gps_info *gps_info = dev_get_drvdata(dev);

	mutex_lock(&gps_info->ublox_gps_lock);

	if (enable)
		gps_info->gps_enable = 1;
	else
		gps_info->gps_enable = 0;

	s3c_gpio_setpin(GPS_POWER, gps_info->gps_enable);

	mutex_unlock(&gps_info->ublox_gps_lock);

	dev_dbg(dev, "[GPS] set power: %d\n", gps_info->gps_enable);
	return count;
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
		   gps_enable_show, gps_enable_store);

static struct attribute *gps_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group gps_attribute_group = {
	.attrs = gps_attributes
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ublox_early_suspend(struct early_suspend *h)
{
	struct ublox_gps_info *gps_info = container_of(h, struct ublox_gps_info, early_suspend);
	struct platform_device *pdev = container_of(gps_info->dev, struct platform_device, dev);

	s3c_gpio_setpin(GPS_POWER, 0);

	dev_dbg(&pdev->dev, "[GPS] ublox_gps_suspend\n");
}

static void ublox_late_resume(struct early_suspend *h)
{
	struct ublox_gps_info *gps_info = container_of(h, struct ublox_gps_info, early_suspend);
	struct platform_device *pdev = container_of(gps_info->dev, struct platform_device, dev);

	s3c_gpio_setpin(GPS_POWER, gps_info->gps_enable);

	dev_dbg(&pdev->dev, "[GPS] ublox_gps_resume :%d\n", gps_info->gps_enable);
}
#endif

static int ublox_gps_probe(struct platform_device *pdev)
{
	int err=0;
	struct ublox_gps_info *gps_info;

	gps_info = kzalloc(sizeof(struct ublox_gps_info), GFP_KERNEL);
	if(!gps_info) {
		dev_err(&pdev->dev, "GPS: kzalloc failed!\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, gps_info);
	gps_info->dev = &pdev->dev;

	mutex_init(&gps_info->ublox_gps_lock);

	err = sysfs_create_group(&pdev->dev.kobj, &gps_attribute_group);
	if (err < 0) {
		dev_err(&pdev->dev, "GPS: ublox_gps_probe failed\n");
		kfree(gps_info);
		return -1;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	gps_info->early_suspend.suspend = ublox_early_suspend;
	gps_info->early_suspend.resume = ublox_late_resume;
	gps_info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	/*register_early_suspend(&gps_info->early_suspend);*/
#endif

            printk("GPS: probe succeeded!\n");

	return 0;
}

static int ublox_gps_remove(struct platform_device *pdev)
{
	struct ublox_gps_info *gps_info = platform_get_drvdata(pdev);

	gps_info->gps_enable = 0;
	s3c_gpio_setpin(GPS_POWER, gps_info->gps_enable);

	sysfs_remove_group(&pdev->dev.kobj, &gps_attribute_group);
	kfree(gps_info);

	return 0;
}

static struct platform_driver m9_ublox_gps = {
	.driver = {
		.name = "m9-ublox-gps",
		.owner = THIS_MODULE,
	},
	.probe =   ublox_gps_probe,
	.remove = __devexit_p(ublox_gps_remove),
};

static int __init ublox_gps_init(void)
{
	int rc = 0;
       
	rc = platform_driver_register(&m9_ublox_gps);
	return rc;
}


static void __exit ublox_gps_exit(void)
{
	platform_driver_unregister(&m9_ublox_gps);
	return;
}

module_init(ublox_gps_init);
module_exit(ublox_gps_exit);

MODULE_AUTHOR("meizu");
MODULE_DESCRIPTION("ublox gps driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
