/* linux/arch/arm/mach-msm/board-supersonic.c
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

#include <linux/cy8c_tmg_ts.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-msm.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/synaptics_t1007.h>
#include <linux/input.h>
#include <linux/akm8973.h>
#include <mach/tpa2018d1.h>
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
#include <mach/camera.h>
#include <mach/msm_iomap.h>
#include <mach/htc_battery.h>
#include <mach/htc_usb.h>
#include <mach/perflock.h>
#include <mach/msm_serial_debugger.h>
#include <mach/system.h>
#include <linux/spi/spi.h>

#include "board-supersonic.h"
#include "devices.h"
#include "proc_comm.h"
#include "smd_private.h"
#include <mach/msm_serial_hs.h>
#include <mach/tpa6130.h>
#include <mach/msm_flashlight.h>
#include <linux/atmel_qt602240.h>
#include <mach/vreg.h>
#ifdef CONFIG_MICROP_COMMON
#include <mach/atmega_microp.h>
#endif

#include <mach/msm_hdmi.h>
#include <mach/msm_hsusb.h>

#define SMEM_SPINLOCK_I2C      6

#ifdef CONFIG_ARCH_QSD8X50
extern unsigned char *get_bt_bd_ram(void);
#endif

static unsigned skuid;

static uint opt_usb_h2w_sw;
module_param_named(usb_h2w_sw, opt_usb_h2w_sw, uint, 0);

void msm_init_pmic_vibrator(void);
extern void __init supersonic_audio_init(void);
#ifdef CONFIG_MICROP_COMMON
void __init supersonic_microp_init(void);
#endif
static struct htc_battery_platform_data htc_battery_pdev_data = {
	.gpio_mbat_in = SUPERSONIC_GPIO_MBAT_IN,
	.gpio_mchg_en_n = SUPERSONIC_GPIO_MCHG_EN_N,
	.gpio_iset = SUPERSONIC_GPIO_ISET,
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

#ifdef CONFIG_MICROP_COMMON
static int capella_cm3602_power(int pwr_device, uint8_t enable);
static struct microp_function_config microp_functions[] = {
	{
		.name   = "reset-int",
		.category = MICROP_FUNCTION_RESET_INT,
		.int_pin = 1 << 8,
	},
};

static struct microp_function_config microp_lightsensor = {
	.name = "light_sensor",
	.category = MICROP_FUNCTION_LSENSOR,
	.levels = { 3, 7, 12, 57, 114, 279, 366, 453, 540, 0x3FF },
	.channel = 3,
	.int_pin = 1 << 9,
	.golden_adc = 0x118,
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
	{
		.name = "wimax",
		.type = LED_WIMAX,
	},
};

static struct microp_led_platform_data microp_leds_data = {
	.num_leds	= ARRAY_SIZE(led_config),
	.led_config	= led_config,
};

static struct bma150_platform_data supersonic_g_sensor_pdata = {
	.microp_new_cmd = 1,
};

/* Proximity Sensor (Capella_CM3602)*/
static int __capella_cm3602_power(int on)
{
	int ret;
	struct vreg *vreg = vreg_get(0, "gp1");;
	if (!vreg) {
		printk(KERN_ERR "%s: vreg error\n", __func__);
		return -EIO;
	}
	ret = vreg_set_level(vreg, 2800);

	printk(KERN_DEBUG "%s: Turn the capella_cm3602 power %s\n",
		__func__, (on) ? "on" : "off");
	if (on) {
		gpio_direction_output(SUPERSONIC_GPIO_PROXIMITY_EN_N, 1);
		ret = vreg_enable(vreg);
		if (ret < 0)
			printk(KERN_ERR "%s: vreg enable failed\n", __func__);
	} else {
		ret = vreg_disable(vreg);
		if (ret < 0)
			printk(KERN_ERR "%s: vreg disable failed\n", __func__);
		gpio_direction_output(SUPERSONIC_GPIO_PROXIMITY_EN_N, 0);
	}

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
	.p_en = SUPERSONIC_GPIO_PROXIMITY_EN_N,
	.p_out = MSM_uP_TO_INT(4),
};
/* End Proximity Sensor (Capella_CM3602)*/

static struct htc_headset_microp_platform_data htc_headset_microp_data = {
	.remote_int		= 1 << 7,
	.remote_irq		= MSM_uP_TO_INT(7),
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
			.platform_data = &supersonic_g_sensor_pdata,
		},
	},
	{
		.name = "supersonic_proximity",
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
	.gpio_reset = SUPERSONIC_GPIO_UP_RESET_N,
	.microp_ls_on = LS_PWR_ON | PS_PWR_ON,
	.spi_devices = SPI_GSENSOR,
};
#endif

static struct gpio_led supersonic_led_list[] = {
	{
		.name = "button-backlight",
		.gpio = SUPERSONIC_AP_KEY_LED_EN,
		.active_low = 0,
	},
};

static struct gpio_led_platform_data supersonic_leds_data = {
	.num_leds	= ARRAY_SIZE(supersonic_led_list),
	.leds		= supersonic_led_list,
};

static struct platform_device supersonic_leds = {
	.name		= "leds-gpio",
	.id		= -1,
	.dev		= {
		.platform_data	= &supersonic_leds_data,
	},
};

/* 2 : wimax UART, 1 : CPU uart, 0 : usb
CPU_WIMAX_SW -> GPIO160
USB_UART#_SW -> GPIO33

XA : GPIO33 = 0 -> USB
    GPIO33 = 1 -> CPU UART

XB : GPIO33 = 0 -> USB
    GPIO33 = 1 , GPIO160 = 0 -> CPU UART     // SUPERSONIC_WIMAX_CPU_UARTz_SW (GPIO160)
    GPIO33 = 1 , GPIO160 = 1 -> Wimax UART   // SUPERSONIC_USB_UARTz_SW (GPIO33)
*/

/*
 * USB cable out: supersonic_uart_usb_switch(1)
 * USB cable in: supersonic_uart_usb_switch(0)
 */
static void supersonic_uart_usb_switch(int uart)
{
	printk(KERN_INFO "%s:uart:%d\n", __func__, uart); 
	/* XA and for USB cable in to reset wimax UART */
	gpio_set_value(SUPERSONIC_USB_UARTz_SW, uart?1:0);

	if (system_rev && uart) {/* XB */
		/* Winmax uart */
		if (gpio_get_value(SUPERSONIC_WIMAX_CPU_UARTz_SW)) {
			printk(KERN_INFO "%s:Wimax UART\n", __func__);
			gpio_set_value(SUPERSONIC_USB_UARTz_SW,1);
			gpio_set_value(SUPERSONIC_WIMAX_CPU_UARTz_SW,1);
		} else {/* USB, CPU UART */
			printk(KERN_INFO "%s:Non wimax UART\n", __func__);
			gpio_set_value(SUPERSONIC_WIMAX_CPU_UARTz_SW, uart==2?1:0);
		}
	}
}

static uint32_t usb_phy_3v3_table[] = {
	PCOM_GPIO_CFG(SUPERSONIC_USB_PHY_3V3_ENABLE, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA)
};

static uint32_t usb_ID_PIN_input_table[] = {
	PCOM_GPIO_CFG(SUPERSONIC_GPIO_USB_ID_PIN, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_4MA),
};

static uint32_t usb_ID_PIN_ouput_table[] = {
	PCOM_GPIO_CFG(SUPERSONIC_GPIO_USB_ID_PIN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA),
};

void config_supersonic_usb_id_gpios(bool output)
{
	if (output) {
		config_gpio_table(usb_ID_PIN_ouput_table,
			ARRAY_SIZE(usb_ID_PIN_ouput_table));
		gpio_set_value(SUPERSONIC_GPIO_USB_ID_PIN, 1);
		printk(KERN_INFO "%s %d output high\n",  __func__, SUPERSONIC_GPIO_USB_ID_PIN);
	} else {
		config_gpio_table(usb_ID_PIN_input_table,
			ARRAY_SIZE(usb_ID_PIN_input_table));
		printk(KERN_INFO "%s %d input none pull\n",  __func__, SUPERSONIC_GPIO_USB_ID_PIN);
	}
}

static int supersonic_phy_init_seq[] = { 0xC, 0x31, 0x30, 0x32, 0x1D, 0x0D, 0x1D, 0x10, -1 };
#ifdef CONFIG_USB_ANDROID
static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.phy_init_seq		= supersonic_phy_init_seq,
	.phy_reset		= msm_hsusb_8x50_phy_reset,
	.usb_id_pin_gpio =  SUPERSONIC_GPIO_USB_ID_PIN,
	.accessory_detect = 1, /* detect by ID pin gpio */
	.usb_uart_switch = supersonic_uart_usb_switch,
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
	.vendor		= "HTC",
	.product	= "Android Phone",
	.release	= 0x0100,
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
	.product_id	= 0x0c8d,
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
static void suc_add_usb_devices(void)
{
	android_usb_pdata.products[0].product_id =
		android_usb_pdata.product_id;
	android_usb_pdata.serial_number = board_serialno();
	msm_hsusb_pdata.serial_number = board_serialno();
	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata;
	config_supersonic_usb_id_gpios(0);
	config_gpio_table(usb_phy_3v3_table, ARRAY_SIZE(usb_phy_3v3_table));
	gpio_set_value(SUPERSONIC_USB_PHY_3V3_ENABLE, 1);
	platform_device_register(&msm_device_hsusb);
	platform_device_register(&usb_mass_storage_device);
	platform_device_register(&android_usb_device);
}
#endif

static struct platform_device supersonic_rfkill = {
	.name = "supersonic_rfkill",
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

static struct spi_platform_data supersonic_spi_pdata = {
	.clk_rate	= 1200000,
};

static struct platform_device qsd_device_spi = {
	.name           = "spi_qsd",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(qsd_spi_resources),
	.resource       = qsd_spi_resources,
	.dev		= {
		.platform_data = &supersonic_spi_pdata
	},
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
static int supersonic_kgsl_power_rail_mode(int follow_clk)
{
	int mode = follow_clk ? 0 : 1;
	int rail_id = PWR_RAIL_GRP_CLK;

	return msm_proc_comm(PCOM_CLKCTL_RPC_RAIL_CONTROL, &rail_id, &mode);
}

static int supersonic_kgsl_power(bool on)
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

static struct android_pmem_platform_data android_pmem_venc_pdata = {
	.name		= "pmem_venc",
	.start		= MSM_PMEM_VENC_BASE,
	.size		= MSM_PMEM_VENC_SIZE,
	.no_allocator	= 0,
	.cached		= 1,
};


#ifdef CONFIG_BUILD_CIQ
static struct android_pmem_platform_data android_pmem_ciq_pdata = {
	.name = "pmem_ciq",
	.start = MSM_PMEM_CIQ_BASE,
	.size = MSM_PMEM_CIQ_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_ciq1_pdata = {
	.name = "pmem_ciq1",
	.start = MSM_PMEM_CIQ1_BASE,
	.size = MSM_PMEM_CIQ1_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_ciq2_pdata = {
	.name = "pmem_ciq2",
	.start = MSM_PMEM_CIQ2_BASE,
	.size = MSM_PMEM_CIQ2_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_ciq3_pdata = {
	.name = "pmem_ciq3",
	.start = MSM_PMEM_CIQ3_BASE,
	.size = MSM_PMEM_CIQ3_SIZE,
	.no_allocator = 0,
	.cached = 0,
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

static struct platform_device android_pmem_venc_device = {
	.name		= "android_pmem",
	.id		= 6,
	.dev		= {
		.platform_data = &android_pmem_venc_pdata,
	},
};

#ifdef CONFIG_BUILD_CIQ
static struct platform_device android_pmem_ciq_device = {
	.name = "android_pmem",
	.id = 7,
	.dev = { .platform_data = &android_pmem_ciq_pdata },
};

static struct platform_device android_pmem_ciq1_device = {
	.name = "android_pmem",
	.id = 8,
	.dev = { .platform_data = &android_pmem_ciq1_pdata },
};

static struct platform_device android_pmem_ciq2_device = {
	.name = "android_pmem",
	.id = 9,
	.dev = { .platform_data = &android_pmem_ciq2_pdata },
};

static struct platform_device android_pmem_ciq3_device = {
	.name = "android_pmem",
	.id = 10,
	.dev = { .platform_data = &android_pmem_ciq3_pdata },
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

static int supersonic_atmel_ts_power(int on)
{
	printk(KERN_INFO "supersonic_atmel_ts_power(%d)\n", on);
	if (on) {
		gpio_set_value(SUPERSONIC_GPIO_TP_RST, 0);
		msleep(5);
		gpio_set_value(SUPERSONIC_GPIO_TP_EN, 1);
		msleep(5);
		gpio_set_value(SUPERSONIC_GPIO_TP_RST, 1);
		msleep(40);
	} else {
		gpio_set_value(SUPERSONIC_GPIO_TP_EN, 0);
		msleep(2);
	}
	return 0;
}

struct atmel_i2c_platform_data supersonic_atmel_ts_data[] = {
	{
		.version = 0x016,
		.abs_x_min = 34,
		.abs_x_max = 990,
		.abs_y_min = 15,
		.abs_y_max = 950,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = SUPERSONIC_GPIO_TP_INT_N,
		.power = supersonic_atmel_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {50, 15, 50},
		.config_T8 = {10, 0, 20, 10, 0, 0, 5, 0},
		.config_T9 = {139, 0, 0, 18, 12, 0, 16, 32, 3, 5, 0, 5, 2, 14, 2, 10, 25, 10, 0, 0, 0, 0, 0, 0, 0, 0, 143, 25, 146, 10, 20},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {15, 0, 0, 0, 0, 0, 0, 0, 14, 0, 1, 8, 12, 16, 30, 40, 0},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 200, 50, 64, 31, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 2, 4, 8, 60},
		.object_crc = {0x0D, 0x1C, 0x8E},
		.cable_config = {30, 30, 8, 16},
		.GCAF_level = {20, 24, 28, 40, 63},
		.filter_level = {46, 100, 923, 978},
	},
	{
		.version = 0x015,
		.abs_x_min = 10,
		.abs_x_max = 1012,
		.abs_y_min = 15,
		.abs_y_max = 960,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = SUPERSONIC_GPIO_TP_INT_N,
		.power = supersonic_atmel_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {100, 10, 50},
		.config_T8 = {8, 0, 50, 50, 0, 0, 50, 0},
		.config_T9 = {3, 0, 0, 18, 12, 0, 32, 40, 2, 5, 0, 0, 0, 0, 2, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 143, 47, 145, 81},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {7, 0, 0, 25, 0, -25, 255, 4, 50, 0, 1, 10, 15, 20, 25, 30, 4},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 200, 50, 64, 31, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 2, 4, 8, 60},
		.object_crc = {0x87, 0xAD, 0xF5},
	},
	{
		.version = 0x014,
		.abs_x_min = 10,
		.abs_x_max = 1012,
		.abs_y_min = 15,
		.abs_y_max = 960,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = SUPERSONIC_GPIO_TP_INT_N,
		.power = supersonic_atmel_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {100, 10, 50},
		.config_T8 = {8, 0, 50, 50, 0, 0, 50, 0},
		.config_T9 = {3, 0, 0, 18, 12, 0, 64, 45, 3, 5, 0, 0, 0, 0, 2, 10, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 143, 47, 143, 81},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {7, 0, 0, 25, 0, -25, 255, 4, 50, 0, 1, 10, 15, 20, 25, 30, 4},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 200, 50, 64, 31, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 2, 4, 8},
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
	.hpin_gpio		= SUPERSONIC_GPIO_35MM_HEADSET_DET,
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
	.layouts = SUPERSONIC_LAYOUTS,
	.project_name = SUPERSONIC_PROJECT_NAME,
	.reset = SUPERSONIC_GPIO_COMPASS_RST_N,
	.intr = SUPERSONIC_GPIO_COMPASS_INT_N,
};

static struct tpa2018d1_platform_data tpa2018_data = {
	.gpio_tpa2018_spk_en = SUPERSONIC_AUD_SPK_EN,
};

/*
 * HDMI platform data
 */

#if 1
#define HDMI_DBG(s...) printk("[hdmi]" s)
#else
#define HDMI_DBG(s...) do {} while (0)
#endif

static int hdmi_power(int on)
{
	HDMI_DBG("%s(%d)\n", __func__, on);

	switch(on) {
	/* Power on/off sequence for normal or D2 sleep mode */
	case 0:
		gpio_set_value(HDMI_RST, 0);
		msleep(2);
		gpio_set_value(V_HDMI_3V3_EN, 0);
		gpio_set_value(V_VGA_5V_SIL9022A_EN, 0);
		msleep(2);
		gpio_set_value(V_HDMI_1V2_EN, 0);
		break;
	case 1:
		gpio_set_value(V_HDMI_1V2_EN, 1);
		msleep(2);
		gpio_set_value(V_VGA_5V_SIL9022A_EN, 1);
		gpio_set_value(V_HDMI_3V3_EN, 1);
		msleep(2);
		gpio_set_value(HDMI_RST, 1);
		msleep(2);
		break;

	/* Power on/off sequence for D3 sleep mode */
	case 2:
		gpio_set_value(V_HDMI_3V3_EN, 0);
		break;
	case 3:
		gpio_set_value(HDMI_RST, 0);
		msleep(2);
		gpio_set_value(V_HDMI_3V3_EN, 1);
		gpio_set_value(V_VGA_5V_SIL9022A_EN, 1);
		msleep(50);
		gpio_set_value(HDMI_RST, 1);
		msleep(10);
		break;
	case 4:
		gpio_set_value(V_VGA_5V_SIL9022A_EN, 0);
		break;
	case 5:
		gpio_set_value(V_VGA_5V_SIL9022A_EN, 1);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static uint32_t hdmi_gpio_on_table[] = {
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R0, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R1, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R2, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R3, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R4, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),

	PCOM_GPIO_CFG(SUPERSONIC_LCD_G0, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G1, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G2, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G3, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G4, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G5, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),

	PCOM_GPIO_CFG(SUPERSONIC_LCD_B0, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_B1, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_B2, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_B3, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_B4, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),

	PCOM_GPIO_CFG(SUPERSONIC_LCD_PCLK, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_VSYNC, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_HSYNC, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_DE, 1, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
};

static uint32_t hdmi_gpio_off_table[] = {
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R0, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R1, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R2, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R3, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_R4, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),

	PCOM_GPIO_CFG(SUPERSONIC_LCD_G0, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G1, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G2, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G3, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G4, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_G5, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),

	PCOM_GPIO_CFG(SUPERSONIC_LCD_B0, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_B1, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_B2, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_B3, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_B4, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),

	PCOM_GPIO_CFG(SUPERSONIC_LCD_PCLK, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_VSYNC, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_HSYNC, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
	PCOM_GPIO_CFG(SUPERSONIC_LCD_DE, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_2MA),
};


static void suc_hdmi_gpio_on(void)
{
	HDMI_DBG("%s\n", __func__);

	config_gpio_table(hdmi_gpio_on_table, ARRAY_SIZE(hdmi_gpio_on_table));
}

static void suc_hdmi_gpio_off(void)
{
	int i = 0;

	HDMI_DBG("%s\n", __func__);
	config_gpio_table(hdmi_gpio_off_table, ARRAY_SIZE(hdmi_gpio_off_table));

	for (i = SUPERSONIC_LCD_R0; i <= SUPERSONIC_LCD_R4; i++)
		gpio_set_value(i, 0);
	for (i = SUPERSONIC_LCD_G0; i <= SUPERSONIC_LCD_G5; i++)
		gpio_set_value(i, 0);
	for (i = SUPERSONIC_LCD_B0; i <= SUPERSONIC_LCD_DE; i++)
		gpio_set_value(i, 0);
}

static struct hdmi_platform_data hdmi_device_data = {
	.hdmi_res = {
		.start = MSM_HDMI_FB_BASE,
		.end = MSM_HDMI_FB_BASE + MSM_HDMI_FB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	.power = hdmi_power,
	.hdmi_gpio_on = suc_hdmi_gpio_on,
	.hdmi_gpio_off = suc_hdmi_gpio_off,
};

static struct tpa6130_platform_data headset_amp_platform_data = {
	.enable_rpc_server = 0,
};

static struct i2c_board_info i2c_devices[] = {
	{
		I2C_BOARD_INFO(ATMEL_QT602240_NAME, 0x94 >> 1),
		.platform_data = &supersonic_atmel_ts_data,
		.irq = MSM_GPIO_TO_INT(SUPERSONIC_GPIO_TP_INT_N)
	},
#ifdef CONFIG_MICROP_COMMON
	{
		I2C_BOARD_INFO(MICROP_I2C_NAME, 0xCC >> 1),
		.platform_data = &microp_data,
		.irq = MSM_GPIO_TO_INT(SUPERSONIC_GPIO_UP_INT_N)
	},
#endif
	{
		I2C_BOARD_INFO("ds2482", 0x30 >> 1),
		/*.platform_data = &microp_data,*/
		/*.irq = MSM_GPIO_TO_INT(PASSION_GPIO_UP_INT_N)*/
	},
	{
		I2C_BOARD_INFO("smb329", 0x6E >> 1),
	},
	{
		I2C_BOARD_INFO("tps65200", 0xD4 >> 1),
	},
	{
		I2C_BOARD_INFO("akm8973", 0x1C),
		.platform_data = &compass_platform_data,
		.irq = MSM_GPIO_TO_INT(SUPERSONIC_GPIO_COMPASS_INT_N),
	},

	{
		I2C_BOARD_INFO("ov8810", 0x6C >> 1),
	},
	{
		I2C_BOARD_INFO("ov9665", 0x60 >> 1),
	},
	{
		I2C_BOARD_INFO(TPA6130_I2C_NAME, 0xC0 >> 1),
		.platform_data = &headset_amp_platform_data,
	},
	{
		I2C_BOARD_INFO("tps65023", 0x48),
		.platform_data = tps65023_data,
	},
	{
		I2C_BOARD_INFO("tpa2018d1", 0x58),
		.platform_data = &tpa2018_data,
	},
	{
		I2C_BOARD_INFO("SiL902x-hdmi", 0x76 >> 1),
		.platform_data = &hdmi_device_data,
		.irq = MSM_uP_TO_INT(1),
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
	PCOM_GPIO_CFG(100, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* CAM1_PWD */
};

static uint32_t camera_on_gpio_table[] = {
	/* CAMERA */
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

static void supersonic_ov8810_clk_switch(void){
	int rc = 0;
	pr_info("SuperSoinc: clk switch (supersonic)(ov8810)\n");
	rc = gpio_request(SUPERSONIC_CLK_SWITCH, "ov8810");
	if (rc < 0)
		pr_err("GPIO (%d) request fail\n", SUPERSONIC_CLK_SWITCH);
	else
		gpio_direction_output(SUPERSONIC_CLK_SWITCH, 0);
	gpio_free(SUPERSONIC_CLK_SWITCH);

	return;
}

static void supersonic_ov9665_clk_switch(void){
	int rc = 0;
	pr_info("SuperSoinc: Doing clk switch (supersonic)(ov9665)\n");
	rc = gpio_request(SUPERSONIC_CLK_SWITCH, "ov9665");
	if (rc < 0)
		pr_err("GPIO (%d) request fail\n", SUPERSONIC_CLK_SWITCH);
	else
		gpio_direction_output(SUPERSONIC_CLK_SWITCH, 1);
	gpio_free(SUPERSONIC_CLK_SWITCH);

	return;
}

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
	.sensor_reset   = SUPERSONIC_MAINCAM_RST, /* CAM1_RST */
	.sensor_pwd     = SUPERSONIC_MAINCAM_PWD,  /* CAM1_PWDN, enabled in a9 */
	.camera_clk_switch	= supersonic_ov8810_clk_switch,
	.pdata = &msm_camera_device_data,
	.resource = msm_camera_resources,
	.num_resources = ARRAY_SIZE(msm_camera_resources),
	.waked_up = 0,
	.need_suspend = 0,
	.flash_cfg	= &msm_camera_sensor_flash_cfg,
};

static struct platform_device msm_camera_sensor_ov8810 = {
    .name           = "msm_camera_ov8810",
    .dev            = {
    .platform_data = &msm_camera_sensor_ov8810_data,
    },
};

static struct msm_camera_sensor_info msm_camera_sensor_ov9665_data = {
	.sensor_name	= "ov9665",
	.sensor_reset	= SUPERSONIC_MAINCAM_RST,
	.sensor_pwd	= SUPERSONIC_2NDCAM_PWD,
	.camera_clk_switch	= supersonic_ov9665_clk_switch,
	.pdata		= &msm_camera_device_data,
	.resource = msm_camera_resources,
	.num_resources = ARRAY_SIZE(msm_camera_resources),
	.waked_up = 0,
	.need_suspend = 0,
};

static struct platform_device msm_camera_sensor_ov9665 = {
	.name	   = "msm_camera_ov9665",
	.dev	    = {
		.platform_data = &msm_camera_sensor_ov9665_data,
	},
};

static void config_supersonic_flashlight_gpios(void)
{
	static uint32_t flashlight_gpio_table[] = {
		PCOM_GPIO_CFG(SUPERSONIC_GPIO_FLASHLIGHT_TORCH, 0,
					GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		PCOM_GPIO_CFG(SUPERSONIC_GPIO_FLASHLIGHT_FLASH, 0,
					GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		PCOM_GPIO_CFG(SUPERSONIC_GPIO_FLASHLIGHT_FLASH_ADJ, 0,
					GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	};
	config_gpio_table(flashlight_gpio_table,
		ARRAY_SIZE(flashlight_gpio_table));
}

static struct flashlight_platform_data supersonic_flashlight_data = {
	.gpio_init  = config_supersonic_flashlight_gpios,
	.torch = SUPERSONIC_GPIO_FLASHLIGHT_TORCH,
	.flash = SUPERSONIC_GPIO_FLASHLIGHT_FLASH,
	.flash_adj = SUPERSONIC_GPIO_FLASHLIGHT_FLASH_ADJ,
	.flash_duration_ms = 600,
	.led_count = 1,
};

static struct platform_device supersonic_flashlight_device = {
	.name = FLASHLIGHT_NAME,
	.dev		= {
		.platform_data	= &supersonic_flashlight_data,
	},
};

static struct platform_device *devices[] __initdata = {
#ifndef CONFIG_MSM_SERIAL_DEBUGGER
	&msm_device_uart1,
#endif
#ifdef CONFIG_SERIAL_MSM_HS
	&msm_device_uart_dm1,
#endif
	&htc_battery_pdev,
	&htc_headset_mgr,
	&htc_headset_gpio,
	&ram_console_device,
	&supersonic_rfkill,
	&msm_device_smd,
	&msm_device_nand,
	/*&msm_device_hsusb,*/
	/*&usb_mass_storage_device,*/
	&android_pmem_mdp_device,
	&android_pmem_adsp_device,
#ifdef CONFIG_720P_CAMERA
	&android_pmem_venc_device,
#endif
#ifdef CONFIG_BUILD_CIQ
	&android_pmem_ciq_device,
	&android_pmem_ciq1_device,
	&android_pmem_ciq2_device,
	&android_pmem_ciq3_device,
#endif
	&msm_camera_sensor_ov8810,
	&msm_kgsl_device,
	&msm_device_i2c,
	&msm_camera_sensor_ov9665,
	&supersonic_flashlight_device,
	&supersonic_leds,
#if defined(CONFIG_SPI_QSD)
	&qsd_device_spi,
#endif
};

/* UART1 debug port init. This is supposed to be init in bootloader */
static uint32_t supersonic_serial_debug_table[] = {
	/* for uart debugger. It should be removed when support usb to serial function */
	PCOM_GPIO_CFG(SUPERSONIC_GPIO_UART1_RX, 3, GPIO_INPUT, GPIO_NO_PULL, GPIO_4MA), /* RX */
	PCOM_GPIO_CFG(SUPERSONIC_GPIO_UART1_TX, 3, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* TX */
};

/* UART1 debug port init. This is supposed to be init in bootloader */
static uint32_t supersonic_serial_debug_off_table[] = {
	/* for uart debugger. It should be removed when support usb to serial function */
	PCOM_GPIO_CFG(SUPERSONIC_GPIO_UART1_RX, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), /* RX */
	PCOM_GPIO_CFG(SUPERSONIC_GPIO_UART1_TX, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* TX */
};

static void supersonic_config_serial_debug_gpios(int on)
{
	if (on){
		config_gpio_table(supersonic_serial_debug_table, ARRAY_SIZE(supersonic_serial_debug_table));
	}else{
		config_gpio_table(supersonic_serial_debug_off_table, ARRAY_SIZE(supersonic_serial_debug_off_table));
		/*gpio_set_value(SUPERSONIC_GPIO_UART1_RX, 0); *//* OL */
		gpio_set_value(SUPERSONIC_GPIO_UART1_TX, 0); /* OL */
	}
	printk(KERN_INFO "supersonic_config_serial_debug_gpios %d  \n", on);
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

static struct msm_acpu_clock_platform_data supersonic_clock_data = {
	.acpu_switch_time_us	= 20,
	.max_speed_delta_khz	= 256000,
	.vdd_switch_time_us	= 62,
	.power_collapse_khz	= 245000,
	.wait_for_irq_khz	= 245000,
};

static unsigned supersonic_perf_acpu_table[] = {
	245000000,
	576000000,
	998400000,
};

static struct perflock_platform_data supersonic_perflock_data = {
	.perf_acpu_table = supersonic_perf_acpu_table,
	.table_size = ARRAY_SIZE(supersonic_perf_acpu_table),
};

int supersonic_init_mmc(int sysrev);

#ifdef CONFIG_SERIAL_MSM_HS
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.rx_wakeup_irq = MSM_GPIO_TO_INT(SUPERSONIC_GPIO_BT_HOST_WAKE),	/*Chip to Device*/
	.inject_rx_on_wakeup = 0,
	.cpu_lock_supported = 0,

	/* for bcm */
	.bt_wakeup_pin_supported = 1,
	.bt_wakeup_pin = SUPERSONIC_GPIO_BT_CHIP_WAKE,
	.host_wakeup_pin = SUPERSONIC_GPIO_BT_HOST_WAKE,
};
#endif

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
struct msm_serial_debug_platform_data msm_uart_debug_pdata = {
	.config_gpio = supersonic_config_serial_debug_gpios,
	.disable_uart = 1,
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

unsigned supersonic_get_skuid(void)
{
	return skuid;
}

static ssize_t supersonic_virtual_keys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_HOME)   ":43:835:86:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_MENU)   ":165:835:100:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_BACK)   ":300:835:110:50"
			":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":425:835:90:50"
			"\n");
}

static struct kobj_attribute supersonic_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.atmel-touchscreen",
		.mode = S_IRUGO,
	},
	.show = &supersonic_virtual_keys_show,
};

static struct attribute *supersonic_properties_attrs[] = {
	&supersonic_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group supersonic_properties_attr_group = {
	.attrs = supersonic_properties_attrs,
};

static void supersonic_reset(void)
{
        gpio_set_value(SUPERSONIC_GPIO_PS_HOLD, 0);
}

/* system_rev == higher 16bits of PCBID
XA -> 0000FFFF -> 0x0000
XB -> 0101FFFF -> 0x0101
XC -> 0202FFFF -> 0x0202
*/
static void __init supersonic_init(void)
{
	int ret;
	struct kobject *properties_kobj;

	printk("supersonic_init() revision=%d\n", system_rev);

	msm_hw_reset_hook = supersonic_reset;

	OJ_BMA_power();

	msm_acpu_clock_init(&supersonic_clock_data);

	perflock_init(&supersonic_perflock_data);

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	msm_device_uart1.dev.platform_data = &msm_uart_debug_pdata;
	msm_serial_debug_init(MSM_UART1_PHYS, INT_UART1,
			      &msm_device_uart1.dev, 1, MSM_GPIO_TO_INT(SUPERSONIC_GPIO_UART1_RX));
#endif

#ifdef CONFIG_ARCH_QSD8X50
	bt_export_bd_address();
#endif

	/* set the gpu power rail to manual mode so clk en/dis will not
	 * turn off gpu power, and hang it on resume */
	supersonic_kgsl_power_rail_mode(0);
	supersonic_kgsl_power(true);

	#ifdef CONFIG_SERIAL_MSM_HS
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
	msm_device_uart_dm1.name = "msm_serial_hs_bcm";	/* for bcm */
	#endif

	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
	/*gpio_direction_output(SUPERSONIC_GPIO_TP_LS_EN, 0);*/
	gpio_direction_output(SUPERSONIC_GPIO_TP_EN, 0);

	supersonic_audio_init();
	supersonic_init_panel();
#ifdef CONFIG_MICROP_COMMON
	supersonic_microp_init();
#endif
	msm_device_i2c_init();

	platform_add_devices(devices, ARRAY_SIZE(devices));
#ifdef CONFIG_USB_ANDROID
	if (!opt_usb_h2w_sw)
		suc_add_usb_devices();
#endif
	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));

	ret = supersonic_init_mmc(system_rev);
	if (ret != 0)
		pr_crit("%s: Unable to initialize MMC\n", __func__);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&supersonic_properties_attr_group);
	if (!properties_kobj || ret)
		pr_err("failed to create board_properties\n");

	msm_init_pmic_vibrator();

}

static void __init supersonic_fixup(struct machine_desc *desc, struct tag *tags,
				 char **cmdline, struct meminfo *mi)
{
	skuid = parse_tag_skuid((const struct tag *)tags);
	printk(KERN_INFO "supersonic_fixup:skuid=0x%x\n", skuid);
	/* First Bank 256MB */
	mi->nr_banks = 1;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].node = PHYS_TO_NID(PHYS_OFFSET);
	mi->bank[0].size = MSM_EBI1_BANK0_SIZE;	/*(219*1024*1024);*/

	/* Second Bank 128MB */
	mi->nr_banks++;
	mi->bank[1].start = MSM_EBI1_BANK1_BASE;
	mi->bank[1].node = PHYS_TO_NID(MSM_EBI1_BANK1_BASE);
	mi->bank[1].size = MSM_EBI1_BANK1_SIZE;
}

static void __init supersonic_map_io(void)
{
	msm_map_common_io();
	msm_clock_init();
}

extern struct sys_timer msm_timer;

MACHINE_START(SUPERSONIC, "supersonic")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x20000100,
	.fixup		= supersonic_fixup,
	.map_io		= supersonic_map_io,
	.init_irq	= msm_init_irq,
	.init_machine	= supersonic_init,
	.timer		= &msm_timer,
MACHINE_END
