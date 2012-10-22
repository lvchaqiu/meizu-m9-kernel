/* linux/arch/arm/mach-s5pv210/include/mach/gpio-m9w.h
 *
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: lvcha qiu <lvcha@meizu.com>
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
 * Inital code : Apr 16 , 2010 : lvcha@meizu.com
 *
*/

#ifndef _MEIZUM9_GPIO_H
#define _MEIZUM9_GPIO_H

/*      ARM_ON1   ARM_ON2
 *1.00v   input     input
 *1.05v   input      0
 *1.20v     0       input
 *1.25v     0        0
 */
#define ARM_ON0		S5PV210_GPJ0(5)
#define ARM_ON1		S5PV210_GPJ0(2)
#define ARM_ON2		S5PV210_GPJ0(4)

/*       INT_ON2
 *1.0      input
 *1.1       0
 */
#define INT_ON2		S5PV210_GPJ0(6)

/*
 *拉高使能LTC3577电源输出
 */
#define PWON		S5PV210_GPG3(2)

/*
 *电源开机键
 */
#define KEY_OFF		S5PV210_GPH0(2)

/*
 *检测充电电流的ADC通道
 */
#define CHARG_ADC 	0

/*             LIM0    LIM1
 *500mA      	0       0
 *1000mA     	0       1
 *50uA       	1       0
 *100mA      	1       1
 */
#define LIM0    S5PV210_GPJ0(1)
#define LIM1    S5PV210_GPJ1(0)

/*
 *充电指示,低电平有效
 */
#define CHARG_IND	S5PV210_GPH0(6)

/*
 *电池低电压检测,低电平有效
 */
#define LOW_BT_DETEC	S5PV210_GPH0(4)

/*
 *USB插入检测,低电平有效
 */
#define USB_INT	S5PV210_GPH0(0)

/*
 *使能usb电源,高电平使能
 */
#define USB_OTG_PW	S5PV210_GPG3(4)

typedef enum {
	LED_MOTO,
	LED_KEY,
	LED_MAX_NUM
} m9w_led_num;

/*开发板上的两个GPIO控制指示灯*/
#define LED0    S5PV210_GPH3(3)
#define LED1    S5PV210_GPH3(4)

/*M9W 手机上的按键灯, 马达*/
#define LED_KEY_GPIO	S5PV210_GPD0(2)
#define LED_MOTO_GPIO	S5PV210_GPD0(0)

/*x-gold infineon modem*/
#define X_SRDY_GPIO		S5PV210_GPH1(0)
#define X_MRDY_GPIO	S5PV210_GPC1(4)
#define X_RESET_GPIO	S5PV210_GPJ1(5)
#define X_POWER_GPIO    	S5PV210_GPJ2(0)
#define X_BB_RST		S5PV210_GPH1(6)

/*sdhci1 power pin*/
#define WL_CD_PIN		S5PV210_GPG0(6)
#define WL_RESET    		S5PV210_GPC1(1)
#define WL_POWER     	S5PV210_GPC1(0)
#define WL_WAKE		S5PV210_GPC1(2)
#define WL_HOST_WAKE	S5PV210_GPH1(1)

/*sdhci2 power pin*/
#define SDHCI2_POWER    S5PV210_GPJ2(1)	/* SD卡电源*/

/* BT control pin*/
#define BT_POWER		S5PV210_GPF2(5)
#define BT_RESET			S5PV210_GPF1(1)
#define BT_WAKE			S5PV210_GPF1(0)
#define BT_HOST_WAKE	S5PV210_GPH1(3)
#define WIFI_BT_TEST_LED    S5PV210_GPH2(4)

/*按键定义*/
#define GPIO_MEIZU_KEY_HOME		S5PV210_GPH0(3) 
#define GPIO_MEIZU_KEY_VOL_UP 		S5PV210_GPH0(1)
#define GPIO_MEIZU_KEY_VOL_DOWN	S5PV210_GPH0(5) 		
#define GPIO_MEIZU_KEY_POWER	    	S5PV210_GPH0(2)
#define GPIO_MEIZU_KEY_EAR            	S5PV210_GPH3(6)

/*MIPI DSIM power pin*/
#define MIPI_DSIM_POWER	S5PV210_GPG3(1)

/*LCD control pin*/
#define LCD_RESET_GPIO		S5PV210_GPF0(1)
#define LCD_POWER_GPIO		S5PV210_GPJ3(6)

/*audio codec power pin*/
#define AUDIO_POWER	S5PV210_GPJ1(3)

/*audio codec reset pin*/
#define AUDIO_RESET		S5PV210_GPC1(3)

/*audio speaker amplified EN pin*/
#define SPK_EN_PIN		S5PV210_GPJ1(2)

/*touch panel power pin*/
#define TOUCH_POWER	S5PV210_GPG3(3)

/*touch pannel interrupted pin*/
#define TOUCH_PANNEL_INT		S5PV210_GPH0(7)

/*accelerator lis302dl interrupted pin*/
#define INT1_PIN		S5PV210_GPH1(7)

/*proximity interrupted pin*/
#define PROX_INT  S5PV210_GPH3(7)

#define ONEDRAM_CLK	S5PV210_GPG0(0)
#define ONEDRAM_CS		S5PV210_GPG0(1)
#define ONEDRAM_CLKE	S5PV210_GPG0(2)

/*camera reset pin*/
#define CAMERA_RESET_PIN		S5PV210_GPE1(4)
/*camera power down pin*/
#define CAMERA_PDN_PIN			S5PV210_GPE0(0)
/*camera power pin*/
#define CAMERA_POWER_PIN		S5PV210_GPJ1(1)

/*camera ready pin*/
#define CAMERA_INT_PIN			S5PV210_GPH3(2)

/*GPS*/
#define GPS_POWER	S5PV210_GPG3(6)

/*COMPASS POWER*/
#define COMPASS_POWER	S5PV210_GPJ0(7)

/*gpio i2c pin*/
/*i2c for battery*/
#define GPIO_I2C3_SCL	S5PV210_GPH2(2)
#define GPIO_I2C3_SDA	S5PV210_GPH2(3)

/*i2c for acc*/
#define GPIO_I2C4_SCL	S5PV210_GPF2(4)
#define GPIO_I2C4_SDA	S5PV210_GPF2(1)

/*i2c for compass*/
#define GPIO_I2C5_SCL	S5PV210_GPH2(1)
#define GPIO_I2C5_SDA	S5PV210_GPH2(0)

/*i2c for ltc3577*/
#define GPIO_I2C6_SCL	S5PV210_GPH3(0)
#define GPIO_I2C6_SDA	S5PV210_GPH2(5)

/*i2c for IR*/
#define GPIO_I2C7_SCL	S5PV210_GPH2(6)
#define GPIO_I2C7_SDA	S5PV210_GPH3(4)

/* pcb version detected pin
 *  pin2	    pin4		version
 * 	1		1		   0
 *	1	 	0		   1
 *    0     	1		   2
 *    0      	0		   3
 */
#define GPIO_VERSION_PIN1	S5PV210_GPF1(2)
#define GPIO_VERSION_PIN2	S5PV210_GPF1(4)	

#endif	/*!_MEIZUM9_GPIO_H*/

