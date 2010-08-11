/* /arch/arm/mach-msm/include/mach/tpa6130.h
 *
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/


/*
 * Definitions for tpa6130a headset amp chip.
 */
#ifndef TPA6130_H
#define TPA6130_H

#include <linux/ioctl.h>

#define TPA6130_I2C_NAME "tpa6130"
void set_headset_amp(int on);

struct tpa6130_platform_data {
	int gpio_hp_sd;
	int enable_rpc_server;
};

struct rpc_headset_amp_ctl_args {
	int on;
};
#endif

