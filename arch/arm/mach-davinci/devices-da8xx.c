/*
 * DA8XX/OMAP L1XX platform device data
 *
 * Copyright (c) 2007-2009, MontaVista Software, Inc. <source@mvista.com>
 * Derived from code that was:
 *	Copyright (C) 2006 Komal Shah <komal_shah802003@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/serial_8250.h>

#include <mach/cputype.h>
#include <mach/common.h>
#include <mach/time.h>
#include <mach/da8xx.h>
#include <video/da8xx-fb.h>

#include "clock.h"

#define DA8XX_TPCC_BASE			0x01c00000
#define DA8XX_TPTC0_BASE		0x01c08000
#define DA8XX_TPTC1_BASE		0x01c08400
#define DA8XX_WDOG_BASE			0x01c21000 /* DA8XX_TIMER64P1_BASE */
#define DA8XX_I2C0_BASE			0x01c22000
#define DA8XX_EMAC_CPPI_PORT_BASE	0x01e20000
#define DA8XX_EMAC_CPGMACSS_BASE	0x01e22000
#define DA8XX_EMAC_CPGMAC_BASE		0x01e23000
#define DA8XX_EMAC_MDIO_BASE		0x01e24000
#define DA8XX_GPIO_BASE			0x01e26000
#define DA8XX_I2C1_BASE			0x01e28000

#define DA8XX_EMAC_CTRL_REG_OFFSET	0x3000
#define DA8XX_EMAC_MOD_REG_OFFSET	0x2000
#define DA8XX_EMAC_RAM_OFFSET		0x0000
#define DA8XX_MDIO_REG_OFFSET		0x4000
#define DA8XX_EMAC_CTRL_RAM_SIZE	SZ_8K

static struct plat_serial8250_port da8xx_serial_pdata[] = {
	{
		.mapbase	= DA8XX_UART0_BASE,
		.irq		= IRQ_DA8XX_UARTINT0,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST |
					UPF_IOREMAP,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
	},
	{
		.mapbase	= DA8XX_UART1_BASE,
		.irq		= IRQ_DA8XX_UARTINT1,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST |
					UPF_IOREMAP,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
	},
	{
		.mapbase	= DA8XX_UART2_BASE,
		.irq		= IRQ_DA8XX_UARTINT2,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST |
					UPF_IOREMAP,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
	},
	{
		.flags	= 0,
	},
};

struct platform_device da8xx_serial_device = {
	.name	= "serial8250",
	.id	= PLAT8250_DEV_PLATFORM,
	.dev	= {
		.platform_data	= da8xx_serial_pdata,
	},
};

static const s8 da8xx_dma_chan_no_event[] = {
	20, 21,
	-1
};

static const s8 da8xx_queue_tc_mapping[][2] = {
	/* {event queue no, TC no} */
	{0, 0},
	{1, 1},
	{-1, -1}
};

static const s8 da8xx_queue_priority_mapping[][2] = {
	/* {event queue no, Priority} */
	{0, 3},
	{1, 7},
	{-1, -1}
};

static struct edma_soc_info da8xx_edma_info[] = {
	{
		.n_channel		= 32,
		.n_region		= 4,
		.n_slot			= 128,
		.n_tc			= 2,
		.n_cc			= 1,
		.noevent		= da8xx_dma_chan_no_event,
		.queue_tc_mapping	= da8xx_queue_tc_mapping,
		.queue_priority_mapping	= da8xx_queue_priority_mapping,
	},
};

static struct resource da8xx_edma_resources[] = {
	{
		.name	= "edma_cc0",
		.start	= DA8XX_TPCC_BASE,
		.end	= DA8XX_TPCC_BASE + SZ_32K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "edma_tc0",
		.start	= DA8XX_TPTC0_BASE,
		.end	= DA8XX_TPTC0_BASE + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "edma_tc1",
		.start	= DA8XX_TPTC1_BASE,
		.end	= DA8XX_TPTC1_BASE + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "edma0",
		.start	= IRQ_DA8XX_CCINT0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "edma0_err",
		.start	= IRQ_DA8XX_CCERRINT,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device da8xx_edma_device = {
	.name		= "edma",
	.id		= -1,
	.dev = {
		.platform_data	= da8xx_edma_info,
	},
	.num_resources	= ARRAY_SIZE(da8xx_edma_resources),
	.resource	= da8xx_edma_resources,
};

int __init da8xx_register_edma(void)
{
	return platform_device_register(&da8xx_edma_device);
}

static struct resource da8xx_i2c_resources0[] = {
	{
		.start	= DA8XX_I2C0_BASE,
		.end	= DA8XX_I2C0_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_DA8XX_I2CINT0,
		.end	= IRQ_DA8XX_I2CINT0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device da8xx_i2c_device0 = {
	.name		= "i2c_davinci",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(da8xx_i2c_resources0),
	.resource	= da8xx_i2c_resources0,
};

static struct resource da8xx_i2c_resources1[] = {
	{
		.start	= DA8XX_I2C1_BASE,
		.end	= DA8XX_I2C1_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_DA8XX_I2CINT1,
		.end	= IRQ_DA8XX_I2CINT1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device da8xx_i2c_device1 = {
	.name		= "i2c_davinci",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(da8xx_i2c_resources1),
	.resource	= da8xx_i2c_resources1,
};

int __init da8xx_register_i2c(int instance,
		struct davinci_i2c_platform_data *pdata)
{
	struct platform_device *pdev;

	if (instance == 0)
		pdev = &da8xx_i2c_device0;
	else if (instance == 1)
		pdev = &da8xx_i2c_device1;
	else
		return -EINVAL;

	pdev->dev.platform_data = pdata;
	return platform_device_register(pdev);
}

static struct resource da8xx_watchdog_resources[] = {
	{
		.start	= DA8XX_WDOG_BASE,
		.end	= DA8XX_WDOG_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device davinci_wdt_device = {
	.name		= "watchdog",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(da8xx_watchdog_resources),
	.resource	= da8xx_watchdog_resources,
};

int __init da8xx_register_watchdog(void)
{
	return platform_device_register(&davinci_wdt_device);
}

static struct resource da8xx_emac_resources[] = {
	{
		.start	= DA8XX_EMAC_CPPI_PORT_BASE,
		.end	= DA8XX_EMAC_CPPI_PORT_BASE + 0x5000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_DA8XX_C0_RX_THRESH_PULSE,
		.end	= IRQ_DA8XX_C0_RX_THRESH_PULSE,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= IRQ_DA8XX_C0_RX_PULSE,
		.end	= IRQ_DA8XX_C0_RX_PULSE,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= IRQ_DA8XX_C0_TX_PULSE,
		.end	= IRQ_DA8XX_C0_TX_PULSE,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= IRQ_DA8XX_C0_MISC_PULSE,
		.end	= IRQ_DA8XX_C0_MISC_PULSE,
		.flags	= IORESOURCE_IRQ,
	},
};

struct emac_platform_data da8xx_emac_pdata = {
	.ctrl_reg_offset	= DA8XX_EMAC_CTRL_REG_OFFSET,
	.ctrl_mod_reg_offset	= DA8XX_EMAC_MOD_REG_OFFSET,
	.ctrl_ram_offset	= DA8XX_EMAC_RAM_OFFSET,
	.mdio_reg_offset	= DA8XX_MDIO_REG_OFFSET,
	.ctrl_ram_size		= DA8XX_EMAC_CTRL_RAM_SIZE,
	.version		= EMAC_VERSION_2,
};

static struct platform_device da8xx_emac_device = {
	.name		= "davinci_emac",
	.id		= 1,
	.dev = {
		.platform_data	= &da8xx_emac_pdata,
	},
	.num_resources	= ARRAY_SIZE(da8xx_emac_resources),
	.resource	= da8xx_emac_resources,
};

static struct resource da830_mcasp1_resources[] = {
	{
		.name	= "mcasp1",
		.start	= DAVINCI_DA830_MCASP1_REG_BASE,
		.end	= DAVINCI_DA830_MCASP1_REG_BASE + (SZ_1K * 12) - 1,
		.flags	= IORESOURCE_MEM,
	},
	/* TX event */
	{
		.start	= DAVINCI_DA830_DMA_MCASP1_AXEVT,
		.end	= DAVINCI_DA830_DMA_MCASP1_AXEVT,
		.flags	= IORESOURCE_DMA,
	},
	/* RX event */
	{
		.start	= DAVINCI_DA830_DMA_MCASP1_AREVT,
		.end	= DAVINCI_DA830_DMA_MCASP1_AREVT,
		.flags	= IORESOURCE_DMA,
	},
};

static struct platform_device da830_mcasp1_device = {
	.name		= "davinci-mcasp",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(da830_mcasp1_resources),
	.resource	= da830_mcasp1_resources,
};

static struct resource da850_mcasp_resources[] = {
	{
		.name	= "mcasp",
		.start	= DAVINCI_DA8XX_MCASP0_REG_BASE,
		.end	= DAVINCI_DA8XX_MCASP0_REG_BASE + (SZ_1K * 12) - 1,
		.flags	= IORESOURCE_MEM,
	},
	/* TX event */
	{
		.start	= DAVINCI_DA8XX_DMA_MCASP0_AXEVT,
		.end	= DAVINCI_DA8XX_DMA_MCASP0_AXEVT,
		.flags	= IORESOURCE_DMA,
	},
	/* RX event */
	{
		.start	= DAVINCI_DA8XX_DMA_MCASP0_AREVT,
		.end	= DAVINCI_DA8XX_DMA_MCASP0_AREVT,
		.flags	= IORESOURCE_DMA,
	},
};

static struct platform_device da850_mcasp_device = {
	.name		= "davinci-mcasp",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(da850_mcasp_resources),
	.resource	= da850_mcasp_resources,
};

int __init da8xx_register_emac(void)
{
	return platform_device_register(&da8xx_emac_device);
}

void __init da8xx_init_mcasp(int id, struct snd_platform_data *pdata)
{
	/* DA830/OMAP-L137 has 3 instances of McASP */
	if (cpu_is_davinci_da830() && id == 1) {
		da830_mcasp1_device.dev.platform_data = pdata;
		platform_device_register(&da830_mcasp1_device);
	} else if (cpu_is_davinci_da850()) {
		da850_mcasp_device.dev.platform_data = pdata;
		platform_device_register(&da850_mcasp_device);
	}
}

static const struct display_panel disp_panel = {
	QVGA,
	16,
	16,
	COLOR_ACTIVE,
};

static struct lcd_ctrl_config lcd_cfg = {
	&disp_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 16,
	.fdd			= 255,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

static struct da8xx_lcdc_platform_data da850_evm_lcdc_pdata = {
	.manu_name = "sharp",
	.controller_data = &lcd_cfg,
	.type = "Sharp_LK043T1DG01",
};

static struct resource da8xx_lcdc_resources[] = {
	[0] = { /* registers */
		.start  = DA8XX_LCD_CNTRL_BASE,
		.end    = DA8XX_LCD_CNTRL_BASE + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = { /* interrupt */
		.start  = IRQ_DA8XX_LCDINT,
		.end    = IRQ_DA8XX_LCDINT,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device da850_lcdc_device = {
	.name		= "da8xx_lcdc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(da8xx_lcdc_resources),
	.resource	= da8xx_lcdc_resources,
	.dev = {
		.platform_data = &da850_evm_lcdc_pdata,
	}
};

int __init da8xx_register_lcdc(void)
{
	return platform_device_register(&da850_lcdc_device);
}

static struct resource da8xx_mmcsd0_resources[] = {
	{		/* registers */
		.start	= DA8XX_MMCSD0_BASE,
		.end	= DA8XX_MMCSD0_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{		/* interrupt */
		.start	= IRQ_DA8XX_MMCSDINT0,
		.end	= IRQ_DA8XX_MMCSDINT0,
		.flags	= IORESOURCE_IRQ,
	},
	{		/* DMA RX */
		.start	= EDMA_CTLR_CHAN(0, 16),
		.end	= EDMA_CTLR_CHAN(0, 16),
		.flags	= IORESOURCE_DMA,
	},
	{		/* DMA TX */
		.start	= EDMA_CTLR_CHAN(0, 17),
		.end	= EDMA_CTLR_CHAN(0, 17),
		.flags	= IORESOURCE_DMA,
	},
};

static struct platform_device da8xx_mmcsd0_device = {
	.name		= "davinci_mmc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(da8xx_mmcsd0_resources),
	.resource	= da8xx_mmcsd0_resources,
};

int __init da8xx_register_mmcsd0(struct davinci_mmc_config *config)
{
	da8xx_mmcsd0_device.dev.platform_data = config;
	return platform_device_register(&da8xx_mmcsd0_device);
}
