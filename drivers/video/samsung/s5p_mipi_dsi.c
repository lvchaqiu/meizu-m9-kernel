/* linux/drivers/video/samsung/s5p_mipi.c
 *
 * Samsung MIPI-DSI driver.
 *
 * InKi Dae, <inki.dae@xxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/gpio.h>
#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#endif

#include <plat/fb.h>
#include <plat/regs-dsim.h>
#include <plat/mipi-dsi.h>
#include <plat/mipi-ddi.h>

#include <mach/map.h>
#ifdef CONFIG_CPU_FREQ
//#include <mach/cpu-freq-m9w.h>
#include <linux/cpufreq.h>
#endif

#include "s5p_mipi_dsi_common.h"

#define REPEATE_CNT	50

static void s5p_dsim_late_resume(struct early_suspend *h);
static void s5p_dsim_early_suspend(struct early_suspend *h);
static void s5p_dsim_earler_suspend(struct early_suspend *h);

#define master_to_driver(a)	(a->lcd_info->mipi_drv)
#define master_to_device(a)	(a->lcd_info->mipi_dev)
#define set_master_to_device(a)	(a->lcd_info->mipi_dev->master = a)

struct mipi_lcd_info {
	struct list_head	list;
	struct mipi_lcd_driver	*mipi_drv;
	struct mipi_lcd_device	*mipi_dev;
};

static LIST_HEAD(lcd_info_list);
static DEFINE_MUTEX(mipi_lock);

struct s5p_platform_dsim *to_dsim_plat(struct platform_device *pdev)
{
	return (struct s5p_platform_dsim *)pdev->dev.platform_data;
}

static irqreturn_t s5p_mipi_interrupt_handler(int irq, void *dev_id)
{
	struct dsim_device *dsim = (struct dsim_device *)dev_id;
	unsigned int reg;

	reg = __raw_readl(dsim->reg_base + S5P_DSIM_INTSRC);
	__raw_writel(reg, dsim->reg_base + S5P_DSIM_INTSRC);

	return IRQ_HANDLED;
}

int s5p_mipi_register_lcd_driver(struct mipi_lcd_driver *lcd_drv)
{
	struct mipi_lcd_info *lcd_info;
	struct mipi_lcd_device *mipi_dev;
	static unsigned int id = 1;
	int ret;

	lcd_info = kzalloc(sizeof(struct mipi_lcd_info), GFP_KERNEL);
	if (!lcd_info) {
		printk(KERN_ERR "failed to allocate lcd_info object.\n");
		return -EFAULT;
	}

	lcd_info->mipi_drv = lcd_drv;

	mipi_dev = kzalloc(sizeof(struct mipi_lcd_device), GFP_KERNEL);
	if (!lcd_info) {
		printk(KERN_ERR "failed to allocate mipi_dev object.\n");
		ret = -EFAULT;
		goto err_mipi_dev;
	}

	mutex_lock(&mipi_lock);

	mipi_dev->id = id++;
	lcd_info->mipi_dev = mipi_dev;

	device_initialize(&mipi_dev->dev);

	strcpy(mipi_dev->modalias, lcd_drv->name);

	dev_set_name(&mipi_dev->dev, "mipi-dsi.%d", mipi_dev->id);

	ret = device_add(&mipi_dev->dev);
	if (ret < 0) {
		printk(KERN_ERR "can't %s %s, status %d\n",
				"add", dev_name(&mipi_dev->dev), ret);
		id--;
		goto err_device_add;
	}

	list_add_tail(&lcd_info->list, &lcd_info_list);

	mutex_unlock(&mipi_lock);

	printk(KERN_INFO "registered panel driver(%s) to mipi-dsi driver.\n",
		lcd_drv->name);

	return ret;

err_device_add:
	kfree(mipi_dev);

err_mipi_dev:
	kfree(lcd_info);

	return ret;
}

/*
 * This function is a wrapper for changing transfer mode.
 * It is used for the panel driver before and after changing gamma value.
 */
int s5p_mipi_change_transfer_mode(struct dsim_device *dsim, unsigned int mode)
{
	if (mode < 0 || mode > 1) {
		dev_err(dsim->dev, "mode range should be 0 or 1.\n");
		return -EINVAL;
	}

	if (mode == 0)
		s5p_mipi_set_data_transfer_mode(dsim,
			DSIM_TRANSFER_BYCPU, mode);
	else
		s5p_mipi_set_data_transfer_mode(dsim,
			DSIM_TRANSFER_BYLCDC, mode);

	return 0;
}

struct mipi_lcd_info *find_mipi_client_registered(struct dsim_device *dsim,
						const char *name)
{
	struct mipi_lcd_info *lcd_info;
	struct mipi_lcd_driver *mipi_drv = NULL;

	mutex_lock(&mipi_lock);

	dev_dbg(dsim->dev, "find lcd panel driver(%s).\n",
		name);

	list_for_each_entry(lcd_info, &lcd_info_list, list) {
		mipi_drv = lcd_info->mipi_drv;

		if ((strcmp(mipi_drv->name, name)) == 0) {
			mutex_unlock(&mipi_lock);
			dev_dbg(dsim->dev, "found!!!(%s).\n", mipi_drv->name);
			return lcd_info;
		}
	}

	dev_warn(dsim->dev, "failed to find lcd panel driver(%s).\n",
		name);

	mutex_unlock(&mipi_lock);

	return NULL;
}

/* define MIPI-DSI Master operations. */
static struct dsim_master_ops master_ops = {
	.cmd_write			= s5p_mipi_wr_data,
	.get_dsim_frame_done		= s5p_mipi_get_frame_done_status,
	.clear_dsim_frame_done		= s5p_mipi_clear_frame_done,
	.change_dsim_transfer_mode	= s5p_mipi_change_transfer_mode,
};

static int s5p_mipi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct dsim_device *dsim;
	struct dsim_config *dsim_info;
	struct s5p_platform_dsim *dsim_pd;
	int ret = -1;

    /*if enter enginer test mode?*/
	if (!gpio_get_value(X_POWER_GPIO))
		return -EPERM;

	dsim = kzalloc(sizeof(struct dsim_device), GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -EFAULT;
	}

	dsim->pd = to_dsim_plat(pdev);
	dsim->dev = &pdev->dev;
	dsim->resume_complete = 0;

	/* get s5p_platform_dsim. */
	dsim_pd = (struct s5p_platform_dsim *)dsim->pd;
	/* get dsim_config. */
	dsim_info = dsim_pd->dsim_info;
	dsim->dsim_info = dsim_info;
	dsim->master_ops = &master_ops;

	dsim->clock = clk_get(&pdev->dev, "dsim");
	if (IS_ERR(dsim->clock)) {
		dev_err(&pdev->dev, "failed to get dsim clock source\n");
		goto err_clock_get;
	}

	clk_enable(dsim->clock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		ret = -EINVAL;
		goto err_platform_get;
	}

	res = request_mem_region(res->start, resource_size(res),
					dev_name(&pdev->dev));
	if (!res) {
		dev_err(&pdev->dev, "failed to request io memory region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	dsim->res = res;

	dsim->reg_base = ioremap(res->start, resource_size(res));
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	/*
	 * it uses frame done interrupt handler
	 * only in case of MIPI Video mode.
	 */
	if (dsim_info->e_interface == DSIM_VIDEO) {
		dsim->irq = platform_get_irq(pdev, 0);
		if (request_irq(dsim->irq, s5p_mipi_interrupt_handler,
				IRQF_DISABLED, "mipi-dsi", dsim)) {
			dev_err(&pdev->dev, "request_irq failed.\n");
			goto err_trigger_irq;
		}
	}

	dsim->regulator_pd = regulator_get(dsim->dev, "pd");
	if (IS_ERR(dsim->regulator_pd)) {
		dev_err(dsim->dev, "failed to get regulator_pd\n");
		ret = -EINVAL;
		goto err_get_pd;
	}
	ret = regulator_enable(dsim->regulator_pd);
	if (ret < 0) {
		dev_err(dsim->dev, "failed to enable regulator_pd\n");
		ret = -EINVAL;
		goto err_pd_power;
	}

	/*
	 * enable mipi dsim power
	 */
	dsim->regulator = regulator_get(dsim->dev, "vcc_mipi");
	if (IS_ERR(dsim->regulator)) {
		dev_err(dsim->dev, "failed to get regulator\n");
		ret = -EINVAL;
		goto err_get_power;
	}
	ret = regulator_enable(dsim->regulator);
	if (ret < 0) {
		dev_err(dsim->dev, "failed to enable regulator\n");
		ret = -EINVAL;
		goto err_mipi_power;
	}

	/* find lcd panel driver registered to mipi-dsi driver. */
	dsim->lcd_info = find_mipi_client_registered(dsim,
				dsim_pd->lcd_panel_name);
	if (dsim->lcd_info == NULL) {
		dev_err(&pdev->dev, "lcd_info is NULL.\n");
		goto err_lcd_info;
	}

	/* set dsim to master of mipi_dev. */
	set_master_to_device(dsim);

	/* open lcd pannel power and reset lcd*/
	if (master_to_driver(dsim) && master_to_driver(dsim)->probe)
		master_to_driver(dsim)->probe(master_to_device(dsim));

retry:
	s5p_mipi_init_dsim(dsim);
	dsim->state = DSIM_STATE_INIT;
	s5p_mipi_init_link(dsim);

	s5p_mipi_set_hs_enable(dsim);
	s5p_mipi_set_data_transfer_mode(dsim, DSIM_TRANSFER_BOTH, 0);

	/* it needs delay for stabilization */
	mdelay(dsim->pd->delay_for_stabilization);

	/* initialize mipi-dsi client(lcd panel). */
	if (master_to_driver(dsim) && master_to_driver(dsim)->init_lcd) {
		static int cnt = 0;
		ret = master_to_driver(dsim)->init_lcd(master_to_device(dsim));
		if (ret && ++cnt<REPEATE_CNT) {
			dev_err(&pdev->dev, "mipi-dsi init_lcd failed.\n");
			if (dsim && dsim->pd && dsim->pd->init_d_phy)
				dsim->pd->init_d_phy(dsim, 0);

			if (dsim && dsim->pd && dsim->pd->part_reset)
				dsim->pd->part_reset(dsim);

			clk_disable(dsim->clock);
			regulator_disable(dsim->regulator_pd);
			mdelay(1);
			regulator_enable(dsim->regulator_pd);
			clk_enable(dsim->clock);
			mdelay(50);
			master_to_driver(dsim)->reset_lcd(master_to_device(dsim));
			goto retry;
		} else {
			cnt = 0;
			if (ret)
				panic("init lcd failed\n");
		}
	}

	s5p_mipi_set_display_mode(dsim, dsim->dsim_info);

	s5p_mipi_set_data_transfer_mode(dsim, DSIM_TRANSFER_BYLCDC, 1);

	/* in case of command mode, trigger. */
	if (dsim->dsim_info->e_interface == DSIM_COMMAND) {
		if (dsim_pd->trigger)
			dsim_pd->trigger(registered_fb[0]);
		else
			dev_warn(&pdev->dev, "trigger is null.\n");
	}

	platform_set_drvdata(pdev, dsim);

#ifdef CONFIG_HAS_EARLYSUSPEND
	dsim->early_suspend.suspend = s5p_dsim_early_suspend;
	dsim->early_suspend.resume = s5p_dsim_late_resume;
	dsim->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB+1;
	register_early_suspend(&dsim->early_suspend);

	dsim->earler_suspend.suspend = s5p_dsim_earler_suspend;
	dsim->earler_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&dsim->earler_suspend);
#endif

	dev_info(&pdev->dev, "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim_info->e_interface == DSIM_COMMAND) ?
			"CPU" : "RGB");

	return 0;

err_lcd_info:
	regulator_disable(dsim->regulator);

err_mipi_power:
	regulator_put(dsim->regulator);

err_get_power:
	regulator_disable(dsim->regulator_pd);

err_pd_power:
	regulator_put(dsim->regulator_pd);

err_get_pd:
	if (dsim->dsim_info->e_interface == DSIM_VIDEO)
		free_irq(dsim->irq, dsim);

err_trigger_irq:
	release_resource(dsim->res);
	kfree(dsim->res);

	iounmap((void __iomem *) dsim->reg_base);

err_mem_region:
err_platform_get:
	clk_disable(dsim->clock);
	clk_put(dsim->clock);

err_clock_get:
	kfree(dsim);

	return ret;

}

static int __devexit s5p_mipi_remove(struct platform_device *pdev)
{
	struct dsim_device *dsim = platform_get_drvdata(pdev);
	struct mipi_lcd_info *lcd_info;

    if (master_to_driver(dsim) && master_to_driver(dsim)->remove)
		master_to_driver(dsim)->remove(master_to_device(dsim));

	if (dsim->dsim_info->e_interface == DSIM_VIDEO)
		free_irq(dsim->irq, dsim);

	iounmap(dsim->reg_base);

	clk_disable(dsim->clock);
	clk_put(dsim->clock);

	regulator_put(dsim->regulator);
	regulator_put(dsim->regulator_pd);

	release_resource(dsim->res);
	kfree(dsim->res);

	list_for_each_entry(lcd_info, &lcd_info_list, list)
		if (lcd_info)
			kfree(lcd_info);

	kfree(dsim);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void s5p_dsim_earler_suspend(struct early_suspend *h)
{
	struct dsim_device *dsim = container_of(h, struct dsim_device, earler_suspend);

	if (master_to_driver(dsim) && master_to_driver(dsim)->suspend)
		master_to_driver(dsim)->suspend(master_to_device(dsim));
}

static void s5p_dsim_early_suspend(struct early_suspend *h)
{
	struct dsim_device *dsim = container_of(h, struct dsim_device, early_suspend);

	if (master_to_driver(dsim) && (master_to_driver(dsim))->shutdown)
		(master_to_driver(dsim))->shutdown(master_to_device(dsim));

	if (dsim && dsim->pd && dsim->pd->init_d_phy)
		dsim->pd->init_d_phy(dsim, 0);

	clk_disable(dsim->clock);

	regulator_disable(dsim->regulator);
	regulator_disable(dsim->regulator_pd);
}

static void s5p_dsim_late_resume(struct early_suspend *h)
{
	struct dsim_device *dsim = container_of(h, struct dsim_device, early_suspend);
	int ret;
	ret = regulator_enable(dsim->regulator_pd);
	if (ret < 0) {
		dev_err(dsim->dev, "failed to enable regulator_pd\n");
		return;
	}
	clk_enable(dsim->clock);

	ret = regulator_enable(dsim->regulator);
	if (ret < 0) {
		regulator_disable(dsim->regulator_pd);
		dev_err(dsim->dev, "failed to enable regulator\n");
		return;
	}

#ifdef CONFIG_CPU_FREQ
	cpufreq_driver_target(cpufreq_cpu_get(0), 400000,
				0x10);
#endif

repeat:
	if (master_to_driver(dsim) && (master_to_driver(dsim))->resume)
		ret = (master_to_driver(dsim))->resume(master_to_device(dsim));

	s5p_mipi_init_dsim(dsim);
	dsim->state = DSIM_STATE_INIT;
	s5p_mipi_init_link(dsim);

	s5p_mipi_set_hs_enable(dsim);
	s5p_mipi_set_data_transfer_mode(dsim, DSIM_TRANSFER_BOTH, 0);

	/* it needs delay for stabilization */
	mdelay(dsim->pd->delay_for_stabilization);

	if (master_to_driver(dsim) && (master_to_driver(dsim))->init_lcd) {
		static int cnt = 0;
		ret = (master_to_driver(dsim))->init_lcd(master_to_device(dsim));
		if (ret && ++cnt<REPEATE_CNT) {
			dev_err(dsim->dev, "mipi-dsi init_lcd failed.\n");

			if (dsim && dsim->pd && dsim->pd->init_d_phy)
				dsim->pd->init_d_phy(dsim, 0);

			if (dsim && dsim->pd && dsim->pd->part_reset)
				dsim->pd->part_reset(dsim);

			clk_disable(dsim->clock);
			regulator_disable(dsim->regulator_pd);
			udelay(5);
			regulator_enable(dsim->regulator_pd);
			clk_enable(dsim->clock);
			goto repeat;

		} else {
			cnt = 0;
			if (ret)
				panic("resume lcd timeout\n");
		}
	}

	s5p_mipi_set_display_mode(dsim, dsim->dsim_info);
	s5p_mipi_set_data_transfer_mode(dsim, DSIM_TRANSFER_BYLCDC, 1);

#ifdef CONFIG_CPU_FREQ
	cpufreq_driver_target(cpufreq_cpu_get(0), 400000,
				0x20);
#endif
}
#endif

static struct platform_driver s5p_mipi_driver = {
	.probe = s5p_mipi_probe,
	.remove = __devexit_p(s5p_mipi_remove),
	.driver = {
		.name = "s5p-dsim",
		.owner = THIS_MODULE,
	},
};

static int s5p_mipi_register(void)
{
	platform_driver_register(&s5p_mipi_driver);

	return 0;
}

static void s5p_mipi_unregister(void)
{
	platform_driver_unregister(&s5p_mipi_driver);
}

module_init(s5p_mipi_register);
module_exit(s5p_mipi_unregister);

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung MIPI-DSI driver");
MODULE_LICENSE("GPL");
