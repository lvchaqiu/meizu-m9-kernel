/*
 * m9w_tlv320aic36.c - SoC audio for Meizu M9 3G smartphone.
 * 
 * Copyright (C) 2009 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: 	lvcha qiu	<lvcha@meizu.com>
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
 * Inital code : Mar 16 , 2010 : lvcha@meizu.com
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <plat/regs-iis.h> 
#include <asm/io.h>
#include <plat/gpio-cfg.h> 
#include <plat/map-base.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>

#include "s5pc1xx-i2s.h"
#include "../codecs/tlv320aic36.h"

#define I2S_NUM 0

static int lowpower = 0;

int set_speaker_OnOff(bool bEn)
{
	s3c_gpio_setpin(SPK_EN_PIN, (bEn?1:0));
	return 0;
}

static int m9w_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int err;

	/* Set codec DAI configuration */
	err = snd_soc_dai_set_fmt(codec_dai,
					 SND_SOC_DAIFMT_I2S |
					 SND_SOC_DAIFMT_NB_NF |
					 SND_SOC_DAIFMT_CBM_CFM);

	if (err < 0)
		return err;

	/* Set cpu DAI configuration */
	err = snd_soc_dai_set_fmt(cpu_dai,
				       SND_SOC_DAIFMT_I2S |
				       SND_SOC_DAIFMT_NB_NF |
				       SND_SOC_DAIFMT_CBM_CFM);
	if (err < 0)
		return err;

	// ap generate CDCLK clock of 24MHZ or 12MHZ to codec
	err = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_CDCLK, 0, SND_SOC_CLOCK_OUT);
	//Set Operation clock for IIS logic. Use Audio bus clock
	err = snd_soc_dai_set_sysclk(cpu_dai, S5P_IISMOD_OPCLK_PCLK, 0, 3);
	//Set II2 as slave and output CDCLK
	err = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_IISMOD_SLAVE, 0, SND_SOC_CLOCK_IN);

	/* Set the codec system clock for DAC and ADC */
	err = snd_soc_dai_set_sysclk(codec_dai, 0, AIC36_FREQ_24000000, SND_SOC_CLOCK_IN);

	return err;
}

static struct snd_soc_ops m9w_ops = {
	.hw_params = m9w_hw_params,
};

static struct snd_soc_dai_link m9w_dai[] = {
	{/* Hifi Playback - for similatious use with voice below */
		.name = "tlv320aic36 playback",
		.stream_name = "Ti AIC36 HiFi Playback",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "tlv320aic36-dai",
		.platform_name = "samsung-audio",
		.codec_name = "tlv320aic36-codec",
		.ops = &m9w_ops,
	},
};

static struct snd_soc_card m9w_snd_soc_card = {
	.name = "Meizu M9W",
	.dai_link = m9w_dai,
	.num_links = ARRAY_SIZE(m9w_dai),
};

struct platform_device *m9w_snd_device;
EXPORT_SYMBOL_GPL(m9w_snd_device);
static int __init m9w_audio_init(void)
{
	int ret;

	pr_info("%s: Disable speaker \n", __func__);

	m9w_snd_device = platform_device_alloc("soc-audio", 0);
	if (!m9w_snd_device)
		return -ENOMEM;

	platform_set_drvdata(m9w_snd_device, &m9w_snd_soc_card);

	ret = platform_device_add(m9w_snd_device);
	if (ret)
		platform_device_put(m9w_snd_device);

	return ret;
}

static void __exit m9w_audio_exit(void)
{
	platform_device_unregister(m9w_snd_device);
}

module_init(m9w_audio_init);
module_exit(m9w_audio_exit);

module_param (lowpower, int, 0444);

MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_DESCRIPTION("ALSA SoC MEIZU M9W");
MODULE_LICENSE("GPLV2");
