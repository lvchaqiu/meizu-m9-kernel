/*
 *  drivers/switch/switch_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-adc.h>
#include <plat/adc.h>
#include <linux/input.h>
#include <linux/delay.h>

struct gpio_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct delayed_work del_work;
};
    
extern int s3c_adc_get_adc_data(int);
extern bool get_ext_mic_bias(void);

struct gpio_switch_data *switch_data;
struct input_dev *mic_input_dev;
struct timer_list detect_mic_adc_timer;
struct work_struct mic_adc_work;
static int is_adc_timer_on = 0;
static unsigned char is_mic_key_pressed = 0;

static void mic_adc_work_func(unsigned long data)
{

    if (switch_data && get_ext_mic_bias() && !gpio_get_value(switch_data->gpio) ) {
        if (s3c_adc_get_adc_data(4) <= 5) {  // mic key is pressed
            msleep(5);
            if (s3c_adc_get_adc_data(4) <= 5) {
                input_report_key(mic_input_dev, KEY_FORWARDMAIL, 1);                
                input_sync(mic_input_dev);
                is_mic_key_pressed = 1;
            }
        } else {
            msleep(5);
            if (get_ext_mic_bias() && (s3c_adc_get_adc_data(4) > 5) ) {
                if (is_mic_key_pressed) {
                    input_report_key(mic_input_dev, KEY_FORWARDMAIL, 0);                
                    input_sync(mic_input_dev);
                    is_mic_key_pressed = 0;
                }
            }
        }
    }
}

static int mic_adc_detect_timer_func(unsigned long data)
{
    schedule_work(&mic_adc_work);
    mod_timer(&detect_mic_adc_timer, jiffies + msecs_to_jiffies(40));
    return 0;
}

static void gpio_switch_work(struct work_struct *work)
{
	int state;
	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, del_work.work);

    if (!get_ext_mic_bias()) {
        schedule_delayed_work(&data->del_work, msecs_to_jiffies(100));
        return;
    }

	state = gpio_get_value(data->gpio);
	msleep(100);
	if (state != gpio_get_value(data->gpio)) {
      return;
  }
	irq_set_irq_type(data->irq, state ? IRQF_TRIGGER_FALLING: IRQF_TRIGGER_RISING);
	state = !state;

	if (state) {    // ear jack plugin
		int loop_cnt;
		state = 2;
		for (loop_cnt = 0; loop_cnt < 10; loop_cnt++) {
			if (get_ext_mic_bias() && (s3c_adc_get_adc_data(4) > 5)) { 
				state = 1;     // set to no mic ear phone
				break;
			}
			msleep(10);
		}
		if (state==1 && !is_adc_timer_on) {
			add_timer(&detect_mic_adc_timer);
			mod_timer(&detect_mic_adc_timer, jiffies + msecs_to_jiffies(200));
			is_adc_timer_on = 1;
		}
	} else {        // ear jack take out
		if (is_adc_timer_on) {
			del_timer_sync(&detect_mic_adc_timer);
			is_adc_timer_on = 0;
      }
      if (is_mic_key_pressed) {
          input_report_key(mic_input_dev, KEY_FORWARDMAIL, 0);                
          input_sync(mic_input_dev);
          is_mic_key_pressed = 0;
		}
	}
	switch_set_state(&data->sdev, state);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
	    (struct gpio_switch_data *)dev_id;

	schedule_delayed_work(&switch_data->del_work, msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

static ssize_t switch_gpio_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static int gpio_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;

	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, switch_data);
	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->sdev.print_state = switch_gpio_print_state;

    ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	ret = gpio_request(switch_data->gpio, pdev->name);
	if (ret < 0)
		goto err_request_gpio;

	ret = gpio_direction_input(switch_data->gpio);
	if (ret < 0)
		goto err_set_gpio_input;

	INIT_DELAYED_WORK(&switch_data->del_work, gpio_switch_work);
	INIT_WORK(&mic_adc_work, (void(*)(struct work_struct *))mic_adc_work_func);

	mic_input_dev = input_allocate_device();
	mic_input_dev->name = "m9w-mic-input"; 
	mic_input_dev->phys = "m9w-mic-input/input0";
	if (input_register_device(mic_input_dev) != 0) {
		input_free_device(mic_input_dev);
		goto err_request_irq;
	}

	set_bit(EV_KEY, mic_input_dev->evbit);
	set_bit(KEY_FORWARDMAIL, mic_input_dev->keybit);

	setup_timer(&detect_mic_adc_timer, (void(*)(unsigned long))mic_adc_detect_timer_func, 0);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		goto err_detect_irq_num_failed;
	}

	ret = request_irq(switch_data->irq, gpio_irq_handler, IRQF_TRIGGER_NONE, pdev->name, switch_data);

	if (ret < 0)
		goto err_request_irq;

	/* Perform initial detection */
	schedule_delayed_work(&switch_data->del_work, msecs_to_jiffies(100));

	return 0;

err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int __devexit gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	if (is_adc_timer_on) {
		del_timer(&detect_mic_adc_timer);
	}
	cancel_delayed_work_sync(&switch_data->del_work);
	cancel_work_sync(&mic_adc_work);

	input_unregister_device(mic_input_dev);

	gpio_free(switch_data->gpio);
	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	gpio_free(switch_data->gpio);
	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	return 0;
}

#ifdef CONFIG_PM
static int gpio_switch_suspend(struct platform_device *pdev, pm_message_t state)
{
    if (is_adc_timer_on) {
        del_timer(&detect_mic_adc_timer);
        is_adc_timer_on = 0;
        cancel_work_sync(&mic_adc_work);
    }

    return 0;
}

static int gpio_switch_resume(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	schedule_delayed_work(&switch_data->del_work, msecs_to_jiffies(100));

    return 0;
}
#else
#define gpio_switch_suspend NULL
#define gpio_switch_resume  NULL
#endif
static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= __devexit_p(gpio_switch_remove),
	.suspend  = gpio_switch_suspend,
	.resume   = gpio_switch_resume,
	.driver		= {
		.name	= "switch-gpio",
		.owner	= THIS_MODULE,
	},
};

static int __init gpio_switch_init(void)
{
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	platform_driver_unregister(&gpio_switch_driver);
}

#ifdef CONFIG_MACH_MEIZU_M9W
late_initcall(gpio_switch_init);
#else
module_init(gpio_switch_init);
#endif
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
