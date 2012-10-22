/*
 * linux/sound/soc/codecs/tlv320aic36.h
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
 * 
 */

#ifndef _TLV320AIC36_H
#define _TLV320AIC36_H
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#define AUDIO_NAME "aic36"
#define AIC36_VERSION "0.1"

/* Macro enables or disables support for miniDSP in the driver */
//#define CONFIG_MINI_DSP
//#undef CONFIG_MINI_DSP

//#define	MINIDSP_SAMPLE_8KHZ
#ifndef MINIDSP_SAMPLE_8KHZ
#define MINIDSP_SAMPLE_44KHZ
#endif
/*bypass mode for test*/
#define UPLINK_CHANNEL_BYPASS_MODE
#define DOWNLINK_CHANNEL_BYPASS_MODE

#if defined (MINIDSP_SAMPLE_44KHZ) && defined(UPLINK_CHANNEL_BYPASS_MODE)
#define MINIDSP_SINGLE_MODE
#endif

/* AIC36 configation as master/slave */
#define AIC36_CODEC_MCLK_RATE	   		(0x1<<0)
#define AIC36_CODEC_SET_PLL	   			(0x1<<1)

/* AIC36 supported sample rate are 8k to 192k */
#define AIC36_RATES	SNDRV_PCM_RATE_8000_192000

/* AIC36 supports the word formats 16bits, 20bits, 24bits and 32 bits */
#define AIC36_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define AIC36_FREQ_12000000 12000000
#define AIC36_FREQ_24000000 24000000
#define AIC36_FREQ_2048000 2048000
#define AIC36_FREQ_11289600 11289600

/* AIC36 register space */
#define AIC36_CACHEREGNUM	384

#define AIC36_VOICE_BBOUT_GAIN_MAX	9
#define AIC36_VOICE_BBOUT_GAIN_MIN	0

#define AIC36_VOICE_LPGA_GAIN		50
#define AIC36_VOICE_RPGA_GAIN		10
#ifdef DOWNLINK_CHANNEL_BYPASS_MODE
#define AIC36_VOICE_GAIN_MIN		0
#define AIC36_VOICE_GAIN_MAX		50
#else
#define AIC36_VOICE_GAIN_MIN		160//129
#define AIC36_VOICE_GAIN_MAX		255
#endif
#define AIC36_VOICE_LEVEL_MAX		10

/* Audio data word length = 16-bits (default setting) */
#define AIC36_WORD_LEN_16BITS		0x00
#define AIC36_WORD_LEN_20BITS		0x01
#define AIC36_WORD_LEN_24BITS		0x02
#define AIC36_WORD_LEN_32BITS		0x03
#define	DATA_LEN_SHIFT			4

/* sink: name of target widget */
#define AIC36_WIDGET_NAME			0
/* control: mixer control name */
#define AIC36_CONTROL_NAME			1
/* source: name of source name */
#define AIC36_SOURCE_NAME			2

/* D15..D8 aic36 register offset */
#define AIC36_REG_OFFSET_INDEX    0
/* D7...D0 register data */
#define AIC36_REG_DATA_INDEX      1

/* Serial data bus uses I2S mode (Default mode) */
#define AIC36_I2S_MODE				0x00
#define AIC36_DSP_MODE				0x01
#define AIC36_RIGHT_JUSTIFIED_MODE		0x02
#define AIC36_LEFT_JUSTIFIED_MODE		0x03
#define AUDIO_MODE_SHIFT			6

/* number of codec specific register for configuration */
#define NO_FEATURE_REGS			2

/*clock mask*/
#define AIC36_NDAC_VALUE_MSK		(0x7f<<0)
#define AIC36_MDAC_VALUE_MSK		(0x7f<<0)
#define AIC36_NADC_VALUE_MSK		(0x7f<<0)
#define AIC36_MADC_VALUE_MSK		(0x7f<<0)

#define AIC36_PLL_PVALUE_SHIFT		(4)
#define AIC36_PLL_PVALUE_MSK		(0x7<<AIC36_PLL_PVALUE_SHIFT)

#define AIC36_PLL_RVALUE_MSK		(0xf<<0)
#define AIC36_PLL_JVALUE_MSK		(0x3f<<0)

#define AIC36_PLL_DVALUE_M_MSK		(0x3f<<0)
#define AIC36_PLL_DVALUE_L_MSK		(0xff<<0)

#define AIC36_AOSR_VALUE_MSK		(0xff<<0)
#define AIC36_DOSR_VALUE_M_MSK		(0x3<<0)
#define AIC36_DOSR_VALUE_L_MSK		(0xff<<0)

#define AIC36_BLKN_VALUE_MSK		(0x7f<<0)


/* 8 bit mask value */
#define AIC36_8BITS_MASK		0xFF
/* ****************** Page 0 Registers **************************************/
#define	PAGE_SELECT			0
#define	RESET				1
#define	CLK_REG_1			4
#define	CLK_REG_2			5
#define	CLK_REG_3			6
#define	CLK_REG_4			7
#define	CLK_REG_5			8
#define	NDAC_CLK_REG_6			11
#define	MDAC_CLK_REG_7			12
#define DAC_OSR_MSB			13
#define DAC_OSR_LSB			14
#define	NADC_CLK_REG_8			18
#define	MADC_CLK_REG_9			19
#define ADC_OSR_REG			20
#define CLK_MUX_REG_9			23
#define CLK_REG_10			24
#define INTERFACE_SET_REG_1		25
#define AIS_REG_2			26
#define AIS_REG_3			27
#define CLK_REG_11			28
#define AIS_REG_4			30
#define AIS_REG_5			31
#define AIS_REG_6			32
#define AIS_REG_7			33
#define ADC_STATUS_REG			36
#define DAC_STATUS_REG			37
#define DAC_STICKY_REG			38
#define STICKY_INTR_FLAG_REG4   50
#define RLTIME_INTR_FLAG_REG4   51
#define INTR_EN_INT1_REG4   52
#define INTR_EN_INT2_REG4   53

#define DAC_CHN_REG			63
#define DAC_MUTE_CTRL_REG		64
#define LDAC_VOL			65
#define RDAC_VOL			66
#define DRC_REG_1			68
#define DRC_REG_2			69
#define DRC_REG_3			70
#define ADC_REG_1			81
#define	ADC_FGA				82
#define LADC_VOL			83
#define RADC_VOL			84
#define LEFT_CHN_AGC_1			86
#define LEFT_CHN_AGC_2			87
#define LEFT_CHN_AGC_3			88
#define LEFT_CHN_AGC_4			89
#define LEFT_CHN_AGC_5			90
#define LEFT_CHN_AGC_6			91
#define LEFT_CHN_AGC_7			92
#define LEFT_CHN_AGC_8			93
#define RIGHT_CHN_AGC_1			94
#define RIGHT_CHN_AGC_2 		95
#define RIGHT_CHN_AGC_3			96
#define RIGHT_CHN_AGC_4			97
#define RIGHT_CHN_AGC_5			98
#define RIGHT_CHN_AGC_6			99
#define RIGHT_CHN_AGC_7			100
#define RIGHT_CHN_AGC_8			101
#define ANALOG_FIR_REG1			114
#define ANALOG_FIR_REG2			115
#define ANALOG_FIR_REG3			116
#define GPIO1_CTRL              120
#define GPIO2_CTRL		        121
#define DOUT_CTRL			    126
#define DIN_CTRL			    127

/******************** Page 1 Registers **************************************/
#define PAGE_1				128
#define ADC_CHN_ACONF1			(PAGE_1 + 10)
#define ADC_MIC_REV1			(PAGE_1 + 34)
#define ADC_MIC_REV2			(PAGE_1 + 36)
#define ADC_MIC_REV3			(PAGE_1 + 37)
#define ADC_MIC_REV4			(PAGE_1 + 39)
#define ADC_CHN_ACONF2			(PAGE_1 + 51)
#define L_MICPGA_P			(PAGE_1 + 52)
#define L_MICPGA_N			(PAGE_1 + 54)
#define R_MICPGA_P			(PAGE_1 + 55)
#define R_MICPGA_N			(PAGE_1 + 57)
#define CM_SETTINGS_UNUSED_INPUTS			(PAGE_1 + 58)
#define L_MICPGA_V			(PAGE_1 + 59)
#define R_MICPGA_V			(PAGE_1 + 60)
#define ADC_LOW_CURRENT			(PAGE_1 + 61)
#define ADC_ANALOG_VOL_CTRL		(PAGE_1 + 62)

/******************** Page 2 Registers **************************************/
#define PAGE_2				256
#define DETECT_DIV_CTRL  (PAGE_2 + 20)
#define DETECT_DIV1_VAL  (PAGE_2 + 21)
#define DETECT_DIV2_VAL  (PAGE_2 + 22)
#define DETECT_CTRL      (PAGE_2 + 23)
#define SHORT_PULSE_DETECT  (PAGE_2 + 24)
#define LONG_PULSE_DETECT   (PAGE_2 + 25)
#define SHORT_PULSE_PERIOD  (PAGE_2 + 26)
#define LONG_PULSE_PERIOD   (PAGE_2 + 27)

#define DETECT_THR1_CTRL (PAGE_2 + 28)
#define DETECT_THR2_CTRL (PAGE_2 + 29)
#define DETECT_THR3_CTRL (PAGE_2 + 30)
#define DETECT_THR4_CTRL (PAGE_2 + 31)
#define DETECT_THR5_CTRL (PAGE_2 + 32)
#define DETECT_THR6_CTRL (PAGE_2 + 33)
#define DETECT_THR7_CTRL (PAGE_2 + 34)
#define	MIC_BIAS			(PAGE_2 + 35)
#define DAC_PWR				(PAGE_2 + 37)
#define	HIGH_PWR_OUT			(PAGE_2 + 40)
#define POP_REDUCTION_REG		(PAGE_2 + 42)
#define	LINEINL_2_HPL_VOL		(PAGE_2 + 45)
#define PGAL_2_HPL_VOL			(PAGE_2 + 46)
#define DACL_2_HPL_VOL			(PAGE_2 + 47)
#define LINEINR_2_HPL_VOL		(PAGE_2 + 48)
#define PGAR_2_HPL_VOL			(PAGE_2 + 49)
#define DACR_2_HPL_VOL			(PAGE_2 + 50)
#define HPL_OUT_VOL			(PAGE_2 + 51)
#define	LINEINL_2_HPR_VOL		(PAGE_2 + 52)
#define PGAL_2_HPR_VOL                  (PAGE_2 + 53)
#define DACL_2_HPR_VOL			(PAGE_2 + 54)
#define LINEINR_2_HPR_VOL		(PAGE_2 + 55)
#define PGAR_2_HPR_VOL			(PAGE_2 + 56)
#define	DACR_2_HPR_VOL			(PAGE_2 + 57)
#define HPR_OUT_VOL			(PAGE_2 + 58)
#define	LINEINL_2_RECL_VOL		(PAGE_2 + 59)
#define	PGAL_2_RECL_VOL			(PAGE_2 + 60)
#define	DACL_2_RECL_VOL			(PAGE_2 + 61)
#define	LINEINR_2_RECL_VOL		(PAGE_2 + 62)
#define	PGAR_2_RECL_VOL			(PAGE_2 + 63)
#define	DACR_2_RECL_VOL			(PAGE_2 + 64)
#define	RECL_OUT_VOL			(PAGE_2 + 65)
#define	LINEINL_2_RECR_VOL		(PAGE_2 + 66)
#define	PGAL_2_RECR_VOL			(PAGE_2 + 67)
#define	DACL_2_RECR_VOL			(PAGE_2 + 68)
#define	LINEINR_2_RECR_VOL		(PAGE_2 + 69)
#define	PGAR_2_RECR_VOL			(PAGE_2 + 70)
#define	DACR_2_RECR_VOL			(PAGE_2 + 71)
#define	RECR_OUT_VOL			(PAGE_2 + 72)
#define	LINEINL_2_LINEOUT_LPM		(PAGE_2 + 80)
#define	PGAL_2_LINEOUT_LPM		(PAGE_2 + 81)
#define	DACL_2_LINEOUT_LPM		(PAGE_2 + 82)
#define	LINEINR_2_LINEOUT_LPM		(PAGE_2 + 83)
#define	PGAR_2_LINEOUT_LPM		(PAGE_2 + 84)
#define	DACR_2_LINEOUT_LPM		(PAGE_2 + 85)
#define	LINEOUT_LPM_VOL			(PAGE_2 + 86)
#define	LINEINL_2_LINEOUT_RPM		(PAGE_2 + 87)
#define	PGAL_2_LINEOUT_RPM		(PAGE_2 + 88)
#define	DACL_2_LINEOUT_RPM		(PAGE_2 + 89)
#define	LINEINR_2_LINEOUT_RPM		(PAGE_2 + 90)
#define	PGAR_2_LINEOUT_RPM		(PAGE_2 + 91)
#define	DACR_2_LINEOUT_RPM		(PAGE_2 + 92)
#define	LINEOUT_RPM_VOL			(PAGE_2 + 93)
#define	MODULE_POWER_STATUS			(PAGE_2 + 94)
#define LOW_PWR_OSC_CTRL        (PAGE_2 + 112)
#define PMU_CONF_REG1			(PAGE_2 + 113)
#define PMU_CONF_REG2			(PAGE_2 + 114)
#define PMU_CONF_REG3			(PAGE_2 + 115)
#define PMU_CONF_REG4			(PAGE_2 + 116)
#define PMU_CONF_REG5			(PAGE_2 + 118)
#define PMU_CONF_REG6			(PAGE_2 + 119)
#define PMU_CONF_REG7			(PAGE_2 + 120)

#define PAGE_8				1024
#define DAC_ADAPT_STATUS			(PAGE_8 + 1)

/****************************************************************************/
/****************************************************************************/
#define BIT7		(1 << 7)
#define BIT6		(1 << 6)
#define BIT5		(1 << 5)
#define BIT4		(1 << 4)
#define	BIT3		(1 << 3)
#define BIT2		(1 << 2)
#define BIT1		(1 << 1)
#define BIT0		(1 << 0)

#define HPL_UNMUTE	BIT3
#define ENABLE_DAC_CHN			(BIT6 | BIT7)
#define ENABLE_ADC_CHN			(BIT6 | BIT7)
#define CODEC_CLKIN_MASK		0x03
#define MCLK_2_CODEC_CLKIN		0x00
#define PLLCLK_2_CODEC_CLKIN		BIT3
/*Bclk_in selection*/
#define BDIV_CLKIN_MASK			0x03
#define	DAC_MOD_CLK_2_BDIV_CLKIN 	BIT0
#define	ADC_CLK_2_BDIV_CLKIN 	BIT1
#define SOFT_RESET			0x01
#define PAGE0				0x00
#define PAGE1				0x01
#define BIT_CLK_MASTER			BIT3
#define WORD_CLK_MASTER			BIT2
#define	HIGH_PLL 			BIT6
#define ENABLE_PLL			BIT7
#define ENABLE_NDAC			BIT7
#define ENABLE_MDAC			BIT7
#define ENABLE_NADC			BIT7
#define ENABLE_MADC			BIT7
#define ENABLE_BCLK			BIT7
#define ENABLE_LDAC			BIT7
#define ENABLE_RDAC			BIT6
#define ENABLE_LADC			BIT7
#define ENABLE_RADC			BIT6
#define LDAC_2_LCHN			BIT4
#define RDAC_2_RCHN			BIT2
#define LDAC_CHNL_2_HPL			BIT3
#define RDAC_CHNL_2_HPR			BIT3
#define SOFT_STEP_2WCLK			BIT0
#define MUTE_ON				0x0C
#define ANALOG_MUTE_ON			0x08
#define DEFAULT_VOL			0x0
#define DISABLE_ANALOG			BIT3
#define LDAC_2_HPL_ROUTEON		BIT3
#define RDAC_2_HPR_ROUTEON		BIT3
#define LINEIN_L_2_LMICPGA_10K		BIT6
#define LINEIN_L_2_LMICPGA_20K		BIT7
#define LINEIN_L_2_LMICPGA_40K		(0x3 << 6)
#define LINEIN_R_2_RMICPGA_10K		BIT6
#define LINEIN_R_2_RMICPGA_20K		BIT7
#define LINEIN_R_2_RMICPGA_40K		(0x3 << 6)

#define MIC1_M_2_LMICPGA_10K		BIT4
#define MIC1_M_2_LMICPGA_20K		BIT5
#define MIC1_M_2_LMICPGA_40K		(0x3 << 4)
#define MIC1_P_2_LMICPGA_10K		BIT4
#define MIC1_P_2_LMICPGA_20K		BIT5
#define MIC1_P_2_LMICPGA_40K		(0x3 << 4)

#define MIC2_P_2_RMICPGA_10K		BIT4
#define MIC2_P_2_RMICPGA_20K		BIT5
#define MIC2_P_2_RMICPGA_40K		(0x3 << 4)
#define MIC2_M_2_RMICPGA_10K		BIT4
#define MIC2_M_2_RMICPGA_20K		BIT5
#define MIC2_M_2_RMICPGA_40K		(0x3 << 4)

#define EXTMIC_P_2_LMICPGA_10K		BIT2
#define EXTMIC_P_2_LMICPGA_20K		BIT3
#define EXTMIC_P_2_LMICPGA_40K		(0x3 << 2)
#define EXTMIC_P_2_RMICPGA_10K		BIT2
#define EXTMIC_P_2_RMICPGA_20K		BIT3
#define EXTMIC_P_2_RMICPGA_40K		(0x3 << 2)
#define EXTMIC_M_2_LMICPGA_10K		BIT2
#define EXTMIC_M_2_LMICPGA_20K		BIT3
#define EXTMIC_M_2_LMICPGA_40K		(0x3 << 2)
#define EXTMIC_M_2_RMICPGA_10K		BIT2
#define EXTMIC_M_2_RMICPGA_20K		BIT3
#define EXTMIC_M_2_RMICPGA_40K		(0x3 << 2)

//"Speaker","Headphone","Receiver",
//"Speaker incall","Headphone incall","Receiver incall",
//"Speaker ringtone","Headphone ringtone","Receiver ringtone",
//"None"
/*  the path of Playback & Capture */

enum _playback_path {
		PLAYBACK_SPK_NORMAL  = 0,
		PLAYBACK_HP_NORMAL,
		PLAYBACK_REC_NORMAL,
		PLAYBACK_SPK_HP_NORMAL,
		PLAYBACK_SPK_INCALL,
		PLAYBACK_HS_INCALL,
		PLAYBACK_REC_INCALL,
		PLAYBACK_SPK_HS_INCALL,
		PLAYBACK_SPK_RING,
		PLAYBACK_HP_RING,
		PLAYBACK_REC_RING,
		PLAYBACK_SPK_HP_RING,
		PLAYBACK_HP_INCALL,
		PLAYBACK_SPK_HP_INCALL,
		
		PLAYBACK_SPK_ANSWER_INCALL,
		PLAYBACK_HS_ANSWER_INCALL,
		PLAYBACK_REC_ANSWER_INCALL,
		PLAYBACK_SPK_HS_ANSWER_INCALL,
		PLAYBACK_HP_ANSWER_INCALL,
		PLAYBACK_SPK_HP_ANSWER_INCALL,
		
		PLAYBACK_NONE,//default
};
enum _capture_path {
		CAPTURE_INT_MIC_NORMAL = 0,
		CAPTURE_INT_MIC_INCALL,
		CAPTURE_EXT_MIC_NORMAL,
		CAPTURE_EXT_MIC_INCALL,
		CAPTURE_NONE,//default
};


/***************************************************************************** 
 * Structures Definitions
 ***************************************************************************** 
 */
/*
 *----------------------------------------------------------------------------
 * @struct  aic36_setup_data |
 *          i2c specific data setup for AIC36.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
struct aic36_setup_data
{
	int i2c_bus;
	unsigned short i2c_address;
	unsigned int gpio_func[2];
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic36_priv |
 *          AIC36 priviate data structure to set the system clock, mode and
 *          page number. 
 * @field   u32 | sysclk |
 *          system clock
 * @field   s32 | master |
 *          master/slave mode setting for AIC36
 * @field   u8 | page_no |
 *          page number. Here, page 0 and page 1 are used.
 *----------------------------------------------------------------------------
 */
struct aic36_priv
{
  u32 sysclk;
  s32 master;
  u8 page_no;

  u8 playback_active;
  u8 playback_path;
  u8 capture_active;
  u8 capture_path;
#ifdef CONFIG_HAS_WAKELOCK
  struct wake_lock aic36_wake_lock;
#endif
  struct work_struct codec_status_wq;
  struct timer_list timer;
  struct mutex mutex; 
  u8  mute;
  u32 voice_gain[AIC36_VOICE_LEVEL_MAX+1];//for cta test
  u8 voice_volume;
  u8 voice_mic_gain;

  u8 music_volume; 
  u8 check_flags;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic36_configs |
 *          AIC36 initialization data which has register offset and register 
 *          value.
 * @field   u16 | reg_offset |
 *          AIC36 Register offsets required for initialization..
 * @field   u8 | reg_val |
 *          value to set the AIC36 register to initialize the AIC36.
 *----------------------------------------------------------------------------
 */
struct aic36_configs
{
  u16 reg_offset;
  u8 reg_val;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic36_rate_divs |
 *          Setting up the values to get different freqencies 
 *          
 * @field   u32 | mclk |
 *          Master clock 
 * @field   u32 | rate |
 *          sample rate
 * @field   u8 | p_val |
 *          value of p in PLL
 * @field   u32 | pll_j |
 *          value for pll_j
 * @field   u32 | pll_d |
 *          value for pll_d
 * @field   u32 | dosr |
 *          value to store dosr
 * @field   u32 | ndac |
 *          value for ndac
 * @field   u32 | mdac |
 *          value for mdac
 * @field   u32 | aosr |
 *          value for aosr
 * @field   u32 | nadc |
 *          value for nadc
 * @field   u32 | madc |
 *          value for madc
 * @field   u32 | blck_N |
 *          value for block N
 * @field   u8 | r_val |
 *          value of r in PLL
 * @field   u32 | aic36_configs |
 *          configurations for aic36 register value
 *----------------------------------------------------------------------------
 */
struct aic36_rate_divs
{
  u32 mclk;
  u32 rate;
  u8 p_val;
  u8 pll_j;
  u16 pll_d;
  u16 dosr;
  u8 ndac;
  u8 mdac;
  u8 aosr;
  u8 nadc;
  u8 madc;
  u8 blck_N;
  u8 r_val;
  struct aic36_configs codec_specific_regs[NO_FEATURE_REGS];
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic36_probe(), aic36_remove(), 
 *			aic36_suspend() and aic36_resume()
 *
 */

extern struct platform_device *m9w_snd_device;
#endif /* _TLV320AIC36_H */
