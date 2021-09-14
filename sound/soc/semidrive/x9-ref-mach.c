/*
 * x9-ref-mach
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

static int x9_ref_late_probe(struct snd_soc_card *card) { return 0; }


static int x9_ref_rtd_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	dev_dbg(rtd->dev,
		"[i2s sc] int fixed cpu -----%s -- %p %p "
		"%p----------------------- \n",
		rtd->cpu_dai->name, rtd->cpu_dai, rtd->cpu_dai->driver,
		rtd->cpu_dai->driver->ops->set_tdm_slot);
	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0xFF, 0xF, 8, 32);
	if (ret < 0) {
		dev_err(rtd->dev,
			"can't set cpu snd_soc_dai_set_tdm_slot err %d\n", ret);
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
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.init = x9_ref_rtd_init,
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM,
    },

};
/*-Init Machine Driver
 * ---------------------------------------------------------------------*/
#define SND_X9_REF_MACH_DRIVER "x9-ref-mach"
#define SND_REF_CARD_NAME SND_X9_REF_MACH_DRIVER
/*Sound Card Driver
 * ------------------------------------------------------------------------*/
static struct snd_soc_card x9_ref_card = {
    .name = SND_REF_CARD_NAME,
    .dai_link = snd_x9_ref_soc_dai_links,
    .num_links = ARRAY_SIZE(snd_x9_ref_soc_dai_links),
    .late_probe = x9_ref_late_probe,
};

/*ALSA machine driver probe functions.*/
static int x9_ref_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &x9_ref_card;
	struct device *dev = &pdev->dev;

	int ret;
	struct snd_x9_chip *chip;
	dev_info(&pdev->dev, ": dev name =  %s %s\n", pdev->name, __func__);
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

static int x9_ref_remove(struct platform_device *pdev)
{
	snd_card_free(platform_get_drvdata(pdev));
	dev_info(&pdev->dev, "%s x9_ref_removed\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
/*pm suspend operation */
static int snd_x9_ref_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	dev_info(&pdev->dev, "%s snd_x9_suspend\n", __func__);
	return 0;
}
/*pm resume operation */
static int snd_x9_ref_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s snd_x9_resume\n", __func__);
	return 0;
}
#endif
static const struct of_device_id x9_ref_dt_match[] = {
    {
	.compatible = "semidrive,x9-ref-mach",
    },
    {},
};

MODULE_DEVICE_TABLE(of, x9_ref_dt_match);

static struct platform_driver x9_ref_mach_driver = {
    .driver =
	{
	    .name = SND_X9_REF_MACH_DRIVER,
	    .of_match_table = x9_ref_dt_match,
#ifdef CONFIG_PM
	    .pm = &snd_soc_pm_ops,
#endif
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},

    .probe = x9_ref_probe,
    .remove = x9_ref_remove,
#ifdef CONFIG_PM
    .suspend = snd_x9_ref_suspend,
    .resume = snd_x9_ref_resume,
#endif
};

module_platform_driver(x9_ref_mach_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 ALSA SoC ref machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 ref alsa mach");
