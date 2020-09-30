/*
 * sdrv-i2s-sc.c
 * Copyright (C) 2019 semidrive
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
#include "sdrv-i2s-sc.h"
#include "sdrv-common.h"
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#define DRV_NAME "snd-afe-i2s-sc"
#define SDRV_I2S_FIFO_SIZE 2048

/* Define a fack buffer for dummy data copy */
static bool fake_buffer = 0;
module_param(fake_buffer, bool, 0444);
MODULE_PARM_DESC(fake_buffer, "Fake buffer allocations.");

static bool full_duplex = true;
module_param(full_duplex, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(full_duplex, "Set to full duplex mode (default: true)");

/*i2s interrupt mode function*/
static void sdrv_pcm_refill_fifo(struct sdrv_afe_i2s_sc *afe)
{

	struct snd_pcm_substream *tx_substream;
	bool tx_active;
	rcu_read_lock();
	tx_substream = rcu_dereference(afe->tx_substream);
	const u16(*p)[2] = (void *)tx_substream->runtime->dma_area;
	tx_active = tx_substream && snd_pcm_running(tx_substream);
	// if(tx_active)
	{
		u32 ret, val, val1;
		if (p) {
			int numb;
			// DEBUG_FUNC_PRT
			ret = regmap_read(afe->regmap,
					  REG_CDN_I2SSC_REGS_FIFO_LEVEL, &val);
			numb = I2S_SC_TX_DEPTH - val;
			while (numb > 0) {

				iowrite32(p[afe->tx_ptr][0],
					  (afe->regs + I2S_SC_FIFO_OFFSET));
				iowrite32(p[afe->tx_ptr][1],
					  (afe->regs + I2S_SC_FIFO_OFFSET));

				if ((++afe->tx_ptr) >=
				    tx_substream->runtime->buffer_size) {
					afe->tx_ptr = 0;
				}
				numb = numb - 2;
			}

			ret = regmap_read(afe->regmap,
					  REG_CDN_I2SSC_REGS_FIFO_LEVEL, &val1);
		}
	}
	rcu_read_unlock();
}

/* reset i2s  */
static void afe_i2s_reset(struct sdrv_afe_i2s_sc *afe)
{
	/*Disable i2s*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (0 << I2S_CTRL_I2S_EN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (0 << I2S_CTRL_SFR_RST_FIELD_OFFSET));

}

/*debug function for dumpping register setting */
static void afe_i2s_sc_dump_reg(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, val;
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_info(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL 0x%x:(0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_I2S_CTRL, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY, &val);
	dev_info(afe->dev,
		"DUMP %d REG_CDN_I2SSC_REGS_FIFO_AEMPTY 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_FIFO_AEMPTY, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL, &val);
	dev_info(afe->dev,
		"DUMP %d REG_CDN_I2SSC_REGS_FIFO_AFULL 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_FIFO_AFULL, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, &val);
	dev_info(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_SRES 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_I2S_SRES, val);

}
/* misc operation */

static void afe_i2s_sc_config(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, val;
	/*Disable i2s*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (0 << I2S_CTRL_I2S_EN_FIELD_OFFSET));

	/*SFR reset*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_SFR_RST,
			   (0 << I2S_CTRL_SFR_RST_FIELD_OFFSET));

	/*FIFO reset*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_FIFO_RST,
			   (0 << I2S_CTRL_FIFO_RST_FIELD_OFFSET));

	if (true == afe->is_full_duplex) {

		/*Reset full duplex FIFO reset*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
				   BIT_CTRL_FDX_FIFO_RST,
				   (0 << I2S_CTRL_FDX_FIFO_RST_FIELD_OFFSET));
		/*Enable full-duplex*/
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_FULL_DUPLEX,
		    (1 << I2S_CTRL_FDX_FULL_DUPLEX_FIELD_OFFSET));
		/*Enable tx sdo*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
				   BIT_CTRL_FDX_I2S_FTX_EN,
				   (1 << I2S_CTRL_FDX_I2S_FTX_EN_FIELD_OFFSET));
		/*Enable rx sdi*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
				   BIT_CTRL_FDX_I2S_FRX_EN,
				   (1 << I2S_CTRL_FDX_I2S_FRX_EN_FIELD_OFFSET));
		/*Unmask full-duplex mode interrupt request.*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
				   BIT_CTRL_FDX_RI2S_MASK,
				   (1 << I2S_CTRL_FDX_RI2S_MASK_FIELD_OFFSET));
		/*Mask full-duplex receive FIFO becomes empty. */
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_RFIFO_EMPTY_MASK,
		    (0 << I2S_CTRL_FDX_RFIFO_EMPTY_MASK_FIELD_OFFSET));
		/*Mask full-duplex receive FIFO becomes almost empty.*/
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_RFIFO_AEMPTY_MASK,
		    (0 << I2S_CTRL_FDX_RFIFO_AEMPTY_MASK_FIELD_OFFSET));
		/*Mask full-duplex receive FIFO becomes full. */
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_RFIFO_FULL_MASK,
		    (0 << I2S_CTRL_FDX_RFIFO_FULL_MASK_FIELD_OFFSET));
		/*Mask full-duplex receive FIFO becomes almost full. */
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_RFIFO_AFULL_MASK,
		    (0 << I2S_CTRL_FDX_RFIFO_AFULL_MASK_FIELD_OFFSET));
	}

	/*config direction of transmission:Tx*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_DIR_CFG,
			   (1 << I2S_CTRL_DIR_CFG_FIELD_OFFSET));

	/*config total data width*/
	regmap_update_bits(
	    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, BIT_CTRL_CHN_WIDTH,
	    (AFE_I2S_CHN_WIDTH << I2S_CTRL_CHN_WIDTH_FIELD_OFFSET));

	/*config almost empty fifo level:tx fifo level */
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY, 0x30);

	/*config almost full fifo level:defualt value*/
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL, 0x7f);

	/*config resolution,valid number of data bit*/
	if (true == afe->is_full_duplex) {
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES_FDR, 0xf);
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, 0xf);
	} else {
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, 0xf);
	}

	regcache_sync(afe->regmap);

	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL 0x%x:(0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_I2S_CTRL, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY, &val);
	dev_dbg(afe->dev,
		"DUMP %d REG_CDN_I2SSC_REGS_FIFO_AEMPTY 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_FIFO_AEMPTY, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL, &val);
	dev_dbg(afe->dev,
		"DUMP %d REG_CDN_I2SSC_REGS_FIFO_AFULL 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_FIFO_AFULL, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_SRES 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_I2S_SRES, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_TDM_FD_DIR 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_TDM_FD_DIR, val);
}

static void afe_i2s_sc_start_capture(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, val;
	/*Firstly,Enable i2s*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (1 << I2S_CTRL_I2S_EN_FIELD_OFFSET));
	/* Then enable interrupt */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_RI2S_MASK,
			   (1 << I2S_CTRL_FDX_RI2S_MASK_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_INTREQ_MASK,
			   (1 << I2S_CTRL_INTREQ_MASK_FIELD_OFFSET));

	/*TODO: Need set more parameters*/
	/*Set fifo almost empty mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_FIFO_AEMPTY_MASK,
			   (0 << I2S_CTRL_FIFO_AEMPTY_MASK_FIELD_OFFSET));

	/*Clear fifo almost full mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_FIFO_AFULL_MASK,
			   (0 << I2S_CTRL_FIFO_AFULL_MASK_FIELD_OFFSET));

	/*Clear fifo afull mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_RFIFO_AFULL_MASK,
			   (0 << I2S_CTRL_FDX_RFIFO_AFULL_MASK_FIELD_OFFSET));

	/*At the last Enable i2s interrupt main switch*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_INTREQ_MASK,
			   (1 << I2S_CTRL_INTREQ_MASK_FIELD_OFFSET));

	regcache_sync(afe->regmap);

	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL(0x%x)\n",
		__LINE__, val);
}

static void afe_i2s_sc_stop_capture(struct sdrv_afe_i2s_sc *afe)
{
	/* Disable interrupt */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_RI2S_MASK,
			   (0 << I2S_CTRL_I2S_EN_FIELD_OFFSET));
	/*TDM slot setting*/
	afe->rx_slot_mask = 0;
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
				BIT_TDM_FD_DIR_CH_RX_EN,
				(afe->rx_slot_mask  << TDM_FD_DIR_CH0_RXEN_FIELD_OFFSET));
}

static void afe_i2s_sc_start_playback(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, val;
	/*Firstly,Enable i2s*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (1 << I2S_CTRL_I2S_EN_FIELD_OFFSET));

	/* Then enable interrupt */
	/*Set interrupt request i2s fdx */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_RI2S_MASK,
			   (1 << I2S_CTRL_FDX_RI2S_MASK_FIELD_OFFSET));

	/*Set all interrupt request generation */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_MASK,
			   (1 << I2S_CTRL_I2S_MASK_FIELD_OFFSET));

	/*Set fifo almost empty mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_FIFO_AEMPTY_MASK,
			   (0 << I2S_CTRL_FIFO_AEMPTY_MASK_FIELD_OFFSET));

	/*Clear fifo almost full mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_FIFO_AFULL_MASK,
			   (0 << I2S_CTRL_FIFO_AFULL_MASK_FIELD_OFFSET));

	/*Clear fifo afull mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_RFIFO_AFULL_MASK,
			   (0 << I2S_CTRL_FDX_RFIFO_AFULL_MASK_FIELD_OFFSET));

	/*At the last Enable i2s interrupt main switch*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_INTREQ_MASK,
			   (1 << I2S_CTRL_INTREQ_MASK_FIELD_OFFSET));

	regcache_sync(afe->regmap);

	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL(0x%x)\n",
		__LINE__, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX, &val);
	dev_dbg(afe->dev, "DUMP REG_CDN_I2SSC_REGS_I2S_CTRL_FDX(0x%x)\n", val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, &val);
	dev_dbg(afe->dev, "DUMP REG_CDN_I2SSC_REGS_I2S_SRES(0x%x)\n", val);
}

static void afe_i2s_sc_stop_playback(struct sdrv_afe_i2s_sc *afe)
{

	/* Disable interrupt */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_INTREQ_MASK,
			   (0 << I2S_CTRL_INTREQ_MASK_FIELD_OFFSET));
	/*TDM slot setting*/
 	afe->tx_slot_mask = 0;
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
				BIT_TDM_FD_DIR_CH_TX_EN,
				(afe->tx_slot_mask << TDM_FD_DIR_CH0_TXEN_FIELD_OFFSET));
}

static void afe_i2s_sc_stop(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, val;
	/*Firstly,Disable i2s*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (0 << I2S_CTRL_I2S_EN_FIELD_OFFSET));
/* 	TODO: 	disable TDM mode ?
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (1 << TDM_CTRL_TDM_EN_FIELD_OFFSET));
	TODO: 	clear status ?
				    */

	regcache_sync(afe->regmap);

	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL(0x%x)\n",
		__LINE__, val);
}

/*
 * dummy buffer handling
 */

static void *dummy_page[2];

static void free_fake_buffer(void)
{
	if (fake_buffer) {
		int i;
		for (i = 0; i < 2; i++)
			if (dummy_page[i]) {
				free_page((unsigned long)dummy_page[i]);
				dummy_page[i] = NULL;
			}
	}
}

static int alloc_fake_buffer(void)
{
	int i;

	if (!fake_buffer)
		return 0;
	for (i = 0; i < 2; i++) {
		dummy_page[i] = (void *)get_zeroed_page(GFP_KERNEL);
		if (!dummy_page[i]) {
			free_fake_buffer();
			return -ENOMEM;
		}
	}
	return 0;
}

/* x9 pcm hardware definition.  */

static const struct snd_pcm_hardware sdrv_pcm_hardware = {

    .info = (SNDRV_PCM_INFO_INTERLEAVED |
	     SNDRV_PCM_INFO_BLOCK_TRANSFER),
    .formats = SND_FORMATS,
    .rates = SND_RATE,
    .rate_min = SND_RATE_MIN,
    .rate_max = SND_RATE_MAX,
    .channels_min = SND_CHANNELS_MIN,
    .channels_max = SND_CHANNELS_MAX,
    .period_bytes_min = MIN_PERIOD_SIZE,
    .period_bytes_max = MAX_PERIOD_SIZE,
    .periods_min = MIN_PERIODS,
    .periods_max = MAX_PERIODS,
    .buffer_bytes_max = MAX_ABUF_SIZE,
    .fifo_size = 0,
};

static int sdrv_snd_pcm_open(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	DEBUG_FUNC_PRT;
	snd_soc_set_runtime_hwparams(substream, &sdrv_pcm_hardware);
	// p = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	runtime->private_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	return 0;
}

static int sdrv_snd_pcm_close(struct snd_pcm_substream *substream)
{
	DEBUG_FUNC_PRT;
	synchronize_rcu();
	return 0;
}
static int sdrv_snd_pcm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{

	struct snd_pcm_runtime *runtime;
	runtime = substream->runtime;
	/* struct sdrv_afe_i2s_sc *afe = runtime->private_data;
	 unsigned channels = params_channels(hw_params); */

	DEBUG_FUNC_PRT;
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int sdrv_snd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	DEBUG_FUNC_PRT;
	return snd_pcm_lib_free_pages(substream);
}

static int sdrv_snd_pcm_prepare(struct snd_pcm_substream *substream)
{

	int ret = 0;
	DEBUG_FUNC_PRT;
	return ret;
}
/*This function for pcm */
static int sdrv_snd_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sdrv_afe_i2s_sc *afe = runtime->private_data;
	DEBUG_FUNC_PRT;

	/* 	printk("pcm:buffer_size = %ld,"
		"dma_area = %p, dma_bytes = %zu reg = %p PeriodsSZ(%d)(%d)\n ",
		runtime->buffer_size, runtime->dma_area, runtime->dma_bytes,
	   afe->regs,runtime->periods,runtime->period_size);
 */
	printk(KERN_INFO "%s:%i ------cmd(%d)--------------\n", __func__,
	       __LINE__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		DEBUG_FUNC_PRT;
		ACCESS_ONCE(afe->tx_ptr) = 0;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_i2s_sc_start_playback(afe);
			atomic_set(&afe->playing, 1);
		} else {
			afe_i2s_sc_start_capture(afe);
			atomic_set(&afe->capturing, 1);
		}
		regcache_sync(afe->regmap);
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			atomic_set(&afe->playing, 0);
			afe_i2s_sc_stop_playback(afe);
		} else {
			atomic_set(&afe->capturing, 0);
			afe_i2s_sc_stop_capture(afe);
		}
		if (!atomic_read(&afe->playing) &&
		    !atomic_read(&afe->capturing)) {
			afe_i2s_sc_stop(afe);
		}
		regcache_sync(afe->regmap);
		return 0;
	}
	return -EINVAL;
}

static snd_pcm_uframes_t
sdrv_snd_pcm_pointer(struct snd_pcm_substream *substream)
{
	// DEBUG_FUNC_PRT;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sdrv_afe_i2s_sc *afe = runtime->private_data;
	snd_pcm_uframes_t pos = ACCESS_ONCE(afe->tx_ptr);

	return pos < runtime->buffer_size ? pos : 0;
}

static struct snd_pcm_ops sdrv_snd_pcm_ops = {
    .open = sdrv_snd_pcm_open,
    .close = sdrv_snd_pcm_close,
    .ioctl = snd_pcm_lib_ioctl,
    .hw_params = sdrv_snd_pcm_hw_params,
    .hw_free = sdrv_snd_pcm_hw_free,
    .prepare = sdrv_snd_pcm_prepare,
    .trigger = sdrv_snd_pcm_trigger,
    .pointer = sdrv_snd_pcm_pointer,
};

/* 	TODO: here setup dai */
/* Set up irq handler
 * ------------------------------------------------------------ */
static irqreturn_t sdrv_i2s_sc_irq_handler(int irq, void *dev_id)
{
	struct sdrv_afe_i2s_sc *afe = dev_id;
	unsigned int i2s_sc_status;
	struct snd_pcm_runtime *runtime;
	int ret;

	runtime = afe->tx_substream->runtime;
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
			  &i2s_sc_status);
	/* clear irq here*/
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT, 0);

	/* printk("pcm:buffer_size = %ld,"
		"dma_area = %p, dma_bytes = %zu reg = %p PeriodsSZ(%d)(%d)
	   I2S_STAT(0x%x)\n ", runtime->buffer_size, runtime->dma_area,
	   runtime->dma_bytes,
	   afe->regs,runtime->periods,runtime->period_size,i2s_sc_status);
    */
	if (ret) {
		dev_err(afe->dev, "%s irq status err\n", __func__);
		return IRQ_HANDLED;
	}

	if (i2s_sc_status & BIT_STAT_FIFO_AEMPTY) {
		/* calculate the processed frames since the
		 * last update 1024 * 4
		 * comment this function for dma mode.
		 */
		/* 	last_frame = afe->periods * runtime->period_size;

		if (last_frame < afe->tx_ptr){
	    size = afe->tx_ptr - last_frame;
		}
	else {
			// less than 0
		size = afe->tx_ptr + runtime->buffer_size -last_frame ;
			//dev_err(afe->dev, "afe->tx_ptr %d %d %d!
	\n",size,afe->periods,last_frame);
		}

	    if (size >= runtime->period_size) {


				if (afe->tx_substream &&
	snd_pcm_running(afe->tx_substream))
				{
					afe->periods++;
					//dev_err(afe->dev, "period_elapsed %d
	%d %d %d ! \n",size,afe->periods,last_frame,afe->tx_ptr);
					//print_hex_dump_bytes("writing to lpe:
	", DUMP_PREFIX_OFFSET,runtime->dma_area[afe->tx_ptr], 20);
					if(afe->periods >= runtime->periods ){
						afe->periods = 0;
					}
					snd_pcm_period_elapsed(afe->tx_substream);
					//sdrv_pcm_refill_fifo(afe,afe->tx_substream->runtime);
				}
				else
				{
					dev_err(afe->dev, "i2s sc tx substream
	stoped \n");
				}
			} */
	}

	if (i2s_sc_status & BIT_STAT_FIFO_EMPTY) {
	}

	/*  TODO: Add rx period elapsed */

	if (i2s_sc_status & BIT_STAT_RDATA_OVERR) {
		/* remove temporarily

		dev_err(afe->dev, "i2s sc overrun! \n");*/
	}

	if (i2s_sc_status & BIT_STAT_TDATA_UNDERR) {
		/* remove temporarily
		dev_err(afe->dev, "i2s sc underrun!\n");*/
	}

	return IRQ_HANDLED;
}

/* -Set up dai regmap
 * here----------------------------------------------------------- */
static bool sdrv_i2s_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_CDN_I2SSC_REGS_I2S_CTRL:
	case REG_CDN_I2SSC_REGS_I2S_CTRL_FDX:
	case REG_CDN_I2SSC_REGS_I2S_SRES:
	case REG_CDN_I2SSC_REGS_I2S_SRES_FDR:
	case REG_CDN_I2SSC_REGS_I2S_SRATE:
	case REG_CDN_I2SSC_REGS_I2S_STAT:
	case REG_CDN_I2SSC_REGS_FIFO_LEVEL:
	case REG_CDN_I2SSC_REGS_FIFO_AEMPTY:
	case REG_CDN_I2SSC_REGS_FIFO_AFULL:
	case REG_CDN_I2SSC_REGS_FIFO_LEVEL_FDR:
	case REG_CDN_I2SSC_REGS_FIFO_AEMPTY_FDR:
	case REG_CDN_I2SSC_REGS_FIFO_AFULL_FDR:
	case REG_CDN_I2SSC_REGS_TDM_CTRL:
	case REG_CDN_I2SSC_REGS_TDM_FD_DIR:
		return true;
	default:
		return false;
	}
}

static bool sdrv_i2s_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_CDN_I2SSC_REGS_I2S_STAT:
		return true;
	default:
		return false;
	}
}

/*  Setup i2s regmap */
static const struct regmap_config sdrv_i2s_sc_regmap_config = {
    .reg_bits = 32,
    .reg_stride = 4,
    .val_bits = 32,
    .max_register = REG_CDN_I2SSC_REGS_TDM_FD_DIR + 0x39,
    .writeable_reg = sdrv_i2s_wr_rd_reg,
    .readable_reg = sdrv_i2s_wr_rd_reg,
    .volatile_reg = sdrv_i2s_wr_rd_reg,
    .cache_type = REGCACHE_FLAT,
};

int snd_afe_dai_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{

	/* struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai; */
	/* TODO: enable main clk */
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);

	DEBUG_FUNC_PRT;
	/* Debug code set to half duplex */
	if (full_duplex == false) {
		afe->is_full_duplex = false;
		DEBUG_ITEM_PRT(afe->is_full_duplex);
	}
	snd_soc_dai_set_dma_data(dai, substream, afe);

	return 0;
}

static int ch_mask[]={0x0,0x1,0x3,0x7,0xf,0x1f,0x3f,0x7f,0xff};

static void snd_afe_dai_ch_cfg(struct sdrv_afe_i2s_sc *afe, int stream, unsigned ch_numb)
{
	int ret, val;
	int channel_numb ;

	afe->slots = max(ch_numb,afe->slots);
	afe->slot_width = AFE_I2S_CHN_WIDTH;
	dev_info(afe->dev, "%s:%i : afe->slots (0x%x) ch_numb(0x%x) afe->slot_width(0x%x) \n", __func__,
	       __LINE__,afe->slots , ch_numb, afe->slot_width);
	/* Enable tdm mode */


	if(stream == SNDRV_PCM_STREAM_PLAYBACK){
		afe->tx_slot_mask = ch_mask[ch_numb];
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
				   BIT_TDM_FD_DIR_CH_TX_EN,
				   (ch_mask[ch_numb] << TDM_FD_DIR_CH0_TXEN_FIELD_OFFSET));

	}else if(stream == SNDRV_PCM_STREAM_CAPTURE){

		afe->rx_slot_mask = ch_mask[ch_numb];
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
				   BIT_TDM_FD_DIR_CH_RX_EN,
				   (ch_mask[ch_numb]  << TDM_FD_DIR_CH0_RXEN_FIELD_OFFSET));

	}else{
		dev_err(afe->dev, "%s(%d) Don't support this stream: %d\n",
				__func__,__LINE__,stream);
	}

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CHN_NUMB,
				   ((afe->slots-1) << TDM_CTRL_CHN_NO_FIELD_OFFSET));

	/* Set enabled number  */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CH_EN,
				   (ch_mask[afe->slots] << TDM_CTRL_CH0_EN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (1 << TDM_CTRL_TDM_EN_FIELD_OFFSET));

	//ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL, &val);
	//dev_dbg(afe->dev, "DUMP REG_CDN_I2SSC_REGS_TDM_CTRL(0x%x)\n", val);
}

int snd_afe_dai_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *hwparam,
			  struct snd_soc_dai *dai)
{
	u32 ret, val;

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	/* 	struct device *dev = dai->dev;
		struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
		struct snd_soc_dai *codec_dai = rtd->codec_dai; */
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);

	unsigned srate = params_rate(hwparam);
	unsigned channels = params_channels(hwparam);
	unsigned period_size = params_period_size(hwparam);
	unsigned sample_size = snd_pcm_format_width(params_format(hwparam));
	/* 	unsigned freq, ratio, level;
		int err; */


	if(afe->tdm_initialized == false){
		if (params_channels(hwparam) == 2) {
			/*config data channel stereo*/
			/* DEBUG_FUNC_PRT */
			/* Disable tdm mode */
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (0 << TDM_CTRL_TDM_EN_FIELD_OFFSET));

			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_AUDIO_MODE,
					(0 << I2S_CTRL_AUDIO_MODE_FIELD_OFFSET));
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_MONO_MODE,
					(0 << I2S_CTRL_MONO_MODE_FIELD_OFFSET));
		} else if (params_channels(hwparam) == 1) {
			/*config data channel mono
			DEBUG_FUNC_PRT*/

			/* Disable tdm mode */
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (0 << TDM_CTRL_TDM_EN_FIELD_OFFSET));

			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_AUDIO_MODE,
					(1 << I2S_CTRL_AUDIO_MODE_FIELD_OFFSET));
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_MONO_MODE,
					(0 << I2S_CTRL_MONO_MODE_FIELD_OFFSET));
		} else {
			/* Multi channels */
			DEBUG_FUNC_PRT
			snd_afe_dai_ch_cfg(afe, substream->stream, params_channels(hwparam));
		}
	}else{
		/* Already set to tdm by snd_afe_set_dai_tdm_slot */

		snd_afe_dai_ch_cfg(afe, substream->stream, params_channels(hwparam));
		DEBUG_FUNC_PRT

	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		DEBUG_FUNC_PRT
		if (false == afe->is_full_duplex) {
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DIR_CFG,
				   (1 << I2S_CTRL_DIR_CFG_FIELD_OFFSET));
		}
		/*set tx substream*/
		afe->tx_substream = substream;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		DEBUG_FUNC_PRT
		if (false == afe->is_full_duplex) {
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DIR_CFG,
				   (0 << I2S_CTRL_DIR_CFG_FIELD_OFFSET));
		}
		/*set rx substream */
		afe->rx_substream = substream;
	} else {
		DEBUG_FUNC_PRT
		return -EINVAL;
	}

	switch (params_format(hwparam)) {
		case SNDRV_PCM_FORMAT_S8:
			dev_err(afe->dev, "%s(%d) Don't support this function for S8 is low dynamic range.\n",
				__func__,__LINE__);
			return -EINVAL;
		case SNDRV_PCM_FORMAT_S16_LE:
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S32_LE:
			break;
		default:
			return -EINVAL;
	}

	/* Here calculate and fill sample rate.  */
	if (true == afe->is_full_duplex) {

		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES_FDR,
			     sample_size - 1);

		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES,
			     sample_size - 1);

	} else {
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES,
			     sample_size - 1);
	}

	/*Channel width fix to 32 */
	if(afe->tdm_initialized == false){
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRATE,
			     I2S_SC_SAMPLE_RATE_CALC(clk_get_rate(afe->clk_i2s), srate,
						     channels,
						     ChnWidthTable[AFE_I2S_CHN_WIDTH]) -
				 1);
	}
	else{
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRATE,
			     I2S_SC_SAMPLE_RATE_CALC(clk_get_rate(afe->clk_i2s), srate,
						     afe->slots,
						     ChnWidthTable[AFE_I2S_CHN_WIDTH]) -
				 1);
	}
	/*  regcache_sync(afe->regmap); */

/* 	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL(0x%x)\n",
		__LINE__, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_SRES(0x%x)\n",
		__LINE__, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES_FDR, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_SRES_FDR(0x%x)\n",
		__LINE__, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRATE, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_SRATE(0x%x)\n",
		__LINE__, val); */
	dev_info(
	    afe->dev,
	    "%s clk%u srate: %u, channels: %u, sample_size: %u, period_size: %u, stream %u\n",
	    __func__,clk_get_rate(afe->clk_i2s), srate, channels, sample_size, period_size,substream->stream);

	return snd_soc_runtime_set_dai_fmt(rtd, rtd->dai_link->dai_fmt);
}

int snd_afe_dai_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{

	/* struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	afe_i2s_sc_config(afe); */
	DEBUG_FUNC_PRT
	return 0;
}

/* snd_afe_dai_trigger */
int snd_afe_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			struct snd_soc_dai *dai)
{

	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	printk(KERN_INFO "%s:%i ------cmd(%d)stream(%d)--------------\n", __func__,
	       __LINE__, cmd,substream->stream );
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		DEBUG_FUNC_PRT;
		ACCESS_ONCE(afe->tx_ptr) = 0;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_i2s_sc_start_playback(afe);
			atomic_set(&afe->playing, 1);
		} else {
			afe_i2s_sc_start_capture(afe);
			atomic_set(&afe->capturing, 1);
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			atomic_set(&afe->playing, 0);
			afe_i2s_sc_stop_playback(afe);
		} else {
			atomic_set(&afe->capturing, 0);
			afe_i2s_sc_stop_capture(afe);
		}
		if (!atomic_read(&afe->playing) &&
		    !atomic_read(&afe->capturing)) {
			printk(KERN_INFO "%s:%i -----i2s stop----------\n", __func__,
	       __LINE__, cmd,substream->stream );
			afe_i2s_sc_stop(afe);
		}
		break;

	default:
		return -EINVAL;
	}
	return 0;
}
int snd_afe_dai_hw_free(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	DEBUG_ITEM_PRT(substream->stream);

	if((afe->tx_slot_mask == 0 )&&
		(afe->rx_slot_mask == 0)){
		DEBUG_FUNC_PRT
	}
	return 0;
}
void snd_afe_dai_shutdown(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	DEBUG_ITEM_PRT(substream->stream);
	/* disable main clk  end of playback or capture*/
}

static int snd_afe_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int val = 0, ret;
	DEBUG_FUNC_PRT
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	/* codec clk & FRM slave i2s master*/
	case SND_SOC_DAIFMT_CBS_CFS:
		DEBUG_FUNC_PRT
		/*config master mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_MS_CFG,
				   (1 << I2S_CTRL_MS_CFG_FIELD_OFFSET));
		break;
	/* codec clk & FRM master i2s slave*/
	case SND_SOC_DAIFMT_CBM_CFM:
		DEBUG_FUNC_PRT
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_MS_CFG,
				   (0 << I2S_CTRL_MS_CFG_FIELD_OFFSET));
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {

	case SND_SOC_DAIFMT_I2S:
		DEBUG_FUNC_PRT
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (1 << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));
		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (0 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));
		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (1 << I2S_CTRL_WS_MODE_FIELD_OFFSET));
		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (1 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));
		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (0 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));
		/*config data order:fisrt send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		DEBUG_FUNC_PRT
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (1 << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));
		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (1 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));
		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (1 << I2S_CTRL_WS_MODE_FIELD_OFFSET));
		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (0 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));
		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (0 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));
		/*config data order:first send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
		DEBUG_FUNC_PRT
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (1 << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));
		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (1 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));
		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (1 << I2S_CTRL_WS_MODE_FIELD_OFFSET));
		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (0 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));
		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (1 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));
		/*config data order:first send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));
		break;
	case SND_SOC_DAIFMT_DSP_A:
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (1 << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));

		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (0 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));

		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (0 << I2S_CTRL_WS_MODE_FIELD_OFFSET));

		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (1 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));

		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (0 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));

		/*config data order:first send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));

		break;
	case SND_SOC_DAIFMT_DSP_B:
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (1 << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));

		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (0 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));

		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (0 << I2S_CTRL_WS_MODE_FIELD_OFFSET));

		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (0 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));

		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (0 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));

		/*config data order:first send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));

		break;
	default:
		return -EINVAL;
	}
	// regcache_sync(afe->regmap);

	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL(0x%x)\n",
		__LINE__, val);
	return 0;
}

static int snd_afe_set_dai_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	int channel_numb ;
	dev_info(afe->dev, "%s:%i : tx_mask(0x%x) rx_mask(0x%x) slots(%d) slot_width(%d) \n", __func__,
	       __LINE__,tx_mask,rx_mask,slots,slot_width);

	/* Enable tdm mode */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (1 << TDM_CTRL_TDM_EN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
				   BIT_TDM_FD_DIR_CH_TX_EN,
				   (tx_mask << TDM_FD_DIR_CH0_TXEN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
				   BIT_TDM_FD_DIR_CH_RX_EN,
				   (rx_mask << TDM_FD_DIR_CH0_RXEN_FIELD_OFFSET));

	if (slots <= 4){
		DEBUG_FUNC_PRT
		slots = 4;
		/* Enable tdm channel */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CHN_NUMB,
				   (3 << TDM_CTRL_CHN_NO_FIELD_OFFSET));
		/* Set enabled number  */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CH_EN,
				   (0xf << TDM_CTRL_CH0_EN_FIELD_OFFSET));

	}else if ( slots <= 8 ){
		DEBUG_FUNC_PRT
		slots = 8;
		/* Enable tdm channel */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CHN_NUMB,
				   (7 << TDM_CTRL_CHN_NO_FIELD_OFFSET));
		/* Set enabled number  */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CH_EN,
				   (0xff << TDM_CTRL_CH0_EN_FIELD_OFFSET));
	}else if ( slots <= 16 ){
		DEBUG_FUNC_PRT
		slots = 16;
		/* Enable tdm channel */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CHN_NUMB,
				   (15 << TDM_CTRL_CHN_NO_FIELD_OFFSET));
		/* Set enabled number  */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CH_EN,
				   (0xffff << TDM_CTRL_CH0_EN_FIELD_OFFSET));
	}
	else{
		return -EINVAL;
	}

/*  	if (true == afe->is_full_duplex) {
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES_FDR, slot_width -1);
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, slot_width -1);
	} else {
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, slot_width -1);
	}  */
	afe->slot_width = slot_width;
	afe->slots = slots;
	afe->tx_slot_mask = tx_mask;
	afe->rx_slot_mask = rx_mask;
	/*Audio will use tdm */
	afe->tdm_initialized = true;
	int ret, val;
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL, &val);
	dev_info(afe->dev, "DUMP REG_CDN_I2SSC_REGS_TDM_CTRL(0x%x)\n", val);
		ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR, &val);
	dev_info(afe->dev, "DUMP REG_CDN_I2SSC_REGS_TDM_FD_DIR(0x%x)\n", val);
	return 0;
}

static struct snd_soc_dai_ops snd_afe_dai_ops = {
    .startup = snd_afe_dai_startup,
    .shutdown = snd_afe_dai_shutdown,
    .hw_params = snd_afe_dai_hw_params,
    .set_tdm_slot	= snd_afe_set_dai_tdm_slot,
    .hw_free = snd_afe_dai_hw_free,
    .prepare = snd_afe_dai_prepare,
    .trigger = snd_afe_dai_trigger,
    .set_fmt = snd_afe_dai_set_fmt,
};

int snd_soc_dai_probe(struct snd_soc_dai *dai)
{
	struct sdrv_afe_i2s_sc *d = snd_soc_dai_get_drvdata(dai);
	struct device *dev = d->dev;

	dev_info(dev, "DAI probe.----------vaddr(0x%llx)------------------\n",d->regs);

	/* 	struct zx_i2s_info *zx_i2s = dev_get_drvdata(dai->dev);
	snd_soc_dai_init_dma_data(dai, &d->playback_dma_data,
		&d->capture_dma_data); */

	dai->capture_dma_data = &d->capture_dma_data;
	dai->playback_dma_data = &d->playback_dma_data;

	return 0;
}

int snd_soc_dai_remove(struct snd_soc_dai *dai)
{
	struct sdrv_afe_i2s_sc *d = snd_soc_dai_get_drvdata(dai);
	struct device *dev = d->dev;

	dev_info(dev, "DAI remove.\n");

	return 0;
}

/* i2s afe dais */
static struct snd_soc_dai_driver snd_afe_dais[] = {
    {
	.name = "snd-afe-sc-i2s-dai0",
	.probe = snd_soc_dai_probe,
	.remove = snd_soc_dai_remove,
	.playback =
	    {
		.stream_name = "I2S Playback",
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = 1,
		.channels_max = 8,
	    },
	.capture =
	    {
		.stream_name = "I2S Capture",
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = 1,
		.channels_max = 8,
	    },
	.ops = &snd_afe_dai_ops,
    },
    {
	.name = "snd-afe-sc-i2s-dai1",
	.probe = snd_soc_dai_probe,
	.remove = snd_soc_dai_remove,
	.playback =
	    {
		.stream_name = "I2S1 Playback",
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = 1,
		.channels_max = 8,
	    },
	.capture =
	    {
		.stream_name = "I2S1 Capture",
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = 1,
		.channels_max = 8,
	    },
	.ops = &snd_afe_dai_ops,
    },
};

static const struct snd_soc_component_driver snd_sample_soc_component = {
    .name = DRV_NAME "-i2s-component",
};

int snd_afe_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct sdrv_afe_i2s_sc *d = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = d->dev;
	struct snd_pcm *pcm = rtd->pcm;

	dev_info(dev, "New.\n");
	if (!fake_buffer) {
		snd_pcm_lib_preallocate_pages_for_all(
		    pcm, SNDRV_DMA_TYPE_CONTINUOUS,
		    snd_dma_continuous_data(GFP_KERNEL), 0,
		    SDRV_I2S_FIFO_SIZE * 8);
	}
	return 0;
}

void snd_afe_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_chip(pcm);
	struct sdrv_afe_i2s_sc *d = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = d->dev;

	dev_info(dev, "Free.\n");
}

/*-----------PCM
 * OPS-----------------------------------------------------------------*/
static const struct snd_soc_platform_driver snd_afe_pcm_soc_platform = {
    .pcm_new = snd_afe_pcm_new,
    .pcm_free = snd_afe_pcm_free,
    .ops = &sdrv_snd_pcm_ops,
};

static int sdrv_pcm_prepare_slave_config(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params,
					 struct dma_slave_config *slave_config)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(rtd->cpu_dai);

	int ret;


	ret = snd_hwparams_to_dma_slave_config(substream, params, slave_config);
	if (ret)
		return ret;

	slave_config->dst_maxburst = 4;
	slave_config->src_maxburst = 4;

	/*Change width by audio format */

	switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S8:
			dev_err(afe->dev, "%s(%d) Don't support this function for S8 is low dynamic range.\n",
				__func__,__LINE__);
			return -EINVAL;
		case SNDRV_PCM_FORMAT_S16_LE:
			DEBUG_FUNC_PRT;
			slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S32_LE:
			DEBUG_FUNC_PRT;
			slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			break;
		default:
			return -EINVAL;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		slave_config->dst_addr = afe->playback_dma_data.addr;
	else
		slave_config->src_addr = afe->capture_dma_data.addr;
	DEBUG_FUNC_PRT;
	return 0;
}

static const struct snd_dmaengine_pcm_config sdrv_dmaengine_pcm_config = {
    .pcm_hardware = &sdrv_pcm_hardware,
    .prepare_slave_config = sdrv_pcm_prepare_slave_config,
    .prealloc_buffer_size = MAX_ABUF_SIZE,
};

static int snd_afe_soc_platform_probe(struct snd_soc_component *pdev)
{
	struct device *dev = pdev->dev;
	/* struct sdrv_afe_i2s_sc *afe; */
	int ret = 0;
	dev_info(dev, "Added.----------------\n");

	DEBUG_FUNC_PRT
	/* TODO: Add afe controls here */

	return ret;
}

static void snd_afe_soc_platform_remove(struct snd_soc_component *pdev)
{
	struct device *dev = pdev->dev;
	dev_info(dev, "Removed.\n");
	DEBUG_FUNC_PRT
	return;
}

static const struct snd_soc_dapm_widget afe_widgets[] = {
    /* Outputs */
	SND_SOC_DAPM_AIF_OUT("PCM0 OUT", NULL,  0,
			SND_SOC_NOPM, 0, 0),
	/* Inputs */
	SND_SOC_DAPM_AIF_IN("PCM0 IN", NULL,  0,
			SND_SOC_NOPM, 0, 0),

};

static const struct snd_soc_dapm_route afe_dapm_map[] = {
    /* Line Out connected to LLOUT, RLOUT */
   /* Outputs */
   { "PCM0 OUT", NULL, "I2S Playback" },
   /* Inputs */
   { "I2S Capture", NULL, "PCM0 IN" },

};
static struct snd_soc_component_driver snd_afe_soc_component = {
	.name = "afe-dai",
    .probe = snd_afe_soc_platform_probe,
    .remove = snd_afe_soc_platform_remove,
	.dapm_widgets = afe_widgets,
	.num_dapm_widgets = ARRAY_SIZE(afe_widgets),
	.dapm_routes = afe_dapm_map,
	.num_dapm_routes = ARRAY_SIZE(afe_dapm_map),
};

static int snd_afe_i2s_sc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_afe_i2s_sc *afe;
	int ret;
	struct resource *res;
	struct resource *scr_res;
	unsigned int irq_id, value;

	dev_info(dev, "Probed.\n");

	afe = devm_kzalloc(dev, sizeof(struct sdrv_afe_i2s_sc), GFP_KERNEL);
	if (afe == NULL){
		return -ENOMEM;
	}
	afe->pdev = pdev;
	afe->dev = dev;
	platform_set_drvdata(pdev, afe);

	/* get i2s sc clock FPGA 0x30300000u IRQ_GIC4_I2S_SC1_INTERRUPT_NUM 90
	 */

	afe->clk_i2s = devm_clk_get(&pdev->dev, "i2s-clk");
	if (IS_ERR(afe->clk_i2s))
		return PTR_ERR(afe->clk_i2s);
	DEBUG_ITEM_PRT(clk_get_rate(afe->clk_i2s));

	afe->mclk = devm_clk_get(&pdev->dev, "i2s-mclk");
	if (IS_ERR(afe->mclk))
		return PTR_ERR(afe->mclk);
	DEBUG_ITEM_PRT(clk_get_rate(afe->mclk));

	/* TODO: need clean next debug code later. */
	ret = clk_set_rate(afe->mclk, 12288000);
	if (ret)
		return ret;
	DEBUG_ITEM_PRT(clk_get_rate(afe->mclk));

	/*  ret = devm_request_threaded_irq(&pdev->dev, irq_id, NULL,
									sdrv_i2s_sc_irq_handler,
									IRQF_SHARED
	| IRQF_ONESHOT, pdev->name, afe); if (ret < 0)
	{
		dev_err(afe->dev, "Could not request irq.\n");
		return ret;
	} */
	/* Get register and print */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe->regs = devm_ioremap_resource(afe->dev, res);
	if (IS_ERR(afe->regs)) {
		ret = PTR_ERR(afe->regs);
		goto err_disable;
	}

	dev_info(&pdev->dev,
		 "ALSA X9"
		 " irq(%d) num(%d) vaddr(0x%llx) paddr(0x%llx) \n",
		 irq_id, pdev->num_resources, afe->regs, res->start);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->regs,
					    &sdrv_i2s_sc_regmap_config);

	if (IS_ERR(afe->regmap)) {
		ret = PTR_ERR(afe->regmap);
		goto err_disable;
	}
	afe_i2s_reset(afe);

		/* Get irq and setting */
	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		regmap_exit(afe->regmap);
		dev_err(afe->dev, "%s no irq\n", dev->of_node->name);
		return -ENXIO;
	}
	DEBUG_ITEM_PRT(irq_id);

	ret = devm_request_irq(afe->dev, irq_id, sdrv_i2s_sc_irq_handler, 0,
			       pdev->name, (void *)afe);

	/* TODO: Set i2s sc default afet to full duplex mode. Next change by dts
	 * setting. Change scr setting before, need check SCR_SEC_BASE + (0xC <<
	 * 10) 's value should be 0x3F*/

	afe->is_full_duplex = true;

	ret = device_property_read_u32(dev, "semidrive,full-duplex", &value);
	if (!ret) {
		/* semidrive,full-duplex is 0 set to half duplex mode.  */
		if (value == 0) {
			regmap_exit(afe->regmap);
			dev_err(&pdev->dev, "Don't suppport set to half duplex mode. Please set scr register in ssystem \n");
			goto err_disable;
		}
	}

	/* 	Set register map to cache bypass mode. */
	//	regcache_cache_bypass(afe->regmap, true);

	if (fake_buffer) {
		ret = alloc_fake_buffer();
	}

	afe->playback_dma_data.addr = res->start + I2S_SC_FIFO_OFFSET;
	afe->playback_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	afe->playback_dma_data.maxburst = 4;

	afe->capture_dma_data.addr = res->start + I2S_SC_FIFO_OFFSET;
	afe->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	afe->capture_dma_data.maxburst = 4;

	ret =
	    devm_snd_dmaengine_pcm_register(dev, &sdrv_dmaengine_pcm_config, 0);
	if (ret)
		goto err_disable;

	/* ret = devm_snd_soc_register_platform(dev, &snd_afe_pcm_soc_platform);
	if (ret < 0)
	{
		dev_err(&pdev->dev, "couldn't devm_snd_soc_register_platform
	platform\n"); goto err_disable;
	} */

	ret = devm_snd_soc_register_component(dev, &snd_afe_soc_component,
					      snd_afe_dais,
					      ARRAY_SIZE(snd_afe_dais));
	if (ret < 0) {
		regmap_exit(afe->regmap);
		dev_err(&pdev->dev, "couldn't register component\n");
		goto err_disable;
	}
	afe->tdm_initialized = false;
	afe_i2s_sc_config(afe);
	return 0;
err_disable:
	/* pm_runtime_disable(&pdev->dev); */
	return ret;
}

static int snd_afe_i2s_sc_remove(struct platform_device *pdev)
{

	struct device *dev = &pdev->dev;
	struct sdrv_afe_i2s_sc *afe = platform_get_drvdata(pdev);
	dev_info(dev, "Removed.\n");
	regmap_exit(afe->regmap);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id sdrv_i2s_sc_of_match[] = {
    {
	.compatible = "semidrive,x9-i2s-sc",
    },
};

static struct platform_driver snd_afe_i2s_sc_driver = {
    .driver =
	{
	    .name = DRV_NAME "-i2s",
	    .of_match_table = sdrv_i2s_sc_of_match,
	},
    .probe = snd_afe_i2s_sc_probe,
    .remove = snd_afe_i2s_sc_remove,
};

module_platform_driver(snd_afe_i2s_sc_driver);
MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 I2S SC ALSA SoC device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 i2s asoc device");
