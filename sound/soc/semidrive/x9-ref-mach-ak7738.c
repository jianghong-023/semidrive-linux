/*
 * x9--mach
 * Copyright (C) 2020 semidrive
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
 *
 */
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include "sdrv-common.h"
#include <linux/i2c.h>
#include <sound/initval.h>
#include <sound/jack.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>

/* -------------------------------- */
/* definition of the chip-specific record  dma related ops*/
struct snd_x9_chip {
	struct snd_soc_card *card;
	struct x9_dummy_model *model;
	struct snd_pcm *pcm;
	struct snd_pcm *pcm_spdif;
	struct snd_pcm_hardware pcm_hw;
	/* Bitmat for valid reg_base and irq numbers
	unsigned int avail_substreams;*/
	struct device *dev;

	int volume;
	int old_volume; /* stores the volume value whist muted */
	int dest;
	int mute;
	int jack_gpio;

	unsigned int opened;
	unsigned int spdif_status;
	struct mutex audio_mutex;
};

/*Controls
 * ------------------------------------------------------------------------------*/
enum snd_x9_ctrl {
	PCM_PLAYBACK_VOLUME,
	PCM_PLAYBACK_MUTE,
	PCM_PLAYBACK_DEVICE,
};

static int snd_x9_ref_ak7738_controls_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_card *card = snd_kcontrol_chip(kcontrol); */
	/*Set value here*/

	return 0;
}

static int snd_x9_ref_ak7738_controls_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	/*  Add control here.
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol); */

	return 0;
}

static int snd_x9_ctl_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	if (kcontrol->private_value == PCM_PLAYBACK_VOLUME) {
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = -10239;
		uinfo->value.integer.max = 400; /* 2303 */
	}
	return 0;
}

static int snd_x9_ctl_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_x9_chip *chip = snd_kcontrol_chip(kcontrol);

	mutex_lock(&chip->audio_mutex);

	if (kcontrol->private_value == PCM_PLAYBACK_VOLUME)
		ucontrol->value.integer.value[0] = chip->volume;
	else if (kcontrol->private_value == PCM_PLAYBACK_MUTE)
		ucontrol->value.integer.value[0] = chip->mute;
	else if (kcontrol->private_value == PCM_PLAYBACK_DEVICE)
		ucontrol->value.integer.value[0] = chip->dest;

	mutex_unlock(&chip->audio_mutex);
	return 0;
}

static int snd_x9_ctl_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	return changed;
}

SNDRV_CTL_TLVD_DECLARE_DB_SCALE(snd_codec_db_scale, -10239, 1, 1);

static const struct snd_kcontrol_new snd_x9_ak7738_controls[] = {
    SOC_SINGLE_BOOL_EXT("X9 Switch", 0, snd_x9_ref_ak7738_controls_get,
			snd_x9_ref_ak7738_controls_put),
    {.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
     .name = "PCM Playback Volume",
     .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
     .private_value = PCM_PLAYBACK_VOLUME,
     .info = snd_x9_ctl_info,
     .get = snd_x9_ctl_get,
     .put = snd_x9_ctl_put,
     .tlv = {.p = snd_codec_db_scale}},
};

/* head set jack GPIO Controller  */
static struct snd_soc_jack hs_jack;
static struct snd_soc_jack_gpio hs_jack_gpios[] = {
    {
	.gpio = -1,
	.name = "headset-gpio",
	.report = SND_JACK_HEADSET,
	.invert = 0,
	.debounce_time = 200,
    },
};

/*
 * Logic for a ak7738 as connected on a x9_ref
 */
static int x9_ref_ak7738_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	DEBUG_FUNC_PRT;
	/*  Add Jack detection API stuff */

	return err;
}

/*
 * Logic for a tas5404 as connected on a x9_ref amp
 */
static int x9_ref_tas5404_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	DEBUG_FUNC_PRT;
	/* Jack detection API stuff */

	return err;
}
static int x9_ref_tas6424_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	DEBUG_FUNC_PRT;
	/* Jack detection API stuff */

	return err;
}

static int x9_ref_hs_ak4556_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	DEBUG_FUNC_PRT;
	return err;
}

/*Dapm widtets*/
static const struct snd_soc_dapm_widget sd_x9_ref_dapm_widgets[] = {
    /* Outputs */
    SND_SOC_DAPM_HP("Headphone Jack", NULL),

    SND_SOC_DAPM_SPK("FL SPK", NULL),
    SND_SOC_DAPM_SPK("FR SPK", NULL),
    SND_SOC_DAPM_SPK("C  SPK", NULL),
    SND_SOC_DAPM_SPK("LFE SPK", NULL),

    SND_SOC_DAPM_SPK("RL SPK", NULL),
    SND_SOC_DAPM_SPK("RR SPK", NULL),
    SND_SOC_DAPM_SPK("SL SPK", NULL),
    SND_SOC_DAPM_SPK("SR SPK", NULL),

    /* Inputs front left and right*/
    SND_SOC_DAPM_MIC("FL Mic", NULL),
    SND_SOC_DAPM_MIC("FR Mic", NULL),

	/* Inputs rear left and right*/
    SND_SOC_DAPM_MIC("RL Mic", NULL),
    SND_SOC_DAPM_MIC("RR Mic", NULL),


};

static const struct snd_soc_dapm_route sd_x9_ref_dapm_map[] = {
    /* Main SPK Out connected to amplifier SPK<- CPU*/
	{"AIF2 Playback", NULL, "PCM0 OUT"},
	{"AIF5 Playback", NULL, "PCM0 OUT"},
	{"DIG_AMP Playback", NULL, "PCM0 OUT"},
	{"DAC IN", NULL, "SDOUT5"},
	{"FL SPK", NULL, "DIG_AMP_OUT"},
	{"FR SPK", NULL, "DIG_AMP_OUT"},

	{"ANA_AMP Playback", NULL, "AOUT1"},
	{"ANA_AMP Playback", NULL, "AOUT2"},
	{"SL SPK", NULL, "ANA_AMP_OUT"},
	{"SR SPK", NULL, "ANA_AMP_OUT"},

    /* Capture audio route*/
//	{"AIF2 Capture",NULL,"AIF4 Playback"}, CPU <-Mic

	{"AIN1L",NULL,"FL Mic"},
	{"IN1P_N",NULL,"FL Mic"},
	{"AIN1R",NULL,"FR Mic"},
	{"IN2P_N",NULL,"FR Mic"},
	{"AIN2",NULL,"RL Mic"},
	{"IN1P_N",NULL,"RL Mic"},
	{"AIN2",NULL,"RR Mic"},
	{"IN2P_N",NULL,"RR Mic"},

	{"SAI1_IN", NULL, "SDOUT4"},
	{"SAI1_IN",NULL,"AIF4 Capture"},
	{"SDIN4",NULL,"XF Capture"},
	{"PCM0 IN",NULL,"AIF2 Capture"},
	{"PCM0 IN",NULL,"AIF4 Capture"},

};


static const struct snd_soc_ops x9_ref_ak7738_ops = {

};

static const struct snd_soc_ops x9_ref_tas5404_ops = {
    // .hw_params = x9_tas5404_soc_hw_params,
};

static const struct snd_soc_ops x9_ref_tas6424_ops = {

};

static const struct snd_soc_ops x9_ref_hs_ops = {
    // .hw_params = x9_ref_hs_soc_hw_params,
};

static int be_hw_params_fixup_sd4(struct snd_soc_pcm_runtime *rtd,
			      struct snd_pcm_hw_params *params)
{

	struct snd_mask *mask;

	int ret;

	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	/*  48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 8;
	dev_dbg(rtd->dev,"[AK7738 SD4] -------------------------------rate: %d chan: %d  \n",rate->max,channels->max);
	/* set 32 bit */
	mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	snd_mask_none(mask);
	snd_mask_set(mask, SNDRV_PCM_FORMAT_S32_LE);
	//ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0x3, 0x3, 2, 24);

	ret = snd_soc_dai_set_fmt(rtd->codec_dai,
				  SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set format to DSP_A, err %d\n", ret);
		return ret;
	}
	return 0;
}

static int be_hw_params_fixup_sd2(struct snd_soc_pcm_runtime *rtd,
			      struct snd_pcm_hw_params *params)
{

	struct snd_mask *mask;

	int ret;

	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	/*  48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 8;
	dev_dbg(rtd->dev,"[AK7738 SD2] -------------------------------rate: %d chan: %d  \n",rate->max,channels->max);
	/* set 32 bit */
/* 	mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	snd_mask_none(mask);
	snd_mask_set(mask, SNDRV_PCM_FORMAT_S24_LE); */

	ret = snd_soc_dai_set_fmt(rtd->codec_dai,
				  SND_SOC_DAIFMT_DSP_A|SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set format to DSP_A, err %d\n", ret);
		return ret;
	}

	return 0;
}


static int be_hw_params_fixup_sd5(struct snd_soc_pcm_runtime *rtd,
			      struct snd_pcm_hw_params *params)
{

	struct snd_mask *mask;

	int ret;

	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	/*  48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 8;
	dev_dbg(rtd->dev,"[AK7738 SD5] -------------------------------rate: %d chan: %d  \n",rate->max,channels->max);
	/* set 32 bit */
	mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	snd_mask_none(mask);
	snd_mask_set(mask, SNDRV_PCM_FORMAT_S24_LE);

	ret = snd_soc_dai_set_fmt(rtd->codec_dai,
				  SND_SOC_DAIFMT_DSP_A|SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set format to DSP_A, err %d\n", ret);
		return ret;
	}
	return 0;
}

static int be_hw_params_fixup_tas6424(struct snd_soc_pcm_runtime *rtd,
			      struct snd_pcm_hw_params *params)
{

	struct snd_mask *mask;

	int ret;

	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	/*  48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 4;
	dev_dbg(rtd->dev,"[tas6424 SD4] -------------------------------rate: %d chan: %d  \n",rate->max,channels->max);

	/* set 32 bit */
	mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	snd_mask_none(mask);
	snd_mask_set(mask, SNDRV_PCM_FORMAT_S24_LE);
	ret = snd_soc_dai_set_fmt(rtd->codec_dai,
				  SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set format to DSP_A, err %d\n", ret);
		return ret;
	}
 	ret = snd_soc_dai_set_tdm_slot(rtd->codec_dai, 0xF, 0x10, 4, 24);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set tdm slot, err %d\n", ret);
		return ret;
	}
	return 0;
}

static int x9_ref_ak7738_rtd_init(struct snd_soc_pcm_runtime *rtd) {
	int ret;
	dev_dbg(rtd->dev,"[i2s sc] int fixed cpu -----%s -- %p %p %p----------------------- \n",rtd->cpu_dai->name,rtd->cpu_dai,rtd->cpu_dai->driver,rtd->cpu_dai->driver->ops->set_tdm_slot);
	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0xFF, 0x3, 8, 32);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set cpu snd_soc_dai_set_tdm_slot err %d\n", ret);
		return ret;
	}
	return 0;
}

static struct snd_soc_dai_link snd_x9_ref_soc_dai_links[] = {
    /* Front End DAI links */
    {
	.name = "x9_hifi",
	.stream_name = "x9 hifi",
	.cpu_name = "30650000.i2s",
	.cpu_dai_name = "snd-afe-sc-i2s-dai0",
	.platform_name = "30650000.i2s",
	.dynamic = 1,
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.init = x9_ref_ak7738_rtd_init,
	//SND_SOC_DAIFMT_CBS_CFS SND_SOC_DAIFMT_I2S SND_SOC_DAIFMT_RIGHT_J SND_SOC_DAIFMT_LEFT_J SND_SOC_DAIFMT_DSP_A SND_SOC_DAIFMT_DSP_B
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
	.dpcm_playback = 1,
	.dpcm_capture = 1,
    },
    /* Back End DAI links */
    {
	.name = "dsp sd2",
	.id = 0,
	.cpu_dai_name = "snd-soc-dummy-dai",
	.platform_name = "snd-soc-dummy",
	.no_pcm = 1,
	.codec_name = "ak7738.0-001c",
	.codec_dai_name = "ak7738-aif2",
	.be_hw_params_fixup = be_hw_params_fixup_sd2,
	.init = x9_ref_ak7738_init,
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM,
	.ops = &x9_ref_ak7738_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.dpcm_playback = 1,
	.dpcm_capture = 1,
    },
    {
	.name = "dsp sd4",
	.id = 1,
	.cpu_dai_name = "snd-soc-dummy-dai",
	.platform_name = "snd-soc-dummy",
	.no_pcm = 1,
	.codec_name = "ak7738.0-001c",
	.codec_dai_name = "ak7738-aif4",
	.be_hw_params_fixup = be_hw_params_fixup_sd4,
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM,//|SND_SOC_DAIFMT_DSP_B
	.ops = &x9_ref_ak7738_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.dpcm_capture = 1,
    },
	{
	.name = "dsp xf",
	.id = 2,
	.cpu_dai_name = "snd-soc-dummy-dai",
	.platform_name = "snd-soc-dummy",
	.no_pcm = 1,
	.codec_name = "xf6020.0-0047",
	.codec_dai_name = "xf6020-codec",
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS,//|SND_SOC_DAIFMT_DSP_B
	.ops = &x9_ref_ak7738_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.dpcm_capture = 1,
    },
    {
	.name = "digital amp hifi",
	.id = 3,
	.cpu_dai_name = "snd-soc-dummy-dai",
	.platform_name = "snd-soc-dummy",
	.no_pcm = 1,
	.codec_name = "tas6424.0-006a",
	.codec_dai_name = "tas6424-amplifier",
	.init = x9_ref_tas6424_init,
	.be_hw_params_fixup = be_hw_params_fixup_tas6424,
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS, //SND_SOC_DAIFMT_I2S
	.ops = &x9_ref_tas6424_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.dpcm_playback = 1,
    },
	{
	.name = "dsp sd5",
	.id = 4,
	.cpu_dai_name = "snd-soc-dummy-dai",
	.platform_name = "snd-soc-dummy",
	.no_pcm = 1,
	.codec_name = "ak7738.0-001c",
	.codec_dai_name = "ak7738-aif5",
	.be_hw_params_fixup = be_hw_params_fixup_sd5,
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM,
	.ops = &x9_ref_ak7738_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.dpcm_playback = 1,
    },
	{
	.name = "analog amp hifi",
	.id = 5,
	.cpu_dai_name = "snd-soc-dummy-dai",
	.platform_name = "snd-soc-dummy",
	.no_pcm = 1,
	.codec_name = "tas5404.0-006c",
	.codec_dai_name = "tas5404-amplifier",
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.dpcm_playback = 1,
    },
};

/*Sound Card Driver
 * ------------------------------------------------------------------------*/
static struct snd_soc_card x9_ref_ak7738_card = {
    .name = "sd-x9-snd-card-ak7738",

    .dai_link = snd_x9_ref_soc_dai_links,
    .num_links = ARRAY_SIZE(snd_x9_ref_soc_dai_links),
    .dapm_widgets = sd_x9_ref_dapm_widgets,
    .num_dapm_widgets = ARRAY_SIZE(sd_x9_ref_dapm_widgets),
    .dapm_routes = sd_x9_ref_dapm_map,
    .num_dapm_routes = ARRAY_SIZE(sd_x9_ref_dapm_map),

};

/*GPIO probe use this function to set gpio. */
static int x9_gpio_probe(struct snd_soc_card *card)
{
	int ret = 0;
	return ret;
}

/*-Init Machine Driver
 * ---------------------------------------------------------------------*/
#define SND_X9_MACH_AK7738_DRIVER "x9-ref-ak7738"

/*ALSA machine driver probe functions.*/
static int x9_ref_ak7738_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &x9_ref_ak7738_card;
	struct device *dev = &pdev->dev;

	int ret;
	struct snd_x9_chip *chip;

	dev_info(&pdev->dev, ": dev name =%s %s\n", pdev->name, __func__);

	DEBUG_FUNC_PRT;
	card->dev = dev;

	/* Allocate chip data */
	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		return -ENOMEM;
	}
	chip->card = card;

	ret = devm_snd_soc_register_card(dev, card);

	if (ret) {
		dev_err(dev, "%s snd_soc_register_card fail %d\n", __func__,
			ret);
	}

	return ret;
}

static int x9_ref_ak7738_remove(struct platform_device *pdev)
{
	snd_card_free(platform_get_drvdata(pdev));
	dev_info(&pdev->dev, "%s x9_ref_ak7738_removed\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
/*pm suspend operation */
static int snd_x9_ref_ak7738_suspend(struct platform_device *pdev,
				     pm_message_t state)
{
	dev_info(&pdev->dev, "%s snd_x9_suspend\n", __func__);
	return 0;
}
/*pm resume operation */
static int snd_x9_ref_ak7738_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s snd_x9_resume\n", __func__);
	return 0;
}
#endif

static const struct of_device_id x9_ref_ak7738_dt_match[] = {
    {
	.compatible = "semidrive,x9-ref-ak7738",
    },
};
MODULE_DEVICE_TABLE(of, x9_ref_hs_dt_match);

static struct platform_driver x9_ref_ak7738_mach_driver = {
    .driver =
	{
	    .name = SND_X9_MACH_AK7738_DRIVER,
	    .of_match_table = x9_ref_ak7738_dt_match,
#ifdef CONFIG_PM
	    .pm = &snd_soc_pm_ops,
#endif
	},

    .probe = x9_ref_ak7738_probe,
    .remove = x9_ref_ak7738_remove,
#ifdef CONFIG_PM

    .suspend = snd_x9_ref_ak7738_suspend,
    .resume = snd_x9_ref_ak7738_resume,
#endif
};

module_platform_driver(x9_ref_ak7738_mach_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 ALSA SoC ref ak7738 machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 ref ak7738 alsa mach");
