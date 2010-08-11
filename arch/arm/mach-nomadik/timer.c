/*
 *  linux/arch/arm/mach-nomadik/timer.c
 *
 * Copyright (C) 2008 STMicroelectronics
 * Copyright (C) 2009 Alessandro Rubini, somewhat based on at91sam926x
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/clockchips.h>
#include <linux/jiffies.h>
#include <asm/mach/time.h>
#include <mach/mtu.h>

#define TIMER_CTRL	0x80	/* No divisor */
#define TIMER_PERIODIC	0x40
#define TIMER_SZ32BIT	0x02

/* Initial value for SRC control register: all timers use MXTAL/8 source */
#define SRC_CR_INIT_MASK	0x00007fff
#define SRC_CR_INIT_VAL		0x2aaa8000

static u32	nmdk_count;		/* accumulated count */
static u32	nmdk_cycle;		/* write-once */
static __iomem void *mtu_base;

/*
 * clocksource: the MTU device is a decrementing counters, so we negate
 * the value being read.
 */
static cycle_t nmdk_read_timer(struct clocksource *cs)
{
	u32 count = readl(mtu_base + MTU_VAL(0));
	return nmdk_count + nmdk_cycle - count;

}

static struct clocksource nmdk_clksrc = {
	.name		= "mtu_0",
	.rating		= 120,
	.read		= nmdk_read_timer,
	.shift		= 20,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

/*
 * Clockevent device: currently only periodic mode is supported
 */
static void nmdk_clkevt_mode(enum clock_event_mode mode,
			     struct clock_event_device *dev)
{
	unsigned long flags;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		/* enable interrupts -- and count current value? */
		raw_local_irq_save(flags);
		writel(readl(mtu_base + MTU_IMSC) | 1, mtu_base + MTU_IMSC);
		raw_local_irq_restore(flags);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		BUG(); /* Not supported, yet */
		/* FALLTHROUGH */
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		/* disable irq */
		raw_local_irq_save(flags);
		writel(readl(mtu_base + MTU_IMSC) & ~1, mtu_base + MTU_IMSC);
		raw_local_irq_restore(flags);
		break;
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static struct clock_event_device nmdk_clkevt = {
	.name		= "mtu_0",
	.features	= CLOCK_EVT_FEAT_PERIODIC,
	.shift		= 32,
	.rating		= 100,
	.set_mode	= nmdk_clkevt_mode,
};

/*
 * IRQ Handler for the timer 0 of the MTU block. The irq is not shared
 * as we are the only users of mtu0 by now.
 */
static irqreturn_t nmdk_timer_interrupt(int irq, void *dev_id)
{
	/* ack: "interrupt clear register" */
	writel( 1 << 0, mtu_base + MTU_ICR);

	/* we can't count lost ticks, unfortunately */
	nmdk_count += nmdk_cycle;
	nmdk_clkevt.event_handler(&nmdk_clkevt);

	return IRQ_HANDLED;
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static struct irqaction nmdk_timer_irq = {
	.name		= "Nomadik Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.handler	= nmdk_timer_interrupt,
};

static void nmdk_timer_reset(void)
{
	u32 cr;

	writel(0, mtu_base + MTU_CR(0)); /* off */

	/* configure load and background-load, and fire it up */
	writel(nmdk_cycle, mtu_base + MTU_LR(0));
	writel(nmdk_cycle, mtu_base + MTU_BGLR(0));
	cr = MTU_CRn_PERIODIC | MTU_CRn_PRESCALE_1 | MTU_CRn_32BITS;
	writel(cr, mtu_base + MTU_CR(0));
	writel(cr | MTU_CRn_ENA, mtu_base + MTU_CR(0));
}

static void __init nmdk_timer_init(void)
{
	u32 src_cr;
	unsigned long rate;
	int bits;

	rate = CLOCK_TICK_RATE; /* 2.4MHz */
	nmdk_cycle = (rate + HZ/2) / HZ;

	/* Configure timer sources in "system reset controller" ctrl reg */
	src_cr = readl(io_p2v(NOMADIK_SRC_BASE));
	src_cr &= SRC_CR_INIT_MASK;
	src_cr |= SRC_CR_INIT_VAL;
	writel(src_cr, io_p2v(NOMADIK_SRC_BASE));

	/* Save global pointer to mtu, used by functions above */
	mtu_base = io_p2v(NOMADIK_MTU0_BASE);

	/* Init the timer and register clocksource */
	nmdk_timer_reset();

	nmdk_clksrc.mult = clocksource_hz2mult(rate, nmdk_clksrc.shift);
	bits =  8*sizeof(nmdk_count);
	nmdk_clksrc.mask = CLOCKSOURCE_MASK(bits);

	clocksource_register(&nmdk_clksrc);

	/* Register irq and clockevents */
	setup_irq(IRQ_MTU0, &nmdk_timer_irq);
	nmdk_clkevt.mult = div_sc(rate, NSEC_PER_SEC, nmdk_clkevt.shift);
	nmdk_clkevt.cpumask = cpumask_of(0);
	clockevents_register_device(&nmdk_clkevt);
}

struct sys_timer nomadik_timer = {
	.init		= nmdk_timer_init,
};
