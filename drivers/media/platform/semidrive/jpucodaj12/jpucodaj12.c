/*
 *=========================================================================
 * This file is linux device driver for JPU.
 *-------------------------------------------------------------------------
 *
 *      This confidential and proprietary software may be used only
 *    as authorized by a licensing agreement from Chips&Media Inc.
 *    In the event of publication, the following notice is applicable:
 *
 *           (C) COPYRIGHT 2006 - 2016  CHIPS&MEDIA INC.
 *                     ALL RIGHTS RESERVED
 *
 *      The entire notice above must be reproduced on all authorized
 *      copies.
 *
 *=========================================================================
 *--------------------------------------------------------------------------
 * Revision Histrory:
 *
 * Version 1.1, 20/11/2020  chentianming<tianming.chen@semidrive.com>
 *                          updata & refactor this file
 */

#define pr_fmt(fmt) "Jpu : " fmt
#ifndef DEBUG
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/sched/signal.h>

#include <linux/reset.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "jpu.h"

#define JPU_PLATFORM_DEVICE_NAME    "semidrive_jpu"
#define JPU_DEV_NAME                "jpucoda"
#define MAX_NUM_JPU_CORE            1
#define MAX_NUM_INSTANCE            4
#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
#define MAX_INST_HANDLE_SIZE        48

#define JPU_SUPPORT_CLOCK_CONTROL
/* driver disable and enable IRQ whenever interrupt asserted. */
//#define JPU_IRQ_CONTROL

#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define NPT_BASE                        0x0000
#define NPT_REG_SIZE                    0x300
#define MJPEG_PIC_STATUS_REG(_inst_no)  (NPT_BASE + (_inst_no*NPT_REG_SIZE) + 0x004)
#define ReadJpuRegister(addr)           (*(volatile unsigned int *)(s_jpu_drv_context->jpu_register.virt_addr + addr))
#define WriteJpuRegister(addr, val)     (*(volatile unsigned int *)(s_jpu_drv_context->jpu_register.virt_addr + addr) = (unsigned int)val)

/* to track the allocated memory buffer */
struct jpudrv_buffer_pool_t {
	struct list_head        list;
	struct jpudrv_buffer_t  jb;
	struct file            *filp;
};

/* to track the instance index and buffer in instance pool */
struct jpudrv_instance_list_t {
	struct list_head    list;
	unsigned long       inst_idx;
	struct file        *filp;
};

struct jpudrv_instance_pool_t {
	unsigned char codec_inst_pool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
};

struct jpu_clock {
	struct clk *core_aclk;
	struct clk *core_cclk;
	uint32_t clk_reference;
};

struct jpu_drv_context_t {
	uint32_t open_count;  /* file open count */
	int jpu_major;
	int jpu_open_ref_count; /* instances open count */
	int jpu_irq;
	int interrupt_flag[MAX_NUM_INSTANCE];
	uint32_t interrupt_reason[MAX_NUM_INSTANCE];
	jpudrv_buffer_t instance_pool;
	jpudrv_buffer_t jpu_register;
	wait_queue_head_t interrupt_wait_q[MAX_NUM_INSTANCE];
	struct fasync_struct *async_queue;
	struct list_head jbp_head;
	struct list_head inst_list_head;
	struct device  *jpu_device;
	struct cdev jpu_cdev;
	struct jpu_clock jpu_clk;
	struct class  *jpu_class;
	struct device *jpu_class_device;
};

static spinlock_t s_jpu_lock = __SPIN_LOCK_UNLOCKED(s_jpu_lock);
static DEFINE_SEMAPHORE(s_jpu_sem);
static struct jpu_drv_context_t *s_jpu_drv_context;

static void jpu_hw_reset(struct device *dev);
static void jpu_clk_disable(struct jpu_clock *jpu_clk);
static void jpu_clk_enable(struct jpu_clock *jpu_clk);
static int jpu_clk_get(struct device *dev, struct jpu_drv_context_t *drv_context);
static void jpu_clk_put(struct jpu_clock *jpu_clk);

/**
 * jpu_hw_reset - reset jpu codaj12-hardware
 * param: device jpu device to reset
 * return: void
 * note: call function when jpu init or reboot
 */
static void jpu_hw_reset(struct device *dev)
{
	struct reset_control *jpu_rst = NULL;

	if (!dev) {
		pr_err("error: hw reset param error\n");
		return;
	}

	jpu_rst = devm_reset_control_get(dev, "jpu-reset");

	if (IS_ERR(jpu_rst)) {
		pr_err("error: %s missing controller reset\n", __func__);
		return;
	}

	if (reset_control_reset(jpu_rst)) {
		reset_control_put(jpu_rst);
		pr_err("error: %s reset jpu failed\n", __func__);
		return;
	}

	reset_control_put(jpu_rst);
	pr_debug("%s request jpu reset success\n", __func__);
}

static int jpu_clk_get(struct device *dev, struct jpu_drv_context_t *drv_context)
{
#ifdef JPU_SUPPORT_CLOCK_CONTROL

	if (!dev || !drv_context) {
		pr_err("error: %s param error\n", __func__);
		return -EFAULT;
	}

	drv_context->jpu_clk.core_aclk = clk_get(dev, "core-aclk");
	drv_context->jpu_clk.core_cclk = clk_get(dev, "core-cclk");

	if (drv_context->jpu_clk.core_aclk == NULL
		|| IS_ERR(drv_context->jpu_clk.core_aclk)
		|| drv_context->jpu_clk.core_cclk == NULL
		|| IS_ERR(drv_context->jpu_clk.core_cclk)) {
		pr_err("error: %s get clk error\n", __func__);
		return -ENOENT;
	}

	pr_info("%s jpu core-aclk-rate %lu, jpu core-cclk rate %lu\n", __func__,
		clk_get_rate(drv_context->jpu_clk.core_aclk),
		clk_get_rate(drv_context->jpu_clk.core_cclk));
	return 0;
#else
	pr_warn("warning: %s no support clock control\n", __func__);
	return 0;
#endif
}

static void jpu_clk_put(struct jpu_clock *clk)
{
#ifdef JPU_SUPPORT_CLOCK_CONTROL

	if (!clk || !clk->core_aclk || !clk->core_cclk) {
		pr_err("error: %s param error\n", __func__);
		return;
	}

	clk_put(clk->core_aclk);
	clk_put(clk->core_cclk);

	clk->core_aclk = NULL;
	clk->core_cclk = NULL;
	pr_debug("%s put jpu clk success\n", __func__);

#endif
}

static void jpu_clk_enable(struct jpu_clock *clk)
{
#ifdef JPU_SUPPORT_CLOCK_CONTROL

	if (!clk || !clk->core_aclk || !clk->core_cclk) {
		pr_err("error: %s param error\n", __func__);
		return;
	}

	if (!(IS_ERR(clk->core_aclk) || IS_ERR(clk->core_cclk))) {
		if (clk_prepare(clk->core_aclk) != 0) {
			pr_err("error: jpu ackl prepare bcore clk error code\n");
			return;
		}

		if (clk_prepare(clk->core_cclk) != 0) {
			pr_err("error: jpu ackl prepare core clk error\n");
			return;
		}

		clk_enable(clk->core_aclk);
		clk_enable(clk->core_cclk);
		clk->clk_reference++;
		pr_debug(" %s clk_reference %d\n", __func__, clk->clk_reference);
	}

#endif
}

static void jpu_clk_disable(struct jpu_clock *clk)
{
#ifdef JPU_SUPPORT_CLOCK_CONTROL

	if (!clk || !clk->core_aclk || !clk->core_cclk) {
		pr_err("error: %s param error\n", __func__);
		return;
	}

	if (!(IS_ERR(clk->core_aclk) || IS_ERR(clk->core_cclk))) {
		clk_disable(clk->core_cclk);
		clk_disable(clk->core_aclk);

		clk_unprepare(clk->core_aclk);
		clk_unprepare(clk->core_cclk);
		clk->clk_reference--;
		pr_debug("disable clk clk_reference %d\n", clk->clk_reference);
	}

#endif
}

/**
 * jpu_alloc_dma_buffer - alloc dma memory for bitstream or yuv
 * param: drv_context contain device of jpu
 * param: jb buffer info, size , phy/dma/virtual address
 * return: 0 success
 * note:
 */
static int jpu_alloc_dma_buffer(struct jpu_drv_context_t *drv_context, jpudrv_buffer_t *jb)
{
	if (!drv_context || !jb) {
		pr_err("error: %s get param error\n", __func__);
		return -ENOMEM;
	}

	jb->base = (unsigned long) dma_alloc_coherent(drv_context->jpu_device, PAGE_ALIGN(jb->size), (dma_addr_t *)(&jb->phys_addr),  GFP_KERNEL | GFP_USER | GFP_DMA);
	if ((void *)(jb->base) == NULL) {
		pr_err(" error: %s physical memory allocation error size=%d\n", __func__, jb->size);
		return -ENOMEM;
	}

	pr_debug("%s alloc dma buffer base %p, phys_addr %p, size %d\n", __func__,
		(void *)jb->base,
		(void *)jb->phys_addr,
		jb->size);
	return 0;
}

static void jpu_free_dma_buffer(struct jpu_drv_context_t *drv_context, jpudrv_buffer_t *jb)
{
	if (!drv_context || !jb) {
		pr_err("error: %s get param error\n", __func__);
		return;
	}

	if (jb->base)
		dma_free_coherent(drv_context->jpu_device, PAGE_ALIGN(jb->size), (void *) jb->base, jb->phys_addr);

	pr_debug(" %s free dma buffer base %p, phys_addr %p, size %d\n", __func__,
		(void *)jb->base,
		(void *)jb->phys_addr,
		jb->size);
}

static int jpu_free_instances(struct file *filp)
{
	struct jpudrv_instance_list_t *vil = NULL, *n = NULL;
	struct jpudrv_instance_pool_t *vip = NULL;
	struct jpu_drv_context_t *drv_context = NULL;
	void *vip_base = NULL;
	int instance_pool_size_per_core = 0;
	void *jdi_mutexes_base = NULL;
	const int PTHREAD_MUTEX_T_DESTROY_VALUE = 0xdead10cc;

	pr_debug("[JPUDRV+] %s\n", __func__);
	drv_context = (struct jpu_drv_context_t *)(filp->private_data);

	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return -ENOMEM;
	}

	/* s_instance_pool.size assigned to the size of all core once call JDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core = (drv_context->instance_pool.size / MAX_NUM_JPU_CORE);
	list_for_each_entry_safe(vil, n, &drv_context->inst_list_head, list) {
		if (vil->filp == filp) {
			vip_base = (void *)(drv_context->instance_pool.base + instance_pool_size_per_core);
			pr_debug("[JPUDRV] detect instance crash instIdx=%d\n", (int) vil->inst_idx);
			vip = (struct jpudrv_instance_pool_t *) vip_base;
			if (vip) {
				/* only first 4 byte is key point(inUse of CodecInst in jpuapi) to free the corresponding instance. */
				memset(&vip->codec_inst_pool[vil->inst_idx], 0x00, 4);
				jdi_mutexes_base = (vip_base + (instance_pool_size_per_core - PTHREAD_MUTEX_T_HANDLE_SIZE * 4));
				pr_debug("[JPUDRV] force to destroy jdi_mutexes_base=%p in userspace\n", jdi_mutexes_base);

				if (jdi_mutexes_base) {
					int i = 0;
					for (i = 0; i < PTHREAD_MUTEX_T_HANDLE_SIZE; i++) {
						memcpy(jdi_mutexes_base, &PTHREAD_MUTEX_T_DESTROY_VALUE, PTHREAD_MUTEX_T_HANDLE_SIZE);
						jdi_mutexes_base += PTHREAD_MUTEX_T_HANDLE_SIZE;
					}
				}
			}

			drv_context->jpu_open_ref_count--;
			list_del(&vil->list);
			kfree(vil);
		}
	}

	pr_debug("[JPUDRV-] %s\n", __func__);
	return 0;
}

static int jpu_free_buffers(struct file *filp)
{
	struct jpudrv_buffer_pool_t *pool = NULL, *n = NULL;
	jpudrv_buffer_t jb = {0};
	struct jpu_drv_context_t *drv_context = NULL;

	pr_debug("[JPUDRV+] %s\n", __func__);
	drv_context = (struct jpu_drv_context_t *)(filp->private_data);

	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return -ENOMEM;
	}

	list_for_each_entry_safe(pool, n, &drv_context->jbp_head, list) {
		if (pool->filp == filp) {
			jb = pool->jb;

			if (jb.base) {
				jpu_free_dma_buffer(drv_context, &jb);
				list_del(&pool->list);
				kfree(pool);
			}
		}
	}

	pr_debug("[JPUDRV-] %s\n", __func__);
	return 0;
}


static irqreturn_t jpu_irq_handler(int irq, void *dev_id)
{
	int i = 0;
	u32 flag = 0;
	struct jpu_drv_context_t  *drv_context  = NULL;

	pr_debug("[JPUDRV+]%s\n", __func__);
	drv_context  = (struct jpu_drv_context_t *) dev_id;
	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return IRQ_HANDLED;
	}

#ifdef JPU_IRQ_CONTROL
	disable_irq_nosync(drv_context->jpu_irq);
#endif

	for (i = 0; i < MAX_NUM_INSTANCE; i++) {
		flag = ReadJpuRegister(MJPEG_PIC_STATUS_REG(i));
		if (flag != 0) {
			WriteJpuRegister(MJPEG_PIC_STATUS_REG(i), flag);
			break;
		}
	}

	drv_context->interrupt_reason[i] = flag;
	drv_context->interrupt_flag[i] = 1;
	pr_debug("reason (%d) interrupt flag: %08x, %08x\n", i, drv_context->interrupt_reason[i], MJPEG_PIC_STATUS_REG(i));

	if (drv_context->async_queue)
		kill_fasync(&drv_context->async_queue, SIGIO, POLL_IN);    // notify the interrupt to userspace

	wake_up_interruptible(&drv_context->interrupt_wait_q[i]);
	pr_debug("[JPUDRV-] %s\n", __func__);
	return IRQ_HANDLED;
}

static int jpu_open(struct inode *inode, struct file *filp)
{

	spin_lock(&s_jpu_lock);
	if (s_jpu_drv_context) {
		s_jpu_drv_context->open_count++;
		filp->private_data = (void *)(s_jpu_drv_context);
	}
	spin_unlock(&s_jpu_lock);

	pr_debug("%s\n", __func__);
	return 0;
}

static long jpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int ret = 0;
	struct jpu_drv_context_t *drv_context = NULL;

	drv_context  = (struct jpu_drv_context_t *)(filp->private_data);
	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return IRQ_HANDLED;
	}

	switch (cmd) {
	case JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY: {
		struct jpudrv_buffer_pool_t *jbp = NULL;

		pr_debug("[JPUDRV+] JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");

		ret = down_interruptible(&s_jpu_sem);
		if (ret == 0) {
			jbp = kzalloc(sizeof(struct jpudrv_buffer_pool_t), GFP_KERNEL);

			if (!jbp) {
				up(&s_jpu_sem);
				return -ENOMEM;
			}

			ret = copy_from_user(&(jbp->jb), (jpudrv_buffer_t *) arg, sizeof(jpudrv_buffer_t));
			if (ret) {
				kfree(jbp);
				up(&s_jpu_sem);
				return -EFAULT;
			}

			ret = jpu_alloc_dma_buffer(drv_context, &(jbp->jb));
			if (ret == -1) {
				ret = -ENOMEM;
				kfree(jbp);
				up(&s_jpu_sem);
				break;
			}

			ret = copy_to_user((void __user *) arg, &(jbp->jb), sizeof(jpudrv_buffer_t));
			if (ret) {
				kfree(jbp);
				ret = -EFAULT;
				up(&s_jpu_sem);
				break;
			}

			jbp->filp = filp;
			spin_lock(&s_jpu_lock);
			list_add(&jbp->list, &drv_context->jbp_head);
			spin_unlock(&s_jpu_lock);

			up(&s_jpu_sem);
		}

		pr_debug("[JPUDRV-]JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
		break;
	}
	case JDI_IOCTL_FREE_PHYSICALMEMORY: {
		struct jpudrv_buffer_pool_t *jbp = NULL, *n = NULL;
		jpudrv_buffer_t jb = {0};

		pr_debug("[JPUDRV+]VDI_IOCTL_FREE_PHYSICALMEMORY\n");

		ret = down_interruptible(&s_jpu_sem);
		if (ret == 0) {
			ret = copy_from_user(&jb, (jpudrv_buffer_t *) arg, sizeof(jpudrv_buffer_t));
			if (ret) {
				up(&s_jpu_sem);
				return -EACCES;
			}

			if (jb.base)
				jpu_free_dma_buffer(drv_context, &jb);

			spin_lock(&s_jpu_lock);
			list_for_each_entry_safe(jbp, n, &drv_context->jbp_head, list) {
				if (jbp->jb.base == jb.base) {
					list_del(&jbp->list);
					kfree(jbp);
					break;
				}
			}
			spin_unlock(&s_jpu_lock);
			up(&s_jpu_sem);
		}

		pr_debug("[JPUDRV-]VDI_IOCTL_FREE_PHYSICALMEMORY\n");
		break;
	}
	case JDI_IOCTL_WAIT_INTERRUPT: {
		jpudrv_intr_info_t info = {0};
		u32 instance_no = 0;

		pr_debug("[JPUDRV+]JDI_IOCTL_WAIT_INTERRUPT\n");
		ret = copy_from_user(&info, (jpudrv_intr_info_t *) arg, sizeof(jpudrv_intr_info_t));
		if (ret != 0)
			return -EFAULT;

		instance_no = info.inst_idx;
		pr_debug("INSTANCE NO: %d, timeout %d\n", instance_no, info.timeout);
		ret = wait_event_interruptible_timeout(drv_context->interrupt_wait_q[instance_no], drv_context->interrupt_flag[instance_no] != 0, msecs_to_jiffies(info.timeout));
		if (ret <= 0) {
			pr_err("error: %s INSTANCE NO: %d ETIME ret %d\n", __func__, instance_no, ret);
			ret = -ETIME;
			break;
		}

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			pr_err("error: %s INSTANCE NO: %d ERESTARTSYS\n", __func__, instance_no);
			break;
		}

		pr_debug("INST(%d) s_interrupt_flag(%d), reason(0x%08x)\n", instance_no, drv_context->interrupt_flag[instance_no], drv_context->interrupt_reason[instance_no]);
		info.intr_reason = drv_context->interrupt_reason[instance_no];
		drv_context->interrupt_flag[instance_no] = 0;
		drv_context->interrupt_reason[instance_no] = 0;
		ret = copy_to_user((void __user *) arg, &info, sizeof(jpudrv_intr_info_t));

#ifdef JPU_IRQ_CONTROL
		enable_irq(drv_context->jpu_irq);
#endif
		pr_debug("[JPUDRV-]JDI_IOCTL_WAIT_INTERRUPT\n");
		if (ret != 0)
			return -EFAULT;

		break;
	}
	case JDI_IOCTL_SET_CLOCK_GATE: {
		u32 clkgate = 0;

		if (get_user(clkgate, (u32 __user *) arg))
			return -EFAULT;

		if (clkgate)
			jpu_clk_enable(&drv_context->jpu_clk);
		else
			jpu_clk_disable(&drv_context->jpu_clk);

		break;
	}
	case JDI_IOCTL_GET_INSTANCE_POOL: {
		pr_debug("[JPUDRV+]JDI_IOCTL_GET_INSTANCE_POOL\n");

		ret = down_interruptible(&s_jpu_sem);
		if (ret != 0) {
			pr_err("error: %s get sem err\n", __func__);
			break;
		}

		/*return instance pool allocated*/
		if (drv_context->instance_pool.base != 0) {
			ret = copy_to_user((void __user *) arg, &drv_context->instance_pool, sizeof(jpudrv_buffer_t));
			up(&s_jpu_sem);
			break;
		}

		/* first time, alloc instance_pool */
		ret = copy_from_user(&drv_context->instance_pool, (jpudrv_buffer_t *) arg, sizeof(jpudrv_buffer_t));
		if (ret != 0) {
			up(&s_jpu_sem);
			pr_err("error: %s copy data err\n", __func__);
			break;
		}

		drv_context->instance_pool.size      = PAGE_ALIGN(drv_context->instance_pool.size);
		drv_context->instance_pool.base      = (unsigned long) vmalloc(drv_context->instance_pool.size);
		drv_context->instance_pool.phys_addr = drv_context->instance_pool.base;
		if (drv_context->instance_pool.base == 0) {
			up(&s_jpu_sem);
			ret = -ENOMEM;
			pr_err("error: %s instance vmalloc err\n", __func__);
			break;
		}

		memset((void *) drv_context->instance_pool.base, 0x0, drv_context->instance_pool.size);
		ret = copy_to_user((void __user *) arg, &drv_context->instance_pool, sizeof(jpudrv_buffer_t));
		if (ret != 0) {
			up(&s_jpu_sem);
			pr_err("error: %s copy data err\n", __func__);
			break;
		}

		up(&s_jpu_sem);
		pr_debug("[JPUDRV-]JDI_IOCTL_GET_INSTANCE_POOL instance pool base %#llx, size %d\n",
			drv_context->instance_pool.base,
			drv_context->instance_pool.size);
		break;
	}
	case JDI_IOCTL_OPEN_INSTANCE: {
		jpudrv_inst_info_t inst_info = {0};

		pr_debug("[JPUDRV+] JDI_IOCTL_OPEN_INSTANCE\n");
		if (copy_from_user(&inst_info, (jpudrv_inst_info_t *) arg, sizeof(jpudrv_inst_info_t)))
			return -EFAULT;

		spin_lock(&s_jpu_lock);
		drv_context->jpu_open_ref_count++; /* flag just for that jpu is in opened or closed */
		inst_info.inst_open_count = drv_context->jpu_open_ref_count;
		spin_unlock(&s_jpu_lock);

		if (copy_to_user((void __user *) arg, &inst_info, sizeof(jpudrv_inst_info_t)))
			return -EFAULT;

		pr_debug("[JPUDRV-] JDI_IOCTL_OPEN_INSTANCE inst_idx=%d, drv_context->jpu_open_ref_count=%d, inst_open_count=%d\n",
			(int) inst_info.inst_idx,
			drv_context->jpu_open_ref_count,
			inst_info.inst_open_count);

		break;
	}
	case JDI_IOCTL_CLOSE_INSTANCE: {
		jpudrv_inst_info_t inst_info = {0};

		pr_debug("[JPUDRV]JDI_IOCTL_CLOSE_INSTANCE\n");

		if (copy_from_user(&inst_info, (jpudrv_inst_info_t *) arg, sizeof(jpudrv_inst_info_t)))
			return -EFAULT;

		spin_lock(&s_jpu_lock);
		drv_context->jpu_open_ref_count--; /* flag just for that jpu is in opened or closed */
		inst_info.inst_open_count = drv_context->jpu_open_ref_count;
		spin_unlock(&s_jpu_lock);

		if (copy_to_user((void __user *) arg, &inst_info, sizeof(jpudrv_inst_info_t)))
			return -EFAULT;

		pr_debug("[JPUDRV-] JDI_IOCTL_CLOSE_INSTANCE inst_idx=%d, drv_context->jpu_open_ref_count=%d, inst_open_count=%d\n",
			(int) inst_info.inst_idx,
			drv_context->jpu_open_ref_count,
			inst_info.inst_open_count);

		break;
	}
	case JDI_IOCTL_GET_INSTANCE_NUM: {
		jpudrv_inst_info_t inst_info = {0};

		pr_debug("[JPUDRV+]JDI_IOCTL_GET_INSTANCE_NUM\n");

		ret = copy_from_user(&inst_info, (jpudrv_inst_info_t *) arg, sizeof(jpudrv_inst_info_t));
		if (ret != 0)
			break;

		spin_lock(&s_jpu_lock);
		inst_info.inst_open_count = drv_context->jpu_open_ref_count;
		spin_unlock(&s_jpu_lock);

		ret = copy_to_user((void __user *) arg, &inst_info, sizeof(jpudrv_inst_info_t));
		pr_debug("[JPUDRV-] JDI_IOCTL_GET_INSTANCE_NUM inst_idx=%d, open_count=%d\n",
			(int) inst_info.inst_idx,
			inst_info.inst_open_count);

		break;
	}
	case JDI_IOCTL_RESET: {
		jpu_hw_reset(drv_context->jpu_device);
		break;
	}
	case JDI_IOCTL_GET_REGISTER_INFO: {
		pr_debug("[JPUDRV+]JDI_IOCTL_GET_REGISTER_INFO\n");
		ret = copy_to_user((void __user *) arg, &drv_context->jpu_register, sizeof(jpudrv_buffer_t));
		if (ret != 0)
			ret = -EFAULT;

		pr_debug("[JPUDRV-]JDI_IOCTL_GET_REGISTER_INFO s_jpu_register.phys_addr=0x%llx, s_jpu_register.virt_addr=0x%llx, s_jpu_register.size=%d\n",
			drv_context->jpu_register.phys_addr, drv_context->jpu_register.virt_addr, drv_context->jpu_register.size);
		break;
	}
	default:
		pr_err("error: %s No such IOCTL, cmd is %d\n", __func__, cmd);
	}

	return ret;
}

static ssize_t jpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	return -1;
}

static ssize_t jpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	pr_debug(" %s write len=%d\n", __func__, (int) len);

	if (!buf) {
		pr_err("error: %s  buf = NULL error\n", __func__);
		return -EFAULT;
	}

	return -1;
}

static int jpu_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	u32 open_count = 0;
	struct jpu_drv_context_t *drv_context = NULL;

	pr_debug("[JPUDRV+] %s\n", __func__);

	drv_context  = (struct jpu_drv_context_t *)(filp->private_data);
	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return IRQ_HANDLED;
	}

	ret = down_interruptible(&s_jpu_sem);
	if (ret == 0) {
		/* find and free the not handled buffer by user applications */
		jpu_free_buffers(filp);
		/* find and free the not closed instance by user applications */
		jpu_free_instances(filp);
		spin_lock(&s_jpu_lock);
		drv_context->open_count--;
		open_count = drv_context->open_count;
		spin_unlock(&s_jpu_lock);
		pr_debug(" %s open_count: %d\n", __func__, drv_context->open_count);

		if (open_count == 0) {
			if (drv_context->instance_pool.base) {
				pr_debug(" %s free instance pool\n", __func__);
				vfree((const void *) drv_context->instance_pool.base);
				drv_context->instance_pool.base = 0;
			}
		}
		up(&s_jpu_sem);
	}

	pr_debug("[JPUDRV-]  %s\n", __func__);
	return 0;
}

static int jpu_fasync(int fd, struct file *filp, int mode)
{
	struct jpu_drv_context_t *drv_context = (struct jpu_drv_context_t *) filp->private_data;

	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return -ENOMEM;
	}

	return fasync_helper(fd, filp, mode, &drv_context->async_queue);
}

static int jpu_map_to_register(struct file *filp, struct vm_area_struct *vm)
{
	unsigned long pfn = 0;
	struct jpu_drv_context_t *drv_context = (struct jpu_drv_context_t *) filp->private_data;

	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return -ENOMEM;
	}

	vm->vm_flags |= VM_IO | VM_RESERVED;
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	pfn = drv_context->jpu_register.phys_addr >> PAGE_SHIFT;

	return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end - vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int jpu_map_to_physical_memory(struct file *filp, struct vm_area_struct *vm)
{
	bool found = false;
	struct jpudrv_buffer_pool_t *pool = NULL, *n = NULL;
	jpudrv_buffer_t jb = {0};
	struct jpu_drv_context_t *drv_context = (struct jpu_drv_context_t *) filp->private_data;

	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return -ENOMEM;
	}

	list_for_each_entry_safe(pool, n, &drv_context->jbp_head, list) {
		if (pool->filp == filp) {
			jb = pool->jb;
			if (jb.phys_addr >> PAGE_SHIFT == vm->vm_pgoff) { //find me
				found = true;
				break;
			}
		}
	}

	if (!found) {
		pr_err("error %s map dma fail jb.phys_add %p base %p\n", __func__, (void *) jb.base, (void *) jb.phys_addr);
		return -EAGAIN;
	}

	pr_debug("map dma success jb.phys_add %p base %p\n", (void *) jb.base, (void *) jb.phys_addr);
	vm->vm_pgoff = 0;
	return dma_mmap_coherent(drv_context->jpu_device, vm, (void *) jb.base, jb.phys_addr, vm->vm_end - vm->vm_start);
}

static int jpu_map_to_instance_pool_memory(struct file *filp, struct vm_area_struct *vm)
{
	int ret = 0;
	long length = vm->vm_end - vm->vm_start;
	unsigned long start = vm->vm_start;
	char *vmalloc_area_ptr = NULL;
	unsigned long pfn = 0;
	struct jpu_drv_context_t *drv_context = (struct jpu_drv_context_t *) filp->private_data;

	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return -ENOMEM;
	}

	vmalloc_area_ptr = (char *) drv_context->instance_pool.base;
	vm->vm_flags |= VM_RESERVED;

	/* loop over all pages, map it page individually */
	while (length > 0) {
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED);
		if (ret < 0) {
			pr_err("error: %s remap pfn range err\n", __func__);
			return ret;
		}

		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}

	return 0;
}

static int jpu_mmap(struct file *filp, struct vm_area_struct *vm)
{
	struct jpu_drv_context_t *drv_context = (struct jpu_drv_context_t *) filp->private_data;

	if (!drv_context) {
		pr_err("error: %s get drv context error\n", __func__);
		return -ENOMEM;
	}

	if (vm->vm_pgoff == 0)
		return jpu_map_to_instance_pool_memory(filp, vm);

	if (vm->vm_pgoff == (drv_context->jpu_register.phys_addr >> PAGE_SHIFT))
		return jpu_map_to_register(filp, vm);

	return jpu_map_to_physical_memory(filp, vm);
}

static const struct  file_operations jpu_fops = {
	.owner = THIS_MODULE,
	.open = jpu_open,
	.read = jpu_read,
	.write = jpu_write,
	.unlocked_ioctl = jpu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = jpu_ioctl,
#endif
	.release = jpu_release,
	.fasync = jpu_fasync,
	.mmap = jpu_mmap,
};


/*
 *set_codaj12_device
 *param: dev
 *param: attr
 *param: buf
 *note: set config info
 */
static ssize_t set_codaj12_device(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	return len;
}

/*
 *show_codaj12_device show coda device info
 *param: dev
 *param: attr
 *param: buf
 *note:  cat /sys/device/platform/soc/31420000.codaj12/coda_debug
 */
static ssize_t show_codaj12_device(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (s_jpu_drv_context) {
		return sprintf(buf, "jpu_register.base: %llu\njpu_register.phys_addr:%llu\njpu_open_ref_count: %d\nclk_reference: %d\n",
					s_jpu_drv_context->jpu_register.base,
					s_jpu_drv_context->jpu_register.phys_addr,
					s_jpu_drv_context->jpu_open_ref_count,
					s_jpu_drv_context->jpu_clk.clk_reference);
	}

	return 0;
}

static DEVICE_ATTR(codaj12_debug, 0664, show_codaj12_device, set_codaj12_device);

static void jpu_class_destrory(struct jpu_drv_context_t *drv_context, struct platform_device *pdev)
{
	if (drv_context->jpu_class && drv_context->jpu_class_device)
		device_destroy(drv_context->jpu_class, drv_context->jpu_major);

	if (drv_context->jpu_class)
		class_destroy(drv_context->jpu_class);

	device_remove_file(&pdev->dev, &dev_attr_codaj12_debug);
}

static int jpu_class_create(struct jpu_drv_context_t *drv_context, struct platform_device *pdev)
{
	drv_context->jpu_class = class_create(THIS_MODULE, "jpu_coda");
	if (!drv_context->jpu_class) {
		pr_err("error: %s could not allocate class\n", __func__);
		return -EBUSY;
	}

	drv_context->jpu_class_device = device_create(drv_context->jpu_class, NULL, drv_context->jpu_major, NULL, JPU_DEV_NAME);
	if (!drv_context->jpu_class_device) {
		pr_err("error: %s could not allocate device\n", __func__);
		return -EBUSY;
		goto CLASS_CREATE_FAIL;
	}

	if (device_create_file(&(pdev->dev), &dev_attr_codaj12_debug)) {
		pr_err("error: %s device_create_file fail\n", __func__);
		goto CLASS_CREATE_FAIL;
	}

	return 0;

CLASS_CREATE_FAIL:

	if (drv_context->jpu_class && drv_context->jpu_class_device)
		device_destroy(drv_context->jpu_class, drv_context->jpu_major);

	if (drv_context->jpu_class)
		class_destroy(drv_context->jpu_class);

	return -EBUSY;
}

static int jpu_probe(struct platform_device *pdev)
{
	int err = 0;
	int i = 0;
	struct resource *res = NULL;

	pr_info("[JPUDRV+] %s\n", __func__);
	s_jpu_drv_context = devm_kzalloc(&pdev->dev, sizeof(struct jpu_drv_context_t), GFP_KERNEL);
	if (!s_jpu_drv_context)
		return -ENOMEM;

	s_jpu_drv_context->jpu_device = &(pdev->dev);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("error: %s get memory resource failed.", __func__);
		return -ENODEV;
	}

	jpu_hw_reset(s_jpu_drv_context->jpu_device);
	if (!devm_request_mem_region(&pdev->dev, res->start, resource_size(res), pdev->name)) {
		pr_err("error: %s mem region error\n", __func__);
		return -EBUSY;
	}

	s_jpu_drv_context->jpu_register.virt_addr = (uint64_t) devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
	if (!s_jpu_drv_context->jpu_register.virt_addr) {
		pr_err("error: %s register mem mmap error\n", __func__);
		return -EBUSY;
	}

	s_jpu_drv_context->jpu_register.phys_addr = res->start;
	s_jpu_drv_context->jpu_register.size = resource_size(res);
	dma_set_coherent_mask(s_jpu_drv_context->jpu_device, DMA_BIT_MASK(32));

	/* initialize the waitqueue list */
	for (i = 0; i < MAX_NUM_INSTANCE; i++)
		init_waitqueue_head(&s_jpu_drv_context->interrupt_wait_q[i]);

	INIT_LIST_HEAD(&s_jpu_drv_context->inst_list_head);
	INIT_LIST_HEAD(&s_jpu_drv_context->jbp_head);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("error: %s ioresource_irq failed\n", __func__);
		return -EFAULT;
	}

	err = devm_request_irq(&pdev->dev, res->start, jpu_irq_handler, 0, "JPU_IRQ", (void *)(s_jpu_drv_context));
	if (err) {
		pr_err("error: %s JPU IRQ request failed\n", __func__);
		return -EFAULT;
	}

	s_jpu_drv_context->jpu_irq = res->start;
	/* get the major number of the character device */
	if ((alloc_chrdev_region(&(s_jpu_drv_context->jpu_major), 0, 1, JPU_DEV_NAME)) < 0) {
		err = -EBUSY;
		pr_err("error:%s could not allocate major number\n", __func__);
		return -EFAULT;
	}

	/* initialize the device structure and register the device with the kernel */
	cdev_init(&(s_jpu_drv_context->jpu_cdev), &jpu_fops);
	if ((cdev_add(&(s_jpu_drv_context->jpu_cdev), s_jpu_drv_context->jpu_major, 1)) < 0) {
		err = -EBUSY;
		pr_err("error: %s could not allocate chrdev\n", __func__);
		goto ERROR_PROVE_DEVICE;
	}

	err = jpu_class_create(s_jpu_drv_context, pdev);
	if (err != 0) {
		pr_err("error: %s class create fail\n", __func__);
		goto ERROR_PROVE_DEVICE;
	}

	jpu_clk_get(&pdev->dev, s_jpu_drv_context);
	platform_set_drvdata(pdev, s_jpu_drv_context);

	pr_info("[JPUDRV-] %s jpu base address get from platform driver physical base addr==0x%llx, virtual base=0x%llx,irq_num=%d, jpu_device %p\n",
		__func__,
		s_jpu_drv_context->jpu_register.phys_addr,
		s_jpu_drv_context->jpu_register.virt_addr,
		s_jpu_drv_context->jpu_irq,
		s_jpu_drv_context->jpu_device);

	return 0;

ERROR_PROVE_DEVICE:

	if (s_jpu_drv_context->jpu_major)
		unregister_chrdev_region(s_jpu_drv_context->jpu_major, 1);

	return err;
}

static int jpu_remove(struct platform_device *pdev)
{
	struct jpu_drv_context_t *drv_context = NULL;

	pr_info("[JPUDRV+] %s\n", __func__);
	drv_context = (struct jpu_drv_context_t *) platform_get_drvdata(pdev);
	if (!drv_context) {
		pr_err("error: %s get context error\n", __func__);
		return -ENOMEM;
	}

	if (drv_context->jpu_clk.core_aclk && drv_context->jpu_clk.core_cclk)
		jpu_clk_put(&drv_context->jpu_clk);

	if (drv_context->instance_pool.base) {
		vfree((const void *) drv_context->instance_pool.base);
		drv_context->instance_pool.base = 0;
	}

	if (drv_context->jpu_major > 0) {
		cdev_del(&(drv_context->jpu_cdev));
		unregister_chrdev_region(drv_context->jpu_major, 1);
		drv_context->jpu_major = 0;
	}

	jpu_class_destrory(drv_context, pdev);
	memset(drv_context, 0, sizeof(struct jpu_drv_context_t));
	devm_kfree(&pdev->dev, drv_context);
	drv_context = NULL;
	pr_info("[JPUDRV-] %s\n", __func__);
	return 0;
}

static void jpu_shutdown(struct platform_device *pdev)
{
	struct jpu_drv_context_t *drv_context = NULL;

	pr_info("[JPUDRV+] %s\n", __func__);
	drv_context = (struct jpu_drv_context_t *) platform_get_drvdata(pdev);
	if (!drv_context) {
		pr_err("err: %s get context error\n", __func__);
		return;
	}

	if (drv_context->jpu_clk.core_aclk && drv_context->jpu_clk.core_cclk)
		jpu_clk_put(&drv_context->jpu_clk);

	if (drv_context->instance_pool.base) {
		vfree((const void *) drv_context->instance_pool.base);
		drv_context->instance_pool.base = 0;
	}

	if (drv_context->jpu_major > 0) {
		cdev_del(&(drv_context->jpu_cdev));
		unregister_chrdev_region(drv_context->jpu_major, 1);
		drv_context->jpu_major = 0;
	}

	jpu_class_destrory(drv_context, pdev);
	memset(drv_context, 0, sizeof(struct jpu_drv_context_t));
	devm_kfree(&pdev->dev, drv_context);
	drv_context = NULL;
	pr_info("[JPUDRV-] %s\n", __func__);
}

#ifdef CONFIG_PM
static int jpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct jpu_drv_context_t *drv_context =  platform_get_drvdata(pdev);

	pr_debug("[JPUDRV+] %s\n", __func__);
	if (!drv_context) {
		pr_err("error: %s get context error\n", __func__);
		return -ENOMEM;
	}

	jpu_clk_disable(&drv_context->jpu_clk);
	pr_debug("[JPUDRV-] %s\n", __func__);
	return 0;
}

static int jpu_resume(struct platform_device *pdev)
{
	struct jpu_drv_context_t *drv_context =  platform_get_drvdata(pdev);

	pr_debug("[JPUDRV+] %s\n", __func__);
	if (!drv_context) {
		pr_err("error: %s get context error\n", __func__);
		return -ENOMEM;
	}

	jpu_clk_enable(&drv_context->jpu_clk);
	pr_debug("[JPUDRV-] %s\n", __func__);
	return 0;
}
#else
#define    jpu_suspend    NULL
#define    jpu_resume     NULL
#endif /* !CONFIG_PM */

static const struct of_device_id jpucoda_of_table[] = {
	{.compatible = "semidrive,codaj12",},
	{},
};
MODULE_DEVICE_TABLE(of, jpucoda_of_table);

static struct platform_driver jpu_driver = {
	.driver = {
		.name = JPU_PLATFORM_DEVICE_NAME,
		.of_match_table = jpucoda_of_table,
	},
	.probe      = jpu_probe,
	.remove     = jpu_remove,
	.suspend    = jpu_suspend,
	.resume     = jpu_resume,
	.shutdown   = jpu_shutdown,
};

static int __init jpu_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&jpu_driver);
}

static void __exit jpu_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&jpu_driver);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A customer using C&M JPU, Inc.");
MODULE_DESCRIPTION("JPU linux driver");

module_init(jpu_init);
module_exit(jpu_exit);

