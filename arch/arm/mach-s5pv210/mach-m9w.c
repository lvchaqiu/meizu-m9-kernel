/* linux/arch/arm/mach-s5pv210/mach-m9w.c
 *
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: lvcha qiu <lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/ltc3577.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/usb/ch9.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/skbuff.h>
#include <linux/console.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/system.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>
#include <mach/gpio-herring.h>
#include <mach/adc.h>
#include <mach/param.h>
#include <mach/spi-clocks.h>
#include <mach/media.h>

#include <linux/usb/gadget.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/wlan_plat.h>
#include <mach/m9w_xgold_spi.h>

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#include <linux/platform_data/ram_console.h>
#endif

#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif

#ifdef CONFIG_S5PV210_POWER_DOMAIN
#include <mach/power-domain.h>
#endif

#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>
#include <plat/mfc.h>
#include <plat/iic.h>
#include <plat/pm.h>
#include <plat/media.h>

#include <plat/sdhci.h>
#include <plat/fimc.h>
#include <plat/csis.h>
#include <plat/jpeg.h>
#include <plat/clock.h>
#include <plat/regs-otg.h>
#include <linux/gp2a.h>
#include <../../../drivers/video/samsung/s3cfb.h>
#include <linux/switch.h>
#include <plat/mipi-dsi.h>
#include <plat/fimg2d.h>
#include <plat/s3c64xx-spi.h>

#include <media/rj64sc110_platform.h>

extern int m9w_bl_on(int onoff);
extern void bt_uart_wake_peer(struct uart_port *port);
extern int (*s5p_config_sleep_gpio_table)(void);

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define S5PV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define S5PV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define S5PV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg m9w_uartcfgs[] __initdata = {
	{
		.hwport		= 0,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
#ifdef CONFIG_BT_BCM4329
		.wake_peer	= bt_uart_wake_peer,
#endif
	},
	{
		.hwport		= 1,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
#ifndef CONFIG_FIQ_DEBUGGER
	{
		.hwport		= 2,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
#endif
	{
		.hwport		= 3,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
};

#ifdef CONFIG_S5P_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
	/* s5pc110 support 12-bit resolution */
	.delay  = 10000,
	.presc  = 65,
	.resolution = 12,
};
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static unsigned int ram_console_start;
static unsigned int ram_console_size;

static struct resource ram_console_resource[] = {
	{
		.flags = IORESOURCE_MEM,
	}
};

struct ram_console_platform_data ram_console_bootinfo = {
	.bootinfo = RAM_CONSOLE_BOOT_INFO,
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources = ARRAY_SIZE(ram_console_resource),
	.resource = ram_console_resource,
	.dev = {
		.platform_data = &ram_console_bootinfo,
	},
};

static void __init setup_ram_console_mem(void)
{
	ram_console_resource[0].start = ram_console_start;
	ram_console_resource[0].end = ram_console_start + ram_console_size - 1;
}
#endif

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
	.name = "pmem",
	.no_allocator = 1,
	.cached = 1,
	.start = 0,
	.size = 0,
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
	.name = "pmem_gpu1",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct android_pmem_platform_data pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct platform_device pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &pmem_pdata },
};

static struct platform_device pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &pmem_adsp_pdata },
};

static void __init android_pmem_set_platdata(void)
{
	pmem_pdata.start = (u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM, 0);
	pmem_pdata.size = (u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM, 0);

	pmem_gpu1_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_GPU1, 0);
	pmem_gpu1_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_GPU1, 0);

	pmem_adsp_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_ADSP, 0);
	pmem_adsp_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_ADSP, 0);
}
#endif

#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0 (10240*SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1 (9600 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2 (6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (32768 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (32768 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD (9600 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG (2048* SZ_1K)
//#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXSTREAM (14400 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXSTREAM (0)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_G2D (8192 * SZ_1K)

static struct s5p_media_device m9w_media_devs[] = {
	{
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0,
		.paddr = 0,
	}, {
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1,
		.paddr = 0,
	}, {
		.id = S5P_MDEV_FIMC0,
		.name = "fimc0",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0,
		.paddr = 0,
	}, {
		.id = S5P_MDEV_FIMC1,
		.name = "fimc1",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1,
		.paddr = 0,
	}, {
		.id = S5P_MDEV_FIMC2,
		.name = "fimc2",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2,
		.paddr = 0,
	}, {
		.id = S5P_MDEV_JPEG,
		.name = "jpeg",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG,
		.paddr = 0,
	}, {
		.id = S5P_MDEV_FIMD,
		.name = "fimd",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD,
		.paddr = 0,
	},
#ifdef CONFIG_ANDROID_PMEM
	{
		.id = S5P_MDEV_PMEM,
		.name = "pmem",
		.bank = 0,
		.memsize = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM*SZ_1K,
		.paddr = 0,
	}, {
		.id = S5P_MDEV_PMEM_GPU1,
		.name = "pmem_gpu1",
		.bank = 0,
		.memsize = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM_GPU1*SZ_1K,
		.paddr = 0,
	}, {
		.id = S5P_MDEV_PMEM_ADSP,
		.name = "pmem_adsp",
		.bank = 0,
		.memsize = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM_ADSP*SZ_1K,
		.paddr = 0,
	},
#endif
	{
		.id = S5P_MDEV_TEXSTREAM,
		.name = "texstream",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXSTREAM,
		.paddr = 0,
	},
#ifdef CONFIG_VIDEO_G2D
	{
		.id = S5P_MDEV_G2D,
		.name = "g2d",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_G2D,
		.paddr = 0,
	},
#endif
};

static struct platform_device watchdog_device = {
	.name = "watchdog",
	.id = -1,
};

static struct regulator_consumer_supply m9w_buck3_consumer[] = {
	{	.supply	= "vddarm", },
};

static struct regulator_consumer_supply m9w_buck4_consumer[] = {
	{	.supply	= "vddint", },
};

static struct regulator_consumer_supply m9w_isink1_consumer[] = {
	{	.supply	= "charger_cur", },
};

static struct regulator_init_data m9w_buck3_data = {
	.constraints	= {
		.name		= "VDD_ARM",
		.min_uV		= 950000,
		.max_uV		= 1250000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1100000,	//cpuf freq is 800MHZ in sleeping
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(m9w_buck3_consumer),
	.consumer_supplies	= m9w_buck3_consumer,
};

static struct regulator_init_data m9w_buck4_data = {
	.constraints	= {
		.name		= "VDD_INT",
		.min_uV		= 1000000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(m9w_buck4_consumer),
	.consumer_supplies	= m9w_buck4_consumer,
};

static struct regulator_init_data m9w_isink_data = {
	.constraints	= {
		.name		= "CHARGER_CUR",
		.min_uA		= 10000,
		.max_uA		= 1000000,
		.valid_ops_mask	= REGULATOR_CHANGE_CURRENT|
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(m9w_isink1_consumer),
	.consumer_supplies	= m9w_isink1_consumer,
};

static struct ltc3577_regulator_data m9w_regulators[] = {
	{ LTC3577_BUCK3, &m9w_buck3_data },		/* 1.25v ARM CORE */
	{ LTC3577_BUCK4, &m9w_buck4_data },		/* 1.10v ARM INT */
	{ LTC3577_ISINK1, &m9w_isink_data },		/* charger current range: 0,100,500,1000 mA */
};

#ifdef CONFIG_REGULATOR_FIXED_VOLTAGE
static struct regulator_consumer_supply m9w_ldo4_consumer[] = {
	{	.supply	= "vcc_mmc", },
};

static struct regulator_consumer_supply m9w_ldo5_consumer[] = {
	REGULATOR_SUPPLY("vcc_usb", "s3c-usbgadget")
};

static struct regulator_consumer_supply m9w_ldo6_consumer[] = {
	REGULATOR_SUPPLY("vcc_mipi", "s5p-dsim"),
};

static struct regulator_consumer_supply m9w_ldo7_consumer[] = {
	{	.supply	= "vcc_camera_core", },
};

static struct regulator_consumer_supply m9w_ldo8_consumer[] = {
	REGULATOR_SUPPLY("vcc_lcd", "mipi-dsi.1"),
};

static struct regulator_consumer_supply m9w_ldo9_consumer[] = {
	{	.supply	= "vcc_audio_1.8", },
};

static struct regulator_consumer_supply m9w_ldo10_consumer[] = {
	{	.supply	= "vcc_touch", },
};

static struct regulator_consumer_supply m9w_ldo11_consumer[] = {
	{	.supply	= "vcc_gps", },
};

static struct regulator_consumer_supply m9w_ldo12_consumer[] = {
	{	.supply	= "vcc_compass", },
};

/* "VRTC_3.0V" */
static struct regulator_init_data m9w_ldo1_fixed_data = {
	.constraints	= {
		.always_on = 1,
		.state_mem	= {
			.disabled = 1,
		},	
	},
};

static struct fixed_voltage_config m9w_fixed_ldo1_config = {
	.supply_name	= "VRTC_3.0V",
	.microvolts	= 3000000,
	.gpio		= -EINVAL,
	.init_data	= &m9w_ldo1_fixed_data,
};

static struct platform_device m9w_fixed_voltage1 = {
	.name	= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo1_config,
	},
};

/* VALIVE_1.1V */
static struct regulator_init_data m9w_ldo2_fixed_data = {
	.constraints	= {
		.always_on = 1,
		.state_mem	= {
			.enabled = 1,
		},	
	},
};

static struct fixed_voltage_config m9w_fixed_ldo2_config = {
	.supply_name	= "VALIVE_1.1V",
	.microvolts	= 1100000,
	.gpio		= -EINVAL,
	.init_data	= &m9w_ldo2_fixed_data,
};

static struct platform_device m9w_fixed_voltage2 = {
	.name	= "reg-fixed-voltage",
	.id		= 1,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo2_config,
	},
};

/* VPLL_1.1V */
static struct regulator_init_data m9w_ldo3_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},	
	},
};

static struct fixed_voltage_config m9w_fixed_ldo3_config = {
	.supply_name	= "VPLL_1.1V",
	.microvolts	= 1100000,
	.gpio		= -EINVAL,
	.init_data	= &m9w_ldo3_fixed_data,
};

static struct platform_device m9w_fixed_voltage3 = {
	.name	= "reg-fixed-voltage",
	.id		= 2,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo3_config,
	},
};

/* SD_3.0V */
static struct regulator_init_data m9w_ldo4_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(m9w_ldo4_consumer),
	.consumer_supplies	= m9w_ldo4_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo4_config = {
	.supply_name	= "SD_3.0V",
	.microvolts	= 3000000,
	.gpio			= SDHCI2_POWER,
	.enable_high = 0,
	.init_data	= &m9w_ldo4_fixed_data,
};

static struct platform_device m9w_fixed_voltage4 = {
	.name	= "reg-fixed-voltage",
	.id		= 3,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo4_config,
	},
};

/* usb otg phy 3.3v */
static struct regulator_init_data m9w_ldo5_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies= ARRAY_SIZE(m9w_ldo5_consumer),
	.consumer_supplies	= m9w_ldo5_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo5_config = {
	.supply_name	= "VUSB_3.3V",
	.microvolts	= 3300000,
	.gpio			= USB_OTG_PW,
	.enable_high = 1,
	.init_data	= &m9w_ldo5_fixed_data,
};

static struct platform_device m9w_fixed_voltage5 = {
	.name	= "reg-fixed-voltage",
	.id		= 4,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo5_config,
	},
};

/* MIPI I/O 1.8v*/
static struct regulator_init_data m9w_ldo6_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(m9w_ldo6_consumer),
	.consumer_supplies	= m9w_ldo6_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo6_config = {
	.supply_name	= "MIPI_1.8V",
	.microvolts	= 1800000,
	.gpio			= MIPI_DSIM_POWER,
	.enable_high = 1,
	.init_data	= &m9w_ldo6_fixed_data,
};

static struct platform_device m9w_fixed_voltage6 = {
	.name	= "reg-fixed-voltage",
	.id		= 5,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo6_config,
	},
};

/**/
static struct regulator_init_data m9w_ldo7_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(m9w_ldo7_consumer),
	.consumer_supplies	= m9w_ldo7_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo7_config = {
	.supply_name	= "CAMERA_1.8V",
	.microvolts	= 1800000,
	.gpio			= CAMERA_POWER_PIN,
	.enable_high = 1,
	.init_data	= &m9w_ldo7_fixed_data,
};

static struct platform_device m9w_fixed_voltage7 = {
	.name	= "reg-fixed-voltage",
	.id		= 6,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo7_config,
	},
};

/**/
static struct regulator_init_data m9w_ldo8_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(m9w_ldo8_consumer),
	.consumer_supplies	= m9w_ldo8_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo8_config = {
	.supply_name	= "LCD_5.5V",
	.microvolts	= 5500000,
	.gpio			= LCD_POWER_GPIO,
	.enable_high = 1,
	.init_data	= &m9w_ldo8_fixed_data,
};

static struct platform_device m9w_fixed_voltage8 = {
	.name	= "reg-fixed-voltage",
	.id		= 7,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo8_config,
	},
};

/**/
static struct regulator_init_data m9w_ldo9_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(m9w_ldo9_consumer),
	.consumer_supplies	= m9w_ldo9_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo9_config = {
	.supply_name	= "AUDIO_1.8V",
	.microvolts	= 1800000,
	.gpio			= AUDIO_POWER,
	.enable_high = 1,
	.init_data	= &m9w_ldo9_fixed_data,
};

static struct platform_device m9w_fixed_voltage9 = {
	.name	= "reg-fixed-voltage",
	.id		= 8,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo9_config,
	},
};

/**/
static struct regulator_init_data m9w_ldo10_fixed_data = {
	.constraints	= {
		.boot_on = 1,
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled = 1,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(m9w_ldo10_consumer),
	.consumer_supplies	= m9w_ldo10_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo10_config = {
	.supply_name	= "TOUCH_3.3V",
	.microvolts	= 3300000,
	.gpio			= TOUCH_POWER,
	.enable_high = 1,
	.init_data	= &m9w_ldo10_fixed_data,
};

static struct platform_device m9w_fixed_voltage10 = {
	.name	= "reg-fixed-voltage",
	.id		= 9,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo10_config,
	},
};

/**/
static struct regulator_init_data m9w_ldo11_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(m9w_ldo11_consumer),
	.consumer_supplies	= m9w_ldo11_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo11_config = {
	.supply_name	= "GPS_1.8V",
	.microvolts	= 1800000,
	.gpio			= GPS_POWER,
	.enable_high = 1,
	.init_data	= &m9w_ldo11_fixed_data,
};

static struct platform_device m9w_fixed_voltage11 = {
	.name	= "reg-fixed-voltage",
	.id		= 10,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo11_config,
	},
};

/**/
static struct regulator_init_data m9w_ldo12_fixed_data = {
	.constraints	= {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(m9w_ldo12_consumer),
	.consumer_supplies	= m9w_ldo12_consumer,
};

static struct fixed_voltage_config m9w_fixed_ldo12_config = {
	.supply_name	= "COMPASS_1.8V",
	.microvolts	= 1800000,
	.gpio			= COMPASS_POWER,
	.enable_high = 1,
	.init_data	= &m9w_ldo12_fixed_data,
};

static struct platform_device m9w_fixed_voltage12 = {
	.name	= "reg-fixed-voltage",
	.id		= 11,
	.dev		= {
		.platform_data	= &m9w_fixed_ldo12_config,
	},
};

/**/
static struct regulator_init_data m9w_buck1_fixed_data = {
	.constraints	= {
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},	
	},
};

static struct fixed_voltage_config m9w_fixed_buck1_config = {
	.supply_name	= "SYSTEM_3.0V",
	.microvolts	= 3000000,
	.gpio		= -EINVAL,
	.init_data	= &m9w_buck1_fixed_data,
};

static struct platform_device m9w_fixed_buck1 = {
	.name	= "reg-fixed-voltage",
	.id		= 12,
	.dev		= {
		.platform_data	= &m9w_fixed_buck1_config,
	},
};

/**/
static struct regulator_init_data m9w_buck2_fixed_data = {
	.constraints	= {
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},	
	},
};

static struct fixed_voltage_config m9w_fixed_buck2_config = {
	.supply_name	= "SYSTEM_1.8V",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &m9w_buck2_fixed_data,
};

static struct platform_device m9w_fixed_buck2 = {
	.name	= "reg-fixed-voltage",
	.id		= 13,
	.dev		= {
		.platform_data	= &m9w_fixed_buck2_config,
	},
};
#endif

#ifdef CONFIG_VIDEO_RJ64SC110
/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * This function also called at fimc_init_camera()
 * Do optimization for cameras on your platform.
*/
static int m9w_mipi_cam_power(int onoff)
{	
	printk("%s, sharp camera power %s.\n\n",__FUNCTION__, onoff ? "on" : "off");

	if (onoff) {
		s3c_gpio_setpin(CAMERA_PDN_PIN, 0);
		s3c_gpio_cfgpin(CAMERA_PDN_PIN, S3C_GPIO_OUTPUT);

		s3c_gpio_setpin(CAMERA_RESET_PIN, 0);
		s3c_gpio_cfgpin(CAMERA_RESET_PIN, S3C_GPIO_OUTPUT);

		s3c_gpio_setpin(CAMERA_POWER_PIN, 1);
		s3c_gpio_cfgpin(CAMERA_POWER_PIN, S3C_GPIO_OUTPUT);
		msleep(5);

		s3c_gpio_setpin(CAMERA_PDN_PIN, 1);
		mdelay(1);
		s3c_gpio_setpin(CAMERA_RESET_PIN, 1);
		msleep(10);
	} else {
		s3c_gpio_setpin(CAMERA_PDN_PIN, 0);
		s3c_gpio_cfgpin(CAMERA_PDN_PIN, S3C_GPIO_OUTPUT);

		s3c_gpio_setpin(CAMERA_RESET_PIN, 0);
		s3c_gpio_cfgpin(CAMERA_RESET_PIN, S3C_GPIO_OUTPUT);

		s3c_gpio_setpin(CAMERA_POWER_PIN, 0);
		s3c_gpio_cfgpin(CAMERA_POWER_PIN, S3C_GPIO_OUTPUT);
	}

	return 0;
}

static struct rj64sc110_platform_data  rj64sc110_plat  = {
	.default_width 	= 640,
	.default_height 	= 480,
	.pixelformat 		= V4L2_PIX_FMT_UYVY,
	.freq 			= 27000000,
	.is_mipi 			= 1,
};

static struct i2c_board_info  rj64sc110_i2c_info  = {
	I2C_BOARD_INFO("RJ64SC110", 0x60),
	.platform_data = &rj64sc110_plat,
	.irq = CAMERA_INT_PIN,
};

static struct s3c_platform_camera rj64sc110 = {
	.id			= CAMERA_CSI_C,
	.type		= CAM_TYPE_MIPI,
	.fmt			= MIPI_CSI_YCBCR422_8BIT,
	.order422		= CAM_ORDER422_8BIT_YCBYCR,
	.i2c_busnum	= 2,
	.info			= &rj64sc110_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "sclk_vpll",
	.clk_name	= "sclk_cam",
	.clk_rate		= 27000000,
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left		= 0,
		.top		= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk		= 0,
	.inv_vsync 	= 0,
	.inv_href		= 0,
	.inv_hsync	= 0,

	.initialized 	= 0,
	.cam_power	= m9w_mipi_cam_power,
};
#endif

#ifdef CONFIG_VIDEO_FIMC
static struct s3c_platform_fimc fimc0_plat_m9w __initdata = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_fimc",
	.lclk_name	= "fimc",
	.clk_rate		= 166750000,
	.default_cam	= CAMERA_CSI_C,
	.hw_ver	= 0x45,
	.camera		= {
#ifdef CONFIG_VIDEO_RJ64SC110
		&rj64sc110,
#endif
	}, 	
};

static struct s3c_platform_fimc fimc1_plat_m9w __initdata = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_fimc",
	.lclk_name	= "fimc",
	.clk_rate		= 166750000,	
	.default_cam	= CAMERA_PAR_A,
	.hw_ver	= 0x50,
};

static struct s3c_platform_fimc fimc2_plat_m9w __initdata = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_fimc",
	.lclk_name	= "fimc",
	.clk_rate		= 166750000,	
	.default_cam	= CAMERA_PAR_A,
	.hw_ver	= 0x50,
};
#endif

#ifdef CONFIG_USB_GADGET
static int m9w_usb_attach(bool enable)
{
	struct usb_gadget *gadget =
		platform_get_drvdata(&s3c_device_usbgadget);
	int ret = -1;

	if (gadget) {
		if (enable)
			ret = usb_gadget_vbus_connect(gadget);
		else
			ret = usb_gadget_vbus_disconnect(gadget);
	}
	return ret;
}
#endif

static struct ltc3577_charger_data m9w_charger = {
	.usb_int = IRQ_EINT(0),
	.charger_int = IRQ_EINT(6),
	.low_bat_int = IRQ_EINT(4),
#ifdef CONFIG_USB_GADGET
	.usb_attach = m9w_usb_attach,
#endif
};

static struct ltc3577_platform_data ltc3755_pdata = {
	.num_regulators = ARRAY_SIZE(m9w_regulators),
	.regulators     = m9w_regulators,
	.charger		= &m9w_charger,
};

static struct s3cfb_lcd ls035b3sx01 = {
	.width = 640,
	.height = 960,
	.p_width = 50,
	.p_height = 75,
	.bpp = 24,
	.freq = 56,

	.timing = {
		.h_fp = 18,
		.h_bp = 4,
		.h_sw = 2,
		.v_fp = 6,
		.v_fpe = 1,
		.v_bp = 4,
		.v_bpe = 1,
		.v_sw = 2,
		.cmd_allow_len = 2,
	},
	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 0,
		.inv_vsync = 0,
		.inv_vden = 0,
	},
};

static void ls035b3sx01_cfg_gpio(struct platform_device *pdev)
{
	/* mDNIe SEL: why we shall write 0x2 ? */
#ifdef CONFIG_FB_S3C_MDNIE
	writel(0x1, S5P_MDNIE_SEL);
#else
	writel(0x2, S5P_MDNIE_SEL);
#endif
}

static int ls035b3sx01_backlight_off(struct platform_device *pdev, int onoff)
{
	return m9w_bl_on(!!onoff);
}

static struct s3c_platform_fb m9w_lcd __initdata = {
	.hw_ver		= 0x62,
	.clk_name		= "sclk_fimd",
	.nr_wins		= 5,
	.default_win	= CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap		= FB_SWAP_HWORD | FB_SWAP_WORD,
	.lcd 			= &ls035b3sx01,
	.cfg_gpio		= ls035b3sx01_cfg_gpio,
	.backlight_onoff    = ls035b3sx01_backlight_off,
};

static struct dsim_config m9w_dsim_info __initdata = {
	.lcd_panel_info = &ls035b3sx01,
};

static struct s5p_platform_dsim m9w_dsi_pd __initdata = {
	.lcd_panel_name = "m9w-ls035b3sx01",
	.dsim_info = &m9w_dsim_info,
};

#ifdef CONFIG_VIDEO_JPEG_V2
static struct s3c_platform_jpeg jpeg_plat __initdata = {
	.max_main_width	= 2592,
	.max_main_height	= 1944,
	.max_thumb_width	= 0,//320,
	.max_thumb_height= 0,//480,
};
#endif

#ifdef CONFIG_SPI_XGOLD_M9W
static int mdm_cfg_gpio(struct m9w_spi_info* info)
{
	return 0;
}

static int mdm_reset(struct m9w_spi_info* info, int normal)
{   
	s3c_gpio_setpin(info->reset_gpio, 0);
	msleep(100);
	if (normal) {
		s3c_gpio_setpin(info->reset_gpio, 1);
		msleep(10);
	}
	return 0;
}

static int mdm_power(struct m9w_spi_info* info, int onoff)
{
	/*modem power up*/
	return onoff ? 0 : s3c_gpio_setpin(info->reset_gpio, 0);
}

static struct m9w_spi_info default_mdm_data __initdata = {
    .srdy_gpio = X_SRDY_GPIO,
    .mrdy_gpio = X_MRDY_GPIO,
    .reset_gpio = X_RESET_GPIO, //reset low, nomal input ,
    .power_gpio = X_POWER_GPIO,
    .bb_reset_gpio = X_BB_RST,	//-EINVAL
    .cfg_gpio = mdm_cfg_gpio,
    .reset_modem = mdm_reset,
    .power_modem = mdm_power,
};
#endif

#ifdef CONFIG_SPI
static struct s3c64xx_spi_csinfo m9w_spi0_csi[] = {
	[0] = {
		.line = S5PV210_GPB(1),
		.set_level = gpio_set_value,
		.fb_delay = 0x0,
	},
};

struct spi_board_info m9w_spi_devs[] __initdata = {
#ifdef CONFIG_SPI_XGOLD_M9W
	[0] = {
	    .modalias        		= "m9w-spi", /* Test Interface */
	    .mode            		= SPI_MODE_1,  /* CPOL=0, CPHA=1 */
	    .max_speed_hz    	= 2400000,
	    /* Connected to SPI-0 as 1st Slave */
	    .bus_num          	= 0,
	    .chip_select		= 0,
	    .controller_data 	= &m9w_spi0_csi[0],
	    .platform_data   	= &default_mdm_data,
	},
#endif
};
#endif

/*i2c3 for battery*/
static struct i2c_gpio_platform_data	i2c3_platdata = {
	.sda_pin			= GPIO_I2C3_SDA,
	.scl_pin			= GPIO_I2C3_SCL,
	.udelay			= 2,	/* freq: 100KHz - udelay*10KHZ = ~80KHZ*/
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only= 0,
};

static struct platform_device s3c_device_i2c3 = {
	.name			= "i2c-gpio",
	.id				= 3,
	.dev.platform_data	= &i2c3_platdata,
};

/*i2c4 for acc*/
static struct i2c_gpio_platform_data	i2c4_platdata = {
	.sda_pin			= GPIO_I2C4_SDA,
	.scl_pin			= GPIO_I2C4_SCL,
	.udelay			= 2,	/* freq: 100KHz - udelay*10KHZ = ~80KHZ*/
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only= 0,
};

static struct platform_device s3c_device_i2c4 = {
	.name			= "i2c-gpio",
	.id				= 4,
	.dev.platform_data	= &i2c4_platdata,
};

/*i2c5 for compass*/
static struct i2c_gpio_platform_data	i2c5_platdata = {
	.sda_pin			= GPIO_I2C5_SDA,
	.scl_pin			= GPIO_I2C5_SCL,
	.udelay			= 2,	/* freq: 100KHz - udelay*10KHZ = ~80KHZ*/
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only= 0,
};

static struct platform_device s3c_device_i2c5 = {
	.name			= "i2c-gpio",
	.id				= 5,
	.dev.platform_data	= &i2c5_platdata,
};

/*i2c6 for ltc3577*/
static struct i2c_gpio_platform_data	i2c6_platdata = {
	.sda_pin			= GPIO_I2C6_SDA,
	.scl_pin			= GPIO_I2C6_SCL,
	.udelay			= 2,	/* freq: 100KHz - udelay*10KHZ = ~80KHZ*/
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only= 0,
};

static struct platform_device s3c_device_i2c6 = {
	.name			= "i2c-gpio",
	.id				= 6,
	.dev.platform_data	= &i2c6_platdata,
};

/*i2c7 for IR*/
static struct i2c_gpio_platform_data	i2c7_platdata = {
	.sda_pin			= GPIO_I2C7_SDA,
	.scl_pin			= GPIO_I2C7_SCL,
	.udelay			= 2,	/* freq: 100KHz - udelay*10KHZ = ~80KHZ*/
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only= 0,
};

static struct platform_device s3c_device_i2c7 = {
	.name			= "i2c-gpio",
	.id				= 7,
	.dev.platform_data	= &i2c7_platdata,
};

#ifdef CONFIG_VIDEO_G2D
static struct fimg2d_platdata fimg2d_data __initdata = {
	.hw_ver = 30,
	.parent_clkname = "mout_mpll",
	.clkname = "sclk_g2d",
	.gate_clkname = "clk_g2d",
	.clkrate = 250 * 1000000,
};
#endif

#ifdef CONFIG_M9W_GPS_CONTROL
struct platform_device m9w_gps = {
	.name		= "m9-ublox-gps",
	.id		= -1,	
};
#endif

#ifdef CONFIG_BT
struct platform_device m9w_bt_ctr = {
	.name		= "bt_ctr",
	.id		= -1,	
};
#endif

#ifdef CONFIG_BCM4329

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER 24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wifi_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

extern int dhd_card_insert(void);
extern void dhd_card_remove(void);
extern void need_test_firmware(void);
extern void reset_firmware_type(void);

ssize_t wifi_card_state_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	int wl_on ;

	wl_on =  gpio_get_value(WL_RESET);
	return sprintf(buf, "%d\n",  wl_on);
}


ssize_t wifi_card_state_store(struct device *dev,
									struct device_attribute *attr,	const char *buf, size_t count)
{	
	unsigned long value = simple_strtoul(buf, NULL, 10);
	int wl_inserded;
	int wl_on;
	int ret = count;

	wl_inserded = !gpio_get_value(WL_CD_PIN);
	wl_on = gpio_get_value(WL_RESET);

	printk("wl_on %d, wl_inserded %d\n", wl_on, wl_inserded);
	if(value) {
		if(!wl_inserded && !wl_on){
			if(value == 2)
				need_test_firmware();
			dhd_card_insert();
		} else {
			printk("wifi card already insert\n");
			ret = -EBUSY;
		}
	} else {
		if(wl_inserded && wl_on) {
			dhd_card_remove();
			reset_firmware_type();
		} else {
			printk("wifi card already remove\n");
			ret = -EBUSY;
		}
	}

	return ret;
}

extern void wifi_card_set_power(unsigned int power_mode);

static int wlan_power_en(int onoff)
{
	if (gpio_get_value(WL_CD_PIN)) {
		WARN(1, "WL_WIFICS is HI\n");
	} else {
		//msleep(2 * 1000);
		/* must be mmc card detected pin low */
		if (onoff) {
			wifi_card_set_power(1);
			msleep(200);
		} else {
			wifi_card_set_power(0);
		}
	}
	return 0;
}

//set reset pin 
static int wlan_reset_en(int onoff)
{	
	gpio_set_value(WL_RESET, onoff ? 1 : 0);
	return 0;
}

static int wlan_carddetect(int  onoff )
{
	if(onoff) {
		s5pv210_setup_sdhci1_cfg_gpio(NULL, 4);
	} else {
		s5pv210_setup_sdhci1_cfg_gpio(NULL, 0);
	}
	msleep(10);
	gpio_set_value(WL_CD_PIN, !onoff);
	msleep(400);
	return 0;
}

static struct wifi_mem_prealloc wifi_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},   
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static __maybe_unused void * wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wifi_mem_array[section].size < size)
		return NULL;

	return wifi_mem_array[section].mem_ptr;
}

int __init m9w_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0 ; i < WLAN_SKB_BUF_NUM ; i++) {
		wlan_static_skb[i] = dev_alloc_skb(
				((i < (WLAN_SKB_BUF_NUM / 2)) ? 4096 : 8192));   

		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wifi_mem_array[i].mem_ptr =
				kmalloc(wifi_mem_array[i].size, GFP_KERNEL);

		if (!wifi_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wifi_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}

static struct wifi_platform_data wifi_pdata = {	
	.set_power		= wlan_power_en,
	.set_reset		= wlan_reset_en,
	.set_carddetect  	= wlan_carddetect,
//	.mem_prealloc		= wlan_mem_prealloc,
	
};

 static struct resource wifi_resources[] = {
	 [0] = {
		 .name	 	= "bcm4329_wlan_irq",
		 .start  		= IRQ_EINT(9),
		 .end		= IRQ_EINT(9),
		 .flags  		= IORESOURCE_IRQ | IRQF_TRIGGER_HIGH| IRQF_ONESHOT,  /*wlan host wake : high*/
	 },
 };
 
static struct platform_device  m9w_wifi = {
	.name			= "bcm4329_wlan",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(wifi_resources),
	.resource			= wifi_resources,
	.dev				= {
		.platform_data = &wifi_pdata,
	},
};
#endif

static struct platform_device *m9w_devices[] __initdata = {
	&watchdog_device,
	&s5p_device_onenand,
#ifdef CONFIG_FIQ_DEBUGGER
	&s5pv210_device_fiqdbg_uart2,
#endif

#ifdef CONFIG_RTC_DRV_S3C
	&s5p_device_rtc,
#endif

#ifdef CONFIG_SND_S5PC1XX_I2S
	&s5pv210_device_iis0,
#endif

#ifdef CONFIG_S3C2410_WATCHDOG
	&s3c_device_wdt,
#endif

#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif

#ifdef CONFIG_VIDEO_MFC50
	&s3c_device_mfc,
#endif

#ifdef CONFIG_S5P_ADC
	&s3c_device_adc,
#endif

#ifdef CONFIG_VIDEO_FIMC
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
#endif
#ifdef CONFIG_VIDEO_FIMC_MIPI
	&s5p_device_mipi_csis0,
#endif
#ifdef CONFIG_VIDEO_JPEG_V2
	&s3c_device_jpeg,
#endif

#ifdef CONFIG_PVR_SGX
	&s3c_device_g3d,
	&s3c_device_lcd,
#endif

	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_i2c2,
	&s3c_device_i2c3,
	&s3c_device_i2c4,
	&s3c_device_i2c5,
	&s3c_device_i2c6,
	&s3c_device_i2c7,

#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif

#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&s3c_device_rndis,
#endif
#endif

#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif

#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif

#ifdef CONFIG_S5PV210_POWER_DOMAIN
	&s5pv210_pd_audio,
	&s5pv210_pd_cam,
	&s5pv210_pd_tv,
	&s5pv210_pd_lcd,
	&s5pv210_pd_g3d,
	&s5pv210_pd_mfc,
#endif

#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
	&pmem_gpu1_device,
	&pmem_adsp_device,
#endif

#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[2],
#endif

#ifdef CONFIG_S5P_MIPI_DSI
	&s5p_device_dsim,
#endif

#ifdef CONFIG_MEIZU_M9W_KEYBOARD
	&m9w_keyboard,
#endif

#ifdef CONFIG_SWITCH_GPIO
	&m9w_switch_gpio,
#endif

#ifdef CONFIG_M9W_GPS_CONTROL
	&m9w_gps,
#endif
	&s5p_device_fimg2d,

	&s5pv210_device_spi0,

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&ram_console_device,
#endif

#ifdef CONFIG_BT
	&m9w_bt_ctr, 
#endif

#ifdef CONFIG_BCM4329
	&m9w_wifi, 
#endif

#if defined(CONFIG_ANDROID_TIMED_GPIO)
	&m9w_timed_gpios,
#endif

#ifdef CONFIG_LEDS_M9W
	&m9w_led_key,
#endif

#ifdef CONFIG_REGULATOR_FIXED_VOLTAGE
	&m9w_fixed_voltage1,
	&m9w_fixed_voltage2,
	&m9w_fixed_voltage3,
	&m9w_fixed_voltage4,
	&m9w_fixed_voltage5,
	&m9w_fixed_voltage6,
	&m9w_fixed_voltage7,
	&m9w_fixed_voltage8,
	&m9w_fixed_voltage9,
	&m9w_fixed_voltage10,
	&m9w_fixed_voltage11,
	&m9w_fixed_voltage12,
	&m9w_fixed_buck1,
	&m9w_fixed_buck2,
#endif
	&m9w_codec_dev,
	&samsung_asoc_dma,
#ifdef CONFIG_CPU_FREQ
	&s5pv210_device_cpufreq,
#endif


};

static void m9w_power_off (void)
{
	/* reset modem -> Output Low -> power off */
	s3c_gpio_cfgpin(X_BB_RST, S3C_GPIO_INPUT);
	s3c_gpio_setpin(X_RESET_GPIO, 0);
	msleep(1000);
	if (gpio_get_value(USB_INT)) {
		/*hold ltc3577 power pin low*/
		s3c_gpio_setpin(PWON, 0);
	} else {
		__raw_writel(1, S5P_SWRESET);
	}
	mdelay(10);
	printk(KERN_EMERG "%s: should not reach here!!\n", __func__);
	while (1);
}

static void __init m9w_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s5pv210_gpiolib_init();
	s3c24xx_init_uarts(m9w_uartcfgs, ARRAY_SIZE(m9w_uartcfgs));
	/* got to have a high enough uart source clock for higher speeds, setting 133MHZ */
	writel((readl(S5P_CLK_DIV4) & ~(0xffff0000)) | 0x44440000, S5P_CLK_DIV4);
	s5p_reserve_bootmem(m9w_media_devs, ARRAY_SIZE(m9w_media_devs),S5P_RANGE_MFC);
#ifdef CONFIG_MTD_ONENAND
	s5p_device_onenand.name = "s5pc110-onenand";
#endif
}

static struct i2c_board_info i2c_devs0[] __initdata = {
	{ I2C_BOARD_INFO("tlv320aic36", 0x1b),},
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{I2C_BOARD_INFO("m9w-rmi-ts",0x20),
	.irq = IRQ_EINT(7),},
};

static struct i2c_board_info i2c_devs2[] __initdata = {
//	{ I2C_BOARD_INFO("RJ64SC110", 0x60),
};

static struct i2c_board_info i2c_devs3[] __initdata = {
	{ I2C_BOARD_INFO("bq27541", 0x55),},
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{ I2C_BOARD_INFO("st-lis3dh", 0x19),},  			/*lis3dh*/
};

static struct i2c_board_info i2c_devs5[] __initdata = {
	{ I2C_BOARD_INFO("akm8975", 0x0c),}, 			/*akm8975*/
};

static struct i2c_board_info i2c_devs6[] __initdata = {
	{ I2C_BOARD_INFO("ltc3577-3", 0x09),
	.platform_data = &ltc3755_pdata,},
};

static struct i2c_board_info i2c_devs7[] __initdata = {
	{ I2C_BOARD_INFO("sharp-gp2ap012a00f", 0x44), },	/*sharp proximity and ambient light sensor*/
};

/*
 * for S5PC110 of D-type, OneDram need handle for some signal
 */
#ifdef CONFIG_S5PC110_MEM_DMC0_ONEDRAM
static __init int m9w_pm_prepare(void)
{
	unsigned long CLKE_level, CS_level;
	unsigned char i;
		
	s3c_gpio_setpull(ONEDRAM_CLK, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(ONEDRAM_CS, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(ONEDRAM_CLKE, S3C_GPIO_PULL_NONE);

	s3c_gpio_setpin(ONEDRAM_CLKE, 1);
	s3c_gpio_cfgpin(ONEDRAM_CLKE, S3C_GPIO_OUTPUT);
	
	s3c_gpio_setpin(ONEDRAM_CS, 1);
	s3c_gpio_cfgpin(ONEDRAM_CS, S3C_GPIO_OUTPUT);

	s3c_gpio_setpin(ONEDRAM_CLK, 0);
	s3c_gpio_cfgpin(ONEDRAM_CLK, S3C_GPIO_OUTPUT);

	ndelay(2);
	CS_level = 0xfdf;
	CLKE_level = 0xfc0;

	for(i=0; i<12; i++){
		s3c_gpio_setpin(ONEDRAM_CLKE, !!((CLKE_level<<i) & 0x800));
		s3c_gpio_setpin(ONEDRAM_CS, !!((CS_level<<i) & 0x800));
		ndelay(2);
		s3c_gpio_setpin(ONEDRAM_CLK, 1);
		ndelay(2);
		s3c_gpio_setpin(ONEDRAM_CLK, 0);
		ndelay(2);
	}

	s3c_gpio_setpull(ONEDRAM_CLK, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(ONEDRAM_CS, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(ONEDRAM_CLKE, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(ONEDRAM_CLK, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(ONEDRAM_CS, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(ONEDRAM_CLKE, S3C_GPIO_INPUT);

	return 0;
}
#endif

static void __init m9w_machine_init(void)
{
//	s3c_usb_set_serial();
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	setup_ram_console_mem();
#endif
	platform_add_devices(m9w_devices, ARRAY_SIZE(m9w_devices));

	/* i2c */
	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	s3c_i2c2_set_platdata(NULL);

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
	i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));

	/* to support system shut down */
	pm_power_off = m9w_power_off;

#ifdef CONFIG_PM_SLEEP
	s5p_config_sleep_gpio_table = m9w_config_sleep_gpio;
#endif

	/* spi */
#ifdef CONFIG_SPI
	s5pv210_spi_set_info(0, S5PV210_SPI_SRCCLK_SCLK, ARRAY_SIZE(m9w_spi0_csi));
	spi_register_board_info(m9w_spi_devs, ARRAY_SIZE(m9w_spi_devs));
#endif

#if defined(CONFIG_FB_S3C)
	s3cfb_set_platdata(&m9w_lcd);
#endif

#if defined(CONFIG_S5P_ADC)
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif

#ifdef CONFIG_VIDEO_FIMC
	/* fimc */
	s3c_fimc0_set_platdata(&fimc0_plat_m9w);
	s3c_fimc1_set_platdata(&fimc1_plat_m9w);
	s3c_fimc2_set_platdata(&fimc2_plat_m9w);
	s3c_csis_set_platdata(NULL);
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	s3c_jpeg_set_platdata(&jpeg_plat);
#endif

#ifdef CONFIG_VIDEO_MFC50
	/* mfc */
	s3c_mfc_set_platdata(NULL);
#endif

#ifdef CONFIG_S3C_DEV_HSMMC1
	s5pv210_default_sdhci1();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s5pv210_default_sdhci2();
#endif
#ifdef CONFIG_S5PV210_SETUP_SDHCI
	s3c_sdhci_set_platdata();
#endif

#if defined(CONFIG_PM)
	s3c_pm_init();
#endif

#ifdef CONFIG_VIDEO_G2D
	s5p_fimg2d_set_platdata(&fimg2d_data);
#endif

#if defined(CONFIG_BCM4329)
//	m9w_init_wifi_mem(); 
#endif

#ifdef CONFIG_S5PC110_MEM_DMC0_ONEDRAM
	m9w_pm_prepare();
#endif

#ifdef CONFIG_S5P_MIPI_DSI
	s5p_dsi_set_platdata(&m9w_dsi_pd);
#endif
	regulator_has_full_constraints();
}

unsigned int pm_debug_scratchpad;

static void __init m9w_fixup(struct machine_desc *desc,
		struct tag *tags, char **cmdline,
		struct meminfo *mi)
{
#define BANK0_ADDR 0x30000000
#define BANK1_ADDR 0x40000000
#define BANK2_ADDR 0x50000000 
	mi->bank[0].start = BANK0_ADDR;
	mi->bank[0].size = 128 * SZ_1M;
//	mi->bank[0].node = PHYS_TO_NID(BANK0_ADDR);

	mi->bank[1].start = BANK1_ADDR;
	mi->bank[1].size = 256 * SZ_1M;
//	mi->bank[1].node = PHYS_TO_NID(BANK1_ADDR);

	mi->bank[2].start = BANK2_ADDR;
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	/* 1M for ram_console buffer */
	mi->bank[2].size = 127 * SZ_1M;
#else
	mi->bank[2].size = 128 * SZ_1M;
#endif
//	mi->bank[2].node = PHYS_TO_NID(BANK2_ADDR);
	mi->nr_banks = 3;

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	ram_console_start = mi->bank[2].start + mi->bank[2].size;
	ram_console_size = SZ_128K - SZ_4K;

	pm_debug_scratchpad = ram_console_start + ram_console_size;
#endif
}

MACHINE_START(MEIZU_M9W, "M9")
//MACHINE_START(MEIZU_M9W, "herring")
	/* Maintainer: Lvcha Qiu <lvcha@meizu.com> */
	//.phys_io		= S3C_PA_UART & 0xfff00000,
	//.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.fixup		= m9w_fixup,
	.init_irq		= s5pv210_init_irq,
	.map_io		= m9w_map_io,
	.init_machine	= m9w_machine_init,
#if defined(CONFIG_S5P_HIGH_RES_TIMERS)
	.timer		= &s5p_systimer,
#else
	.timer		= &s3c24xx_timer,
#endif
MACHINE_END

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void)
{
	/* USB PHY0 Enable */
	writel(readl(S5P_USB_PHY_CONTROL) | (0x1<<0),
			S5P_USB_PHY_CONTROL);
	writel((readl(S3C_USBOTG_PHYPWR) & ~(0x3<<3) & ~(0x1<<0)) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	writel((readl(S3C_USBOTG_PHYCLK) & ~(0x5<<2)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	writel((readl(S3C_USBOTG_RSTCON) & ~(0x3<<1)) | (0x1<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);
	writel(readl(S3C_USBOTG_RSTCON) & ~(0x7<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);

	/* rising/falling time */
	writel(readl(S3C_USBOTG_PHYTUNE) | (0x1<<20),
			S3C_USBOTG_PHYTUNE);

	/* set DC level as 6 (6%) */
	writel((readl(S3C_USBOTG_PHYTUNE) & ~(0xf)) | (0x1<<2) | (0x1<<1),
			S3C_USBOTG_PHYTUNE);
}
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
struct usb_ctrlrequest usb_ctrl __attribute__((aligned(64)));

/* OTG PHY Power Off */
void otg_phy_off(void)
{
	writel(readl(S3C_USBOTG_PHYPWR) | (0x3<<3),
			S3C_USBOTG_PHYPWR);
	writel(readl(S5P_USB_PHY_CONTROL) & ~(1<<0),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(otg_phy_off);
#endif

void s3c_setup_uart_cfg_gpio(unsigned char port)
{
	switch(port) {
	case 0:		/* bluetooth at uart0*/
		s3c_gpio_cfgpin(S5PV210_GPA0(0), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(0), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPA0(1), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(1), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPA0(2), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(2), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(3), S3C_GPIO_PULL_NONE);
		break;
	case 1:		/* infineon modem at uart1*/
		s3c_gpio_cfgpin(S5PV210_GPA0(4), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(4), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPA0(5), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(5), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPA0(6), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(6), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPA0(7), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(7), S3C_GPIO_PULL_NONE);
		break;
	case 2:		/*AP debug at uart2*/
		s3c_gpio_cfgpin(S5PV210_GPA1(0), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA1(0), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPA1(1), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA1(1), S3C_GPIO_PULL_NONE);
		break;
	case 3:		/*GPS at uart3*/
		s3c_gpio_cfgpin(S5PV210_GPA1(2), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA1(2), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPA1(3), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA1(3), S3C_GPIO_PULL_NONE);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(s3c_setup_uart_cfg_gpio);
