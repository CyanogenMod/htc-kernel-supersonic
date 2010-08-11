/*
 * BRIEF MODULE DESCRIPTION
 *	Au1xxx irq map table
 *
 * Copyright 2003 Embedded Edge, LLC
 *		dan@embeddededge.com
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
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

struct au1xxx_irqmap __initdata au1xxx_irq_map[] = {
	{ AU1500_GPIO_204, IRQF_TRIGGER_HIGH, 0 },
	{ AU1500_GPIO_201, IRQF_TRIGGER_LOW, 0 },
	{ AU1500_GPIO_202, IRQF_TRIGGER_LOW, 0 },
	{ AU1500_GPIO_203, IRQF_TRIGGER_LOW, 0 },
	{ AU1500_GPIO_205, IRQF_TRIGGER_LOW, 0 },
	{ AU1500_GPIO_207, IRQF_TRIGGER_LOW, 0 },

	{ AU1000_GPIO_0, IRQF_TRIGGER_LOW, 0 },
	{ AU1000_GPIO_1, IRQF_TRIGGER_LOW, 0 },
	{ AU1000_GPIO_2, IRQF_TRIGGER_LOW, 0 },
	{ AU1000_GPIO_3, IRQF_TRIGGER_LOW, 0 },
	{ AU1000_GPIO_4, IRQF_TRIGGER_LOW, 0 }, /* CF interrupt */
	{ AU1000_GPIO_5, IRQF_TRIGGER_LOW, 0 },
};

void __init board_init_irq(void)
{
	au1xxx_setup_irqmap(au1xxx_irq_map, ARRAY_SIZE(au1xxx_irq_map));
}
