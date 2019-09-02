/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef GPIO_KUNLUN_H
#define GPIO_KUNLUN_H

struct kunlun_port_property {
	struct fwnode_handle *fwnode;
	unsigned int	idx;
	unsigned int	ngpio;
	unsigned int	gpio_base;
	unsigned int	irq;
	bool		irq_shared;
	unsigned int gpio_ranges[4];
};

struct kunlun_platform_data {
	struct kunlun_port_property *properties;
	unsigned int nports;
};

#endif
