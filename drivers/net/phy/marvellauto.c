// SPDX-License-Identifier: GPL-2.0+
/**
 *  Driver for Marvell Automotive 88Q2112/88Q2110 Ethernet PHYs
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/delay.h>
#include <linux/phy.h>

static int ma211x_read(struct phy_device *phydev, u32 devaddr, u32 regaddr)
{
	regaddr = MII_ADDR_C45 | devaddr << MII_DEVADDR_C45_SHIFT | regaddr;
	return phy_read(phydev, regaddr);
}

static int ma211x_write(struct phy_device *phydev, u32 devaddr, u32 regaddr, u16 val)
{
	regaddr = MII_ADDR_C45 | devaddr << MII_DEVADDR_C45_SHIFT | regaddr;
	return phy_write(phydev, regaddr, val);
}

/* return 1 enable autonego, return 0 disable autonego */
static int ma211x_get_autonego_enable(struct phy_device *phydev)
{
	return (0x0 != (ma211x_read(phydev, 7, 0x0200) & 0x1000));
}

/* return 1 1000MB, return 0 100MB */
static int ma211x_get_speed(struct phy_device *phydev)
{
	if (ma211x_get_autonego_enable(phydev))
		return ((ma211x_read(phydev, 7, 0x801a) & 0x4000) >> 14);
	else
		return (ma211x_read(phydev, 1, 0x0834) & 0x0001);
}

static int ma211x_set_master(struct phy_device *phydev, int is_master)
{
	u16 data = ma211x_read(phydev, 1, 0x0834);

	if (is_master)
		data |= 0x4000;
	else
		data &= 0xBFFF;
	return ma211x_write(phydev, 1, 0x0834, data);
}

static int ma211x_check_link(struct phy_device *phydev)
{
	u16 data1, data2;
	u32 count = 10;

	while (count--) {
		data1 = ma211x_read(phydev, 3, 0x8109);
		data2 = ma211x_read(phydev, 3, 0x8108);
		if ((0x0 != (data1 & 0x0004)) && (0x0 != (data2 & 0x3000)))
			break;
		msleep(20);
	}

	if (count)
		return 0;
	else
		return -ETIMEDOUT;
}

static int ma211x_fe_init(struct phy_device *phydev)
{
	u16 data = 0;

	ma211x_set_master(phydev, 1);

	ma211x_write(phydev, 7, 0x0200, 0);
	ma211x_write(phydev, 3, 0xFA07, 0x0202);

	data = ma211x_read(phydev, 1, 0x0834);
	data = data & 0xFFF0;
	ma211x_write(phydev, 1, 0x0834, data);
	msleep(50);

	ma211x_write(phydev, 3, 0x8000, 0x0000);
	ma211x_write(phydev, 3, 0x8100, 0x0200);
	ma211x_write(phydev, 3, 0xFA1E, 0x0002);
	ma211x_write(phydev, 3, 0xFE5C, 0x2402);
	ma211x_write(phydev, 3, 0xFA12, 0x001F);
	ma211x_write(phydev, 3, 0xFA0C, 0x9E05);
	ma211x_write(phydev, 3, 0xFBDD, 0x6862);
	ma211x_write(phydev, 3, 0xFBDE, 0x736E);
	ma211x_write(phydev, 3, 0xFBDF, 0x7F79);
	ma211x_write(phydev, 3, 0xFBE0, 0x8A85);
	ma211x_write(phydev, 3, 0xFBE1, 0x9790);
	ma211x_write(phydev, 3, 0xFBE3, 0xA39D);
	ma211x_write(phydev, 3, 0xFBE4, 0xB0AA);
	ma211x_write(phydev, 3, 0xFBE5, 0x00B8);
	ma211x_write(phydev, 3, 0xFBFD, 0x0D0A);
	ma211x_write(phydev, 3, 0xFBFE, 0x0906);
	ma211x_write(phydev, 3, 0x801D, 0x8000);
	ma211x_write(phydev, 3, 0x8016, 0x0011);

	// reset pcs
	ma211x_write(phydev, 3, 0x0900, 0x8000);
	ma211x_write(phydev, 3, 0xFA07, 0x0200);
	msleep(100);

	ma211x_write(phydev, 31, 0x8001, 0xc000);
	// check link
	return ma211x_check_link(phydev);
}

static int ma211x_probe(struct phy_device *phydev)
{
	u32 id = 0;

	if(!phydev->is_c45)
		return -ENOTSUPP;
	id = ma211x_read(phydev, 1, 2) << 16 | ma211x_read(phydev, 1, 3);
	printk("%s get phy id[%x]\n", __func__, id);
	return 0;
}

static int ma211x_soft_reset(struct phy_device *phydev)
{
	return 0;
}

static int ma211x_config_init(struct phy_device *phydev)
{
	ma211x_fe_init(phydev);
	return 0;
}

static int ma211x_config_aneg(struct phy_device *phydev)
{
	phydev->supported = SUPPORTED_100baseT_Full;
	phydev->advertising = SUPPORTED_100baseT_Full;
	return 0;
}

static int ma211x_aneg_done(struct phy_device *phydev)
{
	return 1;
}

static int ma211x_read_status(struct phy_device *phydev)
{
	if (ma211x_get_speed(phydev))
		phydev->speed = SPEED_1000;
	else
		phydev->speed = SPEED_100;

	if (ma211x_check_link(phydev))
		phydev->link = 0;
	else
		phydev->link = 1;
	phydev->duplex = DUPLEX_FULL;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	return 0;
}

static struct phy_driver marvellauto_driver[] = { {
	.phy_id		= 0x002b0983,
	.name		= "ma211x",
	.phy_id_mask	= 0xfffffff0,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= ma211x_soft_reset,
	.probe		= ma211x_probe,
	.config_init	= ma211x_config_init,
	.config_aneg	= ma211x_config_aneg,
	.aneg_done	= ma211x_aneg_done,
	.read_status	= ma211x_read_status,
} };

module_phy_driver(marvellauto_driver);

static struct mdio_device_id __maybe_unused marvellauto_tbl[] = {
	{ 0x002b0938, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, marvellauto_tbl);

MODULE_DESCRIPTION("Marvell Automotive 88Q2112/88Q2110 PHY driver");
MODULE_AUTHOR("Dejin Zheng");
MODULE_LICENSE("GPL");
