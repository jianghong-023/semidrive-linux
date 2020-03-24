
#ifndef X9_COMMON_H__
#define X9_COMMON_H__
#include "x9-abuf.h"
#include <sound/dmaengine_pcm.h>

/* scr physical address */
#define APB_SCR_SEC_BASE (0x38200000u)

#define DEBUG_FUNC_PRT                                                         \
	printk(KERN_INFO "X9_ALSA: %s:%i --------------------\n", __func__,    \
	       __LINE__);

#define DEBUG_FUNC_PRT_INFO(str)                                               \
	printk(KERN_INFO "X9_ALSA: %s:%i ------%s\n", __func__, __LINE__, str);

#define DEBUG_DEV_PRT(dev)                                                     \
	dev_info(dev, "X9_ALSA: %s:%i --------------------\n", __func__,       \
		 __LINE__);
#define DEBUG_ITEM_PRT(item)                                                   \
	printk(KERN_INFO "X9_ALSA: %s:%i [%s]:0x%x(%d)--------\n", __func__,   \
	       __LINE__, #item, (unsigned int)item, (int)item);

#define DEBUG_ITEM_PTR_PRT(item)                                                   \
	printk(KERN_INFO "X9_ALSA: %s:%i [%s]:0x%llx(%p)--------\n", __func__,   \
	       __LINE__, #item, (long long unsigned int)item, item);

#define X9_I2S_SC_FIFO_SIZE (2048)

/* defaults x9 sound card setting*/
#define MAX_ABUF_SIZE (8 * X9_I2S_SC_FIFO_SIZE)
#define MIN_ABUF_SIZE (256)
#define MAX_PERIOD_SIZE (4 * X9_I2S_SC_FIFO_SIZE)
#define MIN_PERIOD_SIZE (2 * 8)
#define MAX_PERIODS (4 * X9_I2S_SC_FIFO_SIZE)
#define MIN_PERIODS (2)
#define MAX_ABUF

#define SND_FORMATS ((SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE))

#define SND_RATE SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000
#define SND_RATE_MIN 8000
#define SND_RATE_MAX 48000
#define SND_CHANNELS_MIN 1
#define SND_CHANNELS_MAX 2

enum { X9_I2S_SCLK_MCLK,
};

/* x9 afe common structure */
struct x9_afe {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *base_addr;
	void __iomem *sram_address;
};

/* x9 i2s sc audio front end structure */
struct x9_afe_i2s_sc {
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
struct x9_afe_i2s_mc {
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
struct x9_afe_pcm {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *regs;
	/* TODO: Alloc a sram address, use this to for dma transfer  */
	// void __iomem *sram_address;
	// struct regmap *regmap;
	struct x9_audio_buf abuf[X9_ABUF_NUMB];
	/*Back end Data status*/
	/*Audio front end*/
	bool suspended;

	int afe_pcm_ref_cnt;
	int adda_afe_pcm_ref_cnt;
	int i2s_out_on_ref_cnt;
};

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

/* x9 dummy model for debug*/
struct x9_dummy_model {
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

#endif /* X9_COMMON_H__ */
