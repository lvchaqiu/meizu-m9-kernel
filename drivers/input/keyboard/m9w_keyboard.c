/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 * 
 * Copyright (C) 2009 Meizu Technology Co.Ltd, Zhuhai, China
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
 * Inital code : Mar 10 , 2010 : lvcha@meizu.com
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/pm.h>

struct m9w_button_data {
	struct gpio_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
};

struct m9w_keys_drvdata {
	struct input_dev *input;
	struct m9w_button_data data[0];
};

static void m9w_keys_report_event(struct m9w_button_data *bdata)
{
	struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned int type = button->type ?button->type: EV_KEY;
  	int state;

    state = (button->last_state ? 1 : 0) ^ button->active_low;

	pr_debug("m9w_keys_report_event:button->desc:%s, type=%d, state:%d\n",
			button->desc, type, state);
	
	input_event(input, type, button->code, !!state);
	input_sync(input);
}

static void m9w_check_button(unsigned long _data)
{
	struct m9w_button_data *data = (struct m9w_button_data *)_data;
	struct gpio_keys_button *button=data->button;
	int is_active;
	int down = 0;

	pr_debug("m9w_check_button:gpio=%d;desc:%s\n",button->gpio,button->desc);

	down = gpio_get_value(button->gpio);

	if(button->last_state == down) {    //should always be here except for unstable temporary trigger.
		m9w_keys_report_event(data);
	} else {
		printk("Unstable m9w_check_button:gpio=%d;desc:%s,active_low %d\n",
				button->gpio,button->desc,button->active_low);
		is_active = (button->active_low)? !down : down;
		if(is_active) { 
			if(button->code == KEY_HOME)
				mod_timer(&data->timer,jiffies + msecs_to_jiffies(100));
			else
				mod_timer(&data->timer,jiffies + msecs_to_jiffies(500));
		}
	}
}

static irqreturn_t m9w_keys_isr(int irq, void *dev_id)
{
	struct m9w_button_data *bdata = dev_id;
	struct gpio_keys_button *button = bdata->button;
	int down;

	if(!button)
		return IRQ_HANDLED;
	
	BUG_ON(irq != gpio_to_irq(button->gpio));

	down = gpio_get_value(button->gpio);

	pr_debug("m9w_keys_isr:gpio=%d;desc:%s\n",button->gpio,button->desc);
	
	/* the power button of the ipaq are tricky. They send 'released' events even
	 * when the button are already released. The work-around is to proceed only
	 * if the state changed.
	 **/

	if (button->last_state == down)
		return IRQ_HANDLED;
	
	button->last_state = down;

	if (button->debounce_interval) {
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(button->debounce_interval));
    } else {
		m9w_keys_report_event(bdata);
    }

	return IRQ_HANDLED;
}

static int __devinit m9w_keys_probe(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct m9w_keys_drvdata *ddata;
	struct input_dev *input;
	int i, error;
	int wakeup = 0;

	ddata = kzalloc(sizeof(struct m9w_keys_drvdata) +
			pdata->nbuttons * sizeof(struct m9w_button_data),
			GFP_KERNEL);
	input = input_allocate_device();
	if (!ddata || !input) {
		error = -ENOMEM;
		goto fail1;
	}

	platform_set_drvdata(pdev, ddata);

	input->name = pdev->name;
	input->phys = "gpio-keys/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	ddata->input = input;

	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_keys_button *button = &pdata->buttons[i];
		struct m9w_button_data *bdata = &ddata->data[i];
		int irq, active_low;
		unsigned int type = button->type ? : EV_KEY;

		bdata->input = input;
		bdata->button = button;
		setup_timer(&bdata->timer, m9w_check_button, (unsigned long)bdata);

		error = gpio_request(button->gpio, button->desc ?: "gpio_keys");
		if (error < 0) {
			pr_err("gpio-keys: failed to request GPIO %d,"
				" error %d,button Desc:%s\n", button->gpio, error, button->desc);
			goto fail2;
		}

		error = gpio_direction_input(button->gpio);
		if (error < 0) {
			pr_err("gpio-keys: failed to configure input"
				" direction for GPIO %d, error %d\n",
				button->gpio, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		active_low = gpio_get_value(button->gpio);

		if(button->active_low != active_low) {
			printk(KERN_WARNING "button->gpio = %d; active_low = %d, button->active_low = %d\n",
				button->gpio, active_low, button->active_low);
		}

		irq = gpio_to_irq(button->gpio);
		if (irq < 0) {
			error = irq;
			pr_err("gpio-keys: Unable to get irq number for GPIO %d, error %d\n",
				button->gpio, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		button->last_state = gpio_get_value(button->gpio);
	    	error = request_threaded_irq(irq, NULL, m9w_keys_isr,IRQF_SAMPLE_RANDOM | 
	            IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING,
	            button->desc ? button->desc : "gpio_keys", bdata);

		if (error) {
			pr_err("gpio-keys: Unable to claim irq %d; error %d GPIO %d\n",
				irq, error, button->gpio);
			gpio_free(button->gpio);
			goto fail2;
		}
		
		if (button->wakeup)
			wakeup = 1;

		input_set_capability(input, type, button->code);
	}

	error = input_register_device(input);
	if (error) {
		pr_err("gpio-keys: Unable to register input device, "
			"error: %d\n", error);
		goto fail2;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	return 0;

 fail2:
	while (--i >= 0) {
		free_irq(gpio_to_irq(pdata->buttons[i].gpio), &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		gpio_free(pdata->buttons[i].gpio);
	}

	platform_set_drvdata(pdev, NULL);
 fail1:
	input_free_device(input);
	kfree(ddata);

	return error;
}

static int __devexit m9w_keys_remove(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct m9w_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

	device_init_wakeup(&pdev->dev, 0);

	for (i = 0; i < pdata->nbuttons; i++) {
		int irq = gpio_to_irq(pdata->buttons[i].gpio);
		free_irq(irq, &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		gpio_free(pdata->buttons[i].gpio);
	}

	input_unregister_device(input);

	return 0;
}


#ifdef CONFIG_PM
static int m9w_keys_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	if (device_may_wakeup(&pdev->dev)) {
		for (i = 0; i < pdata->nbuttons; i++) {
			struct gpio_keys_button *button = &pdata->buttons[i];
			if (button->wakeup) {
				int irq = gpio_to_irq(button->gpio);
				enable_irq_wake(irq);
			}
		}
	}
	return 0;
}

static int m9w_keys_resume(struct platform_device *pdev)
{

	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct m9w_keys_drvdata *ddata = platform_get_drvdata(pdev);
	int i;
	int sytem_wakeup = false;
	wake_type_t m9w_wake_typed = m9w_get_wakeup_type();
	wake_type_t allow_wake_type = USB_WAKE | LOWBAT_WAKE/* |BLUETOOTH_WAKE*/;

#ifdef CONFIG_AUTOSUSPEND_TEST
       /* Force to send a HOME key to wake the whole system */
       m9w_wake_typed |= KEY_HOME_WAKE;
#endif

	if (m9w_wake_typed & allow_wake_type) {
		input_report_key(ddata->input, KEY_HOME, 1);
		udelay(5);
		input_report_key(ddata->input, KEY_HOME, 0);
		sytem_wakeup = true;
	}

	if (device_may_wakeup(&pdev->dev)) {
		for (i = 0; i < pdata->nbuttons; i++) {
			struct gpio_keys_button *button = &pdata->buttons[i];
			if (button->wakeup) {
				int irq = gpio_to_irq(button->gpio);
				int down = gpio_get_value(button->gpio);

				disable_irq_wake(irq);

				if (down == 0 && sytem_wakeup == false) {
					struct m9w_button_data *bdata = &ddata->data[i];
					button->last_state = down;
					mod_timer(&bdata->timer, jiffies + msecs_to_jiffies(1));
				}
			}
		}
	}

	return 0;
}
#else
#define m9w_keys_suspend	NULL
#define m9w_keys_resume	NULL
#endif

static struct platform_driver m9w_keys_device_driver = {
	.probe		= m9w_keys_probe,
	.remove		= __devexit_p(m9w_keys_remove),
	.suspend		= m9w_keys_suspend,
	.resume		= m9w_keys_resume,
	.driver		= {
		.name	= "m9w_keyboard",
		.owner	= THIS_MODULE,
	}
};

static int __init m9w_keys_init(void)
{
	return platform_driver_register(&m9w_keys_device_driver);
}

static void __exit m9w_keys_exit(void)
{
	platform_driver_unregister(&m9w_keys_device_driver);
}

module_init(m9w_keys_init);
module_exit(m9w_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lvcha Qiu<lvcha@meizu.com>");
MODULE_DESCRIPTION("Keyboard driver for Meizu M9W");
