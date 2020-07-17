//--=========================================================================--
//  This file is linux device driver for VPU.
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2015  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//--=========================================================================-

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

#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/semidrive/sdrv_scr.h>

#include "wave_config.h"
#include "vpu.h"

/* if you want to have clock gating scheme frame by frame */
#define VPU_SUPPORT_CLOCK_CONTROL

/* if the driver want to use interrupt service from kernel ISR */
#define VPU_SUPPORT_ISR

#define VPU_SUPPORT_PLATFORM_DRIVER_REGISTER

/* x9 plat no using this macro */
//#define VPU_SUPPORT_RESERVED_VIDEO_MEMORY

#define VPU_PLATFORM_DEVICE_NAME "vpu-wave412"
#define VPU_CLK_NAME "vpuwaveclock"
#define VPU_DEV_NAME "vpuwave"
#define RSTGEN_VPU_WAVE_BASE_ADDR    0x34440000


/* x9 plat ignored following */
#define VPU_REG_BASE_ADDR 0x306200000
#define VPU_REG_SIZE (0x4000*MAX_NUM_VPU_CORE)

#ifdef VPU_SUPPORT_ISR
#define VPU_IRQ_NUM (51)
#endif

/* x9 plat ignored */
#ifndef VM_RESERVED /*for kernel up to 3.7.0 version*/
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

struct device *vpuwave_device = NULL;

typedef struct vpu_drv_context_t {
    struct fasync_struct *async_queue;
    unsigned long interrupt_reason;
    u32 open_count;                  /*!<< device reference count. Not instance count */
} vpu_drv_context_t;

/* To track the allocated memory buffer */
typedef struct vpudrv_buffer_pool_t {
    struct list_head list;
    struct vpudrv_buffer_t vb;
    struct file *filp;
} vpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct vpudrv_instanace_list_t {
    struct list_head list;
    unsigned long inst_idx;
    unsigned long core_idx;
    struct file *filp;
} vpudrv_instanace_list_t;

typedef struct vpudrv_instance_pool_t {
    unsigned char codecInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
} vpudrv_instance_pool_t;

struct vpuwave_clk {
    struct clk *bcore_clk;
    struct clk *core_clk;
};

/*
 *sram info: inter sram; soc sram
 * id  1 inter; 2 soc sram
 * phy address
 * size mem size
 */
struct sram_info {
    uint32_t id;
    uint32_t phy;
    uint32_t size;
};

struct wave_drive {
    struct class  *wave_class;
    struct device *wave_device;
    /**/
    struct dentry *debug_root;
    /* sram[0] inter sram[1] soc sram */
    struct sram_info  sram[2];
    uint32_t scr_signal;
};

static struct wave_drive drive_data;

/* x9 plat ignored following */
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
#   define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (62*1024*1024)
#   define VPU_DRAM_PHYSICAL_BASE 0x86C00000
#include "vmm.h"
static video_mm_t s_vmem;
static vpudrv_buffer_t s_video_memory = {0};
#endif /*VPU_SUPPORT_RESERVED_VIDEO_MEMORY*/

static int vpu_hw_reset(void);
static void vpu_clk_disable(struct vpuwave_clk *clk);
static int vpu_clk_enable(struct vpuwave_clk *clk);
static struct vpuwave_clk  *vpu_clk_get(struct device *dev);
static void vpu_clk_put(struct vpuwave_clk *clk);

static vpudrv_buffer_t s_instance_pool = {0};
static vpudrv_buffer_t s_common_memory = {0};
static vpu_drv_context_t s_vpu_drv_context;
static int s_vpu_major;
static struct cdev s_vpu_cdev;

//static struct clk *s_vpu_clk;
static struct vpuwave_clk  *s_vpu_clk = NULL;
static int clk_reference = 0;

static int s_vpu_open_ref_count;

#ifdef VPU_SUPPORT_ISR
static int s_vpu_irq = VPU_IRQ_NUM;
#endif

static vpudrv_buffer_t s_vpu_register = {0};

static int s_interrupt_flag;
static wait_queue_head_t s_interrupt_wait_q;

static spinlock_t s_vpu_lock = __SPIN_LOCK_UNLOCKED(s_vpu_lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static DECLARE_MUTEX(s_vpu_sem);
#else
static DEFINE_SEMAPHORE(s_vpu_sem);
#endif
static struct list_head s_vbp_head = LIST_HEAD_INIT(s_vbp_head);
static struct list_head s_inst_list_head = LIST_HEAD_INIT(
            s_inst_list_head);


static vpu_bit_firmware_info_t s_bit_firmware_info[MAX_NUM_VPU_CORE];

#ifdef CONFIG_PM
/* implement to power management functions */
#define BIT_BASE                    0x0000
#define BIT_CODE_RUN                (BIT_BASE + 0x000)
#define BIT_CODE_DOWN               (BIT_BASE + 0x004)
#define BIT_INT_CLEAR               (BIT_BASE + 0x00C)
#define BIT_INT_STS                 (BIT_BASE + 0x010)
#define BIT_CODE_RESET              (BIT_BASE + 0x014)
#define BIT_INT_REASON              (BIT_BASE + 0x174)
#define BIT_BUSY_FLAG               (BIT_BASE + 0x160)
#define BIT_RUN_COMMAND             (BIT_BASE + 0x164)
#define BIT_RUN_INDEX               (BIT_BASE + 0x168)
#define BIT_RUN_COD_STD             (BIT_BASE + 0x16C)

/* WAVE4 registers */
#define W4_REG_BASE                 0x0000
#define W4_VPU_BUSY_STATUS          (W4_REG_BASE + 0x0070)
#define W4_VPU_INT_REASON_CLEAR     (W4_REG_BASE + 0x0034)
#define W4_VPU_VINT_CLEAR           (W4_REG_BASE + 0x003C)
#define W4_VPU_VPU_INT_STS          (W4_REG_BASE + 0x0044)
#define W4_VPU_INT_REASON           (W4_REG_BASE + 0x004c)

#define W4_RET_SUCCESS              (W4_REG_BASE + 0x0110)
#define W4_RET_FAIL_REASON          (W4_REG_BASE + 0x0114)

/* WAVE4 INIT, WAKEUP */
#define W4_PO_CONF                  (W4_REG_BASE + 0x0000)
#define W4_VCPU_CUR_PC              (W4_REG_BASE + 0x0004)

#define W4_VPU_VINT_ENABLE          (W4_REG_BASE + 0x0048)

#define W4_VPU_RESET_REQ            (W4_REG_BASE + 0x0050)
#define W4_VPU_RESET_STATUS         (W4_REG_BASE + 0x0054)

#define W4_VPU_REMAP_CTRL           (W4_REG_BASE + 0x0060)
#define W4_VPU_REMAP_VADDR          (W4_REG_BASE + 0x0064)
#define W4_VPU_REMAP_PADDR          (W4_REG_BASE + 0x0068)
#define W4_VPU_REMAP_CORE_START     (W4_REG_BASE + 0x006C)
#define W4_VPU_BUSY_STATUS          (W4_REG_BASE + 0x0070)

#define W4_REMAP_CODE_INDEX          0
enum {
    W4_INT_INIT_VPU        = 0,
    W4_INT_DEC_PIC_HDR     = 1,
    W4_INT_FINI_SEQ        = 2,
    W4_INT_DEC_PIC         = 3,
    W4_INT_SET_FRAMEBUF    = 4,
    W4_INT_FLUSH_DEC       = 5,
    W4_INT_GET_FW_VERSION  = 8,
    W4_INT_QUERY_DEC       = 9,
    W4_INT_SLEEP_VPU       = 10,
    W4_INT_WAKEUP_VPU      = 11,
    W4_INT_CHANGE_INT      = 12,
    W4_INT_CREATE_INSTANCE = 14,
    W4_INT_BSBUF_EMPTY     = 15,   /*!<< Bitstream buffer empty */
    W4_INT_ENC_SLICE_INT   = 15,
};

#define W4_HW_OPTION                    (W4_REG_BASE + 0x0124)
#define W4_CODE_SIZE                    (W4_REG_BASE + 0x011C)
/* Note: W4_INIT_CODE_BASE_ADDR should be aligned to 4KB */
#define W4_ADDR_CODE_BASE              (W4_REG_BASE + 0x0118)
#define W4_CODE_PARAM                  (W4_REG_BASE + 0x0120)
#define W4_INIT_VPU_TIME_OUT_CNT        (W4_REG_BASE + 0x0134)

/* WAVE4 Wave4BitIssueCommand */
#define W4_CORE_INDEX           (W4_REG_BASE + 0x0104)
#define W4_INST_INDEX           (W4_REG_BASE + 0x0108)
#define W4_COMMAND             (W4_REG_BASE + 0x0100)
#define W4_VPU_HOST_INT_REQ   (W4_REG_BASE + 0x0038)

/* Product register */
#define VPU_PRODUCT_CODE_REGISTER   (BIT_BASE + 0x1044)

//static u32  s_vpu_reg_store[MAX_NUM_VPU_CORE][64];
#endif

#define ReadVpuRegister(addr)       *(volatile unsigned int *)(s_vpu_register.virt_addr + s_bit_firmware_info[core].reg_base_offset + addr)
#define WriteVpuRegister(addr, val) *(volatile unsigned int *)(s_vpu_register.virt_addr + s_bit_firmware_info[core].reg_base_offset + addr) = (unsigned int)val
#define WriteVpu(addr, val)         *(volatile unsigned int *)(addr) = (unsigned int)val;


//#define COMMON_MEMORY_USING_ION
/**
 *Brief allocate common memory that only using vpu
 *param vb
 *return 0 success -1 failure
 *note   function called only one time
 */
static int vpu_alloc_common_buffer(vpudrv_buffer_t *vb)
{

#ifndef  COMMON_MEMORY_USING_ION

    if (!vb)
        return -1;

    vb->base = (unsigned long) dma_alloc_coherent(vpuwave_device,
               PAGE_ALIGN(vb->size), (dma_addr_t *)(&vb->dma_addr),
               GFP_DMA | GFP_KERNEL | GFP_USER);
    vb->phys_addr = vb->dma_addr;

    if ((void *)(vb->base) == NULL)  {
        pr_err("[VPUDRV-ERR] Physical memory allocation error size=%d\n",
                vb->size);
        return -1;
    }

#else /* using ion allocate common memory */

    vb->phys_addr = dma_to_phys(vpuwave_device, vb->dma_addr);
    vb->base = phys_to_virt(vb->phys_addr);

#endif

    pr_info("[VPUDRV] common buffer allocate info :vpuwave_device %p, handle %d , virt %p,  phy %p, dma addr %p, size %d\n",
            vpuwave_device,
            vb->buf_handle,
            (void *)vb->base,
            (void *)vb->phys_addr,
            (void *)vb->dma_addr,
            vb->size);

    return 0;
}


static int vpu_alloc_dma_buffer(vpudrv_buffer_t *vb)
{
    if (!vb)
        return -1;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    vb->phys_addr = (unsigned long)vmem_alloc(&s_vmem, vb->size, 0);

    if ((unsigned long)vb->phys_addr  == (unsigned long) -1) {
        pr_err( "[VPUDRV-ERR] Physical memory allocation error size=%d\n",
                 vb->size);
        return -1;
    }

    vb->base = (unsigned long)(s_video_memory.base + (vb->phys_addr -
                               s_video_memory.phys_addr));
#else
    vb->base = (unsigned long)dma_alloc_coherent(vpuwave_device,
               PAGE_ALIGN(vb->size),
               (dma_addr_t *) (&vb->phys_addr), GFP_DMA | GFP_KERNEL);

    if ((void *)(vb->base) == NULL) {
        pr_err( "[VPUDRV-ERR] Physical memory allocation error size=%d\n",
                 vb->size);
        return -1;
    }

#endif
    return 0;
}

static void vpu_free_dma_buffer(vpudrv_buffer_t *vb)
{
    if (!vb)
        return;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY

    if (vb->base)
        vmem_free(&s_vmem, vb->phys_addr, 0);

#else

    if (vb->base)
        dma_free_coherent(vpuwave_device, PAGE_ALIGN(vb->size),
                          (void *)vb->base,
                          vb->phys_addr);

        pr_info("[VPUDRV] vpu_free dma buffer wave device %p \n", vpuwave_device);

#endif
}

static int vpu_free_instances(struct file *filp)
{
    vpudrv_instanace_list_t *vil, *n;
    vpudrv_instance_pool_t *vip;
    void *vip_base;
    int instance_pool_size_per_core;
    void *vdi_mutexes_base;
    const int PTHREAD_MUTEX_T_DESTROY_VALUE = 0xdead10cc;

    pr_info("[VPUDRV] vpu_free_instances\n");

    instance_pool_size_per_core = (s_instance_pool.size /
                                   MAX_NUM_VPU_CORE); /* s_instance_pool.size  assigned to the size of all core once call VDI_IOCTL_GET_INSTANCE_POOL by user. */

    list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
        if (vil->filp == filp) {

            /* every codec(coda988/wave412) uing independent driver */
            //vip_base = (void *)(s_instance_pool.base + (instance_pool_size_per_core * vil->core_idx));
            vip_base = (void *)(s_instance_pool.base);
            pr_info("[VPUDRV] vpu_free_instances detect instance crash instIdx=%d, coreIdx=%d, vip_base=%p, instance_pool_size_per_core=%d\n",
                    (int)vil->inst_idx, (int)vil->core_idx, vip_base,
                    (int)instance_pool_size_per_core);
            vip = (vpudrv_instance_pool_t *)vip_base;

            if (vip) {
                memset(&vip->codecInstPool[vil->inst_idx], 0x00,
                       4);    /* only first 4 byte is key point(inUse of CodecInst in vpuapi) to free the corresponding instance. */
#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
                vdi_mutexes_base = (vip_base + (instance_pool_size_per_core -
                                                PTHREAD_MUTEX_T_HANDLE_SIZE * 4));
                pr_info("[VPUDRV] vpu_free_instances : force to destroy vdi_mutexes_base=%p in userspace \n",
                        vdi_mutexes_base);

                if (vdi_mutexes_base) {
                    int i;

                    for (i = 0; i < 4; i++) {
                        memcpy(vdi_mutexes_base, &PTHREAD_MUTEX_T_DESTROY_VALUE,
                               PTHREAD_MUTEX_T_HANDLE_SIZE);
                        vdi_mutexes_base += PTHREAD_MUTEX_T_HANDLE_SIZE;
                    }
                }
            }

            s_vpu_open_ref_count--;
            list_del(&vil->list);
            kfree(vil);
        }
    }
    return 1;
}

static int vpu_free_buffers(struct file *filp)
{
    vpudrv_buffer_pool_t *pool, *n;
    vpudrv_buffer_t vb;

    pr_info("[VPUDRV] vpu_free_buffers\n");

    list_for_each_entry_safe(pool, n, &s_vbp_head, list) {
        if (pool->filp == filp) {
            vb = pool->vb;

            if (vb.base) {
                vpu_free_dma_buffer(&vb);
                list_del(&pool->list);
                kfree(pool);
            }
        }
    }

    return 0;
}

static irqreturn_t vpu_irq_handler(int irq, void *dev_id)
{
    vpu_drv_context_t *dev = (vpu_drv_context_t *)dev_id;

    /* this can be removed. it also work in VPU_WaitInterrupt of API function */
    int core;
    int product_code;

    for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
        if (s_bit_firmware_info[core].size ==
                0) {/* it means that we didn't get an information the current core from API layer. No core activated.*/
            pr_info("[VPUDRV] :  s_bit_firmware_info[core].size is zero\n");
            continue;
        }

        product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);

        if (PRODUCT_CODE_WAVE412(product_code)) {
            if (ReadVpuRegister(W4_VPU_VPU_INT_STS)) {
                dev->interrupt_reason = ReadVpuRegister(W4_VPU_INT_REASON);
                WriteVpuRegister(W4_VPU_INT_REASON_CLEAR, dev->interrupt_reason);
                WriteVpuRegister(W4_VPU_VINT_CLEAR, 0x1);
            }
        }
        else {
            pr_err("[VPUDRV-ERR] Unknown product id : %08x\n", product_code);
            continue;
        }

    }

    if (dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO,
                    POLL_IN); /* notify the interrupt to user space */

    s_interrupt_flag = 1;
    wake_up_interruptible(&s_interrupt_wait_q);

    return IRQ_HANDLED;
}

static int vpu_open(struct inode *inode, struct file *filp)
{
    pr_info("[VPUDRV] wave driver %s\n", __func__);
    spin_lock(&s_vpu_lock);

    s_vpu_drv_context.open_count++;

    filp->private_data = (void *)(&s_vpu_drv_context);
    spin_unlock(&s_vpu_lock);

    return 0;
}

/*static int vpu_ioctl(struct inode *inode, struct file *filp, u_int cmd, u_long arg) // for kernel 2.6.9 of C&M*/
static long vpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
    int ret = 0;
    struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)
                                    filp->private_data;

    switch (cmd) {
        case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY: {
            vpudrv_buffer_pool_t *vbp;

            pr_info("[VPUDRV] VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");

            if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
                vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);

                if (!vbp) {
                    up(&s_vpu_sem);
                    return -ENOMEM;
                }

                ret = copy_from_user(&(vbp->vb), (vpudrv_buffer_t *)arg,
                                     sizeof(vpudrv_buffer_t));

                if (ret) {
                    kfree(vbp);
                    up(&s_vpu_sem);
                    return -EFAULT;
                }

                ret = vpu_alloc_dma_buffer(&(vbp->vb));

                if (ret == -1) {
                    ret = -ENOMEM;
                    kfree(vbp);
                    up(&s_vpu_sem);
                    break;
                }

                ret = copy_to_user((void __user *)arg, &(vbp->vb),
                                   sizeof(vpudrv_buffer_t));

                if (ret) {
                    kfree(vbp);
                    ret = -EFAULT;
                    up(&s_vpu_sem);
                    break;
                }

                vbp->filp = filp;
                spin_lock(&s_vpu_lock);
                list_add(&vbp->list, &s_vbp_head);
                spin_unlock(&s_vpu_lock);

                up(&s_vpu_sem);
            }

        }
        break;

        case VDI_IOCTL_FREE_PHYSICALMEMORY: {
            vpudrv_buffer_pool_t *vbp, *n;
            vpudrv_buffer_t vb;
            pr_info("[VPUDRV] VDI_IOCTL_FREE_PHYSICALMEMORY\n");

            if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
                ret = copy_from_user(&vb, (vpudrv_buffer_t *)arg,
                                     sizeof(vpudrv_buffer_t));

                if (ret) {
                    up(&s_vpu_sem);
                    return -EACCES;
                }

                if (vb.base)
                    vpu_free_dma_buffer(&vb);

                spin_lock(&s_vpu_lock);
                list_for_each_entry_safe(vbp, n, &s_vbp_head, list) {
                    if (vbp->vb.base == vb.base) {
                        list_del(&vbp->list);
                        kfree(vbp);
                        break;
                    }
                }
                spin_unlock(&s_vpu_lock);

                up(&s_vpu_sem);
            }

        }
        break;

        case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO: {
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
            pr_info("[VPUDRV][+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");

            if (s_video_memory.base != 0) {
                ret = copy_to_user((void __user *)arg, &s_video_memory,
                                   sizeof(vpudrv_buffer_t));

                if (ret != 0)
                    ret = -EFAULT;
            }
            else {
                ret = -EFAULT;
            }

            pr_info("[VPUDRV][-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
#endif
        }
        break;

        case VDI_IOCTL_WAIT_INTERRUPT: {
            vpudrv_intr_info_t info;
            ret = copy_from_user(&info, (vpudrv_intr_info_t *)arg,
                                 sizeof(vpudrv_intr_info_t));

            if (ret != 0)
                return -EFAULT;

            ret = wait_event_interruptible_timeout(s_interrupt_wait_q,
                                                   s_interrupt_flag != 0,
                                                   msecs_to_jiffies(info.timeout));

            if (!ret) {
                ret = -ETIME;
                break;
            }

            if (signal_pending(current)) {
                ret = -ERESTARTSYS;
                pr_err("[VPUDRV-ERR]  Signal is received now \n");
                break;
            }

            info.intr_reason = dev->interrupt_reason;
            s_interrupt_flag = 0;
            dev->interrupt_reason = 0;
            ret = copy_to_user((void __user *)arg, &info,
                               sizeof(vpudrv_intr_info_t));

            if (ret != 0)
                return -EFAULT;
        }

        break;

        case VDI_IOCTL_SET_CLOCK_GATE: {

#ifdef VPU_SUPPORT_CLOCK_CONTROL
            u32 clkgate;

            if (get_user(clkgate, (u32 __user *) arg))
                return -EFAULT;

            if (clkgate)
                vpu_clk_enable(s_vpu_clk);
            else
                vpu_clk_disable(s_vpu_clk);

#endif
        }
        break;

        case VDI_IOCTL_GET_INSTANCE_POOL: {
            pr_info("[VPUDRV] VDI_IOCTL_GET_INSTANCE_POOL\n");

            if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
                if (s_instance_pool.base != 0) {
                    ret = copy_to_user((void __user *)arg, &s_instance_pool,
                                       sizeof(vpudrv_buffer_t));

                    if (ret != 0)
                        ret = -EFAULT;
                }
                else {
                    ret = copy_from_user(&s_instance_pool, (vpudrv_buffer_t *)arg,
                                         sizeof(vpudrv_buffer_t));

                    if (ret == 0) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
                        s_instance_pool.size = PAGE_ALIGN(s_instance_pool.size);
                        s_instance_pool.base = (unsigned long)vmalloc(s_instance_pool.size);
                        /* pool phys_addr do nothing */
                        s_instance_pool.phys_addr = s_instance_pool.base;

                        if (s_instance_pool.base != 0)
#else

                        if (vpu_alloc_dma_buffer(&s_instance_pool) != -1)
#endif
                        {
                            memset((void *)s_instance_pool.base, 0x0,
                                   s_instance_pool.size); /*clearing memory*/
                            ret = copy_to_user((void __user *)arg, &s_instance_pool,
                                               sizeof(vpudrv_buffer_t));

                            if (ret == 0) {
                                /* success to get memory for instance pool */
                                up(&s_vpu_sem);
                                break;
                            }
                        }

                    }
                    ret = -EFAULT;
                }
                up(&s_vpu_sem);
            }

        }
        break;

        case VDI_IOCTL_GET_COMMON_MEMORY: {
            pr_info("[VPUDRV] VDI_IOCTL_GET_COMMON_MEMORY\n");

            if (s_common_memory.base != 0) {
                ret = copy_to_user((void __user *)arg, &s_common_memory,
                                   sizeof(vpudrv_buffer_t));

                if (ret != 0)
                    ret = -EFAULT;
            }
            else {
                ret = copy_from_user(&s_common_memory, (vpudrv_buffer_t *)arg,
                                     sizeof(vpudrv_buffer_t));

                if (ret == 0) {
                    if (vpu_alloc_common_buffer(&s_common_memory) != -1) {
                        ret = copy_to_user((void __user *) arg, &s_common_memory,
                                           sizeof(vpudrv_buffer_t));

                        if (ret == 0) {
                            /* success to get memory for common memory */
                            break;
                        }
                    }
                }

                ret = -EFAULT;
            }

        }
        break;

        case VDI_IOCTL_OPEN_INSTANCE: {
            vpudrv_inst_info_t inst_info;
            vpudrv_instanace_list_t *vil, *n;

            vil = kzalloc(sizeof(*vil), GFP_KERNEL);

            if (!vil)
                return -ENOMEM;

            if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
                               sizeof(vpudrv_inst_info_t)))
                return -EFAULT;

            vil->inst_idx = inst_info.inst_idx;
            vil->core_idx = inst_info.core_idx;
            vil->filp = filp;

            spin_lock(&s_vpu_lock);
            list_add(&vil->list, &s_inst_list_head);

            inst_info.inst_open_count = 0; /* counting the current open instance number */
            list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
                if (vil->core_idx == inst_info.core_idx)
                    inst_info.inst_open_count++;
            }
            spin_unlock(&s_vpu_lock);

            s_vpu_open_ref_count++; /* flag just for that vpu is in opened or closed */

            if (copy_to_user((void __user *)arg, &inst_info,
                             sizeof(vpudrv_inst_info_t))) {
                kfree(vil);
                return -EFAULT;
            }

            pr_info("[VPUDRV] VDI_IOCTL_OPEN_INSTANCE core_idx=%d, inst_idx=%d, s_vpu_open_ref_count=%d, inst_open_count=%d\n",
                    (int)inst_info.core_idx, (int)inst_info.inst_idx,
                    s_vpu_open_ref_count,
                    inst_info.inst_open_count);
        }
        break;

        case VDI_IOCTL_CLOSE_INSTANCE: {
            vpudrv_inst_info_t inst_info;
            vpudrv_instanace_list_t *vil, *n;

            if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
                               sizeof(vpudrv_inst_info_t)))
                return -EFAULT;

            spin_lock(&s_vpu_lock);
            list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
                if (vil->inst_idx == inst_info.inst_idx
                        && vil->core_idx == inst_info.core_idx) {
                    list_del(&vil->list);
                    kfree(vil);
                    break;
                }
            }

            inst_info.inst_open_count = 0; /* counting the current open instance number */
            list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
                if (vil->core_idx == inst_info.core_idx)
                    inst_info.inst_open_count++;
            }
            spin_unlock(&s_vpu_lock);

            s_vpu_open_ref_count--; /* flag just for that vpu is in opened or closed */

            if (copy_to_user((void __user *)arg, &inst_info,
                             sizeof(vpudrv_inst_info_t)))
                return -EFAULT;

            pr_info("[VPUDRV] VDI_IOCTL_CLOSE_INSTANCE core_idx=%d, inst_idx=%d, s_vpu_open_ref_count=%d, inst_open_count=%d\n",
                    (int)inst_info.core_idx, (int)inst_info.inst_idx,
                    s_vpu_open_ref_count,
                    inst_info.inst_open_count);
        }
        break;

        case VDI_IOCTL_GET_INSTANCE_NUM: {
            vpudrv_inst_info_t inst_info;
            vpudrv_instanace_list_t *vil, *n;

            ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
                                 sizeof(vpudrv_inst_info_t));

            if (ret != 0)
                break;

            inst_info.inst_open_count = 0;

            spin_lock(&s_vpu_lock);
            list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
                if (vil->core_idx == inst_info.core_idx)
                    inst_info.inst_open_count++;
            }
            spin_unlock(&s_vpu_lock);

            ret = copy_to_user((void __user *)arg, &inst_info,
                               sizeof(vpudrv_inst_info_t));

            pr_info("[VPUDRV] VDI_IOCTL_GET_INSTANCE_NUM core_idx=%d, inst_idx=%d, open_count=%d\n",
                    (int)inst_info.core_idx, (int)inst_info.inst_idx,
                    inst_info.inst_open_count);

        }
        break;

        case VDI_IOCTL_RESET: {
            ret = vpu_hw_reset();
        }
        break;

        case VDI_IOCTL_GET_REGISTER_INFO: {
            ret = copy_to_user((void __user *)arg, &s_vpu_register,
                               sizeof(vpudrv_buffer_t));

            if (ret != 0)
                ret = -EFAULT;

            pr_info("[VPUDRV] VDI_IOCTL_GET_REGISTER_INFO s_vpu_register.phys_addr %llx, s_vpu_register.virt_addr %llx, s_vpu_register.size %d\n",
                    s_vpu_register.phys_addr, s_vpu_register.virt_addr,
                    s_vpu_register.size);
        }
        break;

        case VDI_IOCTL_DEVICE_MEMORY_MAP: {

            struct dma_buf *temp = NULL;
            vpudrv_buffer_t  buf = {0};

            if (copy_from_user(&buf,  (void __user *)arg,
                               sizeof(vpudrv_buffer_t)))
                return -1;

            if (NULL == (temp = dma_buf_get(buf.buf_handle))) {
                pr_err("[VPUDRV-ERR] get dma buf error \n");
                return -1;
            }

            if (NULL == (void *)(buf.attachment = (uint64_t)dma_buf_attach(temp,
                                                  vpuwave_device))) {
                pr_err("[VPUDRV-ERR] get dma buf attach error \n");
                return -1;
            }

            if (NULL ==  (void *)(buf.sgt = (uint64_t) dma_buf_map_attachment((
                                                struct dma_buf_attachment *)buf.attachment, 0))) {
                pr_err("[VPUDRV-ERR] dma_buf_map_attachment error \n");
                return -1;
            }

            if (NULL == (void *)( buf.dma_addr  = sg_dma_address(((
                    struct sg_table *)(buf.sgt))->sgl))) {
                pr_err("[VPUDRV-ERR] sg_dma_address error \n");
                return -1;
            }

            if (copy_to_user((void __user *)arg, &buf, sizeof(vpudrv_buffer_t)))
                return -1;

            dma_buf_put(temp);
/*
            pr_info("get dma handle %d, dma buf addr %p, attachment %p, sgt %p\n",
                    buf.buf_handle,
                    (void *)buf.dma_addr,
                    (void *)buf.attachment,
                    (void *)buf.sgt);
*/
        }
        break;

        case VDI_IOCTL_DEVICE_MEMORY_UNMAP: {

            struct dma_buf *temp = NULL;
            vpudrv_buffer_t  buf = {0};

            if (copy_from_user(&buf,  (void __user *)arg,
                               sizeof(vpudrv_buffer_t)))
                return -1;

            if (NULL == (temp = dma_buf_get(buf.buf_handle))) {
                pr_err("[VPUDRV-ERR] unmap get dma buf error buf.buf_handle %d \n",
                        buf.buf_handle);
                return -1;
            }

            if ((NULL == (struct dma_buf_attachment *)(buf.attachment))
                    || ((NULL == (struct sg_table *)buf.sgt))) {
                pr_err("[VPUDRV-ERR] dma_buf_unmap_attachment param null  \n");
                return -1;
            }

            dma_buf_unmap_attachment((struct dma_buf_attachment *)(
                                         buf.attachment),
                                     (struct sg_table *)buf.sgt, 0);
            dma_buf_detach(temp, (struct dma_buf_attachment *)buf.attachment);
            dma_buf_put(temp);
            temp = NULL;

        }
        break;

        case VDI_IOCTL_DEVICE_SRAM_CFG: {

            int ret = 0;
            uint32_t mode = 0;

            if (get_user(mode, (u32 __user *) arg)) {
                pr_err("[VPUDRV-ERR]  sram cfg error get_user\n");
                return -EFAULT;
            }

            if (!drive_data.scr_signal) {
                pr_err("[VPUDRV-ERR]  scr signal value null \n");
                return -EFAULT;
            }

            /* using  scr-api to set sram/linebuffer mode  */
            ret = sdrv_scr_set(SCR_SEC, drive_data.scr_signal, mode);
            if (0 != ret) {
                pr_err("[VPUDRV-ERR]  scr config error, ret %d , mode %d \n", ret, mode);
                return ret;
            }

            return ret;
        }
        break;

        case VDI_IOCTL_DEVICE_GET_SRAM_INFO: {

            /* copy sram_info to user space;  note: two srams */
            if (copy_to_user((void __user *)arg, &(drive_data.sram[0]), sizeof(struct sram_info) * 2))
                return -1;

        }
        break;

        default: {
            pr_err("[VPUDRV] No such IOCTL, cmd is %d\n", cmd);
        }
        break;
    }

    return ret;
}

static ssize_t vpu_read(struct file *filp, char __user *buf,
                        size_t len,
                        loff_t *ppos)
{

    return -1;
}

static ssize_t vpu_write(struct file *filp, const char __user *buf,
                         size_t len,
                         loff_t *ppos)
{

    if (!buf) {
        pr_err( "[VPUDRV] vpu_write buf = NULL error \n");
        return -EFAULT;
    }

    if (len == sizeof(vpu_bit_firmware_info_t)) {
        vpu_bit_firmware_info_t *bit_firmware_info;

        bit_firmware_info = kmalloc(sizeof(vpu_bit_firmware_info_t),
                                    GFP_KERNEL);

        if (!bit_firmware_info) {
            pr_err("[VPUDRV-ERR] vpu_write  bit_firmware_info allocation error \n");
            return -EFAULT;
        }

        if (copy_from_user(bit_firmware_info, buf, len)) {
            pr_err("[VPUDRV-ERR] vpu_write copy_from_user error for bit_firmware_info\n");
            return -EFAULT;
        }

        if (bit_firmware_info->size == sizeof(vpu_bit_firmware_info_t)) {
            pr_info("[VPUDRV] vpu_write set bit_firmware_info coreIdx=0x%x, reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
                    bit_firmware_info->core_idx, (int)bit_firmware_info->reg_base_offset,
                    bit_firmware_info->size, bit_firmware_info->bit_code[0]);

            //if (bit_firmware_info->core_idx > MAX_NUM_VPU_CORE) {
            if (bit_firmware_info->core_idx != 1) {
                pr_err(
                    "[VPUDRV] vpu_write coreIdx[%d] is exceeded than MAX_NUM_VPU_CORE[%d]\n",
                    bit_firmware_info->core_idx, MAX_NUM_VPU_CORE);
                return -ENODEV;
            }

#if 0
            memcpy((void *)&s_bit_firmware_info[bit_firmware_info->core_idx],
                   bit_firmware_info,
                   sizeof(vpu_bit_firmware_info_t));
#else
            memcpy((void *)&s_bit_firmware_info[0], bit_firmware_info,
                   sizeof(vpu_bit_firmware_info_t));
#endif
            kfree(bit_firmware_info);
            return len;
        }

        kfree(bit_firmware_info);
    }

    return -1;
}

static int vpu_release(struct inode *inode, struct file *filp)
{
    int ret = 0;

    pr_info("[VPUDRV] vpu_release\n");

    if ((ret = down_interruptible(&s_vpu_sem)) == 0) {

        /* found and free the not handled buffer by user applications */
        vpu_free_buffers(filp);

        /* found and free the not closed instance by user applications */
        vpu_free_instances(filp);
        s_vpu_drv_context.open_count--;

        if (s_vpu_drv_context.open_count == 0) {
            if (s_instance_pool.base) {
                pr_info("[VPUDRV] free instance pool\n");
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
                vfree((const void *)s_instance_pool.base);
#else
                vpu_free_dma_buffer(&s_instance_pool);
#endif
                s_instance_pool.base = 0;
            }

#ifndef COMMON_MEMORY_USING_ION
#if 0  // common buffer can not free until system power off
            if (s_common_memory.base) {
                pr_info("[VPUDRV] free common memory\n");
                vpu_free_dma_buffer(&s_common_memory);
                s_common_memory.base = 0;
            }
#endif
#endif
        }
    }

    up(&s_vpu_sem);
    return 0;
}


static int vpu_fasync(int fd, struct file *filp, int mode)
{
    struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)
                                    filp->private_data;
    return fasync_helper(fd, filp, mode, &dev->async_queue);
}


static int vpu_map_to_register(struct file *fp,
                               struct vm_area_struct *vm)
{
    unsigned long pfn;

    vm->vm_flags |= VM_IO | VM_RESERVED;
    vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
    pfn = s_vpu_register.phys_addr >> PAGE_SHIFT;
    return remap_pfn_range(vm, vm->vm_start, pfn,
                           vm->vm_end - vm->vm_start,
                           vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_physical_memory(struct file *fp,
                                      struct vm_area_struct *vm)
{
    vm->vm_flags |= VM_IO | VM_RESERVED;
    //vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
    vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot);
    return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff,
                           vm->vm_end - vm->vm_start,
                           vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_instance_pool_memory(struct file *fp,
        struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
    int ret;
    long length = vm->vm_end - vm->vm_start;
    unsigned long start = vm->vm_start;
    char *vmalloc_area_ptr = (char *)s_instance_pool.base;
    unsigned long pfn;

    vm->vm_flags |= VM_RESERVED;

    /* loop over all pages, map it page individually */
    while (length > 0) {
        pfn = vmalloc_to_pfn(vmalloc_area_ptr);

        if ((ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE,
                                   PAGE_SHARED)) < 0) {
            return ret;
        }

        start += PAGE_SIZE;
        vmalloc_area_ptr += PAGE_SIZE;
        length -= PAGE_SIZE;
    }


    return 0;
#else
    vm->vm_flags |= VM_RESERVED;
    return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff,
                           vm->vm_end - vm->vm_start,
                           vm->vm_page_prot) ? -EAGAIN : 0;
#endif
}

/**
 * brief memory map interface for vpu file operation
 * return  0 on success or negative error code on error
 * note  instance pool memory
 *       register memory
 *       common memory
 */
static int vpu_mmap(struct file *fp, struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY

    if (vm->vm_pgoff == 0)
        return vpu_map_to_instance_pool_memory(fp, vm);

    if (vm->vm_pgoff == (s_vpu_register.phys_addr >> PAGE_SHIFT)) {
        pr_info("[VPUDRV]s_vpu_register.phys_addr %p, vm_pagoff %lx\n",
                (void *)(s_vpu_register.phys_addr), vm->vm_pgoff);
        return vpu_map_to_register(fp, vm);
    }

#ifndef COMMON_MEMORY_USING_ION

    /*
     * Common memory onyl using by vpu
     * map dma memory using dam_address
     * dma_address same as phys_address when no smmu
     */
    if (vm->vm_pgoff == (0xfe000 >> PAGE_SHIFT)) {
        pr_info("[VPUDRV] vpuwave_device %p, s_common_memory.phys_addr %p, dma_addr %p ,base %p,  size %d, vm_pagoff %lx \n",
                (void *)vpuwave_device,
                (void *)s_common_memory.phys_addr,
                (void *)s_common_memory.dma_addr,
                (void *)s_common_memory.base,
                s_common_memory.size, vm->vm_pgoff);

        vm->vm_pgoff = 0;
        return dma_mmap_coherent(vpuwave_device, vm,
                                 (void *)s_common_memory.base, s_common_memory.dma_addr,
                                 vm->vm_end - vm->vm_start);
    }

#endif

    pr_err("[VPUDRV-ERR] never come here while using ion  vm_pgoff %lx, s_common_memory.phys_addr %p, s_common_memory.phys_addr >> PAGE_SHIFT %llx \n  ",
            vm->vm_pgoff,
            (void *)s_common_memory.phys_addr,
            s_common_memory.phys_addr >> PAGE_SHIFT);

    return vpu_map_to_physical_memory(fp, vm);
#else

    if (vm->vm_pgoff) {
        if (vm->vm_pgoff == (s_instance_pool.phys_addr >> PAGE_SHIFT))
            return vpu_map_to_instance_pool_memory(fp, vm);

        return vpu_map_to_physical_memory(fp, vm);
    }
    else {
        return vpu_map_to_register(fp, vm);
    }

#endif
}

/*brief get sram info from dts
 *pdev:
 *info:
 *notes: info[0] inter sram in vpu
 *       info[1] sram in soc,  select only one(sram3 or sram2)
 */
static void vpu_get_sram_info(struct platform_device *pdev, struct sram_info  *info)
{
    struct resource *res = NULL;
    struct device_node *ram_node = NULL;
    struct resource res_temp = {0};
    int32_t index = 0; /*sram2 index in soc_sram */

    if(pdev) {
        res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inter_sram");
        if(res) {
            info[0].phy = res->start;
            info[0].size = res->end - res->start + 1;
            info[0].id = 1;  /*for inter sram */
        }

        if(NULL != (ram_node = of_parse_phandle(pdev->dev.of_node, "vpu1,sram2", 0))) {
            if(!of_address_to_resource(ram_node, index, &res_temp)) {
                info[1].phy = res_temp.start;
                info[1].size = res_temp.end - res_temp.start + 1;
                info[1].id = 2;   /* for soc sram */
            }
        }

        index = 1; /*sram3 index in soc_sram */
        if(NULL != (ram_node = of_parse_phandle(pdev->dev.of_node, "vpu1,sram3", 0))) {
            if(!of_address_to_resource(ram_node, index, &res_temp)) {
                info[1].phy = res_temp.start;
                info[1].size = res_temp.end - res_temp.start + 1;
                info[1].id = 2;   /* for soc sram */
            }
        }

    }

    pr_info("[VPUDRV] Wave412 get sram info : sram[0].id %d;  sram[0].phy %#x; sram[0].size %#x ,sram[1].id %d;  sram[1].phy %#x; sram[1].size %#x \n",
                                      info[0].id , info[0].phy, info[0].size,
                                      info[1].id , info[1].phy, info[1].size);
}

/*
 *brief: show wave device debug info
 *para:  dev
 *para: attr
 *buf   value
 *note: cat /sys/device/platform/soc/31440000.wave412/wave_debug
 */

static ssize_t show_wave412_device (struct device *dev,
                  struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "sram[0]:  %#x\nsram[1]:  %#x\nclk_reference:  %d\ncommon_mem base %p",
                        drive_data.sram[0].phy,
                        drive_data.sram[1].phy,
                        clk_reference,
                        (void*)s_common_memory.base);

}

/*
 *brief: set wave device
 *para:  dev
 *para: attr
 *buf   value
 *note: set config info
 */
static ssize_t set_wave412_device (struct device *dev,
               struct device_attribute *attr,
               const char *buf, size_t len)
{
    return len;
}

static DEVICE_ATTR(wave412_debug, 0664, show_wave412_device, set_wave412_device);

static struct file_operations vpu_fops = {
    .owner = THIS_MODULE,
    .open = vpu_open,
    .read = vpu_read,
    .write = vpu_write,
    .unlocked_ioctl = vpu_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = vpu_ioctl,
#endif
    .release = vpu_release,
    .fasync = vpu_fasync,
    .mmap = vpu_mmap,
};

static int vpu_probe(struct platform_device *pdev)
{
    int err = 0;
    struct resource *res = NULL;
    memset(&drive_data, 0 , sizeof(struct wave_drive));

    pr_info("[VPUDRV] vpu_probe\n");

    if (pdev) {
        vpuwave_device = &(pdev->dev);
        res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "register");
    }

    if (res) {/* if platform driver is implemented */
        s_vpu_register.phys_addr = res->start;
        s_vpu_register.virt_addr = (unsigned long)ioremap_nocache(res->start,
                                   res->end - res->start + 1);
        s_vpu_register.size = res->end - res->start + 1;
        pr_info("[VPUDRV] : vpu base address get from platform driver physical base addr %p , kernel virtual base %p , vpuwave_device %p  register size %#x\n",
                (void *)s_vpu_register.phys_addr, (void *)s_vpu_register.virt_addr,
                vpuwave_device, s_vpu_register.size);
    }
    else {
        s_vpu_register.phys_addr = VPU_REG_BASE_ADDR;
        s_vpu_register.virt_addr = (unsigned long)ioremap_nocache(
                                       s_vpu_register.phys_addr,
                                       VPU_REG_SIZE);
        s_vpu_register.size = VPU_REG_SIZE;
        pr_info("[VPUDRV] : vpu base address get from defined value physical base addr %p, virtual base %p\n",
                (void *)s_vpu_register.phys_addr, (void *)s_vpu_register.virt_addr);
    }

    vpu_get_sram_info(pdev, &(drive_data.sram[0]));

    /* get scr config info */
    if (!device_property_read_u32(&(pdev->dev), "sdrv,scr_signal", &(drive_data.scr_signal))) {
        pr_info("[VPUDRV] wave412 scr signal value  %d\n", drive_data.scr_signal);
    }

    /* get the major number of the character device */
    if ((alloc_chrdev_region(&s_vpu_major, 0, 1, VPU_DEV_NAME)) < 0) {
        err = -EBUSY;
        pr_err( "[VPUDRV-ERR] could not allocate major number\n");
        goto ERROR_PROVE_DEVICE;
    }

    /* initialize the device structure and register the device with the kernel */
    cdev_init(&s_vpu_cdev, &vpu_fops);

    if ((cdev_add(&s_vpu_cdev, s_vpu_major, 1)) < 0) {
        err = -EBUSY;
        pr_err( "[VPUDRV-ERR] could not allocate chrdev\n");

        goto ERROR_PROVE_DEVICE;
    }

    if(NULL == (drive_data.wave_class = class_create(THIS_MODULE, "vpu_wave"))) {
        pr_err("[VPUDRV-ERR] could not allocate class\n");
        err = -EBUSY;
        goto ERROR_PROVE_DEVICE;
    }

    if (NULL == (drive_data.wave_device = device_create(drive_data.wave_class, NULL, s_vpu_major, NULL, VPU_DEV_NAME))) {
        err = -EBUSY;
        pr_err("ERR could not allocate device\n");
        goto ERROR_PROVE_DEVICE;
    }

    if (device_create_file(&(pdev->dev), &dev_attr_wave412_debug)) {
        pr_err("[VPUDRV-ERR] cdevice_create_file fail \n");
        goto ERROR_PROVE_DEVICE;
    }

    if (pdev)
        s_vpu_clk = vpu_clk_get(&pdev->dev);
    else
        s_vpu_clk = vpu_clk_get(NULL);

    if (!s_vpu_clk) {
        pr_info("[VPUDRV] : platforem can not support clock controller.\n");
    }
    else
        pr_info("[VPUDRV] : get clock controller s_vpu_clk=%p\n", s_vpu_clk);

//    vpu_clk_enable(s_vpu_clk);

#ifdef VPU_SUPPORT_ISR
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER

    if (pdev)
        res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

    if (res) {/* if platform driver is implemented */
        s_vpu_irq = res->start;
        pr_info("[VPUDRV] : vpu irq number get from platform driver irq=0x%x\n",
                s_vpu_irq);
    }
    else {
        pr_info("[VPUDRV] : vpu irq number get from defined value irq=0x%x\n",
                s_vpu_irq);
    }

#else
    pr_info("[VPUDRV] : vpu irq number get from defined value irq=0x%x\n",
            s_vpu_irq);
#endif

    err = request_irq(s_vpu_irq, vpu_irq_handler, 0, "VPU_CODEC_IRQ",
                      (void *)(&s_vpu_drv_context));

    if (err) {
        pr_err( "[VPUDRV-ERR] :  fail to register interrupt handler\n");
        goto ERROR_PROVE_DEVICE;
    }

#endif

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    s_video_memory.size = VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
    s_video_memory.phys_addr = VPU_DRAM_PHYSICAL_BASE;
    s_video_memory.base = (unsigned long)ioremap_nocache(
                              s_video_memory.phys_addr,
                              PAGE_ALIGN(s_video_memory.size));

    if (!s_video_memory.base) {
        pr_err("[VPUDRV-ERR] :  fail to remap video memory physical phys_addr=0x%lx, base=0x%lx, size=%d\n",
                s_video_memory.phys_addr, s_video_memory.base,
                (int)s_video_memory.size);
        goto ERROR_PROVE_DEVICE;
    }

    if (vmem_init(&s_vmem, s_video_memory.phys_addr,
                  s_video_memory.size) < 0) {
        pr_err( "[VPUDRV-ERR] :  fail to init vmem system\n");
        goto ERROR_PROVE_DEVICE;
    }

    pr_info("[VPUDRV] success to probe vpu device with reserved video memory phys_addr=0x%lx, base=0x%lx\n",
            s_video_memory.phys_addr, s_video_memory.base);
#else
    pr_info("[VPUDRV] success to probe vpu device with non reserved video memory\n");
#endif

    return 0;

ERROR_PROVE_DEVICE:

    if(drive_data.wave_device && drive_data.wave_class) {
        device_destroy(drive_data.wave_class, s_vpu_major);
        drive_data.wave_device = NULL;
    }

    if(drive_data.wave_class) {
        class_destroy(drive_data.wave_class);
        drive_data.wave_class = NULL;
    }

    if (s_vpu_major)
        unregister_chrdev_region(s_vpu_major, 1);

    if (s_vpu_register.virt_addr)
        iounmap((void *)s_vpu_register.virt_addr);

    return err;
}

static int vpu_remove(struct platform_device *pdev)
{
    pr_info("[VPUDRV] vpu_remove\n");
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER

    if (s_instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
        vfree((const void *)s_instance_pool.base);
#else
        vpu_free_dma_buffer(&s_instance_pool);
#endif
        s_instance_pool.base = 0;
    }

    if (s_common_memory.base) {
        vpu_free_dma_buffer(&s_common_memory);
        s_common_memory.base = 0;
    }

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY

    if (s_video_memory.base) {
        iounmap((void *)s_video_memory.base);
        s_video_memory.base = 0;
        vmem_exit(&s_vmem);
    }

#endif

    if (s_vpu_major > 0) {
        cdev_del(&s_vpu_cdev);
        unregister_chrdev_region(s_vpu_major, 1);
        s_vpu_major = 0;
    }

#ifdef VPU_SUPPORT_ISR

    if (s_vpu_irq)
        free_irq(s_vpu_irq, &s_vpu_drv_context);

#endif

    if (s_vpu_register.virt_addr)
        iounmap((void *)s_vpu_register.virt_addr);

    if(s_vpu_clk) {
        vpu_clk_put(s_vpu_clk);
        s_vpu_clk = NULL;
    }

#endif /*VPU_SUPPORT_PLATFORM_DRIVER_REGISTER*/

    return 0;
}

#ifdef CONFIG_PM
#define W4_MAX_CODE_BUF_SIZE     (512*1024)
#define W4_CMD_INIT_VPU       (0x0001)
#define W4_CMD_SLEEP_VPU         (0x0400)
#define W4_CMD_WAKEUP_VPU       (0x0800)

static void Wave4BitIssueCommand(int core, u32 cmd)
{
    WriteVpuRegister(W4_VPU_BUSY_STATUS, 1);
    WriteVpuRegister(W4_CORE_INDEX,  0);
    /*  coreIdx = ReadVpuRegister(W4_VPU_BUSY_STATUS);*/
    /*  coreIdx = 0;*/
    /*  WriteVpuRegister(W4_INST_INDEX,  (instanceIndex&0xffff)|(codecMode<<16));*/
    WriteVpuRegister(W4_COMMAND, cmd);
    WriteVpuRegister(W4_VPU_HOST_INT_REQ, 1);

    return;
}

static int vpu_suspend(struct platform_device *pdev,
                       pm_message_t state)
{
    int core;
    unsigned long timeout = jiffies + HZ;   /* vpu wait timeout to 1sec */
    int product_code;

    pr_info("[VPUDRV] vpu_suspend\n");

    vpu_clk_enable(s_vpu_clk);

    if (s_vpu_open_ref_count > 0) {
        for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
            if (s_bit_firmware_info[core].size == 0)
                continue;

            product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);

            if (PRODUCT_CODE_WAVE412(product_code)) {
                while (ReadVpuRegister(W4_VPU_BUSY_STATUS)) {
                    if (time_after(jiffies, timeout)) {
                        pr_err("SLEEP_VPU BUSY timeout");
                        goto DONE_SUSPEND;
                    }
                }

                Wave4BitIssueCommand(core, W4_CMD_SLEEP_VPU);

                while (ReadVpuRegister(W4_VPU_BUSY_STATUS)) {
                    if (time_after(jiffies, timeout)) {
                        pr_err("SLEEP_VPU BUSY timeout");
                        goto DONE_SUSPEND;
                    }
                }

                if (ReadVpuRegister(W4_RET_SUCCESS) == 0) {
                    pr_err("SLEEP_VPU failed [0x%x]",
                            ReadVpuRegister(W4_RET_FAIL_REASON));
                    goto DONE_SUSPEND;
                }
            }
            else {
                pr_err("[VPUDRV] Unknown product id : %08x\n", product_code);
                goto DONE_SUSPEND;
            }
        }
    }

    vpu_clk_disable(s_vpu_clk);
    return 0;

DONE_SUSPEND:

    vpu_clk_disable(s_vpu_clk);

    return -EAGAIN;

}
static int vpu_resume(struct platform_device *pdev)
{
    int core;
    unsigned long timeout = jiffies + HZ;   /* vpu wait timeout to 1sec */
    int product_code;
    unsigned long   code_base;
    u32         code_size;
    u32         remap_size;
    int          regVal;
    u32         hwOption  = 0;

    pr_info("[VPUDRV] vpu_resume\n");

    vpu_clk_enable(s_vpu_clk);

    for (core = 0; core < MAX_NUM_VPU_CORE; core++) {

        if (s_bit_firmware_info[core].size == 0) {
            continue;
        }

        product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);

        if (PRODUCT_CODE_WAVE412(product_code)) {
            code_base = s_common_memory.phys_addr;
            /* ALIGN TO 4KB */
            code_size = (W4_MAX_CODE_BUF_SIZE & ~0xfff);

            if (code_size < s_bit_firmware_info[core].size * 2) {
                goto DONE_WAKEUP;
            }

            regVal = 0;
            WriteVpuRegister(W4_PO_CONF, regVal);

            /* Reset All blocks */
            regVal = 0x7ffffff;
            WriteVpuRegister(W4_VPU_RESET_REQ, regVal); /*Reset All blocks*/

            /* Waiting reset done */
            while (ReadVpuRegister(W4_VPU_RESET_STATUS)) {
                if (time_after(jiffies, timeout))
                    goto DONE_WAKEUP;
            }

            WriteVpuRegister(W4_VPU_RESET_REQ, 0);

            /* remap page size */
            remap_size = (code_size >> 12) & 0x1ff;
            regVal = 0x80000000 | (W4_REMAP_CODE_INDEX << 12) | (0 << 16) |
                     (1 << 11) | remap_size;
            WriteVpuRegister(W4_VPU_REMAP_CTRL,  regVal);
            WriteVpuRegister(W4_VPU_REMAP_VADDR,
                             0x00000000);    /* DO NOT CHANGE! */
            WriteVpuRegister(W4_VPU_REMAP_PADDR,    code_base);
            WriteVpuRegister(W4_ADDR_CODE_BASE,  code_base);
            WriteVpuRegister(W4_CODE_SIZE,        code_size);
            WriteVpuRegister(W4_CODE_PARAM,      0);
            WriteVpuRegister(W4_INIT_VPU_TIME_OUT_CNT,   timeout);

            WriteVpuRegister(W4_HW_OPTION, hwOption);

            /* Interrupt */
            regVal  = (1 << W4_INT_DEC_PIC_HDR);
            regVal |= (1 << W4_INT_DEC_PIC);
            regVal |= (1 << W4_INT_QUERY_DEC);
            //regVal |= (1 << W4_INT_SLEEP_VPU);
            regVal |= (1 << W4_INT_BSBUF_EMPTY);

            WriteVpuRegister(W4_VPU_VINT_ENABLE,  regVal);

            Wave4BitIssueCommand(core, W4_CMD_INIT_VPU);
            WriteVpuRegister(W4_VPU_REMAP_CORE_START, 1);

            while (ReadVpuRegister(W4_VPU_BUSY_STATUS)) {
                if (time_after(jiffies, timeout))
                    goto DONE_WAKEUP;
            }

            if (ReadVpuRegister(W4_RET_SUCCESS) == 0) {
                pr_info("WAKEUP_VPU failed [0x%x]",
                        ReadVpuRegister(W4_RET_FAIL_REASON));
                goto DONE_WAKEUP;
            }
        }
        else {
            pr_err("[VPUDRV] Unknown product id : %08x\n", product_code);
            goto DONE_WAKEUP;
        }

    }

    if (s_vpu_open_ref_count == 0)
        vpu_clk_disable(s_vpu_clk);

DONE_WAKEUP:

    if (s_vpu_open_ref_count > 0)
        vpu_clk_enable(s_vpu_clk);

    return 0;
}
#else
#define vpu_suspend NULL
#define vpu_resume  NULL
#endif              /* !CONFIG_PM */

#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
static const struct of_device_id vpuwave_of_table[] = {
    {.compatible = "semidrive,wave412",},
    {},
};
MODULE_DEVICE_TABLE(of, vpuwave_of_table);
static struct platform_driver vpu_driver = {
    .driver = {
        .name = VPU_PLATFORM_DEVICE_NAME,
        .of_match_table = vpuwave_of_table,
    },
    .probe = vpu_probe,
    .remove = vpu_remove,
    .suspend = vpu_suspend,
    .resume = vpu_resume,
};
#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */


static int __init vpu_init(void)
{
    int res;
    pr_info("[VPUDRV] wave begin vpu_init\n");
    init_waitqueue_head(&s_interrupt_wait_q);
    s_common_memory.base = 0;
    s_instance_pool.base = 0;
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    pr_info("[VPUDRV] wave guy ...\n");
    res = platform_driver_register(&vpu_driver);
#else
    res = vpu_probe(NULL);
#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

    pr_info("[VPUDRV] end vpu_init result=0x%x\n", res);
    return res;
}

static void __exit vpu_exit(void)
{
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    pr_info("[VPUDRV] vpu_exit\n");

    if(drive_data.wave_device && drive_data.wave_class) {
        device_destroy(drive_data.wave_class, s_vpu_major);
        drive_data.wave_device = NULL;
        pr_info("[VPUDRV] wave device destrory \n");
    }

    if(drive_data.wave_class) {
        class_destroy(drive_data.wave_class);
        drive_data.wave_class = NULL;
        pr_info("[VPUDRV] wave class destroy \n");
    }

    device_remove_file(vpuwave_device, &dev_attr_wave412_debug);
    platform_driver_unregister(&vpu_driver);

#else /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

#ifdef VPU_SUPPORT_CLOCK_CONTROL
    vpu_clk_disable(s_vpu_clk);
#else
#endif
    vpu_clk_put(s_vpu_clk);
    s_vpu_clk = NULL;

    if (s_instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
        vfree((const void *)s_instance_pool.base);
#else
        vpu_free_dma_buffer(&s_instance_pool);
#endif
        s_instance_pool.base = 0;
    }

    if (s_common_memory.base) {
        vpu_free_dma_buffer(&s_common_memory);
        s_common_memory.base = 0;
    }

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY

    if (s_video_memory.base) {
        iounmap((void *)s_video_memory.base);
        s_video_memory.base = 0;
        vmem_exit(&s_vmem);
    }

#endif

    if (s_vpu_major > 0) {
        cdev_del(&s_vpu_cdev);
        unregister_chrdev_region(s_vpu_major, 1);
        s_vpu_major = 0;
    }

#ifdef VPU_SUPPORT_ISR

    if (s_vpu_irq)
        free_irq(s_vpu_irq, &s_vpu_drv_context);

#endif

    if (s_vpu_register.virt_addr) {
        iounmap((void *)s_vpu_register.virt_addr);
        s_vpu_register.virt_addr = 0x00;
    }

#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */
    vpuwave_device = NULL;

    return;
}

MODULE_AUTHOR("A customer using C&M VPU, Inc.");
MODULE_DESCRIPTION("VPU wave412 linux driver");
MODULE_LICENSE("GPL");

module_init(vpu_init);
module_exit(vpu_exit);

static int vpu_hw_reset(void)
{
    struct reset_control *vpu_rst = NULL;

    if (vpuwave_device)
    {
        vpu_rst = devm_reset_control_get(vpuwave_device, "vpu-reset");
        if (IS_ERR(vpu_rst)) {
            dev_err(vpuwave_device, "Error: Missing controller reset\n");
            return -1;
        }

        if (reset_control_reset(vpu_rst)) {
            reset_control_put(vpu_rst);
            dev_err(vpuwave_device, "reset vpu failed\n");
            return -1;
        }

        reset_control_put(vpu_rst);
    } else {
        pr_err("[VPUDRV-ERR] No vpucoda_device\n");
        return -1;
    }

    return 0;
}

static struct vpuwave_clk  *vpu_clk_get(struct device *dev)
{
#ifdef  VPU_SUPPORT_CLOCK_CONTROL
    struct vpuwave_clk *clk = NULL;
    clk = (struct vpuwave_clk *) kmalloc(sizeof(struct vpuwave_clk),
                                         GFP_KERNEL);

    if (!clk)
        return NULL;

    memset(clk, 0x00, sizeof(struct vpuwave_clk));
    clk->core_clk = clk_get(dev, "core-clk");
    clk->bcore_clk = clk_get(dev, "bcore-clk");

    if (!(clk->core_clk == NULL
            || IS_ERR(clk->core_clk)
            || clk->bcore_clk == NULL
            || IS_ERR(clk->bcore_clk))) {

        pr_info("[VPUDRV] wave bcore_clk-rate %lu \n", clk_get_rate(clk->bcore_clk));
        pr_info("[VPUDRV] wave core_clk rate %lu \n", clk_get_rate(clk->core_clk));
       return clk;
    }

    pr_err("[VPUDRV] get clk failure \n");
    return NULL;

#else
    return NULL;
#endif

}

static void vpu_clk_put(struct vpuwave_clk *clk)
{
#ifdef VPU_SUPPORT_CLOCK_CONTROL

    if (clk) {
        if (!(clk->core_clk == NULL
                || IS_ERR(clk->core_clk)
                || clk->bcore_clk == NULL
                || IS_ERR(clk->bcore_clk))) {

            clk_put(clk->bcore_clk);
            clk_put(clk->core_clk);
            pr_info("[VPUDRV] colk and pclk put is success \n");
        }

        kfree(clk);
        clk = NULL;
    }

#else

#endif
}

/*
 *brief: vpu clk enable
 *para : clk
 *note: clk_enable before must call clk_prepare
 *      to check reference
 */
static int vpu_clk_enable(struct vpuwave_clk *clk)
{
    int err_code = 0;
#ifdef  VPU_SUPPORT_CLOCK_CONTROL

    if (clk) {
        if (!(clk->core_clk == NULL
                || IS_ERR(clk->core_clk)
                || clk->bcore_clk == NULL
                || IS_ERR(clk->bcore_clk))) {

            if(0 != (err_code = clk_prepare(clk->bcore_clk))) {
                pr_err("[VPUDRV-ERR] wave bck ckl prepare bcore clk error code %d \n", err_code);
                return err_code;
            }

            if(0 != (err_code = clk_prepare(clk->core_clk))) {
                pr_err("[VPUDRV-ERR] wave core ckl prepare core clk error code %d \n", err_code);
                return err_code;
            }

            clk_enable(clk->bcore_clk);
            clk_enable(clk->core_clk);
            clk_reference++;
        }

    }

    return 0;
#else
    return 0;
#endif
}

/*
 *brief: vpu clk disable
 *para : clk
 *note: clk_disable behind must call clk_unprepare
 *      to reduce reference
 */
static void vpu_clk_disable(struct vpuwave_clk *clk)
{
#ifdef VPU_SUPPORT_CLOCK_CONTROL

    if (clk) {
        if (!(clk->core_clk == NULL
                || IS_ERR(clk->core_clk)
                || clk->bcore_clk == NULL
                || IS_ERR(clk->bcore_clk))) {

            clk_disable(clk->bcore_clk);
            clk_disable(clk->core_clk);

            clk_unprepare(clk->bcore_clk);
            clk_unprepare(clk->core_clk);
            clk_reference--;
        }

    }

#else
    /*do nothing */

#endif
}


