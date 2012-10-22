/*
 * linux/drivers/power/meizum9_bq27541.h
 *
 * Battery measurement code for M9 platform.
 *
 * Copyright (C) 2010 MEIZU Inc..
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _MEIZUM9_BQ27541_H_
#define _MEIZUM9_BQ27541_H_

#define	IIC_ADDR	(0x55)
#define	BQ27541_NAME	"bq27541"
#define	BQ27541_ID 	0x0541

#define bq27541CMD_CNTL_LSB  0x00
#define bq27541CMD_CNTL_MSB  0x01
#define bq27541CMD_AR_LSB    0x02
#define bq27541CMD_AR_MSB    0x03
#define bq27541CMD_ARTTE_LSB 0x04
#define bq27541CMD_ARTTE_MSB 0x05
#define bq27541CMD_TEMP_LSB  0x06
#define bq27541CMD_TEMP_MSB  0x07
#define bq27541CMD_VOLT_LSB  0x08
#define bq27541CMD_VOLT_MSB  0x09
#define bq27541CMD_FLAGS_LSB 0x0A
#define bq27541CMD_FLAGS_MSB 0x0B
#define bq27541CMD_NAC_LSB   0x0C
#define bq27541CMD_NAC_MSB   0x0D
#define bq27541CMD_FAC_LSB   0x0E
#define bq27541CMD_FAC_MSB   0x0F
#define bq27541CMD_RM_LSB    0x10
#define bq27541CMD_RM_MSB    0x11
#define bq27541CMD_FCC_LSB   0x12
#define bq27541CMD_FCC_MSB   0x13
#define bq27541CMD_AI_LSB    0x14
#define bq27541CMD_AI_MSB    0x15
#define bq27541CMD_TTE_LSB   0x16
#define bq27541CMD_TTE_MSB   0x17
#define bq27541CMD_TTF_LSB   0x18
#define bq27541CMD_TTF_MSB   0x19
#define bq27541CMD_SI_LSB    0x1A
#define bq27541CMD_SI_MSB    0x1B
#define bq27541CMD_STTE_LSB  0x1C
#define bq27541CMD_STTE_MSB  0x1D
#define bq27541CMD_MLI_LSB   0x1E
#define bq27541CMD_MLI_MSB   0x1F
#define bq27541CMD_MLTTE_LSB 0x20
#define bq27541CMD_MLTTE_MSB 0x21
#define bq27541CMD_AE_LSB    0x22
#define bq27541CMD_AE_MSB    0x23
#define bq27541CMD_AP_LSB    0x24
#define bq27541CMD_AP_MSB    0x25
#define bq27541CMD_TTECP_LSB 0x26
#define bq27541CMD_TTECP_MSB 0x27
#define bq27541CMD_RSVD_LSB  0x28
#define bq27541CMD_RSVD_MSB  0x29
#define bq27541CMD_CC_LSB    0x2A
#define bq27541CMD_CC_MSB    0x2B
#define bq27541CMD_SOC_LSB   0x2C
#define bq27541CMD_SOC_MSB   0x2D
#define bq27541CMD_DCAP_LSB  0x3C
#define bq27541CMD_DCAP_MSB  0x3D
#define bq27541CMD_DFCLS     0x3E
#define bq27541CMD_DFBLK     0x3F
#define bq27541CMD_ADF       0x40
#define bq27541CMD_ACKSDFD   0x54
#define bq27541CMD_DFDCKS    0x60
#define bq27541CMD_DFDCNTL   0x61
#define bq27541CMD_DNAMELEN  0x62
#define bq27541CMD_DNAME     0x63

#define bq27541CMD_CNTL_SUB_CONTROL_STATUS 	0x0000 //Reports the status of DF Checksum, Hibernate, IT, etc.
#define bq27541CMD_CNTL_SUB_DEVICE_TYPE 	0x0001 //Reports the device type of 0x0541 (indicating bq27541)
#define bq27541CMD_CNTL_SUB_FW_VERSION 		0x0002 //Reports the firmware version on the device type
#define bq27541CMD_CNTL_SUB_HW_VERSION 		0x0003 //Reports the hardware version of the device type
#define bq27541CMD_CNTL_SUB_DF_CHECKSUM 	0x0004 //Enables a data flash checksum to be generated and reports on a read
#define bq27541CMD_CNTL_SUB_RESET_DATA 		0x0005 //Returns reset data
#define bq27541CMD_CNTL_SUB_Reserved 		0x0006 //Not to be used
#define bq27541CMD_CNTL_SUB_PREV_MACWRITE 	0x0007 //Returns previous MAC command code
#define bq27541CMD_CNTL_SUB_CHEM_ID 		0x0008 //Reports the chemical identifier of the Impedance Track? configuration
#define bq27541CMD_CNTL_SUB_DF_VERSION 		0x000C //Reports the data flash version on the device
#define bq27541CMD_CNTL_SUB_SET_FULLSLEEP 	0x0010 //Set the [FullSleep] bit in Control Status register to 1
#define bq27541CMD_CNTL_SUB_SET_HIBERNATE 	0x0011 //Forces CONTROL_STATUS [HIBERNATE] to 1
#define bq27541CMD_CNTL_SUB_CLEAR_HIBERNATE 0x0012 //Forces CONTROL_STATUS [HIBERNATE] to 0
#define bq27541CMD_CNTL_SUB_SET_SHUTDOWN 	0x0013 //Enables the SE pin to change state
#define bq27541CMD_CNTL_SUB_CLEAR_SHUTDOWN 	0x0014 //Disables the SE pin from changing state
#define bq27541CMD_CNTL_SUB_SET_HDQINTEN 	0x0015 //Forces CONTROL_STATUS [HDQIntEn] to 1
#define bq27541CMD_CNTL_SUB_CLEAR_HDQINTEN 	0x0016 //Forces CONTROL_STATUS [HDQIntEn] to 0
#define bq27541CMD_CNTL_SUB_SEALED 			0x0020 //Places the bq27541 is SEALED access mode
#define bq27541CMD_CNTL_SUB_IT_ENABLE 		0x0021 //Enables the Impedance Track? algorithm
#define bq27541CMD_CNTL_SUB_CAL_MODE 		0x0040 //Places the bq27541 in calibration mode
#define bq27541CMD_CNTL_SUB_RESET 			0x0041 //Forces a full reset of the bq27541

#define AVERAGE_DISCHARGE_CURRENT_MA       -250	/* The average discharge current value. USER CONFIG: AtRate setting (mA)*/

////////////////////////////////////////////////////////////////////////////////
int bq27541_get_Control( void );
int bq27541_set_Control(int cntl_data);

int bq27541_get_device_type( void );


int bq27541_get_AtRate( void );
int bq27541_set_AtRate( void );

int bq27541_get_AtRateTimeToEmpty( void );
int bq27541_get_Temperature( void );
int bq27541_get_Voltage( void );
int bq27541_get_Flags( void );
int bq27541_get_NominalAvailableCapacity( void );
int bq27541_get_FullAvailableCapacity( void );
int bq27541_get_RemainingCapacity( void );
int bq27541_get_FullChargeCapacity( void );
int bq27541_get_AverageCurrent( void );
int bq27541_get_TimeToEmpty( void );
int bq27541_get_TimeToFull( void );
int bq27541_get_StandbyCurrent( void );
int bq27541_get_StandbyTimeToEmpty( void );
int bq27541_get_MaxLoadCurrent( void );
int bq27541_get_MaxLoadTimeToEmpty( void );
int bq27541_get_AvailableEnergy( void );
int bq27541_get_AveragePower( void );
int bq27541_get_TTEatConstantPower( void );
int bq27541_get_CycleCount( void );
int bq27541_get_StateOfCharge( void );

int bq27541_get_DesignCapacity( void );
int bq27541_get_DeviceNameLength( void );
unsigned char * bq27541_get_DeviceName(unsigned char * str,int size);

int bq27541_get_reg( unsigned char reg );

int bq27541_checkid(void);

////////////////////////////////////////////////////////////////////////////////
int bq27541_SetDataFlashClass(int subclass);
int bq27541_SetDataFlashBlock(int block);
int bq27541_WriteBlockData(unsigned char * buf,int size);
int bq27541_ReadBlockData(unsigned char * buf,int size);
int bq27541_WriteBlockDataChecksum(int checksum);
int bq27541_BlockDataControl(int enable);

int bq27541_ReadFlashData(int subclass,int offset,unsigned char * buf,int size);
int bq27541_WriteFlashData(int subclass,int offset,unsigned char * buf,int size);
////////////////////////////////////////////////////////////////////////////////
void bq27541_i2c_init(struct platform_device *pdev);
void bq27541_i2c_exit(void);

#endif /*  _MEIZUM9_BQ27541_H_ */
