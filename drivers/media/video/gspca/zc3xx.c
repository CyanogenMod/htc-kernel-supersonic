/*
 *	Z-Star/Vimicro zc301/zc302p/vc30x library
 *	Copyright (C) 2004 2005 2006 Michel Xhaard
 *		mxhaard@magic.fr
 *
 * V4L2 by Jean-Francois Moine <http://moinejf.free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define MODULE_NAME "zc3xx"

#include "gspca.h"
#include "jpeg.h"

MODULE_AUTHOR("Michel Xhaard <mxhaard@users.sourceforge.net>, "
		"Serge A. Suchkov <Serge.A.S@tochka.ru>");
MODULE_DESCRIPTION("GSPCA ZC03xx/VC3xx USB Camera Driver");
MODULE_LICENSE("GPL");

static int force_sensor = -1;

#define QUANT_VAL 1		/* quantization table */
#include "zc3xx-reg.h"

/* specific webcam descriptor */
struct sd {
	struct gspca_dev gspca_dev;	/* !! must be the first item */

	__u8 brightness;
	__u8 contrast;
	__u8 gamma;
	__u8 autogain;
	__u8 lightfreq;
	__u8 sharpness;
	u8 quality;			/* image quality */
#define QUALITY_MIN 40
#define QUALITY_MAX 60
#define QUALITY_DEF 50

	signed char sensor;		/* Type of image sensor chip */
/* !! values used in different tables */
#define SENSOR_ADCM2700 0
#define SENSOR_CS2102 1
#define SENSOR_CS2102K 2
#define SENSOR_GC0305 3
#define SENSOR_HDCS2020b 4
#define SENSOR_HV7131B 5
#define SENSOR_HV7131C 6
#define SENSOR_ICM105A 7
#define SENSOR_MC501CB 8
#define SENSOR_OV7620 9
/*#define SENSOR_OV7648 9 - same values */
#define SENSOR_OV7630C 10
#define SENSOR_PAS106 11
#define SENSOR_PAS202B 12
#define SENSOR_PB0330 13
#define SENSOR_PO2030 14
#define SENSOR_TAS5130CK 15
#define SENSOR_TAS5130CXX 16
#define SENSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;

	u8 *jpeg_hdr;
};

/* V4L2 controls supported by the driver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setgamma(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getgamma(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setfreq(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getfreq(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setsharpness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getsharpness(struct gspca_dev *gspca_dev, __s32 *val);

static struct ctrl sd_ctrls[] = {
#define BRIGHTNESS_IDX 0
#define SD_BRIGHTNESS 0
	{
	    {
		.id      = V4L2_CID_BRIGHTNESS,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.default_value = 128,
	    },
	    .set = sd_setbrightness,
	    .get = sd_getbrightness,
	},
#define SD_CONTRAST 1
	{
	    {
		.id      = V4L2_CID_CONTRAST,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Contrast",
		.minimum = 0,
		.maximum = 256,
		.step    = 1,
		.default_value = 128,
	    },
	    .set = sd_setcontrast,
	    .get = sd_getcontrast,
	},
#define SD_GAMMA 2
	{
	    {
		.id      = V4L2_CID_GAMMA,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gamma",
		.minimum = 1,
		.maximum = 6,
		.step    = 1,
		.default_value = 4,
	    },
	    .set = sd_setgamma,
	    .get = sd_getgamma,
	},
#define SD_AUTOGAIN 3
	{
	    {
		.id      = V4L2_CID_AUTOGAIN,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Auto Gain",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setautogain,
	    .get = sd_getautogain,
	},
#define LIGHTFREQ_IDX 4
#define SD_FREQ 4
	{
	    {
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
		.type    = V4L2_CTRL_TYPE_MENU,
		.name    = "Light frequency filter",
		.minimum = 0,
		.maximum = 2,	/* 0: 0, 1: 50Hz, 2:60Hz */
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setfreq,
	    .get = sd_getfreq,
	},
#define SD_SHARPNESS 5
	{
	    {
		.id	 = V4L2_CID_SHARPNESS,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Sharpness",
		.minimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_setsharpness,
	    .get = sd_getsharpness,
	},
};

static const struct v4l2_pix_format vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

static const struct v4l2_pix_format sif_mode[] = {
	{176, 144, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

/* usb exchanges */
struct usb_action {
	__u8	req;
	__u8	val;
	__u16	idx;
};

static const struct usb_action adcm2700_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc */
	{0xa0, 0x04, ZC3XX_R002_CLOCKSELECT},		/* 00,02,04,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d8,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc */
	{0xa0, 0xde, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,de,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,86,cc */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc */
	{0xa0, 0x58, ZC3XX_R116_RGAIN},			/* 01,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,02,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xbb, 0x00, 0x0408},				/* 04,00,08,bb */
	{0xdd, 0x00, 0x0200},				/* 00,02,00,dd */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xbb, 0xe0, 0x0c2e},				/* 0c,e0,2e,bb */
	{0xbb, 0x01, 0x2000},				/* 20,01,00,bb */
	{0xbb, 0x96, 0x2400},				/* 24,96,00,bb */
	{0xbb, 0x06, 0x1006},				/* 10,06,06,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x5f, 0x2090},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */
	{0xbb, 0x09, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0002},				/* 00,86,02,bb */
	{0xbb, 0xe6, 0x0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,86,02,bb */
	{0xbb, 0xe6, 0x0c01},				/* 0c,e6,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0020},				/* 00,fe,20,aa */
/*mswin+*/
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xfe, 0x0002},
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xb4, 0xcd37},
	{0xaa, 0xa4, 0x0004},
	{0xaa, 0xa8, 0x0007},
	{0xaa, 0xac, 0x0004},
/*mswin-*/
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x04, 0x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0x0100},				/* 00,01,00,dd */
	{0xbb, 0x01, 0x0400},				/* 04,01,00,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x41, 0x2803},				/* 28,41,03,bb */
	{0xbb, 0x40, 0x2c03},				/* 2c,40,03,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{}
};
static const struct usb_action adcm2700_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},		/* 00,02,10,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc */
	{0xa0, 0xd8, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,d8,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,88,cc */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc */
	{0xa0, 0x58, ZC3XX_R116_RGAIN},			/* 01,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,02,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xbb, 0x00, 0x0408},				/* 04,00,08,bb */
	{0xdd, 0x00, 0x0200},				/* 00,02,00,dd */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0050},				/* 00,00,50,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xbb, 0xe0, 0x0c2e},				/* 0c,e0,2e,bb */
	{0xbb, 0x01, 0x2000},				/* 20,01,00,bb */
	{0xbb, 0x96, 0x2400},				/* 24,96,00,bb */
	{0xbb, 0x06, 0x1006},				/* 10,06,06,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x5f, 0x2090},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */
	{0xbb, 0x09, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0002},				/* 00,88,02,bb */
	{0xbb, 0xe6, 0x0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,88,02,bb */
	{0xbb, 0xe6, 0x0c01},				/* 0c,e6,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0020},				/* 00,fe,20,aa */
	/*******/
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x04, 0x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0x0100},				/* 00,01,00,dd */
	{0xbb, 0x01, 0x0400},				/* 04,01,00,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x41, 0x2803},				/* 28,41,03,bb */
	{0xbb, 0x40, 0x2c03},				/* 2c,40,03,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{}
};
static const struct usb_action adcm2700_50HZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x05, 0x8400},				/* 84,05,00,bb */
	{0xbb, 0xd0, 0xb007},				/* b0,d0,07,bb */
	{0xbb, 0xa0, 0xb80f},				/* b8,a0,0f,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{0xaa, 0x26, 0x00d0},				/* 00,26,d0,aa */
	{0xaa, 0x28, 0x0002},				/* 00,28,02,aa */
	{}
};
static const struct usb_action adcm2700_60HZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x07, 0x8400},				/* 84,07,00,bb */
	{0xbb, 0x82, 0xb006},				/* b0,82,06,bb */
	{0xbb, 0x04, 0xb80d},				/* b8,04,0d,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{0xaa, 0x26, 0x0057},				/* 00,26,57,aa */
	{0xaa, 0x28, 0x0002},				/* 00,28,02,aa */
	{}
};
static const struct usb_action adcm2700_NoFliker[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x07, 0x8400},				/* 84,07,00,bb */
	{0xbb, 0x05, 0xb000},				/* b0,05,00,bb */
	{0xbb, 0xa0, 0xb801},				/* b8,a0,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{}
};
static const struct usb_action cs2102_Initial[] = {
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x20, ZC3XX_R080_HBLANKHIGH},
	{0xa0, 0x21, ZC3XX_R081_HBLANKLOW},
	{0xa0, 0x30, ZC3XX_R083_RGAINADDR},
	{0xa0, 0x31, ZC3XX_R084_GGAINADDR},
	{0xa0, 0x32, ZC3XX_R085_BGAINADDR},
	{0xa0, 0x23, ZC3XX_R086_EXPTIMEHIGH},
	{0xa0, 0x24, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x25, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0xb3, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x02, 0x0008},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x12, 0x0089},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x00e9},
	{0xaa, 0x20, 0x0000},
	{0xaa, 0x22, 0x0000},
	{0xaa, 0x0b, 0x0004},
	{0xaa, 0x30, 0x0030},
	{0xaa, 0x31, 0x0030},
	{0xaa, 0x32, 0x0030},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x10, 0x01ae},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, 0x01ad},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x24, ZC3XX_R120_GAMMA00},	/* gamma 5 */
	{0xa0, 0x44, ZC3XX_R121_GAMMA01},
	{0xa0, 0x64, ZC3XX_R122_GAMMA02},
	{0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0, 0x9d, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0, ZC3XX_R128_GAMMA08},
	{0xa0, 0xeb, ZC3XX_R129_GAMMA09},
	{0xa0, 0xf4, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfb, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xff, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x18, ZC3XX_R130_GAMMA10},
	{0xa0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0001},
	{0xaa, 0x24, 0x0055},
	{0xaa, 0x25, 0x00cc},
	{0xaa, 0x21, 0x003f},
	{0xa0, 0x02, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0xab, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x98, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x30, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xd4, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x39, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x70, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xb0, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};

static const struct usb_action cs2102_InitialScale[] = {
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x20, ZC3XX_R080_HBLANKHIGH},
	{0xa0, 0x21, ZC3XX_R081_HBLANKLOW},
	{0xa0, 0x30, ZC3XX_R083_RGAINADDR},
	{0xa0, 0x31, ZC3XX_R084_GGAINADDR},
	{0xa0, 0x32, ZC3XX_R085_BGAINADDR},
	{0xa0, 0x23, ZC3XX_R086_EXPTIMEHIGH},
	{0xa0, 0x24, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x25, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0xb3, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x02, 0x0008},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x11, 0x0001},
	{0xaa, 0x12, 0x0087},
	{0xaa, 0x13, 0x0001},
	{0xaa, 0x14, 0x00e7},
	{0xaa, 0x20, 0x0000},
	{0xaa, 0x22, 0x0000},
	{0xaa, 0x0b, 0x0004},
	{0xaa, 0x30, 0x0030},
	{0xaa, 0x31, 0x0030},
	{0xaa, 0x32, 0x0030},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, 0x01ad},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x24, ZC3XX_R120_GAMMA00},	/* gamma 5 */
	{0xa0, 0x44, ZC3XX_R121_GAMMA01},
	{0xa0, 0x64, ZC3XX_R122_GAMMA02},
	{0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0, 0x9d, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0, ZC3XX_R128_GAMMA08},
	{0xa0, 0xeb, ZC3XX_R129_GAMMA09},
	{0xa0, 0xf4, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfb, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xff, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x18, ZC3XX_R130_GAMMA10},
	{0xa0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0000},
	{0xaa, 0x24, 0x00aa},
	{0xaa, 0x25, 0x00e6},
	{0xaa, 0x21, 0x003f},
	{0xa0, 0x01, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x55, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xcc, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x18, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x6a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x3f, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xa5, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf0, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action cs2102_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x008c}, /* 00,0f,8c,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00ac}, /* 00,04,ac,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00ac}, /* 00,11,ac,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00ac}, /* 00,1d,ac,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x42, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,42,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0x8c, ZC3XX_R01D_HSYNC_0}, /* 00,1d,8c,cc */
	{0xa0, 0xb0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,b0,cc */
	{0xa0, 0xd0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,d0,cc */
	{}
};
static const struct usb_action cs2102_50HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0093}, /* 00,0f,93,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00a1}, /* 00,04,a1,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00a1}, /* 00,11,a1,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00a1}, /* 00,1d,a1,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf7, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f7,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x83, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,83,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0x93, ZC3XX_R01D_HSYNC_0}, /* 00,1d,93,cc */
	{0xa0, 0xb0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,b0,cc */
	{0xa0, 0xd0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,d0,cc */
	{}
};
static const struct usb_action cs2102_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x005d}, /* 00,0f,5d,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00aa}, /* 00,04,aa,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00aa}, /* 00,11,aa,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00aa}, /* 00,1d,aa,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xe4, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,e4,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,3a,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0x5d, ZC3XX_R01D_HSYNC_0}, /* 00,1d,5d,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xd0, 0x00c8}, /* 00,c8,d0,cc */
	{}
};
static const struct usb_action cs2102_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x00b7}, /* 00,0f,b7,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00be}, /* 00,04,be,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00be}, /* 00,11,be,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00be}, /* 00,1d,be,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xfc, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,fc,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x69, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,69,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0xb7, ZC3XX_R01D_HSYNC_0}, /* 00,1d,b7,cc */
	{0xa0, 0xd0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d0,cc */
	{0xa0, 0xe8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e8,cc */
	{}
};
static const struct usb_action cs2102_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0059}, /* 00,0f,59,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x0080}, /* 00,04,80,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x0080}, /* 00,11,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x0080}, /* 00,1d,80,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_R01D_HSYNC_0}, /* 00,1d,59,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,c8,cc */
	{}
};
static const struct usb_action cs2102_NoFlikerScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0059}, /* 00,0f,59,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x0080}, /* 00,04,80,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x0080}, /* 00,11,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x0080}, /* 00,1d,80,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_R01D_HSYNC_0}, /* 00,1d,59,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,c8,cc */
	{}
};

/* CS2102_KOCOM */
static const struct usb_action cs2102K_Initial[] = {
	{0xa0, 0x11, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x55, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0a, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0c, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0d, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xa3, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xfb, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x05, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x03, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0f, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x10, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x11, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x12, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x15, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x16, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x17, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xee, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x19, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x1f, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{}
};

static const struct usb_action cs2102K_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
/*fixme: next sequence = i2c exchanges*/
	{0xa0, 0x55, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0a, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0c, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7b, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0d, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xa3, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xfb, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x05, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x03, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0f, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x10, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x11, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x12, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x15, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x16, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x17, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0xf7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xee, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x19, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x1f, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
/*fixme:what does the next sequence?*/
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xd0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xd0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x02, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0a, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0a, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x44, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x44, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7e, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7e, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{}
};

static const struct usb_action gc0305_Initial[] = {	/* 640x480 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},	/* 00,00,01,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00,08,03,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xa0, 0x04, ZC3XX_R002_CLOCKSELECT},	/* 00,02,04,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},	/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},	/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},	/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},	/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},	/* 01,1c,00,cc */
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},	/* 00,9c,e6,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},	/* 00,9e,86,cc */
	{0xa0, 0x98, ZC3XX_R08B_I2CDEVICEADDR},	/* 00,8b,98,cc */
	{0xaa, 0x13, 0x0002},	/* 00,13,02,aa */
	{0xaa, 0x15, 0x0003},	/* 00,15,03,aa */
	{0xaa, 0x01, 0x0000},	/* 00,01,00,aa */
	{0xaa, 0x02, 0x0000},	/* 00,02,00,aa */
	{0xaa, 0x1a, 0x0000},	/* 00,1a,00,aa */
	{0xaa, 0x1c, 0x0017},	/* 00,1c,17,aa */
	{0xaa, 0x1d, 0x0080},	/* 00,1d,80,aa */
	{0xaa, 0x1f, 0x0008},	/* 00,1f,08,aa */
	{0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/* 00,86,82,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0xaa, 0x0a, 0x0000},	/* 00,0a,00,aa */
	{0xaa, 0x0b, 0x00b0},	/* 00,0b,b0,aa */
	{0xaa, 0x0c, 0x0000},	/* 00,0c,00,aa */
	{0xaa, 0x0d, 0x00b0},	/* 00,0d,b0,aa */
	{0xaa, 0x0e, 0x0000},	/* 00,0e,00,aa */
	{0xaa, 0x0f, 0x00b0},	/* 00,0f,b0,aa */
	{0xaa, 0x10, 0x0000},	/* 00,10,00,aa */
	{0xaa, 0x11, 0x00b0},	/* 00,11,b0,aa */
	{0xaa, 0x16, 0x0001},	/* 00,16,01,aa */
	{0xaa, 0x17, 0x00e6},	/* 00,17,e6,aa */
	{0xaa, 0x18, 0x0002},	/* 00,18,02,aa */
	{0xaa, 0x19, 0x0086},	/* 00,19,86,aa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,aa */
	{0xaa, 0x1b, 0x0020},	/* 00,1b,20,aa */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b7,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},	/* 01,89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,09,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},	/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},	/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},	/* 03,01,08,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,60,cc */
	{0xa0, 0x85, ZC3XX_R18D_YTARGET},	/* 01,8d,85,cc */
	{0xa0, 0x00, 0x011e},	/* 01,1e,00,cc */
	{0xa0, 0x52, ZC3XX_R116_RGAIN},	/* 01,16,52,cc */
	{0xa0, 0x40, ZC3XX_R117_GGAIN},	/* 01,17,40,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},	/* 01,18,52,cc */
	{0xa0, 0x03, ZC3XX_R113_RGB03},	/* 01,13,03,cc */
	{}
};
static const struct usb_action gc0305_InitialScale[] = { /* 320x240 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},	/* 00,00,01,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00,08,03,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},	/* 00,02,10,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},	/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},	/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},	/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},	/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},	/* 01,1c,00,cc */
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},	/* 00,9c,e8,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},	/* 00,9e,88,cc */
	{0xa0, 0x98, ZC3XX_R08B_I2CDEVICEADDR},	/* 00,8b,98,cc */
	{0xaa, 0x13, 0x0000},	/* 00,13,00,aa */
	{0xaa, 0x15, 0x0001},	/* 00,15,01,aa */
	{0xaa, 0x01, 0x0000},	/* 00,01,00,aa */
	{0xaa, 0x02, 0x0000},	/* 00,02,00,aa */
	{0xaa, 0x1a, 0x0000},	/* 00,1a,00,aa */
	{0xaa, 0x1c, 0x0017},	/* 00,1c,17,aa */
	{0xaa, 0x1d, 0x0080},	/* 00,1d,80,aa */
	{0xaa, 0x1f, 0x0008},	/* 00,1f,08,aa */
	{0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/* 00,86,82,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0xaa, 0x0a, 0x0000},	/* 00,0a,00,aa */
	{0xaa, 0x0b, 0x00b0},	/* 00,0b,b0,aa */
	{0xaa, 0x0c, 0x0000},	/* 00,0c,00,aa */
	{0xaa, 0x0d, 0x00b0},	/* 00,0d,b0,aa */
	{0xaa, 0x0e, 0x0000},	/* 00,0e,00,aa */
	{0xaa, 0x0f, 0x00b0},	/* 00,0f,b0,aa */
	{0xaa, 0x10, 0x0000},	/* 00,10,00,aa */
	{0xaa, 0x11, 0x00b0},	/* 00,11,b0,aa */
	{0xaa, 0x16, 0x0001},	/* 00,16,01,aa */
	{0xaa, 0x17, 0x00e8},	/* 00,17,e8,aa */
	{0xaa, 0x18, 0x0002},	/* 00,18,02,aa */
	{0xaa, 0x19, 0x0088},	/* 00,19,88,aa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,aa */
	{0xaa, 0x1b, 0x0020},	/* 00,1b,20,aa */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b7,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},	/* 01,89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,09,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},	/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},	/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},	/* 03,01,08,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,60,cc */
	{0xa0, 0x00, 0x011e},	/* 01,1e,00,cc */
	{0xa0, 0x52, ZC3XX_R116_RGAIN},	/* 01,16,52,cc */
	{0xa0, 0x40, ZC3XX_R117_GGAIN},	/* 01,17,40,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},	/* 01,18,52,cc */
	{0xa0, 0x03, ZC3XX_R113_RGB03},	/* 01,13,03,cc */
	{}
};
static const struct usb_action gc0305_50HZ[] = {
	{0xaa, 0x82, 0x0000},	/* 00,82,00,aa */
	{0xaa, 0x83, 0x0002},	/* 00,83,02,aa */
	{0xaa, 0x84, 0x0038},	/* 00,84,38,aa */	/* win: 00,84,ec */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0b,cc */
	{0xa0, 0x18, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,18,cc */
							/* win: 01,92,10 */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x8e, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,8e,cc */
							/* win: 01,97,ec */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0e,cc */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,15,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},	/* 00,20,ff,cc */
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},	/* 01,1d,60,cc */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc */
/*	{0xa0, 0x85, ZC3XX_R18D_YTARGET},	 * 01,8d,85,cc *
						 * if 640x480 */
	{}
};
static const struct usb_action gc0305_60HZ[] = {
	{0xaa, 0x82, 0x0000},	/* 00,82,00,aa */
	{0xaa, 0x83, 0x0000},	/* 00,83,00,aa */
	{0xaa, 0x84, 0x00ec},	/* 00,84,ec,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0b,cc */
	{0xa0, 0x10, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,10,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0xec, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,ec,cc */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0e,cc */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,15,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},	/* 00,20,ff,cc */
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},	/* 01,1d,60,cc */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc */
	{0xa0, 0x80, ZC3XX_R18D_YTARGET},	/* 01,8d,80,cc */
	{}
};

static const struct usb_action gc0305_NoFliker[] = {
	{0xa0, 0x0c, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0c,cc */
	{0xaa, 0x82, 0x0000},	/* 00,82,00,aa */
	{0xaa, 0x83, 0x0000},	/* 00,83,00,aa */
	{0xaa, 0x84, 0x0020},	/* 00,84,20,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x00, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,00,cc */
	{0xa0, 0x48, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,48,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,10,cc */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0e,cc */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,15,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},	/* 00,20,ff,cc */
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},	/* 01,1d,60,cc */
	{0xa0, 0x03, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,03,cc */
	{0xa0, 0x80, ZC3XX_R18D_YTARGET},	/* 01,8d,80,cc */
	{}
};

static const struct usb_action hdcs2020xb_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x11, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* qtable 0x05 */
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xaa, 0x1c, 0x0000},
	{0xaa, 0x0a, 0x0001},
	{0xaa, 0x0b, 0x0006},
	{0xaa, 0x0c, 0x007b},
	{0xaa, 0x0d, 0x00a7},
	{0xaa, 0x03, 0x00fb},
	{0xaa, 0x05, 0x0000},
	{0xaa, 0x06, 0x0003},
	{0xaa, 0x09, 0x0008},

	{0xaa, 0x0f, 0x0018},	/* set sensor gain */
	{0xaa, 0x10, 0x0018},
	{0xaa, 0x11, 0x0018},
	{0xaa, 0x12, 0x0018},

	{0xaa, 0x15, 0x004e},
	{0xaa, 0x1c, 0x0004},
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},

	{0xa0, 0x66, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xed, ZC3XX_R10B_RGB01},
	{0xa0, 0xed, ZC3XX_R10C_RGB02},
	{0xa0, 0xed, ZC3XX_R10D_RGB10},
	{0xa0, 0x66, ZC3XX_R10E_RGB11},
	{0xa0, 0xed, ZC3XX_R10F_RGB12},
	{0xa0, 0xed, ZC3XX_R110_RGB20},
	{0xa0, 0xed, ZC3XX_R111_RGB21},
	{0xa0, 0x66, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x13, 0x0031},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x0e, 0x0004},
	{0xaa, 0x19, 0x00cd},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x62, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3d, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},

	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 0x14 */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x18, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x2c, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x41, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action hdcs2020xb_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xaa, 0x1c, 0x0000},
	{0xaa, 0x0a, 0x0001},
	{0xaa, 0x0b, 0x0006},
	{0xaa, 0x0c, 0x007a},
	{0xaa, 0x0d, 0x00a7},
	{0xaa, 0x03, 0x00fb},
	{0xaa, 0x05, 0x0000},
	{0xaa, 0x06, 0x0003},
	{0xaa, 0x09, 0x0008},
	{0xaa, 0x0f, 0x0018},	/* original setting */
	{0xaa, 0x10, 0x0018},
	{0xaa, 0x11, 0x0018},
	{0xaa, 0x12, 0x0018},
	{0xaa, 0x15, 0x004e},
	{0xaa, 0x1c, 0x0004},
	{0xa0, 0xf7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x66, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xed, ZC3XX_R10B_RGB01},
	{0xa0, 0xed, ZC3XX_R10C_RGB02},
	{0xa0, 0xed, ZC3XX_R10D_RGB10},
	{0xa0, 0x66, ZC3XX_R10E_RGB11},
	{0xa0, 0xed, ZC3XX_R10F_RGB12},
	{0xa0, 0xed, ZC3XX_R110_RGB20},
	{0xa0, 0xed, ZC3XX_R111_RGB21},
	{0xa0, 0x66, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
 /**** set exposure ***/
	{0xaa, 0x13, 0x0031},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x0e, 0x0004},
	{0xaa, 0x19, 0x00cd},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x62, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3d, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x18, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x2c, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x41, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action hdcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x13, 0x0018},			/* 00,13,18,aa */
	{0xaa, 0x14, 0x0001},			/* 00,14,01,aa */
	{0xaa, 0x0e, 0x0005},			/* 00,0e,05,aa */
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x76, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,76,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x46, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,46,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,28,cc */
	{0xa0, 0x05, ZC3XX_R01D_HSYNC_0}, /* 00,1d,05,cc */
	{0xa0, 0x1a, ZC3XX_R01E_HSYNC_1}, /* 00,1e,1a,cc */
	{0xa0, 0x2f, ZC3XX_R01F_HSYNC_2}, /* 00,1f,2f,cc */
	{}
};
static const struct usb_action hdcs2020b_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x13, 0x0031},			/* 00,13,31,aa */
	{0xaa, 0x14, 0x0001},			/* 00,14,01,aa */
	{0xaa, 0x0e, 0x0004},			/* 00,0e,04,aa */
	{0xaa, 0x19, 0x00cd},			/* 00,19,cd,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x62, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,62,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x3d, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,3d,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,28,cc */
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0}, /* 00,1d,04,cc */
	{0xa0, 0x18, ZC3XX_R01E_HSYNC_1}, /* 00,1e,18,cc */
	{0xa0, 0x2c, ZC3XX_R01F_HSYNC_2}, /* 00,1f,2c,cc */
	{}
};
static const struct usb_action hdcs2020b_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x13, 0x0010},			/* 00,13,10,aa */
	{0xaa, 0x14, 0x0001},			/* 00,14,01,aa */
	{0xaa, 0x0e, 0x0004},			/* 00,0e,04,aa */
	{0xaa, 0x19, 0x0000},			/* 00,19,00,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x70, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,70,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0}, /* 00,1d,04,cc */
	{0xa0, 0x17, ZC3XX_R01E_HSYNC_1}, /* 00,1e,17,cc */
	{0xa0, 0x2a, ZC3XX_R01F_HSYNC_2}, /* 00,1f,2a,cc */
	{}
};

static const struct usb_action hv7131bxx_Initial[] = {		/* 320x240 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x30, 0x002d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x13, 0x0001},	/* {0xaa, 0x13, 0x0000}, */
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e8},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0086},		/* 00,17,88,aa */
	{0xaa, 0x31, 0x0038},
	{0xaa, 0x32, 0x0038},
	{0xaa, 0x33, 0x0038},
	{0xaa, 0x5b, 0x0001},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x02, ZC3XX_R188_MINGAIN},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0x02, 0x0090},			/* 00,02,80,aa */
	{}
};

static const struct usb_action hv7131bxx_InitialScale[] = {	/* 640x480*/
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x30, 0x002d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x11, 0x0001},
	{0xaa, 0x13, 0x0000},	/* {0xaa, 0x13, 0x0001}; */
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e6},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0086},
	{0xaa, 0x31, 0x0038},
	{0xaa, 0x32, 0x0038},
	{0xaa, 0x33, 0x0038},
	{0xaa, 0x5b, 0x0001},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x02, ZC3XX_R188_MINGAIN},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0x02, 0x0090},	/* {0xaa, 0x02, 0x0080}, */
	{}
};
static const struct usb_action hv7131b_50HZ[] = {	/* 640x480*/
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x0053},			/* 00,26,53,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0050},			/* 00,21,50,aa */
	{0xaa, 0x22, 0x001b},			/* 00,22,1b,aa */
	{0xaa, 0x23, 0x00fc},			/* 00,23,fc,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x9b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,9b,cc */
	{0xa0, 0x80, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,80,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0xea, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,ea,cc */
	{0xa0, 0x60, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,60,cc */
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0c,cc */
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,18,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x50, ZC3XX_R01E_HSYNC_1},	/* 00,1e,50,cc */
	{0xa0, 0x1b, ZC3XX_R01F_HSYNC_2},	/* 00,1f,1b,cc */
	{0xa0, 0xfc, ZC3XX_R020_HSYNC_3},	/* 00,20,fc,cc */
	{}
};
static const struct usb_action hv7131b_50HZScale[] = {	/* 320x240 */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x0053},			/* 00,26,53,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0050},			/* 00,21,50,aa */
	{0xaa, 0x22, 0x0012},			/* 00,22,12,aa */
	{0xaa, 0x23, 0x0080},			/* 00,23,80,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x9b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,9b,cc */
	{0xa0, 0x80, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,80,cc */
	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,01,cc */
	{0xa0, 0xd4, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,d4,cc */
	{0xa0, 0xc0, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,c0,cc */
	{0xa0, 0x07, ZC3XX_R18C_AEFREEZE},	/* 01,8c,07,cc */
	{0xa0, 0x0f, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,0f,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x50, ZC3XX_R01E_HSYNC_1},	/* 00,1e,50,cc */
	{0xa0, 0x12, ZC3XX_R01F_HSYNC_2},	/* 00,1f,12,cc */
	{0xa0, 0x80, ZC3XX_R020_HSYNC_3},	/* 00,20,80,cc */
	{}
};
static const struct usb_action hv7131b_60HZ[] = {	/* 640x480*/
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x00a1},			/* 00,26,a1,aa */
	{0xaa, 0x27, 0x0020},			/* 00,27,20,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0040},			/* 00,21,40,aa */
	{0xaa, 0x22, 0x0013},			/* 00,22,13,aa */
	{0xaa, 0x23, 0x004c},			/* 00,23,4c,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x4d, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,4d,cc */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,60,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0xc3, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,c3,cc */
	{0xa0, 0x50, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,50,cc */
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0c,cc */
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,18,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x40, ZC3XX_R01E_HSYNC_1},	/* 00,1e,40,cc */
	{0xa0, 0x13, ZC3XX_R01F_HSYNC_2},	/* 00,1f,13,cc */
	{0xa0, 0x4c, ZC3XX_R020_HSYNC_3},	/* 00,20,4c,cc */
	{}
};
static const struct usb_action hv7131b_60HZScale[] = {	/* 320x240 */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x00a1},			/* 00,26,a1,aa */
	{0xaa, 0x27, 0x0020},			/* 00,27,20,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x22, 0x0016},			/* 00,22,16,aa */
	{0xaa, 0x23, 0x0040},			/* 00,23,40,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x4d, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,4d,cc */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,60,cc */
	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,01,cc */
	{0xa0, 0x86, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,86,cc */
	{0xa0, 0xa0, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,a0,cc */
	{0xa0, 0x07, ZC3XX_R18C_AEFREEZE},	/* 01,8c,07,cc */
	{0xa0, 0x0f, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,0f,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0xa0, ZC3XX_R01E_HSYNC_1},	/* 00,1e,a0,cc */
	{0xa0, 0x16, ZC3XX_R01F_HSYNC_2},	/* 00,1f,16,cc */
	{0xa0, 0x40, ZC3XX_R020_HSYNC_3},	/* 00,20,40,cc */
	{}
};
static const struct usb_action hv7131b_NoFliker[] = {	/* 640x480*/
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0003},			/* 00,25,03,aa */
	{0xaa, 0x26, 0x0000},			/* 00,26,00,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0010},			/* 00,21,10,aa */
	{0xaa, 0x22, 0x0000},			/* 00,22,00,aa */
	{0xaa, 0x23, 0x0003},			/* 00,23,03,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0xf8, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,f8,cc */
	{0xa0, 0x00, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,00,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x02, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,02,cc */
	{0xa0, 0x00, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,00,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,00,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x10, ZC3XX_R01E_HSYNC_1},	/* 00,1e,10,cc */
	{0xa0, 0x00, ZC3XX_R01F_HSYNC_2},	/* 00,1f,00,cc */
	{0xa0, 0x03, ZC3XX_R020_HSYNC_3},	/* 00,20,03,cc */
	{}
};
static const struct usb_action hv7131b_NoFlikerScale[] = { /* 320x240 */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0003},			/* 00,25,03,aa */
	{0xaa, 0x26, 0x0000},			/* 00,26,00,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x22, 0x0016},			/* 00,22,16,aa */
	{0xaa, 0x23, 0x0040},			/* 00,23,40,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0xf8, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,f8,cc */
	{0xa0, 0x00, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,00,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x02, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,02,cc */
	{0xa0, 0x00, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,00,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,00,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0xa0, ZC3XX_R01E_HSYNC_1},	/* 00,1e,a0,cc */
	{0xa0, 0x16, ZC3XX_R01F_HSYNC_2},	/* 00,1f,16,cc */
	{0xa0, 0x40, ZC3XX_R020_HSYNC_3},	/* 00,20,40,cc */
	{}
};

static const struct usb_action hv7131cxx_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xaa, 0x01, 0x000c},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e8},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0088},

	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x89, ZC3XX_R18D_YTARGET},
	{0xa0, 0x50, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x00, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xa1, 0x01, 0x0095},
	{0xa1, 0x01, 0x0096},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x60, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf0, ZC3XX_R10B_RGB01},
	{0xa0, 0xf0, ZC3XX_R10C_RGB02},
	{0xa0, 0xf0, ZC3XX_R10D_RGB10},
	{0xa0, 0x60, ZC3XX_R10E_RGB11},
	{0xa0, 0xf0, ZC3XX_R10F_RGB12},
	{0xa0, 0xf0, ZC3XX_R110_RGB20},
	{0xa0, 0xf0, ZC3XX_R111_RGB21},
	{0xa0, 0x60, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x10, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x25, 0x0007},
	{0xaa, 0x26, 0x0053},
	{0xaa, 0x27, 0x0000},

	{0xa0, 0x10, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 2f */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID},	/* 9b */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/* 80 */
	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0xd4, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xc0, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x13, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa1, 0x01, 0x001d},
	{0xa1, 0x01, 0x001e},
	{0xa1, 0x01, 0x001f},
	{0xa1, 0x01, 0x0020},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action hv7131cxx_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},

	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},	/* diff */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},

	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},

	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 1e0 */

	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xaa, 0x01, 0x000c},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e8},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0088},

	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00 */

	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x89, ZC3XX_R18D_YTARGET},
	{0xa0, 0x50, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x00, ZC3XX_R092_I2CADDRESSSELECT},
						/* read the i2c chips ident */
	{0xa0, 0x02, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xa1, 0x01, 0x0095},
	{0xa1, 0x01, 0x0096},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x60, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf0, ZC3XX_R10B_RGB01},
	{0xa0, 0xf0, ZC3XX_R10C_RGB02},
	{0xa0, 0xf0, ZC3XX_R10D_RGB10},
	{0xa0, 0x60, ZC3XX_R10E_RGB11},
	{0xa0, 0xf0, ZC3XX_R10F_RGB12},
	{0xa0, 0xf0, ZC3XX_R110_RGB20},
	{0xa0, 0xf0, ZC3XX_R111_RGB21},
	{0xa0, 0x60, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x10, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x25, 0x0007},
	{0xaa, 0x26, 0x0053},
	{0xaa, 0x27, 0x0000},

	{0xa0, 0x10, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 2f */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID},	/* 9b */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/* 80 */

	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0xd4, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xc0, ZC3XX_R197_ANTIFLICKERLOW},

	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x13, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa1, 0x01, 0x001d},
	{0xa1, 0x01, 0x001e},
	{0xa1, 0x01, 0x001f},
	{0xa1, 0x01, 0x0020},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action icm105axx_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0c, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x00, ZC3XX_R097_WINYSTARTHIGH},
	{0xa0, 0x01, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R099_WINXSTARTHIGH},
	{0xa0, 0x01, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x01, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x01, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xaa, 0x01, 0x0010},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x04, 0x0001},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0001},
	{0xaa, 0x04, 0x0011},
	{0xaa, 0x05, 0x00a0},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0002},
	{0xaa, 0x04, 0x0013},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0003},
	{0xaa, 0x04, 0x0015},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0004},
	{0xaa, 0x04, 0x0017},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x000d},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0005},
	{0xaa, 0x04, 0x0019},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0006},
	{0xaa, 0x04, 0x0017},
	{0xaa, 0x05, 0x0026},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0007},
	{0xaa, 0x04, 0x0019},
	{0xaa, 0x05, 0x0022},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0008},
	{0xaa, 0x04, 0x0021},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0009},
	{0xaa, 0x04, 0x0023},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x000d},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000a},
	{0xaa, 0x04, 0x0025},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000b},
	{0xaa, 0x04, 0x00ec},
	{0xaa, 0x05, 0x002e},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000c},
	{0xaa, 0x04, 0x00fa},
	{0xaa, 0x05, 0x002a},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x07, 0x000d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x94, 0x0002},
	{0xaa, 0x90, 0x0000},
	{0xaa, 0x91, 0x001f},
	{0xaa, 0x10, 0x0064},
	{0xaa, 0x9b, 0x00f0},
	{0xaa, 0x9c, 0x0002},
	{0xaa, 0x14, 0x001a},
	{0xaa, 0x20, 0x0080},
	{0xaa, 0x22, 0x0080},
	{0xaa, 0x24, 0x0080},
	{0xaa, 0x26, 0x0080},
	{0xaa, 0x00, 0x0084},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0xa8, 0x00c0},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x0008},

	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x52, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf7, ZC3XX_R10B_RGB01},
	{0xa0, 0xf7, ZC3XX_R10C_RGB02},
	{0xa0, 0xf7, ZC3XX_R10D_RGB10},
	{0xa0, 0x52, ZC3XX_R10E_RGB11},
	{0xa0, 0xf7, ZC3XX_R10F_RGB12},
	{0xa0, 0xf7, ZC3XX_R110_RGB20},
	{0xa0, 0xf7, ZC3XX_R111_RGB21},
	{0xa0, 0x52, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x0d, 0x0003},
	{0xaa, 0x0c, 0x008c},
	{0xaa, 0x0e, 0x0095},
	{0xaa, 0x0f, 0x0002},
	{0xaa, 0x1c, 0x0094},
	{0xaa, 0x1d, 0x0002},
	{0xaa, 0x20, 0x0080},
	{0xaa, 0x22, 0x0080},
	{0xaa, 0x24, 0x0080},
	{0xaa, 0x26, 0x0080},
	{0xaa, 0x00, 0x0084},
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x94, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x20, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x84, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x12, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xe3, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xec, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf5, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0xc0, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0xc0, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};

static const struct usb_action icm105axx_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0c, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x00, ZC3XX_R097_WINYSTARTHIGH},
	{0xa0, 0x02, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R099_WINXSTARTHIGH},
	{0xa0, 0x02, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x02, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x02, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xaa, 0x01, 0x0010},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x04, 0x0001},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0001},
	{0xaa, 0x04, 0x0011},
	{0xaa, 0x05, 0x00a0},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0002},
	{0xaa, 0x04, 0x0013},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0003},
	{0xaa, 0x04, 0x0015},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0004},
	{0xaa, 0x04, 0x0017},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x000d},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0005},
	{0xa0, 0x04, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x19, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0006},
	{0xaa, 0x04, 0x0017},
	{0xaa, 0x05, 0x0026},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0007},
	{0xaa, 0x04, 0x0019},
	{0xaa, 0x05, 0x0022},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0008},
	{0xaa, 0x04, 0x0021},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0009},
	{0xaa, 0x04, 0x0023},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x000d},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000a},
	{0xaa, 0x04, 0x0025},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000b},
	{0xaa, 0x04, 0x00ec},
	{0xaa, 0x05, 0x002e},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000c},
	{0xaa, 0x04, 0x00fa},
	{0xaa, 0x05, 0x002a},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x07, 0x000d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x94, 0x0002},
	{0xaa, 0x90, 0x0000},
	{0xaa, 0x91, 0x0010},
	{0xaa, 0x10, 0x0064},
	{0xaa, 0x9b, 0x00f0},
	{0xaa, 0x9c, 0x0002},
	{0xaa, 0x14, 0x001a},
	{0xaa, 0x20, 0x0080},
	{0xaa, 0x22, 0x0080},
	{0xaa, 0x24, 0x0080},
	{0xaa, 0x26, 0x0080},
	{0xaa, 0x00, 0x0084},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0xa8, 0x0080},
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x0008},

	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x52, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf7, ZC3XX_R10B_RGB01},
	{0xa0, 0xf7, ZC3XX_R10C_RGB02},
	{0xa0, 0xf7, ZC3XX_R10D_RGB10},
	{0xa0, 0x52, ZC3XX_R10E_RGB11},
	{0xa0, 0xf7, ZC3XX_R10F_RGB12},
	{0xa0, 0xf7, ZC3XX_R110_RGB20},
	{0xa0, 0xf7, ZC3XX_R111_RGB21},
	{0xa0, 0x52, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x0d, 0x0003},
	{0xaa, 0x0c, 0x0020},
	{0xaa, 0x0e, 0x000e},
	{0xaa, 0x0f, 0x0002},
	{0xaa, 0x1c, 0x000d},
	{0xaa, 0x1d, 0x0002},
	{0xaa, 0x20, 0x0080},
	{0xaa, 0x22, 0x0080},
	{0xaa, 0x24, 0x0080},
	{0xaa, 0x26, 0x0080},
	{0xaa, 0x00, 0x0084},
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x0d, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x1a, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x4b, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x12, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xc8, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xd8, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xea, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action icm105a_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0d, 0x0003}, /* 00,0d,03,aa */
	{0xaa, 0x0c, 0x0020}, /* 00,0c,20,aa */
	{0xaa, 0x0e, 0x000e}, /* 00,0e,0e,aa */
	{0xaa, 0x0f, 0x0002}, /* 00,0f,02,aa */
	{0xaa, 0x1c, 0x000d}, /* 00,1c,0d,aa */
	{0xaa, 0x1d, 0x0002}, /* 00,1d,02,aa */
	{0xaa, 0x20, 0x0080}, /* 00,20,80,aa */
	{0xaa, 0x22, 0x0080}, /* 00,22,80,aa */
	{0xaa, 0x24, 0x0080}, /* 00,24,80,aa */
	{0xaa, 0x26, 0x0080}, /* 00,26,80,aa */
	{0xaa, 0x00, 0x0084}, /* 00,00,84,aa */
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,02,cc */
	{0xa0, 0x0d, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,0d,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,04,cc */
	{0xa0, 0x1a, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,1a,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x4b, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,4b,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x12, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,12,cc */
	{0xa0, 0xc8, ZC3XX_R01D_HSYNC_0}, /* 00,1d,c8,cc */
	{0xa0, 0xd8, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d8,cc */
	{0xa0, 0xea, ZC3XX_R01F_HSYNC_2}, /* 00,1f,ea,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{}
};
static const struct usb_action icm105a_50HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0d, 0x0003}, /* 00,0d,03,aa */
	{0xaa, 0x0c, 0x008c}, /* 00,0c,8c,aa */
	{0xaa, 0x0e, 0x0095}, /* 00,0e,95,aa */
	{0xaa, 0x0f, 0x0002}, /* 00,0f,02,aa */
	{0xaa, 0x1c, 0x0094}, /* 00,1c,94,aa */
	{0xaa, 0x1d, 0x0002}, /* 00,1d,02,aa */
	{0xaa, 0x20, 0x0080}, /* 00,20,80,aa */
	{0xaa, 0x22, 0x0080}, /* 00,22,80,aa */
	{0xaa, 0x24, 0x0080}, /* 00,24,80,aa */
	{0xaa, 0x26, 0x0080}, /* 00,26,80,aa */
	{0xaa, 0x00, 0x0084}, /* 00,00,84,aa */
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,02,cc */
	{0xa0, 0x94, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,94,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,04,cc */
	{0xa0, 0x20, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,20,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x84, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,84,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x12, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,12,cc */
	{0xa0, 0xe3, ZC3XX_R01D_HSYNC_0}, /* 00,1d,e3,cc */
	{0xa0, 0xec, ZC3XX_R01E_HSYNC_1}, /* 00,1e,ec,cc */
	{0xa0, 0xf5, ZC3XX_R01F_HSYNC_2}, /* 00,1f,f5,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN}, /* 01,a7,00,cc */
	{0xa0, 0xc0, ZC3XX_R1A8_DIGITALGAIN}, /* 01,a8,c0,cc */
	{}
};
static const struct usb_action icm105a_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0d, 0x0003}, /* 00,0d,03,aa */
	{0xaa, 0x0c, 0x0004}, /* 00,0c,04,aa */
	{0xaa, 0x0e, 0x000d}, /* 00,0e,0d,aa */
	{0xaa, 0x0f, 0x0002}, /* 00,0f,02,aa */
	{0xaa, 0x1c, 0x0008}, /* 00,1c,08,aa */
	{0xaa, 0x1d, 0x0002}, /* 00,1d,02,aa */
	{0xaa, 0x20, 0x0080}, /* 00,20,80,aa */
	{0xaa, 0x22, 0x0080}, /* 00,22,80,aa */
	{0xaa, 0x24, 0x0080}, /* 00,24,80,aa */
	{0xaa, 0x26, 0x0080}, /* 00,26,80,aa */
	{0xaa, 0x00, 0x0084}, /* 00,00,84,aa */
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,02,cc */
	{0xa0, 0x08, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,08,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,04,cc */
	{0xa0, 0x10, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,10,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x41, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,41,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x12, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,12,cc */
	{0xa0, 0xc1, ZC3XX_R01D_HSYNC_0}, /* 00,1d,c1,cc */
	{0xa0, 0xd4, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d4,cc */
	{0xa0, 0xe8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{}
};
static const struct usb_action icm105a_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0d, 0x0003}, /* 00,0d,03,aa */
	{0xaa, 0x0c, 0x0008}, /* 00,0c,08,aa */
	{0xaa, 0x0e, 0x0086}, /* 00,0e,86,aa */
	{0xaa, 0x0f, 0x0002}, /* 00,0f,02,aa */
	{0xaa, 0x1c, 0x0085}, /* 00,1c,85,aa */
	{0xaa, 0x1d, 0x0002}, /* 00,1d,02,aa */
	{0xaa, 0x20, 0x0080}, /* 00,20,80,aa */
	{0xaa, 0x22, 0x0080}, /* 00,22,80,aa */
	{0xaa, 0x24, 0x0080}, /* 00,24,80,aa */
	{0xaa, 0x26, 0x0080}, /* 00,26,80,aa */
	{0xaa, 0x00, 0x0084}, /* 00,00,84,aa */
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,02,cc */
	{0xa0, 0x85, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,85,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,04,cc */
	{0xa0, 0x08, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,08,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x81, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,81,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x12, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,12,cc */
	{0xa0, 0xc2, ZC3XX_R01D_HSYNC_0}, /* 00,1d,c2,cc */
	{0xa0, 0xd6, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d6,cc */
	{0xa0, 0xea, ZC3XX_R01F_HSYNC_2}, /* 00,1f,ea,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN}, /* 01,a7,00,cc */
	{0xa0, 0xc0, ZC3XX_R1A8_DIGITALGAIN}, /* 01,a8,c0,cc */
	{}
};
static const struct usb_action icm105a_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0d, 0x0003}, /* 00,0d,03,aa */
	{0xaa, 0x0c, 0x0004}, /* 00,0c,04,aa */
	{0xaa, 0x0e, 0x000d}, /* 00,0e,0d,aa */
	{0xaa, 0x0f, 0x0002}, /* 00,0f,02,aa */
	{0xaa, 0x1c, 0x0000}, /* 00,1c,00,aa */
	{0xaa, 0x1d, 0x0002}, /* 00,1d,02,aa */
	{0xaa, 0x20, 0x0080}, /* 00,20,80,aa */
	{0xaa, 0x22, 0x0080}, /* 00,22,80,aa */
	{0xaa, 0x24, 0x0080}, /* 00,24,80,aa */
	{0xaa, 0x26, 0x0080}, /* 00,26,80,aa */
	{0xaa, 0x00, 0x0084}, /* 00,00,84,aa */
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,02,cc */
	{0xa0, 0x00, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,00,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,04,cc */
	{0xa0, 0x20, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,20,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0xc1, ZC3XX_R01D_HSYNC_0}, /* 00,1d,c1,cc */
	{0xa0, 0xd4, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d4,cc */
	{0xa0, 0xe8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{}
};
static const struct usb_action icm105a_NoFlikerScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0d, 0x0003}, /* 00,0d,03,aa */
	{0xaa, 0x0c, 0x0004}, /* 00,0c,04,aa */
	{0xaa, 0x0e, 0x0081}, /* 00,0e,81,aa */
	{0xaa, 0x0f, 0x0002}, /* 00,0f,02,aa */
	{0xaa, 0x1c, 0x0080}, /* 00,1c,80,aa */
	{0xaa, 0x1d, 0x0002}, /* 00,1d,02,aa */
	{0xaa, 0x20, 0x0080}, /* 00,20,80,aa */
	{0xaa, 0x22, 0x0080}, /* 00,22,80,aa */
	{0xaa, 0x24, 0x0080}, /* 00,24,80,aa */
	{0xaa, 0x26, 0x0080}, /* 00,26,80,aa */
	{0xaa, 0x00, 0x0084}, /* 00,00,84,aa */
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,02,cc */
	{0xa0, 0x80, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,80,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,04,cc */
	{0xa0, 0x20, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,20,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0xc1, ZC3XX_R01D_HSYNC_0}, /* 00,1d,c1,cc */
	{0xa0, 0xd4, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d4,cc */
	{0xa0, 0xe8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN}, /* 01,a7,00,cc */
	{0xa0, 0xc0, ZC3XX_R1A8_DIGITALGAIN}, /* 01,a8,c0,cc */
	{}
};

static const struct usb_action MC501CB_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL}, /* 00,00,01,cc */
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT}, /* 00,02,00,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT}, /* 00,10,01,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING}, /* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING}, /* 00,08,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,05,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH}, /* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW}, /* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH}, /* 00,05,01,cc */
	{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW}, /* 00,06,d8,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW}, /* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW}, /* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW}, /* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW}, /* 01,1c,00,cc */
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH}, /* 00,9b,01,cc */
	{0xa0, 0xde, ZC3XX_R09C_WINHEIGHTLOW}, /* 00,9c,de,cc */
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH}, /* 00,9d,02,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW}, /* 00,9e,86,cc */
	{0xa0, 0x33, ZC3XX_R086_EXPTIMEHIGH}, /* 00,86,33,cc */
	{0xa0, 0x34, ZC3XX_R087_EXPTIMEMID}, /* 00,87,34,cc */
	{0xa0, 0x35, ZC3XX_R088_EXPTIMELOW}, /* 00,88,35,cc */
	{0xa0, 0xb0, ZC3XX_R08B_I2CDEVICEADDR}, /* 00,8b,b0,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,05,cc */
	{0xaa, 0x01, 0x0001}, /* 00,01,01,aa */
	{0xaa, 0x01, 0x0003}, /* 00,01,03,aa */
	{0xaa, 0x01, 0x0001}, /* 00,01,01,aa */
	{0xaa, 0x03, 0x0000}, /* 00,03,00,aa */
	{0xaa, 0x10, 0x0000}, /* 00,10,00,aa */
	{0xaa, 0x11, 0x0080}, /* 00,11,80,aa */
	{0xaa, 0x12, 0x0000}, /* 00,12,00,aa */
	{0xaa, 0x13, 0x0000}, /* 00,13,00,aa */
	{0xaa, 0x14, 0x0000}, /* 00,14,00,aa */
	{0xaa, 0x15, 0x0000}, /* 00,15,00,aa */
	{0xaa, 0x16, 0x0000}, /* 00,16,00,aa */
	{0xaa, 0x17, 0x0001}, /* 00,17,01,aa */
	{0xaa, 0x18, 0x00de}, /* 00,18,de,aa */
	{0xaa, 0x19, 0x0002}, /* 00,19,02,aa */
	{0xaa, 0x1a, 0x0086}, /* 00,1a,86,aa */
	{0xaa, 0x20, 0x00a8}, /* 00,20,a8,aa */
	{0xaa, 0x22, 0x0000}, /* 00,22,00,aa */
	{0xaa, 0x23, 0x0000}, /* 00,23,00,aa */
	{0xaa, 0x24, 0x0000}, /* 00,24,00,aa */
	{0xaa, 0x40, 0x0033}, /* 00,40,33,aa */
	{0xaa, 0x41, 0x0077}, /* 00,41,77,aa */
	{0xaa, 0x42, 0x0053}, /* 00,42,53,aa */
	{0xaa, 0x43, 0x00b0}, /* 00,43,b0,aa */
	{0xaa, 0x4b, 0x0001}, /* 00,4b,01,aa */
	{0xaa, 0x72, 0x0020}, /* 00,72,20,aa */
	{0xaa, 0x73, 0x0000}, /* 00,73,00,aa */
	{0xaa, 0x80, 0x0000}, /* 00,80,00,aa */
	{0xaa, 0x85, 0x0050}, /* 00,85,50,aa */
	{0xaa, 0x91, 0x0070}, /* 00,91,70,aa */
	{0xaa, 0x92, 0x0072}, /* 00,92,72,aa */
	{0xaa, 0x03, 0x0001}, /* 00,03,01,aa */
	{0xaa, 0x10, 0x00a0}, /* 00,10,a0,aa */
	{0xaa, 0x11, 0x0001}, /* 00,11,01,aa */
	{0xaa, 0x30, 0x0000}, /* 00,30,00,aa */
	{0xaa, 0x60, 0x0000}, /* 00,60,00,aa */
	{0xaa, 0xa0, ZC3XX_R01A_LASTFRAMESTATE}, /* 00,a0,1a,aa */
	{0xaa, 0xa1, 0x0000}, /* 00,a1,00,aa */
	{0xaa, 0xa2, 0x003f}, /* 00,a2,3f,aa */
	{0xaa, 0xa3, 0x0028}, /* 00,a3,28,aa */
	{0xaa, 0xa4, 0x0010}, /* 00,a4,10,aa */
	{0xaa, 0xa5, 0x0020}, /* 00,a5,20,aa */
	{0xaa, 0xb1, 0x0044}, /* 00,b1,44,aa */
	{0xaa, 0xd0, 0x0001}, /* 00,d0,01,aa */
	{0xaa, 0xd1, 0x0085}, /* 00,d1,85,aa */
	{0xaa, 0xd2, 0x0080}, /* 00,d2,80,aa */
	{0xaa, 0xd3, 0x0080}, /* 00,d3,80,aa */
	{0xaa, 0xd4, 0x0080}, /* 00,d4,80,aa */
	{0xaa, 0xd5, 0x0080}, /* 00,d5,80,aa */
	{0xaa, 0xc0, 0x00c3}, /* 00,c0,c3,aa */
	{0xaa, 0xc2, 0x0044}, /* 00,c2,44,aa */
	{0xaa, 0xc4, 0x0040}, /* 00,c4,40,aa */
	{0xaa, 0xc5, 0x0020}, /* 00,c5,20,aa */
	{0xaa, 0xc6, 0x0008}, /* 00,c6,08,aa */
	{0xaa, 0x03, 0x0004}, /* 00,03,04,aa */
	{0xaa, 0x10, 0x0000}, /* 00,10,00,aa */
	{0xaa, 0x40, 0x0030}, /* 00,40,30,aa */
	{0xaa, 0x41, 0x0020}, /* 00,41,20,aa */
	{0xaa, 0x42, 0x002d}, /* 00,42,2d,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x1c, 0x0050}, /* 00,1C,50,aa */
	{0xaa, 0x11, 0x0081}, /* 00,11,81,aa */
	{0xaa, 0x3b, 0x001d}, /* 00,3b,1D,aa */
	{0xaa, 0x3c, 0x004c}, /* 00,3c,4C,aa */
	{0xaa, 0x3d, 0x0018}, /* 00,3d,18,aa */
	{0xaa, 0x3e, 0x006a}, /* 00,3e,6A,aa */
	{0xaa, 0x01, 0x0000}, /* 00,01,00,aa */
	{0xaa, 0x52, 0x00ff}, /* 00,52,FF,aa */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE}, /* 01,00,0d,cc */
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION}, /* 01,01,37,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS}, /* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE}, /* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05}, /* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE}, /* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS}, /* 03,01,08,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE}, /* 01,80,02,cc */
	{0xaa, 0x03, 0x0002}, /* 00,03,02,aa */
	{0xaa, 0x51, 0x0027}, /* 00,51,27,aa */
	{0xaa, 0x52, 0x0020}, /* 00,52,20,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x50, 0x0010}, /* 00,50,10,aa */
	{0xaa, 0x51, 0x0010}, /* 00,51,10,aa */
	{0xaa, 0x54, 0x0010}, /* 00,54,10,aa */
	{0xaa, 0x55, 0x0010}, /* 00,55,10,aa */
	{0xa0, 0xf0, 0x0199}, /* 01,99,F0,cc */
	{0xa0, 0x80, 0x019a}, /* 01,9A,80,cc */

	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x001d}, /* 00,36,1D,aa */
	{0xaa, 0x37, 0x004c}, /* 00,37,4C,aa */
	{0xaa, 0x3b, 0x001d}, /* 00,3B,1D,aa */
	{}
};

static const struct usb_action MC501CB_Initial[] = {	 /* 320x240 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL}, /* 00,00,01,cc */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT}, /* 00,02,10,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT}, /* 00,10,01,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING}, /* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING}, /* 00,08,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,05,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH}, /* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW}, /* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH}, /* 00,05,01,cc */
	{0xa0, 0xd0, ZC3XX_R006_FRAMEHEIGHTLOW}, /* 00,06,d0,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW}, /* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW}, /* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW}, /* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW}, /* 01,1c,00,cc */
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH}, /* 00,9b,01,cc */
	{0xa0, 0xd8, ZC3XX_R09C_WINHEIGHTLOW}, /* 00,9c,d8,cc */
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH}, /* 00,9d,02,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW}, /* 00,9e,88,cc */
	{0xa0, 0x33, ZC3XX_R086_EXPTIMEHIGH}, /* 00,86,33,cc */
	{0xa0, 0x34, ZC3XX_R087_EXPTIMEMID}, /* 00,87,34,cc */
	{0xa0, 0x35, ZC3XX_R088_EXPTIMELOW}, /* 00,88,35,cc */
	{0xa0, 0xb0, ZC3XX_R08B_I2CDEVICEADDR}, /* 00,8b,b0,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,05,cc */
	{0xaa, 0x01, 0x0001}, /* 00,01,01,aa */
	{0xaa, 0x01, 0x0003}, /* 00,01,03,aa */
	{0xaa, 0x01, 0x0001}, /* 00,01,01,aa */
	{0xaa, 0x03, 0x0000}, /* 00,03,00,aa */
	{0xaa, 0x10, 0x0000}, /* 00,10,00,aa */
	{0xaa, 0x11, 0x0080}, /* 00,11,80,aa */
	{0xaa, 0x12, 0x0000}, /* 00,12,00,aa */
	{0xaa, 0x13, 0x0000}, /* 00,13,00,aa */
	{0xaa, 0x14, 0x0000}, /* 00,14,00,aa */
	{0xaa, 0x15, 0x0000}, /* 00,15,00,aa */
	{0xaa, 0x16, 0x0000}, /* 00,16,00,aa */
	{0xaa, 0x17, 0x0001}, /* 00,17,01,aa */
	{0xaa, 0x18, 0x00d8}, /* 00,18,d8,aa */
	{0xaa, 0x19, 0x0002}, /* 00,19,02,aa */
	{0xaa, 0x1a, 0x0088}, /* 00,1a,88,aa */
	{0xaa, 0x20, 0x00a8}, /* 00,20,a8,aa */
	{0xaa, 0x22, 0x0000}, /* 00,22,00,aa */
	{0xaa, 0x23, 0x0000}, /* 00,23,00,aa */
	{0xaa, 0x24, 0x0000}, /* 00,24,00,aa */
	{0xaa, 0x40, 0x0033}, /* 00,40,33,aa */
	{0xaa, 0x41, 0x0077}, /* 00,41,77,aa */
	{0xaa, 0x42, 0x0053}, /* 00,42,53,aa */
	{0xaa, 0x43, 0x00b0}, /* 00,43,b0,aa */
	{0xaa, 0x4b, 0x0001}, /* 00,4b,01,aa */
	{0xaa, 0x72, 0x0020}, /* 00,72,20,aa */
	{0xaa, 0x73, 0x0000}, /* 00,73,00,aa */
	{0xaa, 0x80, 0x0000}, /* 00,80,00,aa */
	{0xaa, 0x85, 0x0050}, /* 00,85,50,aa */
	{0xaa, 0x91, 0x0070}, /* 00,91,70,aa */
	{0xaa, 0x92, 0x0072}, /* 00,92,72,aa */
	{0xaa, 0x03, 0x0001}, /* 00,03,01,aa */
	{0xaa, 0x10, 0x00a0}, /* 00,10,a0,aa */
	{0xaa, 0x11, 0x0001}, /* 00,11,01,aa */
	{0xaa, 0x30, 0x0000}, /* 00,30,00,aa */
	{0xaa, 0x60, 0x0000}, /* 00,60,00,aa */
	{0xaa, 0xa0, ZC3XX_R01A_LASTFRAMESTATE}, /* 00,a0,1a,aa */
	{0xaa, 0xa1, 0x0000}, /* 00,a1,00,aa */
	{0xaa, 0xa2, 0x003f}, /* 00,a2,3f,aa */
	{0xaa, 0xa3, 0x0028}, /* 00,a3,28,aa */
	{0xaa, 0xa4, 0x0010}, /* 00,a4,10,aa */
	{0xaa, 0xa5, 0x0020}, /* 00,a5,20,aa */
	{0xaa, 0xb1, 0x0044}, /* 00,b1,44,aa */
	{0xaa, 0xd0, 0x0001}, /* 00,d0,01,aa */
	{0xaa, 0xd1, 0x0085}, /* 00,d1,85,aa */
	{0xaa, 0xd2, 0x0080}, /* 00,d2,80,aa */
	{0xaa, 0xd3, 0x0080}, /* 00,d3,80,aa */
	{0xaa, 0xd4, 0x0080}, /* 00,d4,80,aa */
	{0xaa, 0xd5, 0x0080}, /* 00,d5,80,aa */
	{0xaa, 0xc0, 0x00c3}, /* 00,c0,c3,aa */
	{0xaa, 0xc2, 0x0044}, /* 00,c2,44,aa */
	{0xaa, 0xc4, 0x0040}, /* 00,c4,40,aa */
	{0xaa, 0xc5, 0x0020}, /* 00,c5,20,aa */
	{0xaa, 0xc6, 0x0008}, /* 00,c6,08,aa */
	{0xaa, 0x03, 0x0004}, /* 00,03,04,aa */
	{0xaa, 0x10, 0x0000}, /* 00,10,00,aa */
	{0xaa, 0x40, 0x0030}, /* 00,40,30,aa */
	{0xaa, 0x41, 0x0020}, /* 00,41,20,aa */
	{0xaa, 0x42, 0x002d}, /* 00,42,2d,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x1c, 0x0050}, /* 00,1c,50,aa */
	{0xaa, 0x11, 0x0081}, /* 00,11,81,aa */
	{0xaa, 0x3b, 0x003a}, /* 00,3b,3A,aa */
	{0xaa, 0x3c, 0x0098}, /* 00,3c,98,aa */
	{0xaa, 0x3d, 0x0030}, /* 00,3d,30,aa */
	{0xaa, 0x3e, 0x00d4}, /* 00,3E,D4,aa */
	{0xaa, 0x01, 0x0000}, /* 00,01,00,aa */
	{0xaa, 0x52, 0x00ff}, /* 00,52,FF,aa */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE}, /* 01,00,0d,cc */
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION}, /* 01,01,37,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS}, /* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE}, /* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05}, /* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE}, /* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS}, /* 03,01,08,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE}, /* 01,80,02,cc */
	{0xaa, 0x03, 0x0002}, /* 00,03,02,aa */
	{0xaa, 0x51, 0x004e}, /* 00,51,4E,aa */
	{0xaa, 0x52, 0x0041}, /* 00,52,41,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x50, 0x0010}, /* 00,50,10,aa */
	{0xaa, 0x51, 0x0010}, /* 00,51,10,aa */
	{0xaa, 0x54, 0x0010}, /* 00,54,10,aa */
	{0xaa, 0x55, 0x0010}, /* 00,55,10,aa */
	{0xa0, 0xf0, 0x0199}, /* 01,99,F0,cc */
	{0xa0, 0x80, 0x019a}, /* 01,9A,80,cc */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x001d}, /* 00,36,1D,aa */
	{0xaa, 0x37, 0x004c}, /* 00,37,4C,aa */
	{0xaa, 0x3b, 0x001d}, /* 00,3B,1D,aa */
	{}
};

static const struct usb_action MC501CB_50HZ[] = {
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x001d}, /* 00,36,1D,aa */
	{0xaa, 0x37, 0x004c}, /* 00,37,4C,aa */
	{0xaa, 0x3b, 0x001d}, /* 00,3B,1D,aa */
	{0xaa, 0x3c, 0x004c}, /* 00,3C,4C,aa */
	{0xaa, 0x3d, 0x001d}, /* 00,3D,1D,aa */
	{0xaa, 0x3e, 0x004c}, /* 00,3E,4C,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x003a}, /* 00,36,3A,aa */
	{0xaa, 0x37, 0x0098}, /* 00,37,98,aa */
	{0xaa, 0x3b, 0x003a}, /* 00,3B,3A,aa */
	{}
};

static const struct usb_action MC501CB_50HZScale[] = {
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x003a}, /* 00,36,3A,aa */
	{0xaa, 0x37, 0x0098}, /* 00,37,98,aa */
	{0xaa, 0x3b, 0x003a}, /* 00,3B,3A,aa */
	{0xaa, 0x3c, 0x0098}, /* 00,3C,98,aa */
	{0xaa, 0x3d, 0x003a}, /* 00,3D,3A,aa */
	{0xaa, 0x3e, 0x0098}, /* 00,3E,98,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x0018}, /* 00,36,18,aa */
	{0xaa, 0x37, 0x006a}, /* 00,37,6A,aa */
	{0xaa, 0x3d, 0x0018}, /* 00,3D,18,aa */
	{}
};

static const struct usb_action MC501CB_60HZ[] = {
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x0018}, /* 00,36,18,aa */
	{0xaa, 0x37, 0x006a}, /* 00,37,6A,aa */
	{0xaa, 0x3d, 0x0018}, /* 00,3D,18,aa */
	{0xaa, 0x3e, 0x006a}, /* 00,3E,6A,aa */
	{0xaa, 0x3b, 0x0018}, /* 00,3B,18,aa */
	{0xaa, 0x3c, 0x006a}, /* 00,3C,6A,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x0030}, /* 00,36,30,aa */
	{0xaa, 0x37, 0x00d4}, /* 00,37,D4,aa */
	{0xaa, 0x3d, 0x0030}, /* 00,3D,30,aa */
	{}
};

static const struct usb_action MC501CB_60HZScale[] = {
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x0030}, /* 00,36,30,aa */
	{0xaa, 0x37, 0x00d4}, /* 00,37,D4,aa */
	{0xaa, 0x3d, 0x0030}, /* 00,3D,30,aa */
	{0xaa, 0x3e, 0x00d4}, /* 00,3E,D4,aa */
	{0xaa, 0x3b, 0x0030}, /* 00,3B,30,aa */
	{0xaa, 0x3c, 0x00d4}, /* 00,3C,D4,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x0018}, /* 00,36,18,aa */
	{0xaa, 0x37, 0x006a}, /* 00,37,6A,aa */
	{0xaa, 0x3d, 0x0018}, /* 00,3D,18,aa */
	{}
};

static const struct usb_action MC501CB_NoFliker[] = {
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x0018}, /* 00,36,18,aa */
	{0xaa, 0x37, 0x006a}, /* 00,37,6A,aa */
	{0xaa, 0x3d, 0x0018}, /* 00,3D,18,aa */
	{0xaa, 0x3e, 0x006a}, /* 00,3E,6A,aa */
	{0xaa, 0x3b, 0x0018}, /* 00,3B,18,aa */
	{0xaa, 0x3c, 0x006a}, /* 00,3C,6A,aa */
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x0030}, /* 00,36,30,aa */
	{0xaa, 0x37, 0x00d4}, /* 00,37,D4,aa */
	{0xaa, 0x3d, 0x0030}, /* 00,3D,30,aa */
	{}
};

static const struct usb_action MC501CB_NoFlikerScale[] = {
	{0xaa, 0x03, 0x0003}, /* 00,03,03,aa */
	{0xaa, 0x10, 0x00fc}, /* 00,10,fc,aa */
	{0xaa, 0x36, 0x0030}, /* 00,36,30,aa */
	{0xaa, 0x37, 0x00d4}, /* 00,37,D4,aa */
	{0xaa, 0x3d, 0x0030}, /* 00,3D,30,aa */
	{0xaa, 0x3e, 0x00d4}, /* 00,3E,D4,aa */
	{0xaa, 0x3b, 0x0030}, /* 00,3B,30,aa */
	{0xaa, 0x3c, 0x00d4}, /* 00,3C,D4,aa */
	{}
};

/* from zs211.inf - HKR,%OV7620%,Initial - 640x480 */
static const struct usb_action OV7620_mode0[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL}, /* 00,00,01,cc */
	{0xa0, 0x40, ZC3XX_R002_CLOCKSELECT}, /* 00,02,40,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING}, /* 00,08,00,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING}, /* 00,01,01,cc */
	{0xa0, 0x06, ZC3XX_R010_CMOSSENSORSELECT}, /* 00,10,06,cc */
	{0xa0, 0x02, ZC3XX_R083_RGAINADDR}, /* 00,83,02,cc */
	{0xa0, 0x01, ZC3XX_R085_BGAINADDR}, /* 00,85,01,cc */
	{0xa0, 0x80, ZC3XX_R086_EXPTIMEHIGH}, /* 00,86,80,cc */
	{0xa0, 0x81, ZC3XX_R087_EXPTIMEMID}, /* 00,87,81,cc */
	{0xa0, 0x10, ZC3XX_R088_EXPTIMELOW}, /* 00,88,10,cc */
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR}, /* 00,8b,a1,cc */
	{0xa0, 0x08, ZC3XX_R08D_COMPABILITYMODE}, /* 00,8d,08,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH}, /* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW}, /* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH}, /* 00,05,01,cc */
	{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW}, /* 00,06,d8,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,01,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW}, /* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW}, /* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW}, /* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW}, /* 01,1c,00,cc */
	{0xa0, 0xde, ZC3XX_R09C_WINHEIGHTLOW}, /* 00,9c,de,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW}, /* 00,9e,86,cc */
	{0xaa, 0x12, 0x0088}, /* 00,12,88,aa */
	{0xaa, 0x12, 0x0048}, /* 00,12,48,aa */
	{0xaa, 0x75, 0x008a}, /* 00,75,8a,aa */
	{0xaa, 0x13, 0x00a3}, /* 00,13,a3,aa */
	{0xaa, 0x04, 0x0000}, /* 00,04,00,aa */
	{0xaa, 0x05, 0x0000}, /* 00,05,00,aa */
	{0xaa, 0x14, 0x0000}, /* 00,14,00,aa */
	{0xaa, 0x15, 0x0004}, /* 00,15,04,aa */
	{0xaa, 0x17, 0x0018}, /* 00,17,18,aa */
	{0xaa, 0x18, 0x00ba}, /* 00,18,ba,aa */
	{0xaa, 0x19, 0x0002}, /* 00,19,02,aa */
	{0xaa, 0x1a, 0x00f1}, /* 00,1a,f1,aa */
	{0xaa, 0x20, 0x0040}, /* 00,20,40,aa */
	{0xaa, 0x24, 0x0088}, /* 00,24,88,aa */
	{0xaa, 0x25, 0x0078}, /* 00,25,78,aa */
	{0xaa, 0x27, 0x00f6}, /* 00,27,f6,aa */
	{0xaa, 0x28, 0x00a0}, /* 00,28,a0,aa */
	{0xaa, 0x21, 0x0000}, /* 00,21,00,aa */
	{0xaa, 0x2a, 0x0083}, /* 00,2a,83,aa */
	{0xaa, 0x2b, 0x0096}, /* 00,2b,96,aa */
	{0xaa, 0x2d, 0x0005}, /* 00,2d,05,aa */
	{0xaa, 0x74, 0x0020}, /* 00,74,20,aa */
	{0xaa, 0x61, 0x0068}, /* 00,61,68,aa */
	{0xaa, 0x64, 0x0088}, /* 00,64,88,aa */
	{0xaa, 0x00, 0x0000}, /* 00,00,00,aa */
	{0xaa, 0x06, 0x0080}, /* 00,06,80,aa */
	{0xaa, 0x01, 0x0090}, /* 00,01,90,aa */
	{0xaa, 0x02, 0x0030}, /* 00,02,30,aa */
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION}, /* 01,01,77,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE}, /* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS}, /* 01,89,06,cc */
	{0xa0, 0x00, 0x01ad}, /* 01,ad,00,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE}, /* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05}, /* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE}, /* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS}, /* 03,01,08,cc */
	{0xa0, 0x68, ZC3XX_R116_RGAIN}, /* 01,16,68,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN}, /* 01,18,52,cc */
	{0xa0, 0x40, ZC3XX_R11D_GLOBALGAIN}, /* 01,1d,40,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE}, /* 01,80,02,cc */
	{0xa0, 0x50, ZC3XX_R1A8_DIGITALGAIN}, /* 01,a8,50,cc */
	{}
};

/* from zs211.inf - HKR,%OV7620%,InitialScale - 320x240 */
static const struct usb_action OV7620_mode1[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL}, /* 00,00,01,cc */
	{0xa0, 0x50, ZC3XX_R002_CLOCKSELECT},	/* 00,02,50,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00,08,00,cc */
						/* mx change? */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING}, /* 00,01,01,cc */
	{0xa0, 0x06, ZC3XX_R010_CMOSSENSORSELECT}, /* 00,10,06,cc */
	{0xa0, 0x02, ZC3XX_R083_RGAINADDR},	/* 00,83,02,cc */
	{0xa0, 0x01, ZC3XX_R085_BGAINADDR},	/* 00,85,01,cc */
	{0xa0, 0x80, ZC3XX_R086_EXPTIMEHIGH},	/* 00,86,80,cc */
	{0xa0, 0x81, ZC3XX_R087_EXPTIMEMID},	/* 00,87,81,cc */
	{0xa0, 0x10, ZC3XX_R088_EXPTIMELOW},	/* 00,88,10,cc */
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR}, /* 00,8b,a1,cc */
	{0xa0, 0x08, ZC3XX_R08D_COMPABILITYMODE}, /* 00,8d,08,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH}, /* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW}, /* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH}, /* 00,05,01,cc */
	{0xa0, 0xd0, ZC3XX_R006_FRAMEHEIGHTLOW}, /* 00,06,d0,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,01,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},	/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},	/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},	/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},	/* 01,1c,00,cc */
	{0xa0, 0xd6, ZC3XX_R09C_WINHEIGHTLOW},	/* 00,9c,d6,cc */
						/* OV7648 00,9c,d8,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},	/* 00,9e,88,cc */
	{0xaa, 0x12, 0x0088}, /* 00,12,88,aa */
	{0xaa, 0x12, 0x0048}, /* 00,12,48,aa */
	{0xaa, 0x75, 0x008a}, /* 00,75,8a,aa */
	{0xaa, 0x13, 0x00a3}, /* 00,13,a3,aa */
	{0xaa, 0x04, 0x0000}, /* 00,04,00,aa */
	{0xaa, 0x05, 0x0000}, /* 00,05,00,aa */
	{0xaa, 0x14, 0x0000}, /* 00,14,00,aa */
	{0xaa, 0x15, 0x0004}, /* 00,15,04,aa */
	{0xaa, 0x24, 0x0088}, /* 00,24,88,aa */
	{0xaa, 0x25, 0x0078}, /* 00,25,78,aa */
	{0xaa, 0x17, 0x0018}, /* 00,17,18,aa */
	{0xaa, 0x18, 0x00ba}, /* 00,18,ba,aa */
	{0xaa, 0x19, 0x0002}, /* 00,19,02,aa */
	{0xaa, 0x1a, 0x00f2}, /* 00,1a,f2,aa */
	{0xaa, 0x20, 0x0040}, /* 00,20,40,aa */
	{0xaa, 0x27, 0x00f6}, /* 00,27,f6,aa */
	{0xaa, 0x28, 0x00a0}, /* 00,28,a0,aa */
	{0xaa, 0x21, 0x0000}, /* 00,21,00,aa */
	{0xaa, 0x2a, 0x0083}, /* 00,2a,83,aa */
	{0xaa, 0x2b, 0x0096}, /* 00,2b,96,aa */
	{0xaa, 0x2d, 0x0005}, /* 00,2d,05,aa */
	{0xaa, 0x74, 0x0020}, /* 00,74,20,aa */
	{0xaa, 0x61, 0x0068}, /* 00,61,68,aa */
	{0xaa, 0x64, 0x0088}, /* 00,64,88,aa */
	{0xaa, 0x00, 0x0000}, /* 00,00,00,aa */
	{0xaa, 0x06, 0x0080}, /* 00,06,80,aa */
	{0xaa, 0x01, 0x0090}, /* 00,01,90,aa */
	{0xaa, 0x02, 0x0030}, /* 00,02,30,aa */
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION}, /* 01,01,77,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE}, /* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},	/* 01,89,06,cc */
	{0xa0, 0x00, 0x01ad},			/* 01,ad,00,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE}, /* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},	/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE}, /* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},	/* 03,01,08,cc */
	{0xa0, 0x68, ZC3XX_R116_RGAIN},		/* 01,16,68,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},		/* 01,18,52,cc */
	{0xa0, 0x50, ZC3XX_R11D_GLOBALGAIN},	/* 01,1d,50,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE}, /* 01,80,02,cc */
	{0xa0, 0x50, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,50,cc */
	{}
};

/* from zs211.inf - HKR,%OV7620%\AE,50HZ */
static const struct usb_action OV7620_50HZ[] = {
	{0xaa, 0x13, 0x00a3},	/* 00,13,a3,aa */
	{0xdd, 0x00, 0x0100},	/* 00,01,00,dd */
	{0xaa, 0x2b, 0x0096},	/* 00,2b,96,aa */
	{0xaa, 0x75, 0x008a},	/* 00,75,8a,aa */
	{0xaa, 0x2d, 0x0005},	/* 00,2d,05,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,04,cc */
	{0xa0, 0x18, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,18,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x83, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,83,cc */
	{0xaa, 0x10, 0x0082},				/* 00,10,82,aa */
	{0xaa, 0x76, 0x0003},				/* 00,76,03,aa */
/*	{0xa0, 0x40, ZC3XX_R002_CLOCKSELECT},		 * 00,02,40,cc
							 if mode0 (640x480) */
	{}
};

/* from zs211.inf - HKR,%OV7620%\AE,60HZ */
static const struct usb_action OV7620_60HZ[] = {
	{0xaa, 0x13, 0x00a3},			/* 00,13,a3,aa */
						/* (bug in zs211.inf) */
	{0xdd, 0x00, 0x0100},			/* 00,01,00,dd */
	{0xaa, 0x2b, 0x0000},			/* 00,2b,00,aa */
	{0xaa, 0x75, 0x008a},			/* 00,75,8a,aa */
	{0xaa, 0x2d, 0x0005},			/* 00,2d,05,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,04,cc */
	{0xa0, 0x18, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,18,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x83, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,83,cc */
	{0xaa, 0x10, 0x0020},			/* 00,10,20,aa */
	{0xaa, 0x76, 0x0003},			/* 00,76,03,aa */
/*	{0xa0, 0x40, ZC3XX_R002_CLOCKSELECT},	 * 00,02,40,cc
						 * if mode0 (640x480) */
/* ?? in gspca v1, it was
	{0xa0, 0x00, 0x0039},  * 00,00,00,dd *
	{0xa1, 0x01, 0x0037},		*/
	{}
};

/* from zs211.inf - HKR,%OV7620%\AE,NoFliker */
static const struct usb_action OV7620_NoFliker[] = {
	{0xaa, 0x13, 0x00a3},			/* 00,13,a3,aa */
						/* (bug in zs211.inf) */
	{0xdd, 0x00, 0x0100},			/* 00,01,00,dd */
	{0xaa, 0x2b, 0x0000},			/* 00,2b,00,aa */
	{0xaa, 0x75, 0x008e},			/* 00,75,8e,aa */
	{0xaa, 0x2d, 0x0001},			/* 00,2d,01,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,04,cc */
	{0xa0, 0x18, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,18,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x01, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,01,cc */
/*	{0xa0, 0x44, ZC3XX_R002_CLOCKSELECT},	 * 00,02,44,cc
						 - if mode1 (320x240) */
/* ?? was
	{0xa0, 0x00, 0x0039},  * 00,00,00,dd *
	{0xa1, 0x01, 0x0037},		*/
	{}
};

static const struct usb_action ov7630c_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x06, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x08, ZC3XX_R08D_COMPABILITYMODE},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xaa, 0x12, 0x0080},
	{0xa0, 0x02, ZC3XX_R083_RGAINADDR},
	{0xa0, 0x01, ZC3XX_R085_BGAINADDR},
	{0xa0, 0x90, ZC3XX_R086_EXPTIMEHIGH},
	{0xa0, 0x91, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x10, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xd8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xaa, 0x12, 0x0069},
	{0xaa, 0x04, 0x0020},
	{0xaa, 0x06, 0x0050},
	{0xaa, 0x13, 0x0083},
	{0xaa, 0x14, 0x0000},
	{0xaa, 0x15, 0x0024},
	{0xaa, 0x17, 0x0018},
	{0xaa, 0x18, 0x00ba},
	{0xaa, 0x19, 0x0002},
	{0xaa, 0x1a, 0x00f6},
	{0xaa, 0x1b, 0x0002},
	{0xaa, 0x20, 0x00c2},
	{0xaa, 0x24, 0x0060},
	{0xaa, 0x25, 0x0040},
	{0xaa, 0x26, 0x0030},
	{0xaa, 0x27, 0x00ea},
	{0xaa, 0x28, 0x00a0},
	{0xaa, 0x21, 0x0000},
	{0xaa, 0x2a, 0x0081},
	{0xaa, 0x2b, 0x0096},
	{0xaa, 0x2d, 0x0094},
	{0xaa, 0x2f, 0x003d},
	{0xaa, 0x30, 0x0024},
	{0xaa, 0x60, 0x0000},
	{0xaa, 0x61, 0x0040},
	{0xaa, 0x68, 0x007c},
	{0xaa, 0x6f, 0x0015},
	{0xaa, 0x75, 0x0088},
	{0xaa, 0x77, 0x00b5},
	{0xaa, 0x01, 0x0060},
	{0xaa, 0x02, 0x0060},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x46, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R113_RGB03},
/* 0x10, */
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},
/* 0x03, */
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x01, ZC3XX_R120_GAMMA00},	/* gamma 2 ?*/
	{0xa0, 0x0c, ZC3XX_R121_GAMMA01},
	{0xa0, 0x1f, ZC3XX_R122_GAMMA02},
	{0xa0, 0x3a, ZC3XX_R123_GAMMA03},
	{0xa0, 0x53, ZC3XX_R124_GAMMA04},
	{0xa0, 0x6d, ZC3XX_R125_GAMMA05},
	{0xa0, 0x85, ZC3XX_R126_GAMMA06},
	{0xa0, 0x9c, ZC3XX_R127_GAMMA07},
	{0xa0, 0xb0, ZC3XX_R128_GAMMA08},
	{0xa0, 0xc2, ZC3XX_R129_GAMMA09},
	{0xa0, 0xd1, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xde, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xe9, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf2, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xf9, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x05, ZC3XX_R130_GAMMA10},
	{0xa0, 0x0f, ZC3XX_R131_GAMMA11},
	{0xa0, 0x16, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1a, ZC3XX_R133_GAMMA13},
	{0xa0, 0x19, ZC3XX_R134_GAMMA14},
	{0xa0, 0x19, ZC3XX_R135_GAMMA15},
	{0xa0, 0x17, ZC3XX_R136_GAMMA16},
	{0xa0, 0x15, ZC3XX_R137_GAMMA17},
	{0xa0, 0x12, ZC3XX_R138_GAMMA18},
	{0xa0, 0x10, ZC3XX_R139_GAMMA19},
	{0xa0, 0x0e, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x0b, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x09, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x08, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x06, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x03, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xaa, 0x10, 0x001b},
	{0xaa, 0x76, 0x0002},
	{0xaa, 0x2a, 0x0081},
	{0xaa, 0x2b, 0x0000},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x01, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xb8, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x37, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x50, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xaa, 0x13, 0x0083},	/* 40 */
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action ov7630c_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x06, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x08, ZC3XX_R08D_COMPABILITYMODE},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},

	{0xaa, 0x12, 0x0080},
	{0xa0, 0x02, ZC3XX_R083_RGAINADDR},
	{0xa0, 0x01, ZC3XX_R085_BGAINADDR},
	{0xa0, 0x90, ZC3XX_R086_EXPTIMEHIGH},
	{0xa0, 0x91, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x10, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},
	{0xaa, 0x12, 0x0069},	/* i2c */
	{0xaa, 0x04, 0x0020},
	{0xaa, 0x06, 0x0050},
	{0xaa, 0x13, 0x00c3},
	{0xaa, 0x14, 0x0000},
	{0xaa, 0x15, 0x0024},
	{0xaa, 0x19, 0x0003},
	{0xaa, 0x1a, 0x00f6},
	{0xaa, 0x1b, 0x0002},
	{0xaa, 0x20, 0x00c2},
	{0xaa, 0x24, 0x0060},
	{0xaa, 0x25, 0x0040},
	{0xaa, 0x26, 0x0030},
	{0xaa, 0x27, 0x00ea},
	{0xaa, 0x28, 0x00a0},
	{0xaa, 0x21, 0x0000},
	{0xaa, 0x2a, 0x0081},
	{0xaa, 0x2b, 0x0096},
	{0xaa, 0x2d, 0x0084},
	{0xaa, 0x2f, 0x003d},
	{0xaa, 0x30, 0x0024},
	{0xaa, 0x60, 0x0000},
	{0xaa, 0x61, 0x0040},
	{0xaa, 0x68, 0x007c},
	{0xaa, 0x6f, 0x0015},
	{0xaa, 0x75, 0x0088},
	{0xaa, 0x77, 0x00b5},
	{0xaa, 0x01, 0x0060},
	{0xaa, 0x02, 0x0060},
	{0xaa, 0x17, 0x0018},
	{0xaa, 0x18, 0x00ba},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x46, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R113_RGB03},

	{0xa1, 0x01, 0x0002},
	{0xa0, 0x4e, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xfe, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf7, ZC3XX_R10D_RGB10},
	{0xa0, 0x4d, ZC3XX_R10E_RGB11},
	{0xa0, 0xfc, ZC3XX_R10F_RGB12},
	{0xa0, 0x00, ZC3XX_R110_RGB20},
	{0xa0, 0xf6, ZC3XX_R111_RGB21},
	{0xa0, 0x4a, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x16, ZC3XX_R120_GAMMA00},	/* gamma ~4 */
	{0xa0, 0x3a, ZC3XX_R121_GAMMA01},
	{0xa0, 0x5b, ZC3XX_R122_GAMMA02},
	{0xa0, 0x7c, ZC3XX_R123_GAMMA03},
	{0xa0, 0x94, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa9, ZC3XX_R125_GAMMA05},
	{0xa0, 0xbb, ZC3XX_R126_GAMMA06},
	{0xa0, 0xca, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd7, ZC3XX_R128_GAMMA08},
	{0xa0, 0xe1, ZC3XX_R129_GAMMA09},
	{0xa0, 0xea, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xf1, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf7, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xfc, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x20, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x4e, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xfe, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf7, ZC3XX_R10D_RGB10},
	{0xa0, 0x4d, ZC3XX_R10E_RGB11},
	{0xa0, 0xfc, ZC3XX_R10F_RGB12},
	{0xa0, 0x00, ZC3XX_R110_RGB20},
	{0xa0, 0xf6, ZC3XX_R111_RGB21},
	{0xa0, 0x4a, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xaa, 0x10, 0x000d},
	{0xaa, 0x76, 0x0002},
	{0xaa, 0x2a, 0x0081},
	{0xaa, 0x2b, 0x0000},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x00, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xd8, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x1b, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x50, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xaa, 0x13, 0x00c3},

	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action pas106b_Initial_com[] = {
/* Sream and Sensor specific */
	{0xa1, 0x01, 0x0010},	/* CMOSSensorSelect */
/* System */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},	/* SystemControl */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},	/* SystemControl */
/* Picture size */
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},	/* ClockSelect */
	{0xa0, 0x03, 0x003a},
	{0xa0, 0x0c, 0x003b},
	{0xa0, 0x04, 0x0038},
	{}
};

static const struct usb_action pas106b_Initial[] = {	/* 176x144 */
/* JPEG control */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
/* Sream and Sensor specific */
	{0xa0, 0x0f, ZC3XX_R010_CMOSSENSORSELECT},
/* Picture size */
	{0xa0, 0x00, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0xb0, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x00, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0x90, ZC3XX_R006_FRAMEHEIGHTLOW},
/* System */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
/* Sream and Sensor specific */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
/* Sensor Interface */
	{0xa0, 0x08, ZC3XX_R08D_COMPABILITYMODE},
/* Window inside sensor array */
	{0xa0, 0x03, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x03, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x28, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x68, ZC3XX_R09E_WINWIDTHLOW},
/* Init the sensor */
	{0xaa, 0x02, 0x0004},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x09, 0x0005},
	{0xaa, 0x0a, 0x0002},
	{0xaa, 0x0b, 0x0002},
	{0xaa, 0x0c, 0x0005},
	{0xaa, 0x0d, 0x0000},
	{0xaa, 0x0e, 0x0002},
	{0xaa, 0x14, 0x0081},

/* Other registors */
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
/* Frame retreiving */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
/* Gains */
	{0xa0, 0xa0, ZC3XX_R1A8_DIGITALGAIN},
/* Unknown */
	{0xa0, 0x00, 0x01ad},
/* Sharpness */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
/* Other registors */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
/* Auto exposure and white balance */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
/*Dead pixels */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
/* EEPROM */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
/* JPEG control */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},
/* Other registers */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
/* Auto exposure and white balance */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
/*Dead pixels */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
/* EEPROM */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
/* JPEG control */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},

	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
/* Auto correction */
	{0xa0, 0x03, ZC3XX_R181_WINXSTART},
	{0xa0, 0x08, ZC3XX_R182_WINXWIDTH},
	{0xa0, 0x16, ZC3XX_R183_WINXCENTER},
	{0xa0, 0x03, ZC3XX_R184_WINYSTART},
	{0xa0, 0x05, ZC3XX_R185_WINYWIDTH},
	{0xa0, 0x14, ZC3XX_R186_WINYCENTER},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},

/* Auto exposure and white balance */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x03, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xb1, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x87, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},
/* sensor on */
	{0xaa, 0x07, 0x00b1},
	{0xaa, 0x05, 0x0003},
	{0xaa, 0x04, 0x0001},
	{0xaa, 0x03, 0x003b},
/* Gains */
	{0xa0, 0x20, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xa0, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
/* Auto correction */
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},				/* AutoCorrectEnable */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
/* Gains */
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};

static const struct usb_action pas106b_InitialScale[] = {	/* 352x288 */
/* JPEG control */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
/* Sream and Sensor specific */
	{0xa0, 0x0f, ZC3XX_R010_CMOSSENSORSELECT},
/* Picture size */
	{0xa0, 0x01, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x60, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0x20, ZC3XX_R006_FRAMEHEIGHTLOW},
/* System */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
/* Sream and Sensor specific */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
/* Sensor Interface */
	{0xa0, 0x08, ZC3XX_R08D_COMPABILITYMODE},
/* Window inside sensor array */
	{0xa0, 0x03, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x03, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x28, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x68, ZC3XX_R09E_WINWIDTHLOW},
/* Init the sensor */
	{0xaa, 0x02, 0x0004},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x09, 0x0005},
	{0xaa, 0x0a, 0x0002},
	{0xaa, 0x0b, 0x0002},
	{0xaa, 0x0c, 0x0005},
	{0xaa, 0x0d, 0x0000},
	{0xaa, 0x0e, 0x0002},
	{0xaa, 0x14, 0x0081},

/* Other registors */
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
/* Frame retreiving */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
/* Gains */
	{0xa0, 0xa0, ZC3XX_R1A8_DIGITALGAIN},
/* Unknown */
	{0xa0, 0x00, 0x01ad},
/* Sharpness */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
/* Other registors */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
/* Auto exposure and white balance */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x80, ZC3XX_R18D_YTARGET},
/*Dead pixels */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
/* EEPROM */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
/* JPEG control */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},
/* Other registers */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
/* Auto exposure and white balance */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
/*Dead pixels */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
/* EEPROM */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
/* JPEG control */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},

	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
/* Auto correction */
	{0xa0, 0x03, ZC3XX_R181_WINXSTART},
	{0xa0, 0x08, ZC3XX_R182_WINXWIDTH},
	{0xa0, 0x16, ZC3XX_R183_WINXCENTER},
	{0xa0, 0x03, ZC3XX_R184_WINYSTART},
	{0xa0, 0x05, ZC3XX_R185_WINYWIDTH},
	{0xa0, 0x14, ZC3XX_R186_WINYCENTER},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},

/* Auto exposure and white balance */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x03, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xb1, ZC3XX_R192_EXPOSURELIMITLOW},

	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x87, ZC3XX_R197_ANTIFLICKERLOW},

	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
/* sensor on */
	{0xaa, 0x07, 0x00b1},
	{0xaa, 0x05, 0x0003},
	{0xaa, 0x04, 0x0001},
	{0xaa, 0x03, 0x003b},
/* Gains */
	{0xa0, 0x20, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xa0, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
/* Auto correction */
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},				/* AutoCorrectEnable */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
/* Gains */
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},

	{0xa0, 0x00, 0x0007},			/* AutoCorrectEnable */
	{0xa0, 0xff, ZC3XX_R018_FRAMELOST},	/* Frame adjust */
	{}
};
static const struct usb_action pas106b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x06, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,06,cc */
	{0xa0, 0x54, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,54,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x87, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,87,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x30, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,30,cc */
	{0xaa, 0x03, 0x0021},			/* 00,03,21,aa */
	{0xaa, 0x04, 0x000c},			/* 00,04,0c,aa */
	{0xaa, 0x05, 0x0002},			/* 00,05,02,aa */
	{0xaa, 0x07, 0x001c},			/* 00,07,1c,aa */
	{0xa0, 0x04, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,04,cc */
	{}
};
static const struct usb_action pas106b_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x06, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,06,cc */
	{0xa0, 0x2e, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,2e,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x71, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,71,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x30, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,30,cc */
	{0xaa, 0x03, 0x001c},			/* 00,03,1c,aa */
	{0xaa, 0x04, 0x0004},			/* 00,04,04,aa */
	{0xaa, 0x05, 0x0001},			/* 00,05,01,aa */
	{0xaa, 0x07, 0x00c4},			/* 00,07,c4,aa */
	{0xa0, 0x04, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,04,cc */
	{}
};
static const struct usb_action pas106b_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x06, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,06,cc */
	{0xa0, 0x50, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,50,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,20,cc */
	{0xaa, 0x03, 0x0013},			/* 00,03,13,aa */
	{0xaa, 0x04, 0x0000},			/* 00,04,00,aa */
	{0xaa, 0x05, 0x0001},			/* 00,05,01,aa */
	{0xaa, 0x07, 0x0030},			/* 00,07,30,aa */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{}
};

/* from usbvm31b.inf */
static const struct usb_action pas202b_Initial[] = {	/* 640x480 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,00,cc */
	{0xa0, 0x0e, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0e,cc */
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},		/* 00,02,00,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x08, ZC3XX_R08D_COMPABILITYMODE},	/* 00,8d,08,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,00,cc */
	{0xa0, 0x03, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,03,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x03, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,03,cc */
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},		/* 00,9b,01,cc */
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,e6,cc */
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},		/* 00,9d,02,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,86,cc */
	{0xaa, 0x02, 0x0002},			/* 00,02,04,aa --> 02 */
	{0xaa, 0x07, 0x0006},				/* 00,07,06,aa */
	{0xaa, 0x08, 0x0002},				/* 00,08,02,aa */
	{0xaa, 0x09, 0x0006},				/* 00,09,06,aa */
	{0xaa, 0x0a, 0x0001},				/* 00,0a,01,aa */
	{0xaa, 0x0b, 0x0001},				/* 00,0b,01,aa */
	{0xaa, 0x0c, 0x0008},				/* 00,0c,08,aa */
	{0xaa, 0x0d, 0x0000},				/* 00,0d,00,aa */
	{0xaa, 0x10, 0x0000},				/* 00,10,00,aa */
	{0xaa, 0x12, 0x0005},				/* 00,12,05,aa */
	{0xaa, 0x13, 0x0063},				/* 00,13,63,aa */
	{0xaa, 0x15, 0x0070},				/* 00,15,70,aa */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b7,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x00, 0x01ad},				/* 01,ad,00,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc */
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},		/* 01,8d,70,cc */
	{}
};
static const struct usb_action pas202b_InitialScale[] = {	/* 320x240 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,00,cc */
	{0xa0, 0x0e, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0e,cc */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},		/* 00,02,10,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x08, ZC3XX_R08D_COMPABILITYMODE},	/* 00,8d,08,cc */
	{0xa0, 0x08, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,08,cc */
	{0xa0, 0x02, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,02,cc */
	{0xa0, 0x08, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,08,cc */
	{0xa0, 0x02, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,02,cc */
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},		/* 00,9b,01,cc */
	{0xa0, 0xd8, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,d8,cc */
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},		/* 00,9d,02,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,88,cc */
	{0xaa, 0x02, 0x0002},				/* 00,02,02,aa */
	{0xaa, 0x07, 0x0006},				/* 00,07,06,aa */
	{0xaa, 0x08, 0x0002},				/* 00,08,02,aa */
	{0xaa, 0x09, 0x0006},				/* 00,09,06,aa */
	{0xaa, 0x0a, 0x0001},				/* 00,0a,01,aa */
	{0xaa, 0x0b, 0x0001},				/* 00,0b,01,aa */
	{0xaa, 0x0c, 0x0008},				/* 00,0c,08,aa */
	{0xaa, 0x0d, 0x0000},				/* 00,0d,00,aa */
	{0xaa, 0x10, 0x0000},				/* 00,10,00,aa */
	{0xaa, 0x12, 0x0005},				/* 00,12,05,aa */
	{0xaa, 0x13, 0x0063},				/* 00,13,63,aa */
	{0xaa, 0x15, 0x0070},				/* 00,15,70,aa */
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x00, 0x01ad},				/* 01,ad,00,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc */
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},		/* 01,8d,70,cc */
	{}
};
static const struct usb_action pas202b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},		/* 00,19,00,cc */
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},		/* 00,87,20,cc */
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},		/* 00,88,21,cc */
	{0xaa, 0x20, 0x0002},				/* 00,20,02,aa */
	{0xaa, 0x21, 0x0068},				/* 00,21,68,aa */
	{0xaa, 0x03, 0x0044},				/* 00,03,44,aa */
	{0xaa, 0x04, 0x0009},				/* 00,04,09,aa */
	{0xaa, 0x05, 0x0028},				/* 00,05,28,aa */
	{0xaa, 0x0e, 0x0001},				/* 00,0e,01,aa */
	{0xaa, 0x0f, 0x0000},				/* 00,0f,00,aa */
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,14,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,07,cc */
	{0xa0, 0xd2, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,d2,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x4d, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,4d,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},		/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,20,cc */
	{0xa0, 0x44, ZC3XX_R01D_HSYNC_0},		/* 00,1d,44,cc */
	{0xa0, 0x6f, ZC3XX_R01E_HSYNC_1},		/* 00,1e,6f,cc */
	{0xa0, 0xad, ZC3XX_R01F_HSYNC_2},		/* 00,1f,ad,cc */
	{0xa0, 0xeb, ZC3XX_R020_HSYNC_3},		/* 00,20,eb,cc */
	{0xa0, 0x0f, ZC3XX_R087_EXPTIMEMID},		/* 00,87,0f,cc */
	{0xa0, 0x0e, ZC3XX_R088_EXPTIMELOW},		/* 00,88,0e,cc */
	{}
};
static const struct usb_action pas202b_50HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},		/* 00,19,00,cc */
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},		/* 00,87,20,cc */
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},		/* 00,88,21,cc */
	{0xaa, 0x20, 0x0002},				/* 00,20,02,aa */
	{0xaa, 0x21, 0x006c},				/* 00,21,6c,aa */
	{0xaa, 0x03, 0x0041},				/* 00,03,41,aa */
	{0xaa, 0x04, 0x0009},				/* 00,04,09,aa */
	{0xaa, 0x05, 0x002c},				/* 00,05,2c,aa */
	{0xaa, 0x0e, 0x0001},				/* 00,0e,01,aa */
	{0xaa, 0x0f, 0x0000},				/* 00,0f,00,aa */
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,14,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x0f, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0f,cc */
	{0xa0, 0xbe, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,be,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x9b, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,9b,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},		/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,20,cc */
	{0xa0, 0x41, ZC3XX_R01D_HSYNC_0},		/* 00,1d,41,cc */
	{0xa0, 0x6f, ZC3XX_R01E_HSYNC_1},		/* 00,1e,6f,cc */
	{0xa0, 0xad, ZC3XX_R01F_HSYNC_2},		/* 00,1f,ad,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc */
	{0xa0, 0x0f, ZC3XX_R087_EXPTIMEMID},		/* 00,87,0f,cc */
	{0xa0, 0x0e, ZC3XX_R088_EXPTIMELOW},		/* 00,88,0e,cc */
	{}
};
static const struct usb_action pas202b_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},		/* 00,19,00,cc */
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},		/* 00,87,20,cc */
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},		/* 00,88,21,cc */
	{0xaa, 0x20, 0x0002},				/* 00,20,02,aa */
	{0xaa, 0x21, 0x0000},				/* 00,21,00,aa */
	{0xaa, 0x03, 0x0045},				/* 00,03,45,aa */
	{0xaa, 0x04, 0x0008},				/* 00,04,08,aa */
	{0xaa, 0x05, 0x0000},				/* 00,05,00,aa */
	{0xaa, 0x0e, 0x0001},				/* 00,0e,01,aa */
	{0xaa, 0x0f, 0x0000},				/* 00,0f,00,aa */
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,14,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,07,cc */
	{0xa0, 0xc0, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,c0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x40, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,40,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},		/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,20,cc */
	{0xa0, 0x45, ZC3XX_R01D_HSYNC_0},		/* 00,1d,45,cc */
	{0xa0, 0x8e, ZC3XX_R01E_HSYNC_1},		/* 00,1e,8e,cc */
	{0xa0, 0xc1, ZC3XX_R01F_HSYNC_2},		/* 00,1f,c1,cc */
	{0xa0, 0xf5, ZC3XX_R020_HSYNC_3},		/* 00,20,f5,cc */
	{0xa0, 0x0f, ZC3XX_R087_EXPTIMEMID},		/* 00,87,0f,cc */
	{0xa0, 0x0e, ZC3XX_R088_EXPTIMELOW},		/* 00,88,0e,cc */
	{}
};
static const struct usb_action pas202b_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},		/* 00,19,00,cc */
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},		/* 00,87,20,cc */
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},		/* 00,88,21,cc */
	{0xaa, 0x20, 0x0002},				/* 00,20,02,aa */
	{0xaa, 0x21, 0x0004},				/* 00,21,04,aa */
	{0xaa, 0x03, 0x0042},				/* 00,03,42,aa */
	{0xaa, 0x04, 0x0008},				/* 00,04,08,aa */
	{0xaa, 0x05, 0x0004},				/* 00,05,04,aa */
	{0xaa, 0x0e, 0x0001},				/* 00,0e,01,aa */
	{0xaa, 0x0f, 0x0000},				/* 00,0f,00,aa */
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,14,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x0f, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0f,cc */
	{0xa0, 0x9f, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,9f,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x81, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,81,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},		/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,20,cc */
	{0xa0, 0x42, ZC3XX_R01D_HSYNC_0},		/* 00,1d,42,cc */
	{0xa0, 0x6f, ZC3XX_R01E_HSYNC_1},		/* 00,1e,6f,cc */
	{0xa0, 0xaf, ZC3XX_R01F_HSYNC_2},		/* 00,1f,af,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc */
	{0xa0, 0x0f, ZC3XX_R087_EXPTIMEMID},		/* 00,87,0f,cc */
	{0xa0, 0x0e, ZC3XX_R088_EXPTIMELOW},		/* 00,88,0e,cc */
	{}
};
static const struct usb_action pas202b_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},		/* 00,19,00,cc */
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},		/* 00,87,20,cc */
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},		/* 00,88,21,cc */
	{0xaa, 0x20, 0x0002},				/* 00,20,02,aa */
	{0xaa, 0x21, 0x0020},				/* 00,21,20,aa */
	{0xaa, 0x03, 0x0040},				/* 00,03,40,aa */
	{0xaa, 0x04, 0x0008},				/* 00,04,08,aa */
	{0xaa, 0x05, 0x0020},				/* 00,05,20,aa */
	{0xaa, 0x0e, 0x0001},				/* 00,0e,01,aa */
	{0xaa, 0x0f, 0x0000},				/* 00,0f,00,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,07,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x02, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,02,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},		/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,00,cc */
	{0xa0, 0x40, ZC3XX_R01D_HSYNC_0},		/* 00,1d,40,cc */
	{0xa0, 0x60, ZC3XX_R01E_HSYNC_1},		/* 00,1e,60,cc */
	{0xa0, 0x90, ZC3XX_R01F_HSYNC_2},		/* 00,1f,90,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc */
	{0xa0, 0x0f, ZC3XX_R087_EXPTIMEMID},		/* 00,87,0f,cc */
	{0xa0, 0x0e, ZC3XX_R088_EXPTIMELOW},		/* 00,88,0e,cc */
	{}
};
static const struct usb_action pas202b_NoFlikerScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},		/* 00,19,00,cc */
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},		/* 00,87,20,cc */
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},		/* 00,88,21,cc */
	{0xaa, 0x20, 0x0002},				/* 00,20,02,aa */
	{0xaa, 0x21, 0x0010},				/* 00,21,10,aa */
	{0xaa, 0x03, 0x0040},				/* 00,03,40,aa */
	{0xaa, 0x04, 0x0008},				/* 00,04,08,aa */
	{0xaa, 0x05, 0x0010},				/* 00,05,10,aa */
	{0xaa, 0x0e, 0x0001},				/* 00,0e,01,aa */
	{0xaa, 0x0f, 0x0000},				/* 00,0f,00,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x0f, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x02, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,02,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},		/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,00,cc */
	{0xa0, 0x40, ZC3XX_R01D_HSYNC_0},		/* 00,1d,40,cc */
	{0xa0, 0x60, ZC3XX_R01E_HSYNC_1},		/* 00,1e,60,cc */
	{0xa0, 0x90, ZC3XX_R01F_HSYNC_2},		/* 00,1f,90,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc */
	{0xa0, 0x0f, ZC3XX_R087_EXPTIMEMID},		/* 00,87,0f,cc */
	{0xa0, 0x0e, ZC3XX_R088_EXPTIMELOW},		/* 00,88,0e,cc */
	{}
};

static const struct usb_action pb03303x_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0xdc, ZC3XX_R08B_I2CDEVICEADDR},	/* 8b -> dc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xdc, ZC3XX_R08B_I2CDEVICEADDR},
	{0xaa, 0x01, 0x0001},
	{0xaa, 0x06, 0x0000},
	{0xaa, 0x08, 0x0483},
	{0xaa, 0x01, 0x0004},
	{0xaa, 0x08, 0x0006},
	{0xaa, 0x02, 0x0011},
	{0xaa, 0x03, 0x01e7},
	{0xaa, 0x04, 0x0287},
	{0xaa, 0x07, 0x3002},
	{0xaa, 0x20, 0x1100},
	{0xaa, 0x35, 0x0050},
	{0xaa, 0x30, 0x0005},
	{0xaa, 0x31, 0x0000},
	{0xaa, 0x58, 0x0078},
	{0xaa, 0x62, 0x0411},
	{0xaa, 0x2b, 0x0028},
	{0xaa, 0x2c, 0x0030},
	{0xaa, 0x2d, 0x0030},
	{0xaa, 0x2e, 0x0028},
	{0xa0, 0x10, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},
	{0xa0, 0x61, ZC3XX_R116_RGAIN},
	{0xa0, 0x65, ZC3XX_R118_BGAIN},

	{0xa1, 0x01, 0x0002},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x0d, 0x003a},
	{0xa0, 0x02, 0x003b},
	{0xa0, 0x00, 0x0038},
	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x05, 0x0009},
	{0xaa, 0x09, 0x0134},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xec, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x9c, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x1c, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xd7, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xf4, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf9, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action pb03303x_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0xdc, ZC3XX_R08B_I2CDEVICEADDR},	/* 8b -> dc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xdc, ZC3XX_R08B_I2CDEVICEADDR},
	{0xaa, 0x01, 0x0001},
	{0xaa, 0x06, 0x0000},
	{0xaa, 0x08, 0x0483},
	{0xaa, 0x01, 0x0004},
	{0xaa, 0x08, 0x0006},
	{0xaa, 0x02, 0x0011},
	{0xaa, 0x03, 0x01e7},
	{0xaa, 0x04, 0x0287},
	{0xaa, 0x07, 0x3002},
	{0xaa, 0x20, 0x1100},
	{0xaa, 0x35, 0x0050},
	{0xaa, 0x30, 0x0005},
	{0xaa, 0x31, 0x0000},
	{0xaa, 0x58, 0x0078},
	{0xaa, 0x62, 0x0411},
	{0xaa, 0x2b, 0x0028},
	{0xaa, 0x2c, 0x0030},
	{0xaa, 0x2d, 0x0030},
	{0xaa, 0x2e, 0x0028},
	{0xa0, 0x10, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},
	{0xa0, 0x61, ZC3XX_R116_RGAIN},
	{0xa0, 0x65, ZC3XX_R118_BGAIN},

	{0xa1, 0x01, 0x0002},

	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},

	{0xa0, 0x0d, 0x003a},
	{0xa0, 0x02, 0x003b},
	{0xa0, 0x00, 0x0038},
	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x05, 0x0009},
	{0xaa, 0x09, 0x0134},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xec, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x9c, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x1c, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xd7, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xf4, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf9, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};
static const struct usb_action pb0330xx_Initial[] = {
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xaa, 0x01, 0x0006},
	{0xaa, 0x02, 0x0011},
	{0xaa, 0x03, 0x01e7},
	{0xaa, 0x04, 0x0287},
	{0xaa, 0x06, 0x0003},
	{0xaa, 0x07, 0x3002},
	{0xaa, 0x20, 0x1100},
	{0xaa, 0x2f, 0xf7b0},
	{0xaa, 0x30, 0x0005},
	{0xaa, 0x31, 0x0000},
	{0xaa, 0x34, 0x0100},
	{0xaa, 0x35, 0x0060},
	{0xaa, 0x3d, 0x068f},
	{0xaa, 0x40, 0x01e0},
	{0xaa, 0x58, 0x0078},
	{0xaa, 0x62, 0x0411},
	{0xa0, 0x10, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x6c, ZC3XX_R18D_YTARGET},
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x00, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xa1, 0x01, 0x0095},
	{0xa1, 0x01, 0x0096},
	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x05, 0x0066},
	{0xaa, 0x09, 0x02b2},
	{0xaa, 0x10, 0x0002},

	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x8c, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x8a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xd7, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xf0, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf8, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0007},
/*	{0xa0, 0x30, 0x0007}, */
/*	{0xa0, 0x00, 0x0007}, */
	{}
};

static const struct usb_action pb0330xx_InitialScale[] = {
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},	/* 10 */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xaa, 0x01, 0x0006},
	{0xaa, 0x02, 0x0011},
	{0xaa, 0x03, 0x01e7},
	{0xaa, 0x04, 0x0287},
	{0xaa, 0x06, 0x0003},
	{0xaa, 0x07, 0x3002},
	{0xaa, 0x20, 0x1100},
	{0xaa, 0x2f, 0xf7b0},
	{0xaa, 0x30, 0x0005},
	{0xaa, 0x31, 0x0000},
	{0xaa, 0x34, 0x0100},
	{0xaa, 0x35, 0x0060},
	{0xaa, 0x3d, 0x068f},
	{0xaa, 0x40, 0x01e0},
	{0xaa, 0x58, 0x0078},
	{0xaa, 0x62, 0x0411},
	{0xa0, 0x10, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x6c, ZC3XX_R18D_YTARGET},
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x00, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xa1, 0x01, 0x0095},
	{0xa1, 0x01, 0x0096},
	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x50, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf8, ZC3XX_R10B_RGB01},
	{0xa0, 0xf8, ZC3XX_R10C_RGB02},
	{0xa0, 0xf8, ZC3XX_R10D_RGB10},
	{0xa0, 0x50, ZC3XX_R10E_RGB11},
	{0xa0, 0xf8, ZC3XX_R10F_RGB12},
	{0xa0, 0xf8, ZC3XX_R110_RGB20},
	{0xa0, 0xf8, ZC3XX_R111_RGB21},
	{0xa0, 0x50, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x05, 0x0066},
	{0xaa, 0x09, 0x02b2},
	{0xaa, 0x10, 0x0002},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x8c, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x8a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xd7, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xf0, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf8, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0007},
/*	{0xa0, 0x30, 0x0007}, */
/*	{0xa0, 0x00, 0x0007}, */
	{}
};
static const struct usb_action pb0330_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,07,cc */
	{0xa0, 0xee, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,ee,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x46, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,46,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,26,cc */
	{0xa0, 0x68, ZC3XX_R01D_HSYNC_0}, /* 00,1d,68,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,c8,cc */
	{}
};
static const struct usb_action pb0330_50HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,07,cc */
	{0xa0, 0xa0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,a0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x7a, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,7a,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,26,cc */
	{0xa0, 0xe5, ZC3XX_R01D_HSYNC_0}, /* 00,1d,e5,cc */
	{0xa0, 0xf0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,f0,cc */
	{0xa0, 0xf8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,f8,cc */
	{}
};
static const struct usb_action pb0330_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,07,cc */
	{0xa0, 0xdd, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,dd,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x3d, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,3d,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,26,cc */
	{0xa0, 0x43, ZC3XX_R01D_HSYNC_0}, /* 00,1d,43,cc */
	{0xa0, 0x50, ZC3XX_R01E_HSYNC_1}, /* 00,1e,50,cc */
	{0xa0, 0x90, ZC3XX_R01F_HSYNC_2}, /* 00,1f,90,cc */
	{}
};
static const struct usb_action pb0330_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,07,cc */
	{0xa0, 0xa0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,a0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x7a, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,7a,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,26,cc */
	{0xa0, 0x41, ZC3XX_R01D_HSYNC_0}, /* 00,1d,41,cc */
	{0xa0, 0x50, ZC3XX_R01E_HSYNC_1}, /* 00,1e,50,cc */
	{0xa0, 0x90, ZC3XX_R01F_HSYNC_2}, /* 00,1f,90,cc */
	{}
};
static const struct usb_action pb0330_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,07,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x09, ZC3XX_R01D_HSYNC_0}, /* 00,1d,09,cc */
	{0xa0, 0x40, ZC3XX_R01E_HSYNC_1}, /* 00,1e,40,cc */
	{0xa0, 0x90, ZC3XX_R01F_HSYNC_2}, /* 00,1f,90,cc */
	{}
};
static const struct usb_action pb0330_NoFlikerScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,07,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x09, ZC3XX_R01D_HSYNC_0},	/* 00,1d,09,cc */
	{0xa0, 0x40, ZC3XX_R01E_HSYNC_1},	/* 00,1e,40,cc */
	{0xa0, 0x90, ZC3XX_R01F_HSYNC_2},	/* 00,1f,90,cc */
	{}
};

/* from oem9.inf - HKR,%PO2030%,Initial - 640x480 - (close to CS2102) */
static const struct usb_action PO2030_mode0[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL}, /* 00,00,01,cc */
	{0xa0, 0x04, ZC3XX_R002_CLOCKSELECT},	/* 00,02,04,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT}, /* 00,10,01,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING}, /* 00,01,01,cc */
	{0xa0, 0x04, ZC3XX_R080_HBLANKHIGH}, /* 00,80,04,cc */
	{0xa0, 0x05, ZC3XX_R081_HBLANKLOW}, /* 00,81,05,cc */
	{0xa0, 0x16, ZC3XX_R083_RGAINADDR}, /* 00,83,16,cc */
	{0xa0, 0x18, ZC3XX_R085_BGAINADDR}, /* 00,85,18,cc */
	{0xa0, 0x1a, ZC3XX_R086_EXPTIMEHIGH}, /* 00,86,1a,cc */
	{0xa0, 0x1b, ZC3XX_R087_EXPTIMEMID}, /* 00,87,1b,cc */
	{0xa0, 0x1c, ZC3XX_R088_EXPTIMELOW}, /* 00,88,1c,cc */
	{0xa0, 0xee, ZC3XX_R08B_I2CDEVICEADDR}, /* 00,8b,ee,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING}, /* 00,08,03,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,01,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH}, /* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW}, /* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH}, /* 00,05,01,cc */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW}, /* 00,06,e0,cc */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE}, /* 01,80,42,cc */
	{0xaa, 0x8d, 0x0008},			/* 00,8d,08,aa */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},	/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},	/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},	/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},	/* 01,1c,00,cc */
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},	/* 00,9c,e6,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},	/* 00,9e,86,cc */
	{0xaa, 0x09, 0x00ce}, /* 00,09,ce,aa */
	{0xaa, 0x0b, 0x0005}, /* 00,0b,05,aa */
	{0xaa, 0x0d, 0x0054}, /* 00,0d,54,aa */
	{0xaa, 0x0f, 0x00eb}, /* 00,0f,eb,aa */
	{0xaa, 0x87, 0x0000}, /* 00,87,00,aa */
	{0xaa, 0x88, 0x0004}, /* 00,88,04,aa */
	{0xaa, 0x89, 0x0000}, /* 00,89,00,aa */
	{0xaa, 0x8a, 0x0005}, /* 00,8a,05,aa */
	{0xaa, 0x13, 0x0003}, /* 00,13,03,aa */
	{0xaa, 0x16, 0x0040}, /* 00,16,40,aa */
	{0xaa, 0x18, 0x0040}, /* 00,18,40,aa */
	{0xaa, 0x1d, 0x0002}, /* 00,1d,02,aa */
	{0xaa, 0x29, 0x00e8}, /* 00,29,e8,aa */
	{0xaa, 0x45, 0x0045}, /* 00,45,45,aa */
	{0xaa, 0x50, 0x00ed}, /* 00,50,ed,aa */
	{0xaa, 0x51, 0x0025}, /* 00,51,25,aa */
	{0xaa, 0x52, 0x0042}, /* 00,52,42,aa */
	{0xaa, 0x53, 0x002f}, /* 00,53,2f,aa */
	{0xaa, 0x79, 0x0025}, /* 00,79,25,aa */
	{0xaa, 0x7b, 0x0000}, /* 00,7b,00,aa */
	{0xaa, 0x7e, 0x0025}, /* 00,7e,25,aa */
	{0xaa, 0x7f, 0x0025}, /* 00,7f,25,aa */
	{0xaa, 0x21, 0x0000}, /* 00,21,00,aa */
	{0xaa, 0x33, 0x0036}, /* 00,33,36,aa */
	{0xaa, 0x36, 0x0060}, /* 00,36,60,aa */
	{0xaa, 0x37, 0x0008}, /* 00,37,08,aa */
	{0xaa, 0x3b, 0x0031}, /* 00,3b,31,aa */
	{0xaa, 0x44, 0x000f}, /* 00,44,0f,aa */
	{0xaa, 0x58, 0x0002}, /* 00,58,02,aa */
	{0xaa, 0x66, 0x00c0}, /* 00,66,c0,aa */
	{0xaa, 0x67, 0x0044}, /* 00,67,44,aa */
	{0xaa, 0x6b, 0x00a0}, /* 00,6b,a0,aa */
	{0xaa, 0x6c, 0x0054}, /* 00,6c,54,aa */
	{0xaa, 0xd6, 0x0007}, /* 00,d6,07,aa */
	{0xa0, 0xf7, ZC3XX_R101_SENSORCORRECTION}, /* 01,01,f7,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE}, /* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS}, /* 01,89,06,cc */
	{0xa0, 0x00, 0x01ad}, /* 01,ad,00,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE}, /* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05}, /* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE}, /* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS}, /* 03,01,08,cc */
	{0xa0, 0x7a, ZC3XX_R116_RGAIN}, /* 01,16,7a,cc */
	{0xa0, 0x4a, ZC3XX_R118_BGAIN}, /* 01,18,4a,cc */
	{}
};

/* from oem9.inf - HKR,%PO2030%,InitialScale - 320x240 */
static const struct usb_action PO2030_mode1[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL}, /* 00,00,01,cc */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT}, /* 00,02,10,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT}, /* 00,10,01,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING}, /* 00,01,01,cc */
	{0xa0, 0x04, ZC3XX_R080_HBLANKHIGH}, /* 00,80,04,cc */
	{0xa0, 0x05, ZC3XX_R081_HBLANKLOW}, /* 00,81,05,cc */
	{0xa0, 0x16, ZC3XX_R083_RGAINADDR}, /* 00,83,16,cc */
	{0xa0, 0x18, ZC3XX_R085_BGAINADDR}, /* 00,85,18,cc */
	{0xa0, 0x1a, ZC3XX_R086_EXPTIMEHIGH}, /* 00,86,1a,cc */
	{0xa0, 0x1b, ZC3XX_R087_EXPTIMEMID}, /* 00,87,1b,cc */
	{0xa0, 0x1c, ZC3XX_R088_EXPTIMELOW}, /* 00,88,1c,cc */
	{0xa0, 0xee, ZC3XX_R08B_I2CDEVICEADDR}, /* 00,8b,ee,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING}, /* 00,08,03,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,01,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH}, /* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW}, /* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH}, /* 00,05,01,cc */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW}, /* 00,06,e0,cc */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE}, /* 01,80,42,cc */
	{0xaa, 0x8d, 0x0008},			/* 00,8d,08,aa */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW}, /* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW}, /* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW}, /* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW}, /* 01,1c,00,cc */
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW}, /* 00,9c,e8,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW}, /* 00,9e,88,cc */
	{0xaa, 0x09, 0x00cc}, /* 00,09,cc,aa */
	{0xaa, 0x0b, 0x0005}, /* 00,0b,05,aa */
	{0xaa, 0x0d, 0x0058}, /* 00,0d,58,aa */
	{0xaa, 0x0f, 0x00ed}, /* 00,0f,ed,aa */
	{0xaa, 0x87, 0x0000}, /* 00,87,00,aa */
	{0xaa, 0x88, 0x0004}, /* 00,88,04,aa */
	{0xaa, 0x89, 0x0000}, /* 00,89,00,aa */
	{0xaa, 0x8a, 0x0005}, /* 00,8a,05,aa */
	{0xaa, 0x13, 0x0003}, /* 00,13,03,aa */
	{0xaa, 0x16, 0x0040}, /* 00,16,40,aa */
	{0xaa, 0x18, 0x0040}, /* 00,18,40,aa */
	{0xaa, 0x1d, 0x0002}, /* 00,1d,02,aa */
	{0xaa, 0x29, 0x00e8}, /* 00,29,e8,aa */
	{0xaa, 0x45, 0x0045}, /* 00,45,45,aa */
	{0xaa, 0x50, 0x00ed}, /* 00,50,ed,aa */
	{0xaa, 0x51, 0x0025}, /* 00,51,25,aa */
	{0xaa, 0x52, 0x0042}, /* 00,52,42,aa */
	{0xaa, 0x53, 0x002f}, /* 00,53,2f,aa */
	{0xaa, 0x79, 0x0025}, /* 00,79,25,aa */
	{0xaa, 0x7b, 0x0000}, /* 00,7b,00,aa */
	{0xaa, 0x7e, 0x0025}, /* 00,7e,25,aa */
	{0xaa, 0x7f, 0x0025}, /* 00,7f,25,aa */
	{0xaa, 0x21, 0x0000}, /* 00,21,00,aa */
	{0xaa, 0x33, 0x0036}, /* 00,33,36,aa */
	{0xaa, 0x36, 0x0060}, /* 00,36,60,aa */
	{0xaa, 0x37, 0x0008}, /* 00,37,08,aa */
	{0xaa, 0x3b, 0x0031}, /* 00,3b,31,aa */
	{0xaa, 0x44, 0x000f}, /* 00,44,0f,aa */
	{0xaa, 0x58, 0x0002}, /* 00,58,02,aa */
	{0xaa, 0x66, 0x00c0}, /* 00,66,c0,aa */
	{0xaa, 0x67, 0x0044}, /* 00,67,44,aa */
	{0xaa, 0x6b, 0x00a0}, /* 00,6b,a0,aa */
	{0xaa, 0x6c, 0x0054}, /* 00,6c,54,aa */
	{0xaa, 0xd6, 0x0007}, /* 00,d6,07,aa */
	{0xa0, 0xf7, ZC3XX_R101_SENSORCORRECTION}, /* 01,01,f7,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC}, /* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE}, /* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS}, /* 01,89,06,cc */
	{0xa0, 0x00, 0x01ad}, /* 01,ad,00,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE}, /* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05}, /* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE}, /* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS}, /* 03,01,08,cc */
	{0xa0, 0x7a, ZC3XX_R116_RGAIN}, /* 01,16,7a,cc */
	{0xa0, 0x4a, ZC3XX_R118_BGAIN}, /* 01,18,4a,cc */
	{}
};

static const struct usb_action PO2030_50HZ[] = {
	{0xaa, 0x8d, 0x0008}, /* 00,8d,08,aa */
	{0xaa, 0x1a, 0x0001}, /* 00,1a,01,aa */
	{0xaa, 0x1b, 0x000a}, /* 00,1b,0a,aa */
	{0xaa, 0x1c, 0x00b0}, /* 00,1c,b0,aa */
	{0xa0, 0x05, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,05,cc */
	{0xa0, 0x35, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,35,cc */
	{0xa0, 0x70, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,70,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x85, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,85,cc */
	{0xa0, 0x58, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,58,cc */
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE}, /* 01,8c,0c,cc */
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,18,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN}, /* 01,a8,60,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x22, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,22,cc */
	{0xa0, 0x88, ZC3XX_R18D_YTARGET}, /* 01,8d,88,cc */
	{0xa0, 0x58, ZC3XX_R11D_GLOBALGAIN}, /* 01,1d,58,cc */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE}, /* 01,80,42,cc */
	{}
};

static const struct usb_action PO2030_60HZ[] = {
	{0xaa, 0x8d, 0x0008}, /* 00,8d,08,aa */
	{0xaa, 0x1a, 0x0000}, /* 00,1a,00,aa */
	{0xaa, 0x1b, 0x00de}, /* 00,1b,de,aa */
	{0xaa, 0x1c, 0x0040}, /* 00,1c,40,aa */
	{0xa0, 0x08, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,08,cc */
	{0xa0, 0xae, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,ae,cc */
	{0xa0, 0x80, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,80,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x6f, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,6f,cc */
	{0xa0, 0x20, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,20,cc */
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE}, /* 01,8c,0c,cc */
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,18,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN}, /* 01,a8,60,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x22, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,22,cc */
	{0xa0, 0x88, ZC3XX_R18D_YTARGET},		/* 01,8d,88,cc */
							/* win: 01,8d,80 */
	{0xa0, 0x58, ZC3XX_R11D_GLOBALGAIN},		/* 01,1d,58,cc */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc */
	{}
};

static const struct usb_action PO2030_NoFliker[] = {
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE}, /* 01,80,02,cc */
	{0xaa, 0x8d, 0x000d}, /* 00,8d,0d,aa */
	{0xaa, 0x1a, 0x0000}, /* 00,1a,00,aa */
	{0xaa, 0x1b, 0x0002}, /* 00,1b,02,aa */
	{0xaa, 0x1c, 0x0078}, /* 00,1c,78,aa */
	{0xaa, 0x46, 0x0000}, /* 00,46,00,aa */
	{0xaa, 0x15, 0x0000}, /* 00,15,00,aa */
	{}
};

/* TEST */
static const struct usb_action tas5130CK_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x01, 0x003b},
	{0xa0, 0x0e, 0x003a},
	{0xa0, 0x01, 0x0038},
	{0xa0, 0x0b, 0x0039},
	{0xa0, 0x00, 0x0038},
	{0xa0, 0x0b, 0x0039},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0xdc, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xdc, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x08, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x83, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x04, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x08, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x06, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x02, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x11, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xE7, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x01, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x04, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x87, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x02, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x07, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x30, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x51, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x35, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7F, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x30, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x05, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x31, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x58, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x78, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x62, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x11, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x04, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2B, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7f, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2c, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7f, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2D, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7f, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7f, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x10, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x6c, ZC3XX_R18D_YTARGET},
	{0xa0, 0x61, ZC3XX_R116_RGAIN},
	{0xa0, 0x65, ZC3XX_R118_BGAIN},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x4c, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf1, ZC3XX_R10B_RGB01},
	{0xa0, 0x03, ZC3XX_R10C_RGB02},
	{0xa0, 0xfe, ZC3XX_R10D_RGB10},
	{0xa0, 0x51, ZC3XX_R10E_RGB11},
	{0xa0, 0xf1, ZC3XX_R10F_RGB12},
	{0xa0, 0xec, ZC3XX_R110_RGB20},
	{0xa0, 0x03, ZC3XX_R111_RGB21},
	{0xa0, 0x51, ZC3XX_R112_RGB22},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x38, ZC3XX_R120_GAMMA00},	/* gamma > 5 */
	{0xa0, 0x51, ZC3XX_R121_GAMMA01},
	{0xa0, 0x6e, ZC3XX_R122_GAMMA02},
	{0xa0, 0x8c, ZC3XX_R123_GAMMA03},
	{0xa0, 0xa2, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb6, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc8, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd6, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe2, ZC3XX_R128_GAMMA08},
	{0xa0, 0xed, ZC3XX_R129_GAMMA09},
	{0xa0, 0xf5, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfc, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xff, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x12, ZC3XX_R130_GAMMA10},
	{0xa0, 0x1b, ZC3XX_R131_GAMMA11},
	{0xa0, 0x1d, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1a, ZC3XX_R133_GAMMA13},
	{0xa0, 0x15, ZC3XX_R134_GAMMA14},
	{0xa0, 0x12, ZC3XX_R135_GAMMA15},
	{0xa0, 0x0f, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x05, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x4c, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf1, ZC3XX_R10B_RGB01},
	{0xa0, 0x03, ZC3XX_R10C_RGB02},
	{0xa0, 0xfe, ZC3XX_R10D_RGB10},
	{0xa0, 0x51, ZC3XX_R10E_RGB11},
	{0xa0, 0xf1, ZC3XX_R10F_RGB12},
	{0xa0, 0xec, ZC3XX_R110_RGB20},
	{0xa0, 0x03, ZC3XX_R111_RGB21},
	{0xa0, 0x51, ZC3XX_R112_RGB22},
	{0xa0, 0x10, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x09, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x34, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x01, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xd2, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x9a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x1c, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x66, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xd7, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xf4, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf9, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action tas5130CK_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x01, 0x003b},
	{0xa0, 0x0e, 0x003a},
	{0xa0, 0x01, 0x0038},
	{0xa0, 0x0b, 0x0039},
	{0xa0, 0x00, 0x0038},
	{0xa0, 0x0b, 0x0039},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0xdc, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xdc, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x08, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x83, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x04, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x08, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x06, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x02, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x11, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xe5, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x01, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x04, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x85, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x02, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x07, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x30, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x51, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x35, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7F, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x50, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x30, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x05, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x31, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x58, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x78, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x62, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x11, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x04, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2B, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7f, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2C, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7F, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2D, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7f, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7f, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x10, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x6c, ZC3XX_R18D_YTARGET},
	{0xa0, 0x61, ZC3XX_R116_RGAIN},
	{0xa0, 0x65, ZC3XX_R118_BGAIN},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x4c, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf1, ZC3XX_R10B_RGB01},
	{0xa0, 0x03, ZC3XX_R10C_RGB02},
	{0xa0, 0xfe, ZC3XX_R10D_RGB10},
	{0xa0, 0x51, ZC3XX_R10E_RGB11},
	{0xa0, 0xf1, ZC3XX_R10F_RGB12},
	{0xa0, 0xec, ZC3XX_R110_RGB20},
	{0xa0, 0x03, ZC3XX_R111_RGB21},
	{0xa0, 0x51, ZC3XX_R112_RGB22},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x38, ZC3XX_R120_GAMMA00},	/* gamma > 5 */
	{0xa0, 0x51, ZC3XX_R121_GAMMA01},
	{0xa0, 0x6e, ZC3XX_R122_GAMMA02},
	{0xa0, 0x8c, ZC3XX_R123_GAMMA03},
	{0xa0, 0xa2, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb6, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc8, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd6, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe2, ZC3XX_R128_GAMMA08},
	{0xa0, 0xed, ZC3XX_R129_GAMMA09},
	{0xa0, 0xf5, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfc, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xff, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x12, ZC3XX_R130_GAMMA10},
	{0xa0, 0x1b, ZC3XX_R131_GAMMA11},
	{0xa0, 0x1d, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1a, ZC3XX_R133_GAMMA13},
	{0xa0, 0x15, ZC3XX_R134_GAMMA14},
	{0xa0, 0x12, ZC3XX_R135_GAMMA15},
	{0xa0, 0x0f, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x05, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x4c, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf1, ZC3XX_R10B_RGB01},
	{0xa0, 0x03, ZC3XX_R10C_RGB02},
	{0xa0, 0xfe, ZC3XX_R10D_RGB10},
	{0xa0, 0x51, ZC3XX_R10E_RGB11},
	{0xa0, 0xf1, ZC3XX_R10F_RGB12},
	{0xa0, 0xec, ZC3XX_R110_RGB20},
	{0xa0, 0x03, ZC3XX_R111_RGB21},
	{0xa0, 0x51, ZC3XX_R112_RGB22},
	{0xa0, 0x10, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x62, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xaa, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x01, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x03, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x9b, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x47, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x1c, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x14, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x66, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x09, 0x01ad},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x30, 0x0007},
	{0xa0, 0x02, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x00, 0x0007},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{}
};

static const struct usb_action tas5130cxx_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x50, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x02, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x00, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R0A5_EXPOSUREGAIN},
	{0xa0, 0x02, ZC3XX_R0A6_EXPOSUREBLACKLVL},

	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},

	{0xa0, 0x04, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x0f, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x04, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x0f, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x06, ZC3XX_R08D_COMPABILITYMODE},
	{0xa0, 0xf7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x68, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xec, ZC3XX_R10B_RGB01},
	{0xa0, 0xec, ZC3XX_R10C_RGB02},
	{0xa0, 0xec, ZC3XX_R10D_RGB10},
	{0xa0, 0x68, ZC3XX_R10E_RGB11},
	{0xa0, 0xec, ZC3XX_R10F_RGB12},
	{0xa0, 0xec, ZC3XX_R110_RGB20},
	{0xa0, 0xec, ZC3XX_R111_RGB21},
	{0xa0, 0x68, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x018d},
	{0xa0, 0x90, ZC3XX_R18D_YTARGET},	/* 90 */
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},

	{0xaa, 0xa3, 0x0001},
	{0xaa, 0xa4, 0x0077},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x77, ZC3XX_R0A4_EXPOSURETIMELOW},

	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 00 */
	{0xa0, 0x03, ZC3XX_R191_EXPOSURELIMITMID},	/* 03 */
	{0xa0, 0xe8, ZC3XX_R192_EXPOSURELIMITLOW},	/* e8 */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 0 */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 0 */
	{0xa0, 0x7d, ZC3XX_R197_ANTIFLICKERLOW},	/* 7d */

	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x08, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 08 */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 24 */
	{0xa0, 0xf0, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xf4, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf8, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x03, ZC3XX_R09F_MAXXHIGH},
	{0xa0, 0xc0, ZC3XX_R0A0_MAXXLOW},
	{0xa0, 0x50, ZC3XX_R11D_GLOBALGAIN},	/* 50 */
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};
static const struct usb_action tas5130cxx_InitialScale[] = {
/*??	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL}, */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x40, ZC3XX_R002_CLOCKSELECT},

	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa1, 0x01, 0x0008},

	{0xa0, 0x02, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x00, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R0A5_EXPOSUREGAIN},
	{0xa0, 0x02, ZC3XX_R0A6_EXPOSUREBLACKLVL},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x05, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x0f, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x05, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x0f, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x06, ZC3XX_R08D_COMPABILITYMODE},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},

	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa1, 0x01, 0x0008},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x68, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xec, ZC3XX_R10B_RGB01},
	{0xa0, 0xec, ZC3XX_R10C_RGB02},
	{0xa0, 0xec, ZC3XX_R10D_RGB10},
	{0xa0, 0x68, ZC3XX_R10E_RGB11},
	{0xa0, 0xec, ZC3XX_R10F_RGB12},
	{0xa0, 0xec, ZC3XX_R110_RGB20},
	{0xa0, 0xec, ZC3XX_R111_RGB21},
	{0xa0, 0x68, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x018d},
	{0xa0, 0x90, ZC3XX_R18D_YTARGET},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0xa3, 0x0001},
	{0xaa, 0xa4, 0x0063},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x63, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x38, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x47, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x08, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xd3, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xda, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xea, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x03, ZC3XX_R09F_MAXXHIGH},
	{0xa0, 0x4c, ZC3XX_R0A0_MAXXLOW},
	{0xa0, 0x50, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};
static const struct usb_action tas5130cxx_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0xa3, 0x0001}, /* 00,a3,01,aa */
	{0xaa, 0xa4, 0x0063}, /* 00,a4,63,aa */
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,01,cc */
	{0xa0, 0x63, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,63,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x38, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,38,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x47, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,47,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,26,cc */
	{0xa0, 0xd3, ZC3XX_R01D_HSYNC_0}, /* 00,1d,d3,cc */
	{0xa0, 0xda, ZC3XX_R01E_HSYNC_1}, /* 00,1e,da,cc */
	{0xa0, 0xea, ZC3XX_R01F_HSYNC_2}, /* 00,1f,ea,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x03, ZC3XX_R09F_MAXXHIGH}, /* 00,9f,03,cc */
	{}
};
static const struct usb_action tas5130cxx_50HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0xa3, 0x0001}, /* 00,a3,01,aa */
	{0xaa, 0xa4, 0x0077}, /* 00,a4,77,aa */
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,01,cc */
	{0xa0, 0x77, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,77,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x03, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,03,cc */
	{0xa0, 0xe8, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,e8,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x7d, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,7d,cc */
	{0xa0, 0x14, ZC3XX_R18C_AEFREEZE}, /* 01,8c,14,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,26,cc */
	{0xa0, 0xf0, ZC3XX_R01D_HSYNC_0}, /* 00,1d,f0,cc */
	{0xa0, 0xf4, ZC3XX_R01E_HSYNC_1}, /* 00,1e,f4,cc */
	{0xa0, 0xf8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,f8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x03, ZC3XX_R09F_MAXXHIGH}, /* 00,9f,03,cc */
	{}
};
static const struct usb_action tas5130cxx_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0xa3, 0x0001}, /* 00,a3,01,aa */
	{0xaa, 0xa4, 0x0036}, /* 00,a4,36,aa */
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,01,cc */
	{0xa0, 0x36, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,36,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x01, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,01,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x3e, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,3e,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,26,cc */
	{0xa0, 0xca, ZC3XX_R01D_HSYNC_0}, /* 00,1d,ca,cc */
	{0xa0, 0xd0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d0,cc */
	{0xa0, 0xe0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e0,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x03, ZC3XX_R09F_MAXXHIGH}, /* 00,9f,03,cc */
	{}
};
static const struct usb_action tas5130cxx_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0xa3, 0x0001}, /* 00,a3,01,aa */
	{0xaa, 0xa4, 0x0077}, /* 00,a4,77,aa */
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,01,cc */
	{0xa0, 0x77, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,77,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x03, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,03,cc */
	{0xa0, 0xe8, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,e8,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x7d, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,7d,cc */
	{0xa0, 0x14, ZC3XX_R18C_AEFREEZE}, /* 01,8c,14,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x26, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,26,cc */
	{0xa0, 0xc8, ZC3XX_R01D_HSYNC_0}, /* 00,1d,c8,cc */
	{0xa0, 0xd0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d0,cc */
	{0xa0, 0xe0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e0,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x03, ZC3XX_R09F_MAXXHIGH}, /* 00,9f,03,cc */
	{}
};
static const struct usb_action tas5130cxx_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0xa3, 0x0001}, /* 00,a3,01,aa */
	{0xaa, 0xa4, 0x0040}, /* 00,a4,40,aa */
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,01,cc */
	{0xa0, 0x40, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,40,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x01, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,01,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0xbc, ZC3XX_R01D_HSYNC_0}, /* 00,1d,bc,cc */
	{0xa0, 0xd0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d0,cc */
	{0xa0, 0xe0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e0,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x02, ZC3XX_R09F_MAXXHIGH}, /* 00,9f,02,cc */
	{}
};

static const struct usb_action tas5130cxx_NoFlikerScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0xa3, 0x0001}, /* 00,a3,01,aa */
	{0xaa, 0xa4, 0x0090}, /* 00,a4,90,aa */
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH}, /* 00,a3,01,cc */
	{0xa0, 0x90, ZC3XX_R0A4_EXPOSURETIMELOW}, /* 00,a4,90,cc */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x03, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,03,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0xbc, ZC3XX_R01D_HSYNC_0}, /* 00,1d,bc,cc */
	{0xa0, 0xd0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d0,cc */
	{0xa0, 0xe0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e0,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3}, /* 00,20,ff,cc */
	{0xa0, 0x02, ZC3XX_R09F_MAXXHIGH}, /* 00,9f,02,cc */
	{}
};

static const struct usb_action tas5130c_vf0250_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc, */
	{0xa0, 0x02, ZC3XX_R008_CLOCKSETTING},		/* 00,08,02,cc, */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc, */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},		/* 00,02,00,cc,
							 * 0<->10 */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc, */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc, */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc, */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc, */
	{0xa0, 0x98, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,98,cc, */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc, */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc, */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc, */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,00,cc, */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc, */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc, */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc, */
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,e6,cc,
							 * 6<->8 */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,86,cc,
							 * 6<->8 */
	{0xa0, 0x10, ZC3XX_R087_EXPTIMEMID},		/* 00,87,10,cc, */
	{0xa0, 0x98, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,98,cc, */
	{0xaa, 0x1b, 0x0024},		/* 00,1b,24,aa, */
	{0xdd, 0x00, 0x0080},		/* 00,00,80,dd, */
	{0xaa, 0x1b, 0x0000},		/* 00,1b,00,aa, */
	{0xaa, 0x13, 0x0002},		/* 00,13,02,aa, */
	{0xaa, 0x15, 0x0004},		/* 00,15,04,aa */
/*??	{0xaa, 0x01, 0x0000}, */
	{0xaa, 0x01, 0x0000},
	{0xaa, 0x1a, 0x0000},		/* 00,1a,00,aa, */
	{0xaa, 0x1c, 0x0017},		/* 00,1c,17,aa, */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},		/* 00,86,82,cc, */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},		/* 00,87,83,cc, */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},		/* 00,88,84,cc, */
	{0xaa, 0x05, 0x0010},		/* 00,05,10,aa, */
	{0xaa, 0x0a, 0x0000},		/* 00,0a,00,aa, */
	{0xaa, 0x0b, 0x00a0},		/* 00,0b,a0,aa, */
	{0xaa, 0x0c, 0x0000},		/* 00,0c,00,aa, */
	{0xaa, 0x0d, 0x00a0},		/* 00,0d,a0,aa, */
	{0xaa, 0x0e, 0x0000},		/* 00,0e,00,aa, */
	{0xaa, 0x0f, 0x00a0},		/* 00,0f,a0,aa, */
	{0xaa, 0x10, 0x0000},		/* 00,10,00,aa, */
	{0xaa, 0x11, 0x00a0},		/* 00,11,a0,aa, */
/*??	{0xa0, 0x00, 0x0039},
	{0xa1, 0x01, 0x0037}, */
	{0xaa, 0x16, 0x0001},		/* 00,16,01,aa, */
	{0xaa, 0x17, 0x00e8},		/* 00,17,e6,aa, (e6 -> e8) */
	{0xaa, 0x18, 0x0002},		/* 00,18,02,aa, */
	{0xaa, 0x19, 0x0088},		/* 00,19,86,aa, */
	{0xaa, 0x20, 0x0020},		/* 00,20,20,aa, */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b7,cc, */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc, */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc, */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},		/* 01,89,76,cc, */
	{0xa0, 0x09, 0x01ad},				/* 01,ad,09,cc, */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc, */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc, */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc, */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc, */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},		/* 01,a8,60,cc, */
	{0xa0, 0x61, ZC3XX_R116_RGAIN},			/* 01,16,61,cc, */
	{0xa0, 0x65, ZC3XX_R118_BGAIN},			/* 01,18,65,cc */
	{}
};

static const struct usb_action tas5130c_vf0250_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc, */
	{0xa0, 0x02, ZC3XX_R008_CLOCKSETTING},		/* 00,08,02,cc, */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc, */
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},		/* 00,02,10,cc, */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc, */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc, */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc, */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc, */
	{0xa0, 0x98, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,98,cc, */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc, */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc, */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc, */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,00,cc, */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc, */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc, */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc, */
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,e8,cc,
							 * 8<->6 */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,88,cc,
							 * 8<->6 */
	{0xa0, 0x10, ZC3XX_R087_EXPTIMEMID},		/* 00,87,10,cc, */
	{0xa0, 0x98, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,98,cc, */
	{0xaa, 0x1b, 0x0024},		/* 00,1b,24,aa, */
	{0xdd, 0x00, 0x0080},		/* 00,00,80,dd, */
	{0xaa, 0x1b, 0x0000},		/* 00,1b,00,aa, */
	{0xaa, 0x13, 0x0002},		/* 00,13,02,aa, */
	{0xaa, 0x15, 0x0004},		/* 00,15,04,aa */
/*??	{0xaa, 0x01, 0x0000}, */
	{0xaa, 0x01, 0x0000},
	{0xaa, 0x1a, 0x0000},		/* 00,1a,00,aa, */
	{0xaa, 0x1c, 0x0017},		/* 00,1c,17,aa, */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/* 00,86,82,cc, */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc, */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc, */
	{0xaa, 0x05, 0x0010},		/* 00,05,10,aa, */
	{0xaa, 0x0a, 0x0000},		/* 00,0a,00,aa, */
	{0xaa, 0x0b, 0x00a0},		/* 00,0b,a0,aa, */
	{0xaa, 0x0c, 0x0000},		/* 00,0c,00,aa, */
	{0xaa, 0x0d, 0x00a0},		/* 00,0d,a0,aa, */
	{0xaa, 0x0e, 0x0000},		/* 00,0e,00,aa, */
	{0xaa, 0x0f, 0x00a0},		/* 00,0f,a0,aa, */
	{0xaa, 0x10, 0x0000},		/* 00,10,00,aa, */
	{0xaa, 0x11, 0x00a0},		/* 00,11,a0,aa, */
/*??	{0xa0, 0x00, 0x0039},
	{0xa1, 0x01, 0x0037}, */
	{0xaa, 0x16, 0x0001},		/* 00,16,01,aa, */
	{0xaa, 0x17, 0x00e8},		/* 00,17,e6,aa (e6 -> e8) */
	{0xaa, 0x18, 0x0002},		/* 00,18,02,aa, */
	{0xaa, 0x19, 0x0088},		/* 00,19,88,aa, */
	{0xaa, 0x20, 0x0020},		/* 00,20,20,aa, */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b7,cc, */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc, */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc, */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},		/* 01,89,76,cc, */
	{0xa0, 0x09, 0x01ad},				/* 01,ad,09,cc, */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc, */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc, */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc, */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc, */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},		/* 01,a8,60,cc, */
	{0xa0, 0x61, ZC3XX_R116_RGAIN},		/* 01,16,61,cc, */
	{0xa0, 0x65, ZC3XX_R118_BGAIN},		/* 01,18,65,cc */
	{}
};
/* "50HZ" light frequency banding filter */
static const struct usb_action tas5130c_vf0250_50HZ[] = {
	{0xaa, 0x82, 0x0000},		/* 00,82,00,aa */
	{0xaa, 0x83, 0x0001},		/* 00,83,01,aa */
	{0xaa, 0x84, 0x00aa},		/* 00,84,aa,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc, */
	{0xa0, 0x06, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0d,cc, */
	{0xa0, 0xa8, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,50,cc, */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc, */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc, */
	{0xa0, 0x8e, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,47,cc, */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},		/* 01,8c,0e,cc, */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,15,cc, */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc, */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc, */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},		/* 00,1d,62,cc, */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},		/* 00,1e,90,cc, */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},		/* 00,1f,c8,cc, */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc, */
	{0xa0, 0x58, ZC3XX_R11D_GLOBALGAIN},		/* 01,1d,58,cc, */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc, */
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},		/* 01,8d,78,cc */
	{}
};

/* "50HZScale" light frequency banding filter */
static const struct usb_action tas5130c_vf0250_50HZScale[] = {
	{0xaa, 0x82, 0x0000},		/* 00,82,00,aa */
	{0xaa, 0x83, 0x0003},		/* 00,83,03,aa */
	{0xaa, 0x84, 0x0054},		/* 00,84,54,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc, */
	{0xa0, 0x0d, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0d,cc, */
	{0xa0, 0x50, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,50,cc, */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc, */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc, */
	{0xa0, 0x8e, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,8e,cc, */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},		/* 01,8c,0e,cc, */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,15,cc, */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc, */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc, */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},		/* 00,1d,62,cc, */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},		/* 00,1e,90,cc, */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},		/* 00,1f,c8,cc, */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc, */
	{0xa0, 0x58, ZC3XX_R11D_GLOBALGAIN},		/* 01,1d,58,cc, */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc, */
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},		/* 01,8d,78,cc */
	{}
};

/* "60HZ" light frequency banding filter */
static const struct usb_action tas5130c_vf0250_60HZ[] = {
	{0xaa, 0x82, 0x0000},		/* 00,82,00,aa */
	{0xaa, 0x83, 0x0001},		/* 00,83,01,aa */
	{0xaa, 0x84, 0x0062},		/* 00,84,62,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc, */
	{0xa0, 0x05, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,05,cc, */
	{0xa0, 0x88, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,88,cc, */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc, */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc, */
	{0xa0, 0x3b, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,3b,cc, */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},		/* 01,8c,0e,cc, */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,15,cc, */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc, */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc, */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},		/* 00,1d,62,cc, */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},		/* 00,1e,90,cc, */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},		/* 00,1f,c8,cc, */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc, */
	{0xa0, 0x58, ZC3XX_R11D_GLOBALGAIN},		/* 01,1d,58,cc, */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc, */
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},		/* 01,8d,78,cc */
	{}
};

/* "60HZScale" light frequency banding ilter */
static const struct usb_action tas5130c_vf0250_60HZScale[] = {
	{0xaa, 0x82, 0x0000},		/* 00,82,00,aa */
	{0xaa, 0x83, 0x0002},		/* 00,83,02,aa */
	{0xaa, 0x84, 0x00c4},		/* 00,84,c4,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc, */
	{0xa0, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,1,0b,cc, */
	{0xa0, 0x10, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,2,10,cc, */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,5,00,cc, */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,6,00,cc, */
	{0xa0, 0x76, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,7,76,cc, */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},		/* 01,c,0e,cc, */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},		/* 01,f,15,cc, */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,9,10,cc, */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,a,24,cc, */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},		/* 00,d,62,cc, */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},		/* 00,e,90,cc, */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},		/* 00,f,c8,cc, */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,0,ff,cc, */
	{0xa0, 0x58, ZC3XX_R11D_GLOBALGAIN},		/* 01,d,58,cc, */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc, */
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},		/* 01,d,78,cc */
	{}
};

/* "NoFliker" light frequency banding flter */
static const struct usb_action tas5130c_vf0250_NoFliker[] = {
	{0xa0, 0x0c, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0c,cc, */
	{0xaa, 0x82, 0x0000},		/* 00,82,00,aa */
	{0xaa, 0x83, 0x0000},		/* 00,83,00,aa */
	{0xaa, 0x84, 0x0020},		/* 00,84,20,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,0,00,cc, */
	{0xa0, 0x05, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,05,cc, */
	{0xa0, 0x88, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,88,cc, */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc, */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc, */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,10,cc, */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},		/* 01,8c,0e,cc, */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,15,cc, */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},		/* 00,1d,62,cc, */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},		/* 00,1e,90,cc, */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},		/* 00,1f,c8,cc, */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc, */
	{0xa0, 0x58, ZC3XX_R11D_GLOBALGAIN},		/* 01,1d,58,cc, */
	{0xa0, 0x03, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,03,cc */
	{}
};

/* "NoFlikerScale" light frequency banding filter */
static const struct usb_action tas5130c_vf0250_NoFlikerScale[] = {
	{0xa0, 0x0c, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0c,cc, */
	{0xaa, 0x82, 0x0000},		/* 00,82,00,aa */
	{0xaa, 0x83, 0x0000},		/* 00,83,00,aa */
	{0xaa, 0x84, 0x0020},		/* 00,84,20,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc, */
	{0xa0, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0b,cc, */
	{0xa0, 0x10, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,10,cc, */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc, */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc, */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,10,cc, */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},		/* 01,8c,0e,cc, */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},		/* 01,8f,15,cc, */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},		/* 00,1d,62,cc, */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},		/* 00,1e,90,cc, */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},		/* 00,1f,c8,cc, */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},		/* 00,20,ff,cc, */
	{0xa0, 0x58, ZC3XX_R11D_GLOBALGAIN},		/* 01,1d,58,cc, */
	{0xa0, 0x03, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,03,cc */
	{}
};

static u8 reg_r_i(struct gspca_dev *gspca_dev,
		__u16 index)
{
	usb_control_msg(gspca_dev->dev,
			usb_rcvctrlpipe(gspca_dev->dev, 0),
			0xa1,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0x01,			/* value */
			index, gspca_dev->usb_buf, 1,
			500);
	return gspca_dev->usb_buf[0];
}

static u8 reg_r(struct gspca_dev *gspca_dev,
		__u16 index)
{
	u8 ret;

	ret = reg_r_i(gspca_dev, index);
	PDEBUG(D_USBI, "reg r [%04x] -> %02x", index, ret);
	return ret;
}

static void reg_w_i(struct usb_device *dev,
			__u8 value,
			__u16 index)
{
	usb_control_msg(dev,
			usb_sndctrlpipe(dev, 0),
			0xa0,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value, index, NULL, 0,
			500);
}

static void reg_w(struct usb_device *dev,
			__u8 value,
			__u16 index)
{
	PDEBUG(D_USBO, "reg w [%04x] = %02x", index, value);
	reg_w_i(dev, value, index);
}

static __u16 i2c_read(struct gspca_dev *gspca_dev,
			__u8 reg)
{
	__u8 retbyte;
	__u16 retval;

	reg_w_i(gspca_dev->dev, reg, 0x0092);
	reg_w_i(gspca_dev->dev, 0x02, 0x0090);		/* <- read command */
	msleep(25);
	retbyte = reg_r_i(gspca_dev, 0x0091);		/* read status */
	retval = reg_r_i(gspca_dev, 0x0095);		/* read Lowbyte */
	retval |= reg_r_i(gspca_dev, 0x0096) << 8;	/* read Hightbyte */
	PDEBUG(D_USBI, "i2c r [%02x] -> %04x (%02x)",
			reg, retval, retbyte);
	return retval;
}

static __u8 i2c_write(struct gspca_dev *gspca_dev,
			__u8 reg,
			__u8 valL,
			__u8 valH)
{
	__u8 retbyte;

	reg_w_i(gspca_dev->dev, reg, 0x92);
	reg_w_i(gspca_dev->dev, valL, 0x93);
	reg_w_i(gspca_dev->dev, valH, 0x94);
	reg_w_i(gspca_dev->dev, 0x01, 0x90);		/* <- write command */
	msleep(15);
	retbyte = reg_r_i(gspca_dev, 0x0091);		/* read status */
	PDEBUG(D_USBO, "i2c w [%02x] = %02x%02x (%02x)",
			reg, valH, valL, retbyte);
	return retbyte;
}

static void usb_exchange(struct gspca_dev *gspca_dev,
			const struct usb_action *action)
{
	while (action->req) {
		switch (action->req) {
		case 0xa0:	/* write register */
			reg_w(gspca_dev->dev, action->val, action->idx);
			break;
		case 0xa1:	/* read status */
			reg_r(gspca_dev, action->idx);
			break;
		case 0xaa:
			i2c_write(gspca_dev,
				  action->val,			/* reg */
				  action->idx & 0xff,		/* valL */
				  action->idx >> 8);		/* valH */
			break;
		case 0xbb:
			i2c_write(gspca_dev,
				  action->idx >> 8,		/* reg */
				  action->idx & 0xff,		/* valL */
				  action->val);			/* valH */
			break;
		default:
/*		case 0xdd:	 * delay */
			msleep(action->val / 64 + 10);
			break;
		}
		action++;
/*		msleep(1); */
	}
}

static void setmatrix(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i;
	const __u8 *matrix;
	static const u8 adcm2700_matrix[9] =
/*		{0x66, 0xed, 0xed, 0xed, 0x66, 0xed, 0xed, 0xed, 0x66}; */
/*ms-win*/
		{0x74, 0xed, 0xed, 0xed, 0x74, 0xed, 0xed, 0xed, 0x74};
	static const __u8 gc0305_matrix[9] =
		{0x50, 0xf8, 0xf8, 0xf8, 0x50, 0xf8, 0xf8, 0xf8, 0x50};
	static const __u8 ov7620_matrix[9] =
		{0x58, 0xf4, 0xf4, 0xf4, 0x58, 0xf4, 0xf4, 0xf4, 0x58};
	static const __u8 pas202b_matrix[9] =
		{0x4c, 0xf5, 0xff, 0xf9, 0x51, 0xf5, 0xfb, 0xed, 0x5f};
	static const __u8 po2030_matrix[9] =
		{0x60, 0xf0, 0xf0, 0xf0, 0x60, 0xf0, 0xf0, 0xf0, 0x60};
	static const __u8 vf0250_matrix[9] =
		{0x7b, 0xea, 0xea, 0xea, 0x7b, 0xea, 0xea, 0xea, 0x7b};
	static const __u8 *matrix_tb[SENSOR_MAX] = {
		adcm2700_matrix, /* SENSOR_ADCM2700 0 */
		NULL,		/* SENSOR_CS2102 1 */
		NULL,		/* SENSOR_CS2102K 2 */
		gc0305_matrix,	/* SENSOR_GC0305 3 */
		NULL,		/* SENSOR_HDCS2020b 4 */
		NULL,		/* SENSOR_HV7131B 5 */
		NULL,		/* SENSOR_HV7131C 6 */
		NULL,		/* SENSOR_ICM105A 7 */
		NULL,		/* SENSOR_MC501CB 8 */
		ov7620_matrix,	/* SENSOR_OV7620 9 */
		NULL,		/* SENSOR_OV7630C 10 */
		NULL,		/* SENSOR_PAS106 11 */
		pas202b_matrix,	/* SENSOR_PAS202B 12 */
		NULL,		/* SENSOR_PB0330 13 */
		po2030_matrix,	/* SENSOR_PO2030 14 */
		NULL,		/* SENSOR_TAS5130CK 15 */
		NULL,		/* SENSOR_TAS5130CXX 16 */
		vf0250_matrix,	/* SENSOR_TAS5130C_VF0250 17 */
	};

	matrix = matrix_tb[sd->sensor];
	if (matrix == NULL)
		return;		/* matrix already loaded */
	for (i = 0; i < ARRAY_SIZE(ov7620_matrix); i++)
		reg_w(gspca_dev->dev, matrix[i], 0x010a + i);
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 brightness;

	switch (sd->sensor) {
	case SENSOR_GC0305:
	case SENSOR_OV7620:
	case SENSOR_PO2030:
		return;
	}
/*fixme: is it really write to 011d and 018d for all other sensors? */
	brightness = sd->brightness;
	reg_w(gspca_dev->dev, brightness, 0x011d);
	switch (sd->sensor) {
	case SENSOR_ADCM2700:
	case SENSOR_HV7131B:
		return;
	}
	if (brightness < 0x70)
		brightness += 0x10;
	else
		brightness = 0x80;
	reg_w(gspca_dev->dev, brightness, 0x018d);
}

static void setsharpness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	int sharpness;
	static const __u8 sharpness_tb[][2] = {
		{0x02, 0x03},
		{0x04, 0x07},
		{0x08, 0x0f},
		{0x10, 0x1e}
	};

	sharpness = sd->sharpness;
	reg_w(dev, sharpness_tb[sharpness][0], 0x01c6);
	reg_r(gspca_dev, 0x01c8);
	reg_r(gspca_dev, 0x01c9);
	reg_r(gspca_dev, 0x01ca);
	reg_w(dev, sharpness_tb[sharpness][1], 0x01cb);
}

static void setcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	const __u8 *Tgamma, *Tgradient;
	int g, i, k;
	static const __u8 kgamma_tb[16] =	/* delta for contrast */
		{0x15, 0x0d, 0x0a, 0x09, 0x08, 0x08, 0x08, 0x08,
		 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08};
	static const __u8 kgrad_tb[16] =
		{0x1b, 0x06, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x04};
	static const __u8 Tgamma_1[16] =
		{0x00, 0x00, 0x03, 0x0d, 0x1b, 0x2e, 0x45, 0x5f,
		 0x79, 0x93, 0xab, 0xc1, 0xd4, 0xe5, 0xf3, 0xff};
	static const __u8 Tgradient_1[16] =
		{0x00, 0x01, 0x05, 0x0b, 0x10, 0x15, 0x18, 0x1a,
		 0x1a, 0x18, 0x16, 0x14, 0x12, 0x0f, 0x0d, 0x06};
	static const __u8 Tgamma_2[16] =
		{0x01, 0x0c, 0x1f, 0x3a, 0x53, 0x6d, 0x85, 0x9c,
		 0xb0, 0xc2, 0xd1, 0xde, 0xe9, 0xf2, 0xf9, 0xff};
	static const __u8 Tgradient_2[16] =
		{0x05, 0x0f, 0x16, 0x1a, 0x19, 0x19, 0x17, 0x15,
		 0x12, 0x10, 0x0e, 0x0b, 0x09, 0x08, 0x06, 0x03};
	static const __u8 Tgamma_3[16] =
		{0x04, 0x16, 0x30, 0x4e, 0x68, 0x81, 0x98, 0xac,
		 0xbe, 0xcd, 0xda, 0xe4, 0xed, 0xf5, 0xfb, 0xff};
	static const __u8 Tgradient_3[16] =
		{0x0c, 0x16, 0x1b, 0x1c, 0x19, 0x18, 0x15, 0x12,
		 0x10, 0x0d, 0x0b, 0x09, 0x08, 0x06, 0x05, 0x03};
	static const __u8 Tgamma_4[16] =
		{0x13, 0x38, 0x59, 0x79, 0x92, 0xa7, 0xb9, 0xc8,
		 0xd4, 0xdf, 0xe7, 0xee, 0xf4, 0xf9, 0xfc, 0xff};
	static const __u8 Tgradient_4[16] =
		{0x26, 0x22, 0x20, 0x1c, 0x16, 0x13, 0x10, 0x0d,
		 0x0b, 0x09, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02};
	static const __u8 Tgamma_5[16] =
		{0x20, 0x4b, 0x6e, 0x8d, 0xa3, 0xb5, 0xc5, 0xd2,
		 0xdc, 0xe5, 0xec, 0xf2, 0xf6, 0xfa, 0xfd, 0xff};
	static const __u8 Tgradient_5[16] =
		{0x37, 0x26, 0x20, 0x1a, 0x14, 0x10, 0x0e, 0x0b,
		 0x09, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x02};
	static const __u8 Tgamma_6[16] =		/* ?? was gamma 5 */
		{0x24, 0x44, 0x64, 0x84, 0x9d, 0xb2, 0xc4, 0xd3,
		 0xe0, 0xeb, 0xf4, 0xff, 0xff, 0xff, 0xff, 0xff};
	static const __u8 Tgradient_6[16] =
		{0x18, 0x20, 0x20, 0x1c, 0x16, 0x13, 0x10, 0x0e,
		 0x0b, 0x09, 0x07, 0x00, 0x00, 0x00, 0x00, 0x01};
	static const __u8 *gamma_tb[] = {
		NULL, Tgamma_1, Tgamma_2,
		Tgamma_3, Tgamma_4, Tgamma_5, Tgamma_6
	};
	static const __u8 *gradient_tb[] = {
		NULL, Tgradient_1, Tgradient_2,
		Tgradient_3, Tgradient_4, Tgradient_5, Tgradient_6
	};
#ifdef GSPCA_DEBUG
	__u8 v[16];
#endif

	Tgamma = gamma_tb[sd->gamma];
	Tgradient = gradient_tb[sd->gamma];

	k = (sd->contrast - 128)		/* -128 / 128 */
			* Tgamma[0];
	PDEBUG(D_CONF, "gamma:%d contrast:%d gamma coeff: %d/128",
		sd->gamma, sd->contrast, k);
	for (i = 0; i < 16; i++) {
		g = Tgamma[i] + kgamma_tb[i] * k / 128;
		if (g > 0xff)
			g = 0xff;
		else if (g <= 0)
			g = 1;
		reg_w(dev, g, 0x0120 + i);	/* gamma */
#ifdef GSPCA_DEBUG
		if (gspca_debug & D_CONF)
			v[i] = g;
#endif
	}
	PDEBUG(D_CONF, "tb: %02x %02x %02x %02x %02x %02x %02x %02x",
		v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
	PDEBUG(D_CONF, "    %02x %02x %02x %02x %02x %02x %02x %02x",
		v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
	for (i = 0; i < 16; i++) {
		g = Tgradient[i] - kgrad_tb[i] * k / 128;
		if (g > 0xff)
			g = 0xff;
		else if (g <= 0) {
			if (i != 15)
				g = 0;
			else
				g = 1;
		}
		reg_w(dev, g, 0x0130 + i);	/* gradient */
#ifdef GSPCA_DEBUG
		if (gspca_debug & D_CONF)
			v[i] = g;
#endif
	}
	PDEBUG(D_CONF, "    %02x %02x %02x %02x %02x %02x %02x %02x",
		v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
	PDEBUG(D_CONF, "    %02x %02x %02x %02x %02x %02x %02x %02x",
		v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
}

static void setquality(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	__u8 frxt;

	switch (sd->sensor) {
	case SENSOR_ADCM2700:
	case SENSOR_GC0305:
	case SENSOR_HV7131B:
	case SENSOR_OV7620:
	case SENSOR_PO2030:
		return;
	}
/*fixme: is it really 0008 0007 0018 for all other sensors? */
	reg_w(dev, QUANT_VAL, 0x0008);
	frxt = 0x30;
	reg_w(dev, frxt, 0x0007);
#if QUANT_VAL == 0 || QUANT_VAL == 1 || QUANT_VAL == 2
	frxt = 0xff;
#elif QUANT_VAL == 3
	frxt = 0xf0;
#elif QUANT_VAL == 4
	frxt = 0xe0;
#else
	frxt = 0x20;
#endif
	reg_w(dev, frxt, 0x0018);
}

/* Matches the sensor's internal frame rate to the lighting frequency.
 * Valid frequencies are:
 *	50Hz, for European and Asian lighting (default)
 *	60Hz, for American lighting
 *	0 = No Fliker (for outdoore usage)
 * Returns: 0 for success
 */
static int setlightfreq(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i, mode;
	const struct usb_action *zc3_freq;
	static const struct usb_action *freq_tb[SENSOR_MAX][6] = {
/* SENSOR_ADCM2700 0 */
		{adcm2700_NoFliker, adcm2700_NoFliker,
		 adcm2700_50HZ, adcm2700_50HZ,
		 adcm2700_60HZ, adcm2700_60HZ},
/* SENSOR_CS2102 1 */
		{cs2102_NoFliker, cs2102_NoFlikerScale,
		 cs2102_50HZ, cs2102_50HZScale,
		 cs2102_60HZ, cs2102_60HZScale},
/* SENSOR_CS2102K 2 */
		{cs2102_NoFliker, cs2102_NoFlikerScale,
		 NULL, NULL, /* currently disabled */
		 NULL, NULL},
/* SENSOR_GC0305 3 */
		{gc0305_NoFliker, gc0305_NoFliker,
		 gc0305_50HZ, gc0305_50HZ,
		 gc0305_60HZ, gc0305_60HZ},
/* SENSOR_HDCS2020b 4 */
		{hdcs2020b_NoFliker, hdcs2020b_NoFliker,
		 hdcs2020b_50HZ, hdcs2020b_50HZ,
		 hdcs2020b_60HZ, hdcs2020b_60HZ},
/* SENSOR_HV7131B 5 */
		{hv7131b_NoFlikerScale, hv7131b_NoFliker,
		 hv7131b_50HZScale, hv7131b_50HZ,
		 hv7131b_60HZScale, hv7131b_60HZ},
/* SENSOR_HV7131C 6 */
		{NULL, NULL,
		 NULL, NULL,
		 NULL, NULL},
/* SENSOR_ICM105A 7 */
		{icm105a_NoFliker, icm105a_NoFlikerScale,
		 icm105a_50HZ, icm105a_50HZScale,
		 icm105a_60HZ, icm105a_60HZScale},
/* SENSOR_MC501CB 8 */
		{MC501CB_NoFliker, MC501CB_NoFlikerScale,
		 MC501CB_50HZ, MC501CB_50HZScale,
		 MC501CB_60HZ, MC501CB_60HZScale},
/* SENSOR_OV7620 9 */
		{OV7620_NoFliker, OV7620_NoFliker,
		 OV7620_50HZ, OV7620_50HZ,
		 OV7620_60HZ, OV7620_60HZ},
/* SENSOR_OV7630C 10 */
		{NULL, NULL,
		 NULL, NULL,
		 NULL, NULL},
/* SENSOR_PAS106 11 */
		{pas106b_NoFliker, pas106b_NoFliker,
		 pas106b_50HZ, pas106b_50HZ,
		 pas106b_60HZ, pas106b_60HZ},
/* SENSOR_PAS202B 12 */
		{pas202b_NoFlikerScale, pas202b_NoFliker,
		 pas202b_50HZScale, pas202b_50HZ,
		 pas202b_60HZScale, pas202b_60HZ},
/* SENSOR_PB0330 13 */
		{pb0330_NoFliker, pb0330_NoFlikerScale,
		 pb0330_50HZ, pb0330_50HZScale,
		 pb0330_60HZ, pb0330_60HZScale},
/* SENSOR_PO2030 14 */
		{PO2030_NoFliker, PO2030_NoFliker,
		 PO2030_50HZ, PO2030_50HZ,
		 PO2030_60HZ, PO2030_60HZ},
/* SENSOR_TAS5130CK 15 */
		{tas5130cxx_NoFliker, tas5130cxx_NoFlikerScale,
		 tas5130cxx_50HZ, tas5130cxx_50HZScale,
		 tas5130cxx_60HZ, tas5130cxx_60HZScale},
/* SENSOR_TAS5130CXX 16 */
		{tas5130cxx_NoFliker, tas5130cxx_NoFlikerScale,
		 tas5130cxx_50HZ, tas5130cxx_50HZScale,
		 tas5130cxx_60HZ, tas5130cxx_60HZScale},
/* SENSOR_TAS5130C_VF0250 17 */
		{tas5130c_vf0250_NoFliker, tas5130c_vf0250_NoFlikerScale,
		 tas5130c_vf0250_50HZ, tas5130c_vf0250_50HZScale,
		 tas5130c_vf0250_60HZ, tas5130c_vf0250_60HZScale},
	};

	i = sd->lightfreq * 2;
	mode = gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv;
	if (!mode)
		i++;			/* 640x480 */
	zc3_freq = freq_tb[(int) sd->sensor][i];
	if (zc3_freq != NULL) {
		usb_exchange(gspca_dev, zc3_freq);
		switch (sd->sensor) {
		case SENSOR_GC0305:
			if (mode			/* if 320x240 */
			    && sd->lightfreq == 1)	/* and 50Hz */
				reg_w(gspca_dev->dev, 0x85, 0x018d);
						/* win: 0x80, 0x018d */
			break;
		case SENSOR_OV7620:
			if (!mode) {			/* if 640x480 */
				if (sd->lightfreq != 0)	/* and 50 or 60 Hz */
					reg_w(gspca_dev->dev, 0x40, 0x0002);
				else
					reg_w(gspca_dev->dev, 0x44, 0x0002);
			}
			break;
		}
	}
	return 0;
}

static void setautogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 autoval;

	if (sd->autogain)
		autoval = 0x42;
	else
		autoval = 0x02;
	reg_w(gspca_dev->dev, autoval, 0x0180);
}

static void send_unknown(struct usb_device *dev, int sensor)
{
	reg_w(dev, 0x01, 0x0000);		/* led off */
	switch (sensor) {
	case SENSOR_PAS106:
		reg_w(dev, 0x03, 0x003a);
		reg_w(dev, 0x0c, 0x003b);
		reg_w(dev, 0x08, 0x0038);
		break;
	case SENSOR_ADCM2700:
	case SENSOR_GC0305:
	case SENSOR_OV7620:
	case SENSOR_PB0330:
	case SENSOR_PO2030:
		reg_w(dev, 0x0d, 0x003a);
		reg_w(dev, 0x02, 0x003b);
		reg_w(dev, 0x00, 0x0038);
		break;
	}
}

/* start probe 2 wires */
static void start_2wr_probe(struct usb_device *dev, int sensor)
{
	reg_w(dev, 0x01, 0x0000);
	reg_w(dev, sensor, 0x0010);
	reg_w(dev, 0x01, 0x0001);
	reg_w(dev, 0x03, 0x0012);
	reg_w(dev, 0x01, 0x0012);
/*	msleep(2); */
}

static int sif_probe(struct gspca_dev *gspca_dev)
{
	__u16 checkword;

	start_2wr_probe(gspca_dev->dev, 0x0f);		/* PAS106 */
	reg_w(gspca_dev->dev, 0x08, 0x008d);
	msleep(150);
	checkword = ((i2c_read(gspca_dev, 0x00) & 0x0f) << 4)
			| ((i2c_read(gspca_dev, 0x01) & 0xf0) >> 4);
	PDEBUG(D_PROBE, "probe sif 0x%04x", checkword);
	if (checkword == 0x0007) {
		send_unknown(gspca_dev->dev, SENSOR_PAS106);
		return 0x0f;			/* PAS106 */
	}
	return -1;
}

static int vga_2wr_probe(struct gspca_dev *gspca_dev)
{
	struct usb_device *dev = gspca_dev->dev;
	u16 retword;

	start_2wr_probe(dev, 0x00);		/* HV7131B */
	i2c_write(gspca_dev, 0x01, 0xaa, 0x00);
	retword = i2c_read(gspca_dev, 0x01);
	if (retword != 0)
		return 0x00;			/* HV7131B */

	start_2wr_probe(dev, 0x04);		/* CS2102 */
	i2c_write(gspca_dev, 0x01, 0xaa, 0x00);
	retword = i2c_read(gspca_dev, 0x01);
	if (retword != 0)
		return 0x04;			/* CS2102 */

	start_2wr_probe(dev, 0x06);		/* OmniVision */
	reg_w(dev, 0x08, 0x008d);
	i2c_write(gspca_dev, 0x11, 0xaa, 0x00);
	retword = i2c_read(gspca_dev, 0x11);
	if (retword != 0) {
		/* (should have returned 0xaa) --> Omnivision? */
		/* reg_r 0x10 -> 0x06 -->  */
		goto ov_check;
	}

	start_2wr_probe(dev, 0x08);		/* HDCS2020 */
	i2c_write(gspca_dev, 0x15, 0xaa, 0x00);
	retword = i2c_read(gspca_dev, 0x15);
	if (retword != 0)
		return 0x08;			/* HDCS2020 */

	start_2wr_probe(dev, 0x0a);		/* PB0330 */
	i2c_write(gspca_dev, 0x07, 0xaa, 0xaa);
	retword = i2c_read(gspca_dev, 0x07);
	if (retword != 0)
		return 0x0a;			/* PB0330 */
	retword = i2c_read(gspca_dev, 0x03);
	if (retword != 0)
		return 0x0a;			/* PB0330 ?? */
	retword = i2c_read(gspca_dev, 0x04);
	if (retword != 0)
		return 0x0a;			/* PB0330 ?? */

	start_2wr_probe(dev, 0x0c);		/* ICM105A */
	i2c_write(gspca_dev, 0x01, 0x11, 0x00);
	retword = i2c_read(gspca_dev, 0x01);
	if (retword != 0)
		return 0x0c;			/* ICM105A */

	start_2wr_probe(dev, 0x0e);		/* PAS202BCB */
	reg_w(dev, 0x08, 0x008d);
	i2c_write(gspca_dev, 0x03, 0xaa, 0x00);
	msleep(500);
	retword = i2c_read(gspca_dev, 0x03);
	if (retword != 0)
		return 0x0e;			/* PAS202BCB */

	start_2wr_probe(dev, 0x02);		/* ?? */
	i2c_write(gspca_dev, 0x01, 0xaa, 0x00);
	retword = i2c_read(gspca_dev, 0x01);
	if (retword != 0)
		return 0x02;			/* ?? */
ov_check:
	reg_r(gspca_dev, 0x0010);		/* ?? */
	reg_r(gspca_dev, 0x0010);

	reg_w(dev, 0x01, 0x0000);
	reg_w(dev, 0x01, 0x0001);
	reg_w(dev, 0x06, 0x0010);		/* OmniVision */
	reg_w(dev, 0xa1, 0x008b);
	reg_w(dev, 0x08, 0x008d);
	msleep(500);
	reg_w(dev, 0x01, 0x0012);
	i2c_write(gspca_dev, 0x12, 0x80, 0x00);	/* sensor reset */
	retword = i2c_read(gspca_dev, 0x0a) << 8;
	retword |= i2c_read(gspca_dev, 0x0b);
	PDEBUG(D_PROBE, "probe 2wr ov vga 0x%04x", retword);
	switch (retword) {
	case 0x7631:				/* OV7630C */
		reg_w(dev, 0x06, 0x0010);
		break;
	case 0x7620:				/* OV7620 */
	case 0x7648:				/* OV7648 */
		break;
	default:
		return -1;			/* not OmniVision */
	}
	return retword;
}

struct sensor_by_chipset_revision {
	__u16 revision;
	__u8 internal_sensor_id;
};
static const struct sensor_by_chipset_revision chipset_revision_sensor[] = {
	{0xc001, 0x13},		/* MI0360 */
	{0xe001, 0x13},
	{0x8001, 0x13},
	{0x8000, 0x14},		/* CS2102K */
	{0x8400, 0x15},		/* TAS5130K */
};

static int vga_3wr_probe(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	int i;
	__u8 retbyte;
	u16 retword;

/*fixme: lack of 8b=b3 (11,12)-> 10, 8b=e0 (14,15,16)-> 12 found in gspcav1*/
	reg_w(dev, 0x02, 0x0010);
	reg_r(gspca_dev, 0x0010);
	reg_w(dev, 0x01, 0x0000);
	reg_w(dev, 0x00, 0x0010);
	reg_w(dev, 0x01, 0x0001);
	reg_w(dev, 0x91, 0x008b);
	reg_w(dev, 0x03, 0x0012);
	reg_w(dev, 0x01, 0x0012);
	reg_w(dev, 0x05, 0x0012);
	retword = i2c_read(gspca_dev, 0x14);
	if (retword != 0)
		return 0x11;			/* HV7131R */
	retword = i2c_read(gspca_dev, 0x15);
	if (retword != 0)
		return 0x11;			/* HV7131R */
	retword = i2c_read(gspca_dev, 0x16);
	if (retword != 0)
		return 0x11;			/* HV7131R */

	reg_w(dev, 0x02, 0x0010);
	retword = reg_r(gspca_dev, 0x000b) << 8;
	retword |= reg_r(gspca_dev, 0x000a);
	PDEBUG(D_PROBE, "probe 3wr vga 1 0x%04x", retword);
	reg_r(gspca_dev, 0x0010);
	/* value 0x4001 is meaningless */
	if (retword != 0x4001) {
		for (i = 0; i < ARRAY_SIZE(chipset_revision_sensor); i++) {
			if (chipset_revision_sensor[i].revision == retword) {
				sd->chip_revision = retword;
				send_unknown(dev, SENSOR_PB0330);
				return chipset_revision_sensor[i]
							.internal_sensor_id;
			}
		}
	}

	reg_w(dev, 0x01, 0x0000);	/* check ?? */
	reg_w(dev, 0x01, 0x0001);
	reg_w(dev, 0xdd, 0x008b);
	reg_w(dev, 0x0a, 0x0010);
	reg_w(dev, 0x03, 0x0012);
	reg_w(dev, 0x01, 0x0012);
	retword = i2c_read(gspca_dev, 0x00);
	if (retword != 0) {
		PDEBUG(D_PROBE, "probe 3wr vga type 0a ?");
		return 0x0a;			/* ?? */
	}

	reg_w(dev, 0x01, 0x0000);
	reg_w(dev, 0x01, 0x0001);
	reg_w(dev, 0x98, 0x008b);
	reg_w(dev, 0x01, 0x0010);
	reg_w(dev, 0x03, 0x0012);
	msleep(2);
	reg_w(dev, 0x01, 0x0012);
	retword = i2c_read(gspca_dev, 0x00);
	if (retword != 0) {
		PDEBUG(D_PROBE, "probe 3wr vga type %02x", retword);
		if (retword == 0x0011)			/* VF0250 */
			return 0x0250;
		if (retword == 0x0029)			/* gc0305 */
			send_unknown(dev, SENSOR_GC0305);
		return retword;
	}

	reg_w(dev, 0x01, 0x0000);	/* check OmniVision */
	reg_w(dev, 0x01, 0x0001);
	reg_w(dev, 0xa1, 0x008b);
	reg_w(dev, 0x08, 0x008d);
	reg_w(dev, 0x06, 0x0010);
	reg_w(dev, 0x01, 0x0012);
	reg_w(dev, 0x05, 0x0012);
	if (i2c_read(gspca_dev, 0x1c) == 0x007f	/* OV7610 - manufacturer ID */
	    && i2c_read(gspca_dev, 0x1d) == 0x00a2) {
		send_unknown(dev, SENSOR_OV7620);
		return 0x06;		/* OmniVision confirm ? */
	}

	reg_w(dev, 0x01, 0x0000);
	reg_w(dev, 0x00, 0x0002);
	reg_w(dev, 0x01, 0x0010);
	reg_w(dev, 0x01, 0x0001);
	reg_w(dev, 0xee, 0x008b);
	reg_w(dev, 0x03, 0x0012);
/*	msleep(150); */
	reg_w(dev, 0x01, 0x0012);
	reg_w(dev, 0x05, 0x0012);
	retword = i2c_read(gspca_dev, 0x00) << 8;	/* ID 0 */
	retword |= i2c_read(gspca_dev, 0x01);		/* ID 1 */
	PDEBUG(D_PROBE, "probe 3wr vga 2 0x%04x", retword);
	if (retword == 0x2030) {
		retbyte = i2c_read(gspca_dev, 0x02);	/* revision number */
		PDEBUG(D_PROBE, "sensor PO2030 rev 0x%02x", retbyte);
		send_unknown(dev, SENSOR_PO2030);
		return retword;
	}

	reg_w(dev, 0x01, 0x0000);
	reg_w(dev, 0x0a, 0x0010);
	reg_w(dev, 0xd3, 0x008b);
	reg_w(dev, 0x01, 0x0001);
	reg_w(dev, 0x03, 0x0012);
	reg_w(dev, 0x01, 0x0012);
	reg_w(dev, 0x05, 0x0012);
	reg_w(dev, 0xd3, 0x008b);
	retword = i2c_read(gspca_dev, 0x01);
	if (retword != 0) {
		PDEBUG(D_PROBE, "probe 3wr vga type 0a ? ret: %04x", retword);
		return 0x16;			/* adcm2700 (6100/6200) */
	}
	return -1;
}

static int zcxx_probeSensor(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int sensor;

	switch (sd->sensor) {
	case SENSOR_MC501CB:
		return -1;		/* don't probe */
	case SENSOR_TAS5130C_VF0250:
			/* may probe but with no write in reg 0x0010 */
		return -1;		/* don't probe */
	case SENSOR_PAS106:
		sensor =  sif_probe(gspca_dev);
		if (sensor >= 0)
			return sensor;
		break;
	}
	sensor = vga_2wr_probe(gspca_dev);
	if (sensor >= 0)
		return sensor;
	return vga_3wr_probe(gspca_dev);
}

/* this function is called at probe time */
static int sd_config(struct gspca_dev *gspca_dev,
			const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam;
	int sensor;
	int vga = 1;		/* 1: vga, 0: sif */
	static const __u8 gamma[SENSOR_MAX] = {
		4,	/* SENSOR_ADCM2700 0 */
		5,	/* SENSOR_CS2102 1 */
		5,	/* SENSOR_CS2102K 2 */
		4,	/* SENSOR_GC0305 3 */
		4,	/* SENSOR_HDCS2020b 4 */
		4,	/* SENSOR_HV7131B 5 */
		4,	/* SENSOR_HV7131C 6 */
		4,	/* SENSOR_ICM105A 7 */
		4,	/* SENSOR_MC501CB 8 */
		3,	/* SENSOR_OV7620 9 */
		4,	/* SENSOR_OV7630C 10 */
		4,	/* SENSOR_PAS106 11 */
		4,	/* SENSOR_PAS202B 12 */
		4,	/* SENSOR_PB0330 13 */
		4,	/* SENSOR_PO2030 14 */
		4,	/* SENSOR_TAS5130CK 15 */
		4,	/* SENSOR_TAS5130CXX 16 */
		3,	/* SENSOR_TAS5130C_VF0250 17 */
	};

	/* define some sensors from the vendor/product */
	sd->sharpness = 2;
	sd->sensor = id->driver_info;
	sensor = zcxx_probeSensor(gspca_dev);
	if (sensor >= 0)
		PDEBUG(D_PROBE, "probe sensor -> %04x", sensor);
	if ((unsigned) force_sensor < SENSOR_MAX) {
		sd->sensor = force_sensor;
		PDEBUG(D_PROBE, "sensor forced to %d", force_sensor);
	} else {
		switch (sensor) {
		case -1:
			switch (sd->sensor) {
			case SENSOR_MC501CB:
				PDEBUG(D_PROBE, "Sensor MC501CB");
				break;
			case SENSOR_TAS5130C_VF0250:
				PDEBUG(D_PROBE, "Sensor Tas5130 (VF0250)");
				break;
			default:
				PDEBUG(D_PROBE,
					"Sensor UNKNOW_0 force Tas5130");
				sd->sensor = SENSOR_TAS5130CXX;
			}
			break;
		case 0:
			PDEBUG(D_PROBE, "Find Sensor HV7131B");
			sd->sensor = SENSOR_HV7131B;
			break;
		case 0x04:
			PDEBUG(D_PROBE, "Find Sensor CS2102");
			sd->sensor = SENSOR_CS2102;
			break;
		case 0x08:
			PDEBUG(D_PROBE, "Find Sensor HDCS2020(b)");
			sd->sensor = SENSOR_HDCS2020b;
			break;
		case 0x0a:
			PDEBUG(D_PROBE,
				"Find Sensor PB0330. Chip revision %x",
				sd->chip_revision);
			sd->sensor = SENSOR_PB0330;
			break;
		case 0x0c:
			PDEBUG(D_PROBE, "Find Sensor ICM105A");
			sd->sensor = SENSOR_ICM105A;
			break;
		case 0x0e:
			PDEBUG(D_PROBE, "Find Sensor PAS202B");
			sd->sensor = SENSOR_PAS202B;
			sd->sharpness = 1;
			break;
		case 0x0f:
			PDEBUG(D_PROBE, "Find Sensor PAS106");
			sd->sensor = SENSOR_PAS106;
			vga = 0;		/* SIF */
			break;
		case 0x10:
		case 0x12:
			PDEBUG(D_PROBE, "Find Sensor TAS5130");
			sd->sensor = SENSOR_TAS5130CXX;
			break;
		case 0x11:
			PDEBUG(D_PROBE, "Find Sensor HV7131R(c)");
			sd->sensor = SENSOR_HV7131C;
			break;
		case 0x13:
			PDEBUG(D_PROBE,
				"Find Sensor MI0360. Chip revision %x",
				sd->chip_revision);
			sd->sensor = SENSOR_PB0330;
			break;
		case 0x14:
			PDEBUG(D_PROBE,
				"Find Sensor CS2102K?. Chip revision %x",
				sd->chip_revision);
			sd->sensor = SENSOR_CS2102K;
			break;
		case 0x15:
			PDEBUG(D_PROBE,
				"Find Sensor TAS5130CK?. Chip revision %x",
				sd->chip_revision);
			sd->sensor = SENSOR_TAS5130CK;
			break;
		case 0x16:
			PDEBUG(D_PROBE, "Find Sensor ADCM2700");
			sd->sensor = SENSOR_ADCM2700;
			break;
		case 0x29:
			PDEBUG(D_PROBE, "Find Sensor GC0305");
			sd->sensor = SENSOR_GC0305;
			break;
		case 0x0250:
			PDEBUG(D_PROBE, "Sensor Tas5130 (VF0250)");
			sd->sensor =  SENSOR_TAS5130C_VF0250;
			break;
		case 0x2030:
			PDEBUG(D_PROBE, "Find Sensor PO2030");
			sd->sensor = SENSOR_PO2030;
			sd->sharpness = 0;		/* from win traces */
			break;
		case 0x7620:
			PDEBUG(D_PROBE, "Find Sensor OV7620");
			sd->sensor = SENSOR_OV7620;
			break;
		case 0x7631:
			PDEBUG(D_PROBE, "Find Sensor OV7630C");
			sd->sensor = SENSOR_OV7630C;
			break;
		case 0x7648:
			PDEBUG(D_PROBE, "Find Sensor OV7648");
			sd->sensor = SENSOR_OV7620;	/* same sensor (?) */
			break;
		default:
			PDEBUG(D_ERR|D_PROBE, "Unknown sensor %04x", sensor);
			return -EINVAL;
		}
	}
	if (sensor < 0x20) {
		if (sensor == -1 || sensor == 0x10 || sensor == 0x12)
			reg_w(gspca_dev->dev, 0x02, 0x0010);
		else
			reg_w(gspca_dev->dev, sensor & 0x0f, 0x0010);
		reg_r(gspca_dev, 0x0010);
	}

	cam = &gspca_dev->cam;
/*fixme:test*/
	gspca_dev->nbalt--;
	if (vga) {
		cam->cam_mode = vga_mode;
		cam->nmodes = ARRAY_SIZE(vga_mode);
	} else {
		cam->cam_mode = sif_mode;
		cam->nmodes = ARRAY_SIZE(sif_mode);
	}
	sd->brightness = sd_ctrls[SD_BRIGHTNESS].qctrl.default_value;
	sd->contrast = sd_ctrls[SD_CONTRAST].qctrl.default_value;
	sd->gamma = gamma[(int) sd->sensor];
	sd->autogain = sd_ctrls[SD_AUTOGAIN].qctrl.default_value;
	sd->lightfreq = sd_ctrls[SD_FREQ].qctrl.default_value;
	sd->quality = QUALITY_DEF;

	switch (sd->sensor) {
	case SENSOR_GC0305:
	case SENSOR_OV7620:
	case SENSOR_PO2030:
		gspca_dev->ctrl_dis = (1 << BRIGHTNESS_IDX);
		break;
	case SENSOR_HV7131B:
	case SENSOR_HV7131C:
	case SENSOR_OV7630C:
		gspca_dev->ctrl_dis = (1 << LIGHTFREQ_IDX);
		break;
	}

	/* switch the led off */
	reg_w(gspca_dev->dev, 0x01, 0x0000);
	return 0;
}

/* this function is called at probe and resume time */
static int sd_init(struct gspca_dev *gspca_dev)
{
	reg_w(gspca_dev->dev, 0x01, 0x0000);
	return 0;
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	const struct usb_action *zc3_init;
	int mode;
	static const struct usb_action *init_tb[SENSOR_MAX][2] = {
		{adcm2700_Initial, adcm2700_InitialScale},	/* 0 */
		{cs2102_InitialScale, cs2102_Initial},		/* 1 */
		{cs2102K_InitialScale, cs2102K_Initial},	/* 2 */
		{gc0305_Initial, gc0305_InitialScale},		/* 3 */
		{hdcs2020xb_InitialScale, hdcs2020xb_Initial},	/* 4 */
		{hv7131bxx_InitialScale, hv7131bxx_Initial},	/* 5 */
		{hv7131cxx_InitialScale, hv7131cxx_Initial},	/* 6 */
		{icm105axx_InitialScale, icm105axx_Initial},	/* 7 */
		{MC501CB_InitialScale, MC501CB_Initial},	/* 8 */
		{OV7620_mode0, OV7620_mode1},			/* 9 */
		{ov7630c_InitialScale, ov7630c_Initial},	/* 10 */
		{pas106b_InitialScale, pas106b_Initial},	/* 11 */
		{pas202b_Initial, pas202b_InitialScale},	/* 12 */
		{pb0330xx_InitialScale, pb0330xx_Initial},	/* 13 */
/* or		{pb03303x_InitialScale, pb03303x_Initial}, */
		{PO2030_mode0, PO2030_mode1},			/* 14 */
		{tas5130CK_InitialScale, tas5130CK_Initial},	/* 15 */
		{tas5130cxx_InitialScale, tas5130cxx_Initial},	/* 16 */
		{tas5130c_vf0250_InitialScale, tas5130c_vf0250_Initial},
								/* 17 */
	};

	/* create the JPEG header */
	sd->jpeg_hdr = kmalloc(JPEG_HDR_SZ, GFP_KERNEL);
	if (!sd->jpeg_hdr)
		return -ENOMEM;
	jpeg_define(sd->jpeg_hdr, gspca_dev->height, gspca_dev->width,
			0x21);		/* JPEG 422 */
	jpeg_set_qual(sd->jpeg_hdr, sd->quality);

	mode = gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv;
	zc3_init = init_tb[(int) sd->sensor][mode];
	switch (sd->sensor) {
	case SENSOR_HV7131C:
		zcxx_probeSensor(gspca_dev);
		break;
	case SENSOR_PAS106:
		usb_exchange(gspca_dev, pas106b_Initial_com);
		break;
	case SENSOR_PB0330:
		if (mode) {
			if (sd->chip_revision == 0xc001
			    || sd->chip_revision == 0xe001
			    || sd->chip_revision == 0x8001)
				zc3_init = pb03303x_Initial;
		} else {
			if (sd->chip_revision == 0xc001
			    || sd->chip_revision == 0xe001
			    || sd->chip_revision == 0x8001)
				zc3_init = pb03303x_InitialScale;
		}
		break;
	}
	usb_exchange(gspca_dev, zc3_init);

	switch (sd->sensor) {
	case SENSOR_ADCM2700:
	case SENSOR_GC0305:
	case SENSOR_OV7620:
	case SENSOR_PO2030:
	case SENSOR_TAS5130C_VF0250:
/*		msleep(100);			 * ?? */
		reg_r(gspca_dev, 0x0002);	/* --> 0x40 */
		reg_w(dev, 0x09, 0x01ad);	/* (from win traces) */
		reg_w(dev, 0x15, 0x01ae);
		reg_w(dev, 0x0d, 0x003a);
		reg_w(dev, 0x02, 0x003b);
		reg_w(dev, 0x00, 0x0038);
		break;
	}

	setmatrix(gspca_dev);
	setbrightness(gspca_dev);
	switch (sd->sensor) {
	case SENSOR_ADCM2700:
	case SENSOR_OV7620:
		reg_r(gspca_dev, 0x0008);
		reg_w(dev, 0x00, 0x0008);
		break;
	case SENSOR_PAS202B:
	case SENSOR_GC0305:
		reg_r(gspca_dev, 0x0008);
		/* fall thru */
	case SENSOR_PO2030:
		reg_w(dev, 0x03, 0x0008);
		break;
	}
	setsharpness(gspca_dev);

	/* set the gamma tables when not set */
	switch (sd->sensor) {
	case SENSOR_CS2102:		/* gamma set in xxx_Initial */
	case SENSOR_CS2102K:
	case SENSOR_HDCS2020b:
	case SENSOR_PB0330:		/* pb with chip_revision - see above */
	case SENSOR_OV7630C:
	case SENSOR_TAS5130CK:
		break;
	default:
		setcontrast(gspca_dev);
		break;
	}
	setmatrix(gspca_dev);			/* one more time? */
	switch (sd->sensor) {
	case SENSOR_OV7620:
	case SENSOR_PAS202B:
		reg_r(gspca_dev, 0x0180);	/* from win */
		reg_w(dev, 0x00, 0x0180);
		break;
	default:
		setquality(gspca_dev);
		break;
	}
	setlightfreq(gspca_dev);

	switch (sd->sensor) {
	case SENSOR_ADCM2700:
		reg_w(dev, 0x09, 0x01ad);	/* (from win traces) */
		reg_w(dev, 0x15, 0x01ae);
		reg_w(dev, 0x02, 0x0180);
						/* ms-win + */
		reg_w(dev, 0x40, 0x0117);
		break;
	case SENSOR_GC0305:
		reg_w(dev, 0x09, 0x01ad);	/* (from win traces) */
		reg_w(dev, 0x15, 0x01ae);
		/* fall thru */
	case SENSOR_PAS202B:
	case SENSOR_PO2030:
/*		reg_w(dev, 0x40, ZC3XX_R117_GGAIN);  * (from win traces) */
		reg_r(gspca_dev, 0x0180);
		break;
	case SENSOR_OV7620:
		reg_w(dev, 0x09, 0x01ad);
		reg_w(dev, 0x15, 0x01ae);
		i2c_read(gspca_dev, 0x13);	/*fixme: returns 0xa3 */
		i2c_write(gspca_dev, 0x13, 0xa3, 0x00);
					 /*fixme: returned value to send? */
		reg_w(dev, 0x40, 0x0117);
		reg_r(gspca_dev, 0x0180);
		break;
	}

	setautogain(gspca_dev);
	switch (sd->sensor) {
	case SENSOR_PO2030:
		msleep(500);
		reg_r(gspca_dev, 0x0008);
		reg_r(gspca_dev, 0x0007);
		/*fall thru*/
	case SENSOR_PAS202B:
		reg_w(dev, 0x00, 0x0007);	/* (from win traces) */
		reg_w(dev, 0x02, ZC3XX_R008_CLOCKSETTING);
		break;
	}
	return 0;
}

/* called on streamoff with alt 0 and on disconnect */
static void sd_stop0(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	kfree(sd->jpeg_hdr);
	if (!gspca_dev->present)
		return;
	send_unknown(gspca_dev->dev, sd->sensor);
}

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			struct gspca_frame *frame,
			__u8 *data,
			int len)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (data[0] == 0xff && data[1] == 0xd8) {	/* start of frame */
		frame = gspca_frame_add(gspca_dev, LAST_PACKET, frame,
					data, 0);
		/* put the JPEG header in the new frame */
		gspca_frame_add(gspca_dev, FIRST_PACKET, frame,
			sd->jpeg_hdr, JPEG_HDR_SZ);

		/* remove the webcam's header:
		 * ff d8 ff fe 00 0e 00 00 ss ss 00 01 ww ww hh hh pp pp
		 *	- 'ss ss' is the frame sequence number (BE)
		 * 	- 'ww ww' and 'hh hh' are the window dimensions (BE)
		 *	- 'pp pp' is the packet sequence number (BE)
		 */
		data += 18;
		len -= 18;
	}
	gspca_frame_add(gspca_dev, INTER_PACKET, frame, data, len);
}

static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->brightness = val;
	if (gspca_dev->streaming)
		setbrightness(gspca_dev);
	return 0;
}

static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->brightness;
	return 0;
}

static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->contrast = val;
	if (gspca_dev->streaming)
		setcontrast(gspca_dev);
	return 0;
}

static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->contrast;
	return 0;
}

static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->autogain = val;
	if (gspca_dev->streaming)
		setautogain(gspca_dev);
	return 0;
}

static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->autogain;
	return 0;
}

static int sd_setgamma(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->gamma = val;
	if (gspca_dev->streaming)
		setcontrast(gspca_dev);
	return 0;
}

static int sd_getgamma(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->gamma;
	return 0;
}

static int sd_setfreq(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->lightfreq = val;
	if (gspca_dev->streaming)
		setlightfreq(gspca_dev);
	return 0;
}

static int sd_getfreq(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->lightfreq;
	return 0;
}

static int sd_setsharpness(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->sharpness = val;
	if (gspca_dev->streaming)
		setsharpness(gspca_dev);
	return 0;
}

static int sd_getsharpness(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->sharpness;
	return 0;
}

static int sd_querymenu(struct gspca_dev *gspca_dev,
			struct v4l2_querymenu *menu)
{
	switch (menu->id) {
	case V4L2_CID_POWER_LINE_FREQUENCY:
		switch (menu->index) {
		case 0:		/* V4L2_CID_POWER_LINE_FREQUENCY_DISABLED */
			strcpy((char *) menu->name, "NoFliker");
			return 0;
		case 1:		/* V4L2_CID_POWER_LINE_FREQUENCY_50HZ */
			strcpy((char *) menu->name, "50 Hz");
			return 0;
		case 2:		/* V4L2_CID_POWER_LINE_FREQUENCY_60HZ */
			strcpy((char *) menu->name, "60 Hz");
			return 0;
		}
		break;
	}
	return -EINVAL;
}

static int sd_set_jcomp(struct gspca_dev *gspca_dev,
			struct v4l2_jpegcompression *jcomp)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (jcomp->quality < QUALITY_MIN)
		sd->quality = QUALITY_MIN;
	else if (jcomp->quality > QUALITY_MAX)
		sd->quality = QUALITY_MAX;
	else
		sd->quality = jcomp->quality;
	if (gspca_dev->streaming)
		jpeg_set_qual(sd->jpeg_hdr, sd->quality);
	return 0;
}

static int sd_get_jcomp(struct gspca_dev *gspca_dev,
			struct v4l2_jpegcompression *jcomp)
{
	struct sd *sd = (struct sd *) gspca_dev;

	memset(jcomp, 0, sizeof *jcomp);
	jcomp->quality = sd->quality;
	jcomp->jpeg_markers = V4L2_JPEG_MARKER_DHT
			| V4L2_JPEG_MARKER_DQT;
	return 0;
}

static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.init = sd_init,
	.start = sd_start,
	.stop0 = sd_stop0,
	.pkt_scan = sd_pkt_scan,
	.querymenu = sd_querymenu,
	.get_jcomp = sd_get_jcomp,
	.set_jcomp = sd_set_jcomp,
};

static const __devinitdata struct usb_device_id device_table[] = {
	{USB_DEVICE(0x041e, 0x041e)},
	{USB_DEVICE(0x041e, 0x4017)},
	{USB_DEVICE(0x041e, 0x401c), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x041e, 0x401e)},
	{USB_DEVICE(0x041e, 0x401f)},
	{USB_DEVICE(0x041e, 0x4022)},
	{USB_DEVICE(0x041e, 0x4029)},
	{USB_DEVICE(0x041e, 0x4034), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x041e, 0x4035), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x041e, 0x4036)},
	{USB_DEVICE(0x041e, 0x403a)},
	{USB_DEVICE(0x041e, 0x4051), .driver_info = SENSOR_TAS5130C_VF0250},
	{USB_DEVICE(0x041e, 0x4053), .driver_info = SENSOR_TAS5130C_VF0250},
	{USB_DEVICE(0x0458, 0x7007)},
	{USB_DEVICE(0x0458, 0x700c)},
	{USB_DEVICE(0x0458, 0x700f)},
	{USB_DEVICE(0x0461, 0x0a00)},
	{USB_DEVICE(0x046d, 0x089d), .driver_info = SENSOR_MC501CB},
	{USB_DEVICE(0x046d, 0x08a0)},
	{USB_DEVICE(0x046d, 0x08a1)},
	{USB_DEVICE(0x046d, 0x08a2)},
	{USB_DEVICE(0x046d, 0x08a3)},
	{USB_DEVICE(0x046d, 0x08a6)},
	{USB_DEVICE(0x046d, 0x08a7)},
	{USB_DEVICE(0x046d, 0x08a9)},
	{USB_DEVICE(0x046d, 0x08aa)},
	{USB_DEVICE(0x046d, 0x08ac)},
	{USB_DEVICE(0x046d, 0x08ad)},
#if !defined CONFIG_USB_ZC0301 && !defined CONFIG_USB_ZC0301_MODULE
	{USB_DEVICE(0x046d, 0x08ae)},
#endif
	{USB_DEVICE(0x046d, 0x08af)},
	{USB_DEVICE(0x046d, 0x08b9)},
	{USB_DEVICE(0x046d, 0x08d7)},
	{USB_DEVICE(0x046d, 0x08d9)},
	{USB_DEVICE(0x046d, 0x08d8)},
	{USB_DEVICE(0x046d, 0x08da)},
	{USB_DEVICE(0x046d, 0x08dd), .driver_info = SENSOR_MC501CB},
	{USB_DEVICE(0x0471, 0x0325), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x0471, 0x0326), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x0471, 0x032d), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x0471, 0x032e), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x055f, 0xc005)},
	{USB_DEVICE(0x055f, 0xd003)},
	{USB_DEVICE(0x055f, 0xd004)},
	{USB_DEVICE(0x0698, 0x2003)},
	{USB_DEVICE(0x0ac8, 0x0301), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x0ac8, 0x0302), .driver_info = SENSOR_PAS106},
	{USB_DEVICE(0x0ac8, 0x301b)},
	{USB_DEVICE(0x0ac8, 0x303b)},
	{USB_DEVICE(0x0ac8, 0x305b), .driver_info = SENSOR_TAS5130C_VF0250},
	{USB_DEVICE(0x0ac8, 0x307b)},
	{USB_DEVICE(0x10fd, 0x0128)},
	{USB_DEVICE(0x10fd, 0x804d)},
	{USB_DEVICE(0x10fd, 0x8050)},
	{}			/* end of entry */
};
#undef DVNAME
MODULE_DEVICE_TABLE(usb, device_table);

/* -- device connect -- */
static int sd_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
				THIS_MODULE);
}

/* USB driver */
static struct usb_driver sd_driver = {
	.name = MODULE_NAME,
	.id_table = device_table,
	.probe = sd_probe,
	.disconnect = gspca_disconnect,
#ifdef CONFIG_PM
	.suspend = gspca_suspend,
	.resume = gspca_resume,
#endif
};

static int __init sd_mod_init(void)
{
	int ret;
	ret = usb_register(&sd_driver);
	if (ret < 0)
		return ret;
	PDEBUG(D_PROBE, "registered");
	return 0;
}

static void __exit sd_mod_exit(void)
{
	usb_deregister(&sd_driver);
	PDEBUG(D_PROBE, "deregistered");
}

module_init(sd_mod_init);
module_exit(sd_mod_exit);

module_param(force_sensor, int, 0644);
MODULE_PARM_DESC(force_sensor,
	"Force sensor. Only for experts!!!");
