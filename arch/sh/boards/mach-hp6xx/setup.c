/*
 * linux/arch/sh/boards/hp6xx/setup.c
 *
 * Copyright (C) 2002 Andriy Skulysh
 * Copyright (C) 2007 Kristoffer Ericson <Kristoffer_e1@hotmail.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Setup code for HP620/HP660/HP680/HP690 (internal peripherials only)
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <asm/hd64461.h>
#include <asm/io.h>
#include <mach/hp6xx.h>
#include <cpu/dac.h>

#define	SCPCR	0xa4000116
#define	SCPDR	0xa4000136

/* CF Slot */
static struct resource cf_ide_resources[] = {
	[0] = {
		.start = 0x15000000 + 0x1f0,
		.end   = 0x15000000 + 0x1f0 + 0x08 - 0x01,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 0x15000000 + 0x1fe,
		.end   = 0x15000000 + 0x1fe + 0x01,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = 77,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device cf_ide_device = {
	.name		=  "pata_platform",
	.id		=  -1,
	.num_resources	= ARRAY_SIZE(cf_ide_resources),
	.resource	= cf_ide_resources,
};

static struct platform_device jornadakbd_device = {
	.name		= "jornada680_kbd",
	.id		= -1,
};

static struct platform_device *hp6xx_devices[] __initdata = {
	&cf_ide_device,
	&jornadakbd_device,
};

static void __init hp6xx_init_irq(void)
{
	/* Gets touchscreen and powerbutton IRQ working */
	plat_irq_setup_pins(IRQ_MODE_IRQ);
}

static int __init hp6xx_devices_setup(void)
{
	return platform_add_devices(hp6xx_devices, ARRAY_SIZE(hp6xx_devices));
}

static void __init hp6xx_setup(char **cmdline_p)
{
	u8 v8;
	u16 v;

	v = inw(HD64461_STBCR);
	v |=	HD64461_STBCR_SURTST | HD64461_STBCR_SIRST	|
		HD64461_STBCR_STM1ST | HD64461_STBCR_STM0ST	|
		HD64461_STBCR_SAFEST | HD64461_STBCR_SPC0ST	|
		HD64461_STBCR_SMIAST | HD64461_STBCR_SAFECKE_OST|
		HD64461_STBCR_SAFECKE_IST;
#ifndef CONFIG_HD64461_ENABLER
	v |= HD64461_STBCR_SPC1ST;
#endif
	outw(v, HD64461_STBCR);
	v = inw(HD64461_GPADR);
	v |= HD64461_GPADR_SPEAKER | HD64461_GPADR_PCMCIA0;
	outw(v, HD64461_GPADR);

	outw(HD64461_PCCGCR_VCC0 | HD64461_PCCSCR_VCC1, HD64461_PCC0GCR);

#ifndef CONFIG_HD64461_ENABLER
	outw(HD64461_PCCGCR_VCC0 | HD64461_PCCSCR_VCC1, HD64461_PCC1GCR);
#endif

	sh_dac_output(0, DAC_SPEAKER_VOLUME);
	sh_dac_disable(DAC_SPEAKER_VOLUME);
	v8 = ctrl_inb(DACR);
	v8 &= ~DACR_DAE;
	ctrl_outb(v8,DACR);

	v8 = ctrl_inb(SCPDR);
	v8 |= SCPDR_TS_SCAN_X | SCPDR_TS_SCAN_Y;
	v8 &= ~SCPDR_TS_SCAN_ENABLE;
	ctrl_outb(v8, SCPDR);

	v = ctrl_inw(SCPCR);
	v &= ~SCPCR_TS_MASK;
	v |= SCPCR_TS_ENABLE;
	ctrl_outw(v, SCPCR);
}
device_initcall(hp6xx_devices_setup);

static struct sh_machine_vector mv_hp6xx __initmv = {
	.mv_name = "hp6xx",
	.mv_setup = hp6xx_setup,
	/* IRQ's : CPU(64) + CCHIP(16) + FREE_TO_USE(6) */
	.mv_nr_irqs = HD64461_IRQBASE + HD64461_IRQ_NUM + 6,
	/* Enable IRQ0 -> IRQ3 in IRQ_MODE */
	.mv_init_irq = hp6xx_init_irq,
};
