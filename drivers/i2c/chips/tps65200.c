/* drivers/i2c/chips/tps65200.c
 *
 * Copyright (C) 2009 HTC Corporation
 * Author: Josh Hsiao <Josh_Hsiao@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <mach/htc_battery.h>
#include <asm/mach-types.h>

static const unsigned short normal_i2c[] = { I2C_CLIENT_END };

/**
 * Insmod parameters
 */
I2C_CLIENT_INSMOD_1(tps65200);

static int tps65200_probe(struct i2c_client *client,
			const struct i2c_device_id *id);
static int tps65200_detect(struct i2c_client *client, int kind,
			 struct i2c_board_info *info);
static int tps65200_remove(struct i2c_client *client);


/* Supersonic for Switch charger */
struct tps65200_i2c_client {
    struct i2c_client *client;
    u8 address;
    /* max numb of i2c_msg required is for read =2 */
    struct i2c_msg xfer_msg[2];
    /* To lock access to xfer_msg */
    struct mutex xfer_lock;
};
static struct tps65200_i2c_client tps65200_i2c_module;
/**
Function:tps65200_i2c_write
Target:	Write a byte to Switch charger
Timing:	TBD
INPUT: 	value-> write value
		reg  -> reg offset
		num-> number of byte to write
return :TRUE-->OK
		FALSE-->Fail
 */
static int tps65200_i2c_write(u8 *value, u8 reg, u8 num_bytes)
{
    int ret;
    struct tps65200_i2c_client *tps;
    struct i2c_msg *msg;

    tps = &tps65200_i2c_module;

    mutex_lock(&tps->xfer_lock);
    /*
     * [MSG1]: fill the register address data
     * fill the data Tx buffer
     */
    msg = &tps->xfer_msg[0];
    msg->addr = tps->address;
    msg->len = num_bytes + 1;
    msg->flags = 0;
    msg->buf = value;
    /* over write the first byte of buffer with the register address */
    *value = reg;
    ret = i2c_transfer(tps->client->adapter, tps->xfer_msg, 1);
    mutex_unlock(&tps->xfer_lock);

    /* i2cTransfer returns num messages.translate it pls.. */
    if (ret >= 0)
		ret = 0;
    return ret;
}


/**
Function:tps65200_i2c_read
Target:	Read a byte from Switch charger
Timing:	TBD
INPUT: 	value-> store buffer
		reg  -> reg offset to read
		num-> number of byte to read
return :TRUE-->OK
		FALSE-->Fail
 */
static int tps65200_i2c_read(u8 *value, u8 reg, u8 num_bytes)
{
    int ret;
    u8 val;
    struct tps65200_i2c_client *tps;
    struct i2c_msg *msg;

    tps = &tps65200_i2c_module;

    mutex_lock(&tps->xfer_lock);
    /* [MSG1] fill the register address data */
    msg = &tps->xfer_msg[0];
    msg->addr = tps->address;
    msg->len = 1;
    msg->flags = 0; /* Read the register value */
    val = reg;
    msg->buf = &val;
    /* [MSG2] fill the data rx buffer */
    msg = &tps->xfer_msg[1];
    msg->addr = tps->address;
    msg->flags = I2C_M_RD;  /* Read the register value */
    msg->len = num_bytes;   /* only n bytes */
    msg->buf = value;
    ret = i2c_transfer(tps->client->adapter, tps->xfer_msg, 2);
    mutex_unlock(&tps->xfer_lock);

    /* i2cTransfer returns num messages.translate it pls.. */
    if (ret >= 0)
		ret = 0;
    return ret;
}


/**
Function:tps65200_i2c_write_byte
Target:	Write a byte from Switch charger
Timing:	TBD
INPUT: 	value-> store buffer
		reg  -> reg offset to read
return :TRUE-->OK
		FALSE-->Fail
 */
static int tps65200_i2c_write_byte(u8 value, u8 reg)
{
    /* 2 bytes offset 1 contains the data offset 0 is used by i2c_write */
	int result;
	u8 temp_buffer[2] = { 0 };
    /* offset 1 contains the data */
	temp_buffer[1] = value;
	result = tps65200_i2c_write(temp_buffer, reg, 1);
	if (result != 0)
		pr_info("TPS65200 I2C write fail = %d\n", result);

    return result;
}

/**
Function:tps65200_i2c_read_byte
Target:	Read a byte from Switch charger
Timing:	TBD
INPUT: 	value-> store buffer
		reg  -> reg offset to read
return :TRUE-->OK
		FALSE-->Fail
 */
static int tps65200_i2c_read_byte(u8 *value, u8 reg)
{
	int result = 0;
	result = tps65200_i2c_read(value, reg, 1);
	if (result != 0)
		pr_info("TPS65200 I2C read fail = %d\n", result);

	return result;
}

int tps_set_charger_ctrl(u32 ctl)
{
	int result = 0;
	u8 version;
	u8 status;
	u8 regh;

	switch (ctl) {
	case DISABLE:
		pr_info("Switch charger OFF\n");
		tps65200_i2c_write_byte(0x29, 0x01);
		tps65200_i2c_write_byte(0x28, 0x00);
		tps65200_i2c_read_byte(&status, 0x09);
		pr_info("TPS65200 INT2 %x\n", status);
		break;
	case ENABLE_SLOW_CHG:
		pr_info("Switch charger ON (SLOW)\n");
		tps65200_i2c_write_byte(0x29, 0x01);
		tps65200_i2c_write_byte(0x2A, 0x00);
		tps65200_i2c_write_byte(0x86, 0x03);
		break;
	case ENABLE_FAST_CHG:
		pr_info("Switch charger ON (FAST)\n");
		tps65200_i2c_write_byte(0x29, 0x01);
		tps65200_i2c_write_byte(0x2A, 0x00);
		tps65200_i2c_write_byte(0x86, 0x03);
		tps65200_i2c_write_byte(0xA3, 0x02);
		tps65200_i2c_read_byte(&regh, 0x01);
		pr_info("1.batt: Switch charger ON (FAST): regh 0x01=%x\n", regh);
		tps65200_i2c_read_byte(&regh, 0x00);
		pr_info("2.batt: Switch charger ON (FAST): regh 0x00=%x\n", regh);
		tps65200_i2c_read_byte(&regh, 0x03);
		pr_info("2.batt: Switch charger ON (FAST): regh 0x03=%x\n", regh);
		tps65200_i2c_read_byte(&regh, 0x02);
		pr_info("2.batt: Switch charger ON (FAST): regh 0x02=%x\n", regh);
		break;
	case CHECK_CHG:
		pr_info("Switch charger CHECK \n");
		tps65200_i2c_read_byte(&status, 0x06);
		pr_info("TPS65200 STATUS_A%x\n", status);
		break;
	case SET_ICL500:
		pr_info("Switch charger SET_ICL500 \n");
		tps65200_i2c_write_byte(0xA3, 0x02);
		break;
	case SET_ICL100:
		pr_info("Switch charger SET_ICL100 \n");
		tps65200_i2c_write_byte(0x23, 0x02);
		break;
	case CHECK_INT2:
		pr_info("Switch charger CHECK_INT2 \n");
		tps65200_i2c_read_byte(&status, 0x09);
		pr_info("TPS65200 INT2 %x\n", status);
		break;
	default:
		pr_info("%s: Not supported battery ctr called.!", __func__);
		result = -EINVAL;
		break;
	}

	return result;
}
EXPORT_SYMBOL(tps_set_charger_ctrl);
static int cable_status_handler_func(struct notifier_block *nfb,
		unsigned long action, void *param)
{
	u32 ctl = (u32)action;
	pr_info("TPS65200 Switch charger set control%d\n", ctl);
	tps_set_charger_ctrl(ctl);

	return NOTIFY_OK;
}

static struct notifier_block cable_status_handler = {
	.notifier_call = cable_status_handler_func,
};

static int tps65200_detect(struct i2c_client *client, int kind,
			 struct i2c_board_info *info)
{
	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WRITE_BYTE_DATA |
				     I2C_FUNC_SMBUS_BYTE))
		return -ENODEV;

	strlcpy(info->type, "tps65200", I2C_NAME_SIZE);

	return 0;
}

static int tps65200_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tps65200_i2c_client   *data = &tps65200_i2c_module;

	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0) {
		dev_dbg(&client->dev, "[TPS65200]:I2C fail\n");
		return -EIO;
		}

	register_notifier_cable_status(&cable_status_handler);

	data->address = client->addr;
	data->client = client;
	mutex_init(&data->xfer_lock);
	pr_info("[TPS65200]: Driver registration done\n");
	return 0;
}

static int tps65200_remove(struct i2c_client *client)
{
	struct tps65200_i2c_client   *data = i2c_get_clientdata(client);
	int idx;
	if (data->client && data->client != client)
		i2c_unregister_device(data->client);
	tps65200_i2c_module.client = NULL;
	return 0;
}
static const struct i2c_device_id tps65200_id[] = {
	{ "tps65200", 0 },
	{  },
};
static struct i2c_driver tps65200_driver = {
	.driver.name    = "tps65200",
	.id_table   = tps65200_id,
	.probe      = tps65200_probe,
	.remove     = tps65200_remove,
};

static int __init sensors_tps65200_init(void)
{
    int res;

	res = i2c_add_driver(&tps65200_driver);
	if (res) {
		pr_info("[TPS65200]: Driver registration failed \n");
		return res;
		}
	return res;
}

static void __exit sensors_tps65200_exit(void)
{
	i2c_del_driver(&tps65200_driver);
}

MODULE_AUTHOR("Josh Hsiao <Josh_Hsiao@htc.com>");
MODULE_DESCRIPTION("tps65200 driver");
MODULE_LICENSE("GPL");

module_init(sensors_tps65200_init);
module_exit(sensors_tps65200_exit);
