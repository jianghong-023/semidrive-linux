/*
 * 
 *
 * Copyright (c) 2021 Semidrive Semiconductor.
 * All rights reserved.
 *
 * Description: PVT
 *
 * Revision History:
 * -----------------
 */
#ifndef SEMIDRIVE_PVT_H
#define SEMIDRIVE_PVT_H

#define MAX__PVT_CHN (4)

#define SEMIDRIVE_PVT_NAME "pvt_controller"
#define SEMIDRIVE_PVT_COMBINE_NAME "pvt_combine"

struct semidrive_pvt_controller;

enum combine_pvt_sensor_temp_type {
	COMBINE_MAX_TEMP = 0,
	COMBINE_AVG_TMP,
	COMBINE_MIN_TMP,
};

struct semidrive_pvt_controller_ops {
	int (*suspend)(struct semidrive_pvt_controller *);
	int (*resume)(struct semidrive_pvt_controller *);
	int (*get_temp)(struct semidrive_pvt_controller *, u32 id, int *temp);
};

struct semidrive_pvt_controller {
	struct device *dev;
	struct semidrive_pvt_controller_ops *ops;
	atomic_t initialize;
	atomic_t usage;
	atomic_t is_suspend;
	struct mutex lock;
	struct list_head combine_list;
	struct list_head node;
	int combine_num;
	void *data;				/*store the point of pvt_data */
};

struct semidrive_pvt_combine_disc {
	u32  combine_ths_count;
	enum combine_pvt_sensor_temp_type type;
	const char *combine_ths_type;
	u32 combine_ths_id[MAX__PVT_CHN];
	struct semidrive_pvt_controller *controller;
};

struct semidrive_pvt_sensor {
	struct platform_device *pdev;
	u32 sensor_id;			/* the pvt combine sensor id */;
	int last_temp;
	struct semidrive_pvt_combine_disc *combine;
	struct thermal_zone_device *tz;
	atomic_t is_suspend;
	struct list_head node;
};

extern struct semidrive_pvt_controller *
	semidrive_pvt_controller_register(struct device *dev ,struct semidrive_pvt_controller_ops *ops, void *data);
extern void semidrive_pvt_controller_unregister(struct semidrive_pvt_controller *controller);

#endif
