/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Alchemy Pb1550 board setup.
 *
 * Copyright 2000, 2008 MontaVista Software Inc.
 * Author: MontaVista Software, Inc. <source@mvista.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/interrupt.h>

#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-pb1x00/pb1550.h>

#include <prom.h>


char irq_tab_alchemy[][5] __initdata = {
	[12] = { -1, INTB, INTC, INTD, INTA }, /* IDSEL 12 - PCI slot 2 (left)  */
	[13] = { -1, INTA, INTB, INTC, INTD }, /* IDSEL 13 - PCI slot 1 (right) */
};

struct au1xxx_irqmap __initdata au1xxx_irq_map[] = {
	{ AU1000_GPIO_0, IRQF_TRIGGER_LOW, 0 },
	{ AU1000_GPIO_1, IRQF_TRIGGER_LOW, 0 },
};

const char *get_system_type(void)
{
	return "Alchemy Pb1550";
}

void board_reset(void)
{
	/* Hit BCSR.SYSTEM[RESET] */
	au_writew(au_readw(0xAF00001C) & ~BCSR_SYSTEM_RESET, 0xAF00001C);
}

void __init board_init_irq(void)
{
	au1xxx_setup_irqmap(au1xxx_irq_map, ARRAY_SIZE(au1xxx_irq_map));
}

void __init board_setup(void)
{
	u32 pin_func;

#ifdef CONFIG_SERIAL_8250_CONSOLE
	char *argptr;
	argptr = prom_getcmdline();
	argptr = strstr(argptr, "console=");
	if (argptr == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " console=ttyS0,115200");
	}
#endif

	/*
	 * Enable PSC1 SYNC for AC'97.  Normaly done in audio driver,
	 * but it is board specific code, so put it here.
	 */
	pin_func = au_readl(SYS_PINFUNC);
	au_sync();
	pin_func |= SYS_PF_MUST_BE_SET | SYS_PF_PSC1_S1;
	au_writel(pin_func, SYS_PINFUNC);

	au_writel(0, (u32)bcsr | 0x10); /* turn off PCMCIA power */
	au_sync();

	printk(KERN_INFO "AMD Alchemy Pb1550 Board\n");
}
