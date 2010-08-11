#include <linux/module.h>
#include <linux/smp.h>
#include <linux/user.h>
#include <linux/elfcore.h>
#include <linux/sched.h>
#include <linux/in6.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <asm/sections.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/ftrace.h>

extern int dump_fpu(struct pt_regs *, elf_fpregset_t *);

/* platform dependent support */
EXPORT_SYMBOL(dump_fpu);
EXPORT_SYMBOL(kernel_thread);
EXPORT_SYMBOL(strlen);

/* PCI exports */
#ifdef CONFIG_PCI
EXPORT_SYMBOL(pci_alloc_consistent);
EXPORT_SYMBOL(pci_free_consistent);
#endif

/* mem exports */
EXPORT_SYMBOL(memchr);
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(memmove);
EXPORT_SYMBOL(__copy_user);
EXPORT_SYMBOL(__udelay);
EXPORT_SYMBOL(__ndelay);
EXPORT_SYMBOL(__const_udelay);

#define DECLARE_EXPORT(name)		\
	extern void name(void);EXPORT_SYMBOL(name)

DECLARE_EXPORT(__udivsi3);
DECLARE_EXPORT(__sdivsi3);
DECLARE_EXPORT(__lshrsi3);
DECLARE_EXPORT(__ashrsi3);
DECLARE_EXPORT(__ashlsi3);
DECLARE_EXPORT(__ashiftrt_r4_6);
DECLARE_EXPORT(__ashiftrt_r4_7);
DECLARE_EXPORT(__ashiftrt_r4_8);
DECLARE_EXPORT(__ashiftrt_r4_9);
DECLARE_EXPORT(__ashiftrt_r4_10);
DECLARE_EXPORT(__ashiftrt_r4_11);
DECLARE_EXPORT(__ashiftrt_r4_12);
DECLARE_EXPORT(__ashiftrt_r4_13);
DECLARE_EXPORT(__ashiftrt_r4_14);
DECLARE_EXPORT(__ashiftrt_r4_15);
DECLARE_EXPORT(__ashiftrt_r4_20);
DECLARE_EXPORT(__ashiftrt_r4_21);
DECLARE_EXPORT(__ashiftrt_r4_22);
DECLARE_EXPORT(__ashiftrt_r4_23);
DECLARE_EXPORT(__ashiftrt_r4_24);
DECLARE_EXPORT(__ashiftrt_r4_27);
DECLARE_EXPORT(__ashiftrt_r4_30);
DECLARE_EXPORT(__movstr);
DECLARE_EXPORT(__movstrSI8);
DECLARE_EXPORT(__movstrSI12);
DECLARE_EXPORT(__movstrSI16);
DECLARE_EXPORT(__movstrSI20);
DECLARE_EXPORT(__movstrSI24);
DECLARE_EXPORT(__movstrSI28);
DECLARE_EXPORT(__movstrSI32);
DECLARE_EXPORT(__movstrSI36);
DECLARE_EXPORT(__movstrSI40);
DECLARE_EXPORT(__movstrSI44);
DECLARE_EXPORT(__movstrSI48);
DECLARE_EXPORT(__movstrSI52);
DECLARE_EXPORT(__movstrSI56);
DECLARE_EXPORT(__movstrSI60);
DECLARE_EXPORT(__movstr_i4_even);
DECLARE_EXPORT(__movstr_i4_odd);
DECLARE_EXPORT(__movstrSI12_i4);
DECLARE_EXPORT(__movmem);
DECLARE_EXPORT(__movmemSI8);
DECLARE_EXPORT(__movmemSI12);
DECLARE_EXPORT(__movmemSI16);
DECLARE_EXPORT(__movmemSI20);
DECLARE_EXPORT(__movmemSI24);
DECLARE_EXPORT(__movmemSI28);
DECLARE_EXPORT(__movmemSI32);
DECLARE_EXPORT(__movmemSI36);
DECLARE_EXPORT(__movmemSI40);
DECLARE_EXPORT(__movmemSI44);
DECLARE_EXPORT(__movmemSI48);
DECLARE_EXPORT(__movmemSI52);
DECLARE_EXPORT(__movmemSI56);
DECLARE_EXPORT(__movmemSI60);
DECLARE_EXPORT(__movmem_i4_even);
DECLARE_EXPORT(__movmem_i4_odd);
DECLARE_EXPORT(__movmemSI12_i4);
DECLARE_EXPORT(__udiv_qrnnd_16);
DECLARE_EXPORT(__sdivsi3_i4);
DECLARE_EXPORT(__udivsi3_i4);
DECLARE_EXPORT(__sdivsi3_i4i);
DECLARE_EXPORT(__udivsi3_i4i);

#if !defined(CONFIG_CACHE_OFF) && (defined(CONFIG_CPU_SH4) || \
	defined(CONFIG_SH7705_CACHE_32KB))
/* needed by some modules */
EXPORT_SYMBOL(flush_cache_all);
EXPORT_SYMBOL(flush_cache_range);
EXPORT_SYMBOL(flush_dcache_page);
#endif

#ifdef CONFIG_MCOUNT
DECLARE_EXPORT(mcount);
#endif
EXPORT_SYMBOL(csum_partial);
EXPORT_SYMBOL(csum_partial_copy_generic);
#ifdef CONFIG_IPV6
EXPORT_SYMBOL(csum_ipv6_magic);
#endif
EXPORT_SYMBOL(copy_page);
EXPORT_SYMBOL(__clear_user);
EXPORT_SYMBOL(_ebss);
EXPORT_SYMBOL(empty_zero_page);

#ifndef CONFIG_CACHE_OFF
EXPORT_SYMBOL(__flush_purge_region);
EXPORT_SYMBOL(__flush_wback_region);
EXPORT_SYMBOL(__flush_invalidate_region);
#endif
