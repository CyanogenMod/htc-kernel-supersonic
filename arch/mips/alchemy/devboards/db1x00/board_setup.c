/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Alchemy Db1x00 board setup.
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

#include <linux/gpio.h>
#include <linux/init.h>

#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-db1x00/db1x00.h>

#include <prom.h>


static BCSR * const bcsr = (BCSR *)BCSR_KSEG1_ADDR;

const char *get_system_type(void)
{
#ifdef CONFIG_MIPS_BOSPORUS
	return "Alchemy Bosporus Gateway Reference";
#else
	return "Alchemy Db1x00";
#endif
}

void board_reset(void)
{
	/* Hit BCSR.SW_RESET[RESET] */
	bcsr->swreset = 0x0000;
}

void __init board_setup(void)
{
	u32 pin_func = 0;
	char *argptr;

	argptr = prom_getcmdline();
#ifdef CONFIG_SERIAL_8250_CONSOLE
	argptr = strstr(argptr, "console=");
	if (argptr == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " console=ttyS0,115200");
	}
#endif

#ifdef CONFIG_FB_AU1100
	argptr = strstr(argptr, "video=");
	if (argptr == NULL) {
		argptr = prom_getcmdline();
		/* default panel */
		/*strcat(argptr, " video=au1100fb:panel:Sharp_320x240_16");*/
	}
#endif

#if defined(CONFIG_SOUND_AU1X00) && !defined(CONFIG_SOC_AU1000)
	/* au1000 does not support vra, au1500 and au1100 do */
	strcat(argptr, " au1000_audio=vra");
	argptr = prom_getcmdline();
#endif

	/* Not valid for Au1550 */
#if defined(CONFIG_IRDA) && \
   (defined(CONFIG_SOC_AU1000) || defined(CONFIG_SOC_AU1100))
	/* Set IRFIRSEL instead of GPIO15 */
	pin_func = au_readl(SYS_PINFUNC) | SYS_PF_IRF;
	au_writel(pin_func, SYS_PINFUNC);
	/* Power off until the driver is in use */
	bcsr->resets &= ~BCSR_RESETS_IRDA_MODE_MASK;
	bcsr->resets |=  BCSR_RESETS_IRDA_MODE_OFF;
	au_sync();
#endif
	bcsr->pcmcia = 0x0000; /* turn off PCMCIA power */

	/* Enable GPIO[31:0] inputs */
	alchemy_gpio1_input_enable();

#ifdef CONFIG_MIPS_MIRAGE
	/* GPIO[20] is output */
	alchemy_gpio_direction_output(20, 0);

	/* Set GPIO[210:208] instead of SSI_0 */
	pin_func = au_readl(SYS_PINFUNC) | SYS_PF_S0;

	/* Set GPIO[215:211] for LEDs */
	pin_func |= 5 << 2;

	/* Set GPIO[214:213] for more LEDs */
	pin_func |= 5 << 12;

	/* Set GPIO[207:200] instead of PCMCIA/LCD */
	pin_func |= SYS_PF_LCD | SYS_PF_PC;
	au_writel(pin_func, SYS_PINFUNC);

	/*
	 * Enable speaker amplifier.  This should
	 * be part of the audio driver.
	 */
	alchemy_gpio_direction_output(209, 1);
#endif

	au_sync();

#ifdef CONFIG_MIPS_DB1000
	printk(KERN_INFO "AMD Alchemy Au1000/Db1000 Board\n");
#endif
#ifdef CONFIG_MIPS_DB1500
	printk(KERN_INFO "AMD Alchemy Au1500/Db1500 Board\n");
#endif
#ifdef CONFIG_MIPS_DB1100
	printk(KERN_INFO "AMD Alchemy Au1100/Db1100 Board\n");
#endif
#ifdef CONFIG_MIPS_BOSPORUS
	printk(KERN_INFO "AMD Alchemy Bosporus Board\n");
#endif
#ifdef CONFIG_MIPS_MIRAGE
	printk(KERN_INFO "AMD Alchemy Mirage Board\n");
#endif
#ifdef CONFIG_MIPS_DB1550
	printk(KERN_INFO "AMD Alchemy Au1550/Db1550 Board\n");
#endif
}
