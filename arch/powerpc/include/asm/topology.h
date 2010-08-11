#ifndef _ASM_POWERPC_TOPOLOGY_H
#define _ASM_POWERPC_TOPOLOGY_H
#ifdef __KERNEL__


struct sys_device;
struct device_node;

#ifdef CONFIG_NUMA

#include <asm/mmzone.h>

static inline int cpu_to_node(int cpu)
{
	return numa_cpu_lookup_table[cpu];
}

#define parent_node(node)	(node)

#define cpumask_of_node(node) (&numa_cpumask_lookup_table[node])

int of_node_to_nid(struct device_node *device);

struct pci_bus;
#ifdef CONFIG_PCI
extern int pcibus_to_node(struct pci_bus *bus);
#else
static inline int pcibus_to_node(struct pci_bus *bus)
{
	return -1;
}
#endif

#define cpumask_of_pcibus(bus)	(pcibus_to_node(bus) == -1 ?		\
				 cpu_all_mask :				\
				 cpumask_of_node(pcibus_to_node(bus)))

/* sched_domains SD_NODE_INIT for PPC64 machines */
#define SD_NODE_INIT (struct sched_domain) {		\
	.parent			= NULL,			\
	.child			= NULL,			\
	.groups			= NULL,			\
	.min_interval		= 8,			\
	.max_interval		= 32,			\
	.busy_factor		= 32,			\
	.imbalance_pct		= 125,			\
	.cache_nice_tries	= 1,			\
	.busy_idx		= 3,			\
	.idle_idx		= 1,			\
	.newidle_idx		= 0,			\
	.wake_idx		= 0,			\
	.flags			= SD_LOAD_BALANCE	\
				| SD_BALANCE_EXEC	\
				| SD_BALANCE_FORK	\
				| SD_BALANCE_NEWIDLE	\
				| SD_SERIALIZE,		\
	.last_balance		= jiffies,		\
	.balance_interval	= 1,			\
	.nr_balance_failed	= 0,			\
}

extern void __init dump_numa_cpu_topology(void);

extern int sysfs_add_device_to_node(struct sys_device *dev, int nid);
extern void sysfs_remove_device_from_node(struct sys_device *dev, int nid);

#else

static inline int of_node_to_nid(struct device_node *device)
{
	return 0;
}

static inline void dump_numa_cpu_topology(void) {}

static inline int sysfs_add_device_to_node(struct sys_device *dev, int nid)
{
	return 0;
}

static inline void sysfs_remove_device_from_node(struct sys_device *dev,
						int nid)
{
}

#endif /* CONFIG_NUMA */

#include <asm-generic/topology.h>

#ifdef CONFIG_SMP
#include <asm/cputable.h>
#define smt_capable()		(cpu_has_feature(CPU_FTR_SMT))

#ifdef CONFIG_PPC64
#include <asm/smp.h>

#define topology_thread_cpumask(cpu)	(&per_cpu(cpu_sibling_map, cpu))
#define topology_core_cpumask(cpu)	(&per_cpu(cpu_core_map, cpu))
#define topology_core_id(cpu)		(cpu_to_core_id(cpu))
#endif
#endif

#endif /* __KERNEL__ */
#endif	/* _ASM_POWERPC_TOPOLOGY_H */
