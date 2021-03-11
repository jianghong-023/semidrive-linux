/*
 * Copyright (C) 2021 Semidrive, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>

#include "sdrv-deser.h"

#define MAX_SENSOR_NUM 1

#define MAX96706_DEVICE_ID 0x4A
#define MAX96706_SLAVE_ID 0x78


#define MAX96705_SLAVE_ID 0x40
#define MAX96705_DEV_INDEX -9
#define MAX96705_DEVICE_ID 0x41

enum AP0101_I2C_ADDR {
	AP0101_DEF = 0xBA,
	AP0101_CH_A = 0xA8,
};


enum MAX96722_CHANNEL {
	MAX96722_CH_A = 0,
	MAX96722_CH_B = 1,
	MAX96722_CH_C = 2,
	MAX96722_CH_D = 3,
};

enum MAX96705_I2C_ADDR {
	MAX96705_DEF = 0x80,
	MAX96705_CH_A = 0x98,
};

enum MAX96705_READ_WRITE {
	MAX96705_READ,
	MAX96705_WRITE,
};

enum SUBBOARD_TYPE{
	BOARD_SD507_A02P  = 0x00,
	BOARD_SD507_A02  = 0x10,
	BOARD_DB5071 = 0x20,
};


static int max96706_write_reg(deser_dev_t *sensor, u8 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_deser;
	buf[0] = reg;
	buf[1] = val;
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x, val=0x%x\n",
				__func__, reg, val);
		return ret;
	}

	return 0;
}

static int max96706_read_reg(deser_dev_t *sensor, u8 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_deser;
	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x\n",
				__func__, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
}


static int max96705_write_reg(deser_dev_t *dev, int channel, u8 reg,
				  u8 val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;
	client->addr = dev->addr_serer;
	buf[0] = reg;
	buf[1] = val;

	msg.addr = client->addr  + channel;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 2;
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: channel %d chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
			__func__, channel, msg.addr, reg, val);
		return ret;
	}

	return 0;
}

static int max96705_read_reg(deser_dev_t *dev, int channel, u8 reg,
				 u8 *val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;
	client->addr = dev->addr_serer;
	buf[0] = reg;

	msg[0].addr = client->addr + channel;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr + channel;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: channel %d chip 0x%02x error: reg=0x%02x\n",
			__func__, channel,  msg[0].addr, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
}

static int max96705_access_sync(deser_dev_t *dev,  int channel, int write, u16 reg, u8 *val_r, u8 val_w)
{
	int ret = -1;

	if (channel < 0 || channel > 3) {
		dev_err(&dev->i2c_client->dev, "%s: chip max96705 ch=%d, invalid channel.\n", __FUNCTION__, channel);
		return ret;
	}

	mutex_lock(&dev->serer_lock);

	if (write == MAX96705_WRITE)
		ret = max96705_write_reg(dev, channel, reg, val_w);
	else if (write == MAX96705_READ)
		ret = max96705_read_reg(dev, channel, reg, val_r);
	else
		dev_err(&dev->i2c_client->dev, "%s: chip max96705 Invalid parameter.\n", __FUNCTION__);

	mutex_unlock(&dev->serer_lock);
	return ret;
}


static int ap0101_read_reg16(deser_dev_t *sensor, int idx, u16 reg,
							 u16 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;
	client->addr = sensor->addr_isp;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr + idx;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr + idx;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n",
				__func__, reg);
		return ret;
	}

	*val = ((u16)buf[0] << 8) | (u16)buf[1];

	return 0;

}
static int ap0101_write_reg16(deser_dev_t *sensor, int idx, u16 reg,
							  u16 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[4];
	int ret;
	client->addr = sensor->addr_isp;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	msg.addr = client->addr + idx;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x, val=0x%x\n",
				__func__, reg, val);
		return ret;
	}

	return 0;
}


static int ap0101_read_reg32(deser_dev_t *sensor, u8 idx, u16 reg,
							 u32 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[4];
	int ret;
	client->addr = sensor->addr_isp;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr + idx;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = 2;

	msg[1].addr = client->addr + idx;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 4;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n",
				__func__, reg);
		return ret;
	}

	*val = ((u32)buf[0] << 24) | ((u32)buf[1] << 16) | ((u32)buf[2] << 8) |
		   (u32)buf[3];

	return 0;

}

static int ap0101_write_reg32(deser_dev_t *sensor, u8 idx, u16 reg,
							  u32 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[6];
	int ret;
	client->addr = sensor->addr_isp;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 24;
	buf[3] = val >> 16;
	buf[4] = val >> 8;
	buf[5] = val & 0xff;

	msg.addr = client->addr + idx;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x, val=0x%x\n",
				__func__, reg, val);
		return ret;
	}

	return 0;
}

static int tca9539_write_reg(deser_dev_t *sensor, u8 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_gpioext;
	buf[0] = reg;
	buf[1] = val;
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x, val=0x%x\n",
				__func__, reg, val);
		return ret;
	}

	return 0;
}

static int tca9539_read_reg(deser_dev_t *sensor, u8 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_gpioext;
	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x\n",
				__func__, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
}
//Power for Rx max96706/712
static int max96706_power(deser_dev_t *dev, bool enable)
{
	//printk("max96706 power +\n");

	if (dev->pwdn_gpio)
		gpiod_direction_output(dev->pwdn_gpio, enable ? 1 : 0);
	//printk("max96706 power -\n");

	return 0;
}


int check_camera_subboard(deser_dev_t *sensor)
{
	u8  val[2];
	int ret;
	int board_type;
	struct i2c_client *client = sensor->i2c_client;

	/*
	ret = tca9539_read_reg(sensor, 0x00, &val[0]);
	if (ret != 0)
		dev_err(&client->dev, "Fail to get tca9539 reg.");

	printk("TCA9539: 0x0=0x%x\n", val[0]);
	*/
	ret = tca9539_read_reg(sensor, 0x01, &val[1]);
	if (ret != 0) {
		dev_err(&client->dev, "Fail to get tca9539 reg.");
		return ret;
	} else
		board_type = val[1]&0x30;

	printk("TCA9539: 0x1=0x%x, board_type = 0x%x\n", val[1], board_type);

	return board_type;
}


//Power for camera module
static int poc_power(deser_dev_t *dev, bool enable)
{
	if (dev->poc_gpio)
		gpiod_direction_output(dev->poc_gpio, enable ? 1 : 0);
	return 0;
}

//db5071 and sd507 use different gpi gpio.
//tca9539 extent gpio chip
static int gpi_power(deser_dev_t *dev, int gpio, bool enable)
{
	int board_type;
	u8 val;
	u8 reg;
	int shift;

	//printk("poc power +\n");

	if (gpio > 15 || gpio < 0)
		return -EINVAL;


	board_type = check_camera_subboard(dev);
	//printk("check board 0x%x +\n", board_type);

	if (board_type < 0)
		board_type = BOARD_DB5071;

	if (gpio > 7) {
		shift = gpio - 8;
		reg = 0x7;
	} else if (gpio < 7) {
		shift = gpio;
		reg = 0x6;
	}

	if (board_type == BOARD_SD507_A02 || board_type == BOARD_SD507_A02P) {
		printk("gpi sd507 a02/a02+ board.\n");
		if (dev->gpi_gpio) {
			dev_err(&dev->i2c_client->dev, "has gpi_gpio\n", __func__);
			gpiod_direction_output(dev->gpi_gpio, enable ? 1 : 0);
		} else {
			dev_err(&dev->i2c_client->dev, "no gpi_gpio\n", __func__);
		}
	} else {  //For BOARD_DB5071 PIN9
		printk("gpi db5071 board.\n");
		if (enable == true) {
			tca9539_read_reg(dev, reg, &val);
			val &= ~(1 << shift);
			tca9539_write_reg(dev, reg, val);
		} else if (enable == false) {
			tca9539_read_reg(dev, reg, &val);
			val |= (1 << shift);
			tca9539_write_reg(dev, reg, val);
		}
	}

	//printk("poc power -\n");
	return 0;

}

static int ap0101_change_config(deser_dev_t *sensor, u8 idx)
{
	u16 reg;
	u16 val16;
	int i;

	reg = 0xfc00;
	val16 = 0x2800;
	ap0101_write_reg16(sensor, 0, reg, val16);
	usleep_range(10000, 10000);

	reg = 0x0040;
	val16 = 0x8100;
	ap0101_write_reg16(sensor, 0, reg, val16);
	usleep_range(10000, 10000);

	for (i = 0; i < 10; i++) {
		reg = 0x0040;
		val16 = 0;
		ap0101_read_reg16(sensor, 0, reg, &val16);

		if (val16 == 0x0)
			return 0;

		usleep_range(10000, 10000);
	}

	return -1;
}

//Should implement this interface
static int start_deser(deser_dev_t *sensor, bool en)
{
	u8 val;

	if (en == true) {
		msleep(100);
		max96706_read_reg(sensor, 0x04, &val);
		val &= ~(0x40);
		max96706_write_reg(sensor, 0x04, val);
		usleep_range(1000, 1100);
	} else if (en == false) {
		max96706_read_reg(sensor, 0x04, &val);
		val |= 0x40;
		max96706_write_reg(sensor, 0x04, val);
		usleep_range(1000, 1100);
	}
	printk("max96706 start data  -\n");

	return 0;
}

//Should implement this interface
static int max96705_check_chip_id(deser_dev_t *dev)
{
	int ret = 0;
	u8 chip_id = 0;

	ret = max96705_read_reg(dev, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x1E, &chip_id);

	return ret;
}

//Should implement this interface
static int max96706_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
	int i = 0;

	//max96706_power(dev, 0);
	//usleep_range(5000, 6000);
	//max96706_power(dev, 1);
	//msleep(30);

	ret = max96706_read_reg(dev, 0x1E, &chip_id);

	if (ret < 0) {
		//dev_err(&client->dev, "%s: failed to read max96706 chipid.\n",
		//	__func__);
		for (i = 0; i < 2; i++) {
			ret = max96706_read_reg(dev, 0x1E, &chip_id);
			if (ret == 0)
				break;
			//dev_err(&client->dev,"failed to read max96706 chipid.\n");
			usleep_range(10000, 10000);
		}
		//if(ret < 0)
		//	return ret;
	}

	if (chip_id ==  MAX96706_DEVICE_ID) {
		dev_err(&client->dev, "max96706 chipid = 0x%02x\n", chip_id);
		ret = max96706_read_reg(dev, 0x1F, &chip_id);
		dev_err(&client->dev, "max96706 dev version = 0x%02x\n", chip_id);
	} else {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(max96706), got 0x%x\n",
			__func__, MAX96706_DEVICE_ID, chip_id);
	}

	return ret;
}

void max96706_reg_dump(deser_dev_t *sensor)
{
	return;
}


//Should implement this interface
static int max96706_initialization(deser_dev_t *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 val;
	int i = 0;
	u16 reg;
	u16 val16;

	dev_err(&client->dev, "%s()\n", __func__);
	//printk("max96706 init +\n");

	//set him
	val = 0;
	max96706_read_reg(sensor, 0x06, &val);
	val |= 0x80;
	max96706_write_reg(sensor, 0x06, val);
	usleep_range(1000, 1100);

	//invert hsync
	val = 0;
	max96706_read_reg(sensor, 0x02, &val);
	val |= 0x80;
	max96706_write_reg(sensor, 0x02, val);
	usleep_range(1000, 1100);

	//disable output
	val = 0;
	max96706_read_reg(sensor, 0x04, &val);
	val |= 0x40;
	max96706_write_reg(sensor, 0x04, val);
	msleep(20);

	poc_power(sensor, 0);
	usleep_range(1000, 2000);
	poc_power(sensor, 1);
	msleep(100);

	//Retry for i2c address comflict in defaul 7b i2c address 0x40.
	for (i = 0; i < 3; i++) {
		ret = max96705_check_chip_id(sensor);

		if (ret != 0) {
			dev_err(&client->dev, "%s: times %d not found 96705, ret=%d\n", __func__, i, ret);
			msleep(20);
		} else {
			dev_err(&client->dev, "%s: found 96705, ret=%d\n", __func__, ret);
			break;
		}
	}

	//Change I2C
	max96705_write_reg(sensor,  (MAX96705_DEF-MAX96705_CH_A)>>1, 0x00, sensor->addr_serer << 1);
	usleep_range(5000, 6000);
	max96705_write_reg(sensor, 0, 0x01, (sensor->addr_deser) << 1);
	usleep_range(5000, 6000);

	if (sensor->gpi_gpio) {
		/*
		gpiod_direction_output(sensor->gpi_gpio, 1);
		usleep_range(100, 200);
		gpiod_direction_output(sensor->gpi_gpio, 0);
		usleep_range(100, 200);
		gpiod_direction_output(sensor->gpi_gpio, 1);
		msleep(50); */

		gpi_power(sensor, 9, 1);
		usleep_range(100, 200);
		gpi_power(sensor, 9, 0);
		usleep_range(100, 200);
		gpi_power(sensor, 9, 1);
	} else {
		dev_err(&sensor->i2c_client->dev, "%s: no gpi_gpio\n", __func__);
		val = 0;
		max96705_read_reg(sensor, 0, 0x0f, &val);
		dev_info(&sensor->i2c_client->dev, "%s: val=0x%x\n", __func__, val);
		val |= 0x81;
		max96705_write_reg(sensor, 0, 0x0f, val);
		usleep_range(100, 200);
	}

	//enable dbl, hven
	max96705_write_reg(sensor, 0, 0x07, 0x84);
	usleep_range(10000, 10000);
	//[7]dbl, [2]hven, [1]cxtp
	max96706_write_reg(sensor, 0x07, 0x86);
	usleep_range(10000, 10000);

	max96705_write_reg(sensor, 0, 0x9, sensor->addr_isp << 1);
	max96705_write_reg(sensor, 0, 0xa, AP0101_DEF);
	msleep(20);


	reg = 0x0;	//chip version, 0x0160
	val16 = 0;
	ap0101_read_reg16(sensor, 0, reg, &val16);
	dev_err(&client->dev, "0101, reg=0x%x, val=0x%x\n", reg, val16);


	reg = 0xca9c;	//
	val16 = 0;
	ap0101_read_reg16(sensor, 0, reg, &val16);
	dev_err(&client->dev, "0101, reg=0x%x, val=0x%x\n", reg, val16);
	if ((val16 & (0x1<<10)) == 0) {
		val16 = 0x405;
		ap0101_write_reg16(sensor, 0, reg, val16);
		usleep_range(10000, 11000);

		for (i = 0; i < 10; i++) {
			ret = ap0101_change_config(sensor, 0);
			if (ret == 0)
				break;
			msleep(100);
		}
	}
	val16 = 0;
	ap0101_read_reg16(sensor, 0, reg, &val16);
	dev_err(&client->dev, "0101, reg=0x%x, val=0x%x\n", reg, val16);
	if ((val16 & (0x1<<9)) == 0) {
		val16 = 0x605;
		ap0101_write_reg16(sensor, 0, reg, val16);
		usleep_range(10000, 11000);

		for (i = 0; i < 10; i++) {
			ret = ap0101_change_config(sensor, 0);
			if (ret == 0)
				break;
			msleep(100);
		}
	}


	dev_err(&client->dev, "0101, end msb. i=%d\n", i);

	val = 0;
	max96706_read_reg(sensor, 0x5, &val);
	dev_err(&client->dev, "96706, reg=0x5, val=0x%x\n", val);
	val |= 0x40;
	max96706_write_reg(sensor, 0x5, val);
	val = 0;
	max96706_read_reg(sensor, 0x5, &val);
	dev_err(&client->dev, "96706, reg=0x5, val=0x%x\n", val);
	//printk("max96706 init -\n");

	return 0;
}

deser_para_t max96706_para = {
	.name = "max96706",
	.addr_deser = MAX96706_SLAVE_ID,
	.addr_serer = MAX96705_CH_A>>1,
	.addr_isp	= AP0101_CH_A>>1,
	.addr_gpioext = 0x74,
	.pclk		= 72*1000*1000,
	.width		= 1280,
	.htot		= 1320,
	.height		= 720,
	.vtot		= 740,
	.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps		= 25,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.denominator = 25,
	.used		= DESER_NOT_USED, // NOT USED IN DEFAULT
	.reset_gpio = NULL,	//if dts can't config reset and pwdn, use these two members.
	.pwdn_gpio	= NULL,
	.power_deser = max96706_power,
	.power_module = poc_power,
	.detect_deser = max96706_check_chip_id,
	.init_deser = max96706_initialization,
	.start_deser = start_deser,
	.dump_deser	= max96706_reg_dump,
};
