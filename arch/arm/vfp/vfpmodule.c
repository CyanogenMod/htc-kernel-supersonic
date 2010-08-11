/*
 *  linux/arch/arm/vfp/vfpmodule.c
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/thread_notify.h>
#include <asm/vfp.h>

#include "vfpinstr.h"
#include "vfp.h"

/*
 * Our undef handlers (in entry.S)
 */
void vfp_testing_entry(void);
void vfp_support_entry(void);
void vfp_null_entry(void);

void (*vfp_vector)(void) = vfp_null_entry;
union vfp_state *last_VFP_context[NR_CPUS];

/*
 * Dual-use variable.
 * Used in startup: set to non-zero if VFP checks fail
 * After startup, holds VFP architecture
 */
unsigned int VFP_arch;

static int vfp_notifier(struct notifier_block *self, unsigned long cmd, void *v)
{
	struct thread_info *thread = v;
	union vfp_state *vfp;
	__u32 cpu = thread->cpu;

	if (likely(cmd == THREAD_NOTIFY_SWITCH)) {
		u32 fpexc = fmrx(FPEXC);

#ifdef CONFIG_SMP
		/*
		 * On SMP, if VFP is enabled, save the old state in
		 * case the thread migrates to a different CPU. The
		 * restoring is done lazily.
		 */
		if ((fpexc & FPEXC_EN) && last_VFP_context[cpu]) {
			vfp_save_state(last_VFP_context[cpu], fpexc);
			last_VFP_context[cpu]->hard.cpu = cpu;
		}
		/*
		 * Thread migration, just force the reloading of the
		 * state on the new CPU in case the VFP registers
		 * contain stale data.
		 */
		if (thread->vfpstate.hard.cpu != cpu)
			last_VFP_context[cpu] = NULL;
#endif

		/*
		 * Always disable VFP so we can lazily save/restore the
		 * old state.
		 */
		fmxr(FPEXC, fpexc & ~FPEXC_EN);
		return NOTIFY_DONE;
	}

	vfp = &thread->vfpstate;
	if (cmd == THREAD_NOTIFY_FLUSH) {
		/*
		 * Per-thread VFP initialisation.
		 */
		memset(vfp, 0, sizeof(union vfp_state));

		vfp->hard.fpexc = FPEXC_EN;
		vfp->hard.fpscr = FPSCR_ROUND_NEAREST;

		/*
		 * Disable VFP to ensure we initialise it first.
		 */
		fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
	}

	/* flush and release case: Per-thread VFP cleanup. */
	if (last_VFP_context[cpu] == vfp)
		last_VFP_context[cpu] = NULL;

	return NOTIFY_DONE;
}

static struct notifier_block vfp_notifier_block = {
	.notifier_call	= vfp_notifier,
};

/*
 * Raise a SIGFPE for the current process.
 * sicode describes the signal being raised.
 */
void vfp_raise_sigfpe(unsigned int sicode, struct pt_regs *regs)
{
	siginfo_t info;

	memset(&info, 0, sizeof(info));

	info.si_signo = SIGFPE;
	info.si_code = sicode;
	info.si_addr = (void __user *)(instruction_pointer(regs) - 4);

	/*
	 * This is the same as NWFPE, because it's not clear what
	 * this is used for
	 */
	current->thread.error_code = 0;
	current->thread.trap_no = 6;

	send_sig_info(SIGFPE, &info, current);
}

static void vfp_panic(char *reason, u32 inst)
{
	int i;

	printk(KERN_ERR "VFP: Error: %s\n", reason);
	printk(KERN_ERR "VFP: EXC 0x%08x SCR 0x%08x INST 0x%08x\n",
		fmrx(FPEXC), fmrx(FPSCR), inst);
	for (i = 0; i < 32; i += 2)
		printk(KERN_ERR "VFP: s%2u: 0x%08x s%2u: 0x%08x\n",
		       i, vfp_get_float(i), i+1, vfp_get_float(i+1));
}

/*
 * Process bitmask of exception conditions.
 */
static void vfp_raise_exceptions(u32 exceptions, u32 inst, u32 fpscr, struct pt_regs *regs)
{
	int si_code = 0;

	pr_debug("VFP: raising exceptions %08x\n", exceptions);

	if (exceptions == VFP_EXCEPTION_ERROR) {
		vfp_panic("unhandled bounce", inst);
		vfp_raise_sigfpe(0, regs);
		return;
	}

	/*
	 * Update the FPSCR with the additional exception flags.
	 * Comparison instructions always return at least one of
	 * these flags set.
	 */
	fpscr |= exceptions;

	fmxr(FPSCR, fpscr);

#define RAISE(stat,en,sig)				\
	if (exceptions & stat && fpscr & en)		\
		si_code = sig;

	/*
	 * These are arranged in priority order, least to highest.
	 */
	RAISE(FPSCR_DZC, FPSCR_DZE, FPE_FLTDIV);
	RAISE(FPSCR_IXC, FPSCR_IXE, FPE_FLTRES);
	RAISE(FPSCR_UFC, FPSCR_UFE, FPE_FLTUND);
	RAISE(FPSCR_OFC, FPSCR_OFE, FPE_FLTOVF);
	RAISE(FPSCR_IOC, FPSCR_IOE, FPE_FLTINV);

	if (si_code)
		vfp_raise_sigfpe(si_code, regs);
}

/*
 * Emulate a VFP instruction.
 */
static u32 vfp_emulate_instruction(u32 inst, u32 fpscr, struct pt_regs *regs)
{
	u32 exceptions = VFP_EXCEPTION_ERROR;

	pr_debug("VFP: emulate: INST=0x%08x SCR=0x%08x\n", inst, fpscr);

	if (INST_CPRTDO(inst)) {
		if (!INST_CPRT(inst)) {
			/*
			 * CPDO
			 */
			if (vfp_single(inst)) {
				exceptions = vfp_single_cpdo(inst, fpscr);
			} else {
				exceptions = vfp_double_cpdo(inst, fpscr);
			}
		} else {
			/*
			 * A CPRT instruction can not appear in FPINST2, nor
			 * can it cause an exception.  Therefore, we do not
			 * have to emulate it.
			 */
		}
	} else {
		/*
		 * A CPDT instruction can not appear in FPINST2, nor can
		 * it cause an exception.  Therefore, we do not have to
		 * emulate it.
		 */
	}
	return exceptions & ~VFP_NAN_FLAG;
}

/*
 * Package up a bounce condition.
 */
void VFP_bounce(u32 trigger, u32 fpexc, struct pt_regs *regs)
{
	u32 fpscr, orig_fpscr, fpsid, exceptions;

	pr_debug("VFP: bounce: trigger %08x fpexc %08x\n", trigger, fpexc);

	/*
	 * At this point, FPEXC can have the following configuration:
	 *
	 *  EX DEX IXE
	 *  0   1   x   - synchronous exception
	 *  1   x   0   - asynchronous exception
	 *  1   x   1   - sychronous on VFP subarch 1 and asynchronous on later
	 *  0   0   1   - synchronous on VFP9 (non-standard subarch 1
	 *                implementation), undefined otherwise
	 *
	 * Clear various bits and enable access to the VFP so we can
	 * handle the bounce.
	 */
	fmxr(FPEXC, fpexc & ~(FPEXC_EX|FPEXC_DEX|FPEXC_FP2V|FPEXC_VV|FPEXC_TRAP_MASK));

	fpsid = fmrx(FPSID);
	orig_fpscr = fpscr = fmrx(FPSCR);

	/*
	 * Check for the special VFP subarch 1 and FPSCR.IXE bit case
	 */
	if ((fpsid & FPSID_ARCH_MASK) == (1 << FPSID_ARCH_BIT)
	    && (fpscr & FPSCR_IXE)) {
		/*
		 * Synchronous exception, emulate the trigger instruction
		 */
		goto emulate;
	}

	if (fpexc & FPEXC_EX) {
#ifndef CONFIG_CPU_FEROCEON
		/*
		 * Asynchronous exception. The instruction is read from FPINST
		 * and the interrupted instruction has to be restarted.
		 */
		trigger = fmrx(FPINST);
		regs->ARM_pc -= 4;
#endif
	} else if (!(fpexc & FPEXC_DEX)) {
		/*
		 * Illegal combination of bits. It can be caused by an
		 * unallocated VFP instruction but with FPSCR.IXE set and not
		 * on VFP subarch 1.
		 */
		 vfp_raise_exceptions(VFP_EXCEPTION_ERROR, trigger, fpscr, regs);
		goto exit;
	}

	/*
	 * Modify fpscr to indicate the number of iterations remaining.
	 * If FPEXC.EX is 0, FPEXC.DEX is 1 and the FPEXC.VV bit indicates
	 * whether FPEXC.VECITR or FPSCR.LEN is used.
	 */
	if (fpexc & (FPEXC_EX | FPEXC_VV)) {
		u32 len;

		len = fpexc + (1 << FPEXC_LENGTH_BIT);

		fpscr &= ~FPSCR_LENGTH_MASK;
		fpscr |= (len & FPEXC_LENGTH_MASK) << (FPSCR_LENGTH_BIT - FPEXC_LENGTH_BIT);
	}

	/*
	 * Handle the first FP instruction.  We used to take note of the
	 * FPEXC bounce reason, but this appears to be unreliable.
	 * Emulate the bounced instruction instead.
	 */
	exceptions = vfp_emulate_instruction(trigger, fpscr, regs);
	if (exceptions)
		vfp_raise_exceptions(exceptions, trigger, orig_fpscr, regs);

	/*
	 * If there isn't a second FP instruction, exit now. Note that
	 * the FPEXC.FP2V bit is valid only if FPEXC.EX is 1.
	 */
	if (fpexc ^ (FPEXC_EX | FPEXC_FP2V))
		goto exit;

	/*
	 * The barrier() here prevents fpinst2 being read
	 * before the condition above.
	 */
	barrier();
	trigger = fmrx(FPINST2);

 emulate:
	exceptions = vfp_emulate_instruction(trigger, orig_fpscr, regs);
	if (exceptions)
		vfp_raise_exceptions(exceptions, trigger, orig_fpscr, regs);
 exit:
	preempt_enable();
}

static void vfp_enable(void *unused)
{
	u32 access = get_copro_access();

	/*
	 * Enable full access to VFP (cp10 and cp11)
	 */
	set_copro_access(access | CPACC_FULL(10) | CPACC_FULL(11));
}

int vfp_flush_context(void)
{
	unsigned long flags;
	struct thread_info *ti;
	u32 fpexc;
	u32 cpu;
	int saved = 0;

	local_irq_save(flags);

	ti = current_thread_info();
	fpexc = fmrx(FPEXC);
	cpu = ti->cpu;

#ifdef CONFIG_SMP
	/* On SMP, if VFP is enabled, save the old state */
	if ((fpexc & FPEXC_EN) && last_VFP_context[cpu]) {
		last_VFP_context[cpu]->hard.cpu = cpu;
#else
	/* If there is a VFP context we must save it. */
	if (last_VFP_context[cpu]) {
		/* Enable VFP so we can save the old state. */
		fmxr(FPEXC, fpexc | FPEXC_EN);
		isb();
#endif
		vfp_save_state(last_VFP_context[cpu], fpexc);

		/* disable, just in case */
		fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
		saved = 1;
	}
	last_VFP_context[cpu] = NULL;

	local_irq_restore(flags);

	return saved;
}

void vfp_reinit(void)
{
	/* ensure we have access to the vfp */
	vfp_enable(NULL);

	/* and disable it to ensure the next usage restores the state */
	fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
}


#ifdef CONFIG_PM
#include <linux/sysdev.h>

static int vfp_pm_suspend(struct sys_device *dev, pm_message_t state)
{
	struct thread_info *ti = current_thread_info();
	u32 fpexc = fmrx(FPEXC);

	/* if vfp is on, then save state for resumption */
	if (fpexc & FPEXC_EN) {
		printk(KERN_DEBUG "%s: saving vfp state\n", __func__);
		vfp_save_state(&ti->vfpstate, fpexc);

		/* disable, just in case */
		fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
	}

	/* clear any information we had about last context state */
	memset(last_VFP_context, 0, sizeof(last_VFP_context));

	return 0;
}

static int vfp_pm_resume(struct sys_device *dev)
{
	/* ensure we have access to the vfp */
	vfp_enable(NULL);

	/* and disable it to ensure the next usage restores the state */
	fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);

	return 0;
}

static struct sysdev_class vfp_pm_sysclass = {
	.name		= "vfp",
	.suspend	= vfp_pm_suspend,
	.resume		= vfp_pm_resume,
};

static struct sys_device vfp_pm_sysdev = {
	.cls	= &vfp_pm_sysclass,
};

static void vfp_pm_init(void)
{
	sysdev_class_register(&vfp_pm_sysclass);
	sysdev_register(&vfp_pm_sysdev);
}


#else
static inline void vfp_pm_init(void) { }
#endif /* CONFIG_PM */

/*
 * Synchronise the hardware VFP state of a thread other than current with the
 * saved one. This function is used by the ptrace mechanism.
 */
#ifdef CONFIG_SMP
void vfp_sync_state(struct thread_info *thread)
{
	/*
	 * On SMP systems, the VFP state is automatically saved at every
	 * context switch. We mark the thread VFP state as belonging to a
	 * non-existent CPU so that the saved one will be reloaded when
	 * needed.
	 */
	thread->vfpstate.hard.cpu = NR_CPUS;
}
#else
void vfp_sync_state(struct thread_info *thread)
{
	unsigned int cpu = get_cpu();
	u32 fpexc = fmrx(FPEXC);

	/*
	 * If VFP is enabled, the previous state was already saved and
	 * last_VFP_context updated.
	 */
	if (fpexc & FPEXC_EN)
		goto out;

	if (!last_VFP_context[cpu])
		goto out;

	/*
	 * Save the last VFP state on this CPU.
	 */
	fmxr(FPEXC, fpexc | FPEXC_EN);
	vfp_save_state(last_VFP_context[cpu], fpexc);
	fmxr(FPEXC, fpexc);

	/*
	 * Set the context to NULL to force a reload the next time the thread
	 * uses the VFP.
	 */
	last_VFP_context[cpu] = NULL;

out:
	put_cpu();
}
#endif

#include <linux/smp.h>

/*
 * VFP support code initialisation.
 */
static int __init vfp_init(void)
{
	unsigned int vfpsid;
	unsigned int cpu_arch = cpu_architecture();

	if (cpu_arch >= CPU_ARCH_ARMv6)
		vfp_enable(NULL);

	/*
	 * First check that there is a VFP that we can use.
	 * The handler is already setup to just log calls, so
	 * we just need to read the VFPSID register.
	 */
	vfp_vector = vfp_testing_entry;
	barrier();
	vfpsid = fmrx(FPSID);
	barrier();
	vfp_vector = vfp_null_entry;

	printk(KERN_INFO "VFP support v0.3: ");
	if (VFP_arch)
		printk("not present\n");
	else if (vfpsid & FPSID_NODOUBLE) {
		printk("no double precision support\n");
	} else {
		smp_call_function(vfp_enable, NULL, 1);

		VFP_arch = (vfpsid & FPSID_ARCH_MASK) >> FPSID_ARCH_BIT;  /* Extract the architecture version */
		printk("implementor %02x architecture %d part %02x variant %x rev %x\n",
			(vfpsid & FPSID_IMPLEMENTER_MASK) >> FPSID_IMPLEMENTER_BIT,
			(vfpsid & FPSID_ARCH_MASK) >> FPSID_ARCH_BIT,
			(vfpsid & FPSID_PART_MASK) >> FPSID_PART_BIT,
			(vfpsid & FPSID_VARIANT_MASK) >> FPSID_VARIANT_BIT,
			(vfpsid & FPSID_REV_MASK) >> FPSID_REV_BIT);

		vfp_vector = vfp_support_entry;

		thread_register_notifier(&vfp_notifier_block);
		vfp_pm_init();

		/*
		 * We detected VFP, and the support code is
		 * in place; report VFP support to userspace.
		 */
		elf_hwcap |= HWCAP_VFP;
#ifdef CONFIG_VFPv3
		if (VFP_arch >= 3) {
			elf_hwcap |= HWCAP_VFPv3;

			/*
			 * Check for VFPv3 D16. CPUs in this configuration
			 * only have 16 x 64bit registers.
			 */
			if (((fmrx(MVFR0) & MVFR0_A_SIMD_MASK)) == 1)
				elf_hwcap |= HWCAP_VFPv3D16;
		}
#endif
#ifdef CONFIG_NEON
		/*
		 * Check for the presence of the Advanced SIMD
		 * load/store instructions, integer and single
		 * precision floating point operations.
		 */
		if ((fmrx(MVFR1) & 0x000fff00) == 0x00011100)
			elf_hwcap |= HWCAP_NEON;
#endif
	}
	return 0;
}

late_initcall(vfp_init);
