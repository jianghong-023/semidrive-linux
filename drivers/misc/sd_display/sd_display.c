#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/time.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>

#include "include/sd_display.h"
#include "include/dc.h"
#include "include/disp_param.h"

#ifdef SD_NEED_REGIST_DEV
#define SD_DISP_NAME "sd-disp"
#define SD_DISP_MISCDEV_MINOR 188
#endif

static bool g_ds_ready = false;
static struct sd_display_res *disp_res = NULL;
struct disp_info *dpu_info = NULL;
struct dcparam *dc_enode = NULL;
extern int dc_setup(struct dc_device *dev);
extern dc_profile_t *get_dc_prop(uint8_t id);

void post_display(struct dcparam *param)
{
	static int count = 0;
	if (param == NULL) {
		pr_err("invalid input!!");
		return;
	}
	list_add_tail(&param->list, &disp_res->todo);
        pr_info("post %d frames", count++);
}

EXPORT_SYMBOL(post_display);

typedef void (*RetireCallback)(struct dcparam *param);

static RetireCallback pRetireCallBack = NULL;

void set_retirecb(RetireCallback pRetireCb)
{
	pRetireCallBack = pRetireCb;
}
EXPORT_SYMBOL(set_retirecb);

#ifdef SD_NEED_REGIST_DEV
int sd_disp_open(struct inode *node, struct file *file)
{
	struct sd_display_res *disp = NULL;
	struct miscdevice *misc = NULL;

	pr_info("open sd disp device\n");

	misc = file->private_data;
	if (IS_ERR_OR_NULL(misc)) {
		pr_err("sd disp has no misc device\n");
		return -EINVAL;
	}

	disp = container_of(file->private_data, struct sd_display_res, misc_dev);
	if (IS_ERR_OR_NULL(disp)) {
		pr_err("open sd disp device failed!\n");
		return -EINVAL;
	}

	file->private_data = disp;

	return 0;
}

int sd_disp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;
	int ctrl_code = _IOC_NR(cmd);
	size_t size = _IOC_SIZE(cmd);
	struct sd_display_res disp = NULL;

	if (_IOC_TYPE(cmd) != ) {
		ret = -ENOTTY;
		goto exit;
	}
	disp = file->private_data;
	if (IS_ERR_OR_NULL(disp)) {
		pr_err("ioctl with err dev\n");
		ret = -ENODEV;
		goto exit;
	}

	switch (ctrl_code &)

exit:
	return ret;
}

static const struct file_operations sd_disp_fops = {
	.owner = THIS_MODULE,
	.open = sd_disp_open,
	.unlocked_ioctl = sd_disp_ioctl,
};
#endif

static uint32_t get_data_uv_mode(int frm_format)
{
	uint32_t data_uv_mode;
	switch (frm_format) {
		case DATA_YUV420:
		case DATA_NV12:
		case DATA_NV21:
		case DATA_YUV420_10_16PACK:
			data_uv_mode = UV_YUV420;
			break;
		case DATA_YUV422:
			data_uv_mode = UV_YUV422;
			break;
		case DATA_YUV444:
		case DATA_AYUV4444:
		case DATA_ARGB8888:
		case DATA_ARGB2101010:
		case DATA_RGB888:
		case DATA_RGB666:
		case DATA_RGB565:
		case DATA_ARGB1555:
		case DATA_ARGB4444:
			data_uv_mode = UV_YUV444_RGB;
			break;
		default:
			data_uv_mode = UV_YUV444_RGB;
			break;
	}
	return data_uv_mode;
}


static int set_post_param(struct dcparam *param)
{
	struct dcparam *pa = param;

	if (!pa) {
		return -1;
	}
	if (!pa->buffer) {
		dpu_info->layers[1].layer_enable = 0;
		dpu_info->layers[1].layer_dirty = 1;
		return 0;
	}
	dpu_info->layers[1].layer_enable = 1;
	dpu_info->layers[1].width = pa->width;
	dpu_info->layers[1].height = pa->height;
	dpu_info->layers[1].frm_format = pa->format;
	dpu_info->layers[1].data_mode = pa->data_mode;
	dpu_info->layers[1].frm_buf_str_fmt = pa->frm_buf_str_fmt;
	dpu_info->layers[1].frame_addr_y = (uint64_t)pa->buffer + pa->imgbase_offset_y;
	dpu_info->layers[1].frame_addr_u = pa->buffer + pa->imgbase_offset_u;
	dpu_info->layers[1].frame_addr_v = pa->buffer + pa->imgbase_offset_v;
	dpu_info->layers[1].y_stride = pa->stride_y;
	dpu_info->layers[1].u_stride = pa->stride_u;
	dpu_info->layers[1].v_stride = pa->stride_v;
	dpu_info->layers[1].offset_x = pa->offset_x;
	dpu_info->layers[1].offset_y = pa->offset_y;
	dpu_info->layers[1].pos_x = pa->pos_x;
	dpu_info->layers[1].pos_y = pa->pos_y;
	dpu_info->layers[1].data_uv_mode = get_data_uv_mode(pa->format);
	dpu_info->layers[1].data_mode = pa->data_mode;
	dpu_info->layers[1].frm_buf_str_fmt = pa->frm_buf_str_fmt;
	dpu_info->layers[1].sf_info.z_order = 1;

	dpu_info->layers[1].layer_dirty = 1;

	return 0;
}

static int frame_config(void)
{
	struct dc_ops *ops = disp_res->dc.ops;

	ops->config(&disp_res->dc, dpu_info);

	return 0;
}
static int sd_disp_thread(void *data)
{
	struct dcparam *param;
	struct sd_display_res *disp = data;
	uint32_t cur_flag = 0;
	uint32_t last_flag = 0;
        uint32_t need_release = 0;
	struct dcparam *next_node = NULL;
	struct dcparam *current_node = NULL;
	struct dcparam *last_node = NULL;

	disp->disp_thread_status = 1;
	pr_info("sd_disp_thread is running!\n");
	while (1) {
		wait_event(disp->go, atomic_read(&disp->ready));
		atomic_set(&disp->ready, 0);

                if (need_release == 1) {
                        if (current_node) {
                            pr_err("need release, will call pRetireCallBack for current!!\n");
                            pRetireCallBack(current_node);
                        }
                        pr_err("need release, will call pRetireCallBack for next!!\n");
			pRetireCallBack(next_node);
			need_release = 0;
			cur_flag = 0;
			last_flag = 0;
			next_node = NULL;
			current_node = NULL;
			last_node = NULL;
                }

		if (list_empty(&disp->todo)) {
			continue;
		}
                pr_err("get one frame!!\n");

		param = list_entry(disp->todo.next, struct dcparam, list);
		list_del(&param->list);
		set_post_param(param);

		frame_config();
		if (last_flag) {
                        pr_err("will call pRetireCallBack for last!!\n");
			last_node = current_node;
			pRetireCallBack(last_node);
		}
		if (cur_flag) {
			current_node = next_node;
			last_flag = 1;
		}
		next_node = param;
		cur_flag = 1;
		if (!param->buffer) {
                        pr_info("empty frame!!");
			need_release = 1;
		}
	}

	return 0;
}

static void dc_irq_queue_work(struct work_struct *work)
{
	struct sd_display_res *disp = container_of(work,
								struct sd_display_res,
								irq_queue);

	if (disp->disp_thread_status) {
		atomic_set(&disp->ready, 1);
		wake_up(&disp->go);
	}
}
static int sd_disp_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct dc_profile_t *prop;
	struct sd_display_res *priv;
	struct disp_info *info;
	struct dcparam *dc_param;
	struct task_struct *post_thread;

	pr_info("sd disp probe enter\n");
	if (g_ds_ready) {
		printk("sd display is ready!");
		return -1;
	}
	priv = devm_kmalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		pr_err("sd_display_res malloc memory fail!\n");
		return -ENOMEM;
	}

#ifdef SD_NEED_REGIST_DEV
	priv->misc_dev.minor = SD_DISP_MISCDEV_MINOR;
	priv->misc_dev.name = SD_DISP_NAME;
	priv->misc_dev.fops = ;
#endif

	priv->id = 0;
#if 0
	sd_adv_init(0);
#endif
	priv->state = SD_INVALID;
	priv->disp_thread_status = 0;

	/* fetch dc profile to setup display resources */
	prop = get_dc_prop((uint8_t)priv->id);
	priv->dc.id = prop->id;
	priv->dc.irq = prop->irq;
	priv->dc.tcon_irq = platform_get_irq(pdev, 0);

	priv->layer_n = prop->layer_n;

	priv->dc.reg_base = (ulong) ioremap_nocache(prop->base_addr, 0x20000);

	INIT_LIST_HEAD(&priv->todo);
	init_waitqueue_head(&priv->go);
	post_thread = kthread_run(sd_disp_thread, priv, "sd-display");

	INIT_WORK(&priv->irq_queue, dc_irq_queue_work);

	/* setup dc */
	ret = dc_setup(&priv->dc);

	if (ret) {
	    pr_err("dc_setup failed!\n");
	    return ret;
	}

	priv->dc.ops->vsync_set(&priv->dc, 1);
	ret = devm_request_irq(dev, priv->dc.tcon_irq, priv->dc.ops->irq_handler,
					IRQF_TRIGGER_NONE, dev_name(dev), priv);
	if(ret < 0) {
		pr_err("Failed to request irq %d: %d\n", priv->dc.tcon_irq, ret);
		return ret;
	}

	priv->state = SD_READY;
	disp_res = priv;

#ifdef SD_NEED_REGIST_DEV
	ret = misc_register(&priv->misc_dev);
	if (ret < 0) {
		pr_err("misc_regiseter() for minor %d failed\n", SD_DISP_MISCDEV_MINOR);
		return ret;
	}
#endif
	info = kmalloc(sizeof(struct disp_info), GFP_KERNEL);
	if (info == NULL) {
		pr_err("disp_info malloc memory fail!\n");
		goto err_mis_regieter;
	}
	memset(info, 0, sizeof(struct disp_info));
	dpu_info = info;

	dc_param = kmalloc(sizeof(struct dcparam), GFP_KERNEL);
	if (dc_param == NULL) {
		pr_err("dc_param malloc memory fail!\n");
		goto err_dc_param;
	}
	dc_param->buffer = NULL;
	dc_enode = dc_param;

	g_ds_ready = true;
	return 0;
err_dc_param:
	kfree(dpu_info);
err_mis_regieter:
#ifdef SD_NEED_REGIST_DEV
	misc_deregister(&priv->misc_dev);
#endif
	return ret;
}

static int sd_disp_remove(struct platform_device *pdev)
{
	if (dc_enode) {
		kfree(dc_enode);
	}
	if (dpu_info) {
		kfree(dpu_info);
	}
#ifdef SD_NEED_REGIST_DEV
	misc_deregister(&disp_res->misc_dev);
#endif
	return 0;
}

static const struct of_device_id sd_disp_dt_ids[] = {
	{
		.compatible = "semidrive,sd-disp",
	},
	{}
};

static struct platform_driver sd_disp_driver = {
	.probe = sd_disp_probe,
	.remove = sd_disp_remove,
	.driver = {
		.name = "sd-disp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sd_disp_dt_ids),
	},
};

static int __init sd_display_init(void)
{
	pr_info("sd display init enter\n");
	return platform_driver_register(&sd_disp_driver);
}

static void __exit sd_display_exit(void)
{
	platform_driver_unregister(&sd_disp_driver);
}

module_init(sd_display_init);
module_exit(sd_display_exit);
MODULE_DESCRIPTION("SD Display Driver");
MODULE_LICENSE("GPL");
