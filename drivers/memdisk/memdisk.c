#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

MODULE_LICENSE("Dual BSD/GPL");

static int memdisk_major = 0;
module_param(memdisk_major, int, 0);
static int hardsect_size = 512;
module_param(hardsect_size, int, 0);

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512
#define MEMDISK_MINORS	16
#define MEMDISKS_COUNT 4
/*
 * The internal representation of our device.
 */
struct memdisk_dev {
	unsigned long size;             /* Device size in sectors */
	spinlock_t lock;                /* For mutual exclusion */
	u8 *data;                       /* The data array */
	struct request_queue *queue;    /* The device request queue */
	struct gendisk *gd;             /* The gendisk structure */
};

/* default addr & size if not found in dts */
#define MEMDISK_SYSTEM_PHYS    0x80000000
#define MEMDISK_SYSTEM_SIZE    0x48000000  // 1.1G

#define MEMDISK_USERDATA_PHYS  0xc9000000  // haps backdoor only 32bit
#define MEMDISK_USERDATA_SIZE  0x20000000  // 512M

#define MEMDISK_VENDOR_PHYS    0xea000000  //
#define MEMDISK_VENDOR_SIZE    0x10000000  // 256M

#define MEMDISK_CACHE_PHYS     0xfb000000  // use this partition as xfer space
#define MEMDISK_CACHE_SIZE     0x04000000  // 64M

static struct memdisk_dev memdisks[] = {
	[0] = {
		.data    = MEMDISK_SYSTEM_PHYS,
		.size    = MEMDISK_SYSTEM_SIZE,
	},
	[1] = {
		.data    = MEMDISK_USERDATA_PHYS,
		.size    = MEMDISK_USERDATA_SIZE,
	},
	[2] = {
		.data    = MEMDISK_VENDOR_PHYS,
		.size    = MEMDISK_VENDOR_SIZE,
	},
	[3] = {
		.data    = MEMDISK_CACHE_PHYS,
		.size    = MEMDISK_CACHE_SIZE,
        },
};

/*
 * Handle an I/O request.
 */
static void memdisk_transfer(struct memdisk_dev *dev, sector_t sector,
		unsigned long nsect, char *buffer, int write)
{
	unsigned long offset = sector * hardsect_size;
	unsigned long nbytes = nsect * hardsect_size;

	//printk("%s: %s offset:%x, %d bytes. \n", __FUNCTION__, write ? "write" : "read", offset, nbytes);
	if ((offset + nbytes) > (dev->size * hardsect_size)) {
		pr_err("%s: Beyond-end write (%ld %ld)\n", __FUNCTION__, offset, nbytes);
		return;
	}

	if (write)
		memcpy(dev->data + offset, buffer, nbytes);
	else
		memcpy(buffer, dev->data + offset, nbytes);
}

static void memdisk_request(struct request_queue *q) {
	struct request *req;
	struct memdisk_dev *dev = q->queuedata;

	req = blk_fetch_request(q);
	while (req != NULL) {
		memdisk_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req),
				bio_data(req->bio), rq_data_dir(req));
		if ( ! __blk_end_request_cur(req, 0) ) {
			req = blk_fetch_request(q);
		}
	}
}

int memdisk_getgeo(struct block_device * bd, struct hd_geometry * geo)
{
	long size;

	struct memdisk_dev *dev = bd->bd_disk->private_data;
	size = dev->size *(hardsect_size / KERNEL_SECTOR_SIZE);

	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 4;

	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations memdisk_ops = {
	.owner  = THIS_MODULE,
	.getgeo = memdisk_getgeo
};

static int memdisk_parse_dt(void)
{
	struct device_node *matched_node, *memdisk_node;
	struct property *prop;
	const char *str;
	int ret, i, err;
	u64 addr;
	u32 size;

	matched_node = of_find_compatible_node(NULL, NULL, "sd,memdisks");
	if (!matched_node) {
		return -ENODEV;
	}

	for (i = 0; ; i++) {
		memdisk_node = of_parse_phandle(matched_node, "sd-memdisk", i);
		if (!memdisk_node)
			break;

		if (!of_device_is_available(memdisk_node)) {
			of_node_put(memdisk_node);
			continue;
		}
		// found, parse system/userdata/vendor
		err = of_property_read_string(memdisk_node, "memdisk-name", &str);
		if (err) {
			pr_err("Parsing memdisk addr %pOF failed with err %d\n", memdisk_node, err);
			err = -EINVAL;
			break;
		}

		err = of_property_read_u64(memdisk_node, "memdisk-addr", &addr);
		if (err) {
			pr_err("Parsing memdisk addr %pOF failed with err %d\n", memdisk_node, err);
			err = -EINVAL;
			break;
		}

		err = of_property_read_u32(memdisk_node, "memdisk-size", &size);
		if (err) {
			pr_err("Parsing memdisk size %pOF failed with err %d\n", memdisk_node, err);
			err = -EINVAL;
			break;
		}

		if (!str) {
			pr_err("%s: no partition name defined\n", __FUNCTION__);
		}
		else if (!strncmp(str, "system", 6)) {
			memdisks[0].data = addr;
			memdisks[0].size = size;
		}
		else if (!strncmp(str, "userdata", 8)){
			memdisks[1].data = addr;
			memdisks[1].size = size;
		}
		else if (!strncmp(str, "vendor", 6)){
			memdisks[2].data = addr;
			memdisks[2].size = size;
		}
		else {
			pr_err("%s: invalid partition name:%s\n", __FUNCTION__, str);
		}

		of_node_put(memdisk_node);
	}

	of_node_put(matched_node);

	return 0;
}

/*
 * Set up our internal device.
 */
static void memdisk_setup_device(struct memdisk_dev *dev, int i)
{
	unsigned char __iomem  *base;
	pr_info("%s:[%d]\n", __FUNCTION__, i);

	spin_lock_init(&dev->lock);
	base = ioremap((phys_addr_t)dev->data, dev->size);
	if(!base) {
		printk("%s:ioremap error, dev[%d] data: %llx, size: %x\n", __FUNCTION__, i, dev->data, dev->size);
		return;
	}

	printk("%s base: %llx, start: %llx, size: %x\n", __FUNCTION__, base, dev->data, dev->size);
	dev->data = base;
	dev->size = (dev->size / hardsect_size);

	dev->queue = blk_init_queue(memdisk_request, &dev->lock);
	if (dev->queue == NULL) {
		pr_err("%s blk_init_queue failure\n", __FUNCTION__);
		return;
	}

	blk_queue_logical_block_size(dev->queue, hardsect_size);
	dev->queue->queuedata = dev;

	dev->gd = alloc_disk(MEMDISK_MINORS);
	if (! dev->gd) {
		pr_err("%s alloc_disk failure\n", __FUNCTION__);
		return;
	}
	dev->gd->major = memdisk_major;
	dev->gd->first_minor = i*MEMDISK_MINORS;
	dev->gd->fops = &memdisk_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;

	snprintf (dev->gd->disk_name, 32, "memdisk.%d", i);
	set_capacity(dev->gd, dev->size*(hardsect_size/KERNEL_SECTOR_SIZE));
	add_disk(dev->gd);

	pr_info("%s:[%d] success\n", __FUNCTION__, i);
	return;
}

static int __init memdisk_init(void)
{
	int i;

	pr_info("%s \n", __FUNCTION__);
	memdisk_major = register_blkdev(memdisk_major, "memdisk");
	if (memdisk_major <= 0) {
		pr_err("memdisk: unable to get major number\n");
		return -EBUSY;
	}

	memdisk_parse_dt();

	for (i = 0; i < MEMDISKS_COUNT; i++)
		memdisk_setup_device(&memdisks[i], i);

	pr_info("%s finished\n", __FUNCTION__);
	return 0;
}

static void __exit memdisk_exit(void)
{
	int i;

	pr_info("%s\n", __FUNCTION__);
	for (i = 0; i < MEMDISKS_COUNT; i++) {
		struct memdisk_dev *dev = &memdisks[i];

		if (dev->gd) {
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}
		if (dev->queue) {
			blk_cleanup_queue(dev->queue);
		}
	}
	unregister_blkdev(memdisk_major, "memdisk");
}

module_init(memdisk_init);
module_exit(memdisk_exit);
