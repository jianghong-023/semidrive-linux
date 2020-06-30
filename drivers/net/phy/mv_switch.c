// SPDX-License-Identifier: GPL-2.0+
/**
 *  Driver for Marvell Automotive switch chips
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>

struct mv_data {
	unsigned int portaddr;
	unsigned int regnum;
	unsigned int is_write;
	unsigned int data[5];
};

#define MV_GET_CMD		_IOWR('M', 8, struct mv_data)
#define MV_5050_PHYID		0x50505050
#define MV_5072_PHYID		0x50725072
#define MV_GLOBAL1_PORT		0x1b

static struct phy_device *mv_phy[2];
static const struct file_operations mv_5050_fops;
static const struct file_operations mv_5072_fops;


static inline int mv_read(struct phy_device *phydev, u32 portaddr, u32 regnum)
{
	return mdiobus_read(phydev->mdio.bus, portaddr, regnum);
}

static inline int mv_write(struct phy_device *phydev, u32 portaddr, u32 regnum, u16 val)
{
	return mdiobus_write(phydev->mdio.bus, portaddr, regnum, val);
}

static unsigned int mv_get_count(struct phy_device *phy, int port, int num)
{
	unsigned int data;

	while (mv_read(phy, MV_GLOBAL1_PORT, 0x1d) & (1<<15))
		udelay(3);

	data = 0xc000 | ((port) << 5) | (num);
	mv_write(phy, MV_GLOBAL1_PORT, 0x1d, data);

	while (mv_read(phy, MV_GLOBAL1_PORT, 0x1d) & (1<<15))
		udelay(3);

	return mv_read(phy, MV_GLOBAL1_PORT, 0x1e) << 16 | mv_read(phy, MV_GLOBAL1_PORT, 0x1f);
}

static int mv_get_data(struct phy_device *phy, struct mv_data *data)
{
	if (data->is_write && data->regnum != 0xa11) {
		mv_write(phy, data->portaddr, data->regnum, (u16)(data->data[0]));
	} else {
		if (data->regnum == 0xa11) {
			data->data[0] = mv_get_count(phy, data->portaddr, 0);
			data->data[1] = mv_get_count(phy, data->portaddr, 1);
			data->data[2] = mv_get_count(phy, data->portaddr, 2);
			data->data[3] = mv_get_count(phy, data->portaddr, 0xe);
			data->data[4] = mv_get_count(phy, data->portaddr, 0xf);
		} else
			data->data[0] = mv_read(phy, data->portaddr, data->regnum);
	}
	return 0;
}

static long mv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;

	struct phy_device *phy = file->private_data;
	struct mv_data data;

	ret = copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;
	switch (cmd) {
	case MV_GET_CMD:
		ret = mv_get_data(phy, &data);
		break;
	default:
		return -EINVAL;
	}
	ret = copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;
	return ret;
}

static int mv_open(struct inode *inode, struct file *file)
{
	if (file->f_op == &mv_5050_fops)
		file->private_data = mv_phy[0];
	else
		file->private_data = mv_phy[1];
	return 0;
}

static const struct file_operations mv_5050_fops = {
	.owner = THIS_MODULE,
	.open = mv_open,
	.unlocked_ioctl = mv_ioctl,
};

static const struct file_operations mv_5072_fops = {
	.owner = THIS_MODULE,
	.open = mv_open,
	.unlocked_ioctl = mv_ioctl,
};

static struct miscdevice mv_5050 = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mv5050",
	.fops = &mv_5050_fops,
};

static struct miscdevice mv_5072 = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mv5072",
	.fops = &mv_5072_fops,
};

static int mv_probe(struct phy_device *phydev)
{
	struct miscdevice *mv_misc;
	int ret;

	if (phydev->phy_id == MV_5050_PHYID) {
		mv_misc = &mv_5050;
		mv_phy[0] = phydev;
	} else {
		mv_misc = &mv_5072;
		mv_phy[1] = phydev;
	}

	ret = misc_register(mv_misc);
	if (unlikely(ret)) {
		printk("\033[32m%s register mv switch fail\n", __func__);
		return -1;
	}

	printk("%s register %s successfully.\n", __func__, mv_misc->name);
	return 0;
}

static int mv_soft_reset(struct phy_device *phydev)
{
	return 0;
}

static int mv_config_init(struct phy_device *phydev)
{
	return 0;
}

static int mv_config_aneg(struct phy_device *phydev)
{
	phydev->supported = SUPPORTED_100baseT_Full;
	phydev->advertising = SUPPORTED_100baseT_Full;
	return 0;
}

static int mv_aneg_done(struct phy_device *phydev)
{
	return 1;
}

static int mv_read_status(struct phy_device *phydev)
{
	phydev->speed = SPEED_1000;
	phydev->link = 1;
	phydev->duplex = DUPLEX_FULL;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	return 0;
}

static struct phy_driver marvellauto_driver[] = { {
	.phy_id		= MV_5050_PHYID,
	.name		= "mv5050",
	.phy_id_mask	= 0xfffffff0,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= mv_soft_reset,
	.probe		= mv_probe,
	.config_init	= mv_config_init,
	.config_aneg	= mv_config_aneg,
	.aneg_done	= mv_aneg_done,
	.read_status	= mv_read_status,
}, {
	.phy_id		= MV_5072_PHYID,
	.name		= "mv5072",
	.phy_id_mask	= 0xfffffff0,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= mv_soft_reset,
	.probe		= mv_probe,
	.config_init	= mv_config_init,
	.config_aneg	= mv_config_aneg,
	.aneg_done	= mv_aneg_done,
	.read_status	= mv_read_status,
}
};

module_phy_driver(marvellauto_driver);

static struct mdio_device_id __maybe_unused marvellauto_tbl[] = {
	{ MV_5050_PHYID, 0xfffffff0 },
	{ MV_5072_PHYID, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, marvellauto_tbl);

MODULE_DESCRIPTION("Marvell Automotive switch driver");
MODULE_AUTHOR("Dejin Zheng");
MODULE_LICENSE("GPL");
