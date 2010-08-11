#ifndef __ASM_SH_CPU_SH4_DMA_SH7780_H
#define __ASM_SH_CPU_SH4_DMA_SH7780_H

#if defined(CONFIG_CPU_SUBTYPE_SH7343) || \
	defined(CONFIG_CPU_SUBTYPE_SH7722) || \
	defined(CONFIG_CPU_SUBTYPE_SH7730)
#define DMTE0_IRQ	48
#define DMTE4_IRQ	76
#define DMAE0_IRQ	78	/* DMA Error IRQ*/
#define SH_DMAC_BASE0	0xFE008020
#define SH_DMARS_BASE	0xFE009000
#elif defined(CONFIG_CPU_SUBTYPE_SH7763) || \
	defined(CONFIG_CPU_SUBTYPE_SH7764)
#define DMTE0_IRQ	34
#define DMTE4_IRQ	44
#define DMAE0_IRQ	38
#define SH_DMAC_BASE0	0xFF608020
#define SH_DMARS_BASE	0xFF609000
#elif defined(CONFIG_CPU_SUBTYPE_SH7723) || \
      defined(CONFIG_CPU_SUBTYPE_SH7724)
#define DMTE0_IRQ	48	/* DMAC0A*/
#define DMTE4_IRQ	40	/* DMAC0B */
#define DMTE6_IRQ	42
#define DMTE8_IRQ	76	/* DMAC1A */
#define DMTE9_IRQ	77
#define DMTE10_IRQ	72	/* DMAC1B */
#define DMTE11_IRQ	73
#define DMAE0_IRQ	78	/* DMA Error IRQ*/
#define DMAE1_IRQ	74	/* DMA Error IRQ*/
#define SH_DMAC_BASE0	0xFE008020
#define SH_DMAC_BASE1	0xFDC08020
#define SH_DMARS_BASE	0xFDC09000
#elif defined(CONFIG_CPU_SUBTYPE_SH7780)
#define DMTE0_IRQ	34
#define DMTE4_IRQ	44
#define DMTE6_IRQ	46
#define DMTE8_IRQ	92
#define DMTE9_IRQ	93
#define DMTE10_IRQ	94
#define DMTE11_IRQ	95
#define DMAE0_IRQ	38	/* DMA Error IRQ */
#define SH_DMAC_BASE0	0xFC808020
#define SH_DMAC_BASE1	0xFC818020
#define SH_DMARS_BASE	0xFC809000
#else /* SH7785 */
#define DMTE0_IRQ	33
#define DMTE4_IRQ	37
#define DMTE6_IRQ	52
#define DMTE8_IRQ	54
#define DMTE9_IRQ	55
#define DMTE10_IRQ	56
#define DMTE11_IRQ	57
#define DMAE0_IRQ	39	/* DMA Error IRQ0 */
#define DMAE1_IRQ	58	/* DMA Error IRQ1 */
#define SH_DMAC_BASE0	0xFC808020
#define SH_DMAC_BASE1	0xFCC08020
#define SH_DMARS_BASE	0xFC809000
#endif

#define REQ_HE	0x000000C0
#define REQ_H	0x00000080
#define REQ_LE	0x00000040
#define TM_BURST 0x0000020
#define TS_8	0x00000000
#define TS_16	0x00000008
#define TS_32	0x00000010
#define TS_16BLK	0x00000018
#define TS_32BLK	0x00100000

/*
 * The SuperH DMAC supports a number of transmit sizes, we list them here,
 * with their respective values as they appear in the CHCR registers.
 *
 * Defaults to a 64-bit transfer size.
 */
enum {
	XMIT_SZ_8BIT,
	XMIT_SZ_16BIT,
	XMIT_SZ_32BIT,
	XMIT_SZ_128BIT,
	XMIT_SZ_256BIT,
};

/*
 * The DMA count is defined as the number of bytes to transfer.
 */
static unsigned int ts_shift[] __maybe_unused = {
	[XMIT_SZ_8BIT]		= 0,
	[XMIT_SZ_16BIT]		= 1,
	[XMIT_SZ_32BIT]		= 2,
	[XMIT_SZ_128BIT]	= 4,
	[XMIT_SZ_256BIT]	= 5,
};

#endif /* __ASM_SH_CPU_SH4_DMA_SH7780_H */
