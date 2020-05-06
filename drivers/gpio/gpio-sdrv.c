/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <linux/acpi.h>
#include <linux/gpio/driver.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/spinlock.h>
#include <linux/platform_data/gpio-sdrv.h>
#include <linux/slab.h>

#include "gpiolib.h"

/*  pin operation register */
#define GPIO_CTRL_PIN_0		0x00
#define GPIO_CTRL_PIN_1		0x10
#define GPIO_CTRL_SIZE		(GPIO_CTRL_PIN_1 - GPIO_CTRL_PIN_0)
#define GPIO_CTRL_PIN_X(n)    (((n)<=47) ? ((n)>=24 ? ((n)-24)*GPIO_CTRL_SIZE : ((n)+24)*GPIO_CTRL_SIZE) : ((n)*GPIO_CTRL_SIZE))
#define GPIO_SET_PIN_X(n)	(GPIO_CTRL_PIN_X(n) +  0x4)
#define GPIO_CLEAR_PIN_X(n)	(GPIO_CTRL_PIN_X(n) +  0x8)
#define GPIO_TOGGLE_PIN_X(n)	(GPIO_CTRL_PIN_X(n) +  0xc)

#define GPIO_CTRL_DIR_BIT				0
#define GPIO_CTRL_DATA_IN_BIT			1
#define GPIO_CTRL_DATA_OUT_BIT			2
#define GPIO_CTRL_INT_EN_BIT			3
#define GPIO_CTRL_INT_MASK_BIT			4
#define GPIO_CTRL_INT_TYPE_BIT			5
#define GPIO_CTRL_INT_POL_BIT			6
#define GPIO_CTRL_INT_BOTH_EDGE_BIT		7
#define GPIO_CTRL_INT_STATUS_BIT		8
#define GPIO_CTRL_INT_STATUS_UNMASK_BIT	9
#define GPIO_CTRL_INT_EDGE_CLR_BIT		10
#define GPIO_CTRL_INT_LEV_SYNC_BIT		11
#define GPIO_CTRL_INT_DEB_EN_BIT		12
#define GPIO_CTRL_INT_CLK_SEL_BIT		13
#define GPIO_CTRL_PCLK_ACTIVE_BIT		14

/* port operation registers */
#define GPIO_DIR_PORT_1		0x2000
#define GPIO_DIR_PORT_2		0x2010
#define GPIO_DIR_PORT_SIZE \
	(GPIO_DIR_PORT_2 - GPIO_DIR_PORT_1)
#define GPIO_DIR_PORT_X(n) \
	(GPIO_DIR_PORT_1 + ((n) * GPIO_DIR_PORT_SIZE))

#define GPIO_DATA_IN_PORT_1		0x2200
#define GPIO_DATA_IN_PORT_2		0x2210
#define GPIO_DATA_IN_PORT_SIZE \
	(GPIO_DATA_IN_PORT_2 - GPIO_DATA_IN_PORT_1)
#define GPIO_DATA_IN_PORT_X(n) \
	(GPIO_DATA_IN_PORT_1 + ((n) * GPIO_DATA_IN_PORT_SIZE))

#define GPIO_DATA_OUT_PORT_1		0x2400
#define GPIO_DATA_OUT_PORT_2		0x2410
#define GPIO_DATA_OUT_PORT_SIZE \
	(GPIO_DATA_OUT_PORT_2 - GPIO_DATA_OUT_PORT_1)
#define GPIO_DATA_OUT_PORT_X(n) \
	(GPIO_DATA_OUT_PORT_1 + ((n) * GPIO_DATA_OUT_PORT_SIZE))

#define GPIO_INT_EN_PORT_1		0x2600
#define GPIO_INT_EN_PORT_2		0x2610
#define GPIO_INT_EN_PORT_SIZE \
	(GPIO_INT_EN_PORT_2 - GPIO_INT_EN_PORT_1)
#define GPIO_INT_EN_PORT_X(n) \
	(GPIO_INT_EN_PORT_1 + ((n) * GPIO_INT_EN_PORT_SIZE))

#define GPIO_INT_MASK_PORT_1		0x2800
#define GPIO_INT_MASK_PORT_2		0x2810
#define GPIO_INT_MASK_PORT_SIZE \
	(GPIO_INT_MASK_PORT_2 - GPIO_INT_MASK_PORT_1)
#define GPIO_INT_MASK_PORT_X(n) \
	(GPIO_INT_MASK_PORT_1 + ((n) * GPIO_INT_MASK_PORT_SIZE))

#define GPIO_INT_TYPE_PORT_1		0x2a00
#define GPIO_INT_TYPE_PORT_2		0x2a10
#define GPIO_INT_TYPE_PORT_SIZE \
	(GPIO_INT_TYPE_PORT_2 - GPIO_INT_TYPE_PORT_1)
#define GPIO_INT_TYPE_PORT_X(n) \
	(GPIO_INT_TYPE_PORT_1 + ((n) * GPIO_INT_TYPE_PORT_SIZE))

#define GPIO_INT_POL_PORT_1		0x2c00
#define GPIO_INT_POL_PORT_2		0x2c10
#define GPIO_INT_POL_PORT_SIZE \
	(GPIO_INT_POL_PORT_2 - GPIO_INT_POL_PORT_1)
#define GPIO_INT_POL_PORT_X(n) \
	(GPIO_INT_POL_PORT_1 + ((n) * GPIO_INT_POL_PORT_SIZE))

#define GPIO_INT_BOTH_EDGE_PORT_1		0x2e00
#define GPIO_INT_BOTH_EDGE_PORT_2		0x2e10
#define GPIO_INT_BOTH_EDGE_PORT_SIZE \
	(GPIO_INT_BOTH_EDGE_PORT_2 - GPIO_INT_BOTH_EDGE_PORT_1)
#define GPIO_INT_BOTH_EDGE_PORT_X(n) \
	(GPIO_INT_BOTH_EDGE_PORT_1 + ((n) * GPIO_INT_BOTH_EDGE_PORT_SIZE))

#define GPIO_INT_STATUS_PORT_1		0x3000
#define GPIO_INT_STATUS_PORT_2		0x3010
#define GPIO_INT_STATUS_PORT_SIZE \
	(GPIO_INT_STATUS_PORT_2 - GPIO_INT_STATUS_PORT_1)
#define GPIO_INT_STATUS_PORT_X(n) \
	(GPIO_INT_STATUS_PORT_1 + ((n) * GPIO_INT_STATUS_PORT_SIZE))

#define GPIO_INT_STATUS_UNMASK_PORT_1		0x3200
#define GPIO_INT_STATUS_UNMASK_PORT_2		0x3210
#define GPIO_INT_STATUS_UNMASK_PORT_SIZE \
	(GPIO_INT_STATUS_UNMASK_PORT_2 - GPIO_INT_STATUS_UNMASK_PORT_1)
#define GPIO_INT_STATUS_UNMASK_PORT_X(n) \
	(GPIO_INT_STATUS_UNMASK_PORT_1 + \
	((n) * GPIO_INT_STATUS_UNMASK_PORT_SIZE))

#define GPIO_INT_EDGE_CLR_PORT_1		0x3400
#define GPIO_INT_EDGE_CLR_PORT_2		0x3410
#define GPIO_INT_EDGE_CLR_PORT_SIZE \
	(GPIO_INT_EDGE_CLR_PORT_2 - GPIO_INT_EDGE_CLR_PORT_1)
#define GPIO_INT_EDGE_CLR_PORT_X(n) \
	(GPIO_INT_EDGE_CLR_PORT_1 + ((n) * GPIO_INT_EDGE_CLR_PORT_SIZE))

#define GPIO_INT_LEV_SYNC_PORT_1		0x3600
#define GPIO_INT_LEV_SYNC_PORT_2		0x3610
#define GPIO_INT_LEV_SYNC_PORT_SIZE \
	(GPIO_INT_LEV_SYNC_PORT_2 - GPIO_INT_LEV_SYNC_PORT_1)
#define GPIO_INT_LEV_SYNC_PORT_X(n) \
	(GPIO_INT_LEV_SYNC_PORT_1 + ((n) * GPIO_INT_LEV_SYNC_PORT_SIZE))

#define GPIO_INT_DEB_EN_PORT_1		0x3800
#define GPIO_INT_DEB_EN_PORT_2		0x3810
#define GPIO_INT_DEB_EN_PORT_SIZE \
	(GPIO_INT_DEB_EN_PORT_2 - GPIO_INT_DEB_EN_PORT_1)
#define GPIO_INT_DEB_EN_PORT_X(n) \
	(GPIO_INT_DEB_EN_PORT_1 + ((n) * GPIO_INT_DEB_EN_PORT_SIZE))

#define GPIO_INT_CLK_SEL_PORT_1		0x3a00
#define GPIO_INT_CLK_SEL_PORT_2		0x3a10
#define GPIO_INT_CLK_SEL_PORT_SIZE \
	(GPIO_INT_CLK_SEL_PORT_2 - GPIO_INT_CLK_SEL_PORT_1)
#define GPIO_INT_CLK_SEL_PORT_X(n) \
	(GPIO_INT_CLK_SEL_PORT_1 + ((n) * GPIO_INT_CLK_SEL_PORT_SIZE))

#define GPIO_INT_PCLK_ACTIVE_PORT_1		0x3c00
#define GPIO_INT_PCLK_ACTIVE_PORT_2		0x3c10
#define GPIO_INT_PCLK_ACTIVE_PORT_SIZE \
	(GPIO_INT_PCLK_ACTIVE_PORT_2 - GPIO_INT_PCLK_ACTIVE_PORT_1)
#define GPIO_INT_PCLK_ACTIVE_PORT_X(n) \
	(GPIO_INT_PCLK_ACTIVE_PORT_1 + ((n) * GPIO_INT_PCLK_ACTIVE_PORT_SIZE))

#define SEMIDRIVE_MAX_PORTS		5

#define LAST(k, n) ((k) & ((1L<<(n))-1))
#define MID(k, m, n) LAST((k>>m), ((n)-(m)+1))
#define MIDMASK(m, n) ~(MID(0xffffffffu, m, n) << m)

#define APB_SCR_SAF_BASE (0x3c200000u)
#define APB_SCR_SAF_LEN	(0x200000)
#define APB_SCR_SEC_BASE (0x38200000u)
#define APB_SCR_SEC_LEN	(0x200000)

/* used for store Register Port index */
struct sdrv_gpio_port_idx {
	u32 irq;
	u32 port_idx;
	u32 hwirq_start; /* dts gpio_ranges[2] % 32 */
};

struct sdrv_gpio;

#ifdef CONFIG_PM_SLEEP
/* Store GPIO context across system-wide suspend/resume transitions */
struct sdrv_context {
	u32 data;
	u32 dir;
	u32 ext;
	u32 int_en;
	u32 int_mask;
	u32 int_type;
	u32 int_pol;
	u32 int_deb;
};
#endif

struct sdrv_gpio_port {
	struct gpio_chip	gc;
	bool			is_registered;
	struct sdrv_gpio	*gpio;
#ifdef CONFIG_PM_SLEEP
	struct sdrv_context	*ctx;
#endif
	unsigned int		idx;
	struct irq_domain	*domain;  /* each port has one domain */
};

struct sdrv_gpio {
	struct	device		*dev;
	void __iomem		*regs;
	struct sdrv_gpio_port	*ports;
	unsigned int		nr_ports;
	unsigned int		flags;

	struct sdrv_gpio_port_idx	k_port_idx[5];
};

static inline u32
gpio_reg_convert(struct sdrv_gpio *gpio, unsigned int offset)
{
	return offset;
}

static inline u32 sdrv_read(struct sdrv_gpio *gpio, unsigned int offset)
{
	struct gpio_chip *gc	= &gpio->ports[0].gc;
	void __iomem *reg_base	= gpio->regs;
	u32 val = 0;

	val = gc->read_reg(reg_base + gpio_reg_convert(gpio, offset));

	return val;
}

static inline void sdrv_write(struct sdrv_gpio *gpio, unsigned int offset,
			       u32 val)
{
	struct gpio_chip *gc	= &gpio->ports[0].gc;
	void __iomem *reg_base	= gpio->regs;

	gc->write_reg(reg_base + gpio_reg_convert(gpio, offset), val);

}

static int sdrv_gpio_to_irq(struct gpio_chip *gc, unsigned offset)
{
	struct sdrv_gpio_port *port = gpiochip_get_data(gc);
	int irq;

	irq = irq_find_mapping(port->domain, offset);

	return irq;
}

static u32 sdrv_do_irq(struct sdrv_gpio_port *port)
{
	unsigned int reg_idx = 0;
	unsigned int bit_idx = 0;
	unsigned int hwirq = 0;
	u32 irq_status = 0;
	u32 ret = irq_status;
	unsigned long reg_read;
	unsigned long reg_mask_read;
	struct sdrv_gpio *gpio = port->gpio;

	reg_idx = port->idx;

	irq_status = sdrv_read(gpio, GPIO_INT_STATUS_PORT_X(reg_idx));
	ret = irq_status;

	bit_idx = fls(irq_status) - 1;
	hwirq = bit_idx;

	/* mask the port INT */
	reg_mask_read = sdrv_read(gpio, GPIO_INT_MASK_PORT_X(reg_idx));
	sdrv_write(gpio,  GPIO_INT_MASK_PORT_X(reg_idx), 0xffffffff);

	/* clear INT */
	reg_read = sdrv_read(gpio, GPIO_INT_EDGE_CLR_PORT_X(reg_idx));
	sdrv_write(gpio,
		GPIO_INT_EDGE_CLR_PORT_X(reg_idx),
		(reg_read | BIT(bit_idx)));

	while (irq_status) {
		int irq_bit = fls(irq_status) - 1;
		int gpio_irq = irq_find_mapping(port->domain, irq_bit);

		generic_handle_irq(gpio_irq);
		irq_status &= ~BIT(irq_bit);
	}

	/* restore mask value */
	sdrv_write(gpio,  GPIO_INT_MASK_PORT_X(reg_idx), reg_mask_read);

	return ret;
}

static void sdrv_irq_handler(struct irq_desc *desc)
{
	struct sdrv_gpio_port *port = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);

	sdrv_do_irq(port);

	if (chip->irq_eoi)
		chip->irq_eoi(irq_desc_get_irq_data(desc));
}

static void sdrv_irq_enable(struct irq_data *d)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	struct gpio_chip *gc;
	unsigned long flags;
	u32 val;
	unsigned int reg_idx = 0;
	unsigned int bit_idx;
	unsigned long reg_read;

	reg_idx = port->idx;
	bit_idx = d->hwirq + gpio->k_port_idx[reg_idx - 1].hwirq_start;

	gc = &gpio->ports[reg_idx - 1].gc;

	spin_lock_irqsave(&gc->bgpio_lock, flags);

	val = sdrv_read(gpio, GPIO_INT_EN_PORT_X(reg_idx));
	val |= BIT(bit_idx);

	/* unmask */
	sdrv_write(gpio,
		GPIO_CLEAR_PIN_X((reg_idx * 32 + bit_idx)),
		BIT(GPIO_CTRL_INT_MASK_BIT));

	/* INT EN */
	sdrv_write(gpio,  GPIO_INT_EN_PORT_X(reg_idx), val);

	reg_read = sdrv_read(gpio, GPIO_CTRL_PIN_X((reg_idx * 32 + bit_idx)));

	spin_unlock_irqrestore(&gc->bgpio_lock, flags);
}

static void sdrv_irq_disable(struct irq_data *d)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	struct gpio_chip *gc;
	unsigned long flags;
	u32 val;
	unsigned int reg_idx = 0;
	unsigned int bit_idx;

	reg_idx = port->idx;
	bit_idx = d->hwirq + gpio->k_port_idx[reg_idx-1].hwirq_start;

	gc = &gpio->ports[reg_idx - 1].gc;


	spin_lock_irqsave(&gc->bgpio_lock, flags);
	val = sdrv_read(gpio, GPIO_INT_EN_PORT_X(reg_idx));
	val &= ~BIT(bit_idx);
	sdrv_write(gpio, GPIO_INT_EN_PORT_X(reg_idx), val);
	spin_unlock_irqrestore(&gc->bgpio_lock, flags);
}

static int sdrv_irq_reqres(struct irq_data *d)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	struct gpio_chip *gc;
	unsigned int reg_idx = 0;

	reg_idx = port->idx;
	gc = &gpio->ports[reg_idx - 1].gc;


	if (gpiochip_lock_as_irq(gc, irqd_to_hwirq(d))) {
		dev_err(gpio->dev, "unable to lock HW IRQ %lu for IRQ\n",
			irqd_to_hwirq(d));
		return -EINVAL;
	}
	return 0;
}

static void sdrv_irq_relres(struct irq_data *d)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	struct gpio_chip *gc;
	unsigned int reg_idx = 0;

	reg_idx = port->idx;
	gc = &gpio->ports[reg_idx - 1].gc;

	gpiochip_unlock_as_irq(gc, irqd_to_hwirq(d));
}

static int sdrv_irq_set_type(struct irq_data *d, u32 type)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	struct gpio_chip *gc;
	unsigned long flags;
	unsigned int reg_idx = 0;
	unsigned int bit_idx;

	reg_idx = port->idx;
	gc = &gpio->ports[reg_idx - 1].gc;


	if (type & ~(IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING |
		     IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_BOTH))
		return -EINVAL;

	spin_lock_irqsave(&gc->bgpio_lock, flags);

	bit_idx = d->hwirq + gpio->k_port_idx[reg_idx-1].hwirq_start;


	switch (type) {
	case IRQ_TYPE_EDGE_BOTH:
		sdrv_write(gpio, GPIO_SET_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_TYPE_BIT) |
			BIT(GPIO_CTRL_INT_BOTH_EDGE_BIT));
		break;
	case IRQ_TYPE_EDGE_RISING:
		sdrv_write(gpio, GPIO_SET_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_TYPE_BIT));
		sdrv_write(gpio, GPIO_SET_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_POL_BIT));
		break;
	case IRQ_TYPE_EDGE_FALLING:
		sdrv_write(gpio, GPIO_SET_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_TYPE_BIT));
		sdrv_write(gpio, GPIO_CLEAR_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_POL_BIT));
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		sdrv_write(gpio, GPIO_CLEAR_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_TYPE_BIT));
		sdrv_write(gpio, GPIO_SET_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_POL_BIT));
		break;
	case IRQ_TYPE_LEVEL_LOW:
		sdrv_write(gpio, GPIO_CLEAR_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_TYPE_BIT));
		sdrv_write(gpio, GPIO_CLEAR_PIN_X((reg_idx * 32 + bit_idx)),
			BIT(GPIO_CTRL_INT_POL_BIT));
		break;
	}

	irq_setup_alt_chip(d, type);

	spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	return 0;
}

static int sdrv_gpio_set_debounce(struct gpio_chip *gc,
				   unsigned offset, unsigned debounce)
{
	struct sdrv_gpio_port *port = gpiochip_get_data(gc);
	struct sdrv_gpio *gpio = port->gpio;
	unsigned long flags, val_deb;
	unsigned int reg_idx = port->idx;
	unsigned long mask = gc->pin2mask(gc,
		offset + gpio->k_port_idx[reg_idx - 1].hwirq_start);

	spin_lock_irqsave(&gc->bgpio_lock, flags);


	val_deb = sdrv_read(gpio, GPIO_INT_DEB_EN_PORT_X(reg_idx));
	if (debounce)
		sdrv_write(gpio,
			GPIO_INT_DEB_EN_PORT_X(reg_idx), val_deb | mask);
	else
		sdrv_write(gpio,
			GPIO_INT_DEB_EN_PORT_X(reg_idx), val_deb & ~mask);

	spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	return 0;
}

static int sdrv_gpio_set_config(struct gpio_chip *gc, unsigned offset,
				 unsigned long config)
{
	struct sdrv_gpio_port *port = gpiochip_get_data(gc);
	struct sdrv_gpio *gpio = port->gpio;
	u32 debounce;


	if (pinconf_to_config_param(config) != PIN_CONFIG_INPUT_DEBOUNCE) {
		dev_err(gpio->dev, "%s, Not support debounce!\n", __func__);
		return -ENOTSUPP;
	}

	debounce = pinconf_to_config_argument(config);
	return sdrv_gpio_set_debounce(gc, offset, debounce);
}

static irqreturn_t sdrv_irq_handler_mfd(int irq, void *dev_id)
{
	u32 worked = 0;
	struct sdrv_gpio_port *port = dev_id;

	worked = sdrv_do_irq(port);

	return worked ? IRQ_HANDLED : IRQ_NONE;
}

static void sdrv_configure_irqs(struct sdrv_gpio *gpio,
				 struct sdrv_gpio_port *port,
				 struct sdrv_port_property *pp)
{
	struct gpio_chip *gc = &port->gc;
	struct fwnode_handle  *fwnode = pp->fwnode;
	struct irq_chip_generic	*irq_gc = NULL;
	unsigned int hwirq, ngpio = gc->ngpio;
	struct irq_chip_type *ct;
	int err, i;

	port->domain = irq_domain_create_linear(fwnode, ngpio,
						 &irq_generic_chip_ops, gpio);
	if (!port->domain)
		return;

	err = irq_alloc_domain_generic_chips(port->domain, ngpio, 2,
					     "gpio-sdrv", handle_level_irq,
					     IRQ_NOREQUEST, 0,
					     IRQ_GC_INIT_NESTED_LOCK);
	if (err) {
		dev_info(gpio->dev, "irq_alloc_domain_generic_chips failed\n");
		irq_domain_remove(port->domain);
		port->domain = NULL;
		return;
	}

	irq_gc = irq_get_domain_generic_chip(port->domain, 0);
	if (!irq_gc) {
		irq_domain_remove(port->domain);
		port->domain = NULL;
		return;
	}

	irq_gc->reg_base = gpio->regs;
	irq_gc->private = port;

	for (i = 0; i < 2; i++) {
		ct = &irq_gc->chip_types[i];
		ct->chip.irq_ack = irq_gc_ack_set_bit;
		ct->chip.irq_mask = irq_gc_mask_set_bit;
		ct->chip.irq_unmask = irq_gc_mask_clr_bit;
		ct->chip.irq_set_type = sdrv_irq_set_type;
		ct->chip.irq_enable = sdrv_irq_enable;
		ct->chip.irq_disable = sdrv_irq_disable;
		ct->chip.irq_request_resources = sdrv_irq_reqres;
		ct->chip.irq_release_resources = sdrv_irq_relres;

		ct->regs.ack =
			gpio_reg_convert(gpio,
			GPIO_INT_EDGE_CLR_PORT_X(port->idx));
		ct->regs.mask =
			gpio_reg_convert(gpio, GPIO_INT_MASK_PORT_X(port->idx));
		ct->type = IRQ_TYPE_LEVEL_MASK;
	}

	irq_gc->chip_types[0].type = IRQ_TYPE_LEVEL_MASK;
	irq_gc->chip_types[1].type = IRQ_TYPE_EDGE_BOTH;
	irq_gc->chip_types[1].handler = handle_edge_irq;

	if (!pp->irq_shared) {
		dev_info(gpio->dev, "sdrv_gpio irq Not shared...\n");
		irq_set_chained_handler_and_data(pp->irq,
			sdrv_irq_handler, port);
	} else {
		/*
		 * Request a shared IRQ since where MFD would have devices
		 * using the same irq pin
		 */
		dev_info(gpio->dev, "sdrv_gpio irq shared...\n");
		err = devm_request_irq(gpio->dev, pp->irq,
			sdrv_irq_handler_mfd,
			IRQF_SHARED, "gpio-sdrv-mfd", port);
		if (err) {
			dev_err(gpio->dev, "error requesting IRQ\n");
			irq_domain_remove(port->domain);
			port->domain = NULL;
			return;
		}
	}

	for (hwirq = 0 ; hwirq < ngpio ; hwirq++)
		irq_create_mapping(port->domain, hwirq);

	port->gc.to_irq = sdrv_gpio_to_irq;
}

static void sdrv_irq_teardown(struct sdrv_gpio *gpio)
{
	struct sdrv_gpio_port *port = &gpio->ports[0];
	struct gpio_chip *gc = &port->gc;
	unsigned int ngpio = gc->ngpio;
	irq_hw_number_t hwirq;
	int idx = 0;


	for (idx = 0; idx < gpio->nr_ports; idx++) {
		dev_info(gpio->dev, "port idx[%d] removed!\n", idx);
		port = &gpio->ports[idx];
		gc = &port->gc;
		ngpio = gc->ngpio;

		if (!port->domain)
			return;

		for (hwirq = 0 ; hwirq < ngpio ; hwirq++)
			irq_dispose_mapping(
				irq_find_mapping(port->domain, hwirq));

		irq_domain_remove(port->domain);
		port->domain = NULL;
	}

}

static int sdrv_gpio_add_port(struct sdrv_gpio *gpio,
			       struct sdrv_port_property *pp,
			       unsigned int offs)
{
	struct sdrv_gpio_port *port;
	void __iomem *dat, *set, *dirout;
	int err;

	port = &gpio->ports[offs];
	port->gpio = gpio;
	port->idx = pp->idx;

#ifdef CONFIG_PM_SLEEP
	port->ctx = devm_kzalloc(gpio->dev, sizeof(*port->ctx), GFP_KERNEL);
	if (!port->ctx)
		return -ENOMEM;
#endif


	/* using dts "reg" for Register Port idx. */

	/* data: registers reflects the signals value on port */
	dat = gpio->regs + GPIO_DATA_IN_PORT_X(pp->idx);

	/* set: values written to this register are output on port */
	set = gpio->regs + GPIO_DATA_OUT_PORT_X(pp->idx);

	/* Data Direction */
	dirout = gpio->regs + GPIO_DIR_PORT_X(pp->idx);

	err = bgpio_init(&port->gc, gpio->dev, 4, dat, set, NULL, dirout,
			 NULL, false);
	if (err) {
		dev_err(gpio->dev, "failed to init gpio chip for port%d\n",
			port->idx);
		return err;
	}

#ifdef CONFIG_OF_GPIO
	port->gc.of_node = to_of_node(pp->fwnode);
#endif
	port->gc.ngpio = pp->ngpio;
	port->gc.base = pp->gpio_base;

	port->gc.set_config = sdrv_gpio_set_config;

	if (pp->irq)
		sdrv_configure_irqs(gpio, port, pp);

	/* pass reg bit offset to gpiochip */
	port->gc.offset = gpio->k_port_idx[offs].hwirq_start;
	err = gpiochip_add_data(&port->gc, port);
	if (err)
		dev_err(gpio->dev, "failed to register gpiochip for port%d\n",
			port->idx);
	else
		port->is_registered = true;

	return err;
}

static void sdrv_gpio_unregister(struct sdrv_gpio *gpio)
{
	unsigned int m;

	for (m = 0; m < gpio->nr_ports; ++m)
		if (gpio->ports[m].is_registered)
			gpiochip_remove(&gpio->ports[m].gc);
}

static struct sdrv_platform_data *
sdrv_gpio_get_pdata(struct device *dev, struct sdrv_gpio *gpio)
{
	struct fwnode_handle *fwnode;
	struct sdrv_platform_data *pdata;
	struct sdrv_port_property *pp;
	int nports;
	int i;

	nports = device_get_child_node_count(dev);
	if (nports == 0)
		return ERR_PTR(-ENODEV);


	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->properties = devm_kcalloc(dev, nports, sizeof(*pp), GFP_KERNEL);
	if (!pdata->properties)
		return ERR_PTR(-ENOMEM);

	pdata->nports = nports;

	i = 0;
	device_for_each_child_node(dev, fwnode)  {
		pp = &pdata->properties[i++];
		pp->fwnode = fwnode;

		if (fwnode_property_read_u32(fwnode, "reg", &pp->idx) ||
		    pp->idx >= SEMIDRIVE_MAX_PORTS) {
			dev_err(dev,
				"missing/invalid port index for port%d\n", i);
			fwnode_handle_put(fwnode);
			return ERR_PTR(-EINVAL);
		}

		if (fwnode_property_read_u32_array(fwnode, "gpio-ranges",
			pp->gpio_ranges, ARRAY_SIZE(pp->gpio_ranges))) {
			dev_err(dev,
				 "failed to get gpio-ranges for port%d\n", i);
			fwnode_handle_put(fwnode);
			return ERR_PTR(-EINVAL);
		} else {
			dev_info(dev,
				 "Got gpio-ranges[%d, %d, %d, %d] for port%d\n",
				pp->gpio_ranges[0], pp->gpio_ranges[1],
				pp->gpio_ranges[2], pp->gpio_ranges[3], i);
		}

		if (fwnode_property_read_u32(fwnode, "nr-gpios",
					 &pp->ngpio)) {
			dev_info(dev,
				 "failed to get number of gpios for port%d\n",
				 i);
			pp->ngpio = 32;
		}

		if (dev->of_node &&
			fwnode_property_read_bool(fwnode,
						  "interrupt-controller")) {
			pp->irq = irq_of_parse_and_map(to_of_node(fwnode), 0);
			if (!pp->irq)
				dev_warn(dev, "no irq for port%d\n", pp->idx);
			dev_err(dev, "sdrv_gpio: irq [%d]\n", pp->irq);
		}

		pp->irq_shared	= false;
		pp->gpio_base	= -1;

		/*
		 * dts "reg" is regsiter port index(0~4),
		 * and hwirq_start is offset in 32 bits reg
		 */
		gpio->k_port_idx[i-1].irq = pp->irq;
		gpio->k_port_idx[i-1].port_idx = i - 1;
		gpio->k_port_idx[i-1].hwirq_start = pp->gpio_ranges[2] % 32;
		dev_info(dev, "i[%d], irq[%u], port_idx[%u], hwirq_start[%u]\n",
			i, gpio->k_port_idx[i-1].irq,
			gpio->k_port_idx[i-1].port_idx,
			gpio->k_port_idx[i-1].hwirq_start);
	}

	return pdata;
}

static const struct of_device_id sdrv_of_match[] = {
	{ .compatible = "semidrive,sdrv-gpio", .data = (void *)0},
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, sdrv_of_match);

static int sdrv_gpio_probe(struct platform_device *pdev)
{
	unsigned int i;
	struct resource *res;
	struct sdrv_gpio *gpio;
	int err;
	struct device *dev = &pdev->dev;
	struct sdrv_platform_data *pdata = dev_get_platdata(dev);
	const struct of_device_id *of_devid;


	gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	if (!pdata) {
		pdata = sdrv_gpio_get_pdata(dev, gpio);
		if (IS_ERR(pdata)) {
			dev_err(dev, "sdrv_gpio sdrv_gpio_get_pdata error!\n");
			return PTR_ERR(pdata);
		}
	}

	if (!pdata->nports) {
		dev_err(dev, "sdrv_gpio No ports found!\n");
		return -ENODEV;
	}

	gpio->dev = &pdev->dev;
	gpio->nr_ports = pdata->nports;

	gpio->ports = devm_kcalloc(&pdev->dev, gpio->nr_ports,
				   sizeof(*gpio->ports), GFP_KERNEL);
	if (!gpio->ports)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	gpio->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gpio->regs)) {
		dev_err(dev, "sdrv_gpio regs devm_ioremap_resource failed !\n");
		return PTR_ERR(gpio->regs);
	}

	gpio->flags = 0;
	if (dev->of_node) {
		dev_info(dev, "sdrv_gpio of_node found !\n");

		of_devid = of_match_device(sdrv_of_match, dev);
		if (of_devid) {
			if (of_devid->data)
				gpio->flags = (uintptr_t)of_devid->data;
		}
	}

	for (i = 0; i < gpio->nr_ports; i++) {
		err = sdrv_gpio_add_port(gpio, &pdata->properties[i], i);
		if (err) {
			dev_err(dev, "sdrv_gpio sdrv_gpio_add_port failed !\n");
			goto out_unregister;
		}
	}
	platform_set_drvdata(pdev, gpio);

	return 0;

out_unregister:
	sdrv_gpio_unregister(gpio);
	sdrv_irq_teardown(gpio);

	return err;
}

static int sdrv_gpio_remove(struct platform_device *pdev)
{
	struct sdrv_gpio *gpio = platform_get_drvdata(pdev);

	sdrv_gpio_unregister(gpio);
	sdrv_irq_teardown(gpio);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sdrv_gpio_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdrv_gpio *gpio = platform_get_drvdata(pdev);
	struct gpio_chip *gc	= &gpio->ports[0].gc;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&gc->bgpio_lock, flags);
	for (i = 0; i < gpio->nr_ports; i++) {
		unsigned int offset;
		unsigned int idx = gpio->ports[i].idx;
		struct sdrv_context *ctx = gpio->ports[i].ctx;

		BUG_ON(!ctx);

		offset = GPIO_DIR_PORT_X(idx);
		ctx->dir = sdrv_read(gpio, offset);

		offset = GPIO_DATA_IN_PORT_X(idx);
		ctx->data = sdrv_read(gpio, offset);

		offset = GPIO_DATA_OUT_PORT_X(idx);
		ctx->ext = sdrv_read(gpio, offset);

		/* interrupts */
		ctx->int_mask = sdrv_read(gpio, GPIO_INT_MASK_PORT_X(idx));
		ctx->int_en	= sdrv_read(gpio, GPIO_INT_EN_PORT_X(idx));
		ctx->int_pol = sdrv_read(gpio, GPIO_INT_POL_PORT_X(idx));
		ctx->int_type = sdrv_read(gpio, GPIO_INT_TYPE_PORT_X(idx));
		ctx->int_deb = sdrv_read(gpio, GPIO_INT_DEB_EN_PORT_X(idx));

		/* Mask out interrupts */
		sdrv_write(gpio, GPIO_INT_MASK_PORT_X(idx), 0xffffffff);

	}
	spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	return 0;
}

static int sdrv_gpio_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdrv_gpio *gpio = platform_get_drvdata(pdev);
	struct gpio_chip *gc	= &gpio->ports[0].gc;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&gc->bgpio_lock, flags);
	for (i = 0; i < gpio->nr_ports; i++) {
		unsigned int offset;
		unsigned int idx = gpio->ports[i].idx;
		struct sdrv_context *ctx = gpio->ports[i].ctx;

		BUG_ON(!ctx);

		offset = GPIO_DATA_IN_PORT_X(idx);
		sdrv_write(gpio, offset, ctx->data);

		offset = GPIO_DIR_PORT_X(idx);
		sdrv_write(gpio, offset, ctx->dir);

		offset = GPIO_DATA_OUT_PORT_X(idx);
		sdrv_write(gpio, offset, ctx->ext);

		/* interrupts */
		sdrv_write(gpio, GPIO_INT_TYPE_PORT_X(idx), ctx->int_type);
		sdrv_write(gpio, GPIO_INT_POL_PORT_X(idx), ctx->int_pol);
		sdrv_write(gpio, GPIO_INT_DEB_EN_PORT_X(idx), ctx->int_deb);
		sdrv_write(gpio, GPIO_INT_EN_PORT_X(idx), ctx->int_en);
		sdrv_write(gpio, GPIO_INT_MASK_PORT_X(idx), ctx->int_mask);

		/* Clear out spurious interrupts */
		sdrv_write(gpio, GPIO_INT_EDGE_CLR_PORT_X(idx), 0xffffffff);

	}
	spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sdrv_gpio_pm_ops, sdrv_gpio_suspend,
			 sdrv_gpio_resume);

static struct platform_driver sdrv_gpio_driver = {
	.driver		= {
		.name	= "semidrive,sdrv-gpio",
		.pm	= &sdrv_gpio_pm_ops,
		.of_match_table = of_match_ptr(sdrv_of_match),
	},
	.probe		= sdrv_gpio_probe,
	.remove		= sdrv_gpio_remove,
};

module_platform_driver(sdrv_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Semidrive");
MODULE_DESCRIPTION("Semidrive GPIO driver");
