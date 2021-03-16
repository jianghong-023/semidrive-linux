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
#include "poc-max2008x.h"


#define MAX_CAMERA_NUM 4

#define MAX96712_DEVICE_ID 0xa0
#define MAX96722_DEVICE_ID 0xa1
#define MAX96705_DEVICE_ID 0x41

#define MAX96722_SLAVE_ID (0x6b)
#define AP0101_SLAVE_ID (0x5d)
#define MAX20087_SLAVE_ID (0x29)

enum MAX96722_CHANNEL {
	MAX96722_CH_A = 0,
	MAX96722_CH_B = 1,
	MAX96722_CH_C = 2,
	MAX96722_CH_D = 3,
};

enum MAX96705_I2C_ADDR {
	MAX96705_DEF = 0x80,
	MAX96705_CH_A = 0x90,
	MAX96705_CH_B = 0x92,
	MAX96705_CH_C = 0x94,
	MAX96705_CH_D = 0x96,
};

enum AP0101_I2C_ADDR {
	AP0101_DEF  = 0xAB,
	AP0101_CH_A = 0xA0,
	AP0101_CH_B = 0xA2,
	AP0101_CH_C = 0xA4,
	AP0101_CH_D = 0xA6,
};


enum MAX96705_READ_WRITE {
	MAX96705_READ,
	MAX96705_WRITE,
};

//#define ONE_PORT
#define FRAMESYNC_USE
//#define DEBUG_MAX96722

static reg_param_t max96722_reg[] = {
	// Begin of Script
	// MAX96712 Link Initialization to pair with GMSL1 Serializers
	{0x0006, 0x00, 0x0}, // Disable all links

	//Tx/Rx rate Selection
	{0x0010, 0x11, 0x00},// 3Gbps

	// Power up sequence for GMSL1 HIM capable serializers; Also apply to Ser with power up status unknown
	// Turn on HIM on MAX96712
	{0x0C06, 0xEF, 0x0},

#ifndef ONE_PORT
	{0x0B06, 0xEF, 0x0},
	//{0x0C06,0xEF, 0x0},
	{0x0D06, 0xEF, 0x0},
	{0x0E06, 0xEF, 0x0},
#endif
	// disable HS/VS processing
	{0x0C0F, 0x01, 0x0},
#ifndef ONE_PORT
	{0x0B0F, 0x01, 0x0},
	{0x0D0F, 0x01, 0x0},
	{0x0E0F, 0x01, 0x0},
#endif
	{0x0C07, 0x84, 0xa},

#ifndef ONE_PORT
	{0x0B07, 0x84, 0x0},
	{0x0D07, 0x84, 0x0},
	{0x0E07, 0x84, 0x0},
#endif

	// YUV MUX mode
	{0x041A, 0xF0, 0x0},

	//Set i2c path
	//{0x0B04,0x03, 0x0},
	//{0x0B0D,0x81, 0x0},// 设置96706的I2C通讯

	// MAX96712 MIPI settings
	{0x08A0, 0x04, 0x0},
	{0x08A2, 0x30, 0x0},
	{0x00F4, 0x01, 0x0}, //enable 4 pipeline 0xF, enable pipeline 0 0x1
	{0x094A, 0xc0, 0x0},  // Mipi data lane count b6-7 1/2/4lane
	{0x08A3, 0xE4, 0x0}, //0x44 4 lanes data output, 0x4E lane2/3 data output. 0xE4 lane0/1 data output.

	//BPP for pipe lane 0 set as 1E(YUV422)
	{0x040B, 0x80, 0x0}, //0x82
	{0x040C, 0x00, 0x0},
	{0x040D, 0x00, 0x0},
	{0x040E, 0x5E, 0x0},
	{0x040F, 0x7E, 0x0},
	{0x0410, 0x7A, 0x0},
	{0x0411, 0x48, 0x0},
	{0x0412, 0x20, 0x0},

	//lane speed set
	{0x0415, 0xEA, 0x0}, //phy0 lane speed set 0xEF for 1.5g
	{0x0418, 0xEA, 0x0}, //phy1 lane speed set bit0-4 for lane speed. 10001 0xf1 for 1.5g


	//Mapping settings
	{0x090B, 0x07, 0x0},
	{0x092D, 0x15, 0x0},
	{0x090D, 0x1E, 0x0},
	{0x090E, 0x1E, 0x0},
	{0x090F, 0x00, 0x0},
	{0x0910, 0x00, 0x0}, //frame sync frame start map
	{0x0911, 0x01, 0x0},
	{0x0912, 0x01, 0x0},

	//{0x0903,0x80, 0x0},//bigger than 1.5Gbps， Deskew can works.

	{0x094B, 0x07, 0x0},
	{0x096D, 0x15, 0x0},
	{0x094D, 0x1E, 0x0},
	{0x094E, 0x5E, 0x0},
	{0x094F, 0x00, 0x0},
	{0x0950, 0x40, 0x0},
	{0x0951, 0x01, 0x0},
	{0x0952, 0x41, 0x0},

	{0x098B, 0x07, 0x0},
	{0x09AD, 0x15, 0x0},
	{0x098D, 0x1E, 0x0},
	{0x098E, 0x9E, 0x0},
	{0x098F, 0x00, 0x0},
	{0x0990, 0x80, 0x0},
	{0x0991, 0x01, 0x0},
	{0x0992, 0x81, 0x0},

	{0x09CB, 0x07, 0x0},
	{0x09ED, 0x15, 0x0},
	{0x09CD, 0x1E, 0x0},
	{0x09CE, 0xDE, 0x0},//
	{0x09CF, 0x00, 0x0},//
	{0x09D0, 0xC0, 0x0},
	{0x09D1, 0x01, 0x0},
	{0x09D2, 0xC1, 0x0},

#ifdef FRAMESYNC_USE
	//-------------- Frame Sync --------------/
	// set FSYNC period to 25M/30 CLK cycles. PCLK at 25MHz

#ifdef ONE_PORT
	{0x04A2, 0x20, 0x0},
#else
	{0x04A2, 0x00, 0x0},
#endif

	{0x04A7, 0x0F, 0x0},
	{0x04A6, 0x42, 0x0},
	{0x04A5, 0x40, 0x0},

	{0x04AA, 0x00, 0x0},
	{0x04AB, 0x00, 0x0},


	// AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0? GMSL1
#ifdef ONE_PORT
	{0x04AF, 0x42, 0x0},
#else
	{0x04AF, 0x4F, 0x0},
#endif
	{0x04A0, 0x00, 0x0},


	{0x0B08, 0x10, 0x0},
	{0x0C08, 0x10, 0x0},
	{0x0D08, 0x10, 0x0},
	{0x0E08, 0x10, 0x0},
#endif

	//For debug
	{0x0001, 0xcc, 0x0},
	{0x00FA, 0x10, 0x0},

	//Enable mipi
//	{0x040b,0x42, 50},
};

static int max96722_write_reg(deser_dev_t *dev, u16 reg, u8 val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	client->addr = dev->addr_deser;
	buf[0] = (reg & 0xff00) >> 8;
	buf[1] = (reg & 0x00ff);
	buf[2] = val;

	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 3;

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: chip %02x  reg=0x%04x, val=0x%02x\n",
			__func__, dev->addr_deser, reg, val);
		return ret;
	}

	return 0;
}

static int max96722_read_reg(deser_dev_t *dev, u16 reg, u8 *val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[3];
	int ret;

	client->addr = dev->addr_deser;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = 0xee;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = 2;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = &buf[2];
	msg[1].len = 1;


	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: chip %02x  reg=0x%04x, ret=%d\n", __func__, dev->addr_deser, reg, ret);
		return ret;
	}

	*val = buf[2];
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

static int max20087_write_reg(deser_dev_t *dev, u8 reg,
					u8 val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;
	client->addr = dev->addr_poc;
	buf[0] = reg;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 2;
	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: chip %x02x error: reg=0x%02x, val=0x%02x\n",
			__func__, client->addr, reg, val);
		return ret;
	}

	return 0;
}

static int max20087_read_reg(deser_dev_t *dev, u8 reg,
					u8 *val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;
	client->addr = dev->addr_poc;
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
		dev_err(&client->dev, "%s: chip %02x error: reg=0x%02x\n",  __func__, client->addr, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
}

//Power for Rx max96722/712
static int max96722_power(deser_dev_t *dev, bool enable)
{
	if (dev->pwdn_gpio)
		gpiod_direction_output(dev->pwdn_gpio, enable ? 1 : 0);
	return 0;
}

//Power for camera module
static int max20087_power(deser_dev_t *dev, bool enable)
{
	int ret = -EINVAL;

#ifdef CONFIG_POWER_POC_DRIVER
	struct i2c_client *client = dev->i2c_client;
	u8 reg;

	reg = 0x1;
	//pr_debug("sensor->name = %s\n", client->name);
	if (!strcmp(client->name, SDRV_DESER_NAME_I2C)) {
		ret = poc_mdeser0_power(0xf, enable, reg,  0);
	} else if (!strcmp(client->name, SDRV2_DESER_NAME_I2C)) {
		ret = poc_mdeser1_power(0xf, enable, reg,  0);
#if defined(CONFIG_ARCH_SEMIDRIVE_V9)
	} else if (!strcmp(client->name, SDRV_DESER_NAME_I2C_B)) {
		ret = poc_r_mdeser0_power(0xf, enable, reg,  0);
	} else if (!strcmp(client->name, SDRV2_DESER_NAME_I2C_B)) {
		ret = poc_r_mdeser1_power(0xf, enable, reg,  0);
#endif
	} else {
		dev_err(&client->dev, "Can't support POC %s.\n", client->name);
		return ret;
	}
#else
	if (enable == 1)
		ret = max20087_write_reg(dev, 0x01, 0x1f);
	else
		ret = max20087_write_reg(dev, 0x01, 0x10);
#endif

	return ret;
}

static void max96722_preset(deser_dev_t *dev)
{
	//0x00,0x13,0x75,
	max96722_write_reg(dev, 0x0013, 0x75);
	msleep(50);
	return;
}

static void max96722_mipi_enable(deser_dev_t *dev, int en)
{
	//0x04,0x0B,0x42
	if (en == 1)
		max96722_write_reg(dev, 0x040b, 0x42);
	else
		max96722_write_reg(dev, 0x040b, 0x00);
	return;
}

static void max96722_link_enable(deser_dev_t *dev, int en)
{
	if (en == 1)
		max96722_write_reg(dev, 0x0006, 0xF);
	else
		max96722_write_reg(dev, 0x0006, 0x00);
	return;
}

//Should implement this interface
static int start_deser(deser_dev_t *dev, bool en)
{
	if (en == true) {
		msleep(100);
		max96722_link_enable(dev, 1);
		max96722_mipi_enable(dev, 1);
	} else if (en == false) {
		max96722_link_enable(dev, 0);
		max96722_mipi_enable(dev, 0);
	}

	return 0;
}

//Should implement this interface
static int max96705_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
	int i = 0;
	int error_cha = 0;

	for (i = 0; i < 4; i++) {
		ret = max96705_read_reg(dev, i, 0x1E, &chip_id);
		if (ret < 0) {
			error_cha++;
			dev_err(&client->dev,
						"%s ch %d wrong chip identifier,  expected 0x%x(max96705), got 0x%x\n",
						__func__, i, MAX96705_DEVICE_ID, chip_id);
			usleep_range(10000, 11000);
			continue;
		} else {
			printk("\n max96705 dev chipid = 0x%02x\n", chip_id);
			break;
		}
	}
	if (error_cha != 0)
		return error_cha;

	return ret;
}

//Should implement this interface
int max96722_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
	int i = 0;

	ret = max96722_read_reg(dev, 0x000d, &chip_id);

	if (chip_id == MAX96722_DEVICE_ID || chip_id == MAX96712_DEVICE_ID) {
		dev_err(&client->dev, "max96722/12 chipid = 0x%02x\n", chip_id);
	} else {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(max96722), 0x%x(max96712) got 0x%x\n",
			__func__, MAX96722_DEVICE_ID, MAX96712_DEVICE_ID, chip_id);
		return -EIO;
	}

	return ret;
}

void max96722_reg_dump(deser_dev_t *sensor)
{
#ifdef DEBUG_MAX96722
	u8 tmp[32];
	u8 val1;

	msleep(1000);
	max96722_read_reg(sensor, 0x08D0, &tmp[0]);
	max96722_read_reg(sensor, 0x08D2, &tmp[31]);
	max96722_read_reg(sensor, 0x04B6, &tmp[1]);
	max96722_read_reg(sensor, 0x040A, &tmp[2]);
	max96722_read_reg(sensor, 0x11F2, &tmp[3]);

	max96722_read_reg(sensor, 0x0bcb, &tmp[4]);
	max96722_read_reg(sensor, 0x0ccb, &tmp[5]);
	max96722_read_reg(sensor, 0x0dcb, &tmp[6]);
	max96722_read_reg(sensor, 0x0ecb, &tmp[7]);

	max96722_read_reg(sensor, 0x11F0, &tmp[9]);
	max96722_read_reg(sensor, 0x11F1, &tmp[10]);
	max96722_read_reg(sensor, 0x0010, &tmp[11]);
	max96722_read_reg(sensor, 0x040B, &tmp[12]);
	max96722_read_reg(sensor, 0x040C, &tmp[13]);
	max96722_read_reg(sensor, 0x08A3, &val1);
	max96722_read_reg(sensor, 0x0006, &tmp[30]);

	printk("mipi out: 0x08D0=0x%02x\n", tmp[0]);
	printk("mipi out: 0x08D2=0x%02x\n", tmp[31]);
	printk("frameLoc: 0x04B6=0x%02x\n", tmp[1]);
	printk("ErrorHas: 0x040A=0x%02x\n", tmp[2]);
	printk("VsDetect: 0x11F0=0x%02x\n\n", tmp[9]);
	printk("VsDetect: 0x11F1=0x%02x\n\n", tmp[10]);
	printk("VsDetect: 0x11F2=0x%02x\n\n", tmp[3]);
	printk("VsDetect: 0x0010=0x%02x\n\n", tmp[11]);
	printk("VsDetect: 0x040b=0x%02x\n\n", tmp[12]);
	printk("VsDetect: 0x040c=0x%02x\n\n", tmp[13]);
	printk("VsDetect: 0x08A3=0x%02x\n\n", val1);

	printk("LinkA ST: 0x0bcb=0x%02x\n", tmp[4]);
	printk("LinkB ST: 0x0ccb=0x%02x\n", tmp[5]);
	printk("LinkC ST: 0x0dcb=0x%02x\n", tmp[6]);
	printk("LinkD ST: 0x0ecb=0x%02x\n", tmp[7]);
	printk("Link  EN: 0x0006=0x%02x\n", tmp[30]);
#endif
	return;
}


//Should implement this interface
int max96722_initialization(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	u8 value;
	int ret;
	int reglen = sizeof(max96722_reg)/sizeof(max96722_reg[0]);
	int i = 0;

	printk("%s reglen = %d +\n", __FUNCTION__, reglen);

	//Check if deser exists.
	ret = max96722_read_reg(dev, 0x000d, &value);
	if (ret < 0) {
		dev_err(&client->dev, "max96722 fail to read 0x0006=%d\n", ret);
		return ret;
	}

	max96722_preset(dev);
	max96722_mipi_enable(dev, 0);

	//Init max96722
	for (i = 0; i < reglen; i++) {
		max96722_write_reg(dev, max96722_reg[i].nRegAddr, max96722_reg[i].nRegValue);
		msleep(max96722_reg[i].nDelay);
	}

	//Camera Module Power On/Off
	max20087_power(dev, 0);
	usleep_range(10000, 11000);
	max20087_power(dev, 1);
	msleep(100);

	for (i = 0; i < MAX_CAMERA_NUM; i++) {
		//Modify serializer i2c address
		max96722_write_reg(dev, 0x0006, 1<<i);
		usleep_range(10000, 11000);
		max96705_write_reg(dev, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x00, MAX96705_CH_A + i * 2);
		usleep_range(10000, 11000);
		max96705_access_sync(dev, i, MAX96705_WRITE, 0x07, NULL, 0x84);
		usleep_range(1000, 1100);
		max96705_access_sync(dev, i, MAX96705_WRITE, 0x43, NULL, 0x25);
		max96705_access_sync(dev, i, MAX96705_WRITE, 0x45, NULL, 0x01);
		max96705_access_sync(dev, i, MAX96705_WRITE, 0x47, NULL, 0x26);
		usleep_range(10000, 11000);
		max96705_access_sync(dev, i, MAX96705_WRITE, 0x67, NULL, 0xc4);

		max96705_write_reg(dev, i, 0x9, (dev->addr_isp << 1) + i * 2);
		max96705_write_reg(dev, i, 0xa, AP0101_DEF);
	}

	//For enable data
	max96722_mipi_enable(dev, 1);
	max96722_link_enable(dev, 1);
	msleep(20);
	max96722_link_enable(dev, 0);
	printk("%s  -\n", __FUNCTION__);

	return 0;
}

deser_para_t max96722_para = {
	.name = "max96722-712",
	.addr_deser = MAX96722_SLAVE_ID,
	.addr_serer = MAX96705_CH_A >> 1,
	.addr_poc	= MAX20087_SLAVE_ID,
	.addr_gpioext = 0x74,
	.addr_isp	= AP0101_CH_A >> 1,
	.pclk		= 288*1000*1000,
	.mipi_bps	= 1000*1000*1000,
	.width		= 1280,
	.htot		= 1320,
	.height		= 720,
	.vtot		= 740,
	.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps	= 25,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.denominator = 25,
	.used		= DESER_NOT_USED,	// NOT USED IN DEFAULT
	.reset_gpio = NULL,	//if dts can't config reset and pwdn, use these two members.
	.pwdn_gpio	= NULL,
	.power_deser = max96722_power,
	.power_module = max20087_power,
	.detect_deser = max96722_check_chip_id,
	.init_deser = max96722_initialization,
	.start_deser = start_deser,
	.dump_deser	= max96722_reg_dump,
};
