/*
 * linux/sound/soc/codecs/tlv320aic36_path.c
 *
 *
 * Copyright (C) 2010 Meizu, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * History:
 *
 */

/***************************** INCLUDES ************************************/
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/delay.h>
#include "tlv320aic36.h"


/* enable debug prints in the driver */
//#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define dprintk(x...) 	printk(x)
#else
#define dprintk(x...)
#endif

//  Notic:!!!!!!!!!! defined in tlv320aic36_mini-dsp.c
#define	EQ_SEL_HP	1
#define	EQ_SEL_SPK	2
#define	EQ_SEL_DIRECT	3
//--------------------------------------------

#define	INT_MIC_MODE	0
#define	EXT_MIC_MODE	1

#define	MUSIC_MODE	0
#define	VOICE_MODE		1

extern int put_burst_minidsp_mode (struct snd_soc_codec *codec,u8 uVoice, u8 force);
extern int put_drc_mux (struct snd_soc_codec *codec,int bDrcOnOff);
extern int put_EQ_Select (struct snd_soc_codec *codec,int EQMode);
extern unsigned int aic36_read_status(struct snd_soc_codec * codec, unsigned int reg);
volatile int  aic36_phone_func = 0;
volatile int  aic36_lineout_func = 0;
volatile int  aic36_recout_func = 0;
volatile int  aic36_bbout_func = 0;
volatile int  aic36_linein_func = 0;
volatile int  aic36_intmic_func = 0;
volatile int  aic36_extmic_func = 0;
volatile int  aic36_answer_func = 0;
extern u8 voice_mode_enable;
extern bool g_bBiasOnOff;

extern int set_speaker_OnOff(bool bEn);
int aic36_check_ldac_power(struct snd_soc_codec *codec, int on)
{
	int count=10;
	int value = 0;
	int mask=0x80;

	do
	{
		value = aic36_read_status(codec, DAC_STATUS_REG);
		if(on){
			if(value & mask)
				break;
		}else{
			if(!(value & mask))
				break;
		}
		msleep(10);
		count --;
	}while(count>0);
	return 0;	
}
int aic36_check_rdac_power(struct snd_soc_codec *codec, int on)
{
	int count=10;
	int value = 0;	
	int mask=0x08;
	
	do
	{
		value = aic36_read_status(codec, DAC_STATUS_REG);
		if(on){
			if(value & mask)
				break;
		}else{
			if(!(value & mask))
				break;
		}
		msleep(10);
		count --;
	}while(count>0);
	return 0;	
}
int aic36_check_ladc_power(struct snd_soc_codec *codec, int on)
{
	int count=10;
	int value = 0;
	int mask=0x40;

	do
	{
		value = aic36_read_status(codec, ADC_STATUS_REG);
		if(on){
			if(value & mask)
				break;
		}else{
			if(!(value & mask))
				break;
		}
		msleep(10);
		count --;
	}while(count>0);

	return 0;
}
int aic36_check_radc_power(struct snd_soc_codec *codec, int on)
{
	int count=10;
	int value = 0;	
	int mask=0x04;
	
	do
	{
		value = aic36_read_status(codec, ADC_STATUS_REG);
		if(on){
			if(value & mask)
				break;
		}else{
			if(!(value & mask))
				break;
		}
		msleep(10);
		count --;
	}while(count>0);
	return 0;	
}
int
aic36_phone_mute(struct snd_soc_codec *codec,bool bMute)
{
	dprintk ("%s(%d)\n", __func__,bMute);
	if(bMute)
	{
		codec->write(codec,HPL_OUT_VOL,(codec->read(codec,HPL_OUT_VOL) & ~(1<<3)));
		codec->write(codec,HPR_OUT_VOL,(codec->read(codec,HPR_OUT_VOL) & ~(1<<3)));
	}	
	else
	{
		codec->write(codec,HPL_OUT_VOL,(codec->read(codec,HPL_OUT_VOL) |(1<<3)));
		codec->write(codec,HPR_OUT_VOL,(codec->read(codec,HPR_OUT_VOL) |(1<<3)));
	}	

	return 0;
}

int
aic36_spk_mute(struct snd_soc_codec *codec,bool bMute)
{
	dprintk ("%s(%d)\n", __func__,bMute);
	if(bMute)
	{
		codec->write(codec,LINEOUT_LPM_VOL,(codec->read(codec,LINEOUT_LPM_VOL) & ~(1<<3)));
		codec->write(codec,LINEOUT_RPM_VOL,(codec->read(codec,LINEOUT_RPM_VOL) & ~(1<<3)));
	}	
	else
	{
		codec->write(codec,LINEOUT_LPM_VOL,(codec->read(codec,LINEOUT_LPM_VOL) |(1<<3)));
		codec->write(codec,LINEOUT_RPM_VOL,(codec->read(codec,LINEOUT_RPM_VOL) |(1<<3)));
	}	
	return 0;
}

int
aic36_recl_mute(struct snd_soc_codec *codec,bool bMute)
{
	dprintk ("%s(%d)\n", __func__,bMute);
	if(bMute)
	{
		codec->write(codec,RECL_OUT_VOL,(codec->read(codec,RECL_OUT_VOL) & ~(1<<3)));
	}	
	else
	{
		codec->write(codec,RECL_OUT_VOL,(codec->read(codec,RECL_OUT_VOL) |(1<<3)));
	}	

	return 0;
}

int
aic36_recr_mute(struct snd_soc_codec *codec,bool bMute)
{
	dprintk ("%s(%d)\n", __func__,bMute);
	if(bMute)
	{
		codec->write(codec,RECR_OUT_VOL,(codec->read(codec,RECR_OUT_VOL) & ~(1<<3)));
	}		
	else
	{
		codec->write(codec,RECR_OUT_VOL,(codec->read(codec,RECR_OUT_VOL) |(1<<3)));
	}

	return 0;
}

static int
aic36_phone_power(struct snd_soc_codec *codec,bool bPower)
{
	int value = 0;
	
	dprintk ("%s(%d)\n", __func__,bPower);
	if(bPower)
	{
		/* Power on HPL and HPR */ 
		value = codec->read (codec, HPL_OUT_VOL); 
		value |= 0x01;		  
		codec->write (codec, HPL_OUT_VOL, value); 
		value = codec->read (codec, HPR_OUT_VOL); 
		value |= 0x01;
		codec->write (codec, HPR_OUT_VOL, value);
	}	
	else
	{
		/* Power down headset output  */ 
		value = codec->read (codec, HPL_OUT_VOL);
		value &= ~0x01;  
		codec->write (codec, HPL_OUT_VOL, value);
		value = codec->read (codec, HPR_OUT_VOL);
		value &= ~0x01;  
		codec->write (codec, HPR_OUT_VOL, value);
	}	

	return 0;
}

static int
aic36_spk_power(struct snd_soc_codec *codec, bool bLPower, bool bRPower)
{
	int value = 0;

	dprintk ("%s(L %d, R %d)\n", __func__,bLPower, bRPower);
	if(!bLPower && !bRPower)
	{
		set_speaker_OnOff(false);
		mdelay(10);		
	}	
	if(bLPower)
	{
		/* Power on LINEOUT */ 
		value = codec->read (codec,LINEOUT_LPM_VOL); 
		value |= 0x01;
		codec->write (codec, LINEOUT_LPM_VOL, value);
	}	
	else
	{
		/* Power down LINEOUT */ 
		value = codec->read (codec, LINEOUT_LPM_VOL);
		value &= ~0x01;  
		codec->write (codec, LINEOUT_LPM_VOL, value);
	}
	if(bRPower)
	{
		/* Power on LINEOUT */ 
		value = codec->read (codec, LINEOUT_RPM_VOL); 
		value |= 0x01;
		codec->write (codec, LINEOUT_RPM_VOL, value);
	}	
	else
	{
		/* Power down LINEOUT */ 
		value = codec->read (codec, LINEOUT_RPM_VOL);
		value &= ~0x01;  
		codec->write (codec, LINEOUT_RPM_VOL, value);
	}	
	if(bLPower || bRPower)
	{
		mdelay(10);
		set_speaker_OnOff(true);
	}	
	return 0;
}

static int
aic36_recl_power(struct snd_soc_codec *codec,bool bPower)
{
	int value = 0;

	dprintk ("%s(%d)\n", __func__,bPower);
	if(bPower)
	{
		/* Power on RECOUT	*/
		value = codec->read (codec, RECL_OUT_VOL); 
		value |= 0x01;		  
		codec->write (codec, RECL_OUT_VOL, value); 
	}	
	else
	{
		/* Power down RECOUT  */ 
		value = codec->read (codec, RECL_OUT_VOL);
		value &= ~0x01;  
		codec->write (codec, RECL_OUT_VOL, value);	
	}	

	return 0;
}

static int
aic36_recr_power(struct snd_soc_codec *codec,bool bPower)
{
	int value = 0;

	dprintk ("%s(%d)\n", __func__,bPower);
	if(bPower)
	{
		/*Switch on Rec Right output */
		value = codec->read (codec, RECR_OUT_VOL);
		value |= 0x01;		
		codec->write (codec, RECR_OUT_VOL, value); 
		
	}		
	else
	{
		/* Power down Rec Right output  */ 
		value = codec->read (codec, RECR_OUT_VOL);
		value &= ~0x01;  
		codec->write (codec, RECR_OUT_VOL, value);	

	}

	return 0;
}

static void Change_Music_Voice_Path(struct snd_soc_codec *codec,u8 uVoice)
{
     dprintk ("%s(%d)\n", __func__,uVoice);

	 
#ifdef	CONFIG_MINI_DSP
     put_burst_minidsp_mode(codec,uVoice, 0);
#endif	 
}

void set_audio_ldac_power(struct snd_soc_codec *codec, int power)
{
	volatile u8 value;
	if(power)
	{
		/* Power up DAC power*/
		value = codec->read (codec, DAC_PWR);
		value |= 0x80;	
		codec->write (codec, DAC_PWR, value);

		/*Left DAC unmute*/
		value = codec->read (codec, DAC_MUTE_CTRL_REG);
		value &= ~(0x08);
		codec->write (codec, DAC_MUTE_CTRL_REG, value);
	}else{
		/*Left DAC unmute*/
		value = codec->read (codec, DAC_MUTE_CTRL_REG);
		value |= 0x08;
		codec->write (codec, DAC_MUTE_CTRL_REG, value);

		/* Power down DAC power*/
		value = codec->read (codec, DAC_PWR);
		value &= (~0x80);	
		codec->write (codec, DAC_PWR, value);
	}
	aic36_check_ldac_power(codec, power);

}
void set_audio_rdac_power(struct snd_soc_codec *codec, int power)
{
	volatile u8 value;

	if(power)
	{	
		/* Power up DAC power*/
		value = codec->read (codec, DAC_PWR);
		value |= 0x40;	
		codec->write (codec, DAC_PWR, value);
		
		aic36_check_rdac_power(codec, power);
		
		/*Left DAC unmute*/
		value = codec->read (codec, DAC_MUTE_CTRL_REG);
		value &= ~(0x04);
		codec->write (codec, DAC_MUTE_CTRL_REG, value);
	}else{
		/*Left DAC unmute*/
		value = codec->read (codec, DAC_MUTE_CTRL_REG);
		value |= 0x04;
		codec->write (codec, DAC_MUTE_CTRL_REG, value);
		
		/* Power down DAC power*/
		value = codec->read (codec, DAC_PWR);
		value &= (~0x40);	
		codec->write (codec, DAC_PWR, value);
	}
	aic36_check_rdac_power(codec, power);
	
}
void set_audio_ladc_power(struct snd_soc_codec *codec, int power)
{
	volatile u8 value;

	if(power)
	{
		/* Switch on Left ADC */
		value = codec->read (codec, ADC_REG_1);
		codec->write (codec, ADC_REG_1, value | ENABLE_LADC);
		mdelay(10);
		/*Enable Left PGA*/
		value = codec->read(codec, L_MICPGA_V);
		value &= (~0x80);
		codec->write (codec, L_MICPGA_V, value);
		mdelay(10);
		
		/*Unmute Left ADC ADC*/
		value = codec->read (codec, ADC_FGA);
		value &= (~0x80);
		codec->write (codec, ADC_FGA, value);
		mdelay(10);
	}else{
		/*Disable Left PGA*/
		value = codec->read(codec, L_MICPGA_V);
		value |= 0x80;
		codec->write (codec, L_MICPGA_V, value);
		mdelay(10);
		/*Power down  left ADC*/
		value = codec->read (codec, ADC_REG_1);
		value &= (~ENABLE_LADC);
		codec->write (codec, ADC_REG_1, value);
		mdelay(10);
		
		/*Mute left ADC*/
		value = codec->read (codec, ADC_FGA);
		value |= 0x80;
		codec->write (codec, ADC_FGA, value);
		mdelay(10);
	}

}
void set_audio_radc_power(struct snd_soc_codec *codec, int power)
{
	volatile u8 value;

	if(power)
	{
		/* Switch on  Right ADC */
		value = codec->read (codec, ADC_REG_1);
		codec->write (codec, ADC_REG_1, value | ENABLE_RADC);
		mdelay(10);
		/*Enable Right PGA*/
		value = codec->read(codec, R_MICPGA_V);
		value &= (~0x80);
		codec->write (codec, R_MICPGA_V, value);
		mdelay(10);

		/*Unmute Right ADC*/
		value = codec->read (codec, ADC_FGA);
		value &= (~0x08);		
		codec->write (codec, ADC_FGA, value);
		mdelay(10);		
	}else{
		/*Disable Left PGA*/
		value = codec->read(codec, R_MICPGA_V);
		value |= 0x80;
		codec->write (codec, R_MICPGA_V, value);
		mdelay(10);
		/*Power down  ADC*/
		value = codec->read (codec, ADC_REG_1);
		value &= (~ENABLE_RADC);
		codec->write (codec, ADC_REG_1, value);
		mdelay(10);

		/*Mute ADC*/
		value = codec->read (codec, ADC_FGA);
		value |= 0x08;
		codec->write (codec, ADC_FGA, value);		
		mdelay(10);
	}
}

void connect_audio_input_route(struct snd_soc_codec *codec)
{
	volatile u8 value;
	struct aic36_priv *aic36= (struct aic36_priv *)dev_get_drvdata(codec->dev);
	 
	 /* Left and Right MICPGA Gain(0-0x5f), increase*/
	 value = codec->read(codec, L_MICPGA_V);
	 value &= (~0x7f);
	 value |= aic36->voice_mic_gain;
	 codec->write (codec, L_MICPGA_V, value);		 
	 dprintk("%s: voice_mic_gain = %d, L_MICPGA_V=0x%x\n", __func__, aic36->voice_mic_gain, codec->read (codec, L_MICPGA_V));

	 value = codec->read(codec, R_MICPGA_V);
	 value &= (~0x7f);
	 value |= AIC36_VOICE_RPGA_GAIN;
	 codec->write (codec, R_MICPGA_V, value);

	 /* Left and Right ADC Volume(0-40), increase; (0x67-7f) decrease*/
	 codec->write (codec, LADC_VOL, 0x74);//-6
	 codec->write (codec, RADC_VOL, 0x0);//+0

	 /*set linein path*/
	if (aic36_linein_func) { 
		// 01: EXTMIC1 is routed to Right MICPGA with 10K resistance		
		codec->write (codec, R_MICPGA_P, 0x10);
		codec->write (codec, R_MICPGA_N, 0x50);
//		mdelay(10);
//		codec->write (codec, R_MICPGA_N, 0x10);
	}
	/* ADC input common mode: 0.9V */
	codec->write (codec, ADC_CHN_ACONF1, 0x00); 
	
	if(g_bBiasOnOff) {
		/* Micbias power up and 1.8V */
		codec->write (codec, ADC_CHN_ACONF2, 0x60);
	}	

	/*set mic path*/
	if(aic36_intmic_func) {		
		// 01: MIC1_P is routed to Left MICPGA with 10K resistance	
		codec->write (codec, L_MICPGA_P, 0x10);
		//Ensure microphone difference input function work well
		// 01:MIC1_M is routed to Left MICPGA with 10K resistance	
		codec->write (codec, L_MICPGA_N, 0x50);
		//mdelay(20);
		// 01:MIC1_M is routed to Left MICPGA with 10K resistance	
		//codec->write (codec, L_MICPGA_N, 0x10);
		
		/* Micbias power up and 1.8V */
		value = codec->read (codec, ADC_CHN_ACONF2);
		codec->write (codec, ADC_CHN_ACONF2, (value & 0xf0) | 0x06);
	}
	if (aic36_extmic_func)
	{
		// 00: EXTMIC_P routed to Left MICPGA with 10K resistance	
		codec->write (codec, L_MICPGA_P, 0x04);

		//Ensure microphone difference input function work well
		// 00: EXTMIC_N routed to Left MICPGA with 10K resistance
		codec->write (codec, L_MICPGA_N, 0x44);
		//mdelay(20);
		// 00: EXTMIC_P routed to Left MICPGA with 10K resistance	
		//codec->write (codec, L_MICPGA_N, 0x04);
		
		/* Micbias power up and 1.8V */
		codec->write (codec, ADC_CHN_ACONF2, 0x60);			
	}	
}
void connect_audio_output_route(struct snd_soc_codec *codec)
{
	if(!voice_mode_enable)
	{	  
		// Enabling the DAC_L to LINEOUT_L,DAC_R to LINEOUT_R
		codec->write (codec, DACL_2_LINEOUT_LPM, codec->read(codec, DACL_2_LINEOUT_LPM)|(0x80));
		codec->write (codec, DACR_2_LINEOUT_RPM, codec->read(codec, DACR_2_LINEOUT_RPM)|(0x80));
	  
		// Enabling the DAC_L to HPL,DAC_R to HPR
		codec->write (codec, DACL_2_HPL_VOL, codec->read(codec, DACL_2_HPL_VOL)|(0x80));
		codec->write (codec, DACR_2_HPR_VOL, codec->read(codec, DACR_2_HPR_VOL)|(0x80));
		// DAC_R to RECL
		codec->write (codec, DACL_2_RECL_VOL, codec->read(codec, DACL_2_RECL_VOL)|(0x80));
		codec->write (codec, DACR_2_RECL_VOL, codec->read(codec, DACR_2_RECL_VOL)|(0x80));

		/*Reduce Headphone pop*/
		codec->write (codec, LINEINL_2_HPL_VOL, 0x00);//LINEINL bypass to HPL(muted) for reduce DC offset
		codec->write (codec, LINEINR_2_HPR_VOL, 0x00);//LINEINL bypass to HPR(muted) for reduce DC offset
	}
	else
	{
#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
		// PGA_R to RECL
		codec->write (codec, PGAR_2_RECL_VOL, codec->read(codec, PGAR_2_RECL_VOL)|(0x80));

		// Enabling the PGA_R to LINEOUT_L,DAC_R to LINEOUT_L
		codec->write (codec, PGAR_2_LINEOUT_LPM, codec->read(codec, PGAR_2_LINEOUT_LPM)|(0x80));
		codec->write (codec, PGAR_2_LINEOUT_RPM, codec->read(codec, PGAR_2_LINEOUT_RPM)|(0x80));

		// Enabling the PGA_R to HPL,DAC_R to HPR
		codec->write (codec, PGAR_2_HPL_VOL, codec->read(codec, PGAR_2_HPL_VOL)|(0x80));	
		codec->write (codec, PGAR_2_HPR_VOL, codec->read(codec, PGAR_2_HPR_VOL)|(0x80));

		/*Reduce Headphone pop*/
		codec->write (codec, LINEINL_2_HPL_VOL, 0xF6);//LINEINL bypass to HPL(muted) for reduce DC offset
		codec->write (codec, LINEINR_2_HPR_VOL, 0xF6);//LINEINL bypass to HPR(muted) for reduce DC offset
#endif
		// DAC_R to RECL
		codec->write (codec, DACR_2_RECL_VOL, codec->read(codec, DACR_2_RECL_VOL)|(0x80));

		if(aic36_answer_func == 0)
		{
#ifdef UPLINK_CHANNEL_BYPASS_MODE
		// PGA_L to RECR
		codec->write (codec, PGAL_2_RECR_VOL, codec->read(codec, PGAL_2_RECR_VOL)|(0x80));
#else
		// DAC_L to RECL
		codec->write (codec, DACL_2_RECR_VOL, codec->read(codec, DACL_2_RECR_VOL)|(0x80));
#endif	 
		}else{
			// DAC_R to RECR
			codec->write (codec, DACR_2_RECR_VOL, codec->read(codec, DACR_2_RECR_VOL)|(0x80));
		}

		// Enabling the DAC_R to LINEOUT_L,DAC_R to LINEOUT_L
		codec->write (codec, DACR_2_LINEOUT_LPM, codec->read(codec, DACR_2_LINEOUT_LPM)|(0x80));	
		codec->write (codec, DACR_2_LINEOUT_RPM, codec->read(codec, DACR_2_LINEOUT_RPM)|(0x80));	
		
		// Enabling the DAC_R to HPL,DAC_R to HPR
		codec->write (codec, DACR_2_HPL_VOL, codec->read(codec, DACR_2_HPL_VOL)|(0x80));	
		codec->write (codec, DACR_2_HPR_VOL, codec->read(codec, DACR_2_HPR_VOL)|(0x80));	  
	 }	


}

void disconnect_audio_input_route(struct snd_soc_codec *codec)
{
	/*input mixer*/
	codec->write (codec, L_MICPGA_N, 0x00);
	codec->write (codec, L_MICPGA_P, 0x00);
	codec->write (codec, R_MICPGA_P, 0x00);
	codec->write (codec, R_MICPGA_N, 0x40);
}
void disconnect_audio_output_route(struct snd_soc_codec *codec)
{
	/*ouput mixer*/
	codec->write (codec, PGAL_2_LINEOUT_LPM, 0x00);  
	codec->write (codec, PGAL_2_LINEOUT_RPM, 0x00);  
	
	codec->write (codec, PGAL_2_HPL_VOL, 0x00);  
	codec->write (codec, PGAL_2_HPR_VOL, 0x00);  
	
	codec->write (codec, PGAL_2_RECL_VOL, 0x00);
	codec->write (codec, PGAL_2_RECR_VOL, 0x00);

	codec->write (codec, PGAR_2_LINEOUT_LPM, 0x00);  
	codec->write (codec, PGAR_2_LINEOUT_RPM, 0x00);  
	
	codec->write (codec, PGAR_2_HPL_VOL, 0x00);  
	codec->write (codec, PGAR_2_HPR_VOL, 0x00);  
	
	codec->write (codec, PGAR_2_RECL_VOL, 0x00);
	codec->write (codec, PGAR_2_RECR_VOL, 0x00);


	codec->write (codec, DACL_2_LINEOUT_LPM, 0x00);	
	codec->write (codec, DACL_2_LINEOUT_RPM, 0x00);	

	codec->write (codec, DACL_2_HPL_VOL, 0x00);	
	codec->write (codec, DACL_2_HPR_VOL, 0x00);	

	codec->write (codec, DACL_2_RECL_VOL, 0x00);
	codec->write (codec, DACL_2_RECR_VOL, 0x00);

	codec->write (codec, DACR_2_LINEOUT_LPM, 0x00);	
	codec->write (codec, DACR_2_LINEOUT_RPM, 0x00);	

	codec->write (codec, DACR_2_HPL_VOL, 0x00);	
	codec->write (codec, DACR_2_HPR_VOL, 0x00);	

	codec->write (codec, DACR_2_RECL_VOL, 0x00);
	codec->write (codec, DACR_2_RECR_VOL, 0x00);

}

static void
aic36_set_default_gain (struct snd_soc_codec *codec)
{

}

static void
aic36_set_bbout_gain (struct snd_soc_codec *codec)
{
	int value = 0;

	/* bb out Gain(0-0x9), increase*/
	value = codec->read (codec, RECR_OUT_VOL);
	value = 0x00 | (value & 0x0F);
	codec->write (codec, RECR_OUT_VOL, value);	
}

static void
aic36_set_phone_gain (struct snd_soc_codec *codec)
{
	int value = 0;
	struct aic36_priv *aic36= (struct aic36_priv *)dev_get_drvdata(codec->dev);
	
	if(voice_mode_enable)
	{
#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
		//Set PGAR to HP volume
		value = codec->read (codec, PGAR_2_HPL_VOL);
		value = (aic36->voice_gain[aic36->voice_volume] ) | (value & 0x80);
		codec->write (codec, PGAR_2_HPL_VOL, value);
		value = codec->read (codec, PGAR_2_HPR_VOL);
		value = (aic36->voice_gain[aic36->voice_volume] ) | (value & 0x80);
		codec->write (codec, PGAR_2_HPR_VOL, value);	
#else
		/* Left and Right DAC Volume */
		codec->write (codec, LDAC_VOL, 0);
		codec->write (codec, RDAC_VOL, aic36->voice_gain[aic36->voice_volume]); 
#endif

	}else{
		/* Left and Right MICPGA disable*/	
		codec->write (codec, L_MICPGA_V, 0x80);
		codec->write (codec, R_MICPGA_V, 0x80);
		
		value = codec->read (codec, HPL_OUT_VOL);
		value = 0x00 | (value & 0x0F);
		codec->write (codec, HPL_OUT_VOL, value);

		value = codec->read (codec, HPR_OUT_VOL);
		value = 0x00 | (value & 0x0F);
		codec->write (codec, HPR_OUT_VOL, value);		
#if 1	
		/*Left and Right Hardset Volume*/
		value = codec->read (codec, DACL_2_HPL_VOL);
		value = (aic36->music_volume) | (value & 0x80);//-9db
		codec->write (codec, DACL_2_HPL_VOL, value);

		value = codec->read (codec, DACR_2_HPR_VOL);
		value = (aic36->music_volume) | (value & 0x80);//-9db
		codec->write (codec, DACR_2_HPR_VOL, value);
#endif		
	}
}
static void

aic36_set_receiver_gain (struct snd_soc_codec *codec)
{
	int value = 0;
	struct aic36_priv *aic36= (struct aic36_priv *)dev_get_drvdata(codec->dev);
	
	if(voice_mode_enable)
	{
		value = codec->read (codec, RECL_OUT_VOL);
		value = 0x90 | (value & 0x0F);
		codec->write (codec, RECL_OUT_VOL, value);
#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
		// PGA_R to RECL
		value = codec->read (codec, PGAR_2_RECL_VOL);
		value = aic36->voice_gain[aic36->voice_volume] | (value & 0x80);
		codec->write (codec, PGAR_2_RECL_VOL, value);
#else
		/* Left and Right DAC Volume */
		codec->write (codec, LDAC_VOL, 0);
		codec->write (codec, RDAC_VOL, aic36->voice_gain[aic36->voice_volume]);
#endif			
	}else{
		/* Left and Right MICPGA disable*/	
		codec->write (codec, L_MICPGA_V, 0x80);
		codec->write (codec, R_MICPGA_V, 0x80);
	}
}
static void

aic36_set_spreaker_gain (struct snd_soc_codec *codec)
{
	int value = 0;
	struct aic36_priv *aic36= (struct aic36_priv *)dev_get_drvdata(codec->dev);

	if(voice_mode_enable)
	{
		value = codec->read (codec, LINEOUT_LPM_VOL);
		value = 0x00 | (value & 0x0F);
		codec->write (codec, LINEOUT_LPM_VOL, value);

		value = codec->read (codec, LINEOUT_RPM_VOL);
		value = 0x00 | (value & 0x0F);
		codec->write (codec, LINEOUT_RPM_VOL, value);	
		
#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
		//Set PGAR to HP volume
		value = codec->read (codec, PGAR_2_LINEOUT_LPM);
		value = (aic36->voice_gain[aic36->voice_volume] + 12) | (value & 0x80);//speaker 语音模式每级-6db
		codec->write (codec, PGAR_2_LINEOUT_LPM, value);
		value = codec->read (codec, PGAR_2_LINEOUT_RPM);
		value = (aic36->voice_gain[aic36->voice_volume] + 12) | (value & 0x80);//speaker 语音模式每级-6db
		codec->write (codec, PGAR_2_LINEOUT_RPM, value);	
#else
		/* Left and Right DAC Volume */
		codec->write (codec, LDAC_VOL, 0);
		codec->write (codec, RDAC_VOL, aic36->voice_gain[aic36->voice_volume]);
#endif

	}else{
		/* Left and Right MICPGA disable*/	
		codec->write (codec, L_MICPGA_V, 0x80);
		codec->write (codec, R_MICPGA_V, 0x80);

		value = codec->read (codec, LINEOUT_LPM_VOL);
		value = 0x00 | (value & 0x0F);
		codec->write (codec, LINEOUT_LPM_VOL, value);

		value = codec->read (codec, LINEOUT_RPM_VOL);
		value = 0x00 | (value & 0x0F);
		codec->write (codec, LINEOUT_RPM_VOL, value);			
#if 1	
		/*Left and Right Speaker Volume*/
		value = codec->read (codec, DACL_2_LINEOUT_LPM);
		value = (6*2) | (value & 0x80);//-6db
		codec->write (codec, DACL_2_LINEOUT_LPM, value);
	
		value = codec->read (codec, DACR_2_LINEOUT_RPM);
		value = (6*2) | (value & 0x80);//-6db
		codec->write (codec, DACR_2_LINEOUT_RPM, value);
#endif
	}
}

void set_audio_output_power(struct snd_soc_codec *codec, int power)
{
	if(power)
	{
		msleep(10); 
		/*set headset output*/
		if (aic36_phone_func)
		{
			aic36_set_phone_gain(codec);
			/* Power on HPL and HPR */ 
			//aic36_phone_mute(codec,false);
			aic36_phone_power(codec,true);
		}
		/*set lineout output*/
		if (aic36_lineout_func)
		{
			aic36_set_spreaker_gain(codec);
			/* Power on LINEOUT */ 
			aic36_spk_power(codec,true, true);
			//aic36_spk_mute(codec,false);
		}
		/*set receiver output*/
		if (aic36_recout_func)
		{
			aic36_set_receiver_gain(codec);
			/* Power on RECOUT	*/
			aic36_recl_power(codec,true);
			//aic36_recl_mute(codec,false);
		}
		/*set bb output*/		
		if (aic36_bbout_func)
		{
			aic36_set_bbout_gain(codec);
			/*Switch on Rec Right output */
			aic36_recr_power(codec,true);
			//aic36_recr_mute(codec,false);
		}
		if(!(aic36_phone_func || aic36_lineout_func || aic36_recout_func))
		{
			/*set default gain*/	
			aic36_set_default_gain(codec);			
		}
	}else{
		//aic36_spk_mute(codec,true);
		//aic36_phone_mute(codec,true);
		//aic36_recl_mute(codec,true);
		//aic36_recr_mute(codec,true);
		aic36_recr_power(codec,false);
		aic36_recl_power(codec,false);
		aic36_phone_power(codec,false);
		aic36_spk_power(codec,false, false);
		msleep(10);
	}
	
}

void set_receiver_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{	
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 0);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 0;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 1;
		aic36_phone_func = 0;
		aic36_lineout_func = 0;
		aic36_bbout_func = 0;

		set_audio_output_power(codec, 1);

	}
}

void set_speaker_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{	
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 0);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 0;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 1;
		aic36_bbout_func = 0;

		set_audio_output_power(codec, 1);

	}
}

void set_headset_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 0);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 0;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 0;
		aic36_bbout_func = 0;

		set_audio_output_power(codec, 1);
	
	}
}

void set_spreaker_headset_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 0);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 0;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 1;
		aic36_bbout_func=0;

		set_audio_output_power(codec, 1);

	}
}

void set_speaker_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{	
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 0); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 0;
	    aic36_intmic_func = 1;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 1;
		aic36_bbout_func = 1;

		set_audio_output_power(codec, 1);

	}

}
void set_receiver_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{

	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);

		set_audio_ldac_power(codec, 0);
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 1;
		aic36_phone_func = 0;
		aic36_lineout_func = 0;
		aic36_bbout_func=1;

		set_audio_output_power(codec, 1);

	}
}

void set_headset_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 0); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 0;
		aic36_bbout_func=1;

		set_audio_output_power(codec, 1);

	}
}

void set_spreaker_headset_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 0); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		/*input port*/
		aic36_linein_func = 1;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		
		/*output port*/
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 1;
		aic36_bbout_func=1;
		set_audio_output_power(codec, 1);

	}
}

void set_headphone_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 0); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 0;
		aic36_bbout_func=1;

		set_audio_output_power(codec, 1);

	}
}

void set_spreaker_headphone_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 0); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		aic36_answer_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 1;
		aic36_bbout_func=1;

		set_audio_output_power(codec, 1);

	}
}

void set_speaker_answer_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{	
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 0;
	      	aic36_intmic_func = 0;
		aic36_answer_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 1;
		aic36_bbout_func = 1;
		aic36_bbout_func = 1;
		
		set_audio_output_power(codec, 1);

	}

}
void set_receiver_answer_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{

	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);

		set_audio_ldac_power(codec, 1);
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 0;
		aic36_intmic_func = 0;
		aic36_answer_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 0;
		aic36_bbout_func = 1;

		set_audio_output_power(codec, 1);

	}
}

void set_headset_answer_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 0;
		aic36_intmic_func = 0;
		aic36_answer_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 0;
		aic36_bbout_func=1;

		set_audio_output_power(codec, 1);

	}
}

void set_spreaker_headset_answer_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		/*input port*/
		aic36_linein_func = 1;
		aic36_extmic_func = 0;
		aic36_intmic_func = 0;
		aic36_answer_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		
		/*output port*/
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 1;
		aic36_bbout_func=1;
		set_audio_output_power(codec, 1);

	}
}

void close_playback(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	set_audio_output_power(codec, 0);
	
	set_audio_ldac_power(codec, 0);
	set_audio_rdac_power(codec, 0);

	disconnect_audio_output_route(codec);
}

void close_capture(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{

	set_audio_ladc_power(codec, 0);
	set_audio_radc_power(codec, 0);

	disconnect_audio_input_route(codec);
}

void set_intmic_and_output_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		set_audio_ladc_power(codec, 1);

		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		connect_audio_input_route(codec);
	}
}
void set_extmic_and_output_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		set_audio_ladc_power(codec, 1);

		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		connect_audio_input_route(codec);
	}
}

void set_intmic_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 0); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 0;
		aic36_bbout_func=0;

		set_audio_output_power(codec, 1);

	}

}

void set_extmic_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);
	
		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 0); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);
	
		aic36_linein_func = 0;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 0;
		aic36_bbout_func=0;

		set_audio_output_power(codec, 1);

	}
}
void set_extmic_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);

		disconnect_audio_input_route(codec);
		
		aic36_linein_func = 1;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		connect_audio_input_route(codec);
	}
}
void set_intmic_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{	
	if(level == SND_SOC_BIAS_ON)
	{
		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);

		disconnect_audio_input_route(codec);
		
		aic36_linein_func = 1;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		connect_audio_input_route(codec);
	}
}

void set_receiver_and_intmic_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 1;
		aic36_phone_func = 0;
		aic36_lineout_func = 0;
		aic36_bbout_func = 0;

		set_audio_output_power(codec, 1);

	}

}
void set_receiver_and_extmic_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);
		
		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);
			
		aic36_linein_func = 0;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 1;
		aic36_phone_func = 0;
		aic36_lineout_func = 0;
		aic36_bbout_func=0;

		set_audio_output_power(codec, 1);

	}
}
void set_speaker_and_intmic_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 1;
		aic36_bbout_func = 0;

		set_audio_output_power(codec, 1);

	}

}
void set_speaker_and_extmic_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);
		
		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1); 		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);
			
		aic36_linein_func = 0;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 1;
		aic36_bbout_func=0;

		set_audio_output_power(codec, 1);

	}
}
void set_headset_and_intmic_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1);		
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);
			
		aic36_linein_func = 0;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 0;
		aic36_bbout_func=0;	
		set_audio_output_power(codec, 1);
			
	}
}
void set_headset_and_extmic_normal(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);

		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 0);
		set_audio_ldac_power(codec, 1);		
		set_audio_rdac_power(codec, 1);
			
		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 0;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 1;
		aic36_lineout_func = 0;
		aic36_bbout_func=0;
		
		set_audio_output_power(codec, 1);

	}
}
void set_speaker_and_intmic_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);


		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 0); 
		set_audio_rdac_power(codec, 1);

		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 0;
		aic36_intmic_func = 1;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 1;
		aic36_bbout_func=1;
		
		set_audio_output_power(codec, 1);

	}
}
void set_speaker_and_extmic_incall(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	if(level == SND_SOC_BIAS_ON)
	{
		if(codec->bias_level == level)
			set_audio_output_power(codec, 0);
 
		set_audio_ladc_power(codec, 1);
		set_audio_radc_power(codec, 1);
		set_audio_ldac_power(codec, 0);		
		set_audio_rdac_power(codec, 1);
			
		disconnect_audio_output_route(codec);
		disconnect_audio_input_route(codec);

		aic36_linein_func = 1;
		aic36_extmic_func = 1;
		aic36_intmic_func = 0;
		connect_audio_input_route(codec);
		connect_audio_output_route(codec);
		aic36_recout_func = 0;
		aic36_phone_func = 0;
		aic36_lineout_func = 1;
		aic36_bbout_func=1;

		set_audio_output_power(codec, 1);

	}
}

int set_playback_path(struct snd_soc_codec *codec,u8 playback_path,u8 capture_path, enum snd_soc_bias_level level)
{
	int ret = 0;
	dprintk ("%s() :playbackpath = %d,capture_path %d\n", __func__,playback_path,capture_path);

	switch(playback_path)
	{
	case PLAYBACK_REC_NORMAL:
	case PLAYBACK_REC_RING:
		switch(capture_path)
		{
			case CAPTURE_INT_MIC_NORMAL:
				Change_Music_Voice_Path(codec,VOICE_MODE);
				put_drc_mux(codec,1);
				
				set_receiver_and_intmic_normal(codec, level);
				break;
			case CAPTURE_EXT_MIC_NORMAL:
				Change_Music_Voice_Path(codec,VOICE_MODE);
				put_drc_mux(codec,1);
				
				set_receiver_and_extmic_normal(codec, level);
				break;
			default:
				Change_Music_Voice_Path(codec,MUSIC_MODE);
				put_drc_mux(codec,1);

				set_receiver_normal(codec, level);
				break;
		}
		break;
	case PLAYBACK_SPK_NORMAL:
	case PLAYBACK_SPK_RING:			
		switch(capture_path)
		{
			case CAPTURE_INT_MIC_NORMAL:
				Change_Music_Voice_Path(codec,VOICE_MODE);
				put_drc_mux(codec,1);
				
				set_speaker_and_intmic_normal(codec, level);
				break;
			case CAPTURE_EXT_MIC_NORMAL:
				Change_Music_Voice_Path(codec,VOICE_MODE);
				put_drc_mux(codec,1);
				
				set_speaker_and_extmic_normal(codec, level);
				break;
			default:
				Change_Music_Voice_Path(codec,MUSIC_MODE);
				put_drc_mux(codec,1);

				set_speaker_normal(codec, level);
				break;
		}
		break;
	case PLAYBACK_HP_NORMAL:
	case PLAYBACK_HP_RING:
		switch(capture_path)
		{
			case CAPTURE_INT_MIC_NORMAL:
				Change_Music_Voice_Path(codec,VOICE_MODE);
				put_drc_mux(codec,0);
			
				set_headset_and_intmic_normal(codec, level);
				break;
			case CAPTURE_EXT_MIC_NORMAL:
				Change_Music_Voice_Path(codec,VOICE_MODE);
				put_drc_mux(codec,0);
			
				set_headset_and_extmic_normal(codec, level);
				break;
			default:
				Change_Music_Voice_Path(codec,MUSIC_MODE);
				put_drc_mux(codec,0);
			
				set_headset_normal(codec, level);
				break;
		}
		break;
	case PLAYBACK_SPK_HP_NORMAL:
	case PLAYBACK_SPK_HP_RING:
		switch(capture_path)
		{
			case CAPTURE_INT_MIC_NORMAL:
				Change_Music_Voice_Path(codec,VOICE_MODE);
				put_drc_mux(codec,1);
				
				set_spreaker_headset_normal(codec, level);
			
				set_intmic_and_output_normal(codec, level);		

				break;
			case CAPTURE_EXT_MIC_NORMAL:
				Change_Music_Voice_Path(codec,VOICE_MODE);
				put_drc_mux(codec,1);

				set_spreaker_headset_normal(codec, level);
				
				set_extmic_and_output_normal(codec, level);
				
				break;
			default:
				Change_Music_Voice_Path(codec,MUSIC_MODE);
				put_drc_mux(codec,1);
			
				set_spreaker_headset_normal(codec, level);
				break;
		}
		break;
	case PLAYBACK_SPK_INCALL:
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,1);
		
		set_speaker_incall(codec, level);

		break;
	case PLAYBACK_HP_INCALL:
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,0);
		
		set_headphone_incall(codec, level);
		break;
	case PLAYBACK_SPK_HP_INCALL:		
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,1);
		
		set_spreaker_headphone_incall(codec, level);
		break;
			
	case PLAYBACK_HS_INCALL:
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,0);
		
		set_headset_incall(codec, level);
		break;

	case PLAYBACK_SPK_HS_INCALL:		
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,1);
		
		set_spreaker_headset_incall(codec, level);
		break;


	case PLAYBACK_REC_INCALL:			
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,1);
		
		set_receiver_incall(codec, level);

		break;
	case PLAYBACK_SPK_ANSWER_INCALL:
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,1);
		
		set_speaker_answer_incall(codec, level);
		break;
	case PLAYBACK_REC_ANSWER_INCALL:
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,1);
		set_receiver_answer_incall(codec, level);
		break;		
	case PLAYBACK_HP_ANSWER_INCALL:
	case PLAYBACK_HS_ANSWER_INCALL:
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,1);
		set_headset_answer_incall(codec, level);
		break;		
	case PLAYBACK_SPK_HS_ANSWER_INCALL:
	case PLAYBACK_SPK_HP_ANSWER_INCALL:
		Change_Music_Voice_Path(codec,VOICE_MODE);
		put_drc_mux(codec,1);
		set_spreaker_headset_answer_incall(codec, level);
		break;
	case PLAYBACK_NONE:
		switch(capture_path)
		{
			case CAPTURE_INT_MIC_NORMAL:
				set_intmic_normal(codec, level);
				break;
			case CAPTURE_EXT_MIC_NORMAL:
				set_extmic_normal(codec, level);
				break;
			default:
				break;
		}
			break;
	default:			
		printk ("%s() :invalid path = %d\n", __func__,playback_path);
			ret = -1;
			break;			
	}
	return ret;	
}

int set_capture_path(struct snd_soc_codec *codec,u8 playback_path,u8 capture_path, enum snd_soc_bias_level level)
{
	int ret = 0;
	dprintk ("%s() :playbackpath = %d,capture_path %d\n", __func__,playback_path,capture_path);
	switch(capture_path)
	{
		case CAPTURE_INT_MIC_NORMAL:
			switch(playback_path)
			{
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING:
				Change_Music_Voice_Path(codec,VOICE_MODE);
			
				set_intmic_and_output_normal(codec, level);			
				break;
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:			
				Change_Music_Voice_Path(codec,VOICE_MODE);

				set_intmic_and_output_normal(codec, level);	
				break;
			case PLAYBACK_NONE:
				Change_Music_Voice_Path(codec,VOICE_MODE);
			
				set_intmic_normal(codec, level);	
				break;
			default:
				break;			
			}
			break;			
		case CAPTURE_EXT_MIC_NORMAL:
			switch(playback_path)
			{
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING:			
				Change_Music_Voice_Path(codec,VOICE_MODE);
			
				set_extmic_and_output_normal(codec, level);			
				break;
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:				
				Change_Music_Voice_Path(codec,VOICE_MODE);
			
				set_extmic_and_output_normal(codec, level);	
				break;
			case PLAYBACK_NONE:
				Change_Music_Voice_Path(codec,VOICE_MODE);	
				
				set_extmic_normal(codec, level);	
				break;
			default:
				break;			
			}
			break;			
		case CAPTURE_INT_MIC_INCALL:
			switch(playback_path)
			{
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING:			
				Change_Music_Voice_Path(codec,VOICE_MODE);
		
				set_intmic_incall(codec, level);			
				break;
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:			
				Change_Music_Voice_Path(codec,VOICE_MODE);

				set_intmic_incall(codec, level);	
				break;
			default:
				break;			
			}
			break;

		case CAPTURE_EXT_MIC_INCALL:	
			switch(playback_path)
			{
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING:				
				Change_Music_Voice_Path(codec,VOICE_MODE);
		
				set_extmic_incall(codec, level);			
				break;
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:			
				Change_Music_Voice_Path(codec,VOICE_MODE);
		
				set_extmic_incall(codec, level);	
				break;
			default:
				break;			
			}
			break;
		case CAPTURE_NONE:
		switch(playback_path)
		{
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING: 
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:
				close_capture(codec, level);
				break;			
			default:
				break;
			}
			break;
		default:			
			break;			
	}
	return ret;	
}

