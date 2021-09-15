/**************************************************************************
 *  ksz9896c.c
 *
 *  ksz9896c init code version 1.0
 *
 *  Create Date : 2021/07/31
 *
 *  Modify Date :
 *
 *  Create by   : wxh
 *
 **************************************************************************/
#include <linux/i2c.h>

#include <linux/input.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/pm.h>
#include <linux/earlysuspend.h>
#endif
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
//#include <mach/irqs.h>
//#include <mach/system.h>
//#include <mach/hardware.h>

#include <linux/miscdevice.h>

#include <linux/types.h>

#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/device.h>

#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/rfkill.h>
#include <linux/regulator/consumer.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/crypto.h>

#include <linux/scatterlist.h>

#include <linux/pm.h>

/****************/

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
/*****************/

#define DEVICE_NAME	"ksz9896c"

#define NVP6124B_R0_ID 	0x86

static unsigned int nvp6124_cnt = 1;
typedef unsigned char BYTE;
int chip_id[4]={0};

typedef struct ksz9896c_config
{
	u32 ksz9896c_used;
	u32 twi_id;
	u32 address;
	//struct gpio_config reset_gpio;
	u32 reset_gpio;
	//struct gpio_config pwn_gpio;
	u32 pwdn_gpio;

}ksz9896c_config;




static ksz9896c_config ksz9896c_config_info;

static struct i2c_client *this_client;

static const struct i2c_device_id ksz9896c_id[] = {
	{ DEVICE_NAME, 0 },
	{}
};

//#define __DEBUG_ON
#ifdef __DEBUG_ON
#define printinfo(fmt, arg...)	printk("%s-<%d>: "fmt, __func__, __LINE__, ##arg)
#else
#define printinfo(...)
#endif



#if 1
int SEMI_write_cmos_sensor_w16d16(u16 addr, u16 para)
{
	int ret;
	u8 buf[4] = {0};
	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 4,
			.buf	= buf,
		},
	};

	buf[0] = (addr&0xff00)>>8;
	buf[1] = (addr&0x00ff);
	buf[2] = para&0x00ff;
	buf[3] = (para&&0xff00)>>8;

	ret = i2c_transfer(this_client->adapter, msg, 1);
	return ret;
}

u16 SEMI_I2C_ReadByte16_16(u16 addr)
{
	int ret;
	u8 buf[4] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= &buf[0],
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= 2,
			.buf	= &buf[2],
		},
	};

	buf[0] = (addr&0xff00)>>8;
	buf[1] =  addr&0x00ff;
	buf[2] =0xee;
	buf[3] =0xee;
	//msleep(1);
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret >= 0) {
		//return buf[2]; //8bit ok
		//return buf[2]*256 + buf[3];
		return buf[3]*256 + buf[2];
	} else {
		pr_err("msg %s i2c read error: %d\n", __func__, ret); 
		return 0;
	}
}
#else
int SEMI_write_cmos_sensor_w16d16(u16 addr, u16 para)
{
	int ret;
	u8 buf[4] = {0};
	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 4,
			.buf	= buf,
		},
	};

	buf[0] = (addr&0xff00)>>8;
	buf[1] = (addr&0x00ff);
	buf[2] = para&0x00ff;
	buf[3] = (para&&0xff00)>>8;

	//i2c_smbus_write_word_data(const struct i2c_client *client, u8 command,
	//			      u16 value)
	i2c_smbus_write_word_data(this_client,addr,para);
	ret = i2c_transfer(this_client->adapter, msg, 1);
	return ret;
}
#endif
/********************************************************************************/


static u16 reg_9896c[9][3]={
	{0x01,	0x6F,	0xDD0B},
	{0x01, 	0x8F, 	0x6032},
	{0x01, 	0x9D, 	0x248C},
	{0x01, 	0x75, 	0x0060},
	{0x01, 	0xD3, 	0x7777},
	{0x1C,  0x06, 	0x3008},
	{0x1C, 	0x08, 	0x2001},
	{0x1C,   0x4, 	0x00D0},
	{0x7, 	0x3C, 	0x0000},
};
static int ksz9896c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	int i,j,reg_value;
	/**********************/
	int ret;
	struct device *dev;
	struct device_node *np;
	/***************************/
	/********************************************/
	struct i2c_adapter *adapter = client->adapter;

	/*******************************power & reset******************************/

	dev = &(client->dev);
	np = dev->of_node;

	ksz9896c_config_info.reset_gpio = of_get_named_gpio(np, "ksz9896c,reset-gpio", 0);

	if (ksz9896c_config_info.reset_gpio < 0) {
		ksz9896c_config_info.reset_gpio = -1;
		return -1;
	}

	if( !gpio_is_valid(ksz9896c_config_info.reset_gpio) ) {
		printk(KERN_ERR "ksz9896c reset pin(%u) is invalid\n", ksz9896c_config_info.reset_gpio);
		return -1;
	}
	if ( ksz9896c_config_info.reset_gpio != -1 ) {   // 16/06/24
		ret = gpio_request(ksz9896c_config_info.reset_gpio, "ksz9896c reset");
		printk(KERN_ERR"%s :ksz9896c  reset gpio_request ret = %d\n",__FUNCTION__, ret);
		//gpio_direction_output(ksz9896c_config_info->pdn_gpio, 0);
	}

	ksz9896c_config_info.pwdn_gpio = of_get_named_gpio(np, "ksz9896c,pwdn-gpio", 0);

	if (ksz9896c_config_info.pwdn_gpio < 0) {
		ksz9896c_config_info.pwdn_gpio = -1;
		return -1;
	}

	if( !gpio_is_valid(ksz9896c_config_info.pwdn_gpio) ) {
		printk(KERN_ERR "ksz9896c pdn pin(%u) is invalid\n", ksz9896c_config_info.pwdn_gpio);
		return -1;
	}
	if ( ksz9896c_config_info.pwdn_gpio != -1 ) {   // 16/06/24
		ret = gpio_request(ksz9896c_config_info.pwdn_gpio, "ksz9896c pwdn");
		printk(KERN_ERR"%s :ksz9896c  pwdn gpio_request ret = %d\n",__FUNCTION__, ret);
		//gpio_direction_output(ksz9896c_config_info->pdn_gpio, 0);
	}

	gpio_direction_output(ksz9896c_config_info.reset_gpio, 1);
	msleep(100);
	gpio_direction_output(ksz9896c_config_info.pwdn_gpio, 1);
	msleep(100);
	gpio_direction_output(ksz9896c_config_info.reset_gpio, 0);
	msleep(100);
	/*************************************************************/

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;
	//if((ksz9896c_config_info.twi_id == adapter->nr) && (client->addr == ksz9896c_config_info.address))
	if(client->addr == ksz9896c_config_info.address)
	{
		printk("%s: +++++++++++Detected chip %s at adapter %d, address 0x%02x\n",
				__func__, DEVICE_NAME, i2c_adapter_id(adapter), client->addr);
	}
	else
	{
		printk(KERN_ERR"i2c_check_functionality failed! :*************************\n");	
		err = -ENODEV;
		return err;
	}
	/**********************************************/

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR"i2c_check_functionality I2C_FUNC_I2C failed! :*************************\n");
		err = -ENODEV;
		return err;
	}
	this_client = client;
	chip_id[0] = NVP6124B_R0_ID;

	//printk("SEMI_I2C_ReadByte16 a6301 =0x%x \n",reg_value);
	//sizeof(reg_9896c)/sizeof(reg_9896c[0])
	printk("begin to write to reg_9896c*******************\n");
	for(i=1;i<6;i++)
	{
		for(j=0;j<9;j++)
		{
			SEMI_write_cmos_sensor_w16d16(0x011a|(i<<12),reg_9896c[j][0]);
			SEMI_write_cmos_sensor_w16d16(0x011c|(i<<12),reg_9896c[j][1]);
			SEMI_write_cmos_sensor_w16d16(0x011a|(i<<12),reg_9896c[j][0]|(4<<12));
			SEMI_write_cmos_sensor_w16d16(0x011c|(i<<12),reg_9896c[j][2]);
		}
	}

	reg_value = SEMI_I2C_ReadByte16_16(0x6301);
	printk(KERN_ERR"SEMI_I2C_ReadByte16 a6301 =0x%x \n",reg_value);

	SEMI_write_cmos_sensor_w16d16(0x6301, reg_value|0x0010);
	msleep(10);
	reg_value = SEMI_I2C_ReadByte16_16(0x6301);
	printk("SEMI_I2C_ReadByte16 a6301 =0x%x \n",reg_value);

	reg_value = SEMI_I2C_ReadByte16_16(0x111a);
	printk("I2C_ReadByte16 addr 0x111a =0x%x \n",reg_value);

	printk("************************* \n");
	reg_value = SEMI_I2C_ReadByte16_16(0x0001);
	printk("SEMI_I2C_ReadByte16 addr 0xa0001 =0x%x \n",reg_value);
	reg_value = SEMI_I2C_ReadByte16_16(0x0002);
	printk("SEMI_I2C_ReadByte16 addr 0xa0002 =0x%x \n",reg_value);

	/*****************************************************/
	printk("ksz9896c I2C_WriteByte finished ! :*************************\n");

	return 0;
}

static int  ksz9896c_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);
	return 0;
}


static const unsigned short normal_i2c[] = {0xbe>>1,I2C_CLIENT_END};

static int  ksz9896c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	printk(KERN_ERR "ksz9896c_detect+++++++++++++++++adapter->nr=%d,client->addr=%x\n",adapter->nr,client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;
	//if((ksz9896c_config_info.twi_id == adapter->nr) && (client->addr == ksz9896c_config_info.address))
	if(client->addr == ksz9896c_config_info.address)
	{
		printk(KERN_ERR "%s: +++++++++++Detected chip %s at adapter %d, address 0x%02x\n",
				__func__, DEVICE_NAME, i2c_adapter_id(adapter), client->addr);
		int ret = -1;

		char rdbuf[8];
		memset(rdbuf,0,sizeof(rdbuf));
		struct i2c_msg msgs[] = {
			{
				.addr	= client->addr,
				.flags	= 0,
				.len	= 1,
				.buf	= rdbuf,
			},
			{
				.addr	= client->addr,
				.flags	= I2C_M_RD,
				.len	= 1,
				.buf	= rdbuf,
			},
		};

		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			return -ENODEV;
		strlcpy(info->type, DEVICE_NAME, I2C_NAME_SIZE);
		printinfo("detect ksz9896c chip\n");
		printk(KERN_ERR "detect ksz9896c chip\n");

		return 0;
	}else{
		return -ENODEV;
	}
}



MODULE_DEVICE_TABLE(i2c, ksz9896c_id);

static struct i2c_driver ksz9896c_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		= ksz9896c_probe,
	.remove		= ksz9896c_remove,
	//#ifndef CONFIG_HAS_EARLYSUSPEND
	//#ifdef CONFIG_PM
	// .suspend        = ksz9896c_suspend,
	// .resume         = ksz9896c_resume,
	//#endif
	//#endif
	.id_table	= ksz9896c_id,
	.detect = ksz9896c_detect,
	.driver	= {
		.name	= DEVICE_NAME,
		.owner	= THIS_MODULE,
	},
	.address_list	= normal_i2c,
};

static int script_data_init(void)
{
	//script_item_value_type_e type = 0;
	//script_item_u item_temp;
	//struct device_node *np = NULL;  //wxh
	//struct gpio_config config;
	int ret;
	int ksz9896c_used;
	int twi_id;
	int address;

	ksz9896c_config_info.ksz9896c_used = 1;

	ksz9896c_config_info.twi_id = 1;

	ksz9896c_config_info.address=0x5f;

	return 1;
}

static int __init ksz9896c_init(void)
{
	int ret = -1;
	printk("ksz9896c_init *************\n");
	if(script_data_init() > 0)
	{
		ret = i2c_add_driver(&ksz9896c_driver);
	}
	return ret;
}

static void  __exit ksz9896c_exit(void)
{
	//printinfo("ksz9896c__exit\n");
	printk(KERN_ERR "ksz9896c__exit\n");
	i2c_del_driver(&ksz9896c_driver);


	return;
}

module_init(ksz9896c_init);
module_exit(ksz9896c_exit);

MODULE_AUTHOR("<wxh@semidrive.com>");
MODULE_DESCRIPTION("ksz9896c driver");
MODULE_LICENSE("GPL");

