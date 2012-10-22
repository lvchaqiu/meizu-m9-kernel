/*
 * ltc3577-private.h - Voltage regulator driver for the ltc3577-3
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

#ifndef __LINUX_MFD_LTC3577_PRIV_H
#define __LINUX_MFD_LTC3577_PRIV_H

/**
 * struct ltc3577_dev - ltc3577 master device for sub-drivers
 * @dev: master device of the chip (can be used to access platform data)
 * @i2c_client: i2c client private data
 * @dev_read():	chip register read function
 * @dev_write(): chip register write function
 * @dev_update(): chip register update function
 * @iolock: mutex for serializing io access
 */

struct ltc3577_dev {
	struct device *dev;
	struct i2c_client *i2c_client;
	int (*dev_read)(struct ltc3577_dev *ltc3577, u8 reg);
	int (*dev_write)(struct ltc3577_dev *ltc3577, u8 reg, u8 val);
	int (*dev_update)(struct ltc3577_dev *ltc3577, u8 reg, u8 val, u8 mask);
	struct mutex iolock;
	char data[4];
	char status;
};

static inline int ltc3577_read_reg(struct ltc3577_dev *ltc3577, u8 reg)
{
	return ltc3577->dev_read(ltc3577, reg);
}

static inline int ltc3577_write_reg(struct ltc3577_dev *ltc3577, u8 reg,
				    u8 value)
{
	return ltc3577->dev_write(ltc3577, reg, value);
}

static inline int ltc3577_update_reg(struct ltc3577_dev *ltc3577, u8 reg,
				     u8 value, u8 mask)
{
	return ltc3577->dev_update(ltc3577, reg, value, mask);
}

/**************************************************************************************/
/*
Table 8. Buck Control Register
BUCK CONTROL REGISTER
ADDRESS: 00010010
SUB-ADDRESS: 00000000
BIT 	NAME 		FUNCTION
B0 	N/A			Not Used！No Effect on Operation
B1 	N/A 			Not Used！No Effect on Operation
B2 	BK1BRST 		Buck1 Burst Mode Enable
B3 	BK2BRST 		Buck2 Burst Mode Enable
B4 	BK3BRST 		Buck2 Burst Mode Enable
B5 	SLEWCTL1	Buck SW Slew Rate: 00 = 1ns, 
B6 	SLEWCTL2 	01 = 2ns, 10 = 4ns, 11 = 8ns
B7 	N/A 			Not Used！No Effect on Operation
*/
#define  BUCK_CONTROL_REGISTER_ADDR 0X00 
#define  BUCK_CONTROL_ENABLE_VAL 0x3c 
#define  BUCK_CONTROL_DISABLE_VAL 0x00

/*
Table 9. I2C LED Control Register
LED CONTROL REGISTER
ADDRESS: 00010010
SUB-ADDRESS: 00000001
BIT 	NAME 	FUNCTION
B0 	EN 		Enable: 1 = Enable 0 = Off
B1 	GR2		Gradation GR[2:1]: 00 = 15ms, 01 = 460ms,
B2 	GR1		10 = 930ms,11 = 1.85 Seconds
B3 	MD1 	Mode MD[2:1]: 00 = CC Boost,
B4 	MD2		10 = PWM Boost; 01 = HV Boost,
B5 	PWMC1 	PWM CLK PWMC[2:1]: 00 = 8.77kHz,
B6 	PWMC2	01 = 4.39kHz, 10 = 2.92kHz, 11 = 2.19kHz
B7 	SLEWLED LED SW Slew Rate: 0/1 = Fast/Slow
*/
#define  LED_CONTROL_REGISTER_ADDR 0X01
#define  LED_CONTROL_REGISTER_VAL_SLOW 0x03
#define  LED_CONTROL_REGISTER_VAL_FAST 0x05

/*
Table 10. I2C LED DAC Register
LED DAC REGISTER
ADDRESS: 00010010
SUB-ADDRESS: 00000010
BIT 	NAME 	FUNCTION
B0 	DAC[0] 	6-Bit Log DAC Code
B1 	DAC[1]
B2 	DAC[2]
B3 	DAC[3]
B4 	DAC[4]
B5 	DAC[5]
B6 	N/A 		Not Used！No Effect On Operation
B7 	N/A 		Not Used！No Effect On Operation
*/
#define LED_DAC_REGISTER_ADDR 0x02
#define LED_DAC_REGISTER_VAL 0x00

/*
Table 11. LED PWM Register
LED PWM REGISTER
ADDRESS: 00010010
SUB-ADDRESS: 00000011
BIT 	NAME 		FUNCTION
B0 	PWMDEN[0] 	PWM DENOMINATOR
B1 	PWMDEN[1]
B2 	PWMDEN[2]
B3 	PWMDEN[3]
B4 	PWMNUM[0] 	PWM NUMERATOR
B5 	PWMNUM[1]
B6 	PWMNUM[2]
B7 	PWMNUM[3]

Duty Cycle = PWMNUM / PWMDEN
Frequency  = PWMCLK / PWMDEN
=
*/
#define LED_PWM_REGISTER_ADDR   0X03
#define LED_PWM_REGISTER_VAL	0xff

/*
Table 12. I2C READ Register
STATUS REGISTER
ADDRESS: 00010011
SUB-ADDRESS: None
BIT 	NAME 		FUNCTION
A0 	CHARGE 		Charge Status (1 = Charging)
A1 	STAT[0] 		STAT[1:0]; 00 = No Fault
A2 	STAT[1]		01 = TOO COLD/HOT
				10 = BATTERY OVERTEMP 
				11 = BATTERY FAULT
A3 	PGLDO[1] 	LDO1 Power Good
A4 	PGLDO[2] 	LDO2 Power Good
A5 	PGBCK[1] 	Buck1 Power Good
A6 	PGBCK[2] 	Buck2 Power Good
A7 	PGBCK[3] 	Buck3 Power Good
*/
#define LTC3577_CHARGING		(0x1 << 0)
#define LTC3577_BAT_FAULT		(0x3 << 1)
#define LTC3577_BAT_OVERTEMP	(0x2 << 1)
#define LTC3577_BAT_MASK		(0x3 << 1)
#define LTC3577_POWER_GOOD	0xf8

#endif /* __LINUX_MFD_LTC3577_PRIV_H */
