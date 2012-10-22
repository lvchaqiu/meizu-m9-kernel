 /*
 * linux/sound/soc/codecs/tlv320aic36.c
 *
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
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
 * Rev 0.1   ASoC driver support    Mistral         16-11-2009
 *   
 * Rev 0.2   ASoC driver support    Mistral         11-22-2009
 *           - Added amixer controler for "Program Register"
 *           - Enable the Mini-DSP support
 *
 * Rev 0.3   ASoC driver support    Mistral         15-22-2009
 *           - Added "Mic-Bias" Amixer Controller
 *
 * Rev 0.4   ASoC driver support    Mistral         15-05-2010
 *           - Added 8kHz MiniDSP Support
 *
 * Rev 0.5   ASoC driver support    WenbinWu         20 -09-2010
 *           - Added Codec Slave Support
 *           - Added Voice Bypass Support
 */

/***************************** INCLUDES ************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <sound/tlv.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "tlv320aic36.h"


/* enable debug prints in the driver */
//#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define dprintk(x...) 	printk(x)
#else
#define dprintk(x...)
#endif

#define HP_MIXER_NORMAL_VALUE				6*2			//-9db
#define HP_MIXER_SUPPRESS_NOISE_VALUE	18*2		//-15db
/*
 ***************************************************************************** 
 * Global Variable
 ***************************************************************************** 
 */
static u8 aic36_reg_ctl;

u8 voice_mode_enable = 0;
volatile u8 standby_mode_enable = 0;
bool g_bBiasOnOff = false;


extern int set_speaker_OnOff(bool bEn);

//static struct snd_soc_device *aic36_socdev = NULL;
static struct snd_soc_codec *aic36_soc_codec = NULL;
unsigned int aic36_initialized = 0;

static void aic36_ext_mic_bias_func(struct work_struct *work);
static DECLARE_WORK(ext_mic_bias_work,aic36_ext_mic_bias_func);

extern int aic36_recr_mute(struct snd_soc_codec *codec,bool bMute);
extern int aic36_spk_mute(struct snd_soc_codec *codec,bool bMute);
extern int aic36_phone_mute(struct snd_soc_codec *codec,bool bMute);
extern int aic36_recl_mute(struct snd_soc_codec *codec,bool bMute);

#ifdef CONFIG_MINI_DSP
extern int aic36_minidsp_program (struct snd_soc_codec *codec);
extern void aic36_add_minidsp_controls (struct snd_soc_codec *codec);
#endif
extern int put_burst_minidsp_mode (struct snd_soc_codec *codec,u8 uVoice, u8 force);

//------------------------------------------------
// Define the path of Playback & Capture
//------------------------------------------------
static const char *playback_path_name[] = { "Speaker  Nomal","Headphone  Nomal","Receiver  Nomal","Spk_Hp Nomal",
									"Speaker incall","Headset incall","Receiver incall","Spk_Hs incall",
									"Speaker ringtone","Headphone ringtone","Receiver ringtone","Spk_Hp ringtone",
									"Headphone incall","Spk_Hp incall", "Spk answer incall", "Headset answer incall",
									"Receiver answer incall", "Spk_Hs answer incall", "Headphone answer incall",
									"Spk_Hp answer incall",
									"None",};
static const char *capture_path_name[] = { "Int_Mic Nomal","Ext_Mic Nomal","Int_Mic incall","Ext_Mic incall","None",};

static const struct soc_enum path_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(playback_path_name), playback_path_name),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(capture_path_name), capture_path_name),
};
extern int set_playback_path(struct snd_soc_codec *codec,u8 playback_path,u8 capture_path,  enum snd_soc_bias_level level);
extern int set_capture_path(struct snd_soc_codec *codec,u8 playback_path,u8 capture_path, enum snd_soc_bias_level level);

#define SOC_SINGLE_AIC36(xname) \
{\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = __new_control_info, .get = __new_control_get,\
	.put = __new_control_put, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
}

#define SOC_DOUBLE_R_AIC36(xname, reg_left, reg_right, xshift, xmax, xinvert) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.info = snd_soc_info_volsw_2r, \
	.get = snd_soc_get_volsw_2r_aic36, .put = snd_soc_put_volsw_2r_aic36, \
	.private_value = (unsigned long)&(struct soc_mixer_control) \
	{.reg = reg_left, .rreg = reg_right, .shift = xshift, \
	.max = xmax, .invert = xinvert} }

#define SOC_SINGLE_TLV_AIC36(xname, reg, shift, max, invert, tlv_array) \
	{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |\
			 SNDRV_CTL_ELEM_ACCESS_READWRITE,\
		.tlv.p = (tlv_array), \
		.info = snd_soc_info_volsw, .get = snd_soc_get_volsw_dacr_aic36,\
		.put = snd_soc_put_volsw_dacr_aic36, \
		.private_value =  SOC_SINGLE_VALUE(reg, shift, max, invert) }
#define SOC_SINGLE_NOISE_AIC36(xname, reg, shift, max, invert) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_aic36_noise_info, .get = snd_soc_get_aic36_noise,\
	.put = snd_soc_put_aic36_noise, \
	.private_value =  SOC_SINGLE_VALUE(reg, shift, max, invert) }


/*
 *	AIC36 register cache
 *	We are caching the registers here.
 *	NOTE: In AIC36, there are 61 pages of 128 registers supported.
 * 	The following table contains the page0, page1 and page2 registers values.
 */
static const u8 aic36_reg[AIC36_CACHEREGNUM] = {
	/* Page 0 Registers */
	/* 0  */ 0x00, 0x00, 0x12, 0x00, 0x00, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x01, 0x00, 0x80, 0x80,
	/* 10 */ 0x08, 0x00, 0x01, 0x01, 0x80, 0x80, 0x04, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00,
	/* 20 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 30 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x55, 0x55, 0x00, 0x00,
	0x00, 0x01, 0x01, 0x00, 0x14,
	/* 40 */ 0x0c, 0x00, 0x00, 0x00, 0x6f, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xee, 0x10, 0xd8, 0x7e, 0xe3,
	/* 50 */ 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 60 */ 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 70 */ 0x00, 0x00, 0x10, 0x32, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x12, 0x02,
	/* Page 1 Registers */
	/* 0 */ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 10 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 20 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 30 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x80, 0x80, 0x00, 0x00, 0x00,
	/* 40 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 50 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 60 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 70 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* Page 2 Registers */
	/* 0 */ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 10 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 20 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 30 */ 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 40 */ 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 50 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00,
	/* 60 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* 70 */ 0x01, 0x00, 0x0b, 0x0b, 0x07, 0x00, 0x0f, 0x00, 0x00, 0x07, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

int snd_soc_aic36_noise_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int platform_max;

	if (!mc->platform_max)
		mc->platform_max = mc->max;
	platform_max = mc->platform_max;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = platform_max;
	return 0;
}


/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_get_aic36_noise
 * Purpose  : Callback to get the value of a double mixer control that spans
 *            two registers.
 *
 *----------------------------------------------------------------------------
 */
int
snd_soc_get_aic36_noise (struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);

	ucontrol->value.integer.value[0] = aic36->music_volume;
	return 0;
}


/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_put_aic36_noise
 * Purpose  : Callback to set the value of a double mixer control that spans
 *            two registers.
 *
 *----------------------------------------------------------------------------
 */
int snd_soc_put_aic36_noise (struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	int reg = DACL_2_HPL_VOL;
	int reg2 = DACR_2_HPR_VOL;
	int err;
	unsigned short val;
	int on;
	unsigned int val_mask;

	on = ucontrol->value.integer.value[0];
	if (on) {
		val = HP_MIXER_SUPPRESS_NOISE_VALUE;//-18db
	} else {
		val = HP_MIXER_NORMAL_VALUE; //-9db
	}
	val_mask = 0x7F;
	if ((err = snd_soc_update_bits (codec, reg, val_mask, val)) < 0) {
		printk ("Error while updating bits\n");
		return err;
	}

	err = snd_soc_update_bits (codec, reg2, val_mask, val);

	aic36->music_volume = val;

	return err;
}

/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_get_volsw_2r_aic36
 * Purpose  : Callback to get the value of a double mixer control that spans
 *            two registers.
 *
 *----------------------------------------------------------------------------
 */
int snd_soc_get_volsw_2r_aic36 (struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *) kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int reg = mc->reg;
	int reg2 = mc->rreg;
	int mask;
	int shift;
	unsigned short val, val2;

	if (strcmp (kcontrol->id.name, "PCM Playback Volume") == 0) {
		mask = 0xFF;
		shift = 0;
	} else if (strcmp (kcontrol->id.name, "PGA Capture Volume") == 0) {
		mask = 0x7F;
		shift = 0;
	} else {
		printk ("Invalid kcontrol name\n");
		return -1;
	}

	val = (snd_soc_read (codec, reg) >> shift) & mask;
	val2 = (snd_soc_read (codec, reg2) >> shift) & mask;

	if (strcmp (kcontrol->id.name, "PCM Playback Volume") == 0)	{
		ucontrol->value.integer.value[0] =
			(val <= 48) ? (val + 127) : (val - 129);
		ucontrol->value.integer.value[1] =
			(val2 <= 48) ? (val2 + 127) : (val2 - 129);
	} else if (strcmp (kcontrol->id.name, "PGA Capture Volume") == 0) {
		ucontrol->value.integer.value[0] =
			(val <= 38) ? (val + 25) : (val - 103);
		ucontrol->value.integer.value[1] =
			(val2 <= 38) ? (val2 + 25) : (val2 - 103);
	}

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_put_volsw_2r_aic36
 * Purpose  : Callback to set the value of a double mixer control that spans
 *            two registers.
 *
 *----------------------------------------------------------------------------
 */
int snd_soc_put_volsw_2r_aic36 (struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *) kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int reg = mc->reg;
	int reg2 = mc->rreg;
	int err;
	unsigned short val, val2, val_mask;

	val = ucontrol->value.integer.value[0];
	val2 = ucontrol->value.integer.value[1];

	if (strcmp (kcontrol->id.name, "PCM Playback Volume") == 0)	{
		val = (val >= 127) ? (val - 127) : (val + 129);
		val2 = (val2 >= 127) ? (val2 - 127) : (val2 + 129);
		val_mask = 0xFF;		/* 8 bits */
	} else if (strcmp (kcontrol->id.name, "PGA Capture Volume") == 0) {
		val = (val >= 25) ? (val - 25) : (val + 103);
		val2 = (val2 >= 25) ? (val2 - 25) : (val2 + 103);
		val_mask = 0x7F;		/* 7 bits */
	} else {
		printk ("Invalid control name\n");
		return -1;
	}

	if ((err = snd_soc_update_bits (codec, reg, val_mask, val)) < 0) {
		printk ("Error while updating bits\n");
		return err;
	}

	err = snd_soc_update_bits (codec, reg2, val_mask, val2);
	return err;
}

/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_get_volsw_dacr_aic36
 * Purpose  : Callback to get the value of a double mixer control that spans registers.
 *
 *----------------------------------------------------------------------------
 */
int snd_soc_get_volsw_dacr_aic36(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *) kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	int reg = mc->reg;
	int mask;
	int shift;
	unsigned short val;
	int i=0;

#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
	reg = mc->reg;

	mask = 0x7F;
	shift = 0;
 
	val = (snd_soc_read (codec, PGAR_2_RECL_VOL) >> shift) & mask;
	for (i = 0; i < AIC36_VOICE_LEVEL_MAX+1; i++) {
		if (val < aic36->voice_gain[i]) {
			if (i == 0)
				val = 0;
			else
				val = i-1;
			break;
		}
	}
	if (i > AIC36_VOICE_LEVEL_MAX)
		val = AIC36_VOICE_LEVEL_MAX;
	if (mc->invert)
		val = AIC36_VOICE_LEVEL_MAX-val;

	ucontrol->value.integer.value[0] = val;
#else
	val = (snd_soc_read (codec, reg) >> shift) & mask;

	for (i = 0; i < AIC36_VOICE_LEVEL_MAX+1; i++) {
		if(val < aic36->voice_gain[i]) {
			if(i==0)
				val = 0;
			else
				val = i-1;
			break;
		}
	}
	if (i > AIC36_VOICE_LEVEL_MAX)
		val = AIC36_VOICE_LEVEL_MAX;

	if (mc->invert)
		val = AIC36_VOICE_LEVEL_MAX-val;

	ucontrol->value.integer.value[0] = val;
#endif
	pr_debug("get vioce level = %d\n", val); 

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_put_volsw_dacr_aic36
 * Purpose  : Callback to set the value of a double mixer control that spans  registers.
 *
 *----------------------------------------------------------------------------
 */
int
snd_soc_put_volsw_dacr_aic36 (struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *) kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	int reg = mc->reg;
	int err = 0;
	unsigned short val, val_mask;

	val = ucontrol->value.integer.value[0];

#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
	reg = mc->reg;

	val = (val & 0xf);
	val_mask = 0x7f;
	if(val<0 || val>AIC36_VOICE_LEVEL_MAX)
		return 0;

	pr_debug("set vioce level = %d\n", val);	

	if(mc->invert)
		val = AIC36_VOICE_LEVEL_MAX-val;
	/*speaker volume*/
	if ((err = snd_soc_update_bits (codec, PGAR_2_LINEOUT_LPM, val_mask, aic36->voice_gain[val] + 12)) < 0) {
		pr_err("Error while updating bits\n");
		return err;
	}
	if ((err = snd_soc_update_bits (codec, PGAR_2_LINEOUT_RPM, val_mask, aic36->voice_gain[val] + 12)) < 0) {//speaker -6db
		pr_err("Error while updating bits\n");
		return err;
	}
	/*headset volume*/
	if ((err = snd_soc_update_bits (codec, PGAR_2_HPL_VOL, val_mask, aic36->voice_gain[val] )) < 0) {
		pr_err("Error while updating bits\n");
		return err;
	}
	if ((err = snd_soc_update_bits (codec, PGAR_2_HPR_VOL, val_mask, aic36->voice_gain[val] )) < 0)	{
		pr_err("Error while updating bits\n");
		return err;
	}
	/*receiver volume*/
	if ((err = snd_soc_update_bits (codec, PGAR_2_RECL_VOL, val_mask, aic36->voice_gain[val])) < 0)	{
		pr_err("Error while updating bits\n");
		return err;
	}
	aic36->voice_volume = val;
#else
	 val_mask = 0xFF;	   /* 8 bits */

	 if(val<0 || val>AIC36_VOICE_LEVEL_MAX)
		 return 0;

	 pr_debug("set vioce level = %d\n", val);		 

	 if(mc->invert)
		 val = AIC36_VOICE_LEVEL_MAX-val;
	 /*receiver volume*/
	 if ((err = snd_soc_update_bits (codec, reg, val_mask, aic36->voice_gain[val])) < 0) {
		 pr_err("Error while updating bits\n");
		 return err;
	 }
	 aic36->voice_volume = val;  	 
#endif
  return err;
}


/*
 *----------------------------------------------------------------------------
 * Function : aic36_change_page
 * Purpose  : This function is to switch between page 0 and page 1.
 *            
 *----------------------------------------------------------------------------
 */
int aic36_change_page (struct snd_soc_codec *codec, u8 new_page)
{
	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(codec->dev);
	u8 data[2];

	data[0] = 0;
	data[1] = new_page;
	aic36->page_no = new_page;

	if (codec->hw_write (codec->control_data, data, 2) != 2)
	{
		printk ("Error in changing page to 1\n");
		return -1;
	}
	dprintk("w 30 00 %.2x\n", aic36->page_no);
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_read_reg_cache
 * Purpose  : This function is to read aic36 register cache
 *            
 *----------------------------------------------------------------------------
 */
static inline u8 aic36_read_reg_cache(struct snd_soc_codec *codec,u16 reg)
{
	u8 *cache = codec->reg_cache;

	if (reg >= AIC36_CACHEREGNUM) {
		return 0x00;
	}
	return cache[reg];
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_write_reg_cache
 * Purpose  : This function is to write aic36 register cache
 *            
 *----------------------------------------------------------------------------
 */
static inline void
aic36_write_reg_cache (struct snd_soc_codec *codec, u16 reg, u8 value)
{
	u8 *cache = codec->reg_cache;

	if (reg >= AIC36_CACHEREGNUM) {
		return;
	}
	cache[reg] = value;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_read
 * Purpose  : This function is to read the aic36 register space.
 *            
 *----------------------------------------------------------------------------
 */
unsigned int aic36_read (struct snd_soc_codec * codec, unsigned int reg)
{
	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(codec->dev);
	struct i2c_client *client = codec->control_data;
	struct i2c_msg msg[2];
	u8 page = reg / 128;
	u8 value1[2];
	u8 value2[2];
	int err;

	if ((page == 0) || (page == 1) || (page == 2)) {
		dprintk("%s:page = %d reg = %d data = 0x%.2X\n",__func__, page, reg % 128,aic36_read_reg_cache(codec, reg));
		return aic36_read_reg_cache(codec, reg);
	}

	reg = reg % 128;

	if (aic36->page_no != page)
	{
		aic36_change_page (codec, page);
	}
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = value1; 
	value1[0] = reg;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = value2; 
	value2[0] = 0;

    /*standard way of calling things */	
	err = i2c_transfer(client->adapter, msg, 2);
	if (err >= 0) {
		printk("%s:page = %d reg = %.2d value = 0x%.2x\n",__func__, page, reg,value2[0]);	
		return value2[0];	/* Returns here on success */
	}
	printk ("Error in read register: page=%d reg=%d\n", page, reg);
	return 0;
}
/*
 *----------------------------------------------------------------------------
 * Function : aic36_read
 * Purpose  : This function is to read the aic36 register space.
 *            
 *----------------------------------------------------------------------------
 */
unsigned int aic36_read_status(struct snd_soc_codec * codec, unsigned int reg)
{
  	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(codec->dev);
	struct i2c_client *client = codec->control_data;
	struct i2c_msg msg[2];
	u8 page = reg / 128;
	u8 value1[2];
	u8 value2[2];
	int err;
	
	reg = reg % 128;

	if (aic36->page_no != page)
		aic36_change_page (codec, page);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = value1; 
	value1[0] = reg;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = value2; 
	value2[0] = 0;

    /*standard way of calling things */	
	err = i2c_transfer(client->adapter, msg, 2);
	if (err >= 0) {
		dprintk("%s:page = %d reg = %.2d value = 0x%.2x\n",__func__, page, reg,value2[0]);	
		return value2[0];	/* Returns here on success */
	}
	dprintk("Error in read register: page=%d reg=%d\n", page, reg);
	return 0;
}
/*
 *----------------------------------------------------------------------------
 * Function : aic36_write
 * Purpose  : This function is to write to the aic36 register space.
 *            
 *----------------------------------------------------------------------------
 */
int aic36_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(codec->dev);
	u8 data[2];
	u8 page;

	page = reg / 128;
	data[AIC36_REG_OFFSET_INDEX] = reg % 128;

	dprintk("%s:page = %d reg = %d value = 0x%X \n",__func__, page, reg % 128,value);

	/* data is
	 *   D15..D8 aic36 register offset
	 *   D7...D0 register data
	 */
	data[AIC36_REG_DATA_INDEX] = value & AIC36_8BITS_MASK;
	if ( page == 0 && data[0] == 50)
		return 0;

	if ((page == 0) || (page == 1) || (page == 2))
		aic36_write_reg_cache (codec, reg, value);

	if (aic36->page_no != page)
		aic36_change_page (codec, page);

	if (codec->hw_write (codec->control_data, data, 2) != 2) {
		printk ("Error in i2c write\n");
		return -EIO;
	}
	return 0;
}
int aic36_read_overflow_status(struct snd_soc_codec *codec)
{
	u8 reg=DAC_STICKY_REG, value=0, value1=0;

	/*read overflow flags */
	value = aic36_read_status(codec, reg);
	
	/*write back to clear overflow flags*/
	aic36_write(codec, reg, value);
	msleep(50);

	/*read overflow flags again*/
	value1 = aic36_read_status(codec, reg);

	dprintk("%s: overflow flag BF=0x%.2x, AF= 0x%.2x\n", __func__, value, value1);
	return value;
}
int aic36_reset_cache(struct snd_soc_codec *codec)
{
	if (codec->reg_cache != NULL) {
		memcpy(codec->reg_cache, aic36_reg, sizeof (aic36_reg));
		return 0;
	}

	codec->reg_cache = kmemdup(aic36_reg, sizeof (aic36_reg), GFP_KERNEL);
	if (codec->reg_cache == NULL) {
		printk (KERN_ERR "aic36: kmemdup failed\n");
		return -ENOMEM;
	}

	return 0;
}
/*
 *----------------------------------------------------------------------------
 * Function : __new_control_info
 * Purpose  : This function is to initialize data for new control required to 
 *            program the AIC36 registers.
 *            
 *----------------------------------------------------------------------------
 */
static int __new_control_info (struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65535;

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_get
 * Purpose  : This function is to read data of new control for 
 *            program the AIC36 registers.
 *            
 *----------------------------------------------------------------------------
 */
static int __new_control_get (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	u32 val;

	val = aic36_read (codec, aic36_reg_ctl);
	ucontrol->value.integer.value[0] = val;


	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put
 * Purpose  : new_control_put is called to pass data from user/application to
 *            the driver.
 * 
 *----------------------------------------------------------------------------
 */
static int
__new_control_put (struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(codec->dev);

	u32 data_from_user = ucontrol->value.integer.value[0];
	u8 data[2];

	aic36_reg_ctl = data[0] = (u8) ((data_from_user & 0xFF00) >> 8);
	data[1] = (u8) ((data_from_user & 0x00FF));

	if (!data[0])
		aic36->page_no = data[1];

	printk ("reg = %d val = %x\n", data[0], data[1]);

	if (codec->hw_write (codec->control_data, data, 2) != 2) {
		printk ("Error in i2c write\n");
		return -EIO;
	}

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_set_bias_on
 * Purpose  : This function is to turn on codec.
 *            
 *----------------------------------------------------------------------------
 */
void aic36_set_bias_on (struct snd_soc_codec *codec)
{
	volatile u8 value;
  	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(codec->dev);
	
	aic36_change_page (codec, 0);

	if (standby_mode_enable == 1) {
		standby_mode_enable = 0;
		/*Limit LDO current*/
		value = aic36_read (codec, PMU_CONF_REG5);
		value |= 0x07;
		aic36_write (codec, PMU_CONF_REG5, value);

		/* Power up Charge Pump Status */
		value = aic36_read (codec, PMU_CONF_REG1);
		value |= 0x01;
		aic36_write (codec, PMU_CONF_REG1, value);
		/* Power up ADC and  DAC LDO*/
		aic36_write (codec, PMU_CONF_REG4, 0x00);
		/* Power up bgap  */
		value = aic36_read (codec, PMU_CONF_REG6);
		value &= ~0x08;
		aic36_write (codec, PMU_CONF_REG6, value);
		/* Switch on PLL */
		value = aic36_read (codec, CLK_REG_2);
		aic36_write (codec, CLK_REG_2, (value | ENABLE_PLL));
		/* Switch on NDAC Divider */
		value = aic36_read (codec, NDAC_CLK_REG_6);
		aic36_write (codec, NDAC_CLK_REG_6, value | ENABLE_NDAC);
		/* Switch on MDAC Divider */
		value = aic36_read (codec, MDAC_CLK_REG_7);
		aic36_write (codec, MDAC_CLK_REG_7, value | ENABLE_MDAC);
		/* Switch on NADC Divider */
		value = aic36_read (codec, NADC_CLK_REG_8);
		aic36_write (codec, NADC_CLK_REG_8, value | ENABLE_MDAC);
		/* Switch on MADC Divider */
		value = aic36_read (codec, MADC_CLK_REG_9);
		aic36_write (codec, MADC_CLK_REG_9, value | ENABLE_MDAC);	

		/* Power up Left DAC channel*/
		value = codec->read (codec, DAC_CHN_REG);
		value |= 0xc0;	
		codec->write (codec, DAC_CHN_REG, value);
#ifdef CONFIG_TLV320AIC36_MASTER
		/* Switch on BCLK_N Divider */
		value = aic36_read (codec, CLK_REG_11);
		aic36_write (codec, CLK_REG_11, value | ENABLE_BCLK);
#endif
		msleep(10);
	}
	/*set path and output power*/
	set_playback_path(codec, aic36->playback_path, aic36->capture_path, SND_SOC_BIAS_ON);
	
	/*Unlimit LDO current*/
	value = aic36_read (codec, PMU_CONF_REG5);
	value &= ~0x07;
	aic36_write (codec, PMU_CONF_REG5, value);
	msleep(25);
}
/*
 *----------------------------------------------------------------------------
 * Function : aic36_set_bias_off
 * Purpose  : This function is to turn off codec.
 *            
 *----------------------------------------------------------------------------
 */
void aic36_set_bias_off (struct snd_soc_codec *codec)
{
	volatile u8 value;
	int count=5;
	
	aic36_change_page(codec, 0);

	if (standby_mode_enable == 0) {
		standby_mode_enable = 1;

		/* Power down HPL and HPR */ 
		value = aic36_read (codec, HPL_OUT_VOL);
		value &= ~0x01;  
		aic36_write (codec, HPL_OUT_VOL, value);
		value = aic36_read (codec, HPR_OUT_VOL);
		value &= ~0x01;  
		aic36_write (codec, HPR_OUT_VOL, value);
		
		/* Power down RECOUT  */ 
		value = aic36_read (codec, RECL_OUT_VOL);
		value &= ~0x01;  
		aic36_write (codec, RECL_OUT_VOL, value);
		value = aic36_read (codec, RECR_OUT_VOL);
		value &= ~0x01;  
		aic36_write (codec, RECR_OUT_VOL, value);

		/* Power down LINEOUT */ 
		set_speaker_OnOff(false);
		value = aic36_read (codec, LINEOUT_LPM_VOL);
		value &= ~0x01;  
		aic36_write (codec, LINEOUT_LPM_VOL, value);
		value = aic36_read (codec, LINEOUT_RPM_VOL);
		value &= ~0x01;  
		aic36_write (codec, LINEOUT_RPM_VOL, value);
		mdelay(20);

		/* Soft Stepping Enabled and Left and Right DAC Channel Powered Down  */
		aic36_write (codec, DAC_CHN_REG, 0x16);

		aic36_write (codec, DAC_PWR, 0x00);
		do {
			value = aic36_read_status(codec, DAC_STATUS_REG);
			if(!(value & 0x88))
				break;
			msleep(10);
			count --;
		} while(count>0);		
		/*Mute ADC*/
		aic36_write (codec, ADC_FGA, 0x88);
		/*Power down  ADC*/
		value = aic36_read (codec, ADC_REG_1);
		value &= ~0xC0;
		aic36_write (codec, ADC_REG_1, value);

		do {
			value = aic36_read_status(codec, ADC_STATUS_REG);
			if(!(value & 0x44))
				break;
			msleep(10);
			count --;
		} while(count>0);		

		/* Switch off MDAC Divider */
		value = aic36_read (codec, MDAC_CLK_REG_7);
		aic36_write (codec, MDAC_CLK_REG_7, value & ~ENABLE_MDAC);

		/* Switch off NDAC Divider */
		value = aic36_read (codec, NDAC_CLK_REG_6);
		aic36_write (codec, NDAC_CLK_REG_6, value & ~ENABLE_NDAC);

		/* Switch off MADC Divider */
		value = aic36_read (codec, MADC_CLK_REG_9);
		aic36_write (codec, MADC_CLK_REG_9, value & ~ENABLE_MDAC);

		/* Switch off NADC Divider */
		value = aic36_read (codec, NADC_CLK_REG_8);
		aic36_write (codec, NADC_CLK_REG_8, value & ~ENABLE_NDAC);

		/* Switch off BCLK_N Divider */
		value = aic36_read (codec, CLK_REG_11);
		aic36_write (codec, CLK_REG_11, value & ~ENABLE_BCLK);
		/* Switch OFF the PLL */
		value = aic36_read (codec, CLK_REG_2);
		aic36_write (codec, CLK_REG_2, (value & ~ENABLE_PLL));
		
		/*Left Right DAC mute*/
		aic36_write (codec, DAC_MUTE_CTRL_REG, 0x0C);

		/* Power down bgap Status */
		value = aic36_read (codec, PMU_CONF_REG6);
		value |= 0x08;
		aic36_write (codec, PMU_CONF_REG6, value);
		
		/* Power down ADC and DAC LDO*/
		aic36_write (codec, PMU_CONF_REG4, 0x07);
		
		/* Power down Charge Pump Status */
		value = aic36_read (codec, PMU_CONF_REG1);
		value &= ~0x01;
		aic36_write (codec, PMU_CONF_REG1, value);	
	}
	aic36_read_overflow_status(codec);
}
/*
 *----------------------------------------------------------------------------
 * Function : aic36_set_bias_level
 * Purpose  : This function is to get triggered when dapm events occurs.
 *            
 *----------------------------------------------------------------------------
 */
int aic36_set_bias_level (struct snd_soc_codec *codec,
		      enum snd_soc_bias_level level)
{
	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(codec->dev);

	pr_info("++%s ... %s\r\n", __func__,(level == SND_SOC_BIAS_ON?"On"
				:level == SND_SOC_BIAS_OFF?"Off"
				:level == SND_SOC_BIAS_PREPARE?"Prepare"
				:level == SND_SOC_BIAS_STANDBY?"Standby":"Unknown"));
	mutex_lock(&aic36->mutex);	
	aic36_change_page (codec, 0);
	switch (level)
	{
		/* full On */
		case SND_SOC_BIAS_ON:
			aic36_set_bias_on(codec);
			aic36->check_flags = 1;
			//mod_timer(&aic36->timer, jiffies + msecs_to_jiffies(250));
			break;
			/* partial On */
		case SND_SOC_BIAS_PREPARE:
			break;
			/* Off, without power */
		case SND_SOC_BIAS_OFF:
			/* Off, with power */
		case SND_SOC_BIAS_STANDBY:  
			//del_timer(&aic36->timer);		
			aic36_set_bias_off(codec);
			break;
	}
	codec->bias_level = level;
	codec->dapm.bias_level = level;
	mutex_unlock(&aic36->mutex);
	pr_info("--%s\r\n", __func__);
	return 0;
}


/* the sturcture contains the different values for mclk */
static const struct aic36_rate_divs aic36_divs[] = {
	/* 
	 * mclk, rate, p_val, pll_j, pll_d, dosr, ndac, mdac, aosr, nadc, madc, blck_N, r_val 
	 * codec_speficic_initializations 
	 */
	/* 8k rate */
#ifdef  CONFIG_MINI_DSP
	{12000000, 8000, 6, 32, 7680, 256, 16, 4, 128, 32, 4, 16, 2,
		{{60, 0}, {61, 0}}},
	{24000000, 8000, 2, 10, 2400, 512, 15, 2, 0/*256=0*/, 30, 2, 16, 1,
		{{60, 1}, {61, 1}}},
#else
	{12000000, 8000, 1, 7, 6800, 768, 5, 3, 128, 5, 18, 24, 1,
		{{60, 1}, {61, 1}}},
	{24000000, 8000, 2, 7, 6800, 768, 15, 1, 64, 45, 4, 24, 1,
		{{60, 1}, {61, 1}}},
#endif
	/* 11.025k rate */
	{12000000, 11025, 1, 7, 5264, 512, 8, 2, 128, 8, 8, 16, 1,
		{{60, 1}, {61, 1}}},
	{24000000, 11025, 2, 7, 5264, 512, 16, 1, 64, 32, 4, 16, 1,
		{{60, 1}, {61, 1}}},
	/* 16k rate */
	{12000000, 16000, 1, 7, 6800, 384, 5, 3, 128, 5, 9, 12, 1,
		{{60, 1}, {61, 1}}},
	{24000000, 16000, 2, 7, 6800, 384, 15, 1, 64, 18, 5, 12, 1,
		{{60, 1}, {61, 1}}},
	/* 22.05k rate */
	{12000000, 22050, 1, 7, 5264, 256, 4, 4, 128, 4, 8, 8, 1,
		{{60, 1}, {61, 1}}},
	{24000000, 22050, 2, 7, 5264, 256, 16, 1, 64, 16, 4, 8, 1,
		{{60, 1}, {61, 1}}},
	/* 32k rate */
	{12000000, 32000, 1, 7, 1680, 192, 2, 7, 64, 2, 21, 6, 1,
		{{60, 1}, {61, 1}}},
	{24000000, 32000, 2, 7, 1680, 192, 7, 2, 64, 7, 6, 6, 1,
		{{60, 1}, {61, 1}}},
	/* 44.1k rate */
#ifdef  CONFIG_MINI_DSP
	/*mclk, rate, p_val, pll_j, pll_d, dosr, ndac, mdac, aosr, nadc, madc, blck_N, r_val*/
	{12000000, 44100, 1, 7, 5264, 128, 2, 8, 128, 2, 8, 32, 1,
		{{60, 0}, {61, 0}}},
	{24000000, 44100, 2, 7, 5264, 128, 2, 8, 128, 2, 8, 32, 1,
		//  {24000000, 44100, 2, 7, 5264, 128, 8, 2, 64, 8, 4, 4, 1,
		{{60, 0}, {61, 0}}},
#else
	{12000000, 44100, 1, 7, 5264, 128, 2, 8, 128, 2, 8, 4, 1,
		{{60, 1}, {61, 1}}},
	{24000000, 44100, 2, 7, 5264, 128, 8, 2, 64, 8, 4, 4, 1,
		{{60, 1}, {61, 1}}},
#endif
	/* 48k rate */
	{12000000, 48000, 1, 8, 1920, 128, 2, 8, 128, 2, 8, 4, 1,
		{{60, 1}, {61, 1}}},
	{24000000, 48000, 2, 8, 1920, 128, 8, 2, 64, 8, 4, 4, 1,
		{{60, 1}, {61, 1}}},
	/*96k rate */
	{12000000, 96000, 1, 8, 1920, 64, 2, 8, 64, 2, 8, 2, 1,
		{{60, 7}, {61, 7}}},
	{24000000, 96000, 2, 8, 1920, 64, 4, 4, 64, 8, 2, 2, 1,
		{{60, 7}, {61, 7}}},
	/*192k */
	{12000000, 192000, 1, 8, 1920, 32, 2, 8, 32, 2, 8, 1, 1,
		{{60, 17}, {61, 13}}},
	{24000000, 192000, 2, 8, 1920, 32, 4, 4, 32, 4, 4, 1, 1,
		{{60, 17}, {61, 13}}},
	};
/* the sturcture contains the different values for mclk */
static const struct aic36_rate_divs aic36_slave_divs[] = {
	/* 
	 * mclk, rate, p_val, pll_j, pll_d, dosr, ndac, mdac, aosr, nadc, madc, blck_N, r_val 
	 * codec_speficic_initializations 
	 */
	/* 8k rate */
#ifdef  CONFIG_MINI_DSP
	{2048000, 8000, 2, 8, 0, 128, 2, 16, 128, 4, 8, 4, 2,
		{{60, 0}, {61, 0}}},
#else
	{2048000, 8000, 2, 8, 0, 128, 2, 16, 128, 4, 8, 4, 2,
		{{60, 1}, {61, 1}}},
#endif
	/* 44.1k rate */
#ifdef  CONFIG_MINI_DSP
	/*mclk, rate, p_val, pll_j, pll_d, dosr, ndac, mdac, aosr, nadc, madc, blck_N, r_val*/
	{11289600, 44100, 1, 8, 0, 128, 2, 8, 128, 4, 4, 4, 1,
		{{60, 0}, {61, 0}}},
#else
	{11289600, 44100, 1, 8, 0, 128, 2, 8, 128, 4, 4, 4, 1,
		{{60, 1}, {61, 1}}},

#endif
};

/*
 *----------------------------------------------------------------------------
 * Function : aic36_get_divs
 * Purpose  : This function is to get required divisor from the "aic36_divs"
 *            table.
 *            
 *----------------------------------------------------------------------------
 */
static inline int
aic36_get_divs (int mclk, int rate)
{
	int i;
#ifdef CONFIG_TLV320AIC36_MASTER

	for (i = 0; i < ARRAY_SIZE (aic36_divs); i++) {
		if ((aic36_divs[i].rate == rate) && (aic36_divs[i].mclk == mclk))
			return i;
	}
#else
	for (i = 0; i < ARRAY_SIZE (aic36_slave_divs); i++) {
		if ((aic36_slave_divs[i].rate == rate) && (aic36_slave_divs[i].mclk == mclk))
			return i;
	}
#endif
	pr_info("Master clock and sample rate is not supported\n");
	return -EINVAL;
}

static void 
aic36_set_div (struct snd_soc_codec *codec, int index)
{
	int value;
	int j;
	struct aic36_rate_divs div;
	
#ifdef CONFIG_TLV320AIC36_MASTER
	div = aic36_divs[index];
#else
	div = aic36_slave_divs[index];
#endif

	aic36_change_page (codec, 0);

	/*PLL P value*/
	value = aic36_read(codec, CLK_REG_2);
	value &= ~AIC36_PLL_PVALUE_MSK;
	value |= (div.p_val << AIC36_PLL_PVALUE_SHIFT);
	aic36_write(codec, CLK_REG_2, value);

	/*PLL R value*/
	value = aic36_read(codec, CLK_REG_2);
	value &= ~AIC36_PLL_RVALUE_MSK;
	value |= (div.r_val & AIC36_PLL_RVALUE_MSK);
	aic36_write(codec, CLK_REG_2, value);
	
	/*PLL  J value*/
	value = aic36_read(codec, CLK_REG_3);
	value &= ~AIC36_PLL_JVALUE_MSK;
	if(div.pll_j < 4)
		div.pll_j = 4;
	value |= (div.pll_j & AIC36_PLL_JVALUE_MSK);
	aic36_write(codec, CLK_REG_3, value);

	/*PLL  D value*/
	value = aic36_read(codec, CLK_REG_4);
	value &= ~AIC36_PLL_DVALUE_M_MSK;
	value |= ((div.pll_d >> 8) & AIC36_PLL_DVALUE_M_MSK);
	aic36_write(codec, CLK_REG_4, value);

	value = aic36_read(codec, CLK_REG_5);
	value &= ~AIC36_PLL_DVALUE_L_MSK;
	value |= ((div.pll_d & 0x00ff) & AIC36_PLL_DVALUE_L_MSK);
	aic36_write(codec, CLK_REG_5, value);

	/*NDAC value*/
	value =	aic36_read(codec, NDAC_CLK_REG_6);
	value &= ~AIC36_NDAC_VALUE_MSK;
	value |= (div.ndac & AIC36_NDAC_VALUE_MSK);
	aic36_write(codec, NDAC_CLK_REG_6, value);

	/*MDAC value*/
	value =	aic36_read(codec, MDAC_CLK_REG_7);
	value &= ~AIC36_MDAC_VALUE_MSK;
	value |= (div.mdac & AIC36_MDAC_VALUE_MSK);
	aic36_write(codec, MDAC_CLK_REG_7, value);

	/*NADC value*/
	value = aic36_read(codec, NADC_CLK_REG_8);
	value &= ~AIC36_NADC_VALUE_MSK;
	value |= (div.nadc & AIC36_NADC_VALUE_MSK);
	aic36_write(codec, NADC_CLK_REG_8, value);
	/*MADC value*/
	value = aic36_read(codec, MADC_CLK_REG_9);
	value &= ~AIC36_MADC_VALUE_MSK;
	value |= (div.madc & AIC36_MADC_VALUE_MSK);
	aic36_write(codec, MADC_CLK_REG_9, value);

	/*AOSR value*/
	value = aic36_read(codec, ADC_OSR_REG);
	value &= ~AIC36_AOSR_VALUE_MSK;
	value |= (div.aosr& AIC36_AOSR_VALUE_MSK);
	aic36_write(codec, ADC_OSR_REG, value);

	/*DOSR value*/
	value = aic36_read(codec, DAC_OSR_MSB);
	value &= ~AIC36_DOSR_VALUE_M_MSK;
	value |= ((div.dosr>> 8) & AIC36_DOSR_VALUE_M_MSK);
	aic36_write(codec, DAC_OSR_MSB, value);

	value = aic36_read(codec, DAC_OSR_LSB);
	value &= ~AIC36_DOSR_VALUE_L_MSK;
	value |= ((div.dosr & 0x00ff) & AIC36_DOSR_VALUE_L_MSK);
	aic36_write(codec, DAC_OSR_LSB, value);
	
	/*BLOCK N*/
	value = aic36_read(codec, CLK_REG_11);
	value &= ~AIC36_BLKN_VALUE_MSK;
	value |= (div.aosr& AIC36_BLKN_VALUE_MSK);
	aic36_write(codec, CLK_REG_11, value);

	for (j = 0; j < NO_FEATURE_REGS; j++)
	{
		aic36_write (codec,
			div.codec_specific_regs[j].reg_offset,
			div.codec_specific_regs[j].reg_val);
	}

}
/*
 *----------------------------------------------------------------------------
 * Function : aic36_hw_params
 * Purpose  : This function is to set the hardware parameters for AIC36.
 *            The functions set the sample rate and audio serial data word 
 *            length.
 *            
 *----------------------------------------------------------------------------
 */
static int
aic36_hw_params (struct snd_pcm_substream *substream,
		 struct snd_pcm_hw_params *params,
		 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	u8 data;

	pr_debug("aic36_hw_params \n");

	mutex_lock(&aic36->mutex);
	data = aic36_read (codec, INTERFACE_SET_REG_1);

	data = data & ~(3 << 4);

	switch (params_format (params))
	{
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (AIC36_WORD_LEN_20BITS << DATA_LEN_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (AIC36_WORD_LEN_24BITS << DATA_LEN_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (AIC36_WORD_LEN_32BITS << DATA_LEN_SHIFT);
		break;
	}

	aic36_change_page (codec, 0);
	aic36_write (codec, INTERFACE_SET_REG_1, data);
	mutex_unlock(&aic36->mutex);
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_mute
 * Purpose  : This function is to mute or unmute the left and right DAC
 *            
 *----------------------------------------------------------------------------
 */
static int aic36_mute (struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = aic36_soc_codec;
	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);

	if(aic36->mute == mute)
		return 0;
	mutex_lock(&aic36->mutex);  
	aic36->mute = mute;

	aic36_spk_mute(codec, aic36->mute);
	aic36_phone_mute(codec, aic36->mute);
	aic36_recl_mute(codec,aic36->mute);
	aic36_recr_mute(codec,aic36->mute);
	mutex_unlock(&aic36->mutex);
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_set_dai_sysclk
 * Purpose  : This function is to set the DAI system clock
 *            
 *----------------------------------------------------------------------------
 */
static int aic36_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		      int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = aic36_soc_codec;
	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);

	pr_info("%s(%d):\n", __func__,freq);

#ifdef CONFIG_TLV320AIC36_MASTER
	switch (freq)
	{
		case AIC36_FREQ_12000000:
		case AIC36_FREQ_24000000:
			aic36->sysclk = freq;
			return 0;
	}
#else
	aic36->sysclk = freq;
	return 0;
#endif
	pr_err("Invalid frequency to set DAI system clock\n");
	return -EINVAL;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_set_dai_fmt
 * Purpose  : This function is to set the DAI format
 *            
 *----------------------------------------------------------------------------
 */
static int aic36_set_dai_fmt (struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = aic36_soc_codec;
	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	u8 iface_reg;

	pr_info("%s(%d): \n", __func__,fmt);
	mutex_lock(&aic36->mutex);  
	iface_reg = aic36_read (codec, INTERFACE_SET_REG_1);
	iface_reg = iface_reg & ~(3 << 6 | 3 << 2);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK)
	{
		case SND_SOC_DAIFMT_CBM_CFM:
			aic36->master = 1;
			iface_reg |= BIT_CLK_MASTER | WORD_CLK_MASTER;
			break;
		case SND_SOC_DAIFMT_CBS_CFS:
			aic36->master = 0;
			iface_reg &= ~(BIT_CLK_MASTER | WORD_CLK_MASTER);
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
			aic36->master = 0;
			iface_reg |= BIT_CLK_MASTER;
			iface_reg &= ~(WORD_CLK_MASTER);
			break;
		default:
			printk ("Invalid DAI master/slave interface\n");
			mutex_unlock(&aic36->mutex);
			return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK)
	{
		case SND_SOC_DAIFMT_I2S:
			break;
		case SND_SOC_DAIFMT_DSP_A:
			iface_reg |= (AIC36_DSP_MODE << AUDIO_MODE_SHIFT);
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			iface_reg |= (AIC36_RIGHT_JUSTIFIED_MODE << AUDIO_MODE_SHIFT);
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			iface_reg |= (AIC36_LEFT_JUSTIFIED_MODE << AUDIO_MODE_SHIFT);
			break;
		default:
			printk ("Invalid DAI interface format\n");
			mutex_unlock(&aic36->mutex);
			return -EINVAL;
	}

	aic36_change_page (codec, 0);
	aic36_write (codec, INTERFACE_SET_REG_1, iface_reg);
	mutex_unlock(&aic36->mutex);
	return 0;
}

static int aic36_set_pll(struct snd_soc_dai *codec_dai,
	int pll_id, int source, unsigned int freq_in, unsigned int freq_out)
{
	int index;
	struct snd_soc_codec *codec = aic36_soc_codec;
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	pr_info("%s %d freq in %d freq out %d\n", __func__, __LINE__, freq_in, freq_out);
	
	index = aic36_get_divs(freq_in, freq_out);
	if(index < 0)
		return -EINVAL;
	mutex_lock(&aic36->mutex);
	aic36_set_div(codec, index);
	mutex_unlock(&aic36->mutex);	
	return 0;
}


/*
 *----------------------------------------------------------------------------
 * Function : aic36_hw_free
 * Purpose  : This function is to set the free the AIC36 Codec Resources
 *            
 *----------------------------------------------------------------------------
 */
static int aic36_hw_free(struct snd_pcm_substream * substream,
		struct snd_soc_dai *codec_dai)
{
//  struct snd_soc_pcm_runtime *rtd = substream->private_data;
//  struct snd_soc_device *socdev = rtd->socdev;
//  struct snd_soc_codec *codec = socdev->card->codec;
//  struct aic36_priv *aic36 = (struct aic36_priv *)codec_dai->codec->private_data;
  
//  aic36_set_bias_level (codec, SND_SOC_BIAS_STANDBY);

  return 0;
}

static void aic36_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *codec_dai)
{
	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(aic36_soc_codec->dev);
	
	mutex_lock(&aic36->mutex);
	switch(substream->stream)
	{
	case SNDRV_PCM_STREAM_PLAYBACK:
		aic36->playback_active = 0;
		if(aic36->capture_active)
		{
			set_playback_path(codec_dai->codec, PLAYBACK_NONE, aic36->capture_path, codec_dai->codec->bias_level);
		}
		aic36->playback_path = PLAYBACK_NONE;
		dprintk ("aic36_shutdown:SNDRV_PCM_STREAM_PLAYBACK\n");
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		aic36->capture_active = 0;
		if(aic36->playback_active)
		{
			set_capture_path(codec_dai->codec, aic36->playback_path, CAPTURE_NONE, codec_dai->codec->bias_level);
		}
  		aic36->capture_path = CAPTURE_NONE;
		dprintk ("aic36_shutdown:SNDRV_PCM_STREAM_CAPTURE\n");
#ifdef CONFIG_HAS_WAKELOCK			
		wake_unlock(&aic36->aic36_wake_lock);		
#endif
		break;
	}
	mutex_unlock(&aic36->mutex);
}
int aic36_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *codec_dai)
{
	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(aic36_soc_codec->dev);


	switch (substream->stream)
	{
	case SNDRV_PCM_STREAM_PLAYBACK:
		aic36->playback_active = 1;
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		aic36->capture_active = 1;
#ifdef CONFIG_HAS_WAKELOCK		
		wake_lock(&aic36->aic36_wake_lock);
#endif
		break;
	}
  return 0;
}

static const char *micbias_voltage[] =
{ "1.3V", "1.5V", "1.8V", "AVDD Bias" };

#define EXT_MICBIAS_ENUM		0
#define INT_MICBIAS_ENUM		1

static const struct soc_enum aic36_enum[] = {
	SOC_ENUM_SINGLE (ADC_CHN_ACONF2, 4, 4, micbias_voltage),
	SOC_ENUM_SINGLE (ADC_CHN_ACONF2, 0, 4, micbias_voltage),
};

#ifndef CONFIG_MACH_MEIZU_M9W
/* HPLMIX */
static const struct snd_kcontrol_new aic36_dapm_hplmix_controls[] = {
	SOC_DAPM_SINGLE ("Left DAC Switch", DACL_2_HPL_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE ("Right DAC Switch", DACR_2_HPL_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAL Bypass Switch", PGAL_2_HPL_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAR Bypass Switch", PGAR_2_HPL_VOL, 7, 1, 0),
};

/* HPRMIX */
static const struct snd_kcontrol_new aic36_dapm_hprmix_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", DACL_2_HPR_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", DACR_2_HPR_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAL Bypass Switch", PGAL_2_HPR_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAR Bypass Switch", PGAR_2_HPR_VOL, 7, 1, 0),
};

/* RECLMIX */
static const struct snd_kcontrol_new aic36_dapm_reclmix_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", DACL_2_RECL_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", DACR_2_RECL_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAL Bypass Switch", PGAL_2_RECL_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAR Bypass Switch", PGAR_2_RECL_VOL, 7, 1, 0),
};

/* RECRMIX */
static const struct snd_kcontrol_new aic36_dapm_recrmix_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", DACL_2_RECR_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", DACR_2_RECR_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAL Bypass Switch", PGAL_2_RECR_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAR Bypass Switch", PGAR_2_RECR_VOL, 7, 1, 0),
};

/* LINEOUT_LMIX */
static const struct snd_kcontrol_new aic36_dapm_lineout_lmix_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", DACL_2_LINEOUT_LPM, 7, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", DACR_2_LINEOUT_LPM, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAL Bypass Switch", PGAL_2_LINEOUT_LPM, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAR Bypass Switch", PGAR_2_LINEOUT_LPM, 7, 1, 0),
};

/* LINEOUT_RMIX */
static const struct snd_kcontrol_new aic36_dapm_lineout_rmix_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch",	DACL_2_LINEOUT_RPM, 7, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch",DACR_2_LINEOUT_RPM, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAL Bypass Switch", PGAL_2_LINEOUT_RPM, 7, 1, 0),
	SOC_DAPM_SINGLE("PGAR Bypass Switch", PGAR_2_LINEOUT_RPM, 7, 1, 0),
};


static const struct snd_kcontrol_new aic36_dapm_left_pga_positve_controls[] = {
	SOC_DAPM_SINGLE("LINEIN_L Switch", L_MICPGA_P, 6, 3, 0 ),
	SOC_DAPM_SINGLE("MIC1_P Switch", L_MICPGA_P, 4, 3, 0 ),
	SOC_DAPM_SINGLE("EXTMICL_P Switch", L_MICPGA_P, 2, 3, 0 ),
};

static const struct snd_kcontrol_new aic36_dapm_left_pga_negative_controls[] = {
	SOC_DAPM_SINGLE("MIC1_M Switch", L_MICPGA_N, 4, 3, 0 ),
	SOC_DAPM_SINGLE("EXTMICL_M Switch", L_MICPGA_N, 2, 3, 0 ),
};

static const struct snd_kcontrol_new aic36_dapm_right_pga_positve_controls[] = {
	SOC_DAPM_SINGLE("LINEIN_R Switch", R_MICPGA_P, 6, 3, 0 ),
	SOC_DAPM_SINGLE("MIC2_P Switch", R_MICPGA_P, 4, 3, 0 ),
	SOC_DAPM_SINGLE("EXTMICR_P Switch", R_MICPGA_P, 2, 3, 0 ),
};

static const struct snd_kcontrol_new aic36_dapm_right_pga_negative_controls[] = {
	SOC_DAPM_SINGLE("MIC2_M Switch", R_MICPGA_N, 4, 3, 0 ),
	SOC_DAPM_SINGLE("EXTMICR_M Switch", R_MICPGA_N, 2, 3, 0 ),
};

static const struct snd_soc_dapm_widget aic36_dapm_widgets[] = {
	/* Output side */
	/* DACs */
	SND_SOC_DAPM_DAC ("Left DAC", "Left Playback", SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC ("Right DAC", "Right Playback", SND_SOC_NOPM, 0, 0),

	/* Found that the DAPM is switching the Headphone Left and Right automatically with  
	 * the following settings. Going to disable the DAPM settings for Headphone now
	 */

	/*HPLMIX*/ SND_SOC_DAPM_MIXER ("HPLMIX", HPL_OUT_VOL, 0, 0,
			&aic36_dapm_hplmix_controls[0],
			ARRAY_SIZE (aic36_dapm_hplmix_controls)),
	/*HPRMIX*/ SND_SOC_DAPM_MIXER ("HPRMIX", HPR_OUT_VOL, 0, 0,
			&aic36_dapm_hprmix_controls[0],
			ARRAY_SIZE (aic36_dapm_hprmix_controls)),
	/*RECLMIX*/ SND_SOC_DAPM_MIXER ("RECLMIX", RECL_OUT_VOL, 0, 0,
			&aic36_dapm_reclmix_controls[0],
			ARRAY_SIZE (aic36_dapm_reclmix_controls)),
	/*RECRMIX*/ SND_SOC_DAPM_MIXER ("RECRMIX", RECR_OUT_VOL, 0, 0,
			&aic36_dapm_recrmix_controls[0],
			ARRAY_SIZE (aic36_dapm_recrmix_controls)),
	/*LINEOUT_LMIX*/
	SND_SOC_DAPM_MIXER ("LINEOUT_LMIX", LINEOUT_LPM_VOL, 0, 0,
			&aic36_dapm_lineout_lmix_controls[0],
			ARRAY_SIZE (aic36_dapm_lineout_lmix_controls)),
	/*LINEOUT_RMIX*/
	SND_SOC_DAPM_MIXER ("LINEOUT_RMIX", LINEOUT_RPM_VOL, 0, 0,
			&aic36_dapm_lineout_rmix_controls[0],
			ARRAY_SIZE (aic36_dapm_lineout_rmix_controls)),

	/* Bypass PGA */
	SND_SOC_DAPM_PGA("MICPGA_L Bypass PGA", L_MICPGA_V, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MICPGA_R Bypass PGA", R_MICPGA_V, 7, 0, NULL, 0),

	/* ADCs */
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", SND_SOC_NOPM, 0, 0),


	/* Left PGA Positive Terminal Mixer */
	SND_SOC_DAPM_MIXER ("LPGA_P", SND_SOC_NOPM, 0, 0,
			&aic36_dapm_left_pga_positve_controls[0],
			ARRAY_SIZE (aic36_dapm_left_pga_positve_controls)),
	/* Left PGA Negative Terminal Mixer */
	SND_SOC_DAPM_MIXER ("LPGA_N", SND_SOC_NOPM, 0, 0,
			&aic36_dapm_left_pga_negative_controls[0],
			ARRAY_SIZE (aic36_dapm_left_pga_negative_controls)),

	/* Right PGA Positive Terminal Mixer */
	SND_SOC_DAPM_MIXER ("RPGA_P", SND_SOC_NOPM, 0, 0,
			&aic36_dapm_right_pga_positve_controls[0],
			ARRAY_SIZE (aic36_dapm_right_pga_positve_controls)),
	/* Right PGA Negative Terminal Mixer */
	SND_SOC_DAPM_MIXER ("RPGA_N", SND_SOC_NOPM, 0, 0,
			&aic36_dapm_right_pga_negative_controls[0],
			ARRAY_SIZE (aic36_dapm_right_pga_negative_controls)),

	//SND_SOC_DAPM_REG (snd_soc_dapm_micbias, "Ext Mic Bias", ADC_CHN_ACONF2, 6,
	//						1, 1, 0),
	// SND_SOC_DAPM_REG (snd_soc_dapm_micbias, "Int Mic Bias", ADC_CHN_ACONF2, 2,
	//						1, 1, 0),
	SND_SOC_DAPM_MICBIAS("Ext Mic Bias",ADC_CHN_ACONF2, 6, 0),
	SND_SOC_DAPM_MICBIAS("Int Mic Bias",ADC_CHN_ACONF2, 2, 0),

	SND_SOC_DAPM_OUTPUT ("HPL"),
	SND_SOC_DAPM_OUTPUT ("HPR"),
	SND_SOC_DAPM_OUTPUT ("RECL"),
	SND_SOC_DAPM_OUTPUT ("RECR"),
	SND_SOC_DAPM_OUTPUT ("LINEOUT_LPM"),
	SND_SOC_DAPM_OUTPUT ("LINEOUT_RPM"),

	SND_SOC_DAPM_INPUT ("LINEIN_L"),
	SND_SOC_DAPM_INPUT ("LINEIN_R"),
	SND_SOC_DAPM_INPUT ("MIC1_P"),
	SND_SOC_DAPM_INPUT ("MIC1_M"),
	SND_SOC_DAPM_INPUT ("MIC2_P"),
	SND_SOC_DAPM_INPUT ("MIC2_M"),
	SND_SOC_DAPM_INPUT ("EXTMICL_P"),
	SND_SOC_DAPM_INPUT ("EXTMICL_M"),
	SND_SOC_DAPM_INPUT ("EXTMICR_P"),
	SND_SOC_DAPM_INPUT ("EXTMICR_M"),
};

static const struct snd_soc_dapm_route aic36_audio_map[] = {
  /* Bypass PGAs */
	{ "MICPGA_L Bypass PGA", NULL, "LINEIN_L" },
	{ "MICPGA_R Bypass PGA", NULL, "LINEIN_R" },
	/* HPLMIX */
	{ "HPLMIX", "Left DAC Switch", "Left DAC" },
	{ "HPLMIX", "Right DAC Switch", "Right DAC" },
	{ "HPLMIX", "PGAL Bypass Switch", "MICPGA_L Bypass PGA" },
	{ "HPLMIX", "PGAR Bypass Switch", "MICPGA_R Bypass PGA" },

	/* HPRMIX */
	{ "HPRMIX", "Left DAC Switch", "Left DAC" },
	{ "HPRMIX", "Right DAC Switch", "Right DAC" },
	{ "HPRMIX", "PGAL Bypass Switch", "MICPGA_L Bypass PGA" },
	{ "HPRMIX", "PGAR Bypass Switch", "MICPGA_R Bypass PGA" },

	/* RECLMIX */
	{ "RECLMIX", "Left DAC Switch", "Left DAC" },
	{ "RECLMIX", "Right DAC Switch", "Right DAC" },
	{ "RECLMIX", "PGAL Bypass Switch", "MICPGA_L Bypass PGA" },
	{ "RECLMIX", "PGAR Bypass Switch", "MICPGA_R Bypass PGA" },

	/* RECRMIX */
	{ "RECRMIX", "Left DAC Switch", "Left DAC" },
	{ "RECRMIX", "Right DAC Switch", "Right DAC" },
	{ "RECRMIX", "PGAL Bypass Switch", "MICPGA_L Bypass PGA" },
	{ "RECRMIX", "PGAR Bypass Switch", "MICPGA_R Bypass PGA" },

	/* LINEOUT_LMIX */
	{ "LINEOUT_LMIX", "Left DAC Switch", "Left DAC" },
	{ "LINEOUT_LMIX", "Right DAC Switch", "Right DAC" },
	{ "LINEOUT_LMIX", "PGAL Bypass Switch", "MICPGA_L Bypass PGA" },
	{ "LINEOUT_LMIX", "PGAR Bypass Switch", "MICPGA_R Bypass PGA" },

	/* LINEOUT_RMIX */
	{ "LINEOUT_RMIX", "Left DAC Switch", "Left DAC" },
	{ "LINEOUT_RMIX", "Right DAC Switch", "Right DAC" },
	{ "LINEOUT_RMIX", "PGAL Bypass Switch", "MICPGA_L Bypass PGA" },
	{ "LINEOUT_RMIX", "PGAR Bypass Switch", "MICPGA_R Bypass PGA" },

	{"HPL", NULL, "HPLMIX"},
	/* HPR Output */
	{"HPR", NULL, "HPRMIX"},
	/* RECL Output */
	{"RECL", NULL, "RECLMIX"},
	/* RECR Output */
	{"RECR", NULL, "RECRMIX"},
	/* LINEOUT_LPM Output */
	{"LINEOUT_LPM", NULL, "LINEOUT_LMIX"},
	/* LINEOUT_RPM Output */
	{"LINEOUT_RPM", NULL, "LINEOUT_RMIX"},

	/* Inputs */
	{"LPGA_P", "LINEIN_L Switch", "LINEIN_L"},
	{"LPGA_P", "MIC1_P Switch", "MIC1_P"},
	{"LPGA_P", "EXTMICL_P Switch", "EXTMICL_P"},

	{"LPGA_N", "MIC1_M Switch", "MIC1_M"},
	{"LPGA_N", "EXTMICL_M Switch", "EXTMICL_M"},

	{"RPGA_P", "LINEIN_R Switch", "LINEIN_R"},
	{"RPGA_P", "MIC2_P Switch", "MIC2_P"},
	{"RPGA_P", "EXTMICR_P Switch", "EXTMICR_P"},

	{"RPGA_N", "MIC2_M Switch", "MIC2_M"},
	{"RPGA_N", "EXTMICR_M Switch", "EXTMICR_M"},

	{"Left ADC", NULL, "LPGA_P"},
	{"Right ADC", NULL, "RPGA_P"},
	{"Left ADC", NULL, "LPGA_N"},
	{"Right ADC", NULL, "RPGA_N"},
};

static int
aic36_add_widgets (struct snd_soc_codec *codec)
{
  snd_soc_dapm_new_controls (codec, aic36_dapm_widgets,
			     ARRAY_SIZE (aic36_dapm_widgets));

  /* set up the AIC36 audio map */
  snd_soc_dapm_add_routes (codec, aic36_audio_map,
			   ARRAY_SIZE (aic36_audio_map));

  snd_soc_dapm_new_widgets (codec);
  return 0;
}
#endif

static int aic36_get_playback_path(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{	
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);

  	ucontrol->value.integer.value[0] = aic36->playback_path;
	return 0;
}

static int aic36_set_playback_path(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	int user_value = ucontrol->value.integer.value[0];
	int ret = 0;
	
	if(ARRAY_SIZE(playback_path_name) <= user_value)
	{
		pr_info("%s:Invalid input, user_value %d\n",__func__, user_value);
		return -EINVAL;
	}

	// To do
	pr_debug("%s(): path => %s\n", __func__,playback_path_name[user_value]);
	if(aic36->playback_path != user_value)
	{
		mutex_lock(&aic36->mutex);
		ret = set_playback_path(codec, user_value, aic36->capture_path, codec->bias_level);	
		if(ret >= 0)	
			aic36->playback_path= user_value;
		mutex_unlock(&aic36->mutex);		
	}
	return ret;
}

static int aic36_get_capture_path(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{	
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);

  	ucontrol->value.integer.value[0] = aic36->capture_path;
	return 0;
}

static int aic36_set_capture_path(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	int user_value = ucontrol->value.integer.value[0];
	int ret = 0;

	if(ARRAY_SIZE(capture_path_name) <= user_value)
	{
		printk ("%s:Invalid input\n",__func__);
		return -EINVAL;
	}
	
	// To do
	dprintk(KERN_INFO "%s(): path => %s\n", __func__,capture_path_name[user_value]);	
	if(aic36->capture_path != user_value)
	{
		mutex_lock(&aic36->mutex);
		ret = set_capture_path(codec, aic36->playback_path, user_value, codec->bias_level);
		if(ret >= 0)
			aic36->capture_path= user_value;
		mutex_unlock(&aic36->mutex);
	}
	return ret;
}

static const DECLARE_TLV_DB_LINEAR(dac_voice_tlv, 0, AIC36_VOICE_LEVEL_MAX);
static const DECLARE_TLV_DB_LINEAR(output_voice_tlv, 0, AIC36_VOICE_LEVEL_MAX);

static const struct snd_kcontrol_new aic36_snd_controls[] = {
	//SOC_DOUBLE_R_AIC36 ("PCM Playback Volume", LDAC_VOL, RDAC_VOL, 0, 0xaf, 1),
	SOC_DOUBLE_R_AIC36 ("PCM Playback Volume", LDAC_VOL, RDAC_VOL, 0, 0x7f, 0),
	SOC_DOUBLE_R_AIC36 ("PGA Capture Volume", LADC_VOL, RADC_VOL, 0, 0x41, 0),
	SOC_DOUBLE_R ("PGA Capture Gain", L_MICPGA_V, R_MICPGA_V, 0, 0x5F, 0),
	//SOC_DOUBLE_R("Speaker Playback Volume", LINEOUT_LPM_VOL, LINEOUT_RPM_VOL, 4, 0x9, 0),
	//SOC_DOUBLE_R("Headphone Playback Volume", HPL_OUT_VOL, HPR_OUT_VOL, 4, 0x9, 0),
	//SOC_DOUBLE_R("Speaker Playback Volume", DACL_2_LINEOUT_LPM, DACR_2_LINEOUT_RPM, 0, 0x76, 1),
	//SOC_DOUBLE_R("Headphone Playback Volume", DACL_2_HPL_VOL, DACR_2_HPR_VOL, 0, 0x76, 1),
	//SOC_SINGLE("Earpiece Playback Volume", R_MICPGA_V, 0, 30, 0),
	//SOC_SINGLE("Earpiece Playback Volume", RECL_OUT_VOL, 4, 0x9, 0),

#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
	SOC_SINGLE_TLV_AIC36("Earpiece Playback Volume", PGAR_2_RECL_VOL, 0, AIC36_VOICE_LEVEL_MAX, 1,output_voice_tlv),//0dB  --- -9dB
#else
	SOC_SINGLE_TLV_AIC36("Earpiece Playback Volume", RDAC_VOL, 0, AIC36_VOICE_LEVEL_MAX, 0,dac_voice_tlv),//  3dB --- 0dB  --- -78.3dB
	//SOC_SINGLE_TLV_AIC36("Earpiece Playback Volume", RDAC_VOL, 0, 133, 0,dac_voice_tlv),//  3dB --- 0dB  --- -78.3dB
#endif
	SOC_SINGLE_NOISE_AIC36("Noise Suppress Mode", 0, 0, 0x1, 0),
	SOC_SINGLE("Recout Playback Volume", DACL_2_RECR_VOL, 0, 0x76, 1),
	SOC_SINGLE("PCML Playback Volume", LDAC_VOL, 0, 0xAf, 0),
	SOC_SINGLE("PCMR Playback Volume", LDAC_VOL, 0, 0xAf, 0),
	SOC_SINGLE ("HPL Mute", HPL_OUT_VOL, 3, 1, 1),
	SOC_SINGLE ("HPR Mute", HPR_OUT_VOL, 3, 1, 1),
	SOC_SINGLE ("RECL Mute", RECL_OUT_VOL, 3, 1, 1),
	SOC_SINGLE ("RECR Mute", RECR_OUT_VOL, 3, 1, 1),
	SOC_SINGLE ("LINEOUT_LPM Mute", LINEOUT_LPM_VOL, 3, 1, 1),
	SOC_SINGLE ("LINEOUT_RPM Mute", LINEOUT_RPM_VOL, 3, 1, 1),
	SOC_SINGLE("MICPGA_L Mute", L_MICPGA_V, 7, 1, 0),
	SOC_SINGLE("MICPGA_R Mute", R_MICPGA_V, 7, 1, 0),
	SOC_SINGLE("ADCPOWER_L Mute", ADC_REG_1, 7, 1, 0),
	SOC_SINGLE("ADCPOWER_R Mute", ADC_REG_1, 6, 1, 0),
	SOC_SINGLE("ADC_L Mute", ADC_FGA, 7, 1, 1),
	SOC_SINGLE("ADC_R Mute", ADC_FGA, 3, 1, 1),
	SOC_DOUBLE("DRC Mute", DRC_REG_1, 6,5, 1, 0),
	SOC_SINGLE("Int Mic Bias Switch", ADC_CHN_ACONF2, 2, 1, 0),
	SOC_SINGLE("Ext Mic Bias Switch", ADC_CHN_ACONF2, 6, 1, 0),
	SOC_ENUM ("Ext Mic Bias Voltage", aic36_enum[EXT_MICBIAS_ENUM]),
	SOC_ENUM ("Int Mic Bias Voltage", aic36_enum[INT_MICBIAS_ENUM]),
	SOC_ENUM_EXT("Playback_path Selection", path_enum[0],aic36_get_playback_path, aic36_set_playback_path),
	SOC_ENUM_EXT("Capture_path Selection", path_enum[1],aic36_get_capture_path, aic36_set_capture_path),
	SOC_SINGLE_AIC36 ("Program Registers"),
};

static int aic36_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE (aic36_snd_controls); i++)
	{
		err = snd_ctl_add(codec->card->snd_card,
				snd_soc_cnew(&aic36_snd_controls[i], codec, NULL, NULL));
		if (err < 0)
			return err;
	}
	return 0;
}

static const struct aic36_configs aic36_reg_init[] = {
	// {RESET, 1},
	//{RESET, 0},

	{CLK_REG_1, PLLCLK_2_CODEC_CLKIN},

	{AIS_REG_3, ADC_CLK_2_BDIV_CLKIN},
	{ANALOG_FIR_REG1, 0x30},
	{ANALOG_FIR_REG2, 0x75},
	{ANALOG_FIR_REG3, 0xc9},

	{INTERFACE_SET_REG_1, 0x0C}, 

};

/*
 *----------------------------------------------------------------------------
 * Function : aic36_init
 * Purpose  : This function is to initialise the AIC36 driver
 *            register the mixer and dsp interfaces with the kernel.
 *            
 *----------------------------------------------------------------------------
 */
static int tlv320aic36_codec_init(struct snd_soc_codec *codec)
{
	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	int ret = 0;
	int i = 0;

	codec->read = aic36_read;
	codec->write = aic36_write;
	codec->num_dai = 1;
	codec->reg_cache = kmemdup (aic36_reg, sizeof (aic36_reg), GFP_KERNEL);
	if (codec->reg_cache == NULL) {
		printk (KERN_ERR "aic36: kmemdup failed\n");
		return -ENOMEM;
	}

	aic36->page_no = 0;

	aic36_change_page (codec, 0x0);
	aic36_write (codec, RESET, 1);
	aic36_write (codec, RESET, 0);

	for (i = 0;	i < sizeof (aic36_reg_init) / sizeof (struct aic36_configs); i++) {
		aic36_write (codec, aic36_reg_init[i].reg_offset,
				aic36_reg_init[i].reg_val);
	}

	/* off, with power on */
	aic36_set_bias_level (codec, SND_SOC_BIAS_STANDBY);
	aic36_add_controls (codec);
#ifndef CONFIG_MACH_MEIZU_M9W
	aic36_add_widgets (codec);
#endif

#ifdef CONFIG_MINI_DSP
	aic36_add_minidsp_controls (codec);
#endif

	return ret;
}

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
/*
 *----------------------------------------------------------------------------
 * Function : aic36_codec_probe
 * Purpose  : This function attaches the i2c client and initializes 
 *				AIC36 CODEC.
 *            NOTE:
 *            This function is called from i2c core when the I2C address is
 *            valid.
 *            If the i2c layer weren't so broken, we could pass this kind of 
 *            data around
 *            
 *----------------------------------------------------------------------------
 */
static int tlv320aic36_codec_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct snd_soc_codec *codec = aic36_soc_codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = (void*)i2c;

	ret = tlv320aic36_codec_init(codec);
	if (ret < 0)
	{
		printk (KERN_ERR "aic36: failed to attach codec at addr\n");
		return -1;
	}

#ifdef CONFIG_MINI_DSP
	/* Program MINI DSP for ADC and DAC */
	aic36_minidsp_program (codec);
#endif

	return ret;
}

/*
 *----------------------------------------------------------------------------
 * Function : tlv320aic3007_i2c_remove
 * Purpose  : This function removes the i2c client and uninitializes 
 *                              AIC3007 CODEC.
 *            NOTE:
 *            This function is called from i2c core 
 *            If the i2c layer weren't so broken, we could pass this kind of 
 *            data around
 *            
 *----------------------------------------------------------------------------
 */
static int __exit tlv320aic36_i2c_remove (struct i2c_client *i2c)
{
	put_device (&i2c->dev);
	return 0;
}

static const struct i2c_device_id tlv320aic36_id[] = {
	{"tlv320aic36", 0},
	{}
};

MODULE_DEVICE_TABLE (i2c, tlv320aic36_id);

static struct i2c_driver tlv320aic36_i2c_driver = {
	.driver = {
		.name = "tlv320aic36",
	},
	.probe = tlv320aic36_codec_probe,
	.remove = __exit_p (tlv320aic36_i2c_remove),
	.id_table = tlv320aic36_id,
};

static int aic36_add_i2c_device(struct platform_device *pdev,
				 const struct aic36_setup_data *setup)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	//struct i2c_client *client;
	int ret;

	ret = i2c_add_driver(&tlv320aic36_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = setup->i2c_address;
	strlcpy(info.type, "tlv320aic36", I2C_NAME_SIZE);

	adapter = i2c_get_adapter(setup->i2c_bus);
	if (!adapter) {
		dev_err(&pdev->dev, "can't get i2c adapter %d\n",
			setup->i2c_bus);
		goto err_driver;
	}
	/*FIXME: do we need to create a new i2c device here?*/
//	client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
//	if (!client) {
//		dev_err(&pdev->dev, "can't add i2c device at 0x%x\n",
//			(unsigned int)info.addr);
//		goto err_driver;
//	}
    
	return 0;

err_driver:
	i2c_del_driver(&tlv320aic36_i2c_driver);
	return -ENODEV;
}
#endif //#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)

static void aic36_ext_mic_bias_func(struct work_struct *work)
{
	u8 value;
	struct snd_soc_codec *codec = aic36_soc_codec;

	if(!aic36_initialized)		
		return;
	
	if(!codec)
	{
		printk(KERN_INFO "%s(): failed!!!\n", __func__);		
		return;
	}
	
	value = codec->read(codec, ADC_CHN_ACONF2);
	
	if(g_bBiasOnOff)
		codec->write(codec, ADC_CHN_ACONF2, (value & 0x0f) | 0x60);
	else
		codec->write(codec, ADC_CHN_ACONF2, (value & 0x0f));
	
	return;
}



void set_ext_mic_bias(bool bOnOff)
{
	g_bBiasOnOff = bOnOff;
	schedule_work(&ext_mic_bias_work);
}
EXPORT_SYMBOL(set_ext_mic_bias);

bool get_ext_mic_bias(void)
{
	return g_bBiasOnOff;
}
EXPORT_SYMBOL(get_ext_mic_bias);

extern void tlv320aic36_power(int power_on);
extern void tlv320aic36_reset(void);
extern int set_minidsp_music_mode(struct snd_soc_codec *codec);
static ssize_t aic36_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf);
static ssize_t aic36_store(struct device *dev, 
			     struct device_attribute *attr,
			     const char *buf, size_t count);

#define AIC36_CODEC_ATTR(_name)\
{\
    .attr = { .name = #_name, .mode = S_IRUGO | S_IWUGO,},\
    .show = aic36_show_property,\
    .store = aic36_store,\
}

static struct device_attribute aic36_attrs[] = {
    AIC36_CODEC_ATTR(voice_gain_level),
    AIC36_CODEC_ATTR(reg_program),
    AIC36_CODEC_ATTR(soft_reset),
    AIC36_CODEC_ATTR(hardware_reset),
    AIC36_CODEC_ATTR(i2c_test),
};
enum {
    AIC36_GAIN_LEVEL = 0,
    AIC36_REG_PROGRAM,		
    AIC36_SOFT_RESET,
    AIC36_HW_RESET,
    AIC36_I2C_TEST,
};
static ssize_t aic36_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
	int i = 0;
	u8 page, reg, value;
	struct snd_soc_codec *codec = aic36_soc_codec;
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);

	const ptrdiff_t off = attr - aic36_attrs;
	switch(off){
	case AIC36_GAIN_LEVEL:
		i = scnprintf(buf+i, PAGE_SIZE-i, "mic gain = %d\noutput level: 0-%d,1-%d,2-%d,3-%d,4-%d,5-%d,6-%d,7-%d,8-%d,9-%d,10-%d\n", \
			   aic36->voice_mic_gain, aic36->voice_gain[0], aic36->voice_gain[1], aic36->voice_gain[2], aic36->voice_gain[3], \
			   aic36->voice_gain[4], aic36->voice_gain[5], aic36->voice_gain[6], aic36->voice_gain[7], \
			   aic36->voice_gain[8], aic36->voice_gain[9], aic36->voice_gain[10]);
		break;
	case AIC36_REG_PROGRAM:
		for(page=0; page<3; page++)
		{	
			for(reg=0; reg<128; reg++)
			{
				  if (aic36->page_no != page)
				    {
				      aic36_change_page (codec, page);
				    }
				/*standard way of calling things */
				  i2c_master_send(codec->control_data, (char *)&reg, 1);
				  i2c_master_recv(codec->control_data, &value, 1);
					
				  printk("CODEC REG %d :%.2d = 0x%.2x\n", page, reg,value);
			}
		}
		i += scnprintf(buf+i, PAGE_SIZE-i, "End Read\n");
		break;
	case AIC36_SOFT_RESET:
		i += scnprintf(buf+i, PAGE_SIZE-i, "Nothing\n");
		break;		
	case AIC36_HW_RESET:
		i += scnprintf(buf+i, PAGE_SIZE-i, "Nothing\n");
		break;	
	case AIC36_I2C_TEST:
		i += scnprintf(buf+i, PAGE_SIZE-i, "Nothing\n");
		break;	
	}
	return i;
}

static ssize_t aic36_store(struct device *dev, 
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int i=0, j=0,value;
	int page, reg, reg2;	
	u32 voice_gain[AIC36_VOICE_LEVEL_MAX+1];
	u32 voice_mic_gain;
	int ret = 0;
	struct snd_soc_codec *codec = aic36_soc_codec;
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	const ptrdiff_t off = attr - aic36_attrs;

	switch(off){
	case AIC36_GAIN_LEVEL:
		if (sscanf(buf, "mic:%d,level:0-%d,1-%d,2-%d,3-%d,4-%d,5-%d,6-%d,7-%d,8-%d,9-%d,10-%d\n", \
			&voice_mic_gain, &voice_gain[0],&voice_gain[1],&voice_gain[2],&voice_gain[3],&voice_gain[4],\
			&voice_gain[5],&voice_gain[6],&voice_gain[7],&voice_gain[8],&voice_gain[9],&voice_gain[10]) == 12) 
		{
			for(i=0; i<11; i++)
			{
				aic36->voice_gain[i] = voice_gain[i];
			}
			//apply voice volume
#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
			//Set PGAR to Speaker volume
			value = codec->read (codec, PGAR_2_LINEOUT_LPM);
			value = (aic36->voice_gain[aic36->voice_volume] + 12) | (value & 0x80);
			codec->write (codec, PGAR_2_LINEOUT_LPM, value);
			value = codec->read (codec, PGAR_2_LINEOUT_RPM);
			value = (aic36->voice_gain[aic36->voice_volume]+ 12) | (value & 0x80);
			codec->write (codec, PGAR_2_LINEOUT_RPM, value);
#endif
			aic36->voice_mic_gain = voice_mic_gain;
			//apply gain
			 value = codec->read(codec, L_MICPGA_V);
			 value &= (~0x7f);
			 value |= aic36->voice_mic_gain;
			 codec->write (codec, L_MICPGA_V, value);
		}
		ret = count;
		break;
	case AIC36_REG_PROGRAM:
		if (sscanf(buf, "%d:%d=%d", &page, &reg, &value) == 3) 
		{
			reg2 = page*128 + reg;
			value = aic36_write(codec, reg, value);
		}
		ret = count;
		break;		
	case AIC36_SOFT_RESET:
		aic36_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
		mdelay(50);
		set_minidsp_music_mode(codec);
		mdelay(50);
		aic36_set_bias_level(codec, SND_SOC_BIAS_ON);
		aic36_spk_mute(codec, false);
		aic36_phone_mute(codec, false);
		aic36_recl_mute(codec,false);
		aic36_recr_mute(codec,false);
		ret = count;
		break;
	case AIC36_HW_RESET:
		/*turn on audio codec power*/
		//tlv320aic36_power(1);
		/*reset codec*/
		//tlv320aic36_reset();
		mdelay(100);
		set_minidsp_music_mode(codec);
		ret = count;
		break;
	case AIC36_I2C_TEST:
		for(i=0; i<10000000; i++) {
			for(j=0; j< 12; j++)
				aic36_change_page(codec, j);
			msleep(100);
		}
		ret = count;
		break;
	}
	return ret;	
}

static int aic36_create_attrs(struct device * dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(aic36_attrs); i++) {
		rc = device_create_file(dev, &aic36_attrs[i]);
		if (rc)
			goto aic36_attrs_failed;
	}
	goto succeed;

aic36_attrs_failed:
	while (i--)
		device_remove_file(dev, &aic36_attrs[i]);
succeed:		
	return rc;

}
static void aic36_codec_status_work(struct work_struct *work)
{
	struct snd_soc_codec *codec = aic36_soc_codec;
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);
	u8 value=0;
	u8 dacstatus=0;
	
	value = aic36_read_overflow_status(codec);
	if(value & 0x07 && aic36->check_flags == 1)
	{
		printk("%s : overflow status 0x%.2x#########################\n", __func__, value);
	
		/*restart codec*/
		mutex_lock(&aic36->mutex);
		dacstatus = aic36_read_status (codec, DAC_STATUS_REG);
		/* Power down Left DAC channel*/
		value = codec->read (codec, DAC_CHN_REG);
		value &= (~0xC0);	
		codec->write (codec, DAC_CHN_REG, value);	
		msleep(100);
		if(dacstatus & 0x80)
		{
			/* Power up Left DAC channel*/
			value = codec->read (codec, DAC_CHN_REG);
			value |= 0x80;
			codec->write (codec, DAC_CHN_REG, value);	
		}
		if(dacstatus & 0x08)
		{
			/* Power up Right DAC channel*/
			value = codec->read (codec, DAC_CHN_REG);
			value |= 0x40;
			codec->write (codec, DAC_CHN_REG, value);	
		}
		
		mutex_unlock(&aic36->mutex);
		aic36->check_flags = 0;
	}
}
static void aic36_codec_status_func(unsigned long data)
{
	struct snd_soc_codec *codec = (struct snd_soc_codec *)data;
  	struct aic36_priv *aic36 = dev_get_drvdata(codec->dev);

	if(standby_mode_enable == 0)
	{
		schedule_work(&aic36->codec_status_wq);
	}
}
/*
 *----------------------------------------------------------------------------
 * Function : aic36_probe
 * Purpose  : This is first driver function called by the SoC core driver.
 *            
 *----------------------------------------------------------------------------
 */
static int aic36_probe(struct snd_soc_codec  *codec)
{
	struct aic36_priv *aic36;
	struct aic36_setup_data *aic36_setup = codec->dev->platform_data;
	int ret = 0, i = 0;

	printk (KERN_INFO "AIC36 Audio Codec %s\n", AIC36_VERSION);

	aic36 = kzalloc(sizeof (struct aic36_priv), GFP_KERNEL);
	if (aic36 == NULL){
		kfree(codec);
		return -ENOMEM;
	}
	snd_soc_codec_set_drvdata(codec, aic36);

	aic36->playback_active = 0;
	aic36->capture_active = 0;
	aic36->playback_path = PLAYBACK_NONE;
	aic36->capture_path = CAPTURE_NONE;

	aic36->music_volume = HP_MIXER_NORMAL_VALUE;//-9db

	aic36->voice_volume = 0;
	aic36->voice_mic_gain = AIC36_VOICE_LPGA_GAIN;
	aic36->mute = 1;
	for(i=0; i<AIC36_VOICE_LEVEL_MAX+1; i++) {
		aic36->voice_gain[i] = ((AIC36_VOICE_GAIN_MAX-AIC36_VOICE_GAIN_MIN)*i)/AIC36_VOICE_LEVEL_MAX + AIC36_VOICE_GAIN_MIN;
	}
	aic36->voice_gain[AIC36_VOICE_LEVEL_MAX] = AIC36_VOICE_GAIN_MAX;

	mutex_init(&aic36->mutex);

	aic36_soc_codec = codec;
	INIT_WORK(&aic36->codec_status_wq, aic36_codec_status_work);
	setup_timer(&aic36->timer, aic36_codec_status_func, (unsigned long)codec);  
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (aic36_setup->i2c_address) {
		codec->hw_write = (hw_write_t)i2c_master_send;
		codec->hw_read = NULL;
		ret = aic36_add_i2c_device(to_platform_device(codec->dev), aic36_setup);
	}else{
		printk(KERN_ERR "can't add i2c driver");
	}
#else
	/* Add other interfaces here */
#endif

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&aic36->aic36_wake_lock, WAKE_LOCK_SUSPEND, "aic36_working");
#endif

	aic36_create_attrs(codec->dev);
	aic36_initialized = 1;
	/*applyed ext mic bais valtage */
	g_bBiasOnOff = true;  
	schedule_work(&ext_mic_bias_work);

	return ret;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_remove
 * Purpose  : to remove aic36 soc device 
 *            
 *----------------------------------------------------------------------------
 */
static int aic36_remove (struct snd_soc_codec *codec)
{
	struct aic36_priv *aic36 = (struct aic36_priv *)dev_get_drvdata(codec->dev);

	printk(KERN_INFO "%s(): \n", __func__);
	/* power down chip */
	if (codec->control_data)
		aic36_set_bias_level (codec, SND_SOC_BIAS_OFF);

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver (&tlv320aic36_i2c_driver);
#endif
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&aic36->aic36_wake_lock);
#endif
	kfree (aic36);
	kfree (codec);
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_suspend
 * Purpose  : This function is to suspend the AIC36 driver.
 *            
 *----------------------------------------------------------------------------
 */
static int aic36_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	aic36_set_bias_level (codec, SND_SOC_BIAS_STANDBY);
	/*ext mic bias voltage power off*/
	aic36_write(codec, ADC_CHN_ACONF2, 0x00);
	g_bBiasOnOff = false;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic36_resume
 * Purpose  : This function is to resume the AIC36 driver
 *            
 *----------------------------------------------------------------------------
 */
static int aic36_resume(struct snd_soc_codec *codec)
{
	aic36_set_bias_level (codec, codec->suspend_bias_level);

	/*ext mic bias voltage power on/off*/
	g_bBiasOnOff = true;
	schedule_work(&ext_mic_bias_work);
	return 0;
}

static struct snd_soc_dai_ops tlv320aic36_dai_ops = {
	.hw_params = aic36_hw_params,
	.digital_mute = aic36_mute,
	.set_sysclk = aic36_set_dai_sysclk,
	.set_fmt = aic36_set_dai_fmt,
	.hw_free = aic36_hw_free,
	.startup = aic36_startup,
	.shutdown = aic36_shutdown,
	.set_pll = aic36_set_pll,
};


static struct snd_soc_dai_driver tlv320aic36_dai[] = {
	{
		.name 	= "tlv320aic36-dai",
		.id		= 1,
		.playback	= {
			.stream_name = "AIC36 PLAYBACK",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AIC36_RATES,
			.formats = AIC36_FORMATS,
		},
		.capture	= {
			.stream_name  = "AIC36 CAPTURE",
			.channels_min = 1,
			.channels_max = 2,
#ifdef MINIDSP_SAMPLE_8KHZ
			.rates = SNDRV_PCM_RATE_8000,
#else
			.rates = SNDRV_PCM_RATE_44100,
#endif
			.formats = AIC36_FORMATS,
		},
		.ops = &tlv320aic36_dai_ops,
	},
};

EXPORT_SYMBOL_GPL(tlv320aic36_dai);

static struct snd_soc_codec_driver soc_codec_dev_tlv320aic36 = {
	.probe 		= aic36_probe,
	.remove		= aic36_remove,
	.suspend	= aic36_suspend,
	.resume		= aic36_resume,
	.read		= aic36_read,
	.write		= aic36_write,
	.set_bias_level		= aic36_set_bias_level,
	.reg_word_size	= 2,
	.compress_type	= SND_SOC_RBTREE_COMPRESSION,
};

static int __devinit tlv320aic36_probe(struct platform_device *pdev)
{
	snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tlv320aic36,
			tlv320aic36_dai, ARRAY_SIZE(tlv320aic36_dai));
	return 0;
}

static int __devexit tlv320aic36_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver tlv320aic36_plat_driver = {
	.probe		= tlv320aic36_probe,
	.remove		= __devexit_p(tlv320aic36_remove),
	.driver		= {
		.name	= "tlv320aic36-codec",
		.owner	= THIS_MODULE,
	},
};

static int __init tlv320aic36_init(void)
{
	return platform_driver_register(&tlv320aic36_plat_driver);
}

static void __exit tlv320aic36_exit(void)
{
	snd_soc_unregister_codec(&m9w_snd_device->dev);
}

module_init(tlv320aic36_init);
module_exit(tlv320aic36_exit);

MODULE_DESCRIPTION (" ASoC TLV320AIC36 codec driver ");
MODULE_AUTHOR (" Arun KS <arunks@mistralsolutions.com> ");
MODULE_AUTHOR (" Caoziqiang<caoziqiang@meizu.com> ");
MODULE_LICENSE ("GPL");
