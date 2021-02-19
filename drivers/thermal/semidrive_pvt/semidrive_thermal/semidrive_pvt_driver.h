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

#ifndef SEMIDRIVE_PVT_DRIVER_H
#define SEMIDRIVE_PVT_DRIVER_H




#define SEMIDRIVE_PVT_SUPPORT_IRQ

enum alarm_irq_temp_type {
	THS_LOW_TEMP_ALARM = 0,
	THS_HIGH_TEMP_ALARM,
	VOL_TEMP_ALARM,
	PORC_TEMP_ALARM,
};


struct alarm_mode_enable {
	u32	reg;
	u8	shift;
};

struct pvt_irq_enable {
	u32	reg;
	u8	shift;
};

struct pvt_irq_status {
	u32	reg;
	u8	shift;
};


struct semidrive_pvt_hw_ctrl {
	struct 	alarm_mode_enable alarm_enable;
	struct	pvt_irq_enable	irq_enable;
	struct	pvt_irq_status	irq_status; 
};


struct semidrive_pvt_hw_sensor {
	u8	sensor_id;
	struct	semidrive_pvt_hw_ctrl	*alarm_h;
	struct	semidrive_pvt_hw_ctrl	*alarm_l;
	struct	semidrive_pvt_hw_ctrl	*alarm_r;
	struct	semidrive_pvt_hw_ctrl	*alarm_f;
};


#define SEMIDRIVE_PVT_HW_CTRL(name, alarm_enable_reg,alarm_enable_shift,irq_enable_reg,	\
		irq_enable_shift, irq_sta_reg, irq_sta_shift)\
static struct semidrive_pvt_hw_ctrl semidrive_pvt_hw_##name = { \
	.alarm_enable = {						\
		.reg = alarm_enable_reg,				\
		.shift =alarm_enable_shift,			\
	},							\
	.irq_enable = {						\
		.reg = irq_enable_reg,				\
		.shift = irq_enable_shift,			\
	},							\
	.irq_status = {						\
		.reg = irq_sta_reg,				\
		.shift = irq_sta_shift,				\
	},							\
}

SEMIDRIVE_PVT_HW_CTRL(alarm_h,PVT_CTRL_CTRL_MODE_SHEFT,16, PVT_REGISTER_INT_EN_OFFSET, 0, PVT_REGISTER_INT_STATUS_OFFSET, 0 );
SEMIDRIVE_PVT_HW_CTRL(alarm_l,PVT_CTRL_CTRL_MODE_SHEFT,17, PVT_REGISTER_INT_EN_OFFSET, 1, PVT_REGISTER_INT_STATUS_OFFSET, 1 );
SEMIDRIVE_PVT_HW_CTRL(alarm_r,PVT_CTRL_CTRL_MODE_SHEFT,18, PVT_REGISTER_INT_EN_OFFSET, 2, PVT_REGISTER_INT_STATUS_OFFSET,2 );
SEMIDRIVE_PVT_HW_CTRL(alarm_f, PVT_CTRL_CTRL_MODE_SHEFT,19,PVT_REGISTER_INT_EN_OFFSET, 3, PVT_REGISTER_INT_STATUS1_OFFSET,3 );

struct semidrive_pvt_hw_sensor pvt_hw_sensor[] = {
	{0, &semidrive_pvt_hw_alarm_h,  &semidrive_pvt_hw_alarm_l,&semidrive_pvt_hw_alarm_r, &semidrive_pvt_hw_alarm_f},
};


char *pvt_id_name_mapping[] = {
	"AP0",
};

#endif /* SEMIDRIVE_PVT_DRIVER_H */
