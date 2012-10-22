/*
 * bq27541 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 *
 * Based on bq27541 battery driver
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <asm/unaligned.h>

#include "m9w_bq27541.h"

//#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define dprintk(x...) 	printk(x)
#else
#define dprintk(x...)
#endif

struct bq27541_device_info {
	struct platform_device 		*pdev;
	struct i2c_client			*client;
};

struct bq27541_device_info g_bq27541_device_info = 
{
	.pdev = NULL,
	.client = NULL,
};


/*
 * bq27541 specific code
 */
	
static int bq27541_iic_read(struct i2c_client *client,unsigned char reg,unsigned char *rt_value,int size)
{
	int err;

	//dprintk("%s:reg = %d \n", __func__,reg);

	if (!client || !client->adapter)
		return -ENODEV;
	
	err = i2c_master_send(g_bq27541_device_info.client, (char *)&reg, 1);
	if( err >= 0 )
	{
	  err = i2c_master_recv(g_bq27541_device_info.client, rt_value, size);
	  if( err >= 0 )
		return 0;
	}
	
	return err;
}

static int bq27541_iic_write(struct i2c_client *client,unsigned char adr,unsigned char *data,int size)
{
	char buf[128];
	int ret;
	
	//dprintk("%s:reg = %d \n", __func__,adr);
	if (!client || !client->adapter)
		return -ENODEV;
	
	buf[0] = adr;
	memcpy(&buf[1],data,size);
	ret = i2c_master_send(client, buf, size+1);
	if(ret<0)
		pr_err("failed to transmit instructions to bq27541.\n");
	
	return ret;
}

static int bq27541_iic_read_byte(struct i2c_client *client,unsigned char reg,unsigned char *rt_value)
{
	return bq27541_iic_read(client,reg,rt_value,1);
}

static int bq27541_iic_read_word(struct i2c_client *client,unsigned char reg,unsigned short * rt_value)
{
	return bq27541_iic_read(client,reg,(unsigned char *)rt_value,2);
}

static int bq27541_iic_write_byte(struct i2c_client *client,unsigned char adr,unsigned char data)
{
	return bq27541_iic_write(client,adr,&data,1);;
}

static int bq27541_iic_write_word(struct i2c_client *client,unsigned char adr,unsigned short data)
{
	return bq27541_iic_write(client,adr,(unsigned char *)&data,2);
}

/////////////////////////////////////////////////////////////////////////////////
int bq27541_get_reg( unsigned char reg )	
{
	int ret;
	unsigned short val = 0; 
	
	//dprintk("%s:++\n", __func__);

	ret = bq27541_iic_read_word(g_bq27541_device_info.client,reg,&val);
	if(ret < 0 )
	{
		pr_warn("%s:failed to read data from bq27541,try again.\n",__func__);
		ret = bq27541_iic_read_word(g_bq27541_device_info.client,reg,&val);
		if(ret < 0)
		{
			pr_err("%s:failed to read data from bq27541.\n",__func__);
			return ret;
		}
	}		
	
	//dprintk("%s:(%d)\n", __func__,val);

	return val;
}

int bq27541_get_Control( void )
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_CNTL_LSB);
	
	dprintk("%s:(0x%.4X)\n", __func__,ret);

	return ret;
}

int bq27541_set_Control(int cntl_data)	
{
	int ret = 0;
	
	//dprintk("%s:(%d)++\n", __func__,cntl_data);

	ret = bq27541_iic_write_word(g_bq27541_device_info.client,bq27541CMD_CNTL_LSB,cntl_data);
	if(ret < 0 )
	{
		pr_err("%s:failed to read data from bq27541.\n",__func__);
		return ret;
	}		
	
	//dprintk("%s:--\n", __func__);

	return ret;
}

int bq27541_get_control_status( void )
{
	int ret = 0;

	ret = bq27541_set_Control(bq27541CMD_CNTL_SUB_CONTROL_STATUS);
	ret = bq27541_get_Control();
	
	dprintk("%s:(0x%.4X)\n", __func__,ret);

	return ret;
}

int bq27541_get_device_type( void )
{
	int ret = 0;

	ret = bq27541_set_Control(bq27541CMD_CNTL_SUB_DEVICE_TYPE);
	ret = bq27541_get_Control();
	
	dprintk("%s:(0x%.4X)\n", __func__,ret);

	return ret;
}

int bq27541_get_fw_version( void )
{
	int ret = 0;

	ret = bq27541_set_Control(bq27541CMD_CNTL_SUB_FW_VERSION);
	ret = bq27541_get_Control();
	
	dprintk("%s:(0x%.4X)\n", __func__,ret);

	return ret;
}

int bq27541_get_hw_version( void )
{
	int ret = 0;

	ret = bq27541_set_Control(bq27541CMD_CNTL_SUB_HW_VERSION);
	ret = bq27541_get_Control();
	
	dprintk("%s:(0x%.4X)\n", __func__,ret);

	return ret;
}


/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
int bq27541_get_AtRate( void )	
{
	int ret = 0;

	ret = (short)bq27541_get_reg(bq27541CMD_AR_LSB);
	
	dprintk("%s:(%dmA)\n", __func__,ret);
	
	//if( ret < 0 ) // Output
	//	ret = -1 * ret;

	return ret;
}


/*-----------------------------------------------------------------------------
The AtRate( ) read-/write-word function is the first half of a two-function command set used to set the AtRate
value used in calculations made by the AtRateTimeToEmpty( ) function. The AtRate( ) units are in mA.

The AtRate( ) value is a signed integer, with negative values interpreted as a discharge current value. The
AtRateTimeToEmpty( ) function returns the predicted operating time at the AtRate value of discharge. The
default value for AtRate( ) is zero and will force AtRateTimeToEmpty( ) to return 65,535. Both the AtRate( ) and
AtRateTimeToEmpty( ) commands should only be used in NORMAL mode.
-----------------------------------------------------------------------------*/
int bq27541_set_AtRate( void )	
{
	int ret;
	unsigned short val = 0; 
	
	dprintk("%s:++\n", __func__);

	ret = bq27541_iic_read_word(g_bq27541_device_info.client,bq27541CMD_SOC_LSB,&val);
	if(ret < 0 )
	{
		pr_err("%s:failed to read data from bq27541.\n",__func__);
		return ret;
	}		
	
	dprintk("%s:(%d mA)\n", __func__,val);

	return val;
}


/*--------------------------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the predicted remaining operating time if the battery
is discharged at the AtRate( ) value in minutes with a range of 0 to 65,534. A value of 65,535 indicates AtRate( )
= 0. The fuel gauge updates AtRateTimeToEmpty( ) within 1 s after the system sets the AtRate( ) value. The fuel
gauge automatically updates AtRateTimeToEmpty( ) based on the AtRate( ) value every 1s. Both the AtRate( )
and AtRateTimeToEmpty( ) commands should only be used in NORMAL mode.
--------------------------------------------------------------------------------------------*/
int bq27541_get_AtRateTimeToEmpty( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_ARTTE_LSB);

	dprintk("%s:(%dMinutes)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the battery temperature 
in units of 0.1K measured by the fuel gauge.
-----------------------------------------------------------------------------*/
int bq27541_get_Temperature( void )	
{
	int ret= 0;

	ret = bq27541_get_reg(bq27541CMD_TEMP_LSB);
	
	dprintk("%s:(%d 0.1K)\n", __func__,ret); // 273.16K

	return (ret-2732);
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the measured cell-pack voltage 
in mV with a range of 0 to 6000 mV.
-----------------------------------------------------------------------------*/
int bq27541_get_Voltage( void )	
{
	int ret= 0;
	
	ret = bq27541_get_reg(bq27541CMD_VOLT_LSB);
	
	dprintk("%s:(%dmV)\n", __func__,ret);

	return ret;
}


/*-----------------------------------------------------------------------------
This read-only function returns the contents of the gas-gauge status register, 
depicting the current operating status.

[bit15]OTC = Over-Temperature in Charge condition is detected. True when set
[bit14]OTD = Over-Temperature in Discharge condition is detected. True when set
[bit11]CHG_INH =Charge Inhibit indicates the temperature is outside the range 
		[Charge Inhibit Temp Low, CHG_INH = High]. True when set
[bit10]XCHG = Charge Suspend Alert indicates indicates the temperature is outside the range.
		[Suspend Temperature Low, Suspend Temperature High]. True when set
[bit09]FC = Full-charged condition reached (RMFCC=1; Set FC_Set%=-1% when RMFCC=0). 
		True when set
[bit08]CHG = (Fast) charging allowed. True when set
[bit02]SOC1 = State-of-Charge-Threshold 1 (SOC1 Set) reached. True when set
[bit01]SOCF = State-of-Charge-Threshold Final (SOCF Set %) reached. True when set
[bit00]DSG = Discharging detected. True when set
-----------------------------------------------------------------------------*/
int bq27541_get_Flags( void )
{
	int ret= 0;

	ret = bq27541_get_reg(bq27541CMD_FLAGS_LSB);
	
	dprintk("%s:(0x%.4X)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only command pair returns the uncompensated 
(less than C/20 load) battery capacity remaining. 
Units are mAh.
-----------------------------------------------------------------------------*/
int bq27541_get_NominalAvailableCapacity( void )	
{
	int ret= 0;

	ret = bq27541_get_reg(bq27541CMD_NAC_LSB);
	
	dprintk("%s:(%dmAh)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only command pair returns the uncompensated (less than C/20 load) capacity of the battery when fully
charged. Units are mAh. FullAvailableCapacity( ) is updated at regular intervals, as specified by the IT algorithm.
-----------------------------------------------------------------------------*/
int bq27541_get_FullAvailableCapacity( void )	
{
	int ret= 0;

	ret = bq27541_get_reg(bq27541CMD_FAC_LSB);	
	
	dprintk("%s:(%dmAh)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only command pair returns the compensated battery capacity remaining. Units are mAh.
-----------------------------------------------------------------------------*/
int bq27541_get_RemainingCapacity( void )	
{
	int ret= 0;

	ret = bq27541_get_reg(bq27541CMD_RM_LSB);
	
	dprintk("%s:(%dmAh)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only command pair returns the compensated capacity of the battery when fully charged. 
FullChargeCapacity( ) is updated at regular intervals, as specified by the IT algorithm.
Units aremAh. 
-----------------------------------------------------------------------------*/
int bq27541_get_FullChargeCapacity( void )	
{
	int ret= 0;

	ret = bq27541_get_reg(bq27541CMD_FCC_LSB);
	
	dprintk("%s:(%dmAh)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only command pair returns a signed integer value that is the average current flow 
through the sense resistor. It is updated every 1 second. 
Units are mA.
-----------------------------------------------------------------------------*/
int bq27541_get_AverageCurrent( void )	
{
	int ret= 0;

	ret = (short)bq27541_get_reg(bq27541CMD_AI_LSB);
	
	dprintk("%s:(%dmA)\n", __func__,ret);
	
	//if( ret < 0 ) // Output
	//	ret = -1 * ret;

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the predicted remaining battery life at the present
rate of discharge, in minutes. A value of 65,535 indicates battery is not being discharged.

-----------------------------------------------------------------------------*/
int bq27541_get_TimeToEmpty( void )	
{
	int ret= 0;

	ret = (short)bq27541_get_reg(bq27541CMD_TTE_LSB);
	
	dprintk("%s:(%dMinutes)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of predicted remaining time until the battery reaches
full charge, in minutes, based upon AverageCurrent( ). The computation accounts for the taper current time
extension from the linear TTF computation based on a fixed AverageCurrent( ) rate of charge accumulation. A
value of 65,535 indicates the battery is not being charged.

-----------------------------------------------------------------------------*/
int bq27541_get_TimeToFull( void )	
{
	int ret= 0;

	ret = bq27541_get_reg(bq27541CMD_TTF_LSB);
		
	dprintk("%s:(%dMinutes)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns a signed integer value of the measured standby current through the sense
resistor. The StandbyCurrent( ) is an adaptive measurement. Initially it reports the standby current programmed
in Initial Standby, and after spending some time in standby, reports the measured standby current.
The register value is updated every 1 second when the measured current is above the Deadband Current and is
less than or equal to 2 x Initial Standby Current. The first and last values that meet this criteria are not
averaged in, since they may not be stable values. To approximate a 1 minute time constant, each new
StandbyCurrent( ) value is computed by taking approximate 93% weight of the last standby current and
approximate 7% of the current measured average current.
-----------------------------------------------------------------------------*/
int bq27541_get_StandbyCurrent( void )	
{
	int ret= 0;

	ret = (short)bq27541_get_reg(bq27541CMD_SI_LSB);
	
	dprintk("%s:(%dmA)\n", __func__,ret);
	
	//if( ret < 0 ) // Output
	//	ret = -1 * ret;

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the predicted remaining battery life at the standby
rate of discharge, in minutes. The computation uses Nominal Available Capacity (NAC), the uncompensated
remaining capacity, for this computation. A value of 65,535 indicates battery is not being discharged.
-----------------------------------------------------------------------------*/
int bq27541_get_StandbyTimeToEmpty( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_STTE_LSB);
		
	dprintk("%s:(%dMinutes)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns a signed integer value, in units of mA, of the maximum load conditions. The
MaxLoadCurrent( ) is an adaptive measurement which is initially reported as the maximum load current
programmed in Initial Max Load Current. If the measured current is ever greater than Initial Max Load
Current, then MaxLoadCurrent( ) updates to the new current. MaxLoadCurrent( ) is reduced to the average of
the previous value and Initial Max Load Current whenever the battery is charged to full after a previous
discharge to an SOC less than 50%. This prevents the reported value from maintaining an unusually high value.
-----------------------------------------------------------------------------*/
int bq27541_get_MaxLoadCurrent( void )	
{
	int ret = 0;

	ret = (short)bq27541_get_reg(bq27541CMD_MLI_LSB);
	
	dprintk("%s:(%dmA)\n", __func__,ret);
	
	//if( ret < 0 ) // Output
	//	ret = -1 * ret;
	
	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the predicted remaining battery life at the maximum
load current discharge rate, in minutes. A value of 65,535 indicates that the battery is not being discharged.
-----------------------------------------------------------------------------*/
int bq27541_get_MaxLoadTimeToEmpty( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_MLTTE_LSB);
	
	dprintk("%s:(%dMinutes)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the predicted charge or energy remaining in the
battery. The value is reported in units of mWh.
-----------------------------------------------------------------------------*/
int bq27541_get_AvailableEnergy( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_AE_LSB);
		
	dprintk("%s:(%d  10mWhr)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-word function returns an unsigned integer value of the average power of the current discharge. 
A value of 0 indicates that the battery is not being discharged. The value is reported in units of mW.
-----------------------------------------------------------------------------*/
int bq27541_get_AveragePower( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_AP_LSB);
	
	dprintk("%s:(%d 10mW)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the predicted remaining operating time if the battery
is discharged at the AveragePower( ) value in minutes. A value of 65,535 indicates AveragePower( ) = 0. The
fuel gauge automatically updates TimeToEmptyatContantPower( ) based on the AveragePower( ) value every 1s.
-----------------------------------------------------------------------------*/
int bq27541_get_TTEatConstantPower( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_TTECP_LSB);
	
	dprintk("%s:(%dMinutes)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the number of cycles the battery 
has experienced with a range of 0 to 65,535. 
One cycle occurs when accumulated discharge ¡Ý CC Threshold.
-----------------------------------------------------------------------------*/
int bq27541_get_CycleCount( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_CC_LSB);
	
	dprintk("%s:(%d Counts)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
This read-only function returns an unsigned integer value of the predicted remaining battery capacity 
expressed as a percentage of FullChargeCapacity( ), with a range of 0 to 100%.
-----------------------------------------------------------------------------*/
int bq27541_get_StateOfCharge( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_SOC_LSB);
		
	dprintk("%s:(%d%%)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
SEALED and UNSEALED Access: This command returns the value is stored in Design Capacity and is
expressed in mAh. This is intended to be the theoretical or nominal capacity of a new pack, but has no bearing
on the operation of the fuel gauge functionality.
-----------------------------------------------------------------------------*/
int bq27541_get_DesignCapacity( void )	
{
	int ret = 0;

	ret = bq27541_get_reg(bq27541CMD_DCAP_LSB);
	
	dprintk("%s:(%d mAH)\n", __func__,ret);

	return ret;
}

/*-----------------------------------------------------------------------------
UNSEALED and SEALED Access: This byte contains the length of the Device Name.
-----------------------------------------------------------------------------*/
int bq27541_get_DeviceNameLength( void )	
{
	int ret = 0;
	int val = 0;

	ret = bq27541_iic_read_byte(g_bq27541_device_info.client,bq27541CMD_DNAMELEN,(unsigned char *)&val);
	if(ret < 0 )
	{
		pr_err("%s:failed to read data from bq27541.\n",__func__);
		return ret;
	}		
	
	dprintk("%s:(%d)\n", __func__,val);

	return val;
}

/*-----------------------------------------------------------------------------
UNSEALED and SEALED Access: This block contains the device name that is programmed in Device Name.
-----------------------------------------------------------------------------*/
unsigned char * bq27541_get_DeviceName(unsigned char * str,int size)	
{
	int ret;
	
	dprintk("%s:++\n", __func__);

	memset(str,0,size);
	ret = bq27541_get_DeviceNameLength();

	if(size > ret)
		size = ret;
		
	ret = bq27541_iic_read(g_bq27541_device_info.client,bq27541CMD_DNAME,str,size);
	if(ret < 0 )
	{
		pr_err("%s:failed to read data from bq27541.\n",__func__);
		return NULL;
	}		
	
	dprintk("%s:(%s)\n", __func__,str);

	return str;
}


/*-----------------------------------------------------------------------------
Instructs the fuel gauge to return the device type
-----------------------------------------------------------------------------*/
int bq27541_checkid(void)
{
	if( bq27541_get_Voltage() < 0 )	
		return 0;
	
	if( bq27541_get_device_type() == BQ27541_ID)
		return BQ27541_ID;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
/*-----------------------------------------------------------------------------
UNSEALED Access: This command sets the data flash class to be accessed. 
The class to be accessed should be entered in hexadecimal.
SEALED Access: This command is not available in SEALED mode.
-----------------------------------------------------------------------------*/
int bq27541_SetDataFlashClass(int subclass)
{
	int ret;
	
	ret = bq27541_iic_write_byte(g_bq27541_device_info.client,bq27541CMD_DFCLS,subclass);
	if(ret < 0 )
	{
		pr_err("%s:failed to read data from bq27541.\n",__func__);
		return ret;
	}		

	return 0;
}


/*-----------------------------------------------------------------------------
UNSEALED Access: This command sets the data flash block to be accessed. When 0x00 is written to
BlockDataControl( ), DataFlashBlock( ) holds the block number of the data flash to be read or written. 
Example:
writing a 0x00 to DataFlashBlock( ) specifies access to the first 32 byte block and a 0x01 specifies 
access to the second 32 byte block, and so on.
SEALED Access: 
This command directs which data flash block will be accessed by the BlockData( ) command.
Writing a 0x00 to DataFlashBlock( ) specifies the BlockData( ) command will transfer authentication data. 
Issuing a 0x01, 0x02 or 0x03 instructs the BlockData( ) command to transfer Manufacturer Info 
Block A, B, or C, respectively.
-----------------------------------------------------------------------------*/
int bq27541_SetDataFlashBlock(int block)
{
	int ret;
	
	ret = bq27541_iic_write_byte(g_bq27541_device_info.client,bq27541CMD_DFBLK,block);
	if(ret < 0 )
	{
		pr_err("%s:failed to read data from bq27541.\n",__func__);
		return ret;
	}		

	return 0;
}


/*-----------------------------------------------------------------------------
This command range is used to transfer data for data flash class access. 
This command range is the 32-byte data block used to access Manufacturer Info Block A, B, or C. 
Manufacturer Info Block A is read only for the sealed access. UNSEALED access is read/write.
-----------------------------------------------------------------------------*/
int bq27541_WriteBlockData(unsigned char * buf,int size)
{
	return bq27541_iic_write(g_bq27541_device_info.client,bq27541CMD_ADF,buf,size);
}

int bq27541_ReadBlockData(unsigned char * buf,int size)
{
	return bq27541_iic_read(g_bq27541_device_info.client,bq27541CMD_ADF,buf,size);
}


/*-----------------------------------------------------------------------------
The host system should write this value to inform the device that new data is ready for programming 
into the specified data flash class and block.¡±
UNSEALED Access: This byte contains the checksum on the 32 bytes of block data read or written to data flash.
The least-significant byte of the sum of the data bytes written must be complemented ( [255 ¨C x] , 
for x the least-significant byte) before being written to 0x60.
SEALED Access: This byte contains the checksum for the 32 bytes of block data written to Manufacturer Info
Block A, B, or C. The least-significant byte of the sum of the data bytes written must be complemented ( [255 ¨C
x] , for x the least-significant byte) before being written to 0x60.
-----------------------------------------------------------------------------*/
int bq27541_WriteBlockDataChecksum(int checksum)
{
	int ret;
	
	ret = bq27541_iic_write_byte(g_bq27541_device_info.client,bq27541CMD_DFDCKS,checksum);
	if(ret < 0 )
	{
		pr_err("%s:failed to read data from bq27541.\n",__func__);
		return ret;
	}		

	return 0;
}


/*-----------------------------------------------------------------------------
UNSEALED Access: This command is used to control data flash access mode. Writing 0x00 to this command
enables BlockData( ) to access general data flash. Writing a 0x01 to this command enables SEALED mode
operation of DataFlashBlock( ).
SEALED Access: This command is not available in SEALED mode.
-----------------------------------------------------------------------------*/
int bq27541_BlockDataControl(int enable)
{
	int ret;
	
	ret = bq27541_iic_write_byte(g_bq27541_device_info.client,bq27541CMD_DFDCNTL,(!enable));
	if(ret < 0 )
	{
		pr_err("%s:failed to read data from bq27541.\n",__func__);
		return ret;
	}		

	return 0;
}

int bq27541_ReadFlashData(int subclass,int offset,unsigned char * buf,int size)
{
	int ret;
	
	if( bq27541_BlockDataControl(true) != 0)
		return ret;
	
	if( bq27541_SetDataFlashClass(subclass) != 0)
		return ret;
	if( bq27541_SetDataFlashBlock(offset) != 0)
		return ret;

	if( bq27541_ReadBlockData(buf,size) != 0)
		return ret;

	if( bq27541_BlockDataControl(false) != 0)
		return ret;

	return 0;
}

int bq27541_WriteFlashData(int subclass,int offset,unsigned char * buf,int size)
{
	int ret;
	
	if( bq27541_BlockDataControl(true) != 0)
		return ret;
	
	if( bq27541_SetDataFlashClass(subclass) != 0)
		return ret;
	if( bq27541_SetDataFlashBlock(offset) != 0)
		return ret;

	if( bq27541_WriteBlockData(buf,size) != 0)
		return ret;
	
	if( bq27541_BlockDataControl(false) != 0)
		return ret;

	return 0;
}


////////////////////////////////////////////////////////////////////////////////


static int bq27541_hw_init(void)
{
	int ret = 0;
	
	// Set AtRate (units = mA)
	ret = bq27541_iic_write_word(g_bq27541_device_info.client,bq27541CMD_AR_LSB,AVERAGE_DISCHARGE_CURRENT_MA);
	
	return ret;
}

/////////////////////////////////////////////////////////

struct bq27541_setup_data {
	unsigned i2c_bus;
	unsigned short i2c_address;
};

/*
static struct bq27541_setup_data g_bq27541_setup_data = 
{   
    .i2c_bus = 3,   
    .i2c_address = IIC_ADDR,
};
*/


static int bq27541_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret = 0;

   //check i2c capability
   if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("bq27541_i2c_probe: i2c_check_functionality error!\n");
        return -ENODEV;
   }

//	i2c_set_clientdata(client, platform_get_drvdata(g_bq27541_device_info.pdev));
	g_bq27541_device_info.client = client;
	i2c_set_clientdata(client, &g_bq27541_device_info);

	ret = bq27541_hw_init();
	if (ret < 0)
		pr_err("failed to initialise bq27541\n");

	return ret;
}

static int bq27541_i2c_remove(struct i2c_client *client)
{	
	return 0;
}

#ifdef CONFIG_PM
static int bq27541_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	
	return 0;
}

static int bq27541_i2c_resume(struct i2c_client *client)
{
	
	return 0;
}

#else
#define bq27541_i2c_suspend			NULL
#define bq27541_i2c_resume			NULL
#endif   /* CONFIG_PM */


static const struct i2c_device_id bq27541_i2c_id[] = {
	{ BQ27541_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bq27541_i2c_id);

static struct i2c_driver bq27541_i2c_driver = {
	.driver = {
		.name = BQ27541_NAME,
		.owner = THIS_MODULE,
	},
	.probe =    bq27541_i2c_probe,
	.remove =   bq27541_i2c_remove,
#ifdef CONFIG_PM
	.suspend = bq27541_i2c_suspend,
	.resume =  bq27541_i2c_resume,
#endif
	.id_table = bq27541_i2c_id,
};


void bq27541_i2c_init(struct platform_device *pdev)
{
	g_bq27541_device_info.pdev = pdev;
}

void bq27541_i2c_exit(void)
{
	g_bq27541_device_info.pdev = NULL;
}


static int __init bq27541_init(void)
{
    return i2c_add_driver(&bq27541_i2c_driver);
}

static void __exit bq27541_exit(void)
{
    i2c_del_driver(&bq27541_i2c_driver);
}

module_init(bq27541_init);
module_exit(bq27541_exit);

MODULE_DESCRIPTION("bq27541 driver");
MODULE_LICENSE("GPL");
