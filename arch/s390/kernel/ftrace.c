/*
 * Dynamic function tracer architecture backend.
 *
 * Copyright IBM Corp. 2009
 *
 *   Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>,
 *
 */

#include <linux/hardirq.h>
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <trace/syscall.h>
#include <asm/lowcore.h>

#ifdef CONFIG_DYNAMIC_FTRACE

void ftrace_disable_code(void);
void ftrace_disable_return(void);
void ftrace_call_code(void);
void ftrace_nop_code(void);

#define FTRACE_INSN_SIZE 4

#ifdef CONFIG_64BIT

asm(
	"	.align	4\n"
	"ftrace_disable_code:\n"
	"	j	0f\n"
	"	.word	0x0024\n"
	"	lg	%r1,"__stringify(__LC_FTRACE_FUNC)"\n"
	"	basr	%r14,%r1\n"
	"ftrace_disable_return:\n"
	"	lg	%r14,8(15)\n"
	"	lgr	%r0,%r0\n"
	"0:\n");

asm(
	"	.align	4\n"
	"ftrace_nop_code:\n"
	"	j	.+"__stringify(MCOUNT_INSN_SIZE)"\n");

asm(
	"	.align	4\n"
	"ftrace_call_code:\n"
	"	stg	%r14,8(%r15)\n");

#else /* CONFIG_64BIT */

asm(
	"	.align	4\n"
	"ftrace_disable_code:\n"
	"	j	0f\n"
	"	l	%r1,"__stringify(__LC_FTRACE_FUNC)"\n"
	"	basr	%r14,%r1\n"
	"ftrace_disable_return:\n"
	"	l	%r14,4(%r15)\n"
	"	j	0f\n"
	"	bcr	0,%r7\n"
	"	bcr	0,%r7\n"
	"	bcr	0,%r7\n"
	"	bcr	0,%r7\n"
	"	bcr	0,%r7\n"
	"	bcr	0,%r7\n"
	"0:\n");

asm(
	"	.align	4\n"
	"ftrace_nop_code:\n"
	"	j	.+"__stringify(MCOUNT_INSN_SIZE)"\n");

asm(
	"	.align	4\n"
	"ftrace_call_code:\n"
	"	st	%r14,4(%r15)\n");

#endif /* CONFIG_64BIT */

static int ftrace_modify_code(unsigned long ip,
			      void *old_code, int old_size,
			      void *new_code, int new_size)
{
	unsigned char replaced[MCOUNT_INSN_SIZE];

	/*
	 * Note: Due to modules code can disappear and change.
	 *  We need to protect against faulting as well as code
	 *  changing. We do this by using the probe_kernel_*
	 *  functions.
	 *  This however is just a simple sanity check.
	 */
	if (probe_kernel_read(replaced, (void *)ip, old_size))
		return -EFAULT;
	if (memcmp(replaced, old_code, old_size) != 0)
		return -EINVAL;
	if (probe_kernel_write((void *)ip, new_code, new_size))
		return -EPERM;
	return 0;
}

static int ftrace_make_initial_nop(struct module *mod, struct dyn_ftrace *rec,
				   unsigned long addr)
{
	return ftrace_modify_code(rec->ip,
				  ftrace_call_code, FTRACE_INSN_SIZE,
				  ftrace_disable_code, MCOUNT_INSN_SIZE);
}

int ftrace_make_nop(struct module *mod, struct dyn_ftrace *rec,
		    unsigned long addr)
{
	if (addr == MCOUNT_ADDR)
		return ftrace_make_initial_nop(mod, rec, addr);
	return ftrace_modify_code(rec->ip,
				  ftrace_call_code, FTRACE_INSN_SIZE,
				  ftrace_nop_code, FTRACE_INSN_SIZE);
}

int ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	return ftrace_modify_code(rec->ip,
				  ftrace_nop_code, FTRACE_INSN_SIZE,
				  ftrace_call_code, FTRACE_INSN_SIZE);
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	ftrace_dyn_func = (unsigned long)func;
	return 0;
}

int __init ftrace_dyn_arch_init(void *data)
{
	*(unsigned long *)data = 0;
	return 0;
}

#endif /* CONFIG_DYNAMIC_FTRACE */

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
#ifdef CONFIG_DYNAMIC_FTRACE
/*
 * Patch the kernel code at ftrace_graph_caller location:
 * The instruction there is branch relative on condition. The condition mask
 * is either all ones (always branch aka disable ftrace_graph_caller) or all
 * zeroes (nop aka enable ftrace_graph_caller).
 * Instruction format for brc is a7m4xxxx where m is the condition mask.
 */
int ftrace_enable_ftrace_graph_caller(void)
{
	unsigned short opcode = 0xa704;

	return probe_kernel_write(ftrace_graph_caller, &opcode, sizeof(opcode));
}

int ftrace_disable_ftrace_graph_caller(void)
{
	unsigned short opcode = 0xa7f4;

	return probe_kernel_write(ftrace_graph_caller, &opcode, sizeof(opcode));
}

static inline unsigned long ftrace_mcount_call_adjust(unsigned long addr)
{
	return addr - (ftrace_disable_return - ftrace_disable_code);
}

#else /* CONFIG_DYNAMIC_FTRACE */

static inline unsigned long ftrace_mcount_call_adjust(unsigned long addr)
{
	return addr - MCOUNT_OFFSET_RET;
}

#endif /* CONFIG_DYNAMIC_FTRACE */

/*
 * Hook the return address and push it in the stack of return addresses
 * in current thread info.
 */
unsigned long prepare_ftrace_return(unsigned long ip, unsigned long parent)
{
	struct ftrace_graph_ent trace;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		goto out;
	if (ftrace_push_return_trace(parent, ip, &trace.depth, 0) == -EBUSY)
		goto out;
	trace.func = ftrace_mcount_call_adjust(ip) & PSW_ADDR_INSN;
	/* Only trace if the calling function expects to. */
	if (!ftrace_graph_entry(&trace)) {
		current->curr_ret_stack--;
		goto out;
	}
	parent = (unsigned long)return_to_handler;
out:
	return parent;
}
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */

#ifdef CONFIG_FTRACE_SYSCALLS

extern unsigned long __start_syscalls_metadata[];
extern unsigned long __stop_syscalls_metadata[];
extern unsigned int sys_call_table[];

static struct syscall_metadata **syscalls_metadata;

struct syscall_metadata *syscall_nr_to_meta(int nr)
{
	if (!syscalls_metadata || nr >= NR_syscalls || nr < 0)
		return NULL;

	return syscalls_metadata[nr];
}

int syscall_name_to_nr(char *name)
{
	int i;

	if (!syscalls_metadata)
		return -1;
	for (i = 0; i < NR_syscalls; i++)
		if (syscalls_metadata[i])
			if (!strcmp(syscalls_metadata[i]->name, name))
				return i;
	return -1;
}

void set_syscall_enter_id(int num, int id)
{
	syscalls_metadata[num]->enter_id = id;
}

void set_syscall_exit_id(int num, int id)
{
	syscalls_metadata[num]->exit_id = id;
}

static struct syscall_metadata *find_syscall_meta(unsigned long syscall)
{
	struct syscall_metadata *start;
	struct syscall_metadata *stop;
	char str[KSYM_SYMBOL_LEN];

	start = (struct syscall_metadata *)__start_syscalls_metadata;
	stop = (struct syscall_metadata *)__stop_syscalls_metadata;
	kallsyms_lookup(syscall, NULL, NULL, NULL, str);

	for ( ; start < stop; start++) {
		if (start->name && !strcmp(start->name + 3, str + 3))
			return start;
	}
	return NULL;
}

static int __init arch_init_ftrace_syscalls(void)
{
	struct syscall_metadata *meta;
	int i;
	syscalls_metadata = kzalloc(sizeof(*syscalls_metadata) * NR_syscalls,
				    GFP_KERNEL);
	if (!syscalls_metadata)
		return -ENOMEM;
	for (i = 0; i < NR_syscalls; i++) {
		meta = find_syscall_meta((unsigned long)sys_call_table[i]);
		syscalls_metadata[i] = meta;
	}
	return 0;
}
arch_initcall(arch_init_ftrace_syscalls);
#endif
