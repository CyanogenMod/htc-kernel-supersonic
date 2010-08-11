/* Board-specific reboot/shutdown routines
 * Copyright (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 *
 * Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 * Author: Wu Zhangjin, wuzj@lemote.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <loongson.h>

void mach_prepare_reboot(void)
{
	BONITO_BONGENCFG &= ~(1 << 2);
	BONITO_BONGENCFG |= (1 << 2);
}

void mach_prepare_shutdown(void)
{
}
