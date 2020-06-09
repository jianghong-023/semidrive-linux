/* sdrv-common.h
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

#ifndef SDRV_COMMON_H__
#define SDRV_COMMON_H__
#include "sdrv-abuf.h"
#include <sound/dmaengine_pcm.h>

/* scr physical address */
#define APB_SCR_SEC_BASE (0x38200000u)

#define DEBUG_FUNC_PRT                                                         \
	printk(KERN_INFO "SDRV_ALSA: %s:%-120i\n", __func__,    \
	       __LINE__);

#define DEBUG_FUNC_PRT_INFO(str)                                               \
	printk(KERN_INFO "SDRV_ALSA: %s:%-120i %s\n", __func__, __LINE__, str);

#define DEBUG_DEV_PRT(dev)                                                     \
	dev_info(dev, "SDRV_ALSA: %s:%-120i \n", __func__,       \
		 __LINE__);

#define DEBUG_ITEM_PRT(item)                                                   \
	printk(KERN_INFO "SDRV_ALSA: %s:%-120i [%s]:0x%x(%d)\n", __func__,   \
	       __LINE__, #item, (unsigned int)item, (int)item);

#define DEBUG_ITEM_PTR_PRT(item)                                                   \
	printk(KERN_INFO "SDRV_ALSA: %s:%-120i [%s]:0x%llx(%p)\n", __func__,   \
	       __LINE__, #item, (long long unsigned int)item, item);

#define SDRV_I2S_SC_FIFO_SIZE (2048)

/* defaults x9 sound card setting*/
#define MAX_ABUF_SIZE (8 * 2 * SDRV_I2S_SC_FIFO_SIZE)
#define MIN_ABUF_SIZE (256)
#define MAX_PERIOD_SIZE (4 * SDRV_I2S_SC_FIFO_SIZE)
#define MIN_PERIOD_SIZE (2)
#define MAX_PERIODS (4 * SDRV_I2S_SC_FIFO_SIZE)
#define MIN_PERIODS (2)
#define MAX_ABUF MAX_ABUF_SIZE

#define SND_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |SNDRV_PCM_FMTBIT_S32_LE)

#define SND_RATE SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000
#define SND_RATE_MIN 8000
#define SND_RATE_MAX 48000
#define SND_CHANNELS_MIN 1
#define SND_CHANNELS_MAX 2

enum {
	I2S_SCLK_MCLK,
};

/* x9 afe common structure */
struct sdrv_afe {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *base_addr;
	void __iomem *sram_address;
};

/* x9 i2s sc audio front end structure */
struct sdrv_afe_i2s_sc {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *regs;

	/* TODO: Alloc a sram address, use this to for dma transfer  */
	void __iomem *sram_address;

	/* this is clk for i2s clock */
	struct clk *clk_i2s;

	/* mclk for output  */
	struct clk *mclk;
	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct snd_dmaengine_dai_dma_data playback_dma_data;
	char capture_dma_chan[8];
	char playback_dma_chan[8];
	struct snd_dmaengine_pcm_config dma_config;
	struct regmap *regmap;
	struct snd_pcm_substream __rcu *tx_substream;
	struct snd_pcm_substream __rcu *rx_substream;

	unsigned int tx_ptr; /* next frame index in the sample buffer */
	
	unsigned int periods;
	/* current fifo level estimate.
	 * Doesn't have to be perfectly accurate, but must be not less than
	 * the actual FIFO level in order to avoid stall on push attempt.
	 */
	unsigned tx_fifo_level;

	/* FIFO level at which level interrupt occurs */
	unsigned tx_fifo_low;

	/* maximal FIFO level */
	unsigned tx_fifo_high;

	/* Full Duplex mode true is full duplex*/
	bool is_full_duplex;

	/* In full-duplex playing and capturing flag */
	atomic_t playing;
	atomic_t capturing;
};

/* x9 i2s mc audio front end structure */
struct sdrv_afe_i2s_mc {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *regs;
	/* TODO: Alloc a sram address, use this to for dma transfer  */
	void __iomem *sram_address;

	struct clk *clk_i2s;
	struct clk *mclk;

	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct snd_dmaengine_dai_dma_data playback_dma_data;
	char capture_dma_chan[8];
	char playback_dma_chan[8];
	struct snd_dmaengine_pcm_config dma_config;
	struct regmap *regmap;
	struct snd_pcm_substream __rcu *tx_substream;
	struct snd_pcm_substream __rcu *rx_substream;

	unsigned int tx_ptr; /* next frame index in the sample buffer */
	unsigned int periods;
	/* current fifo level estimate.
	 * Doesn't have to be perfectly accurate, but must be not less than
	 * the actual FIFO level in order to avoid stall on push attempt.
	 */
	unsigned tx_fifo_level;

	/* FIFO level at which level interrupt occurs */
	unsigned tx_fifo_low;

	/* maximal FIFO level */
	unsigned tx_fifo_high;

	/* loop back test  */
	bool loopback_mode;

	/* stream is slav mode  */
	bool is_slave;
};

/* Define audio front end model */
struct sdrv_afe_pcm {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *regs;
	/* TODO: Alloc a sram address, use this to for dma transfer  */
	// void __iomem *sram_address;
	// struct regmap *regmap;
	struct sdrv_audio_buf abuf[SDRV_ABUF_NUMB];
	/*Back end Data status*/
	/*Audio front end*/
	bool suspended;

	int afe_pcm_ref_cnt;
	int adda_afe_pcm_ref_cnt;
	int i2s_out_on_ref_cnt;
};


/* x9 dummy model for debug*/
struct sdrv_dummy_model {
	const char *name;
	int (*playback_constraints)(struct snd_pcm_runtime *runtime);
	int (*capture_constraints)(struct snd_pcm_runtime *runtime);
	u64 formats;
	size_t buffer_bytes_max;
	size_t period_bytes_min;
	size_t period_bytes_max;
	unsigned int periods_min;
	unsigned int periods_max;
	unsigned int rates;
	unsigned int rate_min;
	unsigned int rate_max;
	unsigned int channels_min;
	unsigned int channels_max;
};

#endif /* SDRV_COMMON_H__ */
