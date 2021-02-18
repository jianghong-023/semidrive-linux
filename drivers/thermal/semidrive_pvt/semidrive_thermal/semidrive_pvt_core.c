/*
  * 
  *
  * Copyright (c) 2021 Semidrive Semiconductor.
  * All rights reserved.
  *
  * Description: PVT SENSOR
  *
  * Revision History:
  * -----------------
 */ 
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "semidrive_pvt.h"
#include "semidrive_pvt_core.h"


static const char * const pvt_mode[] = {
	[PVT_MODE_TYPE_TEMP]	= "temp",
	[PVT_MODE_TYPE_VOL] 	= "vol",
	[PVT_MODE_TYPE_P]		= "process",
};

static enum pvt_mode_t get_pvt_mode(const char *t)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(pvt_mode); i++)
		if (!strcasecmp(t, pvt_mode[i])) {
			return i;
		}
	return 0;
}


static ssize_t
pvt_sensor_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct semidrive_pvt_data *ths_data = platform_get_drvdata(pdev);
	int indx;
	struct thermal_sensor_info *sensor_info =
			(struct thermal_sensor_info *)ths_data->data;
	for (indx = 0; indx < ths_data->sensor_cnt; indx++) {
		sprintf(buf, "sensor_%d_%s_temp", indx, sensor_info[indx].ths_name);
		if (!strcmp(attr->attr.name, buf)) {
			printk("pvt sensor %d located in %s, temp is = %d\n",indx,sensor_info[indx].ths_name,sensor_info[indx].temp);
			return sprintf(buf, "pvt sensor %d located in %s, temp is = %d\n",
					indx,
					sensor_info[indx].ths_name,
					sensor_info[indx].temp);
		}
	}
	return -EINVAL;
}

int pvt_driver_create_sensor_info_attrs(struct semidrive_pvt_data *ths_data,
				struct thermal_sensor_info *sensor_info)
{
	int indx, size;
	size = sizeof(struct pvt_info_attr) * ths_data->sensor_cnt;
	ths_data->ths_info_attrs = kzalloc(size, GFP_KERNEL);
	if (!ths_data->ths_info_attrs) {
		pr_err("pvt_info_attrs: not enough memory for sensor_info\n");
		return -ENOMEM;
	}
	for (indx = 0; indx < ths_data->sensor_cnt; indx++) {
		/* create trip type attribute */
		snprintf(ths_data->ths_info_attrs[indx].name, PVT_ATTR_LENGTH,
			 "pvt_sensor_%d_%s_temp", indx, sensor_info[indx].ths_name);
		sysfs_attr_init(&ths_data->ths_info_attrs[indx].attr.attr);
		ths_data->ths_info_attrs[indx].attr.attr.name =
						ths_data->ths_info_attrs[indx].name;
		ths_data->ths_info_attrs[indx].attr.attr.mode = S_IRUGO;
		ths_data->ths_info_attrs[indx].attr.show = pvt_sensor_info_show;
		device_create_file(&ths_data->pdev->dev,
				&ths_data->ths_info_attrs[indx].attr);
	}
	return 0;
}

void pvt_drvier_remove_trip_attrs(struct semidrive_pvt_data *ths_data)
{
	int indx;
	for (indx = 0; indx < ths_data->sensor_cnt; indx++) {
		device_remove_file(&ths_data->pdev->dev,
				&ths_data->ths_info_attrs[indx].attr);
	}
	kfree(ths_data->ths_info_attrs);
}


int pvt_driver_startup(struct semidrive_pvt_data *ths_data,
				struct device *dev)
{
	struct device_node *np = NULL;
	np = dev->of_node;
	ths_data->base_addr = of_iomap(np, 0);
	if (ths_data->base_addr != NULL) {
		printk("PVT base: %p !\n", ths_data->base_addr);
	} else {
		pr_err("%s:Failed to ioremap() io memory region.\n", __func__);
		return -EBUSY;
	}
	ths_data->irq = irq_of_parse_and_map(np, 0);
	if (ths_data->irq != 0) {
		printk("PVT irq num: %d !\n", ths_data->irq);
	} else {
		pr_err("%s:Failed to map irq.\n", __func__);
		return -EBUSY;
	}
	if (of_property_read_u32(np, "sensor_num", &ths_data->sensor_cnt)) {
		pr_err("%s: get pvt sensor_num failed\n", __func__);
		return -EBUSY;
	}
	if (of_property_read_u32(np, "combine_num", &ths_data->combine_cnt))
		pr_err("%s: get combine_num failed\n", __func__);

	if (of_property_read_u32(np, "alarm_low_temp",
				&ths_data->alarm_low_temp)) {
		pr_err("%s: get alarm_low_temp failed\n", __func__);
	}
	/* getting the pvt sensor mode :T/V/P */
	if (of_property_read_string(np, "pvt_sensor_mode", &ths_data->pvt_mode)) {
		pr_err("%s: get  pvt_sensor_mode\n", __func__);
		return -EBUSY;
		}
		ths_data->pvt_set_mode = get_pvt_mode(ths_data->pvt_mode);
	if (of_property_read_u32(np, "alarm_high_temp",
				&ths_data->alarm_high_temp)) {
		pr_err("%s: get alarm_high_temp failed\n", __func__);
	}
	if (of_property_read_u32(np, "alarm_temp_hysteresis",
				&ths_data->alarm_temp_hysteresis)) {
		pr_err("%s: get alarm_temp_hysteresis failed\n", __func__);
	}
	return 0;
}


int pvt_driver_get_temp(struct semidrive_pvt_data *ths_data, int id)
{
	int i = 0;
	u32 data = 0;
	int temp=0;
	float temp_data = 1;
	float final_data = 0;
	u32 dout_is_temperature = 0;
	float a4_t[5];
	float a2_v[2];
	a4_t[4] = 1.6034E-11;
	a4_t[3] = 1.5608E-08;
	a4_t[2] = -1.5089E-04;
	a4_t[1] = 3.3408E-01;
	a4_t[0] = -6.2861E+01;
	a2_v[1] = 5.9677E-04;
	a2_v[0] = 5.1106E-01;
	void __iomem *pvtbase_ctl_addr = ths_data->base_addr;
	void __iomem *pvt_dout_addr = pvtbase_ctl_addr + PVT_REGISTER_DOUT_ADDR_OFFSET;
	data = readl(pvtbase_ctl_addr);
	if (data & 0xe) {
	dout_is_temperature = 0;
	}
	else {
	dout_is_temperature = 1;
	}
	data = readl(pvt_dout_addr);
	// bit10~1 is dout value
	// bit0 is pvt dout valid indicate 0 is invalid 1 is valid
	data = (data & 0x7fe) >> 1;
	// y = a4*x^4+a3*x^3+a2*x^2+a1*x^1+a0
		{
		for (i = 0; i < 5; i++) {
			final_data += a4_t[i] * temp_data;
			temp_data = data * temp_data;
		}
		temp = (int)final_data;
		pvtprintk( "temperature value = %d\n", temp);
	}
	return temp;
}
//set pvt mode
static void pvt_set_mode(struct semidrive_pvt_data *ths_data)
{
	u32 data = 0;
	data = readl(ths_data->base_addr);
	switch (ths_data->pvt_set_mode) {
		case PVT_MODE_TYPE_TEMP:
			data = (data & (~0xE));//bit1~3 set 0
			break;
		case PVT_MODE_TYPE_VOL:
			data = (data | (0x2));//bit1 set 1
			break;
		case PVT_MODE_TYPE_P:
			data = ((data & (~0x6)) | (0x4));//bit2~3 set xxx
			break;
		default:
			data = (data & (~0xE));//bit1~3 set 0
			break;
		}
		writel(data, ths_data->base_addr); // set pvt mode
		data = readl(ths_data->base_addr);
}
/**
 * pvt ctrl mode init 
 * 
 */
static void pvt_driver_ctrl_mode_init(struct semidrive_pvt_data *ths_data)
{
		u32 reg_value;
		reg_value = readl(ths_data->base_addr);
		// set pvt ctrl mode: ctrl from register
		reg_value = reg_value | (1 << PVT_CTRL_CTRL_MODE);
		writel(reg_value, ths_data->base_addr);
		reg_value = readl(ths_data->base_addr);
		reg_value = reg_value | (1 << PVT_CTRL_SOFTWARE_CTRL) ;
		writel(reg_value, ths_data->base_addr);
		reg_value = readl(ths_data->base_addr);
		pvtprintk("pvt_ctrl_mode_init data = 0x%x\n", reg_value);
}

int pvt_driver_disable_reg(struct semidrive_pvt_data *ths_data)
{
	u32 reg_value;
	reg_value = readl(ths_data->base_addr);
	// set pvt ctrl mode: ctrl from register
	reg_value = reg_value | (1 << PVT_CTRL_CTRL_MODE);
	writel(reg_value, ths_data->base_addr);
	reg_value = readl(ths_data->base_addr);
	// disable pvt ctrl register
	reg_value &= ~(reg_value | (1 << PVT_CTRL_SOFTWARE_CTRL) );
	writel(reg_value, ths_data->base_addr);
	reg_value = readl(ths_data->base_addr);
	pvtprintk("pvt_ctrl_mode_disreg data = 0x%x\n", reg_value);
	return 0;

}

int pvt_driver_init_reg(struct semidrive_pvt_data *ths_data)
{
	pvt_driver_ctrl_mode_init(ths_data);
	pvt_set_mode(ths_data);
	return 0;
}



