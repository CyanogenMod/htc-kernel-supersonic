/*
 *	Copyright (C) 1992, 1998 Linus Torvalds, Ingo Molnar
 *
 * This file contains the lowest level x86_64-specific interrupt
 * entry and irq statistics code. All the remaining irq logic is
 * done by the generic kernel/irq/ code and in the
 * x86_64-specific irq controller code. (e.g. i8259.c and
 * io_apic.c.)
 */

#include <linux/kernel_stat.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ftrace.h>
#include <linux/uaccess.h>
#include <linux/smp.h>
#include <asm/io_apic.h>
#include <asm/idle.h>
#include <asm/apic.h>

DEFINE_PER_CPU_SHARED_ALIGNED(irq_cpustat_t, irq_stat);
EXPORT_PER_CPU_SYMBOL(irq_stat);

DEFINE_PER_CPU(struct pt_regs *, irq_regs);
EXPORT_PER_CPU_SYMBOL(irq_regs);

/*
 * Probabilistic stack overflow check:
 *
 * Only check the stack in process context, because everything else
 * runs on the big interrupt stacks. Checking reliably is too expensive,
 * so we just check from interrupts.
 */
static inline void stack_overflow_check(struct pt_regs *regs)
{
#ifdef CONFIG_DEBUG_STACKOVERFLOW
	u64 curbase = (u64)task_stack_page(current);

	WARN_ONCE(regs->sp >= curbase &&
		  regs->sp <= curbase + THREAD_SIZE &&
		  regs->sp <  curbase + sizeof(struct thread_info) +
					sizeof(struct pt_regs) + 128,

		  "do_IRQ: %s near stack overflow (cur:%Lx,sp:%lx)\n",
			current->comm, curbase, regs->sp);
#endif
}

bool handle_irq(unsigned irq, struct pt_regs *regs)
{
	struct irq_desc *desc;

	stack_overflow_check(regs);

	desc = irq_to_desc(irq);
	if (unlikely(!desc))
		return false;

	generic_handle_irq_desc(irq, desc);
	return true;
}

#ifdef CONFIG_HOTPLUG_CPU
/* A cpu has been removed from cpu_online_mask.  Reset irq affinities. */
void fixup_irqs(void)
{
	unsigned int irq;
	static int warned;
	struct irq_desc *desc;

	for_each_irq_desc(irq, desc) {
		int break_affinity = 0;
		int set_affinity = 1;
		const struct cpumask *affinity;

		if (!desc)
			continue;
		if (irq == 2)
			continue;

		/* interrupt's are disabled at this point */
		spin_lock(&desc->lock);

		affinity = desc->affinity;
		if (!irq_has_action(irq) ||
		    cpumask_equal(affinity, cpu_online_mask)) {
			spin_unlock(&desc->lock);
			continue;
		}

		if (cpumask_any_and(affinity, cpu_online_mask) >= nr_cpu_ids) {
			break_affinity = 1;
			affinity = cpu_all_mask;
		}

		if (desc->chip->mask)
			desc->chip->mask(irq);

		if (desc->chip->set_affinity)
			desc->chip->set_affinity(irq, affinity);
		else if (!(warned++))
			set_affinity = 0;

		if (desc->chip->unmask)
			desc->chip->unmask(irq);

		spin_unlock(&desc->lock);

		if (break_affinity && set_affinity)
			printk("Broke affinity for irq %i\n", irq);
		else if (!set_affinity)
			printk("Cannot set affinity for irq %i\n", irq);
	}

	/* That doesn't seem sufficient.  Give it 1ms. */
	local_irq_enable();
	mdelay(1);
	local_irq_disable();
}
#endif

extern void call_softirq(void);

asmlinkage void do_softirq(void)
{
	__u32 pending;
	unsigned long flags;

	if (in_interrupt())
		return;

	local_irq_save(flags);
	pending = local_softirq_pending();
	/* Switch to interrupt stack */
	if (pending) {
		call_softirq();
		WARN_ON_ONCE(softirq_count());
	}
	local_irq_restore(flags);
}
