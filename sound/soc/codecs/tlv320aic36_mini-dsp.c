/*
 * linux/sound/soc/codecs/tlv320aic36_mini-dsp.c
 *
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 *
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
 * Rev 0.1 	 mini DSP support    		Mistral         08-12-2009
 * Rev 0.2 	 voice mode support    		Mistral         18-05-2010
 * Rev 0.3   	 multi-byte coefficient update
 *           	 Support                    	Mistral         08-06-2010
 * Rev 0.4   	 Optimized Mode switching
 *           	 Logic                      	Mistral         10-06-2010
 *
 *          The mini DSP programming support is added to codec AIC36.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/control.h>
#include <linux/time.h>		/* For timing computations */
#include "tlv320aic36.h"
#include "tlv320aic36_mini-dsp.h"


/* enable debug prints in the driver */
//#define DEBUG
#undef DEBUG

#ifdef CONFIG_MINI_DSP

//#define VERIFY_MINIDSP                                1

/* Function prototypes */
#ifdef REG_DUMP_MINIDSP
static void aic36_dump_page (struct snd_soc_codec *codec, u8 page);
#endif

/* EXTERN DECLARATIONS *****************************************************/
extern int aic36_change_page (struct snd_soc_codec *codec, u8 new_page);
extern int aic36_write (struct snd_soc_codec *codec, u16 reg, u8 value);
extern unsigned int aic36_read (struct snd_soc_codec *codec, u16 reg);
extern unsigned int aic36_read_status(struct snd_soc_codec * codec, unsigned int reg);
static int minidsp_driver_init (struct snd_soc_codec *codec);
extern int aic36_reset_cache (struct snd_soc_codec *codec);



/* GLOBAL DECLARATIONS *****************************************************/
/* Magic Number used by the TiLoad Application for parsing PPS generated
 * .CFG Files
 */
static unsigned int magic_num;

extern u8 voice_mode_enable;
#define	EQ_SEL_HP	1
#define	EQ_SEL_SPK	2
#define	EQ_SEL_DIRECT	3
int mEQ_Select = EQ_SEL_DIRECT-1;

int mDRCOnOff = 0;

/* Global Array used to store the co-efficient values given by user at Run-time */
//static u8 coeff_array[256];

/* Mistral: Added the minidsp_parser_data type global variables which will
 * parse the miniDSP Header files and keep the burst miniDSP write
 * configuration prepared even before the user wishes to switch from one
 * mode to another.
 */
minidsp_parser_data dsp_parse_data[MINIDSP_PARSER_ARRAY_SIZE*2];

/* Mistral: For switching between Multiple miniDSP Modes, it is required
 * to update the Codec Registers across Several Pages. The information
 * on the Pages and Register Offsets with values to be written will be
 * stored in the above minidsp_parser_data Arrays. However, for 
 * communicating them to the Codec, we still need the i2c_msg structure.
 * so instead of initializing the i2c_msg at run-time, we will also 
 * populate a global array of the i2c_msg structures and keep it
 * ready 
 */
struct i2c_msg i2c_transaction[MINIDSP_PARSER_ARRAY_SIZE * 2]; 

/* Actual Count of the above Arrays */
int music_i2c_count = 0;
int voice_i2c_count = 0;

minidsp_i2c_page music_i2c_page_array[MINIDSP_PARSER_ARRAY_SIZE];
minidsp_i2c_page voice_i2c_page_array[MINIDSP_PARSER_ARRAY_SIZE];

/* Actual Count of the above arrays */
int music_i2c_page_count = 0;
int voice_i2c_page_count = 0;

const u16 eq_coeffs[5][42][5] = {
#include "hgeq_coeffs.h"
};



#define	MAX_EQ_LEVEL		20
#define	MAX_EQ_BAND_NUMS 	5
static const char *snd_eq_volume_str[MAX_EQ_BAND_NUMS] = {"EQ1 Playback Volume", "EQ2 Playback Volume", "EQ3 Playback Volume", "EQ4 Playback Volume", "EQ5 Playback Volume"};
static struct snd_kcontrol_new snd_eq_volume_controls[MAX_EQ_BAND_NUMS];
static u8 snd_eq_volume[MAX_EQ_BAND_NUMS];
static int set_eq_value_bytable (struct snd_soc_codec *codec,u8 eq_band,u8 eq_value);

static int put_EQ_Select (struct snd_soc_codec *codec,int EQMode);
/******************************** Debug section *****************************/

#ifdef REG_DUMP_MINIDSP

/***************************************************************************
 * \brief aic36_dump_page
 *
 * Read and display one codec register page, for debugging purpose
 *
 * \return
 *       void
 */
static void aic36_dump_page (struct snd_soc_codec *codec, u8 page)
{
	int i;
	u8 data;
	u8 test_page_array[256];
	struct i2c_client *i2c = codec->control_data;
	aic36_change_page (codec, page);

	data = 0x0;

	i2c_master_send (i2c, data, 1);
	i2c_master_recv (i2c, test_page_array, 128);

	pr_debug ("\n------- MINI_DSP PAGE %d DUMP --------\n", page);
	for (i = 0; i < 128; i++)
	{
		pr_debug (" [ %d ] = 0x%x\n", i, test_page_array[i]);
	}
}
#endif
/*
 *----------------------------------------------------------------------------
 * Function : aic36_write_reg_cache
 * Purpose  : This function is to write aic36 register cache
 *            
 *----------------------------------------------------------------------------
 */
int switch_minidsp_runtime_buffer(struct snd_soc_codec *codec)
{
	u8 value;
	//	u8 page;
	int i;
	//	struct i2c_client *i2c = codec->control_data;

	value = aic36_read_status(codec, DAC_STATUS_REG);
	if(value & 0x88)
	{
		value = aic36_read_status(codec, DAC_ADAPT_STATUS);
		value |= 0x01;
		if(!aic36_write(codec, DAC_ADAPT_STATUS, value))
		{
			for(i=0; i<5; i++)
			{
				value = aic36_read_status(codec, DAC_ADAPT_STATUS);
				if(!(value & 0x01))
				{
					pr_debug ("Codec A/B buffer switched!\n");
					break;
				}
				msleep(5);
			}
		}
	}
	return 0;
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

	if (cache && reg >= AIC36_CACHEREGNUM)
	{
		return;
	}
	cache[reg] = value;
}

/******************** MINI DSP Static Programming section *******************/

/***********************************************************************/
/* brief aic36_minidsp_write_burst
 *
 * Write one I2C burst to the codec registers. The buffer for burst
 * transfer is given by aic36_minidsp_get_burst() function.
 *
 * return
 *       int - 0 on success and -1 on failure
 */
	static int
aic36_minidsp_write_burst (struct snd_soc_codec *codec,
		minidsp_parser_data * parse_data)
{
#ifdef VERIFY_MINIDSP
	int i;
	char read_addr;
	char test_page_array[256];
#endif
	struct i2c_client *i2c = codec->control_data;

	aic36_change_page (codec, parse_data->page_num);

	/* write burst data */
	if ((i2c_master_send (i2c, parse_data->burst_array,
					parse_data->burst_size)) != parse_data->burst_size)
	{
		pr_debug ("Mini DSP: i2c_master_send failed\n");
		return -1;
	}
#ifdef VERIFY_MINIDSP
	read_addr = parse_data->burst_array[0];
	i2c_master_send (i2c, &read_addr, 1);

	if ((i2c_master_recv (i2c, test_page_array, parse_data->burst_size))
			!= parse_data->burst_size)
	{
		pr_debug ("Mini DSP: i2c_master_recv failed\n");
		return -1;
	}

	for (i = 0; i < parse_data->burst_size - 1; i++)
	{
		if (test_page_array[i] != parse_data->burst_array[i + 1])
		{
			pr_debug
				("MINI DSP program verification failure on page 0x%x\n",
				 parse_data->page_num);
			return -1;
		}
	}
	pr_debug ("MINI DSP program verification success on page 0x%x\n",
			parse_data->page_num);
#endif

	return 0;
}



/***************************************************************************
 * \brief aic36_minidsp_get_musice_burst
 *
 * Format one I2C burst for transfer from mini dsp program array. This function 
 * will parse the program array and get next burst data for doing an 
 * I2C bulk transfer for the music mode.
 *
 * \return
 *       void
 */

	static void
aic36_minidsp_get_musice_burst (music_reg_value * program_ptr,
		int program_size,
		minidsp_parser_data * parse_data)
{
	int index = parse_data->current_loc;
	int burst_write_count = 0;

	/* check if first location is page register, and populate page addr */
	if (program_ptr[index].reg_off == 0)
	{
		parse_data->page_num = program_ptr[index].reg_val;
		parse_data->burst_array[burst_write_count++] = program_ptr[index].reg_off;
		parse_data->burst_array[burst_write_count++] = program_ptr[index].reg_val;	  
		index++;
		goto finish_out;	  
	}

	parse_data->burst_array[burst_write_count++] = program_ptr[index].reg_off;
	parse_data->burst_array[burst_write_count++] = program_ptr[index].reg_val;
	index++;

	for (; index < program_size; index++)
	{
		if (program_ptr[index].reg_off != (program_ptr[index - 1].reg_off + 1))
			break;
		else
		{
			parse_data->burst_array[burst_write_count++] =
				program_ptr[index].reg_val;
		}
	}
finish_out:
	parse_data->burst_size = burst_write_count;
	if (index == program_size)
	{
		/* parsing completed */
		parse_data->current_loc = MINIDSP_PARSING_END;
	}
	else
		parse_data->current_loc = index;
}

#ifndef MINIDSP_SINGLE_MODE
/***************************************************************************
 * \brief aic36_minidsp_get_voice_burst
 *
 * Format one I2C burst for transfer from mini dsp program array. This function 
 * will parse the program array and get next burst data for doing an 
 * I2C bulk transfer for the music mode.
 *
 * \return
 *       void
 */

	static void
aic36_minidsp_get_voice_burst (voice_reg_value * program_ptr,
		int program_size,
		minidsp_parser_data * parse_data)
{
	int index = parse_data->current_loc;
	int burst_write_count = 0;

	/* check if first location is page register, and populate page addr */
	if (program_ptr[index].reg_off == 0)
	{
		parse_data->page_num = program_ptr[index].reg_val;
		parse_data->burst_array[burst_write_count++] = program_ptr[index].reg_off;
		parse_data->burst_array[burst_write_count++] = program_ptr[index].reg_val;	  
		index++;
		goto finish_out;	  
	}

	parse_data->burst_array[burst_write_count++] = program_ptr[index].reg_off;
	parse_data->burst_array[burst_write_count++] = program_ptr[index].reg_val;
	index++;

	for (; index < program_size; index++)
	{
		if (program_ptr[index].reg_off != (program_ptr[index - 1].reg_off + 1))
			break;
		else
		{
			parse_data->burst_array[burst_write_count++] =
				program_ptr[index].reg_val;
		}
	}
finish_out:
	parse_data->burst_size = burst_write_count;
	if (index == program_size)
	{
		/* parsing completed */
		parse_data->current_loc = MINIDSP_PARSING_END;
	}
	else
		parse_data->current_loc = index;
}
#endif

/***************************************************************************
 * \brief minidsp_brust_write_musice_program
 *
 * Configures the AIC36 register map as per the PPS generated MUSIC mode
 * Header file settings.  
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
minidsp_brust_write_musice_program (struct snd_soc_codec *codec,
		music_reg_value * program_ptr, int program_size)
{
	struct i2c_client *client = codec->control_data;

	minidsp_parser_data parse_data;
	int count=0;

	/* point the current location to start of program array */
	parse_data.current_loc = 0;
	parse_data.page_num = 0;
	do
	{
		/* Get first burst data */
		aic36_minidsp_get_musice_burst (program_ptr, program_size, &parse_data);
		dsp_parse_data[count] = parse_data;
		pr_debug ("Burst,PAGE=0x%x Size=%d\n", parse_data.page_num,
				parse_data.burst_size);

		i2c_transaction[count].addr = client->addr;
		i2c_transaction[count].flags = client->flags & I2C_M_TEN;
		i2c_transaction[count].len = dsp_parse_data[count].burst_size;
		i2c_transaction[count].buf = dsp_parse_data[count].burst_array;

		count++;
		/* Proceed to the next burst reg_addr_incruence */
	}
	while (parse_data.current_loc != MINIDSP_PARSING_END);
	if(count>0)
	{
		if(i2c_transfer(client->adapter, i2c_transaction, count) != count)
		{
			pr_debug ("Write brust i2c data error!\n");
		}
		//printk("%s: transfer count=%d\n", __func__, count);
	}
	return 0;
}

#ifndef MINIDSP_SINGLE_MODE
/***************************************************************************
 * \brief minidsp_brust_write_musice_program
 *
 * Configures the AIC36 register map as per the PPS generated MUSIC mode
 * Header file settings.  
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
minidsp_brust_write_voice_program (struct snd_soc_codec *codec,
		voice_reg_value * program_ptr, int program_size)
{
	struct i2c_client *client = codec->control_data;

	minidsp_parser_data parse_data;
	int count=0;

	/* point the current location to start of program array */
	parse_data.current_loc = 0;
	parse_data.page_num = 0;

	do
	{
		/* Get first burst data */
		aic36_minidsp_get_voice_burst (program_ptr, program_size, &parse_data);
		dsp_parse_data[count] = parse_data;
		pr_debug ("Burst,PAGE=0x%x Size=%d\n", parse_data.page_num,
				parse_data.burst_size);

		i2c_transaction[count].addr = client->addr;
		i2c_transaction[count].flags = client->flags & I2C_M_TEN;
		i2c_transaction[count].len = dsp_parse_data[count].burst_size;
		i2c_transaction[count].buf = dsp_parse_data[count].burst_array;

		count++;
		/* Proceed to the next burst reg_addr_incruence */
	}
	while (parse_data.current_loc != MINIDSP_PARSING_END);
	if(count>0)
	{
		if(i2c_transfer(client->adapter, i2c_transaction, count) != count)
		{
			pr_debug ("Write brust i2c data error!\n");
		}
		//printk("%s: transfer count=%d\n", __func__, count);
	}
	return 0;
}


	int
set_minidsp_voice_mode(struct snd_soc_codec *codec)

{
	int i=0;
	int page=0;
	int reg = 0;

	printk("%s: switch voice mode start\n", __func__);

	aic36_reset_cache(codec);
	/* Array size should be greater than 1 to start programming,
	 *	*		   * since first write command will be the page register 
	 *	   *				   */
	if (ARRAY_SIZE (REG_voice_Section_init_program) > 1)
	{
		minidsp_brust_write_voice_program (codec, REG_voice_Section_init_program,
				ARRAY_SIZE
				(REG_voice_Section_init_program));

		for(i=0; i<ARRAY_SIZE(REG_voice_Section_init_program); i++)
		{
			if(0 == REG_voice_Section_init_program[i].reg_off)
			{
				page = REG_voice_Section_init_program[i].reg_val;
				continue;
			}
			reg = page*128 + REG_voice_Section_init_program[i].reg_off;
			aic36_write_reg_cache (codec, reg, REG_voice_Section_init_program[i].reg_val);
		}

	}
	else
	{
		pr_debug ("MUSIC_CODEC_REGS: Insufficient data for programming\n");
	}
	if (ARRAY_SIZE (miniDSP_A_voice_reg_values) > 1)
	{
		minidsp_brust_write_voice_program(codec, miniDSP_A_voice_reg_values,
				ARRAY_SIZE (miniDSP_A_voice_reg_values));
	}
	else
	{
		pr_debug ("MINI_DSP_A_MUSIC: Insufficient data for programming\n");
	}

	if (ARRAY_SIZE (miniDSP_D_voice_reg_values) > 1)
	{
		minidsp_brust_write_voice_program(codec, miniDSP_D_voice_reg_values,
				ARRAY_SIZE (miniDSP_D_voice_reg_values));
	}
	else
	{
		pr_debug ("MINI_DSP_D_MUSIC: Insufficient data for programming\n");
	}

	if (ARRAY_SIZE (REG_voice_Section_post_program) > 1)
	{
		minidsp_brust_write_voice_program (codec, REG_voice_Section_post_program,
				ARRAY_SIZE
				(REG_voice_Section_post_program));
		for(i=0; i<ARRAY_SIZE(REG_voice_Section_post_program); i++)
		{
			if(0 == REG_voice_Section_post_program[i].reg_off)
			{
				page = REG_voice_Section_post_program[i].reg_val;
				continue;
			}
			reg = page*128 + REG_voice_Section_post_program[i].reg_off;
			aic36_write_reg_cache (codec, reg, REG_voice_Section_post_program[i].reg_val);
		}
	}
	else
	{
		pr_debug ("MUSIC_CODEC_REGS: Insufficient data for programming\n");
	}
	printk("%s: switch voice mode finished\n", __func__);	
	return 0;

}
#endif
	int
set_minidsp_music_mode(struct snd_soc_codec *codec)

{
	int i=0;
	int page=0;
	int reg = 0;

	printk("%s: switch music mode start\n", __func__);

	aic36_reset_cache(codec);
	if (ARRAY_SIZE (REG_music_Section_init_program) > 1)
	{
		minidsp_brust_write_musice_program(codec, REG_music_Section_init_program,
				ARRAY_SIZE(REG_music_Section_init_program));
		for(i=0; i<ARRAY_SIZE(REG_music_Section_init_program); i++)
		{
			if(0 == REG_music_Section_init_program[i].reg_off)
			{
				page = REG_music_Section_init_program[i].reg_val;
				continue;
			}
			reg = page*128 + REG_music_Section_init_program[i].reg_off;
			aic36_write_reg_cache (codec, reg, REG_music_Section_init_program[i].reg_val);
		}
	}
	else
	{
		pr_debug ("MUSIC_CODEC_REGS: Insufficient data for programming\n");
	}

	if (ARRAY_SIZE (miniDSP_A_music_reg_values) > 1)
	{
		minidsp_brust_write_musice_program (codec, miniDSP_A_music_reg_values,
				ARRAY_SIZE (miniDSP_A_music_reg_values));
	}
	else
	{
		pr_debug ("MINI_DSP_A_MUSIC: Insufficient data for programming\n");
	}

	if (ARRAY_SIZE (miniDSP_D_music_reg_values) > 1)
	{
		minidsp_brust_write_musice_program (codec, miniDSP_D_music_reg_values,
				ARRAY_SIZE (miniDSP_D_music_reg_values));
	}
	else
	{
		pr_debug ("MINI_DSP_D_MUSIC: Insufficient data for programming\n");
	}

	if (ARRAY_SIZE (REG_music_Section_post_program) > 1)
	{
		minidsp_brust_write_musice_program(codec, REG_music_Section_post_program,
				ARRAY_SIZE(REG_music_Section_post_program));  
		for(i=0; i<ARRAY_SIZE(REG_music_Section_post_program); i++)
		{
			if(0 == REG_music_Section_post_program[i].reg_off)
			{
				page = REG_music_Section_post_program[i].reg_val;
				continue;
			}
			reg = page*128 + REG_music_Section_post_program[i].reg_off;
			aic36_write_reg_cache (codec, reg, REG_music_Section_post_program[i].reg_val);
		}
	}
	else
	{
		pr_debug ("MUSIC_CODEC_REGS: Insufficient data for programming\n");
	}
	printk("%s: switch music mode finished\n", __func__);
	return 0;
}


/***************************************************************************
 * \brief aic36_minidsp_program
 *
 * Program mini dsp for AIC36 codec chip. This routine is alled from the 
 * aic36 codec driver, if mini dsp programming is enabled.   
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	int
aic36_minidsp_program (struct snd_soc_codec *codec)
{
	int i, ret;

	pr_info("AIC36: programming mini dsp\n");

#ifdef CODEC_REG_NAMES
	for (i = 0; i < ARRAY_SIZE (REG_music_Section_names); i++)
		pr_debug("%s\n", REG_music_Section_names[i]);

	for (i = 0; i < ARRAY_SIZE (REG_voice_Section_names); i++)
		pr_debug("%s\n", REG_voice_Section_names[i]);
#endif

#ifdef PROGRAM_MINI_DSP_MUSIC
	set_minidsp_music_mode(codec);
#endif
#ifdef PROGRAM_MINI_DSP_VOICE
	set_minidsp_voice_mode(codec);
#endif
	ret = minidsp_driver_init(codec);

	return ret;
}

/********************* AMIXER Controls for mini dsp *************************/

#ifdef ADD_MINI_DSP_CONTROLS

/* Volume Lite coefficents table */
static int volume_lite_table[] = {
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0001, 0x0001, 0x0001, 0x0001,
	0x0001, 0x0001, 0x0001, 0x0001,
	0x0001, 0x0001, 0x0001, 0x0001,
	0x0002, 0x0002, 0x0002, 0x0002,
	0x0002, 0x0002, 0x0002, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003,
	0x0004, 0x0004, 0x0004, 0x0004,
	0x0005, 0x0005, 0x0005, 0x0006,
	0x0006, 0x0006, 0x0007, 0x0007,
	0x0008, 0x0008, 0x0009, 0x0009,
	0x000A, 0x000A, 0x000B, 0x000C,
	0x000D, 0x000D, 0x000E, 0x000F,
	0x0010, 0x0011, 0x0012, 0x0013,
	0x0014, 0x0015, 0x0017, 0x0018,
	0x0019, 0x001B, 0x001D, 0x001E,
	0x0020, 0x0022, 0x0024, 0x0026,
	0x0029, 0x002B, 0x002E, 0x0030,
	0x0033, 0x0036, 0x003A, 0x003D,
	0x0041, 0x0045, 0x0049, 0x004D,
	0x0052, 0x0056, 0x005C, 0x0061,
	0x0067, 0x006D, 0x0073, 0x007A,
	0x0082, 0x0089, 0x0092, 0x009A,
	0x00A3, 0x00B7, 0x00AD, 0x00C2,
	0x00CE, 0x00DA, 0x00E7, 0x00F5,
	0x0103, 0x0113, 0x0123, 0x0134,
	0x0146, 0x015A, 0x016E, 0x0184,
	0x019B, 0x01B3, 0x01CD, 0x01E9,
	0x0206, 0x0224, 0x0245, 0x0267,
	0x028C, 0x02B2, 0x02DB, 0x0307,
	0x0335, 0x0365, 0x0399, 0x03CF,
	0x0409, 0x0447, 0x0487, 0x04CC,
	0x0515, 0x0562, 0x05B4, 0x060A,
	0x0666, 0x06C7, 0x072E, 0x079B,
	0x080E, 0x0888, 0x090A, 0x0993,
	0x0A24, 0x0ABE, 0x0B61, 0x0C0E,
	0x0CC5, 0x0D86, 0x0E53, 0x0F2D,
	0x1013, 0x1107, 0x1209, 0x131B,
	0x143D, 0x1570, 0x16B5, 0x180D,
	0x197A, 0x1AFD, 0x1C96, 0x1E48,
	0x2013, 0x21FA, 0x23FD, 0x261F,
	0x2861, 0x2AC6, 0x2D4E, 0x2FFE,
	0x32D6, 0x35D9, 0x390A, 0x3C6B,
	0x4000, 0x43CA, 0x47CF, 0x4C10,
	0x5092, 0x5558, 0x5A67, 0x5FC2,
	0x656E, 0x6B71, 0x71CF, 0x788D,
	0x7FB2
};

/************************ VolumeLite control section ************************/

static struct snd_kcontrol_new snd_vol_music_controls[MAX_VOLUME_CONTROLS];
static struct snd_kcontrol_new snd_vol_voice_controls[MAX_VOLUME_CONTROLS];

/***************************************************************************
 * \brief __new_control_info_music_minidsp_volume
 *
 * info routine for volumeLite amixer kcontrols 
 * aic36 codec driver, if mini dsp programming is enabled.   
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
static int __new_control_info_music_minidsp_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	int index;
	int ret_val = -1;

	for (index = 0; index < ARRAY_SIZE (VOLUME_music_controls); index++) {
		if (strstr (kcontrol->id.name, VOLUME_music_control_names[index]))
			break;
	}

	if (index < ARRAY_SIZE (VOLUME_music_controls))	{
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = MIN_VOLUME;
		uinfo->value.integer.max = MAX_VOLUME;
		ret_val = 0;
	}
	return ret_val;
}

/***************************************************************************
 * \brief __new_control_get_music_minidsp_volume
 *
 * get routine for amixer kcontrols, read current registervalues. Used for for mini dsp 
 * 'VolumeLite' amixer controls. 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
static int __new_control_get_music_minidsp_volume (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
}

/***************************************************************************
 * \brief __new_control_put_music_minidsp_volume
 *
 * put routine for amixer kcontrols, write user values to registers values. 
 * Used for for mini dsp 'VolumeLite' amixer controls. 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
static int __new_control_put_music_minidsp_volume (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 data[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int index;
	int user_value = ucontrol->value.integer.value[0];
	struct i2c_client *i2c = codec->control_data;
	int ret_val = -1;
	int coeff;

	pr_debug ("user value = 0x%x\n", user_value);

	for (index = 0; index < ARRAY_SIZE (VOLUME_music_controls); index++)
	{
		if (strstr (kcontrol->id.name, VOLUME_music_control_names[index]))
			break;
	}

	if (index < ARRAY_SIZE (VOLUME_music_controls))
	{
		aic36_change_page (codec, VOLUME_music_controls[index].control_page);

		coeff = volume_lite_table[user_value << 1];

		data[1] = (u8) ((coeff >> 8) & AIC36_8BITS_MASK);
		data[2] = (u8) ((coeff) & AIC36_8BITS_MASK);

		/* Start register address */
		data[0] = VOLUME_music_controls[index].control_base;

		ret_val = i2c_master_send (i2c, data, VOLUME_REG_SIZE + 1);

		if (ret_val != VOLUME_REG_SIZE + 1)
		{
			pr_debug ("i2c_master_send transfer failed\n");
		}
		else
		{
			/* store the current level */
			kcontrol->private_value = user_value;
			ret_val = 0;

			/* Enable adaptive filtering for ADC/DAC */
			//data[0] = 0x1;  /* reg 1*/
			//data[1] = 0x05; /* Enable shifting buffer from A to B */

			//printk("Enabling the adaptive filtering & switching the buffer .......... \n");

			//i2c_master_send(i2c, data, 2);
		}

		switch_minidsp_runtime_buffer(codec);

		aic36_change_page (codec, VOLUME_music_controls[index].control_page);

		ret_val = i2c_master_send (i2c, data, VOLUME_REG_SIZE + 1);
		ret_val = 0;

	}

	aic36_change_page (codec, 0);
	return (ret_val);
}

/***************************************************************************
 * \brief minidsp_volume_music_mixer_controls
 *
 * Add amixer kcontrols for mini dsp volume Lite controls, 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
minidsp_volume_music_mixer_controls (struct snd_soc_codec *codec)
{
	int i, err, no_volume_controls;
	static char volume_control_name[MAX_VOLUME_CONTROLS][40];

	no_volume_controls = ARRAY_SIZE (VOLUME_music_controls);

	pr_debug (" %d mixer controls for mini dsp 'volumeLite' \n",
			no_volume_controls);

	if (no_volume_controls)
	{
		for (i = 0; i < no_volume_controls; i++)
		{
			strcpy (volume_control_name[i], VOLUME_music_control_names[i]);
			strcat (volume_control_name[i], VOLUME_KCONTROL_NAME);

			pr_debug ("Volume controls: %s\n", volume_control_name[i]);

			snd_vol_music_controls[i].name = volume_control_name[i];
			snd_vol_music_controls[i].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
			snd_vol_music_controls[i].access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
			snd_vol_music_controls[i].info =
				__new_control_info_music_minidsp_volume;
			snd_vol_music_controls[i].get =
				__new_control_get_music_minidsp_volume;
			snd_vol_music_controls[i].put =
				__new_control_put_music_minidsp_volume;
			/* 
			 *      TBD: read volume reg and update the index number 
			 */
			snd_vol_music_controls[i].private_value = 0;
			snd_vol_music_controls[i].count = 0;

			err = snd_ctl_add(codec->card->snd_card,
					snd_soc_cnew (&snd_vol_music_controls[i],
						codec, NULL, NULL));
			if (err < 0)
			{
				printk ("%s:Invalid control %s\n", __FILE__,
						snd_vol_music_controls[i].name);
			}
		}
	}
	return 0;
}


/***************************************************************************
 * \brief __new_control_info_voice_minidsp_volume
 *
 * info routine for volumeLite amixer kcontrols 
 * aic36 codec driver, if mini dsp programming is enabled.   
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_info_voice_minidsp_volume (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	int index;
	int ret_val = -1;

	for (index = 0; index < ARRAY_SIZE (VOLUME_voice_controls); index++)
	{
		if (strstr (kcontrol->id.name, VOLUME_voice_control_names[index]))
			break;
	}

	if (index < ARRAY_SIZE (VOLUME_voice_controls))
	{
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = MIN_VOLUME;
		uinfo->value.integer.max = MAX_VOLUME;
		ret_val = 0;
	}
	return ret_val;
}

/***************************************************************************
 * \brief __new_control_get_voice_minidsp_volume
 *
 * get routine for amixer kcontrols, read current registervalues. Used for for mini dsp 
 * 'VolumeLite' amixer controls. 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_get_voice_minidsp_volume (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
}

/***************************************************************************
 * \brief __new_control_put_voice_minidsp_volume
 *
 * put routine for amixer kcontrols, write user values to registers values. 
 * Used for for mini dsp 'VolumeLite' amixer controls. 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_put_voice_minidsp_volume (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 data[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int index;
	int user_value = ucontrol->value.integer.value[0];
	struct i2c_client *i2c = codec->control_data;
	int ret_val = -1;
	int coeff;
	//  u8 value[2];

	pr_debug ("user value = 0x%x\n", user_value);

	for (index = 0; index < ARRAY_SIZE (VOLUME_voice_controls); index++)
	{
		if (strstr (kcontrol->id.name, VOLUME_voice_control_names[index]))
			break;
	}

	if (index < ARRAY_SIZE (VOLUME_voice_controls))
	{
		aic36_change_page (codec, VOLUME_voice_controls[index].control_page);

		coeff = volume_lite_table[user_value << 1];

		data[1] = (u8) ((coeff >> 8) & AIC36_8BITS_MASK);
		data[2] = (u8) ((coeff) & AIC36_8BITS_MASK);

		/* Start register address */
		data[0] = VOLUME_voice_controls[index].control_base;

		ret_val = i2c_master_send (i2c, data, VOLUME_REG_SIZE + 1);

		if (ret_val != VOLUME_REG_SIZE + 1)
		{
			pr_debug ("i2c_master_send transfer failed\n");
		}
		else
		{
			/* store the current level */
			kcontrol->private_value = user_value;
			ret_val = 0;
		}

		switch_minidsp_runtime_buffer(codec);

		aic36_change_page (codec, VOLUME_voice_controls[index].control_page);

		ret_val = i2c_master_send (i2c, data, VOLUME_REG_SIZE + 1);
		ret_val = 0;

	}

	aic36_change_page (codec, 0);
	return (ret_val);
}

/***************************************************************************
 * \brief minidsp_volume_voice_mixer_controls
 *
 * Add amixer kcontrols for mini dsp volume Lite controls, 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
minidsp_volume_voice_mixer_controls (struct snd_soc_codec *codec)
{
	int i, err, no_volume_controls;
	static char volume_control_name[MAX_VOLUME_CONTROLS][40];

	no_volume_controls = ARRAY_SIZE (VOLUME_voice_controls);

	pr_debug (" %d mixer controls for mini dsp 'volumeLite' \n",
			no_volume_controls);

	if (no_volume_controls)
	{

		for (i = 0; i < no_volume_controls; i++)
		{
			strcpy (volume_control_name[i], VOLUME_voice_control_names[i]);
			strcat (volume_control_name[i], VOLUME_KCONTROL_NAME);

			pr_debug ("Volume controls: %s\n", volume_control_name[i]);

			snd_vol_voice_controls[i].name = volume_control_name[i];
			snd_vol_voice_controls[i].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
			snd_vol_voice_controls[i].access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
			snd_vol_voice_controls[i].info =
				__new_control_info_voice_minidsp_volume;
			snd_vol_voice_controls[i].get =
				__new_control_get_voice_minidsp_volume;
			snd_vol_voice_controls[i].put =
				__new_control_put_voice_minidsp_volume;
			/* 
			 *      TBD: read volume reg and update the index number 
			 */
			snd_vol_voice_controls[i].private_value = 0;
			snd_vol_voice_controls[i].count = 0;

			err = snd_ctl_add(codec->card->snd_card,
					snd_soc_cnew (&snd_vol_voice_controls[i],
						codec, NULL, NULL));
			if (err < 0)
			{
				printk ("%s:Invalid control %s\n", __FILE__,
						snd_vol_voice_controls[i].name);
			}
		}
	}
	return 0;
}

/************************** VOICE/MUSIC MODE CONTROL section *****************************/
static struct snd_kcontrol_new snd_mode_controls;



/***************************************************************************
 * \brief __new_control_get_minidsp_mode
 *
 * Add amixer kcontrols for mini dsp volume Lite controls, 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_get_minidsp_mode (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = voice_mode_enable;
	return 0;
}

int put_burst_minidsp_mode (struct snd_soc_codec *codec,u8 uVoice, u8 force)
{
	int i;
	int ret_val = -1;
	int user_value = uVoice;

	if( user_value == voice_mode_enable && force == 0)
	{
		return 0;
	}
	pr_debug ("miniDSP: Change miniDSP mode from %s to %s \r\n", voice_mode_enable?"voice":"music",user_value?"voice":"music");

	if (user_value == 0)
	{
		pr_debug("Switching from voice to music mode\n");   
		voice_mode_enable = 0;

#ifndef MINIDSP_SINGLE_MODE	
		set_minidsp_music_mode(codec);
		for(i = 0;i < 5; i++)
		{
			set_eq_value_bytable(codec,i,snd_eq_volume[i]*(40/MAX_EQ_LEVEL));
		}	  
		put_EQ_Select(codec,mEQ_Select);
#else
		if(force)
		{
			set_minidsp_music_mode(codec);
			for(i = 0;i < 5; i++)
			{
				set_eq_value_bytable(codec,i,snd_eq_volume[i]*(40/MAX_EQ_LEVEL));
			}	  
			put_EQ_Select(codec,mEQ_Select);		
		}
#endif	

	}
	else
	{
		pr_debug("Switching from music to voice mode\n");	    
		voice_mode_enable = 1;
#ifndef MINIDSP_SINGLE_MODE	
		set_minidsp_voice_mode(codec);
#endif
	}

	ret_val = 0;

	return (ret_val);
}
/***************************************************************************
 * \brief __new_control_put_burst_minidsp_mode
 *
 * amixer control callback function invoked when user performs the cset
 * option for the miniDSP Mode switch. This is the burst implementation of
 * miniDSP mode switching logic and it internally refers to the global
 * music_dsp_A_parse_data[], music_dsp_D_parse_data[], and
 * voice_dsp_A_parse_data[], voice_dsp_D_parse_data[] arrays for MUSIC and
 * VOICE Mode switching respectively. These arrays will be pre-populated
 * during the Driver Initialization sequence and they will contain the
 * list of programming required for switching the modes.
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_put_burst_minidsp_mode (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int user_value = ucontrol->value.integer.value[0];
	return put_burst_minidsp_mode(codec, user_value, 0);
}
static const char *miniDSP_Mode_Str[] = {"Music", "Voice"};

static const struct soc_enum miniDSP_Mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(miniDSP_Mode_Str), miniDSP_Mode_Str),
};
/***************************************************************************
 * \brief minidsp_mode_mixer_controls
 *
 * Configures the AMIXER Controls for the miniDSP run-time configuration.
 * This routine configures the function pointers for the get put and the info
 * members of the snd_mode_controls and calls the snd_ctl_add routine
 * to register this with the ALSA Library.
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
minidsp_mode_mixer_controls (struct snd_soc_codec *codec)
{
	int err;

	snd_mode_controls.name = "miniDSP Mode Selection";
	snd_mode_controls.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	snd_mode_controls.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
	snd_mode_controls.info = snd_soc_info_enum_ext;//__new_control_info_minidsp_mode;
	snd_mode_controls.get = __new_control_get_minidsp_mode;
#ifdef OLD_MINIDSP_MODE_SWITCH
	snd_mode_controls.put = __new_control_put_minidsp_mode;
#else
	snd_mode_controls.put = __new_control_put_burst_minidsp_mode;
#endif

	snd_mode_controls.private_value = (unsigned long)&miniDSP_Mode_enum[0];
	snd_mode_controls.count = 0;

	err = snd_ctl_add(codec->card->snd_card,
			snd_soc_cnew (&snd_mode_controls, codec, NULL, NULL));
	if (err < 0)
	{
		printk ("%s:Invalid control %s\n", __FILE__, snd_mode_controls.name);
	}

	return err;
}

/*************************MiniDSP EQ Selection *******************************/

/***************************************************************************
 * \brief __new_control_get_EQ_Select
 *
 * Add amixer kcontrols for mini dsp EQ selection controls, 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_get_EQ_Select (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mEQ_Select;
	return 0;
}

static int put_EQ_Select (struct snd_soc_codec *codec,int EQMode)
{
	u8 data[MUX_CTRL_REG_SIZE + 1];
	static int index = -1;
	struct i2c_client *i2c;
	u8 value[2];
	//  u8 page;
	int user_value = EQMode;
	u8 page_offset = 0;

	/* Enable the below line only when reading the I2C Transactions */
	/*u8 read_data[10]; */

	int ret_val = -1;
	i2c = codec->control_data;
	//  page = MUX_music_controls[index].control_page;
#ifndef MINIDSP_SINGLE_MODE
	if(voice_mode_enable)
		return 0;
#endif

	//pr_debug ("user value = 0x%x\n", user_value);
	pr_debug ("miniDSP: Change EQ mode to %s\n",(user_value == 0)?"HP EQ":((user_value == 1)?"SPK EQ":"No EQ"));

	if( index == -1)
	{
		for (index = 0; index < ARRAY_SIZE (MUX_music_controls); index++)
		{
			if (strstr ("Stereo_Mux_1", MUX_music_control_names[index]))
				break;
		}
		if( index == ARRAY_SIZE (MUX_music_controls) )
		{
			index = -1;
			return -1;
		}
	}

	//index = 1;			/// EQ Selection  index
	user_value ++;
	if(user_value < EQ_SEL_HP || user_value > EQ_SEL_DIRECT)
		return -1;

	if (index < ARRAY_SIZE (MUX_music_controls))
	{
		value[0] = aic36_read_status(codec, DAC_ADAPT_STATUS);

		if(  (value[0]&2)  == 2 )
			page_offset = 0; // A
		else
			page_offset = 4; // B


		pr_debug ("Index %d Changing to Page %d\r\n", index,
				MUX_music_controls[index].control_page+page_offset);
		aic36_change_page (codec, MUX_music_controls[index].control_page+page_offset);

		data[1] = (u8) ((user_value >> 8) & AIC36_8BITS_MASK);
		data[2] = (u8) ((user_value) & AIC36_8BITS_MASK);

		/* start register address */
		data[0] = MUX_music_controls[index].control_base;

		pr_debug ("Writing %d %d %d \r\n", data[0], data[1], data[2]);

		ret_val = i2c_master_send (i2c, data, MUX_CTRL_REG_SIZE + 1);

		if (ret_val != MUX_CTRL_REG_SIZE + 1)
		{
			pr_debug ("i2c_master_send transfer failed\n");
		}
		else
		{	
			/* store the current level */
			//mEQ_Select = ucontrol->value.integer.value[0];
			mEQ_Select = EQMode;

			ret_val = 0;
			/* Enable adaptive filtering for ADC/DAC */

		}

		/* Following block of code for testing the previous I2C WRITE Transaction. 
		 * Need not enable them in the final release.
		 */
		/*i2c_master_send (i2c, &data[0], 1);

		  i2c_master_recv(i2c, &read_data[0], 2);

		  printk("I2C Read Values are %d %d\r\n", read_data[0], read_data[1]);    */

		/* Perform a BUFFER SWAP Command. Check if we are currently not in Page 8,
		 * if so, swap to Page 8 first
		 */
		switch_minidsp_runtime_buffer(codec);

		if(page_offset == 0)
			page_offset = 4;
		else
			page_offset = 0;

		pr_debug ("Index %d Changing to Page %d\r\n", index,
				MUX_music_controls[index].control_page+page_offset);

		aic36_change_page (codec, MUX_music_controls[index].control_page+page_offset);
		ret_val = i2c_master_send (i2c, data, MUX_CTRL_REG_SIZE + 1);
		pr_debug ("Writing %d %d %d \r\n", data[0], data[1], data[2]);
		ret_val = 0;
	}

	aic36_change_page (codec, 0);
	return (ret_val);
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put_EQ_Select
 *
 * Purpose  : put routine for amixer kcontrols, write user values to registers
 *            values. Used for for mini dsp 'MUX control' amixer controls.
 *----------------------------------------------------------------------------
 */
	static int
__new_control_put_EQ_Select (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int user_value = ucontrol->value.integer.value[0];

	/* Enable the below line only when reading the I2C Transactions */
	/*u8 read_data[10]; */

	int ret_val = 0;

	put_EQ_Select(codec,user_value);

	return (ret_val);
}

static const char *miniDSP_EQ_Select_Str[] = {"Headphone EQ", "Speaker EQ","Direct"};

static const struct soc_enum miniDSP_EQ_Select_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(miniDSP_EQ_Select_Str), miniDSP_EQ_Select_Str),
};

/***************************************************************************
 * \brief minidsp_EQ_Select_mixer_controls
 *
 * Configures the AMIXER Controls for the miniDSP run-time configuration.
 * This routine configures the function pointers for the get put and the info
 * members of the snd_mode_controls and calls the snd_ctl_add routine
 * to register this with the ALSA Library.
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
minidsp_EQ_Select_mixer_controls (struct snd_soc_codec *codec)
{
	int err;

	snd_mode_controls.name = "miniDSP EQ Selection";
	snd_mode_controls.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	snd_mode_controls.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
	snd_mode_controls.info = snd_soc_info_enum_ext;//__new_control_info_minidsp_mode;
	snd_mode_controls.get = __new_control_get_EQ_Select;
	snd_mode_controls.put = __new_control_put_EQ_Select;
	snd_mode_controls.private_value = (unsigned long)&miniDSP_EQ_Select_enum[0];
	snd_mode_controls.count = 0;

	err = snd_ctl_add(codec->card->snd_card,
			snd_soc_cnew (&snd_mode_controls, codec, NULL, NULL));
	if (err < 0)
	{
		printk ("%s:Invalid control %s\n", __FILE__, snd_mode_controls.name);
	}

	return err;
}

#define NUM_BANDS 5
#define NUM_COEFFS_PER_BIQUAD 5

	static int
set_eq_value_bytable (struct snd_soc_codec *codec,u8 eq_band,u8 eq_value)
{
	struct i2c_client *i2c;
	u16 index = 0;
	u8 value[2];  
	int ret_val = -1;
	u8 eq_table_index;

	u8 page_offset = 0;
	minidsp_parser_data parser_data;
	i2c = codec->control_data;

	if(eq_band >=5 )
		return -1;

	if(eq_value >40 )
		eq_value = 40;

	value[0] = aic36_read_status(codec, DAC_ADAPT_STATUS);

	if(  (value[0]&2)  == 2 )
		page_offset = 0; // A
	else
		page_offset = 4; // B

	eq_table_index = eq_value +1;

	parser_data.burst_size = NUM_COEFFS_PER_BIQUAD*2 +1; 
	parser_data.page_num = eq_coeffs[eq_band][0][0]+page_offset;
	parser_data.burst_array[0] = eq_coeffs[eq_band][0][1]; //regoffset

	for(index= 0;index< NUM_COEFFS_PER_BIQUAD;index++)
	{
		parser_data.burst_array[index*2 + 1] = (eq_coeffs[eq_band][eq_table_index][index] >> 8) & 0xFF;
		parser_data.burst_array[index*2 + 2] = (eq_coeffs[eq_band][eq_table_index][index] ) & 0xFF;
	}

	aic36_minidsp_write_burst(codec, &parser_data);

	/* Logic for moving to the other coefficient buffer set */

	switch_minidsp_runtime_buffer(codec);
	if(page_offset == 0)
		page_offset = 4;
	else
		page_offset = 0;

	parser_data.page_num = eq_coeffs[eq_band][0][0]+page_offset;
	aic36_minidsp_write_burst(codec, &parser_data);

	aic36_change_page (codec, 0);

	ret_val = 0;
	return ret_val;

}

/***************************************************************************
 * \brief __new_control_info_eq_volume
 * -10dB(0)  -0db(20) -- +10dB(40)
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_info_eq_volume (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = MAX_EQ_LEVEL;

	return 0;
}

/***************************************************************************
 * \brief __new_control_get_minidsp_multibyte
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_get_eq_volume (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{

	ucontrol->value.integer.value[0] =  kcontrol->private_value;

	return 0;
}

#define ABS(v) (((v)>=0) ? (v):(-(v)))
/***************************************************************************
 * \brief __new_control_get_minidsp_multibyte
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_put_eq_volume (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int i,j;
	int gain_steps,direction,current_gain;

#ifndef MINIDSP_SINGLE_MODE
	if( voice_mode_enable == 1)
		return 0; // No used in voice mode 
#endif  	
	for( i = 0;i < MAX_EQ_BAND_NUMS; i++) {
		if ( strcmp (kcontrol->id.name, snd_eq_volume_str[i] ) == 0) {
			if(kcontrol->private_value == ucontrol->value.integer.value[0])
				return 0;

			gain_steps = (ucontrol->value.integer.value[0] - kcontrol->private_value) * (40/MAX_EQ_LEVEL);
			direction = 1;
			if(gain_steps < 0)
				direction = -1;
			gain_steps = ABS(gain_steps);		

			current_gain = kcontrol->private_value * (40/MAX_EQ_LEVEL);
			pr_debug ("%s: current_value = %d gain_steps = %d direction = %d gain = %lddB\n",
					snd_eq_volume_str[i],current_gain,gain_steps,direction,
					ucontrol->value.integer.value[0]-10);

			for (j=0; j< gain_steps; j++) {
				current_gain += direction ;
				set_eq_value_bytable(codec,i,current_gain);
			}  

			kcontrol->private_value = ucontrol->value.integer.value[0];
			snd_eq_volume[i] = kcontrol->private_value;

			aic36_change_page (codec, 0);
			return 0;
		}  
	}

	return -1;
}

/***************************************************************************
 * \brief minidsp_eq_volume_controls
 *
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
minidsp_eq_volume_controls (struct snd_soc_codec *codec)
{
	int err,i;

	for( i = 0;i < MAX_EQ_BAND_NUMS; i++)
	{
		snd_eq_volume_controls[i].name = (unsigned char*)snd_eq_volume_str[i];
		snd_eq_volume_controls[i].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		snd_eq_volume_controls[i].access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
		snd_eq_volume_controls[i].info = __new_control_info_eq_volume;
		snd_eq_volume_controls[i].get = __new_control_get_eq_volume;
		snd_eq_volume_controls[i].put = __new_control_put_eq_volume;

		snd_eq_volume[i] = MAX_EQ_LEVEL/2; // 0dB
		snd_eq_volume_controls[i].private_value = snd_eq_volume[i] ;
		snd_eq_volume_controls[i].count = 1; 

		err = snd_ctl_add(codec->card->snd_card,
				snd_soc_cnew (&snd_eq_volume_controls[i], codec, NULL, NULL));
		if (err < 0)
		{
			printk ("%s:Invalid control %s\n", __FILE__,snd_eq_volume_controls[i].name);
		}
	}

	return 0;
}


/************************** MUX CONTROL section *****************************/
static struct snd_kcontrol_new snd_mux_controls[MAX_MUX_CONTROLS];

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_info_minidsp_mux
 *
 * Purpose  : info routine for mini dsp mux control amixer kcontrols 
 *----------------------------------------------------------------------------
 */
	static int
__new_control_info_minidsp_mux (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	int index;
	int ret_val = -1;

	for (index = 0; index < ARRAY_SIZE (MUX_music_controls); index++)
	{
		if (strstr (kcontrol->id.name, MUX_music_control_names[index]))
			break;
	}

	if (index < ARRAY_SIZE (MUX_music_controls))
	{
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = MIN_MUX_CTRL;
		uinfo->value.integer.max = MAX_MUX_CTRL;
		ret_val = 0;
	}
	return ret_val;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_get_minidsp_mux
 *
 * Purpose  : get routine for  mux control amixer kcontrols, 
 * 			  read current register values to user. 
 * 			  Used for for mini dsp 'MUX control' amixer controls.
 *----------------------------------------------------------------------------
 */
	static int
__new_control_get_minidsp_mux (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put_minidsp_mux
 *
 * Purpose  : put routine for amixer kcontrols, write user values to registers
 *            values. Used for for mini dsp 'MUX control' amixer controls.
 *----------------------------------------------------------------------------
 */
	static int
__new_control_put_minidsp_mux (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 data[MUX_CTRL_REG_SIZE + 1];
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int index = 0;
	int user_value = ucontrol->value.integer.value[0];
	struct i2c_client *i2c;
	//  u8 value[2];
	u8 page;

	/* Enable the below line only when reading the I2C Transactions */
	/*u8 read_data[10]; */

	int ret_val = -1;
	i2c = codec->control_data;
	page = MUX_music_controls[index].control_page;

	pr_debug ("user value = 0x%x\n", user_value);

	for (index = 0; index < ARRAY_SIZE (MUX_music_controls); index++)
	{
		if (strstr (kcontrol->id.name, MUX_music_control_names[index]))
			break;
	}

	if (index < ARRAY_SIZE (MUX_music_controls))
	{
		pr_debug ("Index %d Changing to Page %d\r\n", index,
				MUX_music_controls[index].control_page);
		aic36_change_page (codec, MUX_music_controls[index].control_page);

		data[1] = (u8) ((user_value >> 8) & AIC36_8BITS_MASK);
		data[2] = (u8) ((user_value) & AIC36_8BITS_MASK);

		/* start register address */
		data[0] = MUX_music_controls[index].control_base;

		pr_debug ("Writing %d %d %d \r\n", data[0], data[1], data[2]);

		ret_val = i2c_master_send (i2c, data, MUX_CTRL_REG_SIZE + 1);

		if (ret_val != MUX_CTRL_REG_SIZE + 1)
		{
			pr_debug ("i2c_master_send transfer failed\n");
		}
		else
		{
			/* store the current level */
			kcontrol->private_value = user_value;
			ret_val = 0;
			/* Enable adaptive filtering for ADC/DAC */

		}

		/* Following block of code for testing the previous I2C WRITE Transaction. 
		 * Need not enable them in the final release.
		 */
		/*i2c_master_send (i2c, &data[0], 1);

		  i2c_master_recv(i2c, &read_data[0], 2);

		  printk("I2C Read Values are %d %d\r\n", read_data[0], read_data[1]);    */

		/* Perform a BUFFER SWAP Command. Check if we are currently not in Page 8,
		 * if so, swap to Page 8 first
		 */
		switch_minidsp_runtime_buffer(codec);

	}
	aic36_change_page (codec, MUX_music_controls[index].control_page);
	ret_val = i2c_master_send (i2c, data, MUX_CTRL_REG_SIZE + 1);
	ret_val = 0;

	aic36_change_page (codec, 0);
	return (ret_val);
}

/*
 *----------------------------------------------------------------------------
 * Function : minidsp_mux_ctrl_mixer_controls
 *
 * Purpose  : Add amixer kcontrols for mini dsp mux controls, 
 *----------------------------------------------------------------------------
 */
	static int
minidsp_mux_ctrl_mixer_controls (struct snd_soc_codec *codec)
{
	int i, err, no_mux_controls;

	no_mux_controls = ARRAY_SIZE (MUX_music_controls);

	pr_debug (" %d mixer controls for mini dsp MUX \n", no_mux_controls);

	if (no_mux_controls)
	{
		for (i = 0; i < no_mux_controls; i++)
		{

			snd_mux_controls[i].name = MUX_music_control_names[i];
			snd_mux_controls[i].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
			snd_mux_controls[i].access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
			snd_mux_controls[i].info = __new_control_info_minidsp_mux;
			snd_mux_controls[i].get = __new_control_get_minidsp_mux;
			snd_mux_controls[i].put = __new_control_put_minidsp_mux;
			/* 
			 *  TBD: read volume reg and update the index number 
			 */
			snd_mux_controls[i].private_value = 0;
			snd_mux_controls[i].count = 0;

			err = snd_ctl_add(codec->card->snd_card,
					snd_soc_cnew (&snd_mux_controls[i],
						codec, NULL, NULL));
			if (err < 0)
			{
				pr_debug ("%s:Invalid control %s\n", __FILE__,
						snd_mux_controls[i].name);
			}
		}
	}
	return 0;
}

/************************** VOICE/MUSIC MODE CONTROL section *****************************/
static struct snd_kcontrol_new snd_drc_controls;

/**
 * __new_control_info_drc_mux 
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a single mixer control.
 *
 * Returns 0 for success.
 */

int __new_control_info_drc_mux(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;

	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_get_drc_mux
 *
 * Purpose  : get routine for  mux control amixer kcontrols, 
 * 			  read current register values to user. 
 * 			  Used for for mini dsp 'MUX control' amixer controls.
 *----------------------------------------------------------------------------
 */
	static int
__new_control_get_drc_mux (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mDRCOnOff;
	//ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
}

int put_drc_mux (struct snd_soc_codec *codec,int bDrcOnOff)
{
	u8 data[MUX_CTRL_REG_SIZE + 1];
	static int index = 0;
	struct i2c_client *i2c;
	u8 value[2];
	u8 page;

	int user_value = bDrcOnOff;
	u8 page_offset = 0;

	/* Enable the below line only when reading the I2C Transactions */
	/*u8 read_data[10]; */

	int ret_val = -1;
	i2c = codec->control_data;
	page = MUX_music_controls[index].control_page;
#ifndef MINIDSP_SINGLE_MODE  
	if( voice_mode_enable!=0 )
		return 0;
#endif
	pr_debug ("miniDSP: Set DRC %s\n",user_value?"enable":"disable");

	if( index == -1)
	{
		for (index = 0; index < ARRAY_SIZE (MUX_music_controls); index++)
		{
			if (strstr ("Stereo_Mux_TwoToOne_3", MUX_music_control_names[index]))
				break;
		}
		if( index >= ARRAY_SIZE (MUX_music_controls) )
		{
			index = -1;
			return -1;
		}
	}

	//index = 0;			/// DRC index
	if(user_value == 1)
		user_value = 1;	// DRC Enable
	else	
		user_value =0xffff;	// DRC Disable

	if (index < ARRAY_SIZE (MUX_music_controls))
	{

		aic36_change_page (codec, 8);

		value[0] = 1;

		if (i2c_master_send (i2c, value, 1) != 1)
		{
			pr_debug ("Can not write register address\n");
		}

		if (i2c_master_recv (i2c, value, 1) != 1)
		{
			pr_debug ("Can not read codec registers\n");
		}

		if(  (value[0]&2)  == 2 )
			page_offset = 0; // A
		else
			page_offset = 4; // B

		pr_debug ("Index %d Changing to Page %d\r\n", index,
				MUX_music_controls[index].control_page+page_offset);
		aic36_change_page (codec, MUX_music_controls[index].control_page+page_offset);

		data[1] = (u8) ((user_value >> 8) & AIC36_8BITS_MASK);
		data[2] = (u8) ((user_value) & AIC36_8BITS_MASK);

		/* start register address */
		data[0] = MUX_music_controls[index].control_base;

		pr_debug ("Writing %d %d %d \r\n", data[0], data[1], data[2]);

		ret_val = i2c_master_send (i2c, data, MUX_CTRL_REG_SIZE + 1);

		if (ret_val != MUX_CTRL_REG_SIZE + 1)
		{
			pr_debug ("i2c_master_send transfer failed\n");
		}
		else
		{	
			/* store the current level */
			//kcontrol->private_value = ucontrol->value.integer.value[0];
			mDRCOnOff = bDrcOnOff;

			ret_val = 0;
			/* Enable adaptive filtering for ADC/DAC */

		}

		/* Following block of code for testing the previous I2C WRITE Transaction. 
		 * Need not enable them in the final release.
		 */
		/*i2c_master_send (i2c, &data[0], 1);

		  i2c_master_recv(i2c, &read_data[0], 2);

		  printk("I2C Read Values are %d %d\r\n", read_data[0], read_data[1]);    */

		/* Perform a BUFFER SWAP Command. Check if we are currently not in Page 8,
		 * if so, swap to Page 8 first
		 */
		switch_minidsp_runtime_buffer(codec);
		if(page_offset == 0)
			page_offset = 4;
		else
			page_offset = 0;


		pr_debug ("Index %d Changing to Page %d\r\n", index,
				MUX_music_controls[index].control_page+page_offset);
		aic36_change_page (codec, MUX_music_controls[index].control_page+page_offset);
		pr_debug ("Writing %d %d %d \r\n", data[0], data[1], data[2]);

		ret_val = i2c_master_send (i2c, data, MUX_CTRL_REG_SIZE + 1);
		ret_val = 0;

	}
	aic36_change_page (codec, 0);
	return (ret_val);
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put_drc_mux
 *
 * Purpose  : put routine for amixer kcontrols, write user values to registers
 *            values. Used for for mini dsp 'MUX control' amixer controls.
 *----------------------------------------------------------------------------
 */
	static int
__new_control_put_drc_mux (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int user_value = ucontrol->value.integer.value[0];

	/* Enable the below line only when reading the I2C Transactions */
	/*u8 read_data[10]; */

	int ret_val = -1;

	ret_val = put_drc_mux(codec,user_value);

	return (ret_val);
}

/***************************************************************************
 * \brief minidsp_drc_mux_controls
 *
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
minidsp_drc_mux_controls (struct snd_soc_codec *codec)
{
	int err;

	snd_drc_controls.name = "mDRC Enable";
	snd_drc_controls.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	snd_drc_controls.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
	snd_drc_controls.info = __new_control_info_drc_mux; // snd_soc_info_volsw
	snd_drc_controls.get = __new_control_get_drc_mux;
	snd_drc_controls.put = __new_control_put_drc_mux;
	snd_drc_controls.private_value = 0;
	snd_drc_controls.count = 0;

	err = snd_ctl_add (codec->card->snd_card, snd_soc_cnew (&snd_drc_controls, codec, NULL, NULL));
	if (err < 0)
	{
		printk ("%s:Invalid control %s\n", __FILE__, snd_drc_controls.name);
	}

	return err;
}


/************************** Adaptive filtering section **********************/


/***************************************************************************
 * \brief __new_control_info_minidsp_adaptive
 *
 * Informs the caller about the valid range of values that can be configured
 * for the Adaptive Mode AMIXER Control. The valid values are 0 [Adaptive Mode
 * OFF] and 1[Adaptive Mode ON].
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_info_minidsp_adaptive (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

/***************************************************************************
 * \brief __new_control_get_minidsp_adaptive
 *
 * Informs the caller about the current settings of the miniDSP Adaptive Mode
 * for the Adaptive Mode AMIXER Control. The valid values are 0 [Adaptive Mode
 * OFF] and 1[Adaptive Mode ON].
 *
 * The current configuration is updated in the ucontrol->value.integer.value
 * \return
 *       int - 0 on success and -1 on failure
 */
	static int
__new_control_get_minidsp_adaptive (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	struct i2c_client *i2c;
	char data[2];
	int ret = 0;

	/* The kcontrol->private_value is a pointer value which is configured
	 * by the driver to hold the following:
	 * Page NO for the Adaptive Mode configuration.
	 * Register No holding the Adaptive Mode Configuration and
	 * BitMask signifying the bit mask required to write it the Register.
	 */
	u8 page = (kcontrol->private_value) & AIC36_8BITS_MASK;
	u8 reg = (kcontrol->private_value >> 8) & AIC36_8BITS_MASK;
	u8 rmask = (kcontrol->private_value >> 16) & AIC36_8BITS_MASK;

	i2c = codec->control_data;

	pr_debug ("page %d, reg %d, mask 0x%x\n", page, reg, rmask);

	/* Read the register value */
	aic36_change_page (codec, page);

	/* write register addr to read */
	data[0] = reg;

	if (i2c_master_send (i2c, data, 1) != 1)
	{
		printk ("Can not write register address\n");
		ret = -1;
		goto revert;
	}
	/* read the codec/minidsp registers */
	if (i2c_master_recv (i2c, data, 1) != 1)
	{
		printk ("Can not read codec registers\n");
		ret = -1;
		goto revert;
	}

	pr_debug ("read: 0x%x\n", data[0]);

	/* return the read status to the user */
	if (data[0] & rmask)
	{
		ucontrol->value.integer.value[0] = 1;
	}
	else
	{
		ucontrol->value.integer.value[0] = 0;
	}

revert:
	/* put page back to zero */
	aic36_change_page (codec, 0);
	return ret;
}

/***************************************************************************
 * \brief __new_control_put_minidsp_adaptive
 *
 * Configures the Adaptive Mode settings as per the User specification. The
 * User can specify 0 or 1 as the Adaptive AMIXER Control Value.  The user
 * configuration is specified in the ucontrol->value.integer.value member of the
 * struct snd_ctl_elem_value * member passed as argument to this routine from
 * user space. 
 *
 * The current configuration is updated in the ucontrol->value.integer.value
 * \return
 *       int - 0 on success and -1 on failure
 */
static int __new_control_put_minidsp_adaptive (struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	int user_value = ucontrol->value.integer.value[0];
	struct i2c_client *i2c;
	char data[2];
	int ret = 0;

	u8 page = (kcontrol->private_value) & AIC36_8BITS_MASK;
	u8 reg = (kcontrol->private_value >> 8) & AIC36_8BITS_MASK;
	u8 wmask = (kcontrol->private_value >> 24) & AIC36_8BITS_MASK;

	i2c = codec->control_data;

	pr_debug ("page %d, reg %d, mask 0x%x, user_value %d\n",
			page, reg, wmask, user_value);

	/* Program the register value */
	aic36_change_page (codec, page);

	/* read register addr to read */
	data[0] = reg;

	if (i2c_master_send (i2c, data, 1) != 1)
	{
		pr_err("Can not write register address\n");
		ret = -1;
		goto revert;
	}
	/* read the codec/minidsp registers */
	if (i2c_master_recv (i2c, data, 1) != 1)
	{
		pr_err("Can not read codec registers\n");
		ret = -1;
		goto revert;
	}

	pr_debug("read: 0x%x\n", data[0]);

	/* set the bitmask and update the register */
	if (user_value == 0)
		data[1] = (data[0]) & (~wmask);
	else
		data[1] = (data[0]) | wmask;

	data[0] = reg;

	if (i2c_master_send (i2c, data, 2) != 2) {
		pr_debug ("Can not write register address\n");
		ret = -1;
	}

revert:
	/* put page back to zero */
	aic36_change_page (codec, 0);
	return ret;
}

/* 
 * AMIXER Control Interface definition. The definition is done using the
 * below macro SOC_ADAPTIVE_CTL_AIC36 which configures the .iface, .access
 * .info, .get and .put function pointers. 
 * The private_value member informs the register/page details to be used
 * when the AMIXER control is used at run-time.
 */
#define SOC_ADAPTIVE_CTL_AIC36(xname, page, reg, read_mask, write_mask) \
{   .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.info = __new_control_info_minidsp_adaptive, \
	.get = __new_control_get_minidsp_adaptive, 	\
	.put = __new_control_put_minidsp_adaptive, \
	.count = 0,	\
	.private_value = (page) | (reg << 8) | 	\
	( read_mask << 16) | (write_mask << 24) \
}

/* Adaptive filtering control and buffer swap  mixer kcontrols */
static struct snd_kcontrol_new snd_adaptive_controls[] = {
	SOC_ADAPTIVE_CTL_AIC36 (FILT_CTL_NAME_DAC, BUFFER_PAGE_DAC, 0x1, 0x4,
			0x4),
	SOC_ADAPTIVE_CTL_AIC36 (COEFF_CTL_NAME_DAC, BUFFER_PAGE_DAC, 0x1, 0x2,
			0x1),
};

/***************************************************************************
 * \brief minidsp_adaptive_filter_mixer_controls
 *
 * Configures the AMIXER Control Interfaces that can be exercised by the user
 * at run-time. Utilizes the  the snd_adaptive_controls[]  array to specify
 * two run-time controls. 
 *
 * \return
 *       int - 0 on success and -1 on failure
 */
static int minidsp_adaptive_filter_mixer_controls (struct snd_soc_codec *codec)
{
	int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE (snd_adaptive_controls); i++) {
		err = snd_ctl_add (codec->card->snd_card,
				snd_soc_cnew (&snd_adaptive_controls[i], codec,
					NULL, NULL));

		if (err < 0){
			pr_err ("%s:Invalid control %s\n", __FILE__,
					snd_adaptive_controls[i].name);
			return err;
		}
	}
	return 0;
}

#endif /* end of #ifdef ADD_MINI_DSP_CONTROLS */


/***************************************************************************
 * \brief aic36_add_minidsp_controls
 *
 * Configures the AMIXER Control Interfaces that can be exercised by the user
 * at run-time. Utilizes the  the snd_adaptive_controls[]  array to specify
 * two run-time controls. 
 *
 * \return
 *       void
 */
void aic36_add_minidsp_controls (struct snd_soc_codec *codec)
{
#ifdef ADD_MINI_DSP_CONTROLS
	if (minidsp_volume_music_mixer_controls (codec))
		pr_debug("mini DSP music volumeLite mixer control registration failed\n");

	if (minidsp_volume_voice_mixer_controls (codec))
		pr_debug("mini DSP voice volumeLite mixer control registration failed\n");

	if (minidsp_mux_ctrl_mixer_controls (codec))
		pr_debug("mini DSP mux selection mixer control registration failed\n");

	if (minidsp_adaptive_filter_mixer_controls (codec))
		pr_debug("Adaptive filter mixer control registration failed\n");

	if (minidsp_mode_mixer_controls (codec))
		pr_debug("mini DSP mode mixer control registration failed\n");

	/* if this is used in Android mode, the system hangs, so no using this*/
	/*
	   if (minidsp_multibyte_mixer_controls (codec))
	   {
	   printk ("mini DSP multibyte write mixer control registration failed\n");
	   }
	   */  
	if (minidsp_eq_volume_controls (codec))
		pr_debug("mini DSP eq volume mixer control registration failed\n");

	if (minidsp_drc_mux_controls (codec))
		pr_debug("mini drc mute control registration failed\n");

	if (minidsp_EQ_Select_mixer_controls (codec))
		pr_debug("mini drc mute control registration failed\n");
#endif
}

/************** Dynamic MINI DSP programmer, TI LOAD support  ***************/

static struct cdev *minidsp_cdev;
static int minidsp_major = 0;	/* Dynamic allocation of Mjr No. */
static int minidsp_opened = 0;	/* Dynamic allocation of Mjr No. */
static struct snd_soc_codec *minidsp_codec;

/***************************************************************************
 * \brief minidsp_open
 *
 * Character Interface Open Function for miniDSP Driver
 *
 * \return
 *       int 
 */
static int minidsp_open (struct inode *in, struct file *filp)
{
	if (minidsp_opened)	{
		pr_err("%s device is already opened\n", "minidsp");
		pr_err("%s: only one instance of driver is allowed\n", "minidsp");
		return -1;
	}
	minidsp_opened++;
	return 0;
}

/***************************************************************************
 * \brief minidsp_release
 *
 * Character Interface Close Function for miniDSP Driver
 *
 * \return
 *       int 
 */
static int minidsp_release (struct inode *in, struct file *filp)
{
	minidsp_opened--;
	return 0;
}

/***************************************************************************
 * \brief minidsp_read
 *
 * Character Interface read Function for miniDSP Driver
 *
 * \return
 *       int 
 */
static ssize_t minidsp_read (struct file *file, char __user * buf,
		size_t count, loff_t * offset)
{
	static char rd_data[256];
	char reg_addr;
	size_t size;
	struct i2c_client *i2c = minidsp_codec->control_data;

	if (count > 128)
	{
		pr_info("Max 256 bytes can be read\n");
		count = 128;
	}

	/* copy register address from user space  */
	size = copy_from_user (&reg_addr, buf, 1);
	if (size != 0)
	{
		pr_err("read: copy_from_user failure\n");
		return -1;
	}

	if (i2c_master_send (i2c, &reg_addr, 1) != 1)
	{
		pr_err("Can not write register address\n");
		return -1;
	}
	/* read the codec/minidsp registers */
	size = i2c_master_recv (i2c, rd_data, count);

	if (size != count)
	{
		pr_info("read %d registers from the codec\n", size);
	}

	if (copy_to_user (buf, rd_data, size) != 0)
	{
		pr_err("copy_to_user failed\n");
		return -1;
	}

	return size;
}

/***************************************************************************
 * \brief minidsp_write
 *
 * Character Interface Write Function for miniDSP Driver
 *
 * \return
 *       int 
 */
static ssize_t minidsp_write (struct file *file, const char __user * buf,
		size_t count, loff_t * offset)
{
	static char wr_data[258];
	size_t size;
	struct i2c_client *i2c = minidsp_codec->control_data;

	/* copy buffer from user space  */
	size = copy_from_user (wr_data, buf, count);
	if (size != 0) {
		printk ("copy_from_user failure %d\n", size);
		return -1;
	}

	if (wr_data[0] == 0) {
		aic36_change_page (minidsp_codec, wr_data[1]);
	}

	size = i2c_master_send (i2c, wr_data, count);
	return size;
}

/***************************************************************************
 * \brief minidsp_ioctl
 *
 * Character Interface IOCTL Function for miniDSP Driver
 *
 * \return
 *       int 
 */
static long minidsp_ioctl ( struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int err=0;

	if (_IOC_TYPE (cmd) != AIC36_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
		case AIC36_IOMAGICNUM_GET:
			err = copy_to_user ((void*)arg, &magic_num, sizeof (int));
			break;
		case AIC36_IOMAGICNUM_SET:
			err = copy_from_user (&magic_num, (void*)arg, sizeof (int));
			break;
	}
	return 0;
}

/*********** File operations structure for minidsp programming *************/
static struct file_operations minidsp_fops = {
	.owner = THIS_MODULE,
	.open = minidsp_open,
	.release = minidsp_release,
	.read = minidsp_read,
	.write = minidsp_write,
	.unlocked_ioctl = minidsp_ioctl,
};

/***************************************************************************
 * \brief minidsp_driver_init
 *
 * Registers a Character Interface Driver for the miniDSP Programming
 *
 * \return
 *       int 
 */
static int minidsp_driver_init (struct snd_soc_codec *codec)
{
	int result;
	dev_t dev = MKDEV (minidsp_major, 0);

	minidsp_codec = codec;

	pr_debug ("allocating dynamic major number\n");

	result = alloc_chrdev_region (&dev, 0, 1, "minidsp-aic36");

	if (result < 0)	{
		pr_err("cannot allocate major number %d\n", minidsp_major);
		return result;
	}

	minidsp_major = MAJOR (dev);
	pr_debug ("allocated Major Number: %d\n", minidsp_major);

	minidsp_cdev = cdev_alloc ();
	cdev_init (minidsp_cdev, &minidsp_fops);
	minidsp_cdev->owner = THIS_MODULE;
	minidsp_cdev->ops = &minidsp_fops;

	if (cdev_add(minidsp_cdev, dev, 1) < 0) {
		pr_debug ("minidsp_driver: cdev_add failed \n");
		unregister_chrdev_region (dev, 1);
		minidsp_cdev = NULL;
		return 1;
	}
	pr_info("Registered minidsp driver, Major number: %d \n", minidsp_major);
	return 0;
}

#endif
