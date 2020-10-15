/*
 * Copyright (C) 2020 semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <ce.h>
#include <sx_sm2.h>
#include <linux/interrupt.h>

#define TRNG_CE2_VCE2_NUM 1

#define SEMIDRIVE_CRYPTO_ALG_SM2 0xaa
#define SEMIDRIVE_CRYPTO_ALG_RSA 0xab

#define SEMIDRIVE_CRYPTO_SM2_INIT _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 1)
#define SEMIDRIVE_CRYPTO_SM2_VERIFY_NO_DIGEST _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 1)
#define SEMIDRIVE_CRYPTO_SM2_VERIFY_DIGEST _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 2)
#define SEMIDRIVE_CRYPTO_SM2_VERIFY_MSG _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 3)
#define SEMIDRIVE_CRYPTO_SM2_SET_PUBKEY _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 4)
#define SEMIDRIVE_CRYPTO_SM2_SIGN _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 4)

#define SEMIDRIVE_CRYPTO_SM2_SET_TIME_STAMP _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 9)
#define SEMIDRIVE_CRYPTO_SM2_GET_TIME_STAMP _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 0xa)

#define CRYPTOLIB_SUCCESS 0
#define CRYPTOLIB_CRYPTO_ERR 6

struct crypto_sm2_msg {
	const uint8_t* msg;
	size_t msg_len;
	const uint8_t* sig;
	size_t sig_len;
	const uint8_t* key;
	size_t key_len;
	uint8_t* ret;
} __attribute__((packed));

//sm2 gb pattern start
/*dB：3945208F 7B2144B1 3F36E38A C6D39F95 88939369 2860B51A 42FB81EF 4DF7C5B8*/
uint8_t __attribute__((aligned(CACHE_LINE))) crypto_gb_prv_key[32] = "\x39\x45\x20\x8F\x7B\x21\x44\xB1\x3F\x36\xE3\x8A\xC6\xD3\x9F\x95\x88\x93\x93\x69\x28\x60\xB5\x1A\x42\xFB\x81\xEF\x4D\xF7\xC5\xB8"; //-- d

/*xB：09F9DF31 1E5421A1 50DD7D16 1E4BC5C6 72179FAD 1833FC07 6BB08FF3 56F35020*/
/*yB：CCEA490C E26775A5 2DC6EA71 8CC1AA60 0AED05FB F35E084A 6632F607 2DA9AD13*/

uint8_t __attribute__((aligned(CACHE_LINE))) crypto_gb_pub_key[64] = "\x09\xF9\xDF\x31\x1E\x54\x21\xA1\x50\xDD\x7D\x16\x1E\x4B\xC5\xC6\x72\x17\x9F\xAD\x18\x33\xFC\x07\x6B\xB0\x8F\xF3\x56\xF3\x50\x20" //-- Qx
                             "\xCC\xEA\x49\x0C\xE2\x67\x75\xA5\x2D\xC6\xEA\x71\x8C\xC1\xAA\x60\x0A\xED\x05\xFB\xF3\x5E\x08\x4A\x66\x32\xF6\x07\x2D\xA9\xAD\x13";  //-- Qy

/*message digest:6D65737361676520646967657374*/
uint8_t crypto_gb_sig_msg[14] = "\x6D\x65\x73\x73\x61\x67\x65\x20\x64\x69\x67\x65\x73\x74"; //-- m
/*IDA GB/T1988: 31323334 35363738 31323334 35363738*/
uint8_t crypto_gb_id_test[16] = "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
/*
r：F5A03B06 48D2C463 0EEAC513 E1BB81A1 5944DA38 27D5B741 43AC7EAC EEE720B3
s：B1B6AA29 DF212FD8 763182BC 0D421CA1 BB9038FD 1F7F42D4 840B69C4 85BBC1AA*/

uint8_t __attribute__((aligned(CACHE_LINE))) crypto_gb_ver_msg[64] = "\xF5\xA0\x3B\x06\x48\xD2\xC4\x63\x0E\xEA\xC5\x13\xE1\xBB\x81\xA1\x59\x44\xDA\x38\x27\xD5\xB7\x41\x43\xAC\x7E\xAC\xEE\xE7\x20\xB3"  //-- r
                             "\xB1\xB6\xAA\x29\xDF\x21\x2F\xD8\x76\x31\x82\xBC\x0D\x42\x1C\xA1\xBB\x90\x38\xFD\x1F\x7F\x42\xD4\x84\x0B\x69\xC4\x85\xBB\xC1\xAA"; //-- s \xAA

uint8_t __attribute__((aligned(CACHE_LINE))) crypto_gb_m[46];
 
uint8_t __attribute__((aligned(CACHE_LINE))) verify_msg_buff[512] = {0};
uint8_t __attribute__((aligned(CACHE_LINE))) verify_sig_buff[128] = {0};
uint8_t __attribute__((aligned(CACHE_LINE))) verify_key_buff[128] = {0};

#define to_crypto_dev(priv) container_of((priv), struct crypto_dev, miscdev)

static long crypto_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct crypto_sm2_msg sm2_msg;
	int err;
    int time_stamp;
	const void * curve;
	struct crypto_dev* crypto;
	block_t message;
	block_t key;
	block_t signature;

	//of_set_sys_cnt_ce(4);

	crypto = to_crypto_dev(filp->private_data);

    switch(_IOC_TYPE(cmd)){
			case SEMIDRIVE_CRYPTO_ALG_SM2:
			{
				//pr_info("crypto_ioctl enter cmd=%d, arg=%p", cmd,(char __user *)arg);
				if(cmd == SEMIDRIVE_CRYPTO_SM2_SET_TIME_STAMP){
					err = copy_from_user((char *)&time_stamp, (char __user *)arg, sizeof(int));
					if (err < 0) {
					//pr_info("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)arg, err);
						return err;
					}
					of_set_sys_cnt_ce(time_stamp);
					return;
				}

				if(cmd == SEMIDRIVE_CRYPTO_SM2_GET_TIME_STAMP){
					ret = of_get_sys_cnt_ce();
					copy_to_user((char __user *)arg, &ret, sizeof(int));
					return;
				}
				//of_set_sys_cnt_ce(5);
				err = copy_from_user((char *)&sm2_msg, (char __user *)arg, sizeof(struct crypto_sm2_msg));
				if (err < 0) {
					//pr_info("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)arg, err);
					return err;
				}

				err = copy_from_user((char *)&verify_msg_buff, (char __user *)(sm2_msg.msg), sm2_msg.msg_len);
				if (err < 0) {
					//pr_info("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(verify_msg.msg), err);
					return err;
				}
				err = copy_from_user((char *)&verify_sig_buff, (char __user *)(sm2_msg.sig),sm2_msg.sig_len);
				if (err < 0) {
					return err;
				}
				if(sm2_msg.key_len > 0){
					err = copy_from_user((char *)&verify_key_buff, (char __user *)(sm2_msg.key), sm2_msg.key_len);
					if (err < 0) {
						return err;
					}
				}
				//curve =	get_curve_value_by_nid(sm2_msg.curve_nid);
				//of_set_sys_cnt_ce(6);
				switch (cmd) {
				case SEMIDRIVE_CRYPTO_SM2_VERIFY_NO_DIGEST:

					message.addr = verify_msg_buff;
					message.addr_type = EXT_MEM;
					message.len = sm2_msg.msg_len;

					key.addr = verify_key_buff;
					key.addr_type = EXT_MEM;
					key.len = sm2_msg.key_len;

					signature.addr = verify_sig_buff;
					signature.addr_type = EXT_MEM;
					signature.len = sm2_msg.sig_len;
                    //sm2_generate_digest must use sx_ecc_sm2_curve_p256 , if use sx_ecc_sm2_curve_p256_rev will error
					ret = sm2_verify_signature((void *)crypto, &sx_ecc_sm2_curve_p256, &message, &key, &signature, ALG_SM3);

					break;
				case SEMIDRIVE_CRYPTO_SM2_VERIFY_DIGEST:
					message.addr = verify_msg_buff;
					message.addr_type = EXT_MEM;
					message.len = sm2_msg.msg_len;

					key.addr = verify_key_buff;
					key.addr_type = EXT_MEM;
					key.len = sm2_msg.key_len;

					signature.addr = verify_sig_buff;
					signature.addr_type = EXT_MEM;
					signature.len = sm2_msg.sig_len;

					ret = sm2_verify_signature_digest_update((void *)crypto, &sx_ecc_sm2_curve_p256_rev, 0, &message, &key, sm2_msg.key_len, &signature);

					break;
				case SEMIDRIVE_CRYPTO_SM2_SET_PUBKEY:
					key.addr = verify_key_buff;
					key.addr_type = EXT_MEM;
					key.len = sm2_msg.key_len;
					ret = sm2_verify_update_pubkey((void *)crypto, &key, sx_ecc_sm2_curve_p256_rev.bytesize);
					break;
				case SEMIDRIVE_CRYPTO_SM2_VERIFY_MSG:
					sm2_compute_id_digest((void *)crypto, crypto_gb_id_test, 16, crypto_gb_m, 32, verify_key_buff, 64);

					memcpy(&crypto_gb_m[32], verify_msg_buff, sm2_msg.msg_len);

					//printf_binary("z msg", crypto_gb_m, 46);

					message.addr = crypto_gb_m;
					message.addr_type = EXT_MEM;
					message.len = sm2_msg.msg_len+32;

					key.addr = verify_key_buff;
					key.addr_type = EXT_MEM;
					key.len = sm2_msg.key_len;

					signature.addr = verify_sig_buff;
					signature.addr_type = EXT_MEM;
					signature.len = sm2_msg.sig_len;

					ret = sm2_generate_signature_update((void *)crypto, &sx_ecc_sm2_curve_p256_rev, &message, &key, &signature, ALG_SM3);
					break;
				default:
					pr_warn("%s: Unhandled ioctl cmd: 0x%x\n",
						__func__, cmd);
					ret = -EINVAL;
				}
				//pr_info("crypto_ioctl out ret=%d", ret);
				//of_set_sys_cnt_ce(7);
				copy_to_user((char __user *)(sm2_msg.ret), &ret, sizeof(int));
				//of_set_sys_cnt_ce(8);
				return ret;
			}
				break;
			case SEMIDRIVE_CRYPTO_ALG_RSA:
				break;
		default:
			pr_warn("%s: Unhandled ioctl cmd: 0x%x\n",
				__func__, cmd);
			ret = -EINVAL;
	}

}

static ssize_t crypto_read(struct file *filp, char __user *userbuf, size_t len,
			    loff_t *f_pos)
{
	return 0;
}


static ssize_t crypto_write(struct file *filp, const char __user *userbuf,
			     size_t len, loff_t *f_pos)
{

	int rc;

	return rc;
}
static int crypto_ip_test(void*device)
{
	int ret;
	block_t message;
	block_t key;
	block_t signature;

	pr_info("crypto_ip_test enter ");

	sm2_compute_id_digest(device, crypto_gb_id_test, 16, crypto_gb_m, 32, crypto_gb_pub_key, 64);

    memcpy(&crypto_gb_m[32], crypto_gb_sig_msg, 14);

	//printf_binary("z msg", crypto_gb_m, 46);
	message.addr = (uint8_t*)&crypto_gb_m;
	message.addr_type = EXT_MEM;
	message.len = 46;
	key.addr = (uint8_t*)&crypto_gb_pub_key;
	key.addr_type = EXT_MEM;
	key.len = 64;
	signature.addr = (uint8_t*)&crypto_gb_ver_msg;
	signature.addr_type = EXT_MEM;
	signature.len = 64;
	ret = sm2_verify_signature(device, &sx_ecc_sm2_curve_p256, &message, &key, &signature, ALG_SM3);
	pr_info("crypto_ip_test crypto_verify_signature ret =%d ", ret);
	return 0;
}

static int crypto_open(struct inode *inode, struct file *filp)
{
	int ret;

	pr_info("crypto_open enter ");
	struct crypto_dev *crypto;

	crypto = to_crypto_dev(filp->private_data);

	//of_set_sys_cnt_ce(1);

	if (!mutex_trylock(&(crypto->lock))) {
		//pr_info("Device Busy\n");
		return -EBUSY;
	}
	//of_set_sys_cnt_ce(2);
	sm2_load_curve(crypto, &sx_ecc_sm2_curve_p256_rev, BA414EP_BIGEND);
//	crypto_ip_test(crypto);
	//of_set_sys_cnt_ce(3);
	return 0;
}


static int crypto_release(struct inode *inode, struct file *filp)
{
	//pr_info("crypto_release enter ");
	struct crypto_dev *crypto;

	crypto = to_crypto_dev(filp->private_data);

	//of_set_sys_cnt_ce(2);
	mutex_unlock(&(crypto->lock));
	return 0;
}

static const struct file_operations crypto_fops = {
	.open		= crypto_open,
	.release	= crypto_release,
	.unlocked_ioctl	= crypto_ioctl,
	.read	= crypto_read,
	.write	= crypto_write,
	.owner		= THIS_MODULE,
};

#if 0
static struct miscdevice crypto_misc_dev0 = {
  	.minor		= MISC_DYNAMIC_MINOR,
  	.name		= "crypto_dev0",
  	.fops		= &crypto_fops
  };
#endif

static irqreturn_t crypto_irq_handler(int irq, void *data)
{
	struct crypto_dev *crypto = data;
	int irq_value;

	irq_value = readl((crypto->base + REG_INTSTAT_CE_(crypto->ce_id)));

	//pr_info("crypto_irq_handler irq_value:%d\n", irq_value);

    writel(0x1f, (crypto->base +REG_INTCLR_CE_(crypto->ce_id)));

	if(irq_value== 0x1){
		crypto->dma_condition = 1;
		wake_up_interruptible(&crypto->cedmawaitq);
	}
	if(irq_value== 0x8){
		//pr_info("crypto_irq_handler wakeup hash %p\n",sm2);
		crypto->hash_condition = 1;
		wake_up_interruptible(&crypto->cehashwaitq);
	}
	if(irq_value== 0x10){
		crypto->pke_condition = 1;
		//wake_up_interruptible(&sm2->cepkewaitq);
	}

	return IRQ_HANDLED;
}

static int semidrive_crypto_probe(struct platform_device *pdev)
{
	struct crypto_dev *crypto_inst;
	struct miscdevice *crypto_fn;
	struct resource *ctrl_reg;
	struct resource *sram_base;
	int ret;
	dev_t devt;
	u32 ce_id;
	int irq;
	pr_info("semidrive_crypto_probe enter");

	crypto_inst = devm_kzalloc(&pdev->dev, sizeof(*crypto_inst), GFP_KERNEL);
	if (!crypto_inst)
		return -ENOMEM;

	platform_set_drvdata(pdev, crypto_inst);

	ctrl_reg = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ctrl_reg");
	crypto_inst->base = devm_ioremap_resource(&pdev->dev, ctrl_reg);

	if (IS_ERR(crypto_inst->base))
		return PTR_ERR(crypto_inst->base);

	sram_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sram_base");
	crypto_inst->sram_base = devm_ioremap_resource(&pdev->dev, sram_base);

	if (IS_ERR(crypto_inst->sram_base))
		return PTR_ERR(crypto_inst->sram_base);
	
	of_property_read_u32(pdev->dev.of_node, "ce_id", &ce_id);

	crypto_inst->ce_id = ce_id;
	crypto_inst->dev = &pdev->dev;

	init_waitqueue_head(&crypto_inst->cehashwaitq);
	init_waitqueue_head(&crypto_inst->cedmawaitq);
	init_waitqueue_head(&crypto_inst->cepkewaitq);

	irq = platform_get_irq(pdev, 0);
	//pr_info("semidrive_crypto_probe irq =%d",irq);
	if (irq < 0) {
		dev_err(&pdev->dev, "Cannot get IRQ resource\n");
		return irq;
	}

	ret = devm_request_threaded_irq(&pdev->dev, irq, crypto_irq_handler,
					NULL, IRQF_ONESHOT,
					dev_name(&pdev->dev), crypto_inst);

    writel(0x1f, (crypto_inst->base +REG_INTCLR_CE_(ce_id)));
    //writel(0xe, (crypto_inst->base +REG_INTEN_CE_(ce_id))); //disable dma/pke intrrupt
	writel(0x0, (crypto_inst->base +REG_INTEN_CE_(ce_id))); //disable all intrrupt

	crypto_fn = &crypto_inst->miscdev;
	sprintf(crypto_inst->name_buff, "semidrive-ce%d", ce_id);

	crypto_fn->minor = MISC_DYNAMIC_MINOR;
  	crypto_fn->name = crypto_inst->name_buff;
  	crypto_fn->fops = &crypto_fops;

	mutex_init(&(crypto_inst->lock));

	ret = misc_register(crypto_fn);

	if (ret) {
		dev_err(&pdev->dev, "failed to register sm2\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id semidrive_crypto_match[] = {
	{ .compatible = "semidrive,silex-ce1" },
	{ }
};

MODULE_DEVICE_TABLE(of, semidrive_crypto_match);

static struct platform_driver semidrive_crypto_driver = {
	.probe		= semidrive_crypto_probe,
	.driver		= {
		.name	= "semidrive-ce",
		.of_match_table = of_match_ptr(semidrive_crypto_match),
	},
};

module_platform_driver(semidrive_crypto_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("semidrive <semidrive@semidrive.com>");
MODULE_DESCRIPTION("semidrive sm2 driver");
