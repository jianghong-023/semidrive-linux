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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/thermal.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "semidrive_pvt.h"
#include "semidrive_pvt_core.h"
#include "semidrive_pvt_driver.h"

static struct semidrive_pvt_controller *main_ctrl;

/**
 *PVT Temp to dout
 */

static u32 pvt_temp_to_dout(u32 temp_value){
	int i = 0;
	float temp_data = 1;
	float a4_t[5];
	float final_data = 0;
	a4_t[4] = 1.2569E-08;
	a4_t[3] = 2.4759E-05;
	a4_t[2] = 6.6309E-03;
	a4_t[1] = 3.6384E+00;
	a4_t[0] = 2.0705E+02;
	// y = a4*x^4+a3*x^3+a2*x^2+a1*x^1+a0
	for (i = 0; i < 5; i++) {
		final_data += a4_t[i] * temp_data;
		temp_data = temp_value * temp_data;
	}
	return (u32)final_data;
}

static void semidrive_pvt_reg_init(struct semidrive_pvt_data *ths_data)
{

	pvt_driver_init_reg(ths_data);
	return;
}

static void semidrive_pvt_exit(struct semidrive_pvt_data *ths_data)
{
	pvt_driver_disable_reg(ths_data);
	return;
}


#ifdef SEMIDRIVE_PVT_SUPPORT_IRQ
static void semidrive_pvt_set_alarm_threshold_temp(struct semidrive_pvt_data *ths_data, u32 id)
{
	u32 dout_h_temp = 0;
	u32 dout_l_temp = 0;
	dout_h_temp = pvt_temp_to_dout(ths_data->alarm_high_temp);
	dout_l_temp = pvt_temp_to_dout(ths_data->alarm_low_temp);
	dout_h_temp = (dout_h_temp&0x3ff)|((dout_l_temp&0x3ff)<<10);
	writel(dout_h_temp, ths_data->base_addr+PVT_REGISTER_HYST_L_OFFSET);
}
static void semidrive_pvt_clr_alarm_irq_pending(struct semidrive_pvt_data *ths_data,
									u32 id)
{
	u32 reg;
	reg = readl(ths_data->base_addr +
			pvt_hw_sensor[id].alarm_h->irq_status.reg);
	reg = SET_BITS(pvt_hw_sensor[id].alarm_h->irq_status.shift, 1, reg, 0x1);
	writel(reg, ths_data->base_addr +
			pvt_hw_sensor[id].alarm_h->irq_status.reg);
	reg = readl(ths_data->base_addr +
			pvt_hw_sensor[id].alarm_l->irq_status.reg);
	reg = SET_BITS(pvt_hw_sensor[id].alarm_l->irq_status.shift, 1,
								reg, 0x1);
	writel(reg, ths_data->base_addr +
			pvt_hw_sensor[id].alarm_l->irq_status.reg);
}

static void semidrive_pvt_disable_alarm_irq(struct semidrive_pvt_data *ths_data, u32 id)
{
	u32 reg;
	struct thermal_sensor_info *sensor_info =
		(struct thermal_sensor_info *)ths_data->data;
	reg = readl(ths_data->base_addr +
			pvt_hw_sensor[id].alarm_h->irq_enable.reg);
	reg = SET_BITS(pvt_hw_sensor[id].alarm_h->irq_enable.shift, 1, reg, 0x0);
	writel(reg, ths_data->base_addr +
			pvt_hw_sensor[id].alarm_h->irq_enable.reg);
	sensor_info[id].irq_enabled = false;
	printk("semidrive_pvt_disable_alarm_irq\n");
}


static int semidrive_pvt_enable_alarm_irq_sensor(struct semidrive_pvt_data *data, u32 i)
{
	u32 reg;
	struct thermal_sensor_info *sensor_info =
		(struct thermal_sensor_info *)data->data;
	/* clear irq pending before enable irq */
	semidrive_pvt_clr_alarm_irq_pending(data, i);
	/*enale alarm mode irq */
	reg = readl(data->base_addr + pvt_hw_sensor[i].alarm_h->alarm_enable.reg);
	reg = SET_BITS(pvt_hw_sensor[i].alarm_h->alarm_enable.shift, 1, reg, 0x1);
	writel(reg, data->base_addr + pvt_hw_sensor[i].alarm_h->alarm_enable.reg);
	/*enable irq control*/
	reg = readl(data->base_addr + pvt_hw_sensor[i].alarm_h->irq_enable.reg);
	reg = SET_BITS(pvt_hw_sensor[i].alarm_h->irq_enable.shift, 1, reg, 0x1);
	writel(reg, data->base_addr + pvt_hw_sensor[i].alarm_h->irq_enable.reg);
	sensor_info[i].irq_enabled = true;
	printk("semidrive_pvt_enable_alarm_irq_sensor\n");
	return 0;
}

/* init enable alarm irq ctrl */
static void semidrive_pvt_enable_alarm_irq_ctrl(struct semidrive_pvt_data *ths_data)
{
	u32 i;
	struct thermal_sensor_info *sensor_info =
		(struct thermal_sensor_info *)ths_data->data;
	for (i = 0; i < ths_data->sensor_cnt; i++) {
		sensor_info[i].alarm_irq_type = THS_LOW_TEMP_ALARM;
		semidrive_pvt_set_alarm_threshold_temp(ths_data, i);
	}
	for (i = 0; i < ths_data->sensor_cnt; i++)
		semidrive_pvt_enable_alarm_irq_sensor(ths_data, i);
}



static u32 semidrive_pvt_query_alarmirq_pending(struct semidrive_pvt_data *data, u32 id)
{
	u32 reg, irq_mask;
	irq_mask = 1 << pvt_hw_sensor[id].alarm_h->irq_status.shift;
	reg = readl(data->base_addr +
			pvt_hw_sensor[id].alarm_h->irq_status.reg);
	return reg&irq_mask;
}



static void semidrive_pvt_handle_irq_pending(struct semidrive_pvt_data *ths_data)
{
	u32 id;
	struct thermal_sensor_info *sensor_info =
		(struct thermal_sensor_info *)ths_data->data;

	for (id = 0; id < ths_data->sensor_cnt; id++) {
		if (sensor_info[id].irq_enabled) {
			if (semidrive_pvt_query_alarmirq_pending(ths_data, id)) {
				printk("pvt sensor[%d]&irq-type=%d\n", id,
					sensor_info[id].alarm_irq_type);
				if (sensor_info[id].alarm_irq_type ==
							THS_LOW_TEMP_ALARM) {
					sensor_info[id].alarm_irq_type =
						THS_HIGH_TEMP_ALARM;
					semidrive_pvt_set_alarm_threshold_temp(
								ths_data, id);
				} else
					semidrive_pvt_disable_alarm_irq(
								ths_data, id);
			}
		}
		semidrive_pvt_clr_alarm_irq_pending(ths_data, id);
	}

}

static irqreturn_t semidrive_pvt_alarm_irq(int irq, void *dev)
{
	struct semidrive_pvt_data *ths_data = dev;

	printk("alarm_irq\n");
	disable_irq_nosync(irq);
	semidrive_pvt_handle_irq_pending(ths_data);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t semidrive_pvt_alarm_irq_thread(int irq, void *dev)
{
	struct semidrive_pvt_data *ths_data = dev;
	int i;
	struct thermal_sensor_info *sensor_info =
		(struct thermal_sensor_info *)ths_data->data;

	for (i = 0; i < ths_data->combine_cnt; i++) {
		thermal_zone_device_update(ths_data->comb_sensor[i]->tz,THERMAL_DEVICE_ENABLED);
		printk("pvt[%d]_alarmtemp = %d\n", i, sensor_info[i].temp);
	}
	printk("semidrive_pvt_alarm_irq_thread\n");
	enable_irq(irq);

	return IRQ_HANDLED;
}
#else
static void semidrive_pvt_enable_alarm_irq_ctrl(struct semidrive_pvt_data *ths_data)
{

}

#endif

static int semidrive_pvt_get_temp(struct semidrive_pvt_controller *controller, u32 id,
				int *temp)
{
	int t = 0;
	struct semidrive_pvt_data *ths_data =
		(struct semidrive_pvt_data *)controller->data;
	struct thermal_sensor_info *sensor_info =
		(struct thermal_sensor_info *)ths_data->data;
	if (ths_data->sensor_cnt <= id) {
		pvtprintk("ths driver get wrong sensor num!\n");
		return -1;
	}
	t = pvt_driver_get_temp(ths_data, id);
	if (-40 > t || 180 < t) {
		pvtprintk("ths driver get unvalid temp!\n");
		return t;
	}
	sensor_info[id].temp = t;
	*temp = t;
	return 0;
}



static void semidrive_pvt_info_init(struct thermal_sensor_info *sensor_info,
				int sensor_num)
{
	sensor_num -= 1;
	while (sensor_num >= 0) {
		sensor_info[sensor_num].id = sensor_num;
		sensor_info[sensor_num].ths_name = pvt_id_name_mapping[sensor_num];
		sensor_info[sensor_num].temp = 0;
		sensor_info[sensor_num].alarm_irq_type = THS_LOW_TEMP_ALARM;
		sensor_info[sensor_num].irq_enabled = false;
		pvtprintk("sensor_info:id=%d,name=%s,temp=%d!\n",
			sensor_info[sensor_num].id,
			sensor_info[sensor_num].ths_name,
			sensor_info[sensor_num].temp);
		sensor_num--;
	}
	return;
}

static void semidrive_pvt_para_init(struct semidrive_pvt_data *ths_data,
				struct thermal_sensor_info *sensor_info)
{
	semidrive_pvt_info_init(sensor_info, ths_data->sensor_cnt);
	ths_data->data = sensor_info;
	return;
}

int pvt_get_sensor_temp(u32 sensor_num, int *temperature)
{
	return semidrive_pvt_get_temp(main_ctrl, sensor_num, temperature);
}
EXPORT_SYMBOL(pvt_get_sensor_temp);

static int semidrive_pvt_suspend(struct semidrive_pvt_controller *controller)
{
	struct semidrive_pvt_data *ths_data =
		(struct semidrive_pvt_data *)controller->data;
	pvtprintk("enter: semidrive_pvt_controller_suspend.\n");
	atomic_set(&controller->is_suspend, 1);
	semidrive_pvt_exit(ths_data);
	return 0;
}

static int semidrive_pvt_resume(struct semidrive_pvt_controller *controller)
{
	struct semidrive_pvt_data *ths_data =
		(struct semidrive_pvt_data *)controller->data;
	pvtprintk("enter: semidrive_pvt_controller_resume.\n");
	semidrive_pvt_reg_init(ths_data);
#ifdef SEMIDRIVE_PVT_SUPPORT_IRQ
	semidrive_pvt_enable_alarm_irq_ctrl(ths_data);
#endif
	atomic_set(&controller->is_suspend, 0);
	return 0;
}

struct semidrive_pvt_controller_ops semidrive_pvt_ops = {
	.suspend = semidrive_pvt_suspend,
	.resume = semidrive_pvt_resume,
	.get_temp = semidrive_pvt_get_temp,
};

static int semidrive_pvt_probe(struct platform_device *pdev)
{
	int err = 0;
	struct semidrive_pvt_data *ths_data;
	struct semidrive_pvt_controller *ctrl;
	struct thermal_sensor_info *sensor_info;
	if (!pdev->dev.of_node) {
		pr_err("%s:semidrive_pvt device tree err!\n", __func__);
		return -EBUSY;
	}
	ths_data = kzalloc(sizeof(*ths_data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(ths_data)) {
		pr_err("ths_data: not enough memory for ths_data\n");
		err = -ENOMEM;
		goto fail0;
	}
	ths_data->ths_coefficent =
		kzalloc(sizeof(*ths_data->ths_coefficent), GFP_KERNEL);
	if (IS_ERR_OR_NULL(ths_data->ths_coefficent)) {
		pr_err("ths_coe: not enough memory for ths_coe\n");
		err = -ENOMEM;
		goto fail1;
	}
	mutex_init(&ths_data->ths_mutex_lock);
	ths_data->cp_ft_flag = 0;
	ths_data->pdev = pdev;
	err = pvt_driver_startup(ths_data, &pdev->dev);
	if (err)
		goto fail2;
	ths_data->comb_sensor = kzalloc(ths_data->combine_cnt *
			(sizeof(struct semidrive_pvt_sensor *)), GFP_KERNEL);
	if (IS_ERR_OR_NULL(ths_data->comb_sensor)) {
		pr_err("ths_comb_sensor: not enough memory to alloc!\n");
		err = -ENOMEM;
		goto fail3;
	}

#ifdef SEMIDRIVE_PVT_SUPPORT_IRQ
	err = devm_request_threaded_irq(&pdev->dev, ths_data->irq,
					semidrive_pvt_alarm_irq,
					semidrive_pvt_alarm_irq_thread,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,"semidrive_pvt", ths_data);
	if (err < 0) {
		pr_err("failed to request alarm irq: %d\n", err);
		return err;
	}
#endif
	sensor_info =
		kcalloc(ths_data->sensor_cnt, sizeof(*sensor_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(sensor_info)) {
		pr_err("pvt_sensor_info: not enough memory for pvt_sensor_info\n");
		err = -ENOMEM;
		goto fail2;
	}
	semidrive_pvt_para_init(ths_data, sensor_info);
	platform_set_drvdata(pdev, ths_data);
	semidrive_pvt_reg_init(ths_data);

#ifdef SEMIDRIVE_PVT_SUPPORT_IRQ
	semidrive_pvt_enable_alarm_irq_ctrl(ths_data);
#endif
	pvt_driver_create_sensor_info_attrs(ths_data, sensor_info);
	ctrl = semidrive_pvt_controller_register(&pdev->dev,
							&semidrive_pvt_ops, ths_data);
	if (!ctrl) {
		pr_err("semidrive: pvt controller register err.\n");
		err = -ENOMEM;
		goto fail3;
	}
	ths_data->ctrl = ctrl;
	main_ctrl = ctrl;
	printk("semidrive_pvt_controller is ok.\n");
	return 0;
fail3:
	kfree(sensor_info);
	sensor_info = NULL;
fail2:
	kfree(ths_data->ths_coefficent);
	ths_data->ths_coefficent = NULL;
fail1:
	kfree(ths_data);
	ths_data = NULL;
fail0:
	return err;
}

static int semidrive_pvt_remove(struct platform_device *pdev)
{
	struct semidrive_pvt_data *ths_data = platform_get_drvdata(pdev);
	semidrive_pvt_controller_unregister(ths_data->ctrl);
	semidrive_pvt_exit(ths_data);
	pvt_drvier_remove_trip_attrs(ths_data);
	kfree(ths_data->ths_coefficent);
	kfree(ths_data);
	return 0;
}

#ifdef CONFIG_OF
/* Translate OpenFirmware node properties into platform_data */
static struct of_device_id semidrive_pvt_of_match[] = {
	{.compatible = "semidrive,pvt_v1.0",},
	{},
};

MODULE_DEVICE_TABLE(of, semidrive_pvt_of_match);
#endif

static struct platform_driver semidrive_pvt_driver = {
	.probe = semidrive_pvt_probe,
	.remove = semidrive_pvt_remove,
	.driver = {
			.name = SEMIDRIVE_PVT_NAME,
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(semidrive_pvt_of_match),
			},
};
static int __init semidrive_pvt_sensor_init(void)
{
	return platform_driver_register(&semidrive_pvt_driver);
}

static void __exit semidrive_pvt_sensor_exit(void)
{
	platform_driver_unregister(&semidrive_pvt_driver);
}

device_initcall_sync(semidrive_pvt_sensor_init);
module_exit(semidrive_pvt_sensor_exit);
MODULE_DESCRIPTION("Semidrive Pvt driver");
MODULE_AUTHOR("david");
MODULE_LICENSE("GPL v2");
