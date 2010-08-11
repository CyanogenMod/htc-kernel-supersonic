#ifndef ASMARM_PCI_H
#define ASMARM_PCI_H

#ifdef __KERNEL__
#include <asm-generic/pci-dma-compat.h>

#include <mach/hardware.h> /* for PCIBIOS_MIN_* */

#ifdef CONFIG_PCI_HOST_ITE8152
/* ITE bridge requires setting latency timer to avoid early bus access
   termination by PIC bus mater devices
*/
extern void pcibios_set_master(struct pci_dev *dev);
#else
static inline void pcibios_set_master(struct pci_dev *dev)
{
	/* No special bus mastering setup handling */
}
#endif

static inline void pcibios_penalize_isa_irq(int irq, int active)
{
	/* We don't do dynamic PCI IRQ allocation */
}

/*
 * The PCI address space does equal the physical memory address space.
 * The networking and block device layers use this boolean for bounce
 * buffer decisions.
 */
#define PCI_DMA_BUS_IS_PHYS     (1)

/*
 * Whether pci_unmap_{single,page} is a nop depends upon the
 * configuration.
 */
#define DECLARE_PCI_UNMAP_ADDR(ADDR_NAME)	dma_addr_t ADDR_NAME;
#define DECLARE_PCI_UNMAP_LEN(LEN_NAME)		__u32 LEN_NAME;
#define pci_unmap_addr(PTR, ADDR_NAME)		((PTR)->ADDR_NAME)
#define pci_unmap_addr_set(PTR, ADDR_NAME, VAL)	(((PTR)->ADDR_NAME) = (VAL))
#define pci_unmap_len(PTR, LEN_NAME)		((PTR)->LEN_NAME)
#define pci_unmap_len_set(PTR, LEN_NAME, VAL)	(((PTR)->LEN_NAME) = (VAL))

#ifdef CONFIG_PCI
static inline void pci_dma_burst_advice(struct pci_dev *pdev,
					enum pci_dma_burst_strategy *strat,
					unsigned long *strategy_parameter)
{
	*strat = PCI_DMA_BURST_INFINITY;
	*strategy_parameter = ~0UL;
}
#endif

#define HAVE_PCI_MMAP
extern int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
                               enum pci_mmap_state mmap_state, int write_combine);

extern void
pcibios_resource_to_bus(struct pci_dev *dev, struct pci_bus_region *region,
			 struct resource *res);

extern void
pcibios_bus_to_resource(struct pci_dev *dev, struct resource *res,
			struct pci_bus_region *region);

/*
 * Dummy implementation; always return 0.
 */
static inline int pci_get_legacy_ide_irq(struct pci_dev *dev, int channel)
{
	return 0;
}

#endif /* __KERNEL__ */
 
#endif
