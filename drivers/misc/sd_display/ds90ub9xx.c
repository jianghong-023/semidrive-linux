#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/time.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/i2c.h>

enum ds90ub9xx_ic {
	DS90UB941 = 0,
	DS90UB948,
	DS90UB9XX_MAX
};

struct ds90ub9xx_data {
	struct i2c_client *client;
	u8 addr_ds9xx[DS90UB9XX_MAX];
};

static int ds90ub9xx_i2c_read(enum ds90ub9xx_ic ic ,struct ds90ub9xx_data *data,
	u8 reg, u8 *buf, int len)
{
	struct i2c_msg msgs[2];
	int ret;

	msgs[0].flags = 0;
	msgs[0].addr  = data->addr_ds9xx[ic];
	msgs[0].len   = 1;
	msgs[0].buf   = &reg;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = data->addr_ds9xx[ic];
	msgs[1].len   = len;
	msgs[1].buf   = buf;

	ret = i2c_transfer(data->client->adapter, msgs, 2);
	return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

static int ds90ub9xx_i2c_write(enum ds90ub9xx_ic ic, struct ds90ub9xx_data *data,
	u8 reg, u8 *buf, int len)
{
	u8 *addr_buf;
	struct i2c_msg msg;
	int ret;

	addr_buf = kmalloc(len + 1, GFP_KERNEL);

	if (!addr_buf)
		return -ENOMEM;

	addr_buf[0] = reg;
	memcpy(&addr_buf[1], buf, len);

	msg.flags = 0;
	msg.addr  = data->addr_ds9xx[ic];
	msg.buf   = addr_buf;
	msg.len   = len + 1;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	kfree(addr_buf);
	return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

static void ds90ub941_init(struct ds90ub9xx_data *data)
{
	u8 dreg, dval;
	enum ds90ub9xx_ic ic = DS90UB941;
	pr_info("%s enter\n", __func__);

	//dreg = 0x01; dval = 0x8;
	//ds90ub9xx_i2c_write(ic, data, dreg, &dval,1); //Disable DSI
	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x1E; dval = 0x04;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Use I2D ID+1 for FPD-Link III Port 1 register access
	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x03; dval = 0x9A;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Enable I2C_PASSTHROUGH, FPD-Link III Port 0
	dreg = 0x1E; dval = 0x02;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 1
	dreg = 0x03; dval = 0x9A;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Enable I2C_PASSTHROUGH, FPD-Link III Port 1

	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x40; dval = 0x05;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI Port 0 digital registers
	dreg = 0x41; dval = 0x21;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI_CONFIG_1 register
	dreg = 0x42; dval = 0x60;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Set DSI_VS_POLARITY=DSI_HS_POLARITY=1(active low)

	dreg = 0x1E; dval = 0x02;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 1
	dreg = 0x40; dval = 0x09;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI Port 1 digital registers
	dreg = 0x41; dval = 0x21;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI_CONFIG_1 register
	dreg = 0x42; dval = 0x60;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Set DSI_VS_POLARITY=DSI_HS_POLARITY=1(active low)

	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x5B; dval = 0x05;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Force Independent 2:2 mode
	//dreg = 0x5B; dval = 0x01;
	//ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Forced Single FPD-Link III Transmitter mode (Port 1 disabled)
	dreg = 0x4F; dval = 0x8C;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Set DSI_CONTINUOUS_CLOCK, 4 lanes, DSI Port 0

	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x40; dval = 0x04;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI Port 0 digital registers
	dreg = 0x41; dval = 0x05;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DPHY_SKIP_TIMING register
#if defined(MIPI_KD070_SERDES_1024X600_LCD)
	dreg = 0x42; dval = 0x10;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Write TSKIP_CNT value for 200 MHz DSI clock frequency (1920x720, Round(65x0.2) -5)
#else
	dreg = 0x42; dval = 0x15;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Write TSKIP_CNT value for 300 MHz DSI clock frequency (1920x720, Round(65x0.3) -5)
#endif

	dreg = 0x1E; dval = 0x02;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 1
	dreg = 0x4F; dval = 0x8C;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Set DSI_CONTINUOUS_CLOCK, 4 lanes, DSI Port 1

	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x40; dval = 0x08;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI Port 1 digital registers
	dreg = 0x41; dval = 0x05;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DPHY_SKIP_TIMING register
#if defined(MIPI_KD070_SERDES_1024X600_LCD)
	dreg = 0x42; dval = 0x10;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Write TSKIP_CNT value for 200 MHz DSI clock frequency (1920x720, Round(65x0.2) -5)
#else
	dreg = 0x42; dval = 0x15;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Write TSKIP_CNT value for 300 MHz DSI clock frequency (1920x720, Round(65x0.3) -5)
#endif
	dreg = 0x10; dval = 0x00;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Enable DSI

	pr_info("%s done\n", __func__);
}

static void ds90ub948_init(struct ds90ub9xx_data *data)
{
	u8 dreg, dval;
	enum ds90ub9xx_ic ic = DS90UB948;
	pr_info("%s enter\n", __func__);

#if defined(MIPI_KD070_SERDES_1024X600_LCD)
	if (0) {/*lastest hardware this not need cfg*/
		dreg = 0x1E; dval = 0x09;
		ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);
		ds90ub9xx_i2c_read(ic, data, dreg, &dval, 1);
		pr_info("%s: read: dreg=0x1D, dval=0x%x\n", __func__, dval);

		dreg = 0x20; dval = 0x09;
		ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);
		ds90ub9xx_i2c_read(ic, data, dreg, &dval, 1);
		pr_info("%s: read: dreg=0x20, dval=0x%x\n", __func__, dval);
	}
#else
	dreg = 0x49; dval = 0x60;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//set MAPSEL high
	ds90ub9xx_i2c_read(ic, data, dreg, &dval, 1);
	pr_info("%s: read: dreg=0x49, dval=0x%x\n", __func__, dval);
#endif

	pr_info("%s done\n", __func__);
}

static int ds90ub9xx_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ds90ub9xx_data *data;
	int ret = 0;
	u32 val;

	pr_info("I2C Address: 0x%02x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("I2C check functionality failed.\n");
		return -ENXIO;
	}

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	data->client = client;
	i2c_set_clientdata(client, data);

	/*get resource from dts*/
	ret = of_property_read_u32(client->dev.of_node, "addr_ds941", &val);
	if (ret < 0) {
		pr_err("Missing addr_ds941\n");
	}

	data->addr_ds9xx[DS90UB941] = (u8)val;

	ret = of_property_read_u32(client->dev.of_node, "addr_ds948", &val);

	if (ret < 0) {
		pr_err("Missing addr_ds948\n");
	}

	data->addr_ds9xx[DS90UB948] = (u8)val;

	pr_info("data->addr_ds941=0x%x, data->addr_ds948=0x%x\n",
		data->addr_ds9xx[DS90UB941], data->addr_ds9xx[DS90UB948]);

	ds90ub941_init(data);
	ds90ub948_init(data);

	return ret;
}

static int ds90ub9xx_remove(struct i2c_client *pdev)
{
	return 0;
}

static const struct of_device_id ds90ub9xx_dt_ids[] = {
	{.compatible = "semidrive,ds90ub9xx",},
	{}
};

static struct i2c_driver ds90ub9xx_driver = {
	.probe = ds90ub9xx_probe,
	.remove = ds90ub9xx_remove,
	.driver = {
		.name = "ds90ub9xx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ds90ub9xx_dt_ids),
	},
};

module_i2c_driver(ds90ub9xx_driver);

MODULE_DESCRIPTION("DS90U9XX Driver");
MODULE_LICENSE("GPL");
