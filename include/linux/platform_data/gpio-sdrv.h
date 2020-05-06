/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef GPIO_SDRV_H
#define GPIO_SDRV_H

struct sdrv_port_property {
	struct fwnode_handle *fwnode;
	unsigned int	idx;
	unsigned int	ngpio;
	unsigned int	gpio_base;
	unsigned int	irq;
	bool		irq_shared;
	unsigned int gpio_ranges[4];
};

struct sdrv_platform_data {
	struct sdrv_port_property *properties;
	unsigned int nports;
};

#endif
