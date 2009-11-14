/*
  Copyright (C) 2009 Troy Kisky, Boundary Devices Inc.
  <troy.kisky@boundarydevices.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <video/davincifb_config.h>

#define MAX_REGS 0x8a

/* Each client has this additional data */
struct ths8200_data {
	struct i2c_client	*client;
	u8 reg;
	u8 spare1,spare2,spare3;
	u8 reg_buf[MAX_REGS];
};
static struct ths8200_data *gdata;

struct i2c_registers_t {
	unsigned char regno ;
	unsigned char value ;
};
static struct i2c_registers_t const i2c_static_regs[] = {
   { 0x1c, 0x38 }		// data path control: no ifir filters
,  { 0x38, 0x87 }		// dtg on/mode VESA slave
,  { 0x4a, 0x09 }		// mult off, CSM clipping/scaling/mult factors (0x8a, for u-boot shadow)
,  { 0x4b, 0x12 }		// multiplication constants: (0.7V/1.3V)*1024 == 0x2e... brightened til U-Boot shadow disappears (0x22)
,  { 0x4c, 0x80 }
,  { 0x4d, 0x80 }
,  { 0x4e, 0x80 }
,  { 0x4f, 0x00 }		// mult off
};
#define num_static_i2c (sizeof(i2c_static_regs)/sizeof(i2c_static_regs[0]))

static int ths_write(u_int8_t *buf, int len)
{
	struct ths8200_data *data = gdata;
	int retval;
	if (!len)
		return -EINVAL;
	retval = i2c_master_send(data->client, buf, len);
	if (retval != len)
		printk(KERN_ERR "%s: Error setting reg %02x = %02x, retval=%i (expected %i)\n",
				__func__, buf[0], buf[1], retval, len);
	else {
		int i = 1;
		unsigned reg = buf[0];
		if (0) printk(KERN_ERR "%s: reg %02x = %02x, len=%i\n",
				__func__, buf[0], buf[1], len);
		data->reg = (u8)reg;
		while (i < len) {
			if (reg >= MAX_REGS)
				break;
			data->reg_buf[reg++] = buf[i++];
		}
	}
	return retval;
}

void ths8200_setup(const DISPLAYCFG* disp)
{
	int totalh, totalv;
	unsigned vsync_length ;
	unsigned hs_in_dly, vs_in_dly;
	unsigned hdly, vdly;
	unsigned field_size;
	int i = 0;
	int retval;
	unsigned char buf[13 + 1];	/* 0x70-0x7c */

	if (!gdata)
		return;

	totalh = disp->xres+disp->hsync_len+disp->left_margin+disp->right_margin;
	totalv = disp->yres+disp->vsync_len+disp->upper_margin+disp->lower_margin;

	hs_in_dly = disp->hsync_len + disp->left_margin;
	vs_in_dly = disp->vsync_len + disp->upper_margin;
	hdly = disp->xres + disp->right_margin;
	/*
	 * hdly += 1 if mult used (if 0x4a bit 7, 0x4f bit 7,6 == 1)
	 * hdly += 8 if CSC used (if 0x19 bit 1 == 0)
	 * hdly += 36 (if ifir12 not bypassed)(if 0x1c bit 5 == 0)
	 * hdly += 18 if 2x interpolation used (if ifir35 not bypassed)(if 0x1c bit 4 == 0)
	 */
	vdly = disp->yres + disp->lower_margin;

	/* put in reset state */
	buf[0] = 0x03;
	buf[1] = 0;
	do {
		retval = ths_write(buf, 2);
		if (retval == 2)
			break;
		i++;
		if (i >= 10) {
			return;
		}
	} while (1);

	for( i = 0 ; i < num_static_i2c ; i++ ){
		struct i2c_registers_t const *reg = i2c_static_regs+i;
		buf[0] = reg->regno;
		buf[1] = reg->value;
		ths_write(buf, 2);
	}

	// set horizontal total length
	buf[0] = 0x34;
	buf[0x34 - 0x34 + 1] = (unsigned char)((totalh & 0x1f00) >> 8);
	buf[0x35 - 0x34 + 1] = (unsigned char)totalh;
	ths_write(buf, 3);

	// set vertical total length
	field_size = totalv + 1;
	buf[0] = 0x39;
	buf[0x39 - 0x39 + 1] = (unsigned char)(((totalv & 0x700) >> 4) |
			((field_size & 0x700) >> 8));
	buf[0x3a - 0x39 + 1] = (unsigned char)totalv;
	buf[0x3b - 0x39 + 1] = (unsigned char)field_size;
	ths_write(buf, 4);

	// set horizontal pulse width and offset
/* hlength 7:0 */
	buf[0] = 0x70;
	buf[0x70 - 0x70 + 1] = (unsigned char)disp->hsync_len;
	buf[0x71 - 0x70 + 1] = (unsigned char)
/* hlength 9:8 */
		(((disp->hsync_len & 0x300) >> 2) |
/* hdly 12:8 */
		((hdly & 0x1f00) >> 8));
/* hdly 7:0 */
	buf[0x72 - 0x70 + 1] = (unsigned char)hdly;
	vsync_length = (disp->vsync_len) + 1;	 /* +1 is needed for some reason, bad docs */
/* vlength1 7:0 */
	buf[0x73 - 0x70 + 1] = (unsigned char)vsync_length;
	buf[0x74 - 0x70 + 1] = (unsigned char)
/* vlength1 9:8 */
		(((vsync_length & 0x300) >> 2) |
/* vdly1 10:8 */
		((vdly & 0x700) >> 8));
/* vdly1 7:0 */
	buf[0x75 - 0x70 + 1] = (unsigned char)vdly;
/* vlength2 7:0 */
	buf[0x76 - 0x70 + 1] = (unsigned char)0;
	buf[0x77 - 0x70 + 1] = (unsigned char)
/* vlength2 9:8 */
		(((0 & 0x300) >> 2) |
/* vdly2 10:8*/
		((0x7ff & 0x700) >> 8));
/* vdly2 7:0 */
	buf[0x78 - 0x70 + 1] = (unsigned char)0x7ff;
/* hs_in_dly 12:8 */
	buf[0x79 - 0x70 + 1] = (unsigned char)((hs_in_dly & 0x1f00) >> 8);
/* hs_in_dly 7:0 */
	buf[0x7a - 0x70 + 1] = (unsigned char)hs_in_dly;
/* vs_in_dly 10:8 */
	buf[0x7b - 0x70 + 1] = (unsigned char)((vs_in_dly & 0x1f00) >> 8);
/* vs_in_dly 7:0 */
	buf[0x7c - 0x70 + 1] = (unsigned char)vs_in_dly;
	ths_write(buf, 14);

	buf[0] = 0x82;
	buf[0x82 - 0x82 + 1] = ((0!=disp->vsyn_acth)<<1)|((0!=disp->hsyn_acth)<<0);
	buf[0x82 - 0x82 + 1] |= (buf[0x82 - 0x82 + 1] & 3) << 3;
	buf[0x82 - 0x82 + 1] |= 0x44;
	ths_write(buf, 2);
	/* release from reset state */
	buf[0] = 0x03;
	buf[1] = 1;
	ths_write(buf, 2);
}
EXPORT_SYMBOL(ths8200_setup);

/* following are the sysfs callback functions */
static ssize_t read_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ths8200_data *data = i2c_get_clientdata(client);
	unsigned val;
	int len;
	u8 i2c_buf[2];

	i2c_buf[0] = data->reg;
	len = i2c_master_send(client, i2c_buf, 1);
	if (len != 1)
		return -EIO; 
	len = i2c_master_recv(client, i2c_buf, 1);
	if (len != 1)
		return -EIO; 
	val = i2c_buf[0];
	return sprintf(buf, "%02x\n", val);
}

static DEVICE_ATTR(read, S_IRUGO, read_show, NULL);

static ssize_t write_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct ths8200_data *data = dev_get_drvdata(dev);
	int i = 0;
	int cnt = 0;
	/* we have 4096 bytes, plenty of space */
	while (i < MAX_REGS) {
		if (!(i & 0xf))
			cnt += sprintf(buf + cnt, "%02x:", i);
		if ((i & 0xf) == 8)
			cnt += sprintf(buf + cnt, " ");
		cnt += sprintf(buf + cnt, " %02x", data->reg_buf[i++]);
		if (!(i & 0xf))
			cnt += sprintf(buf + cnt, "\n");
	}
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t write_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ths8200_data *data = i2c_get_clientdata(client);
	unsigned long reg, value;
	u8 write_buf[40];
	char *ibuf = (char *)buf;
	char *ebuf;
	char *endbuf = ibuf + count;
	int i;
	while (*ibuf == ' ')
		ibuf++;
	reg = simple_strtoul(ibuf, &ebuf, 16);
	if ((reg >= MAX_REGS) || (ibuf == ebuf))
		return -EINVAL;
	ibuf = ebuf;
	i = 0;
	data->reg = write_buf[i++] = reg;
	do {
		while (*ibuf == ' ')
			ibuf++;
		if (!*ibuf)
			break;
		if (ibuf >= endbuf)
			break;
		if (reg >= MAX_REGS)
			return -EINVAL;
		value = simple_strtoul(ibuf, &ebuf, 16);
		if ((value > 0xff) || (ibuf == ebuf))
			return -EINVAL;
		ibuf = ebuf;
		data->reg_buf[reg++] = write_buf[i++] = (u8)value;
	} while (i < 40);
	
	i2c_master_send(client, write_buf, i);
	return count;
}

static DEVICE_ATTR(write, S_IWUSR | S_IRUGO, write_show, write_store);

static struct attribute *ths8200_attributes[] = {
	&dev_attr_read.attr,
	&dev_attr_write.attr,
	NULL
};

static const struct attribute_group ths8200_attr_group = {
	.attrs = ths8200_attributes,
};

/* Return 0 if detection is successful, -ENODEV otherwise */
static int ths8200_detect(struct i2c_client *client, int kind,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;
	strlcpy(info->type, "ths8200", I2C_NAME_SIZE);
	return 0;
}

static int ths8200_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct ths8200_data *data;
	struct device *dev = &client->dev;
	int err;

	data = kzalloc(sizeof(struct ths8200_data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "failed to allocate space\n");
		return -ENOMEM;
	}
	data->client = client;
	i2c_set_clientdata(client, data);

	err = sysfs_create_group(&client->dev.kobj, &ths8200_attr_group);
	if (err)
		kfree(data);
	else {
		gdata = data;
		printk("%s: success\n", __func__);
	}
	return err;
}

static int ths8200_remove(struct i2c_client *client)
{
	struct ths8200_data *data = i2c_get_clientdata(client);
	sysfs_remove_group(&client->dev.kobj, &ths8200_attr_group);
	gdata = NULL;
	kfree(data);
	return 0;
}

static const struct i2c_device_id ths8200_idtable[] = {
	{ "ths8200", 0 },
	{ }
};

static struct i2c_driver ths8200_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ths8200",
	},
	.id_table	= ths8200_idtable,
	.probe		= ths8200_probe,
	.remove		= __devexit_p(ths8200_remove),
	.detect		= ths8200_detect,
};

static int __init ths8200_init(void)
{
	return i2c_add_driver(&ths8200_driver);
}

static void __exit ths8200_exit(void)
{
	i2c_del_driver(&ths8200_driver);
}

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("ths8200 driver");
MODULE_LICENSE("GPL");

subsys_initcall(ths8200_init);
module_exit(ths8200_exit);
