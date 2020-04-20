/*
 * PCIe host controller driver for Kunlun
 *
 *
 *
 *
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/compiler.h>
#include <linux/clk.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include <linux/types.h>

#include "pcie-designware.h"

#define to_kunlun_pcie(x) dev_get_drvdata((x)->dev)

#define KUNLUN_PCIE_ATU_OFFSET (0xc0000)

/* PCIe PHY NCR registers */
#define PCIE_PHY_NCR_CTRL0 (0x0)
#define PCIE_PHY_NCR_CTRL1 (0x4)
#define PCIE_PHY_NCR_CTRL4 (0x10)
#define PCIE_PHY_NCR_CTRL15 (0x3c)
#define PCIE_PHY_NCR_CTRL25 (0x64)
#define PCIE_PHY_NCR_STS0 (0x80)

/* info in PCIe PHY NCR registers */
#define CR_ADDR_MODE_BIT (0x1 << 29)
#define BIF_EN_BIT (0x1 << 1)
#define PHY_REF_USE_PAD_BIT (0x1 << 5)
#define PHY_REF_ALT_CLK_SEL_BIT (0x1 << 8)

#define PHY_RESET_BIT (0x1 << 0)
#define CR_CKEN_BIT (0x1 << 24)

/* PCIe CTRL NCR registers */
#define PCIE_CTRL_NCR_INTR0 (0x0)
#define PCIE_CTRL_NCR_INTEN0 (0x34)
#define PCIE_CTRL_NCR_INTEN1 (0x38)
#define PCIE_CTRL_NCR_INTEN2 (0x3c)
#define PCIE_CTRL_NCR_INTEN3 (0x40)
#define PCIE_CTRL_NCR_INTEN4 (0x44)
#define PCIE_CTRL_NCR_INTEN5 (0x48)
#define PCIE_CTRL_NCR_INTEN6 (0x4c)
#define PCIE_CTRL_NCR_INTEN7 (0x50)
#define PCIE_CTRL_NCR_INTEN8 (0x54)
#define PCIE_CTRL_NCR_INTEN9 (0x58)
#define PCIE_CTRL_NCR_INTEN10 (0x5c)
#define PCIE_CTRL_NCR_INTEN11 (0x60)
#define PCIE_CTRL_NCR_INTEN12 (0x64)
#define PCIE_CTRL_NCR_CTRL0 (0x68)
#define PCIE_CTRL_NCR_CTRL2 (0x70)
#define PCIE_CTRL_NCR_CTRL21 (0xbc)
#define PCIE_CTRL_NCR_CTRL22 (0xc0)
#define PCIE_CTRL_NCR_CTRL23 (0xc4)
#define PCIE_CTRL_NCR_CTRL24 (0xc8)

/* info in PCIe CTRL NCR registers */
#define APP_LTSSM_EN_BIT (0x1 << 2)
#define DEVICE_TYPE_BIT (0x1 << 3)
#define APP_HOLD_PHY_RST_BIT (0x1 << 6)
#define INTR_SMLH_LINK_UP (0x1 << 28)
#define INTR_RDLH_LINK_UP (0x1 << 27)

/* Time for delay */
#define TIME_PHY_RST_MIN (5)
#define TIME_PHY_RST_MAX (10)
#define LINK_WAIT_MIN (900)
#define LINK_WAIT_MAX (1000)

#define NO_PCIE (0)
#define PCIE1_INDEX (1)
#define PCIE2_INDEX (2)

#define PHY_REFCLK_USE_INTERNAL (0x1)
#define PHY_REFCLK_USE_EXTERNAL (0x2)
#define PHY_REFCLK_USE_DIFFBUF (0x3)
#define PHY_REFCLK_USE_MASK (0x3)
#define DIFFBUF_OUT_EN (0x1 << 31)

#define LANE_CFG(lane1, lane0) (lane0 | lane1 << 16)
#define GET_LANE0_CFG(val) (val & 0xffff)
#define GET_LANE1_CFG(val) (val >> 16)

enum kunlun_pcie_lane_cfg {
	PCIE1_WITH_LANE0 = LANE_CFG(NO_PCIE, PCIE1_INDEX),
	PCIE1_WITH_LANE1 = LANE_CFG(PCIE1_INDEX, NO_PCIE),
	PCIE1_WITH_ALL_LANES = LANE_CFG(PCIE1_INDEX, PCIE1_INDEX),
	PCIE2_WITH_LANE1 = LANE_CFG(PCIE2_INDEX, NO_PCIE),
	PCIE1_2_EACH_ONE_LANE = LANE_CFG(PCIE2_INDEX, PCIE1_INDEX),
};

struct kunlun_pcie {
	struct dw_pcie *pcie;
	struct clk *pcie_aclk;
	struct clk *pcie_pclk;
	struct clk *phy_ref;
	struct clk *phy_pclk;
	void __iomem *phy_base;
	void __iomem *phy_ncr_base;
	void __iomem *ctrl_ncr_base;
	enum kunlun_pcie_lane_cfg lane_config;
	u32 phy_refclk_sel;
};

static inline void kunlun_apb_phy_writel(struct kunlun_pcie *kunlun_pcie,
					 u32 val, u32 reg)
{
	writel(val, kunlun_pcie->phy_base + reg);
}

static inline u32 kunlun_apb_phy_readl(struct kunlun_pcie *kunlun_pcie, u32 reg)
{
	return readl(kunlun_pcie->phy_base + reg);
}

static inline void kunlun_phy_ncr_writel(struct kunlun_pcie *kunlun_pcie,
					 u32 val, u32 reg)
{
	writel(val, kunlun_pcie->phy_ncr_base + reg);
}

static inline u32 kunlun_phy_ncr_readl(struct kunlun_pcie *kunlun_pcie, u32 reg)
{
	return readl(kunlun_pcie->phy_ncr_base + reg);
}

static inline void kunlun_ctrl_ncr_writel(struct kunlun_pcie *kunlun_pcie,
					  u32 val, u32 reg)
{
	writel(val, kunlun_pcie->ctrl_ncr_base + reg);
}

static inline u32 kunlun_ctrl_ncr_readl(struct kunlun_pcie *kunlun_pcie,
					u32 reg)
{
	return readl(kunlun_pcie->ctrl_ncr_base + reg);
}

static int kunlun_pcie_get_clk(struct kunlun_pcie *kunlun_pcie,
			       struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	kunlun_pcie->pcie_aclk = devm_clk_get(dev, "pcie_aclk");
	if (IS_ERR(kunlun_pcie->pcie_aclk))
		return PTR_ERR(kunlun_pcie->pcie_aclk);

	kunlun_pcie->pcie_pclk = devm_clk_get(dev, "pcie_pclk");
	if (IS_ERR(kunlun_pcie->pcie_pclk))
		return PTR_ERR(kunlun_pcie->pcie_pclk);

	kunlun_pcie->phy_ref = devm_clk_get(dev, "pcie_phy_ref");
	if (IS_ERR(kunlun_pcie->phy_ref))
		return PTR_ERR(kunlun_pcie->phy_ref);

	kunlun_pcie->phy_pclk = devm_clk_get(dev, "pcie_phy_pclk");
	if (IS_ERR(kunlun_pcie->phy_pclk))
		return PTR_ERR(kunlun_pcie->phy_pclk);

	return 0;
}

static int kunlun_pcie_get_resource(struct kunlun_pcie *kunlun_pcie,
				    struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *ctrl_ncr;
	struct resource *phy;
	struct resource *phy_ncr;
	struct resource *dbi;

	ctrl_ncr = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				      "ctrl_ncr");
	kunlun_pcie->ctrl_ncr_base = devm_ioremap_resource(dev, ctrl_ncr);
	if (IS_ERR(kunlun_pcie->ctrl_ncr_base))
		return PTR_ERR(kunlun_pcie->ctrl_ncr_base);

	phy = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	kunlun_pcie->phy_base = devm_ioremap_resource(dev, phy);
	if (IS_ERR(kunlun_pcie->phy_base))
		return PTR_ERR(kunlun_pcie->phy_base);

	dbi = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	kunlun_pcie->pcie->dbi_base = devm_ioremap_resource(dev, dbi);
	if (IS_ERR(kunlun_pcie->pcie->dbi_base))
		return PTR_ERR(kunlun_pcie->pcie->dbi_base);

	kunlun_pcie->pcie->atu_base =
	    kunlun_pcie->pcie->dbi_base + KUNLUN_PCIE_ATU_OFFSET;

	phy_ncr = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy_ncr");
	kunlun_pcie->phy_ncr_base = devm_ioremap_resource(dev, phy_ncr);
	if (IS_ERR(kunlun_pcie->phy_ncr_base))
		return PTR_ERR(kunlun_pcie->phy_ncr_base);

	return 0;
}

void kunlun_pcie_phy_refclk_sel(struct kunlun_pcie *kunlun_pcie)
{
	u32 reg_val;

	u32 flag = kunlun_pcie->phy_refclk_sel & PHY_REFCLK_USE_MASK;
	bool diffbuf_out_en = !!(kunlun_pcie->phy_refclk_sel >> 31);

	if ((flag == PHY_REFCLK_USE_DIFFBUF) && diffbuf_out_en) {
		pr_info("[ERROR] wrong ref clk cfg\n");
		BUG();
	}

	if (flag == PHY_REFCLK_USE_INTERNAL) {
		pr_info("using internel ref clk\n");
		reg_val = kunlun_phy_ncr_readl(kunlun_pcie, PCIE_PHY_NCR_CTRL1);
		reg_val &= ~PHY_REF_USE_PAD_BIT;
		reg_val &= ~PHY_REF_ALT_CLK_SEL_BIT;
		kunlun_phy_ncr_writel(kunlun_pcie, reg_val, PCIE_PHY_NCR_CTRL1);
	} else if (flag == PHY_REFCLK_USE_EXTERNAL) {
		pr_info("using externel ref clk\n");
		reg_val = kunlun_phy_ncr_readl(kunlun_pcie, PCIE_PHY_NCR_CTRL1);
		reg_val |= PHY_REF_USE_PAD_BIT;
		kunlun_phy_ncr_writel(kunlun_pcie, reg_val, PCIE_PHY_NCR_CTRL1);
	} else if (flag == PHY_REFCLK_USE_DIFFBUF) {
		pr_info("using diffbuf_extref_clk\n");
		reg_val = kunlun_phy_ncr_readl(kunlun_pcie, PCIE_PHY_NCR_CTRL1);
		reg_val &= ~(PHY_REF_USE_PAD_BIT);
		reg_val |= PHY_REF_ALT_CLK_SEL_BIT;
		kunlun_phy_ncr_writel(kunlun_pcie, reg_val, PCIE_PHY_NCR_CTRL1);

		reg_val = kunlun_phy_ncr_readl(kunlun_pcie,
				       PCIE_PHY_NCR_CTRL15);
		reg_val &= ~0x1;
		kunlun_phy_ncr_writel(kunlun_pcie, reg_val,
				       PCIE_PHY_NCR_CTRL15);
	} else {
		pr_info("error phy_refclk_sel\n");
		BUG();
	}

	if (diffbuf_out_en) {
		pr_info("diffbuf out enable\n");
		reg_val = kunlun_phy_ncr_readl(kunlun_pcie,
				      PCIE_PHY_NCR_CTRL15);
		reg_val |= 0x1;
		kunlun_phy_ncr_writel(kunlun_pcie, reg_val,
				      PCIE_PHY_NCR_CTRL15);
	}
}

static void kunlun_pcie_lane_config(struct kunlun_pcie *kunlun_pcie)
{
	u32 lane0 = GET_LANE0_CFG(kunlun_pcie->lane_config);
	u32 lane1 = GET_LANE1_CFG(kunlun_pcie->lane_config);

	u32 reg_val = 0;

	reg_val = kunlun_phy_ncr_readl(kunlun_pcie, PCIE_PHY_NCR_CTRL0);
	if (lane1 == PCIE2_INDEX)
		reg_val |= BIF_EN_BIT;
	else
		reg_val &= ~BIF_EN_BIT;
	kunlun_phy_ncr_writel(kunlun_pcie, reg_val, PCIE_PHY_NCR_CTRL0);

	reg_val = 0;

	if (lane0 == NO_PCIE)
		reg_val |= 0xf2;
	if (lane1 == NO_PCIE)
		reg_val |= 0xf20000;
	kunlun_phy_ncr_writel(kunlun_pcie, reg_val, PCIE_PHY_NCR_CTRL25);
}

static int kunlun_pcie_phy_init(struct kunlun_pcie *kunlun_pcie)
{
	u32 reg_val;

	kunlun_pcie_phy_refclk_sel(kunlun_pcie);

	reg_val = kunlun_phy_ncr_readl(kunlun_pcie, PCIE_PHY_NCR_CTRL0);
	reg_val |= CR_CKEN_BIT;
	kunlun_phy_ncr_writel(kunlun_pcie, reg_val, PCIE_PHY_NCR_CTRL0);

	reg_val = kunlun_phy_ncr_readl(kunlun_pcie, PCIE_PHY_NCR_CTRL4);
	reg_val &= ~CR_ADDR_MODE_BIT;
	kunlun_phy_ncr_writel(kunlun_pcie, reg_val, PCIE_PHY_NCR_CTRL4);

	kunlun_pcie_lane_config(kunlun_pcie);

	reg_val = kunlun_phy_ncr_readl(kunlun_pcie, PCIE_PHY_NCR_CTRL0);
	reg_val &= ~PHY_RESET_BIT;
	kunlun_phy_ncr_writel(kunlun_pcie, reg_val, PCIE_PHY_NCR_CTRL0);

	usleep_range(TIME_PHY_RST_MIN, TIME_PHY_RST_MAX);

	return 0;
}

static int kunlun_pcie_clk_ctrl(struct kunlun_pcie *kunlun_pcie, bool enable)
{
	int ret = 0;

	if (!enable)
		goto disable_clk;

	ret = clk_prepare_enable(kunlun_pcie->phy_ref);
	if (ret)
		return ret;

	ret = clk_prepare_enable(kunlun_pcie->phy_pclk);
	if (ret)
		goto phy_pclk_fail;

	ret = clk_prepare_enable(kunlun_pcie->pcie_aclk);
	if (ret)
		goto pcie_aclk_fail;

	ret = clk_prepare_enable(kunlun_pcie->pcie_pclk);
	if (ret)
		goto pcie_pclk_fail;

	return 0;

disable_clk:
	clk_disable_unprepare(kunlun_pcie->pcie_pclk);

pcie_pclk_fail:
	clk_disable_unprepare(kunlun_pcie->pcie_aclk);

pcie_aclk_fail:
	clk_disable_unprepare(kunlun_pcie->phy_pclk);

phy_pclk_fail:
	clk_disable_unprepare(kunlun_pcie->phy_ref);

	return ret;
}

static int kunlun_pcie_power_on(struct kunlun_pcie *kunlun_pcie)
{
	int ret;

	ret = kunlun_pcie_clk_ctrl(kunlun_pcie, true);
	if (ret)
		goto close_clk;

	kunlun_pcie_phy_init(kunlun_pcie);

	return 0;

close_clk:
	kunlun_pcie_clk_ctrl(kunlun_pcie, false);
	return ret;
}

static int kunlun_pcie_link_up(struct dw_pcie *pcie)
{
	struct kunlun_pcie *kunlun_pcie = to_kunlun_pcie(pcie);

	u32 reg_val = kunlun_ctrl_ncr_readl(kunlun_pcie, PCIE_CTRL_NCR_INTR0);

	if ((reg_val & INTR_SMLH_LINK_UP) != INTR_SMLH_LINK_UP)
		return 0;

	if ((reg_val & INTR_RDLH_LINK_UP) != INTR_RDLH_LINK_UP)
		return 0;

	return 1;
}

static void kunlun_pcie_core_init(struct kunlun_pcie *kunlun_pcie)
{
	u32 reg_val;

	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN0);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN1);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN2);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN3);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN4);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN5);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN6);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN7);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN8);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN9);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN10);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN11);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_INTEN12);

	reg_val = kunlun_ctrl_ncr_readl(kunlun_pcie, PCIE_CTRL_NCR_CTRL0);
	reg_val &= ~APP_HOLD_PHY_RST_BIT;
	kunlun_ctrl_ncr_writel(kunlun_pcie, reg_val, PCIE_CTRL_NCR_CTRL0);

	reg_val = kunlun_ctrl_ncr_readl(kunlun_pcie, PCIE_CTRL_NCR_CTRL0);
	reg_val |= DEVICE_TYPE_BIT;
	kunlun_ctrl_ncr_writel(kunlun_pcie, reg_val, PCIE_CTRL_NCR_CTRL0);

	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_CTRL21);
	kunlun_ctrl_ncr_writel(kunlun_pcie, 0, PCIE_CTRL_NCR_CTRL23);
}

static int kunlun_pcie_establish_link(struct pcie_port *pp)
{
	struct dw_pcie *pcie = to_dw_pcie_from_pp(pp);
	struct kunlun_pcie *kunlun_pcie = to_kunlun_pcie(pcie);
	struct device *dev = kunlun_pcie->pcie->dev;
	u32 reg_val;
	int count = 0;

	if (kunlun_pcie_link_up(pcie))
		return 0;

	dw_pcie_setup_rc(pp);

	reg_val = kunlun_ctrl_ncr_readl(kunlun_pcie, PCIE_CTRL_NCR_CTRL0);
	reg_val |= APP_LTSSM_EN_BIT;
	kunlun_ctrl_ncr_writel(kunlun_pcie, reg_val, PCIE_CTRL_NCR_CTRL0);

	/* check if the link is up or not */
	while (!kunlun_pcie_link_up(pcie)) {
		usleep_range(LINK_WAIT_MIN, LINK_WAIT_MAX);
		count++;
		if (count == 1000) {
			dev_err(dev, "Link Fail\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int kunlun_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pcie = to_dw_pcie_from_pp(pp);
	struct kunlun_pcie *kunlun_pcie = to_kunlun_pcie(pcie);

	kunlun_pcie_core_init(kunlun_pcie);

	kunlun_pcie_establish_link(pp);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		dw_pcie_msi_init(pp);

	return 0;
}

static struct dw_pcie_ops kunlun_dw_pcie_ops = {
	.link_up = kunlun_pcie_link_up,
};

static const struct dw_pcie_host_ops kunlun_pcie_host_ops = {
	.host_init = kunlun_pcie_host_init,
};

static irqreturn_t kunlun_pcie_msi_handler(int irq, void *arg)
{
	struct kunlun_pcie *kunlun_pcie = arg;
	struct dw_pcie *pcie = kunlun_pcie->pcie;
	struct pcie_port *pp = &pcie->pp;

	return dw_handle_msi_irq(pp);
}

static int __init kunlun_add_pcie_port(struct dw_pcie *pcie,
				       struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct pcie_port *pp = &(pcie->pp);
	struct kunlun_pcie *kunlun_pcie = to_kunlun_pcie(pcie);

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq_byname(pdev, "msi");
		if (pp->msi_irq <= 0) {
			dev_err(dev, "failed to get MSI irq\n");
			return -ENODEV;
		}

		ret = devm_request_irq(dev, pp->msi_irq,
				     kunlun_pcie_msi_handler,
				     IRQF_SHARED | IRQF_NO_THREAD,
				     "kunlun-pcie-msi", kunlun_pcie);
		if (ret) {
			dev_err(dev, "failed to request MSI irq\n");
			return ret;
		}
	}

	pp->ops = &kunlun_pcie_host_ops;

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(dev, "failed to initialize host\n");
		return ret;
	}

	return 0;
}

static int kunlun_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct kunlun_pcie *kunlun_pcie;
	struct dw_pcie *pcie;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "NULL node\n");
		return -EINVAL;
	}

	kunlun_pcie = devm_kzalloc(dev, sizeof(struct kunlun_pcie), GFP_KERNEL);
	if (!kunlun_pcie)
		return -ENOMEM;

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pcie->dev = dev;
	pcie->ops = &kunlun_dw_pcie_ops;
	kunlun_pcie->pcie = pcie;
	kunlun_pcie->lane_config = PCIE1_2_EACH_ONE_LANE;
	kunlun_pcie->phy_refclk_sel = PHY_REFCLK_USE_EXTERNAL | DIFFBUF_OUT_EN;

	ret = kunlun_pcie_get_clk(kunlun_pcie, pdev);
	if (ret)
		return ret;

	ret = kunlun_pcie_get_resource(kunlun_pcie, pdev);
	if (ret)
		return ret;

	ret = kunlun_pcie_power_on(kunlun_pcie);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, kunlun_pcie);

	ret = kunlun_add_pcie_port(kunlun_pcie->pcie, pdev);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct of_device_id kunlun_pcie_match[] = {
	{.compatible = "semidrive,kunlun-pcie-1"},
	{.compatible = "semidrive,kunlun-pcie-2"},
	{},
};

static struct platform_driver kunlun_pcie_driver = {
	.probe = kunlun_pcie_probe,
	.driver = {
		.name = "kunlun-pcie",
		.of_match_table = kunlun_pcie_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(kunlun_pcie_driver);
