/* linux/arch/arm/mach-msm/board-incrediblec.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation.
 * Author: Dima Zavin <dima@android.com>
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-msm.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/input.h>
#include <linux/akm8973.h>
#include <linux/bma150.h>
#include <linux/capella_cm3602.h>
#include <linux/regulator/machine.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <mach/htc_headset_mgr.h>
#include <mach/htc_headset_gpio.h>
#include <mach/htc_headset_microp.h>

#include <mach/board.h>
#include <mach/board_htc.h>
#include <mach/hardware.h>
#include <mach/atmega_microp.h>
#include <mach/camera.h>
#include <mach/msm_iomap.h>
#include <mach/htc_battery.h>
#include <mach/htc_usb.h>
#include <mach/perflock.h>
#include <mach/msm_serial_debugger.h>
#include <mach/system.h>
#include <linux/spi/spi.h>
#include <linux/curcial_oj.h>
#include <mach/msm_panel.h>
#include "board-incrediblec.h"
#include "devices.h"
#include "proc_comm.h"
#include "smd_private.h"
#if 1	/*allenou, bt for bcm, 2009/7/8 */
#include <mach/msm_serial_hs.h>
#endif
#include <mach/tpa6130.h>
#include <mach/msm_flashlight.h>
#include <linux/atmel_qt602240.h>
#include <mach/vreg.h>
/* #include <mach/pmic.h> */
#include <mach/msm_hsusb.h>

#define SMEM_SPINLOCK_I2C      6
#define INCREDIBLEC_MICROP_VER		0x04

#ifdef CONFIG_ARCH_QSD8X50
extern unsigned char *get_bt_bd_ram(void);
#endif

void msm_init_pmic_vibrator(void);
extern void __init incrediblec_audio_init(void);
#ifdef CONFIG_MICROP_COMMON
void __init incrediblec_microp_init(void);
#endif

#define SAMSUNG_PANEL		0
/*Bitwise mask for SONY PANEL ONLY*/
#define SONY_PANEL		0x1		/*Set bit 0 as 1 when it is SONY PANEL*/
#define SONY_PWM_SPI		0x2		/*Set bit 1 as 1 as PWM_SPI mode, otherwise it is PWM_MICROP mode*/
#define SONY_GAMMA		0x4		/*Set bit 2 as 1 when panel contains GAMMA table in its NVM*/
#define SONY_RGB666		0x8		/*Set bit 3 as 1 when panel is 18 bit, otherwise it is 16 bit*/

extern int panel_type;

unsigned int engineerid;

static struct htc_battery_platform_data htc_battery_pdev_data = {
/*	.gpio_mbat_in = INCREDIBLEC_GPIO_MBAT_IN,*/
/*	.gpio_mchg_en_n = INCREDIBLEC_GPIO_MCHG_EN_N,*/
/*	.gpio_iset = INCREDIBLEC_GPIO_ISET,*/
	.guage_driver = GUAGE_MODEM,
	.m2a_cable_detect = 1,
	.charger = SWITCH_CHARGER,
};

static struct platform_device htc_battery_pdev = {
	.name = "htc_battery",
	.id = -1,
	.dev	= {
		.platform_data = &htc_battery_pdev_data,
	},
};
static int capella_cm3602_power(int pwr_device, uint8_t enable);
/*XA, XB*/
static struct microp_function_config microp_functions[] = {
	{
		.name   = "microp_intrrupt",
		.category = MICROP_FUNCTION_INTR,
	},
	{
		.name   = "reset-int",
		.category = MICROP_FUNCTION_RESET_INT,
		.int_pin = 1 << 8,
	},
	{
		.name   = "oj",
		.category = MICROP_FUNCTION_OJ,
		.int_pin = 1 << 12,
	},
	{
		.name   = "proximity",
		.category = MICROP_FUNCTION_P,
		.int_pin = 1 << 11,
		.mask_r = {0x00, 0x00, 0x10},
		.mask_w = {0x00, 0x00, 0x04},
	},
};

/*For XC: Change ALS chip from CM3602 to CM3605*/
static struct microp_function_config microp_functions_1[] = {
	{
		.name   = "remote-key",
		.category = MICROP_FUNCTION_REMOTEKEY,
		.levels = {0, 33, 50, 110, 160, 220},
		.channel = 1,
		.int_pin = 1 << 5,
	},
	{
		.name   = "microp_intrrupt",
		.category = MICROP_FUNCTION_INTR,
	},
	{
		.name   = "reset-int",
		.category = MICROP_FUNCTION_RESET_INT,
		.int_pin = 1 << 8,
	},
	{
		.name   = "oj",
		.category = MICROP_FUNCTION_OJ,
		.int_pin = 1 << 12,
	},
	{
		.name   = "proximity",
		.category = MICROP_FUNCTION_P,
		.int_pin = 1 << 11,
		.mask_r = {0x00, 0x00, 0x10},
		.mask_w = {0x00, 0x00, 0x04},
	},
};

static struct microp_function_config microp_lightsensor = {
	.name = "light_sensor",
	.category = MICROP_FUNCTION_LSENSOR,
	.levels = { 0, 11, 16, 22, 75, 209, 362, 488, 560, 0x3FF },
	.channel = 3,
	.int_pin = 1 << 9,
	.golden_adc = 0xD2,
	.mask_w = {0x00, 0x00, 0x04},
	.ls_power = capella_cm3602_power,
};

static struct lightsensor_platform_data lightsensor_data = {
	.config = &microp_lightsensor,
	.irq = MSM_uP_TO_INT(9),
};

static struct microp_led_config led_config[] = {
	{
		.name = "amber",
		.type = LED_RGB,
	},
	{
		.name = "green",
		.type = LED_RGB,
	},
};

static struct microp_led_platform_data microp_leds_data = {
	.num_leds	= ARRAY_SIZE(led_config),
	.led_config	= led_config,
};

static struct bma150_platform_data incrediblec_g_sensor_pdata = {
	.microp_new_cmd = 1,
};

/* Proximity Sensor (Capella_CM3602)*/

static int __capella_cm3602_power(int on)
{
	uint8_t data[3], addr;
	int ret;

	printk(KERN_DEBUG "%s: Turn the capella_cm3602 power %s\n",
		__func__, (on) ? "on" : "off");
	if (on)
		gpio_direction_output(INCREDIBLEC_GPIO_PROXIMITY_EN_N, 1);

	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x04;
	addr = on ? MICROP_I2C_WCMD_GPO_LED_STATUS_EN :
			MICROP_I2C_WCMD_GPO_LED_STATUS_DIS;
	ret = microp_i2c_write(addr, data, 3);
	if (ret < 0)
		pr_err("%s: %s capella power failed\n",
			__func__, (on ? "enable" : "disable"));

	if (!on)
		gpio_direction_output(INCREDIBLEC_GPIO_PROXIMITY_EN_N, 0);

	return ret;
}

static DEFINE_MUTEX(capella_cm3602_lock);
static unsigned int als_power_control;

static int capella_cm3602_power(int pwr_device, uint8_t enable)
{
	unsigned int old_status = 0;
	int ret = 0, on = 0;
	mutex_lock(&capella_cm3602_lock);

	old_status = als_power_control;
	if (enable)
		als_power_control |= pwr_device;
	else
		als_power_control &= ~pwr_device;

	on = als_power_control ? 1 : 0;
	if (old_status == 0 && on)
		ret = __capella_cm3602_power(1);
	else if (!on)
		ret = __capella_cm3602_power(0);

	mutex_unlock(&capella_cm3602_lock);
	return ret;
}

static struct capella_cm3602_platform_data capella_cm3602_pdata = {
	.power = capella_cm3602_power,
	.p_en = INCREDIBLEC_GPIO_PROXIMITY_EN_N,
	.p_out = MSM_uP_TO_INT(11),
};
/* End Proximity Sensor (Capella_CM3602)*/

static struct htc_headset_microp_platform_data htc_headset_microp_data = {
	.remote_int		= 1 << 5,
	.remote_irq		= MSM_uP_TO_INT(5),
	.remote_enable_pin	= NULL,
	.adc_channel		= 0x01,
	.adc_remote		= {0, 33, 50, 110, 160, 220},
};

static struct platform_device microp_devices[] = {
	{
		.name = "lightsensor_microp",
		.dev = {
			.platform_data = &lightsensor_data,
		},
	},
	 {
		.name = "leds-microp",
		.id = -1,
		.dev = {
			.platform_data = &microp_leds_data,
		},
	},
	{
		.name = BMA150_G_SENSOR_NAME,
		.dev = {
			.platform_data = &incrediblec_g_sensor_pdata,
		},
	},
	{
		.name = "incrediblec_proximity",
		.id = -1,
		.dev = {
			.platform_data = &capella_cm3602_pdata,
		},
	},
	{
		.name	= "HTC_HEADSET_MICROP",
		.id	= -1,
		.dev	= {
			.platform_data	= &htc_headset_microp_data,
		},
	},
};

static struct microp_i2c_platform_data microp_data = {
	.num_functions   = ARRAY_SIZE(microp_functions),
	.microp_function = microp_functions,
	.num_devices = ARRAY_SIZE(microp_devices),
	.microp_devices = microp_devices,
	.gpio_reset = INCREDIBLEC_GPIO_UP_RESET_N,
	.microp_ls_on = LS_PWR_ON | PS_PWR_ON,
	.spi_devices = SPI_OJ | SPI_GSENSOR,
};

static struct gpio_led incrediblec_led_list[] = {
	{
		.name = "button-backlight",
		.gpio = INCREDIBLEC_AP_KEY_LED_EN,
		.active_low = 0,
	},
};

static struct gpio_led_platform_data incrediblec_leds_data = {
	.num_leds	= ARRAY_SIZE(incrediblec_led_list),
	.leds		= incrediblec_led_list,
};

static struct platform_device incrediblec_leds = {
	.name		= "leds-gpio",
	.id		= -1,
	.dev		= {
		.platform_data	= &incrediblec_leds_data,
	},
};

static uint32_t usb_phy_3v3_table[] = {
	PCOM_GPIO_CFG(INCREDIBLEC_USB_PHY_3V3_ENABLE, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA)
};

static uint32_t usb_ID_PIN_table[] = {
	PCOM_GPIO_CFG(INCREDIBLEC_GPIO_USB_ID_PIN, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_4MA),
};

static int incrediblec_phy_init_seq[] = { 0x1D, 0x0D, 0x1D, 0x10, -1 };
#ifdef CONFIG_USB_ANDROID
static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.phy_init_seq		= incrediblec_phy_init_seq,
	.phy_reset		= msm_hsusb_8x50_phy_reset,
	.usb_id_pin_gpio =  INCREDIBLEC_GPIO_USB_ID_PIN,
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 3,
	.vendor		= "HTC",
	.product	= "Android Phone",
	.release	= 0x0100,
	.cdrom_lun	= 4,
};

static struct platform_device usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id	= 0x0bb4,
	.product_id	= 0x0c9e,
	.version	= 0x0100,
	.product_name		= "Android Phone",
	.manufacturer_name	= "HTC",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id		= -1,
	.dev		= {
		.platform_data = &android_usb_pdata,
	},
};
static void inc_add_usb_devices(void)
{
	android_usb_pdata.products[0].product_id =
		android_usb_pdata.product_id;
	android_usb_pdata.serial_number = board_serialno();
	msm_hsusb_pdata.serial_number = board_serialno();
	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata;
	config_gpio_table(usb_phy_3v3_table, ARRAY_SIZE(usb_phy_3v3_table));
	gpio_set_value(INCREDIBLEC_USB_PHY_3V3_ENABLE, 1);
	config_gpio_table(usb_ID_PIN_table, ARRAY_SIZE(usb_ID_PIN_table));
	platform_device_register(&msm_device_hsusb);
	platform_device_register(&usb_mass_storage_device);
	platform_device_register(&android_usb_device);
}
#endif

static struct platform_device incrediblec_rfkill = {
	.name = "incrediblec_rfkill",
	.id = -1,
};

static struct resource qsd_spi_resources[] = {
	{
		.name   = "spi_irq_in",
		.start  = INT_SPI_INPUT,
		.end    = INT_SPI_INPUT,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_out",
		.start  = INT_SPI_OUTPUT,
		.end    = INT_SPI_OUTPUT,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_err",
		.start  = INT_SPI_ERROR,
		.end    = INT_SPI_ERROR,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_base",
		.start  = 0xA1200000,
		.end    = 0xA1200000 + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "spi_clk",
		.start  = 17,
		.end    = 1,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_mosi",
		.start  = 18,
		.end    = 1,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_miso",
		.start  = 19,
		.end    = 1,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_cs0",
		.start  = 20,
		.end    = 1,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_pwr",
		.start  = 21,
		.end    = 0,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_cs0",
		.start  = 22,
		.end    = 0,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device qsd_device_spi = {
	.name           = "spi_qsd",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(qsd_spi_resources),
	.resource       = qsd_spi_resources,
};

static struct resource msm_kgsl_resources[] = {
	{
		.name	= "kgsl_reg_memory",
		.start	= MSM_GPU_REG_PHYS,
		.end	= MSM_GPU_REG_PHYS + MSM_GPU_REG_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "kgsl_phys_memory",
		.start	= MSM_GPU_MEM_BASE,
		.end	= MSM_GPU_MEM_BASE + MSM_GPU_MEM_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_GRAPHICS,
		.end	= INT_GRAPHICS,
		.flags	= IORESOURCE_IRQ,
	},
};

#define PWR_RAIL_GRP_CLK		8
static int incrediblec_kgsl_power_rail_mode(int follow_clk)
{
	int mode = follow_clk ? 0 : 1;
	int rail_id = PWR_RAIL_GRP_CLK;

	return msm_proc_comm(PCOM_CLKCTL_RPC_RAIL_CONTROL, &rail_id, &mode);
}

static int incrediblec_kgsl_power(bool on)
{
	int cmd;
	int rail_id = PWR_RAIL_GRP_CLK;

	cmd = on ? PCOM_CLKCTL_RPC_RAIL_ENABLE : PCOM_CLKCTL_RPC_RAIL_DISABLE;
	return msm_proc_comm(cmd, &rail_id, NULL);
}

static struct platform_device msm_kgsl_device = {
	.name		= "kgsl",
	.id		= -1,
	.resource	= msm_kgsl_resources,
	.num_resources	= ARRAY_SIZE(msm_kgsl_resources),
};

static struct android_pmem_platform_data mdp_pmem_pdata = {
	.name		= "pmem",
	.start		= MSM_PMEM_MDP_BASE,
	.size		= MSM_PMEM_MDP_SIZE,
	.no_allocator	= 0,
	.cached		= 1,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name		= "pmem_adsp",
	.start		= MSM_PMEM_ADSP_BASE,
	.size		= MSM_PMEM_ADSP_SIZE,
	.no_allocator	= 0,
	.cached		= 1,
};

#ifdef CONFIG_720P_CAMERA
static struct android_pmem_platform_data android_pmem_venc_pdata = {
	.name		= "pmem_venc",
	.start		= MSM_PMEM_VENC_BASE,
	.size		= MSM_PMEM_VENC_SIZE,
	.no_allocator	= 0,
	.cached		= 1,
};
#else
static struct android_pmem_platform_data android_pmem_camera_pdata = {
	.name		= "pmem_camera",
	.start		= MSM_PMEM_CAMERA_BASE,
	.size		= MSM_PMEM_CAMERA_SIZE,
	.no_allocator	= 1,
	.cached		= 1,
};
#endif

static struct platform_device android_pmem_mdp_device = {
	.name		= "android_pmem",
	.id		= 0,
	.dev		= {
		.platform_data = &mdp_pmem_pdata
	},
};

static struct platform_device android_pmem_adsp_device = {
	.name		= "android_pmem",
	.id		= 4,
	.dev		= {
		.platform_data = &android_pmem_adsp_pdata,
	},
};

#ifdef CONFIG_720P_CAMERA
static struct platform_device android_pmem_venc_device = {
	.name		= "android_pmem",
	.id		= 5,
	.dev		= {
		.platform_data = &android_pmem_venc_pdata,
	},
};
#else
static struct platform_device android_pmem_camera_device = {
	.name		= "android_pmem",
	.id		= 5,
	.dev		= {
		.platform_data = &android_pmem_camera_pdata,
	},
};

#endif

static struct resource ram_console_resources[] = {
	{
		.start	= MSM_RAM_CONSOLE_BASE,
		.end	= MSM_RAM_CONSOLE_BASE + MSM_RAM_CONSOLE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ram_console_device = {
	.name		= "ram_console",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ram_console_resources),
	.resource	= ram_console_resources,
};

static int incrediblec_atmel_ts_power(int on)
{
	printk(KERN_INFO "incrediblec_atmel_ts_power(%d)\n", on);
	if (on) {
		gpio_set_value(INCREDIBLEC_GPIO_TP_EN, 1);
		msleep(2);
		gpio_set_value(INCREDIBLEC_GPIO_TP_RST, 1);
	} else {
		gpio_set_value(INCREDIBLEC_GPIO_TP_EN, 0);
		msleep(2);
	}
	return 0;
}

struct atmel_i2c_platform_data incrediblec_atmel_ts_data[] = {
	{
		.version = 0x016,
		.abs_x_min = 1,
		.abs_x_max = 1023,
		.abs_y_min = 2,
		.abs_y_max = 966,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = INCREDIBLEC_GPIO_TP_INT_N,
		.power = incrediblec_atmel_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {50, 15, 25},
		.config_T8 = {10, 0, 20, 10, 0, 0, 5, 15},
		.config_T9 = {139, 0, 0, 18, 12, 0, 16, 38, 3, 7, 0, 5, 2, 15, 2, 10, 25, 5, 0, 0, 0, 0, 0, 0, 0, 0, 159, 47, 149, 81, 40},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {15, 0, 0, 0, 0, 0, 0, 0, 16, 0, 1, 0, 7, 18, 25, 30, 0},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 200, 50, 64, 31, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 2, 4, 8, 60},
		.object_crc = {0xDB, 0xBF, 0x60},
		.cable_config = {35, 30, 8, 16},
		.GCAF_level = {20, 24, 28, 40, 63},
		.filter_level = {15, 60, 963, 1008},
	},
	{
		.version = 0x015,
		.abs_x_min = 13,
		.abs_x_max = 1009,
		.abs_y_min = 15,
		.abs_y_max = 960,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = INCREDIBLEC_GPIO_TP_INT_N,
		.power = incrediblec_atmel_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {50, 15, 25},
		.config_T8 = {12, 0, 20, 20, 0, 0, 20, 0},
		.config_T9 = {139, 0, 0, 18, 12, 0, 32, 40, 2, 7, 0, 5, 2, 0, 2, 10, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 159, 47, 149, 81},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {7, 0, 0, 0, 0, 0, 0, 30, 20, 4, 15, 5},
		.config_T22 = {7, 0, 0, 25, 0, -25, 255, 4, 50, 0, 1, 10, 15, 20, 25, 30, 4},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 200, 50, 64, 31, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 2, 4, 8, 60},
		.object_crc = {0x19, 0x87, 0x7E},
	},
	{
		.version = 0x014,
		.abs_x_min = 13,
		.abs_x_max = 1009,
		.abs_y_min = 15,
		.abs_y_max = 960,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = INCREDIBLEC_GPIO_TP_INT_N,
		.power = incrediblec_atmel_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {50, 15, 25},
		.config_T8 = {12, 0, 20, 20, 0, 0, 10, 15},
		.config_T9 = {3, 0, 0, 18, 12, 0, 48, 45, 2, 7, 0, 0, 0, 0, 2, 10, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 143, 47, 143, 81},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {5, 0, 0, 25, 0, -25, 255, 4, 50, 0, 1, 10, 15, 20, 25, 30, 4},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 200, 50, 64, 31, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 2, 4, 8, 60},
	}
};

static struct regulator_consumer_supply tps65023_dcdc1_supplies[] = {
	{
		.supply = "acpu_vcore",
	},
};

static struct regulator_init_data tps65023_data[5] = {
	{
		.constraints = {
			.name = "dcdc1", /* VREG_MSMC2_1V29 */
			.min_uV = 1000000,
			.max_uV = 1300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
		},
		.consumer_supplies = tps65023_dcdc1_supplies,
		.num_consumer_supplies = ARRAY_SIZE(tps65023_dcdc1_supplies),
	},
	/* dummy values for unused regulators to not crash driver: */
	{
		.constraints = {
			.name = "dcdc2", /* VREG_MSMC1_1V26 */
			.min_uV = 1260000,
			.max_uV = 1260000,
		},
	},
	{
		.constraints = {
			.name = "dcdc3", /* unused */
			.min_uV = 800000,
			.max_uV = 3300000,
		},
	},
	{
		.constraints = {
			.name = "ldo1", /* unused */
			.min_uV = 1000000,
			.max_uV = 3150000,
		},
	},
	{
		.constraints = {
			.name = "ldo2", /* V_USBPHY_3V3 */
			.min_uV = 3300000,
			.max_uV = 3300000,
		},
	},
};

static void set_h2w_dat(int n)
{
	gpio_set_value(INCREDIBLEC_GPIO_H2W_DATA, n);
}

static void set_h2w_clk(int n)
{
	gpio_set_value(INCREDIBLEC_GPIO_H2W_CLK, n);
}

static int get_h2w_dat(void)
{
	return gpio_get_value(INCREDIBLEC_GPIO_H2W_DATA);
}

static int get_h2w_clk(void)
{
	return gpio_get_value(INCREDIBLEC_GPIO_H2W_CLK);
}

static void h2w_dev_power_on(int on)
{
	printk(KERN_INFO "Not support H2W power\n");
}

/* default TX,RX to GPI */
static uint32_t uart3_off_gpi_table[] = {
	/* RX, H2W DATA */
	PCOM_GPIO_CFG(INCREDIBLEC_GPIO_H2W_DATA, 0,
		      GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
	/* TX, H2W CLK */
	PCOM_GPIO_CFG(INCREDIBLEC_GPIO_H2W_CLK, 0,
		      GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
};

/* set TX,RX to GPO */
static uint32_t uart3_off_gpo_table[] = {
	/* RX, H2W DATA */
	PCOM_GPIO_CFG(INCREDIBLEC_GPIO_H2W_DATA, 0,
		      GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	/* TX, H2W CLK */
	PCOM_GPIO_CFG(INCREDIBLEC_GPIO_H2W_CLK, 0,
		      GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
};

static void set_h2w_dat_dir(int n)
{
#if 0
	if (n == 0) /* input */
		gpio_direction_input(INCREDIBLEC_GPIO_H2W_DATA);
	else
		gpio_configure(INCREDIBLEC_GPIO_H2W_DATA, GPIOF_DRIVE_OUTPUT);
#else
	if (n == 0) /* input */
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX,
			      uart3_off_gpi_table + 0, 0);
	else
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX,
			      uart3_off_gpo_table + 0, 0);
#endif
}

static void set_h2w_clk_dir(int n)
{
#if 0
	if (n == 0) /* input */
		gpio_direction_input(INCREDIBLEC_GPIO_H2W_CLK);
	else
		gpio_configure(INCREDIBLEC_GPIO_H2W_CLK, GPIOF_DRIVE_OUTPUT);
#else
	if (n == 0) /* input */
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX,
			      uart3_off_gpi_table + 1, 0);
	else
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX,
			      uart3_off_gpo_table + 1, 0);
#endif
}


static void incrediblec_config_serial_debug_gpios(void);

static void h2w_configure(int route)
{
	printk(KERN_INFO "H2W route = %d \n", route);
	switch (route) {
	case H2W_UART3:
		incrediblec_config_serial_debug_gpios();
		printk(KERN_INFO "H2W -> UART3\n");
		break;
	case H2W_GPIO:
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX,
			      uart3_off_gpi_table + 0, 0);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX,
			      uart3_off_gpi_table + 1, 0);
		printk(KERN_INFO "H2W -> GPIO\n");
		break;
	}
}

static struct htc_headset_mgr_platform_data htc_headset_mgr_data = {
};

static struct platform_device htc_headset_mgr = {
	.name	= "HTC_HEADSET_MGR",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_mgr_data,
	},
};

static struct htc_headset_gpio_platform_data htc_headset_gpio_data = {
	.hpin_gpio		= INCREDIBLEC_GPIO_35MM_HEADSET_DET,
	.key_enable_gpio	= NULL,
	.mic_select_gpio	= NULL,
};

static struct platform_device htc_headset_gpio = {
	.name	= "HTC_HEADSET_GPIO",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_gpio_data,
	},
};

static struct akm8973_platform_data compass_platform_data = {
	.layouts = INCREDIBLEC_LAYOUTS,
	.project_name = INCREDIBLEC_PROJECT_NAME,
	.reset = INCREDIBLEC_GPIO_COMPASS_RST_N,
	.intr = INCREDIBLEC_GPIO_COMPASS_INT_N,
};

static struct tpa6130_platform_data headset_amp_platform_data = {
	.enable_rpc_server = 0,
};

static struct i2c_board_info i2c_devices[] = {
	{
		I2C_BOARD_INFO(ATMEL_QT602240_NAME, 0x94 >> 1),
		.platform_data = &incrediblec_atmel_ts_data,
		.irq = MSM_GPIO_TO_INT(INCREDIBLEC_GPIO_TP_INT_N)
	},
	{
		I2C_BOARD_INFO(MICROP_I2C_NAME, 0xCC >> 1),
		.platform_data = &microp_data,
		.irq = MSM_GPIO_TO_INT(INCREDIBLEC_GPIO_UP_INT_N)
	},
	{
		I2C_BOARD_INFO("ds2482", 0x30 >> 1),
		/*.platform_data = &microp_data,*/
		/*.irq = MSM_GPIO_TO_INT(PASSION_GPIO_UP_INT_N)*/
	},
	{
		I2C_BOARD_INFO("smb329", 0x6E >> 1),
	},
	{
		I2C_BOARD_INFO("akm8973", 0x1C),
		.platform_data = &compass_platform_data,
		.irq = MSM_GPIO_TO_INT(INCREDIBLEC_GPIO_COMPASS_INT_N),
	},
#ifdef CONFIG_MSM_CAMERA
#ifdef CONFIG_OV8810
	{
		I2C_BOARD_INFO("ov8810", 0x6C >> 1),
	},
#endif
#endif/*CONIFIG_MSM_CAMERA*/

	{
		I2C_BOARD_INFO(TPA6130_I2C_NAME, 0xC0 >> 1),
		.platform_data = &headset_amp_platform_data,
	},
	{
		I2C_BOARD_INFO("tps65023", 0x48),
		.platform_data = tps65023_data,
	},
};

#ifdef CONFIG_ARCH_QSD8X50
static char bdaddress[20];

static void bt_export_bd_address(void)
 {
	unsigned char cTemp[6];

	memcpy(cTemp, get_bt_bd_ram(), 6);
	sprintf(bdaddress, "%02x:%02x:%02x:%02x:%02x:%02x", cTemp[0], cTemp[1], cTemp[2], cTemp[3], cTemp[4], cTemp[5]);
	printk(KERN_INFO "YoYo--BD_ADDRESS=%s\n", bdaddress);
}

module_param_string(bdaddress, bdaddress, sizeof(bdaddress), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bdaddress, "BT MAC ADDRESS");
#endif

static uint32_t camera_off_gpio_table[] = {

#if 0	/* CAMERA OFF*/
	PCOM_GPIO_CFG(0, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT0 */
	PCOM_GPIO_CFG(1, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT1 */
	PCOM_GPIO_CFG(2, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT2 */
	PCOM_GPIO_CFG(3, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT3 */
	PCOM_GPIO_CFG(4, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT4 */
	PCOM_GPIO_CFG(5, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT5 */
	PCOM_GPIO_CFG(6, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT6 */
	PCOM_GPIO_CFG(7, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT7 */
	PCOM_GPIO_CFG(8, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT8 */
	PCOM_GPIO_CFG(9, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT9 */
	PCOM_GPIO_CFG(10, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT10 */
	PCOM_GPIO_CFG(11, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT11 */
	PCOM_GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* PCLK */
	PCOM_GPIO_CFG(13, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* HSYNC */
	PCOM_GPIO_CFG(14, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* VSYNC */
	PCOM_GPIO_CFG(15, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* MCLK */
#endif
	/* CAMERA SUSPEND*/
	PCOM_GPIO_CFG(0, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT0 */
	PCOM_GPIO_CFG(1, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT1 */
	PCOM_GPIO_CFG(2, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT2 */
	PCOM_GPIO_CFG(3, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT3 */
	PCOM_GPIO_CFG(4, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT4 */
	PCOM_GPIO_CFG(5, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT5 */
	PCOM_GPIO_CFG(6, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT6 */
	PCOM_GPIO_CFG(7, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT7 */
	PCOM_GPIO_CFG(8, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT8 */
	PCOM_GPIO_CFG(9, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT9 */
	PCOM_GPIO_CFG(10, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT10 */
	PCOM_GPIO_CFG(11, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT11 */
	PCOM_GPIO_CFG(12, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* PCLK */
	PCOM_GPIO_CFG(13, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* HSYNC */
	PCOM_GPIO_CFG(14, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* VSYNC */
	PCOM_GPIO_CFG(15, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* MCLK */
	PCOM_GPIO_CFG(99, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* CAM1_RST */
	PCOM_GPIO_CFG(INCREDIBLEC_CAM_PWD,
		0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* CAM1_PWD */
};

static uint32_t camera_on_gpio_table[] = {
	/* CAMERA ON */
	PCOM_GPIO_CFG(0, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT0 */
	PCOM_GPIO_CFG(1, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT1 */
	PCOM_GPIO_CFG(2, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT2 */
	PCOM_GPIO_CFG(3, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT3 */
	PCOM_GPIO_CFG(4, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT4 */
	PCOM_GPIO_CFG(5, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT5 */
	PCOM_GPIO_CFG(6, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT6 */
	PCOM_GPIO_CFG(7, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT7 */
	PCOM_GPIO_CFG(8, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT8 */
	PCOM_GPIO_CFG(9, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT9 */
	PCOM_GPIO_CFG(10, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT10 */
	PCOM_GPIO_CFG(11, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* DAT11 */
	PCOM_GPIO_CFG(12, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_16MA), /* PCLK */
	PCOM_GPIO_CFG(13, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* HSYNC */
	PCOM_GPIO_CFG(14, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* VSYNC */
	PCOM_GPIO_CFG(15, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_16MA), /* MCLK */
};

static void config_camera_on_gpios(void)
{
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
}

static void config_camera_off_gpios(void)
{
	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
}

static struct resource msm_camera_resources[] = {
	{
		.start	= MSM_VFE_PHYS,
		.end	= MSM_VFE_PHYS + MSM_VFE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_VFE,
		 INT_VFE,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.ioext.mdcphy = MSM_MDC_PHYS,
	.ioext.mdcsz  = MSM_MDC_SIZE,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
};

static int flashlight_control(int mode)
{
	return aat1271_flashlight_control(mode);
}

static struct camera_flash_cfg msm_camera_sensor_flash_cfg = {
	.camera_flash		= flashlight_control,
	.num_flash_levels	= FLASHLIGHT_NUM,
	.low_temp_limit		= 10,
	.low_cap_limit		= 15,
};

static struct msm_camera_sensor_info msm_camera_sensor_ov8810_data = {
	.sensor_name    = "ov8810",
	.sensor_reset   = INCREDIBLEC_CAM_RST, /* CAM1_RST */
	.sensor_pwd     = INCREDIBLEC_CAM_PWD,  /* CAM1_PWDN, enabled in a9 */
	.pdata		= &msm_camera_device_data,
	.resource	= msm_camera_resources,
	.num_resources	= ARRAY_SIZE(msm_camera_resources),
	.waked_up	= 0,
	.need_suspend	= 0,
	.flash_cfg	= &msm_camera_sensor_flash_cfg,
};

static struct platform_device msm_camera_sensor_ov8810 = {
    .name           = "msm_camera_ov8810",
    .dev            = {
    .platform_data = &msm_camera_sensor_ov8810_data,
    },
};

static void config_incrediblec_flashlight_gpios(void)
{
	static uint32_t flashlight_gpio_table[] = {
		PCOM_GPIO_CFG(INCREDIBLEC_GPIO_FLASHLIGHT_TORCH, 0,
					GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		PCOM_GPIO_CFG(INCREDIBLEC_GPIO_FLASHLIGHT_FLASH, 0,
					GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		PCOM_GPIO_CFG(INCREDIBLEC_GPIO_FLASHLIGHT_FLASH_ADJ, 0,
					GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	};

	config_gpio_table(flashlight_gpio_table,
		ARRAY_SIZE(flashlight_gpio_table));
}

static struct flashlight_platform_data incrediblec_flashlight_data = {
	.gpio_init  = config_incrediblec_flashlight_gpios,
	.torch = INCREDIBLEC_GPIO_FLASHLIGHT_TORCH,
	.flash = INCREDIBLEC_GPIO_FLASHLIGHT_FLASH,
	.flash_adj = INCREDIBLEC_GPIO_FLASHLIGHT_FLASH_ADJ,
	.flash_duration_ms = 600,
	.led_count = 1,
};

static struct platform_device incrediblec_flashlight_device = {
	.name = "flashlight",
	.dev		= {
		.platform_data	= &incrediblec_flashlight_data,
	},
};

static void curcial_oj_shutdown(int enable)
{
	uint8_t cmd[3];
	memset(cmd, 0, sizeof(uint8_t)*3);

	cmd[2] = 0x80;
	if (enable)
		microp_i2c_write(0x91, cmd, 3);
	else
		microp_i2c_write(0x90, cmd, 3);
}

static int curcial_oj_poweron(int on)
{
	struct vreg *oj_power = vreg_get(0, "synt");
	if (IS_ERR(oj_power)) {
		printk(KERN_ERR "%s: Error power domain\n", __func__);
		return 0;
	}

	if (on) {
		vreg_set_level(oj_power, 2750);
		vreg_enable(oj_power);
	} else
		vreg_disable(oj_power);

	printk(KERN_INFO "%s: OJ power enable(%d)\n", __func__, on);
	return 1;
};
static void curcial_oj_adjust_xy(uint8_t *data, int16_t *mSumDeltaX, int16_t *mSumDeltaY)
{
	int8_t 	deltaX;
	int8_t 	deltaY;


	if (data[2] == 0x80)
		data[2] = 0x81;
	if (data[1] == 0x80)
		data[1] = 0x81;
	if (0) {
		deltaX = (1)*((int8_t) data[2]); /*X=2*/
		deltaY = (1)*((int8_t) data[1]); /*Y=1*/
	} else {
		deltaX = (1)*((int8_t) data[1]);
		deltaY = (1)*((int8_t) data[2]);
	}
	*mSumDeltaX += -((int16_t)deltaX);
	*mSumDeltaY += -((int16_t)deltaY);
}
static struct curcial_oj_platform_data incrediblec_oj_data = {
	.oj_poweron	= curcial_oj_poweron,
	.oj_shutdown	= curcial_oj_shutdown,
	.oj_adjust_xy = curcial_oj_adjust_xy,
	.microp_version	= INCREDIBLEC_MICROP_VER,
	.debugflag = 0,
	.mdelay_time = 0,
	.normal_th = 8,
	.xy_ratio = 15,
	.interval = 20,
	.swap		= true,
	.ap_code = false,
	.x 		= 1,
	.y		= 1,
	.share_power	= true,
	.Xsteps = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9},
	.Ysteps = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9},
	.sht_tbl = {0, 2000, 2250, 2500, 2750, 3000},
	.pxsum_tbl = {0, 0, 40, 50, 60, 70},
	.degree = 6,
	.irq = MSM_uP_TO_INT(12),
};

static struct platform_device incrediblec_oj = {
	.name = CURCIAL_OJ_NAME,
	.id = -1,
	.dev = {
		.platform_data	= &incrediblec_oj_data,
	}
};

static int amoled_power(int on)
{
	static struct vreg *vreg_lcm_2v6;
	if (!vreg_lcm_2v6) {
		vreg_lcm_2v6 = vreg_get(0, "gp1");
		if (IS_ERR(vreg_lcm_2v6))
			return -EINVAL;
	}

	if (on) {
		unsigned id, on = 1;

		id = PM_VREG_PDOWN_CAM_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on, &id);
		vreg_enable(vreg_lcm_2v6);

		gpio_set_value(INCREDIBLEC_LCD_RST_ID1, 1);
		mdelay(25);
		gpio_set_value(INCREDIBLEC_LCD_RST_ID1, 0);
		mdelay(10);
		gpio_set_value(INCREDIBLEC_LCD_RST_ID1, 1);
		mdelay(20);
	} else {
		unsigned id, on = 0;

		gpio_set_value(INCREDIBLEC_LCD_RST_ID1, 0);

		id = PM_VREG_PDOWN_CAM_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on, &id);
		vreg_disable(vreg_lcm_2v6);
	}
	return 0;
}

static int sonywvga_power(int on)
{
	unsigned id, on_off;
	static struct vreg *vreg_lcm_2v6;
	if (!vreg_lcm_2v6) {
		vreg_lcm_2v6 = vreg_get(0, "gp1");
		if (IS_ERR(vreg_lcm_2v6))
			return -EINVAL;
	}

	if (on) {
		on_off = 0;

		id = PM_VREG_PDOWN_CAM_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on, &id);
		vreg_enable(vreg_lcm_2v6);

		gpio_set_value(INCREDIBLEC_LCD_RST_ID1, 1);
		mdelay(10);
		gpio_set_value(INCREDIBLEC_LCD_RST_ID1, 0);
		udelay(500);
		gpio_set_value(INCREDIBLEC_LCD_RST_ID1, 1);
		mdelay(10);
	} else {
		on_off = 1;

		gpio_set_value(INCREDIBLEC_LCD_RST_ID1, 0);

		mdelay(120);

		id = PM_VREG_PDOWN_CAM_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on, &id);
		vreg_disable(vreg_lcm_2v6);
	}
	return 0;
}

#define LCM_GPIO_CFG(gpio, func) \
PCOM_GPIO_CFG(gpio, func, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA)
static uint32_t display_on_gpio_table[] = {
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R0, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R1, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R2, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R3, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R4, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R5, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G0, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G1, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G2, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G3, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G4, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G5, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B0, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B1, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B2, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B3, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B4, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B5, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_PCLK, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_VSYNC, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_HSYNC, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_DE, 1),
};

static uint32_t display_off_gpio_table[] = {
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R0, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R1, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R2, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R3, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R4, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_R5, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G0, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G1, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G2, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G3, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G4, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_G5, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B0, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B1, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B2, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B3, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B4, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_B5, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_PCLK, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_VSYNC, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_HSYNC, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_DE, 0),
};

static uint32_t sony_display_on_gpio_table[] = {
	LCM_GPIO_CFG(INCREDIBLEC_SPI_CLK, 1),
	LCM_GPIO_CFG(INCREDIBLEC_SPI_CS, 1),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_ID0, 1),
        LCM_GPIO_CFG(INCREDIBLEC_SPI_DO, 1),
};

static uint32_t sony_display_off_gpio_table[] = {
	LCM_GPIO_CFG(INCREDIBLEC_SPI_CLK, 0),
	LCM_GPIO_CFG(INCREDIBLEC_SPI_CS, 0),
	LCM_GPIO_CFG(INCREDIBLEC_LCD_ID0, 0),
        LCM_GPIO_CFG(INCREDIBLEC_SPI_DO, 0),
};

static int panel_gpio_switch(int on)
{
	if (on) {
		config_gpio_table(display_on_gpio_table,
			ARRAY_SIZE(display_on_gpio_table));

		if(panel_type != SAMSUNG_PANEL) {
			config_gpio_table(sony_display_on_gpio_table,
					ARRAY_SIZE(sony_display_on_gpio_table));
		}
	}
	else {
		int i;

		config_gpio_table(display_off_gpio_table,
			ARRAY_SIZE(display_off_gpio_table));

		for (i = INCREDIBLEC_LCD_R0; i <= INCREDIBLEC_LCD_R5; i++)
			gpio_set_value(i, 0);
		for (i = INCREDIBLEC_LCD_G0; i <= INCREDIBLEC_LCD_G5; i++)
			gpio_set_value(i, 0);
		for (i = INCREDIBLEC_LCD_B0; i <= INCREDIBLEC_LCD_DE; i++)
			gpio_set_value(i, 0);

		if(panel_type != SAMSUNG_PANEL) {
			config_gpio_table(sony_display_off_gpio_table,
					ARRAY_SIZE(sony_display_off_gpio_table));
		}
	}
	return 0;
}

static struct resource resources_msm_fb[] = {
	{
		.start = MSM_FB_BASE,
		.end = MSM_FB_BASE + MSM_FB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct panel_platform_data amoled_data = {
	.fb_res = &resources_msm_fb[0],
	.power = amoled_power,
	.gpio_switch = panel_gpio_switch,
};

static struct platform_device amoled_panel = {
	.name = "panel-tl2796a",
	.id = -1,
	.dev = {
		.platform_data = &amoled_data
	},
};

static struct panel_platform_data sonywvga_data = {
	.fb_res = &resources_msm_fb[0],
	.power = sonywvga_power,
	.gpio_switch = panel_gpio_switch,
};

static struct platform_device sonywvga_panel = {
	.name = "panel-sonywvga-s6d16a0x21",
	.id = -1,
	.dev = {
		.platform_data = &sonywvga_data,
	},
};
static struct platform_device *devices[] __initdata = {
	&msm_device_uart1,
#ifdef CONFIG_SERIAL_MSM_HS
	&msm_device_uart_dm1,
#endif
	&htc_battery_pdev,
	&htc_headset_mgr,
	&htc_headset_gpio,
	&ram_console_device,
	&incrediblec_rfkill,
	&msm_device_smd,
	&msm_device_nand,
	/*&msm_device_hsusb,*/
	/*&usb_mass_storage_device,*/
	&android_pmem_mdp_device,
	&android_pmem_adsp_device,
#ifdef CONFIG_720P_CAMERA
	&android_pmem_venc_device,
#else
	&android_pmem_camera_device,
#endif
	&msm_camera_sensor_ov8810,
	&msm_kgsl_device,
	&msm_device_i2c,
	&incrediblec_flashlight_device,
	&incrediblec_leds,

#if defined(CONFIG_SPI_QSD)
	&qsd_device_spi,
#endif
	&incrediblec_oj,
};

static uint32_t incrediblec_serial_debug_table[] = {
	/* RX */
	PCOM_GPIO_CFG(INCREDIBLEC_GPIO_UART3_RX, 3, GPIO_INPUT, GPIO_NO_PULL,
		      GPIO_4MA),
	/* TX */
	PCOM_GPIO_CFG(INCREDIBLEC_GPIO_UART3_TX, 3, GPIO_OUTPUT, GPIO_NO_PULL,
		      GPIO_4MA),
};

static uint32_t incrediblec_uart_gpio_table[] = {
        /* RX */
	PCOM_GPIO_CFG(INCREDIBLEC_GPIO_UART3_RX, 3, GPIO_INPUT, GPIO_NO_PULL,
                      GPIO_4MA),
	/* TX */
        PCOM_GPIO_CFG(INCREDIBLEC_GPIO_UART3_TX, 3, GPIO_INPUT, GPIO_NO_PULL,
                      GPIO_4MA),
};

static void incrediblec_config_serial_debug_gpios(void)
{
	config_gpio_table(incrediblec_serial_debug_table,
				ARRAY_SIZE(incrediblec_serial_debug_table));
}

static void incrediblec_config_uart_gpios(void)
{
        config_gpio_table(incrediblec_uart_gpio_table,
				ARRAY_SIZE(incrediblec_uart_gpio_table));
}

static struct msm_i2c_device_platform_data msm_i2c_pdata = {
	.i2c_clock = 100000,
	.clock_strength = GPIO_8MA,
	.data_strength = GPIO_8MA,
};

static void __init msm_device_i2c_init(void)
{
	msm_i2c_gpio_init();
	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}

static struct msm_acpu_clock_platform_data incrediblec_clock_data = {
	.acpu_switch_time_us	= 20,
	.max_speed_delta_khz	= 256000,
	.vdd_switch_time_us	= 62,
	.power_collapse_khz	= 245000,
	.wait_for_irq_khz	= 245000,
};

static unsigned incrediblec_perf_acpu_table[] = {
	245000000,
	576000000,
	998400000,
};

static struct perflock_platform_data incrediblec_perflock_data = {
	.perf_acpu_table = incrediblec_perf_acpu_table,
	.table_size = ARRAY_SIZE(incrediblec_perf_acpu_table),
};

int incrediblec_init_mmc(int sysrev);

#ifdef CONFIG_SERIAL_MSM_HS
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.rx_wakeup_irq = MSM_GPIO_TO_INT(INCREDIBLEC_GPIO_BT_HOST_WAKE),	/*Chip to Device*/
	.inject_rx_on_wakeup = 0,
	.cpu_lock_supported = 0,

	/* for bcm */
	.bt_wakeup_pin_supported = 1,
	.bt_wakeup_pin = INCREDIBLEC_GPIO_BT_CHIP_WAKE,
	.host_wakeup_pin = INCREDIBLEC_GPIO_BT_HOST_WAKE,

};
#endif

static int OJ_BMA_power(void)
{
	int ret;
	struct vreg *vreg = vreg_get(0, "synt");

	if (!vreg) {
		printk(KERN_ERR "%s: vreg error\n", __func__);
		return -EIO;
	}
	ret = vreg_set_level(vreg, 2850);

	ret = vreg_enable(vreg);
	if (ret < 0)
		printk(KERN_ERR "%s: vreg enable failed\n", __func__);

	return 0;
}

unsigned int incrediblec_get_engineerid(void)
{
	return engineerid;
}

static ssize_t incrediblec_virtual_keys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	if (engineerid > 1 && system_rev > 1) {
			/* center: x: home: 45, menu: 152, back: 318, search 422, y: 830 */
		return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_HOME)	    ":47:830:74:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_MENU)   ":155:830:80:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_BACK)   ":337:830:90:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":434:830:60:50"
			"\n");
	} else {
		/* center: x: home: 50, menu: 184, back: 315, search 435, y: 830*/
		return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_HOME)	    ":50:830:98:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_MENU)   ":184:830:120:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_BACK)   ":315:830:100:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":435:830:88:50"
			"\n");
	}

}

static struct kobj_attribute incrediblec_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.atmel-touchscreen",
		.mode = S_IRUGO,
	},
	.show = &incrediblec_virtual_keys_show,
};

static struct attribute *incrediblec_properties_attrs[] = {
	&incrediblec_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group incrediblec_properties_attr_group = {
	.attrs = incrediblec_properties_attrs,
};

static void incrediblec_reset(void)
{
       gpio_set_value(INCREDIBLEC_GPIO_PS_HOLD, 0);
}

static int incrediblec_init_panel(void)
{
	int ret = 0;

	if (panel_type != SAMSUNG_PANEL)
		ret = platform_device_register(&sonywvga_panel);
	else
		ret = platform_device_register(&amoled_panel);

	return ret;
}

static void __init incrediblec_init(void)
{
	int ret;
	struct kobject *properties_kobj;

	printk("incrediblec_init() revision=%d, engineerid=%d\n", system_rev, engineerid);

	 msm_hw_reset_hook = incrediblec_reset;

	if (0 == engineerid || 0xF == engineerid) {
		mdp_pmem_pdata.start = MSM_PMEM_MDP_XA_BASE;
		android_pmem_adsp_pdata.start = MSM_PMEM_ADSP_XA_BASE;
                msm_kgsl_resources[1].start = MSM_GPU_MEM_XA_BASE;
                msm_kgsl_resources[1].end = MSM_GPU_MEM_XA_BASE + MSM_GPU_MEM_SIZE - 1;
	} else if (engineerid >= 3) {
		mdp_pmem_pdata.start = MSM_PMEM_MDP_BASE + MSM_MEM_128MB_OFFSET;
		android_pmem_adsp_pdata.start = MSM_PMEM_ADSP_BASE + MSM_MEM_128MB_OFFSET;
		msm_kgsl_resources[1].start = MSM_GPU_MEM_BASE;
		msm_kgsl_resources[1].end =  msm_kgsl_resources[1].start + MSM_GPU_MEM_SIZE - 1;
	}

	OJ_BMA_power();

	msm_acpu_clock_init(&incrediblec_clock_data);

	perflock_init(&incrediblec_perflock_data);

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	msm_serial_debug_init(MSM_UART1_PHYS, INT_UART1,
				&msm_device_uart1.dev, 1, INT_UART1_RX);
#endif

#ifdef CONFIG_ARCH_QSD8X50
	bt_export_bd_address();
#endif
	/* set the gpu power rail to manual mode so clk en/dis will not
	 * turn off gpu power, and hang it on resume */
	incrediblec_kgsl_power_rail_mode(0);
	incrediblec_kgsl_power(true);

	#ifdef CONFIG_SERIAL_MSM_HS
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
	msm_device_uart_dm1.name = "msm_serial_hs_bcm";	/* for bcm */
	#endif

	incrediblec_config_uart_gpios();

	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
	/*gpio_direction_output(INCREDIBLEC_GPIO_TP_LS_EN, 0);*/
	gpio_direction_output(INCREDIBLEC_GPIO_TP_EN, 0);

	incrediblec_audio_init();
	msm_device_i2c_init();
#ifdef CONFIG_MICROP_COMMON
	incrediblec_microp_init();
#endif

#ifdef CONFIG_USB_ANDROID
	inc_add_usb_devices();
#endif

	if (system_rev >= 2) {
		microp_data.num_functions   = ARRAY_SIZE(microp_functions_1);
		microp_data.microp_function = microp_functions_1;
	}

	platform_add_devices(devices, ARRAY_SIZE(devices));
	incrediblec_init_panel();
	if (system_rev > 2) {
		incrediblec_atmel_ts_data[0].config_T9[7] = 33;
		incrediblec_atmel_ts_data[0].object_crc[0] = 0x2E;
		incrediblec_atmel_ts_data[0].object_crc[1] = 0x80;
		incrediblec_atmel_ts_data[0].object_crc[2] = 0xE0;
	}
	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));

	ret = incrediblec_init_mmc(system_rev);
	if (ret != 0)
		pr_crit("%s: Unable to initialize MMC\n", __func__);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&incrediblec_properties_attr_group);
	if (!properties_kobj || ret)
		pr_err("failed to create board_properties\n");

	msm_init_pmic_vibrator();

}

static void __init incrediblec_fixup(struct machine_desc *desc, struct tag *tags,
				 char **cmdline, struct meminfo *mi)
{
	engineerid = parse_tag_engineerid(tags);
	mi->nr_banks = 1;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].node = PHYS_TO_NID(PHYS_OFFSET);
	if (0 == engineerid || 0xF == engineerid)
		mi->bank[0].size = (MSM_LINUX_XA_SIZE);
	else if (engineerid <= 2) { /* 4G3G */
		mi->bank[0].size = MSM_EBI1_BANK0_SIZE;
		mi->nr_banks++;
		mi->bank[1].start = MSM_EBI1_BANK1_BASE;
		mi->bank[1].node = PHYS_TO_NID(MSM_EBI1_BANK1_BASE);
		mi->bank[1].size = MSM_EBI1_BANK1_SIZE;
	} else {
		mi->bank[0].size = MSM_EBI1_BANK0_SIZE;
		mi->nr_banks++;
		mi->bank[1].start = MSM_EBI1_BANK1_BASE;
		mi->bank[1].node = PHYS_TO_NID(MSM_EBI1_BANK1_BASE);
		mi->bank[1].size = MSM_EBI1_BANK1_SIZE + MSM_MEM_128MB_OFFSET;
	}
}

static void __init incrediblec_map_io(void)
{
	msm_map_common_io();
	msm_clock_init();
}

extern struct sys_timer msm_timer;

MACHINE_START(INCREDIBLEC, "incrediblec")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x20000100,
	.fixup		= incrediblec_fixup,
	.map_io		= incrediblec_map_io,
	.init_irq	= msm_init_irq,
	.init_machine	= incrediblec_init,
	.timer		= &msm_timer,
MACHINE_END
