/*
 * SGI RTC clock/timer routines.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *  Copyright (c) 2009 Silicon Graphics, Inc.  All Rights Reserved.
 *  Copyright (c) Dimitri Sivanich
 */
#include <linux/clockchips.h>

#include <asm/uv/uv_mmrs.h>
#include <asm/uv/uv_hub.h>
#include <asm/uv/bios.h>
#include <asm/uv/uv.h>
#include <asm/apic.h>
#include <asm/cpu.h>

#define RTC_NAME		"sgi_rtc"

static cycle_t uv_read_rtc(struct clocksource *cs);
static int uv_rtc_next_event(unsigned long, struct clock_event_device *);
static void uv_rtc_timer_setup(enum clock_event_mode,
				struct clock_event_device *);

static struct clocksource clocksource_uv = {
	.name		= RTC_NAME,
	.rating		= 400,
	.read		= uv_read_rtc,
	.mask		= (cycle_t)UVH_RTC_REAL_TIME_CLOCK_MASK,
	.shift		= 10,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct clock_event_device clock_event_device_uv = {
	.name		= RTC_NAME,
	.features	= CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 20,
	.rating		= 400,
	.irq		= -1,
	.set_next_event	= uv_rtc_next_event,
	.set_mode	= uv_rtc_timer_setup,
	.event_handler	= NULL,
};

static DEFINE_PER_CPU(struct clock_event_device, cpu_ced);

/* There is one of these allocated per node */
struct uv_rtc_timer_head {
	spinlock_t	lock;
	/* next cpu waiting for timer, local node relative: */
	int		next_cpu;
	/* number of cpus on this node: */
	int		ncpus;
	struct {
		int	lcpu;		/* systemwide logical cpu number */
		u64	expires;	/* next timer expiration for this cpu */
	} cpu[1];
};

/*
 * Access to uv_rtc_timer_head via blade id.
 */
static struct uv_rtc_timer_head		**blade_info __read_mostly;

static int				uv_rtc_enable;

/*
 * Hardware interface routines
 */

/* Send IPIs to another node */
static void uv_rtc_send_IPI(int cpu)
{
	unsigned long apicid, val;
	int pnode;

	apicid = cpu_physical_id(cpu);
	pnode = uv_apicid_to_pnode(apicid);
	val = (1UL << UVH_IPI_INT_SEND_SHFT) |
	      (apicid << UVH_IPI_INT_APIC_ID_SHFT) |
	      (GENERIC_INTERRUPT_VECTOR << UVH_IPI_INT_VECTOR_SHFT);

	uv_write_global_mmr64(pnode, UVH_IPI_INT, val);
}

/* Check for an RTC interrupt pending */
static int uv_intr_pending(int pnode)
{
	return uv_read_global_mmr64(pnode, UVH_EVENT_OCCURRED0) &
		UVH_EVENT_OCCURRED0_RTC1_MASK;
}

/* Setup interrupt and return non-zero if early expiration occurred. */
static int uv_setup_intr(int cpu, u64 expires)
{
	u64 val;
	int pnode = uv_cpu_to_pnode(cpu);

	uv_write_global_mmr64(pnode, UVH_RTC1_INT_CONFIG,
		UVH_RTC1_INT_CONFIG_M_MASK);
	uv_write_global_mmr64(pnode, UVH_INT_CMPB, -1L);

	uv_write_global_mmr64(pnode, UVH_EVENT_OCCURRED0_ALIAS,
		UVH_EVENT_OCCURRED0_RTC1_MASK);

	val = (GENERIC_INTERRUPT_VECTOR << UVH_RTC1_INT_CONFIG_VECTOR_SHFT) |
		((u64)cpu_physical_id(cpu) << UVH_RTC1_INT_CONFIG_APIC_ID_SHFT);

	/* Set configuration */
	uv_write_global_mmr64(pnode, UVH_RTC1_INT_CONFIG, val);
	/* Initialize comparator value */
	uv_write_global_mmr64(pnode, UVH_INT_CMPB, expires);

	return (expires < uv_read_rtc(NULL) && !uv_intr_pending(pnode));
}

/*
 * Per-cpu timer tracking routines
 */

static __init void uv_rtc_deallocate_timers(void)
{
	int bid;

	for_each_possible_blade(bid) {
		kfree(blade_info[bid]);
	}
	kfree(blade_info);
}

/* Allocate per-node list of cpu timer expiration times. */
static __init int uv_rtc_allocate_timers(void)
{
	int cpu;

	blade_info = kmalloc(uv_possible_blades * sizeof(void *), GFP_KERNEL);
	if (!blade_info)
		return -ENOMEM;
	memset(blade_info, 0, uv_possible_blades * sizeof(void *));

	for_each_present_cpu(cpu) {
		int nid = cpu_to_node(cpu);
		int bid = uv_cpu_to_blade_id(cpu);
		int bcpu = uv_cpu_hub_info(cpu)->blade_processor_id;
		struct uv_rtc_timer_head *head = blade_info[bid];

		if (!head) {
			head = kmalloc_node(sizeof(struct uv_rtc_timer_head) +
				(uv_blade_nr_possible_cpus(bid) *
					2 * sizeof(u64)),
				GFP_KERNEL, nid);
			if (!head) {
				uv_rtc_deallocate_timers();
				return -ENOMEM;
			}
			spin_lock_init(&head->lock);
			head->ncpus = uv_blade_nr_possible_cpus(bid);
			head->next_cpu = -1;
			blade_info[bid] = head;
		}

		head->cpu[bcpu].lcpu = cpu;
		head->cpu[bcpu].expires = ULLONG_MAX;
	}

	return 0;
}

/* Find and set the next expiring timer.  */
static void uv_rtc_find_next_timer(struct uv_rtc_timer_head *head, int pnode)
{
	u64 lowest = ULLONG_MAX;
	int c, bcpu = -1;

	head->next_cpu = -1;
	for (c = 0; c < head->ncpus; c++) {
		u64 exp = head->cpu[c].expires;
		if (exp < lowest) {
			bcpu = c;
			lowest = exp;
		}
	}
	if (bcpu >= 0) {
		head->next_cpu = bcpu;
		c = head->cpu[bcpu].lcpu;
		if (uv_setup_intr(c, lowest))
			/* If we didn't set it up in time, trigger */
			uv_rtc_send_IPI(c);
	} else {
		uv_write_global_mmr64(pnode, UVH_RTC1_INT_CONFIG,
			UVH_RTC1_INT_CONFIG_M_MASK);
	}
}

/*
 * Set expiration time for current cpu.
 *
 * Returns 1 if we missed the expiration time.
 */
static int uv_rtc_set_timer(int cpu, u64 expires)
{
	int pnode = uv_cpu_to_pnode(cpu);
	int bid = uv_cpu_to_blade_id(cpu);
	struct uv_rtc_timer_head *head = blade_info[bid];
	int bcpu = uv_cpu_hub_info(cpu)->blade_processor_id;
	u64 *t = &head->cpu[bcpu].expires;
	unsigned long flags;
	int next_cpu;

	spin_lock_irqsave(&head->lock, flags);

	next_cpu = head->next_cpu;
	*t = expires;
	/* Will this one be next to go off? */
	if (next_cpu < 0 || bcpu == next_cpu ||
			expires < head->cpu[next_cpu].expires) {
		head->next_cpu = bcpu;
		if (uv_setup_intr(cpu, expires)) {
			*t = ULLONG_MAX;
			uv_rtc_find_next_timer(head, pnode);
			spin_unlock_irqrestore(&head->lock, flags);
			return 1;
		}
	}

	spin_unlock_irqrestore(&head->lock, flags);
	return 0;
}

/*
 * Unset expiration time for current cpu.
 *
 * Returns 1 if this timer was pending.
 */
static int uv_rtc_unset_timer(int cpu)
{
	int pnode = uv_cpu_to_pnode(cpu);
	int bid = uv_cpu_to_blade_id(cpu);
	struct uv_rtc_timer_head *head = blade_info[bid];
	int bcpu = uv_cpu_hub_info(cpu)->blade_processor_id;
	u64 *t = &head->cpu[bcpu].expires;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&head->lock, flags);

	if (head->next_cpu == bcpu && uv_read_rtc(NULL) >= *t)
		rc = 1;

	*t = ULLONG_MAX;

	/* Was the hardware setup for this timer? */
	if (head->next_cpu == bcpu)
		uv_rtc_find_next_timer(head, pnode);

	spin_unlock_irqrestore(&head->lock, flags);

	return rc;
}


/*
 * Kernel interface routines.
 */

/*
 * Read the RTC.
 */
static cycle_t uv_read_rtc(struct clocksource *cs)
{
	return (cycle_t)uv_read_local_mmr(UVH_RTC);
}

/*
 * Program the next event, relative to now
 */
static int uv_rtc_next_event(unsigned long delta,
			     struct clock_event_device *ced)
{
	int ced_cpu = cpumask_first(ced->cpumask);

	return uv_rtc_set_timer(ced_cpu, delta + uv_read_rtc(NULL));
}

/*
 * Setup the RTC timer in oneshot mode
 */
static void uv_rtc_timer_setup(enum clock_event_mode mode,
			       struct clock_event_device *evt)
{
	int ced_cpu = cpumask_first(evt->cpumask);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_RESUME:
		/* Nothing to do here yet */
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		uv_rtc_unset_timer(ced_cpu);
		break;
	}
}

static void uv_rtc_interrupt(void)
{
	struct clock_event_device *ced = &__get_cpu_var(cpu_ced);
	int cpu = smp_processor_id();

	if (!ced || !ced->event_handler)
		return;

	if (uv_rtc_unset_timer(cpu) != 1)
		return;

	ced->event_handler(ced);
}

static int __init uv_enable_rtc(char *str)
{
	uv_rtc_enable = 1;

	return 1;
}
__setup("uvrtc", uv_enable_rtc);

static __init void uv_rtc_register_clockevents(struct work_struct *dummy)
{
	struct clock_event_device *ced = &__get_cpu_var(cpu_ced);

	*ced = clock_event_device_uv;
	ced->cpumask = cpumask_of(smp_processor_id());
	clockevents_register_device(ced);
}

static __init int uv_rtc_setup_clock(void)
{
	int rc;

	if (!uv_rtc_enable || !is_uv_system() || generic_interrupt_extension)
		return -ENODEV;

	generic_interrupt_extension = uv_rtc_interrupt;

	clocksource_uv.mult = clocksource_hz2mult(sn_rtc_cycles_per_second,
				clocksource_uv.shift);

	rc = clocksource_register(&clocksource_uv);
	if (rc) {
		generic_interrupt_extension = NULL;
		return rc;
	}

	/* Setup and register clockevents */
	rc = uv_rtc_allocate_timers();
	if (rc) {
		clocksource_unregister(&clocksource_uv);
		generic_interrupt_extension = NULL;
		return rc;
	}

	clock_event_device_uv.mult = div_sc(sn_rtc_cycles_per_second,
				NSEC_PER_SEC, clock_event_device_uv.shift);

	clock_event_device_uv.min_delta_ns = NSEC_PER_SEC /
						sn_rtc_cycles_per_second;

	clock_event_device_uv.max_delta_ns = clocksource_uv.mask *
				(NSEC_PER_SEC / sn_rtc_cycles_per_second);

	rc = schedule_on_each_cpu(uv_rtc_register_clockevents);
	if (rc) {
		clocksource_unregister(&clocksource_uv);
		generic_interrupt_extension = NULL;
		uv_rtc_deallocate_timers();
	}

	return rc;
}
arch_initcall(uv_rtc_setup_clock);
