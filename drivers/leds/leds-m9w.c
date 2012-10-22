/*
 * drivers/leds/leds-m9w.c
 * 
 * Copyright (C) 2011 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: 	lvcha qiu   <lvcha@meizu.com>
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
 * Inital code : May 26 , 2010 : lvcha@meizu.com
 *
 */
 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif
#include <linux/m9w_bat.h>

#include <mach/hardware.h>
#include <mach/leds-gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/pm.h>

#if 0//defined(CONFIG_MZ_ENG)
#define CONFIG_TOUCH_NOTIFIED
#define TURNOFF_TIMEOUT 2*HZ
#endif

/* our context */
struct m9w_gpio_led {
	struct led_classdev cdev;
	struct m9w_led_platdata *pdata;
	struct mutex led_mutex;
#ifdef CONFIG_HAVE_PWM	
	struct pwm_device	*pwm;
	unsigned int		period;
	enum led_brightness brightness;
	struct delayed_work query_dwork;
	unsigned long last_time;
	unsigned int brightness_max;
	bool auto_turnoff;
#endif
	/*pm*/
#ifdef CONFIG_HAS_WAKELOCK
        struct early_suspend    early_suspend;
#endif
	struct notifier_block leds_m9w_notifier;
};

extern int register_m9w_rmi_notifier(struct notifier_block *nb);
extern int unregister_m9w_rmi_notifier(struct notifier_block *nb);

static void leds_early_suspend(struct early_suspend *h);
static void leds_early_resume(struct early_suspend *h);

enum led_brightness brightness_prev;

static inline struct m9w_gpio_led *pdev_to_gpio(struct platform_device *dev)
{
	return platform_get_drvdata(dev);
}

static inline struct m9w_gpio_led *to_gpio(struct led_classdev *led_cdev)
{
	return container_of(led_cdev, struct m9w_gpio_led, cdev);
}

static enum led_brightness m9w_led_get(struct led_classdev *led_cdev)
{
	struct m9w_gpio_led *led = to_gpio(led_cdev);
	return led->brightness;
}

static void m9w_led_set(struct led_classdev *led_cdev,
			    enum led_brightness brightness)
{
	struct m9w_gpio_led *led = to_gpio(led_cdev);
	int max = led->brightness_max;
#ifdef CONFIG_TOUCH_NOTIFIED
	static bool first_time = true;
#endif

	mutex_lock(&led->led_mutex);

	dev_dbg(led_cdev->dev, "m9w_led_set, value = %d\n", brightness);

	if (led->brightness !=  brightness) {
		if (brightness == 0) {
			pwm_disable(led->pwm);
			pwm_config(led->pwm, 0, led->period);
			led->brightness = 0;
			led->auto_turnoff = false;
		} else {
			int bright_tmp = (brightness <= max) ? brightness : max;
			pwm_config(led->pwm, (led->period * bright_tmp) / max, led->period);
			pwm_enable(led->pwm);
			led->brightness = bright_tmp;
			led->auto_turnoff = true;
#ifdef CONFIG_TOUCH_NOTIFIED
			if (first_time) {
				schedule_delayed_work(&led->query_dwork, TURNOFF_TIMEOUT);
				first_time = false;
			}
#endif
		}
		led->last_time = jiffies;
	}

	mutex_unlock(&led->led_mutex);
}

#ifdef CONFIG_TOUCH_NOTIFIED
static void led_query_dwork(struct work_struct *work)
{
	struct m9w_gpio_led *led  =
		container_of(work, struct m9w_gpio_led, query_dwork.work);
	static enum led_brightness brightness;

	if (led->auto_turnoff != true) {
		WARN(1, "led->brightness = %d, org_brightness = %d\n\n", led->brightness, brightness);
		return;
	}

	mutex_lock(&led->led_mutex);

	if (led->brightness) {
		brightness = led->brightness;
		pwm_disable(led->pwm);
		pwm_config(led->pwm, 0, led->period);
		led->brightness = 0;
	} else {
		int max = led->brightness_max;
		pwm_config(led->pwm, (led->period * brightness) / max, led->period);
		pwm_enable(led->pwm);
		led->brightness = brightness;
		brightness = 0;
	}

	mutex_unlock(&led->led_mutex);
}

static int leds_m9w_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct m9w_gpio_led *led =
		container_of(this, struct m9w_gpio_led, leds_m9w_notifier);

	pr_debug("led = 0x%p, event = %ld, ptr = 0x%p\n", led, event, ptr);

	switch (event) {
	case POWER_STATE_CHANGE:
		break;
	case POWER_LEVEL_CHANGE:
		break;
	case POWER_STATE_SUSPEND:
		break;
	case POWER_STATE_RESUME:
		break;
	case EV_LED:
		cancel_delayed_work(&led->query_dwork);
		if (get_suspend_state() == PM_SUSPEND_MEM)
			return 0;
		if ((led->brightness != led->brightness_max) &&
		     (led->auto_turnoff == true)) {			
			if (led->brightness) {
				schedule_delayed_work(&led->query_dwork, TURNOFF_TIMEOUT);
			} else {
				unsigned long time =  led->last_time+TURNOFF_TIMEOUT;
				if (time_after(jiffies, time)) {
					schedule_delayed_work(&led->query_dwork, 0);
				}
			}
		} else {
			if (led->brightness == led->brightness_max) {
				printk(KERN_DEBUG "led->brightness == max\n");
			}

			if (led->auto_turnoff == false) {
				printk(KERN_DEBUG "led->auto_turnoff == false\n");
			}
		}

		break;
	}
	return 0;
}
#endif

static int m9w_led_probe(struct platform_device *pdev)
{
	struct m9w_led_platdata *pdata = pdev->dev.platform_data;
	struct m9w_gpio_led *led;
	int ret;

	led = kzalloc(sizeof(struct m9w_gpio_led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&pdev->dev, "No memory for device\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, led);

	led->cdev.brightness_set = m9w_led_set;
	led->cdev.brightness_get = m9w_led_get;
	led->cdev.default_trigger = pdata->def_trigger;
	led->cdev.name = pdata->name;
	led->brightness = 255;
//	led->cdev.flags |= LED_CORE_SUSPENDRESUME;

	led->pdata = pdata;

	mutex_init(&led->led_mutex);

	led->brightness_max = pdata->max_brightness;
	led->period = pdata->pwm_period_ns;
	led->pwm = pwm_request(pdata->pwm_id, "keypad-light");
	if (IS_ERR(led->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM for keypad-light\n");
		ret = PTR_ERR(led->pwm);
		goto err_pwm;
	} else {
		pwm_config(led->pwm, 0, led->period);
		pwm_disable(led->pwm);
		dev_dbg(&pdev->dev, "got pwm for keypad-light\n");
	}

	/* register our new led device */
	ret = led_classdev_register(&pdev->dev, &led->cdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "led_classdev_register failed\n");
		goto exit_err1;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	led->early_suspend.suspend = leds_early_suspend;
	led->early_suspend.resume = leds_early_resume;
	led->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&led->early_suspend);
#endif

#ifdef CONFIG_TOUCH_NOTIFIED
	INIT_DELAYED_WORK(&led->query_dwork, led_query_dwork);
	led->leds_m9w_notifier.notifier_call = leds_m9w_notifier_event;
	register_m9w_rmi_notifier(&led->leds_m9w_notifier);
#endif

	pr_info("m9w_led_probe success\n");

	return 0;

 exit_err1:
 	pwm_free(led->pwm);
 err_pwm:
	kfree(led);
	return ret;
}

static int m9w_led_remove(struct platform_device *dev)
{
	struct m9w_gpio_led *led = pdev_to_gpio(dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&led->early_suspend);
#endif

#ifdef CONFIG_TOUCH_NOTIFIED
	unregister_m9w_rmi_notifier(&led->leds_m9w_notifier);
#endif
	
	led_classdev_unregister(&led->cdev);

	pwm_config(led->pwm, 0, led->period);
	pwm_disable(led->pwm);
	pwm_free(led->pwm);

	kfree(led);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void leds_early_suspend(struct early_suspend *h)
{
	struct m9w_gpio_led *led =
		container_of(h, struct m9w_gpio_led, early_suspend);

#ifdef CONFIG_TOUCH_NOTIFIED
	flush_delayed_work(&led->query_dwork);
	pwm_disable(led->pwm);
	pwm_config(led->pwm, 0, led->period);
#else
	if (led->brightness) {
		pwm_disable(led->pwm);
		pwm_config(led->pwm, 0, led->period);
	} 
#endif
}

static void leds_early_resume(struct early_suspend *h)
{
	struct m9w_gpio_led *led =
		container_of(h, struct m9w_gpio_led, early_suspend);
	int max = led->pdata->max_brightness;

#ifdef CONFIG_TOUCH_NOTIFIED
	if (led->brightness == led->brightness_max) {
		pwm_config(led->pwm, (led->period * led->brightness) / max, led->period);
		pwm_enable(led->pwm);
	}
#else
	if (led->brightness) {
		pwm_config(led->pwm, (led->period * led->brightness) / max, led->period);
		pwm_enable(led->pwm);
	}
#endif
}
#endif

static struct platform_driver m9w_led_driver = {
	.probe		= m9w_led_probe,
	.remove		= m9w_led_remove,
	.driver		= {
		.name		= "m9w_led",
		.owner		= THIS_MODULE,
	},
};

static int __init m9w_led_init(void)
{
	return platform_driver_register(&m9w_led_driver);
}

static void __exit m9w_led_exit(void)
{
	platform_driver_unregister(&m9w_led_driver);
}

module_init(m9w_led_init);
module_exit(m9w_led_exit);

MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_DESCRIPTION("Meizu M9W LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:m9w_led");
