/*
 * Copyright (C) 2010 Samsung Electronics Co., Ltd.
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Modified for Crespo on August, 2010 By Samsung Electronics Co.
 * This is modified operate according to each status.
 *
 */

/* Control bluetooth power for Crespo platform */

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
#include <linux/serial_core.h>

#include <mach/gpio.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <plat/irqs.h>
#include <linux/slab.h>

#ifndef	GPIO_LEVEL_LOW
#define GPIO_LEVEL_LOW			0
#define GPIO_LEVEL_HIGH		1
#endif
extern void s3c_setup_uart_cfg_gpio(unsigned char port);


static const char bt_name[] = "bcm4329_bt";
static struct rfkill *bt_rfk;
static struct wake_lock rfkill_wake_lock;

struct bt_gpio_info {
	int bt_enable;
	int bt_wake;
	int bt_test_mode;
	struct mutex bt_lock;
	struct device *dev;
};

static struct m9_bt_lpm {
	struct hrtimer bt_lpm_timer;
	ktime_t bt_lpm_delay;
} bt_lpm;


void bt_uart_rts_ctrl(int flag)
{
	if(!gpio_get_value(BT_RESET))
		return;

	if(flag) {
		// BT RTS Set to HIGH
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S5PV210_GPA0(3), S3C_GPIO_PULL_NONE);
		gpio_set_value(S5PV210_GPA0(3), 1);

            	s3c_gpio_slp_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SLP_OUT0);
		s3c_gpio_slp_setpull_updown(S5PV210_GPA0(3), S3C_GPIO_PULL_NONE);
	}
	else {
		// BT RTS Set to LOW
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPA0(3), 0);

		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(3), S3C_GPIO_PULL_NONE);
	}
}
EXPORT_SYMBOL(bt_uart_rts_ctrl);

//////////////////low power control//////////////////////////////

static int g_bt_is_running = 0;

int check_bt_state(void)
{
	return  g_bt_is_running;
}
EXPORT_SYMBOL_GPL(check_bt_state);
static enum hrtimer_restart bt_enter_lpm(struct hrtimer *timer)
{
	g_bt_is_running = 0;
	gpio_set_value(BT_WAKE, GPIO_LEVEL_LOW);	
	return HRTIMER_NORESTART;
}

void bt_uart_wake_peer(struct uart_port *port)
{
	if (!bt_lpm.bt_lpm_timer.function)
		return;

	g_bt_is_running = 1;
	hrtimer_try_to_cancel(&bt_lpm.bt_lpm_timer);
	gpio_set_value(BT_WAKE, GPIO_LEVEL_HIGH);
	hrtimer_start(&bt_lpm.bt_lpm_timer, bt_lpm.bt_lpm_delay, HRTIMER_MODE_REL);
}

static int bt_lpm_init(void)
{
	int ret;
	
	/*Set init wake state low*/
	ret = gpio_request(BT_WAKE, "gpio_bt_wake");
	if (ret) {
		printk(KERN_ERR "Failed to request gpio_bt_wake control\n");
		return -1;
	}
	
	gpio_direction_output(BT_WAKE, GPIO_LEVEL_LOW);

	/*init hr timer*/
	g_bt_is_running = 0;
	hrtimer_init(&bt_lpm.bt_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bt_lpm.bt_lpm_delay = ktime_set(1, 0);	/* 1 sec */
	bt_lpm.bt_lpm_timer.function = bt_enter_lpm;
	
	return 0;
}

/////////////////rfkill interface/////////////////////////////////

static int bt_set_power(void *data, enum rfkill_user_states state)
{
	int ret = 0;
	int wl_ret =0;
	int irq;

	/* BT Host Wake IRQ */
	irq = gpio_to_irq(BT_HOST_WAKE);

	switch (state) {
	case RFKILL_USER_STATE_UNBLOCKED:
		
		pr_info("[BT] Device Powering ON\n");
		
		/* config uart0 */ 
		s3c_setup_uart_cfg_gpio(0);
		
		/* set gpio direction */
		if (gpio_is_valid(BT_POWER))
			gpio_direction_output(BT_POWER, GPIO_LEVEL_HIGH);
		
		//request gpio wl_power when use it
		wl_ret = gpio_request(WL_POWER, "gpio_wl_power"); 
		if (gpio_is_valid(WL_POWER))
			gpio_direction_output(WL_POWER, GPIO_LEVEL_HIGH);		
		if (gpio_is_valid(BT_RESET))
			gpio_direction_output(BT_RESET, GPIO_LEVEL_LOW);

		
		/* Set BT_POWER high */
		s3c_gpio_setpull(BT_POWER, S3C_GPIO_PULL_NONE);
		gpio_set_value(BT_POWER, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(BT_POWER, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(BT_POWER, S3C_GPIO_PULL_NONE);
		
		/* Set WL_POWER high */
		s3c_gpio_setpull(WL_POWER, S3C_GPIO_PULL_NONE);
		gpio_set_value(WL_POWER, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(WL_POWER, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(WL_POWER,	S3C_GPIO_PULL_NONE);		
		if (wl_ret == 0) {						
			gpio_free(WL_POWER);		//free wl_power 
		}
		
		/*
		 * FIXME sleep should be enabled disabled since the device is
		 * not booting if its enabled ?
		 */
		 
		/*
		 *  at least 100 msec delay,  between reg_on & rst.
		 * (bcm4329 powerup sequence)
		 */
		msleep(100);	//default 50; lvcha

		/* Set BT_RESET high */
		s3c_gpio_setpull(BT_RESET, S3C_GPIO_PULL_NONE);
		gpio_set_value(BT_RESET, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(BT_RESET, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(BT_RESET, S3C_GPIO_PULL_NONE);
	
		/*
		 * at least 50 msec  delay,  after bt rst
		 * (bcm4329 powerup sequence)
		 */
		msleep(50);

		gpio_set_value(BT_WAKE, GPIO_LEVEL_HIGH);

		ret = enable_irq_wake(irq);
		if (ret < 0)
			pr_err("[BT] set wakeup src failed\n");

		enable_irq(irq);
		break;

	case RFKILL_USER_STATE_SOFT_BLOCKED:
		
		pr_info("[BT] Device Powering OFF\n");
		
		/* Set irq */
		ret = disable_irq_wake(irq);
		if (ret < 0)
			pr_err("[BT] unset wakeup src failed\n");

		disable_irq(irq);

		/* Unlock wake lock */
		wake_unlock(&rfkill_wake_lock);

		gpio_set_value(BT_WAKE, GPIO_LEVEL_LOW);

		/* Set BT_RESET low */
		s3c_gpio_setpull(BT_RESET, S3C_GPIO_PULL_NONE);
		gpio_set_value(BT_RESET, GPIO_LEVEL_LOW);
		s3c_gpio_slp_cfgpin(BT_RESET, S3C_GPIO_SLP_OUT0);
		s3c_gpio_slp_setpull_updown(BT_RESET, S3C_GPIO_PULL_NONE);
		
		/* Check WL_RESET */
		if (gpio_get_value(WL_RESET) == 0) {
			/* Set WL_POWER low */
			s3c_gpio_setpull(WL_POWER, S3C_GPIO_PULL_NONE);
			gpio_set_value(WL_POWER, GPIO_LEVEL_LOW);
			s3c_gpio_slp_cfgpin(WL_POWER, S3C_GPIO_SLP_OUT0);
			s3c_gpio_slp_setpull_updown(WL_POWER,	S3C_GPIO_PULL_NONE);

			/* Set BT_POWER low */
			s3c_gpio_setpull(BT_POWER, S3C_GPIO_PULL_NONE);
			gpio_set_value(BT_POWER, GPIO_LEVEL_LOW);
			s3c_gpio_slp_cfgpin(BT_POWER, S3C_GPIO_SLP_OUT0);
			s3c_gpio_slp_setpull_updown(BT_POWER,	S3C_GPIO_PULL_NONE);	
		}
		break;

	default:
		pr_err("[BT] Bad bluetooth rfkill state %d\n", state);
	}

	return 0;
}

irqreturn_t bt_host_wake_irq_handler(int irq, void *dev_id)
{
	if (gpio_get_value(BT_HOST_WAKE)) {
		g_bt_is_running = 1;
		wake_lock(&rfkill_wake_lock);
	} else
		wake_lock_timeout(&rfkill_wake_lock, 2*HZ);

	return IRQ_HANDLED;
}

static int bt_rfkill_set_block(void *data, bool blocked)
{
	unsigned int ret = 0;

	ret = bt_set_power(data, blocked ?
			RFKILL_USER_STATE_SOFT_BLOCKED :
			RFKILL_USER_STATE_UNBLOCKED);

	return ret;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_rfkill_set_block,
};

/////////////////sysfs interface/////////////////////////////////
static ssize_t bt_name_show(struct device *dev,
    					struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",  bt_name);
}

static ssize_t bt_enable_show(struct device *dev,
     					struct device_attribute *attr, char *buf)
{
	struct bt_gpio_info *bt_info = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",  bt_info->bt_enable);
}

static ssize_t bt_enable_store(struct device *dev,
      					struct device_attribute *attr,
      const char *buf, size_t count)
{
	unsigned long enable = simple_strtoul(buf, NULL, 10);
	struct bt_gpio_info *bt_info = dev_get_drvdata(dev);
	if (enable == 1 || enable == 0){
		mutex_lock(&bt_info->bt_lock);
		bt_info->bt_enable = enable;
		mutex_unlock(&bt_info->bt_lock);		
		bt_set_power(NULL, enable ? RFKILL_USER_STATE_UNBLOCKED : RFKILL_USER_STATE_SOFT_BLOCKED );
	} else
		dev_warn(dev, "[BT] input error\n");
	return count;
}

static ssize_t bt_wake_show(struct device *dev,
     						struct device_attribute *attr, char *buf)
{
	struct bt_gpio_info *bt_info = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",  bt_info->bt_wake);
}

static ssize_t bt_wake_store(struct device *dev,
      						struct device_attribute *attr,      const char *buf, size_t count)
{
	unsigned long wake = simple_strtoul(buf, NULL, 10);
	struct bt_gpio_info *bt_info = dev_get_drvdata(dev);

	if (wake == 1 || wake == 0) {
		mutex_lock(&bt_info->bt_lock);
		bt_info->bt_wake = wake;
		mutex_unlock(&bt_info->bt_lock);
		s3c_gpio_setpin(BT_WAKE, wake ? 1:0 );  
	} else
		dev_warn(dev, "[BT] input error\n");

	return count;
}

static ssize_t bt_test_mode_show(struct device *dev,
     							struct device_attribute *attr, char *buf)
{
	struct bt_gpio_info *bt_info = dev_get_drvdata(dev);
	
	int gpio_val1=0;
	int gpio_val2=0;

	mutex_lock(&bt_info->bt_lock);		

	gpio_val1 = gpio_get_value(GPIO_MEIZU_KEY_VOL_UP);   				//voluem+ 

	s3c_gpio_cfgpin(X_POWER_GPIO, S3C_GPIO_INPUT);
	s3c_gpio_setpull(X_POWER_GPIO, S3C_GPIO_PULL_DOWN);  
	gpio_val2 = gpio_get_value(X_POWER_GPIO);  			 			//bb key on

	if( (gpio_val1 ==0) &&(gpio_val2 ==0) ) 
	{
		mdelay(100);
		gpio_val1 = gpio_get_value(GPIO_MEIZU_KEY_VOL_UP);
		gpio_val2 = gpio_get_value(X_POWER_GPIO);

		if( (gpio_val1 ==0) &&(gpio_val2 ==0) ) 
		{
			bt_info->bt_test_mode =1;            							//test mode
			
			s3c_gpio_cfgpin(WIFI_BT_TEST_LED, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(WIFI_BT_TEST_LED, S3C_GPIO_PULL_NONE);  
			s3c_gpio_setpin(WIFI_BT_TEST_LED, 1);    					 //set led on 
		}
	}
	mutex_unlock(&bt_info->bt_lock);

	return sprintf(buf, "%d\n",  bt_info->bt_test_mode);
	
}


static DEVICE_ATTR(name, S_IRUGO|S_IWUSR|S_IWGRP,
  		 				bt_name_show, NULL);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
   						bt_enable_show, bt_enable_store);
static DEVICE_ATTR(wake, S_IRUGO|S_IWUSR|S_IWGRP,
		   				bt_wake_show, bt_wake_store);
static DEVICE_ATTR(bt_test_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		   				bt_test_mode_show, NULL);

static struct attribute * bcm_attributes[] = {
	&dev_attr_name.attr,
	&dev_attr_enable.attr,
	&dev_attr_wake.attr,
	&dev_attr_bt_test_mode.attr,
	NULL
};

static struct attribute_group bcm_attribute_group = {
	.attrs = bcm_attributes
};

static int  bt_ctr_probe(struct platform_device *pdev)
{
	int irq;
	int ret;

	struct bt_gpio_info *bt_info;
	
	/* Initialize wake locks */
	wake_lock_init(&rfkill_wake_lock, WAKE_LOCK_SUSPEND, "bt_host_wake");

	/* request gpio */
	ret = gpio_request(BT_POWER, "GPB");
	if (ret < 0) {
		pr_err("[BT] Failed to request BT_POWER!\n");
		goto err_req_bt_power;
	}

	ret = gpio_request(BT_RESET, "GPB");
	if (ret < 0) {
		pr_err("[BT] Failed to request BT_RESET!\n");
		goto err_req_bt_reset;
	}

	/* BT Host Wake IRQ */
	irq = gpio_to_irq(BT_HOST_WAKE);
	
	ret = request_irq(irq, bt_host_wake_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"bt_host_wake_irq_handler", NULL);

	if (ret < 0) {
		pr_err("[BT] Request_irq failed\n");
		goto err_req_irq;
	}

	disable_irq(irq);

	/* init rfkill */
	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&bt_rfkill_ops, NULL);

	if (!bt_rfk) {
		pr_err("[BT] bt_rfk : rfkill_alloc is failed\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	rfkill_init_sw_state(bt_rfk, 0);	

	ret = rfkill_register(bt_rfk);
	if (ret) {
		pr_debug("********ERROR IN REGISTERING THE RFKILL********\n");
		goto err_register;
	}

	rfkill_set_sw_state(bt_rfk, 1);
	
	/* init low power state*/
	ret = bt_lpm_init();
	if (ret < 0) {
		pr_debug("[BT]  set low power failed\n");
		goto err_register;
	}

	/* create sysfs attributes */
	bt_info = kzalloc(sizeof(struct bt_gpio_info), GFP_KERNEL);
	if(!bt_info) {
		pr_debug("[BT]  sysfs_create_group failed\n");
		goto err_register;
	}

	bt_info->bt_test_mode =0;     //bt   in normal mode
	bt_info->bt_enable = 0;
	bt_info->bt_wake = 0;
	mutex_init(&bt_info->bt_lock);	
	
	bt_info->dev = &pdev->dev;
	platform_set_drvdata(pdev, bt_info);

	ret = sysfs_create_group(&pdev->dev.kobj, &bcm_attribute_group);
	if (ret < 0) {
		pr_debug("[BT]  sysfs_create_group failed\n");
		goto err_register;
	}

	device_init_wakeup(&pdev->dev, 1);

	/* set init power state*/
	bt_set_power(NULL, RFKILL_USER_STATE_SOFT_BLOCKED);

	return ret;

 err_register:
	rfkill_destroy(bt_rfk);

 err_alloc:
	free_irq(irq, NULL);

 err_req_irq:
	gpio_free(BT_RESET);

 err_req_bt_reset:
	gpio_free(BT_POWER);

 err_req_bt_power:
	return ret;
}

static int bt_ctr_remove(struct platform_device *pdev)
{
	int irq = gpio_to_irq(BT_HOST_WAKE);
	struct bt_gpio_info *bt_info = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &bcm_attribute_group);
	free_irq(irq, bt_info);
	rfkill_unregister(bt_rfk);
	bt_set_power(NULL, RFKILL_USER_STATE_SOFT_BLOCKED);
	gpio_free(BT_WAKE);
	gpio_free(BT_RESET);
	gpio_free(BT_POWER);
	return 0;	
}

static struct platform_driver bt_device_ctr = {
	.probe = bt_ctr_probe,
	.remove = __devexit_p(bt_ctr_remove),
	.driver = {
		.name = "bt_ctr",
		.owner = THIS_MODULE,
	},
};

static int __init bt_ctr_init(void)
{
	int rc = 0;
	rc = platform_driver_register(&bt_device_ctr);

	return rc;
}

static void __exit bt_ctr_exit(void)
{
	return platform_driver_unregister(&bt_device_ctr);
}

module_init(bt_ctr_init);
module_exit(bt_ctr_exit);
MODULE_DESCRIPTION("m9 bluetooth rfkill");
MODULE_LICENSE("GPL");
