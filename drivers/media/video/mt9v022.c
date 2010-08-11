/*
 * Driver for MT9V022 CMOS Image Sensor from Micron
 *
 * Copyright (C) 2008, Guennadi Liakhovetski <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/log2.h>

#include <media/v4l2-subdev.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>

/* mt9v022 i2c address 0x48, 0x4c, 0x58, 0x5c
 * The platform has to define ctruct i2c_board_info objects and link to them
 * from struct soc_camera_link */

static char *sensor_type;
module_param(sensor_type, charp, S_IRUGO);
MODULE_PARM_DESC(sensor_type, "Sensor type: \"colour\" or \"monochrome\"");

/* mt9v022 selected register addresses */
#define MT9V022_CHIP_VERSION		0x00
#define MT9V022_COLUMN_START		0x01
#define MT9V022_ROW_START		0x02
#define MT9V022_WINDOW_HEIGHT		0x03
#define MT9V022_WINDOW_WIDTH		0x04
#define MT9V022_HORIZONTAL_BLANKING	0x05
#define MT9V022_VERTICAL_BLANKING	0x06
#define MT9V022_CHIP_CONTROL		0x07
#define MT9V022_SHUTTER_WIDTH1		0x08
#define MT9V022_SHUTTER_WIDTH2		0x09
#define MT9V022_SHUTTER_WIDTH_CTRL	0x0a
#define MT9V022_TOTAL_SHUTTER_WIDTH	0x0b
#define MT9V022_RESET			0x0c
#define MT9V022_READ_MODE		0x0d
#define MT9V022_MONITOR_MODE		0x0e
#define MT9V022_PIXEL_OPERATION_MODE	0x0f
#define MT9V022_LED_OUT_CONTROL		0x1b
#define MT9V022_ADC_MODE_CONTROL	0x1c
#define MT9V022_ANALOG_GAIN		0x35
#define MT9V022_BLACK_LEVEL_CALIB_CTRL	0x47
#define MT9V022_PIXCLK_FV_LV		0x74
#define MT9V022_DIGITAL_TEST_PATTERN	0x7f
#define MT9V022_AEC_AGC_ENABLE		0xAF
#define MT9V022_MAX_TOTAL_SHUTTER_WIDTH	0xBD

/* Progressive scan, master, defaults */
#define MT9V022_CHIP_CONTROL_DEFAULT	0x188

#define MT9V022_MAX_WIDTH		752
#define MT9V022_MAX_HEIGHT		480
#define MT9V022_MIN_WIDTH		48
#define MT9V022_MIN_HEIGHT		32
#define MT9V022_COLUMN_SKIP		1
#define MT9V022_ROW_SKIP		4

static const struct soc_camera_data_format mt9v022_colour_formats[] = {
	/* Order important: first natively supported,
	 * second supported with a GPIO extender */
	{
		.name		= "Bayer (sRGB) 10 bit",
		.depth		= 10,
		.fourcc		= V4L2_PIX_FMT_SBGGR16,
		.colorspace	= V4L2_COLORSPACE_SRGB,
	}, {
		.name		= "Bayer (sRGB) 8 bit",
		.depth		= 8,
		.fourcc		= V4L2_PIX_FMT_SBGGR8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
	}
};

static const struct soc_camera_data_format mt9v022_monochrome_formats[] = {
	/* Order important - see above */
	{
		.name		= "Monochrome 10 bit",
		.depth		= 10,
		.fourcc		= V4L2_PIX_FMT_Y16,
	}, {
		.name		= "Monochrome 8 bit",
		.depth		= 8,
		.fourcc		= V4L2_PIX_FMT_GREY,
	},
};

struct mt9v022 {
	struct v4l2_subdev subdev;
	struct v4l2_rect rect;	/* Sensor window */
	__u32 fourcc;
	int model;	/* V4L2_IDENT_MT9V022* codes from v4l2-chip-ident.h */
	u16 chip_control;
};

static struct mt9v022 *to_mt9v022(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct mt9v022, subdev);
}

static int reg_read(struct i2c_client *client, const u8 reg)
{
	s32 data = i2c_smbus_read_word_data(client, reg);
	return data < 0 ? data : swab16(data);
}

static int reg_write(struct i2c_client *client, const u8 reg,
		     const u16 data)
{
	return i2c_smbus_write_word_data(client, reg, swab16(data));
}

static int reg_set(struct i2c_client *client, const u8 reg,
		   const u16 data)
{
	int ret;

	ret = reg_read(client, reg);
	if (ret < 0)
		return ret;
	return reg_write(client, reg, ret | data);
}

static int reg_clear(struct i2c_client *client, const u8 reg,
		     const u16 data)
{
	int ret;

	ret = reg_read(client, reg);
	if (ret < 0)
		return ret;
	return reg_write(client, reg, ret & ~data);
}

static int mt9v022_init(struct i2c_client *client)
{
	struct mt9v022 *mt9v022 = to_mt9v022(client);
	int ret;

	/* Almost the default mode: master, parallel, simultaneous, and an
	 * undocumented bit 0x200, which is present in table 7, but not in 8,
	 * plus snapshot mode to disable scan for now */
	mt9v022->chip_control |= 0x10;
	ret = reg_write(client, MT9V022_CHIP_CONTROL, mt9v022->chip_control);
	if (!ret)
		ret = reg_write(client, MT9V022_READ_MODE, 0x300);

	/* All defaults */
	if (!ret)
		/* AEC, AGC on */
		ret = reg_set(client, MT9V022_AEC_AGC_ENABLE, 0x3);
	if (!ret)
		ret = reg_write(client, MT9V022_ANALOG_GAIN, 16);
	if (!ret)
		ret = reg_write(client, MT9V022_TOTAL_SHUTTER_WIDTH, 480);
	if (!ret)
		ret = reg_write(client, MT9V022_MAX_TOTAL_SHUTTER_WIDTH, 480);
	if (!ret)
		/* default - auto */
		ret = reg_clear(client, MT9V022_BLACK_LEVEL_CALIB_CTRL, 1);
	if (!ret)
		ret = reg_write(client, MT9V022_DIGITAL_TEST_PATTERN, 0);

	return ret;
}

static int mt9v022_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = sd->priv;
	struct mt9v022 *mt9v022 = to_mt9v022(client);

	if (enable)
		/* Switch to master "normal" mode */
		mt9v022->chip_control &= ~0x10;
	else
		/* Switch to snapshot mode */
		mt9v022->chip_control |= 0x10;

	if (reg_write(client, MT9V022_CHIP_CONTROL, mt9v022->chip_control) < 0)
		return -EIO;
	return 0;
}

static int mt9v022_set_bus_param(struct soc_camera_device *icd,
				 unsigned long flags)
{
	struct i2c_client *client = to_i2c_client(to_soc_camera_control(icd));
	struct mt9v022 *mt9v022 = to_mt9v022(client);
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	unsigned int width_flag = flags & SOCAM_DATAWIDTH_MASK;
	int ret;
	u16 pixclk = 0;

	/* Only one width bit may be set */
	if (!is_power_of_2(width_flag))
		return -EINVAL;

	if (icl->set_bus_param) {
		ret = icl->set_bus_param(icl, width_flag);
		if (ret)
			return ret;
	} else {
		/*
		 * Without board specific bus width settings we only support the
		 * sensors native bus width
		 */
		if (width_flag != SOCAM_DATAWIDTH_10)
			return -EINVAL;
	}

	flags = soc_camera_apply_sensor_flags(icl, flags);

	if (flags & SOCAM_PCLK_SAMPLE_RISING)
		pixclk |= 0x10;

	if (!(flags & SOCAM_HSYNC_ACTIVE_HIGH))
		pixclk |= 0x1;

	if (!(flags & SOCAM_VSYNC_ACTIVE_HIGH))
		pixclk |= 0x2;

	ret = reg_write(client, MT9V022_PIXCLK_FV_LV, pixclk);
	if (ret < 0)
		return ret;

	if (!(flags & SOCAM_MASTER))
		mt9v022->chip_control &= ~0x8;

	ret = reg_write(client, MT9V022_CHIP_CONTROL, mt9v022->chip_control);
	if (ret < 0)
		return ret;

	dev_dbg(&client->dev, "Calculated pixclk 0x%x, chip control 0x%x\n",
		pixclk, mt9v022->chip_control);

	return 0;
}

static unsigned long mt9v022_query_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	unsigned int width_flag;

	if (icl->query_bus_param)
		width_flag = icl->query_bus_param(icl) &
			SOCAM_DATAWIDTH_MASK;
	else
		width_flag = SOCAM_DATAWIDTH_10;

	return SOCAM_PCLK_SAMPLE_RISING | SOCAM_PCLK_SAMPLE_FALLING |
		SOCAM_HSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_LOW |
		SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_VSYNC_ACTIVE_LOW |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_MASTER | SOCAM_SLAVE |
		width_flag;
}

static int mt9v022_s_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = sd->priv;
	struct mt9v022 *mt9v022 = to_mt9v022(client);
	struct v4l2_rect rect = a->c;
	struct soc_camera_device *icd = client->dev.platform_data;
	int ret;

	/* Bayer format - even size lengths */
	if (mt9v022->fourcc == V4L2_PIX_FMT_SBGGR8 ||
	    mt9v022->fourcc == V4L2_PIX_FMT_SBGGR16) {
		rect.width	= ALIGN(rect.width, 2);
		rect.height	= ALIGN(rect.height, 2);
		/* Let the user play with the starting pixel */
	}

	soc_camera_limit_side(&rect.left, &rect.width,
		     MT9V022_COLUMN_SKIP, MT9V022_MIN_WIDTH, MT9V022_MAX_WIDTH);

	soc_camera_limit_side(&rect.top, &rect.height,
		     MT9V022_ROW_SKIP, MT9V022_MIN_HEIGHT, MT9V022_MAX_HEIGHT);

	/* Like in example app. Contradicts the datasheet though */
	ret = reg_read(client, MT9V022_AEC_AGC_ENABLE);
	if (ret >= 0) {
		if (ret & 1) /* Autoexposure */
			ret = reg_write(client, MT9V022_MAX_TOTAL_SHUTTER_WIDTH,
					rect.height + icd->y_skip_top + 43);
		else
			ret = reg_write(client, MT9V022_TOTAL_SHUTTER_WIDTH,
					rect.height + icd->y_skip_top + 43);
	}
	/* Setup frame format: defaults apart from width and height */
	if (!ret)
		ret = reg_write(client, MT9V022_COLUMN_START, rect.left);
	if (!ret)
		ret = reg_write(client, MT9V022_ROW_START, rect.top);
	if (!ret)
		/* Default 94, Phytec driver says:
		 * "width + horizontal blank >= 660" */
		ret = reg_write(client, MT9V022_HORIZONTAL_BLANKING,
				rect.width > 660 - 43 ? 43 :
				660 - rect.width);
	if (!ret)
		ret = reg_write(client, MT9V022_VERTICAL_BLANKING, 45);
	if (!ret)
		ret = reg_write(client, MT9V022_WINDOW_WIDTH, rect.width);
	if (!ret)
		ret = reg_write(client, MT9V022_WINDOW_HEIGHT,
				rect.height + icd->y_skip_top);

	if (ret < 0)
		return ret;

	dev_dbg(&client->dev, "Frame %ux%u pixel\n", rect.width, rect.height);

	mt9v022->rect = rect;

	return 0;
}

static int mt9v022_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = sd->priv;
	struct mt9v022 *mt9v022 = to_mt9v022(client);

	a->c	= mt9v022->rect;
	a->type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int mt9v022_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= MT9V022_COLUMN_SKIP;
	a->bounds.top			= MT9V022_ROW_SKIP;
	a->bounds.width			= MT9V022_MAX_WIDTH;
	a->bounds.height		= MT9V022_MAX_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int mt9v022_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *f)
{
	struct i2c_client *client = sd->priv;
	struct mt9v022 *mt9v022 = to_mt9v022(client);
	struct v4l2_pix_format *pix = &f->fmt.pix;

	pix->width		= mt9v022->rect.width;
	pix->height		= mt9v022->rect.height;
	pix->pixelformat	= mt9v022->fourcc;
	pix->field		= V4L2_FIELD_NONE;
	pix->colorspace		= V4L2_COLORSPACE_SRGB;

	return 0;
}

static int mt9v022_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *f)
{
	struct i2c_client *client = sd->priv;
	struct mt9v022 *mt9v022 = to_mt9v022(client);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_crop a = {
		.c = {
			.left	= mt9v022->rect.left,
			.top	= mt9v022->rect.top,
			.width	= pix->width,
			.height	= pix->height,
		},
	};
	int ret;

	/* The caller provides a supported format, as verified per call to
	 * icd->try_fmt(), datawidth is from our supported format list */
	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y16:
		if (mt9v022->model != V4L2_IDENT_MT9V022IX7ATM)
			return -EINVAL;
		break;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SBGGR16:
		if (mt9v022->model != V4L2_IDENT_MT9V022IX7ATC)
			return -EINVAL;
		break;
	case 0:
		/* No format change, only geometry */
		break;
	default:
		return -EINVAL;
	}

	/* No support for scaling on this camera, just crop. */
	ret = mt9v022_s_crop(sd, &a);
	if (!ret) {
		pix->width = mt9v022->rect.width;
		pix->height = mt9v022->rect.height;
		mt9v022->fourcc = pix->pixelformat;
	}

	return ret;
}

static int mt9v022_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *f)
{
	struct i2c_client *client = sd->priv;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int align = pix->pixelformat == V4L2_PIX_FMT_SBGGR8 ||
		pix->pixelformat == V4L2_PIX_FMT_SBGGR16;

	v4l_bound_align_image(&pix->width, MT9V022_MIN_WIDTH,
		MT9V022_MAX_WIDTH, align,
		&pix->height, MT9V022_MIN_HEIGHT + icd->y_skip_top,
		MT9V022_MAX_HEIGHT + icd->y_skip_top, align, 0);

	return 0;
}

static int mt9v022_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = sd->priv;
	struct mt9v022 *mt9v022 = to_mt9v022(client);

	if (id->match.type != V4L2_CHIP_MATCH_I2C_ADDR)
		return -EINVAL;

	if (id->match.addr != client->addr)
		return -ENODEV;

	id->ident	= mt9v022->model;
	id->revision	= 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9v022_g_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = sd->priv;

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->reg > 0xff)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	reg->size = 2;
	reg->val = reg_read(client, reg->reg);

	if (reg->val > 0xffff)
		return -EIO;

	return 0;
}

static int mt9v022_s_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = sd->priv;

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->reg > 0xff)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	if (reg_write(client, reg->reg, reg->val) < 0)
		return -EIO;

	return 0;
}
#endif

static const struct v4l2_queryctrl mt9v022_controls[] = {
	{
		.id		= V4L2_CID_VFLIP,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Flip Vertically",
		.minimum	= 0,
		.maximum	= 1,
		.step		= 1,
		.default_value	= 0,
	}, {
		.id		= V4L2_CID_HFLIP,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Flip Horizontally",
		.minimum	= 0,
		.maximum	= 1,
		.step		= 1,
		.default_value	= 0,
	}, {
		.id		= V4L2_CID_GAIN,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Analog Gain",
		.minimum	= 64,
		.maximum	= 127,
		.step		= 1,
		.default_value	= 64,
		.flags		= V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id		= V4L2_CID_EXPOSURE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Exposure",
		.minimum	= 1,
		.maximum	= 255,
		.step		= 1,
		.default_value	= 255,
		.flags		= V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id		= V4L2_CID_AUTOGAIN,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Automatic Gain",
		.minimum	= 0,
		.maximum	= 1,
		.step		= 1,
		.default_value	= 1,
	}, {
		.id		= V4L2_CID_EXPOSURE_AUTO,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Automatic Exposure",
		.minimum	= 0,
		.maximum	= 1,
		.step		= 1,
		.default_value	= 1,
	}
};

static struct soc_camera_ops mt9v022_ops = {
	.set_bus_param		= mt9v022_set_bus_param,
	.query_bus_param	= mt9v022_query_bus_param,
	.controls		= mt9v022_controls,
	.num_controls		= ARRAY_SIZE(mt9v022_controls),
};

static int mt9v022_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = sd->priv;
	const struct v4l2_queryctrl *qctrl;
	unsigned long range;
	int data;

	qctrl = soc_camera_find_qctrl(&mt9v022_ops, ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		data = reg_read(client, MT9V022_READ_MODE);
		if (data < 0)
			return -EIO;
		ctrl->value = !!(data & 0x10);
		break;
	case V4L2_CID_HFLIP:
		data = reg_read(client, MT9V022_READ_MODE);
		if (data < 0)
			return -EIO;
		ctrl->value = !!(data & 0x20);
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		data = reg_read(client, MT9V022_AEC_AGC_ENABLE);
		if (data < 0)
			return -EIO;
		ctrl->value = !!(data & 0x1);
		break;
	case V4L2_CID_AUTOGAIN:
		data = reg_read(client, MT9V022_AEC_AGC_ENABLE);
		if (data < 0)
			return -EIO;
		ctrl->value = !!(data & 0x2);
		break;
	case V4L2_CID_GAIN:
		data = reg_read(client, MT9V022_ANALOG_GAIN);
		if (data < 0)
			return -EIO;

		range = qctrl->maximum - qctrl->minimum;
		ctrl->value = ((data - 16) * range + 24) / 48 + qctrl->minimum;

		break;
	case V4L2_CID_EXPOSURE:
		data = reg_read(client, MT9V022_TOTAL_SHUTTER_WIDTH);
		if (data < 0)
			return -EIO;

		range = qctrl->maximum - qctrl->minimum;
		ctrl->value = ((data - 1) * range + 239) / 479 + qctrl->minimum;

		break;
	}
	return 0;
}

static int mt9v022_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int data;
	struct i2c_client *client = sd->priv;
	const struct v4l2_queryctrl *qctrl;

	qctrl = soc_camera_find_qctrl(&mt9v022_ops, ctrl->id);
	if (!qctrl)
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		if (ctrl->value)
			data = reg_set(client, MT9V022_READ_MODE, 0x10);
		else
			data = reg_clear(client, MT9V022_READ_MODE, 0x10);
		if (data < 0)
			return -EIO;
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->value)
			data = reg_set(client, MT9V022_READ_MODE, 0x20);
		else
			data = reg_clear(client, MT9V022_READ_MODE, 0x20);
		if (data < 0)
			return -EIO;
		break;
	case V4L2_CID_GAIN:
		/* mt9v022 has minimum == default */
		if (ctrl->value > qctrl->maximum || ctrl->value < qctrl->minimum)
			return -EINVAL;
		else {
			unsigned long range = qctrl->maximum - qctrl->minimum;
			/* Valid values 16 to 64, 32 to 64 must be even. */
			unsigned long gain = ((ctrl->value - qctrl->minimum) *
					      48 + range / 2) / range + 16;
			if (gain >= 32)
				gain &= ~1;
			/* The user wants to set gain manually, hope, she
			 * knows, what she's doing... Switch AGC off. */

			if (reg_clear(client, MT9V022_AEC_AGC_ENABLE, 0x2) < 0)
				return -EIO;

			dev_dbg(&client->dev, "Setting gain from %d to %lu\n",
				reg_read(client, MT9V022_ANALOG_GAIN), gain);
			if (reg_write(client, MT9V022_ANALOG_GAIN, gain) < 0)
				return -EIO;
		}
		break;
	case V4L2_CID_EXPOSURE:
		/* mt9v022 has maximum == default */
		if (ctrl->value > qctrl->maximum || ctrl->value < qctrl->minimum)
			return -EINVAL;
		else {
			unsigned long range = qctrl->maximum - qctrl->minimum;
			unsigned long shutter = ((ctrl->value - qctrl->minimum) *
						 479 + range / 2) / range + 1;
			/* The user wants to set shutter width manually, hope,
			 * she knows, what she's doing... Switch AEC off. */

			if (reg_clear(client, MT9V022_AEC_AGC_ENABLE, 0x1) < 0)
				return -EIO;

			dev_dbg(&client->dev, "Shutter width from %d to %lu\n",
				reg_read(client, MT9V022_TOTAL_SHUTTER_WIDTH),
				shutter);
			if (reg_write(client, MT9V022_TOTAL_SHUTTER_WIDTH,
				      shutter) < 0)
				return -EIO;
		}
		break;
	case V4L2_CID_AUTOGAIN:
		if (ctrl->value)
			data = reg_set(client, MT9V022_AEC_AGC_ENABLE, 0x2);
		else
			data = reg_clear(client, MT9V022_AEC_AGC_ENABLE, 0x2);
		if (data < 0)
			return -EIO;
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		if (ctrl->value)
			data = reg_set(client, MT9V022_AEC_AGC_ENABLE, 0x1);
		else
			data = reg_clear(client, MT9V022_AEC_AGC_ENABLE, 0x1);
		if (data < 0)
			return -EIO;
		break;
	}
	return 0;
}

/* Interface active, can use i2c. If it fails, it can indeed mean, that
 * this wasn't our capture interface, so, we wait for the right one */
static int mt9v022_video_probe(struct soc_camera_device *icd,
			       struct i2c_client *client)
{
	struct mt9v022 *mt9v022 = to_mt9v022(client);
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	s32 data;
	int ret;
	unsigned long flags;

	if (!icd->dev.parent ||
	    to_soc_camera_host(icd->dev.parent)->nr != icd->iface)
		return -ENODEV;

	/* Read out the chip version register */
	data = reg_read(client, MT9V022_CHIP_VERSION);

	/* must be 0x1311 or 0x1313 */
	if (data != 0x1311 && data != 0x1313) {
		ret = -ENODEV;
		dev_info(&client->dev, "No MT9V022 found, ID register 0x%x\n",
			 data);
		goto ei2c;
	}

	/* Soft reset */
	ret = reg_write(client, MT9V022_RESET, 1);
	if (ret < 0)
		goto ei2c;
	/* 15 clock cycles */
	udelay(200);
	if (reg_read(client, MT9V022_RESET)) {
		dev_err(&client->dev, "Resetting MT9V022 failed!\n");
		if (ret > 0)
			ret = -EIO;
		goto ei2c;
	}

	/* Set monochrome or colour sensor type */
	if (sensor_type && (!strcmp("colour", sensor_type) ||
			    !strcmp("color", sensor_type))) {
		ret = reg_write(client, MT9V022_PIXEL_OPERATION_MODE, 4 | 0x11);
		mt9v022->model = V4L2_IDENT_MT9V022IX7ATC;
		icd->formats = mt9v022_colour_formats;
	} else {
		ret = reg_write(client, MT9V022_PIXEL_OPERATION_MODE, 0x11);
		mt9v022->model = V4L2_IDENT_MT9V022IX7ATM;
		icd->formats = mt9v022_monochrome_formats;
	}

	if (ret < 0)
		goto ei2c;

	icd->num_formats = 0;

	/*
	 * This is a 10bit sensor, so by default we only allow 10bit.
	 * The platform may support different bus widths due to
	 * different routing of the data lines.
	 */
	if (icl->query_bus_param)
		flags = icl->query_bus_param(icl);
	else
		flags = SOCAM_DATAWIDTH_10;

	if (flags & SOCAM_DATAWIDTH_10)
		icd->num_formats++;
	else
		icd->formats++;

	if (flags & SOCAM_DATAWIDTH_8)
		icd->num_formats++;

	mt9v022->fourcc = icd->formats->fourcc;

	dev_info(&client->dev, "Detected a MT9V022 chip ID %x, %s sensor\n",
		 data, mt9v022->model == V4L2_IDENT_MT9V022IX7ATM ?
		 "monochrome" : "colour");

	ret = mt9v022_init(client);
	if (ret < 0)
		dev_err(&client->dev, "Failed to initialise the camera\n");

ei2c:
	return ret;
}

static void mt9v022_video_remove(struct soc_camera_device *icd)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);

	dev_dbg(&icd->dev, "Video removed: %p, %p\n",
		icd->dev.parent, icd->vdev);
	if (icl->free_bus)
		icl->free_bus(icl);
}

static struct v4l2_subdev_core_ops mt9v022_subdev_core_ops = {
	.g_ctrl		= mt9v022_g_ctrl,
	.s_ctrl		= mt9v022_s_ctrl,
	.g_chip_ident	= mt9v022_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9v022_g_register,
	.s_register	= mt9v022_s_register,
#endif
};

static struct v4l2_subdev_video_ops mt9v022_subdev_video_ops = {
	.s_stream	= mt9v022_s_stream,
	.s_fmt		= mt9v022_s_fmt,
	.g_fmt		= mt9v022_g_fmt,
	.try_fmt	= mt9v022_try_fmt,
	.s_crop		= mt9v022_s_crop,
	.g_crop		= mt9v022_g_crop,
	.cropcap	= mt9v022_cropcap,
};

static struct v4l2_subdev_ops mt9v022_subdev_ops = {
	.core	= &mt9v022_subdev_core_ops,
	.video	= &mt9v022_subdev_video_ops,
};

static int mt9v022_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	struct mt9v022 *mt9v022;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct soc_camera_link *icl;
	int ret;

	if (!icd) {
		dev_err(&client->dev, "MT9V022: missing soc-camera data!\n");
		return -EINVAL;
	}

	icl = to_soc_camera_link(icd);
	if (!icl) {
		dev_err(&client->dev, "MT9V022 driver needs platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	mt9v022 = kzalloc(sizeof(struct mt9v022), GFP_KERNEL);
	if (!mt9v022)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&mt9v022->subdev, client, &mt9v022_subdev_ops);

	mt9v022->chip_control = MT9V022_CHIP_CONTROL_DEFAULT;

	icd->ops		= &mt9v022_ops;
	/*
	 * MT9V022 _really_ corrupts the first read out line.
	 * TODO: verify on i.MX31
	 */
	icd->y_skip_top		= 1;

	mt9v022->rect.left	= MT9V022_COLUMN_SKIP;
	mt9v022->rect.top	= MT9V022_ROW_SKIP;
	mt9v022->rect.width	= MT9V022_MAX_WIDTH;
	mt9v022->rect.height	= MT9V022_MAX_HEIGHT;

	ret = mt9v022_video_probe(icd, client);
	if (ret) {
		icd->ops = NULL;
		i2c_set_clientdata(client, NULL);
		kfree(mt9v022);
	}

	return ret;
}

static int mt9v022_remove(struct i2c_client *client)
{
	struct mt9v022 *mt9v022 = to_mt9v022(client);
	struct soc_camera_device *icd = client->dev.platform_data;

	icd->ops = NULL;
	mt9v022_video_remove(icd);
	i2c_set_clientdata(client, NULL);
	client->driver = NULL;
	kfree(mt9v022);

	return 0;
}
static const struct i2c_device_id mt9v022_id[] = {
	{ "mt9v022", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9v022_id);

static struct i2c_driver mt9v022_i2c_driver = {
	.driver = {
		.name = "mt9v022",
	},
	.probe		= mt9v022_probe,
	.remove		= mt9v022_remove,
	.id_table	= mt9v022_id,
};

static int __init mt9v022_mod_init(void)
{
	return i2c_add_driver(&mt9v022_i2c_driver);
}

static void __exit mt9v022_mod_exit(void)
{
	i2c_del_driver(&mt9v022_i2c_driver);
}

module_init(mt9v022_mod_init);
module_exit(mt9v022_mod_exit);

MODULE_DESCRIPTION("Micron MT9V022 Camera driver");
MODULE_AUTHOR("Guennadi Liakhovetski <kernel@pengutronix.de>");
MODULE_LICENSE("GPL");
