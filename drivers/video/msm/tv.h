/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __TV_H
#define __TV_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/fb.h>

#include <mach/hardware.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/earlysuspend.h>

enum {
	NTSC_M,		/* North America, Korea */
	NTSC_J,		/* Japan */
	PAL_BDGHIN,	/* Non-argentina PAL-N */
	PAL_M,		/* PAL-M */
	PAL_N,		/* Argentina PAL-N */

	NUM_OF_TV_MODE
};

// 3.57954545 Mhz
#define TVENC_CTL_TV_MODE_NTSC_M_PAL60  0
// 3.57961149 Mhz
#define TVENC_CTL_TV_MODE_PAL_M             BIT(0)
//non-Argintina = 4.3361875 Mhz
#define TVENC_CTL_TV_MODE_PAL_BDGHIN        BIT(1)
//Argentina = 3.582055625 Mhz
#define TVENC_CTL_TV_MODE_PAL_N             (BIT(1)|BIT(0))

#define TVENC_CTL_ENC_EN                    BIT(2)
#define TVENC_CTL_CC_EN                     BIT(3)
#define TVENC_CTL_CGMS_EN                   BIT(4)
#define TVENC_CTL_MACRO_EN                  BIT(5)
#define TVENC_CTL_Y_FILTER_W_NOTCH          BIT(6)
#define TVENC_CTL_Y_FILTER_WO_NOTCH         0
#define TVENC_CTL_Y_FILTER_EN               BIT(7)
#define TVENC_CTL_CR_FILTER_EN              BIT(8)
#define TVENC_CTL_CB_FILTER_EN              BIT(9)
#define TVENC_CTL_SINX_FILTER_EN            BIT(10)
#define TVENC_CTL_TEST_PATT_EN              BIT(11)
#define TVENC_CTL_OUTPUT_INV                BIT(12)
#define TVENC_CTL_PAL60_MODE                BIT(13)
#define TVENC_CTL_NTSCJ_MODE                BIT(14)
#define TVENC_CTL_TPG_CLRBAR                0
#define TVENC_CTL_TPG_MODRAMP               BIT(15)
#define TVENC_CTL_TPG_REDCLR                BIT(16)

#define MSM_TV_ENC_CTL			0x00
#define MSM_TV_LEVEL			0x04
#define MSM_TV_GAIN			0x08
#define MSM_TV_OFFSET			0x0c
#define MSM_TV_CGMS			0x10
#define MSM_TV_SYNC_1			0x14
#define MSM_TV_SYNC_2			0x18
#define MSM_TV_SYNC_3			0x1c
#define MSM_TV_SYNC_4			0x20
#define MSM_TV_SYNC_5			0x24
#define MSM_TV_SYNC_6			0x28
#define MSM_TV_SYNC_7			0x2c
#define MSM_TV_BURST_V1			0x30
#define MSM_TV_BURST_V2			0x34
#define MSM_TV_BURST_V3			0x38
#define MSM_TV_BURST_V4			0x3c
#define MSM_TV_BURST_H			0x40
#define MSM_TV_SOL_REQ_ODD		0x44
#define MSM_TV_SOL_REQ_EVEN		0x48
#define MSM_TV_DAC_CTL			0x4c
#define MSM_TV_TEST_MUX			0x50
#define MSM_TV_TEST_MODE		0x54
#define MSM_TV_TEST_MISR_RESET		0x58
#define MSM_TV_TEST_EXPORT_MISR		0x5c
#define MSM_TV_TEST_MISR_CURR_VAL	0x60
#define MSM_TV_TEST_SOF_CFG		0x64
#define MSM_TV_DAC_INTF			0x100

#define TV_SCREEN_WIDTH			720
#define TV_SCREEN_HEIGHT_NTSC		480
#define TV_SCREEN_HEIGHT_PAL		576

enum {
        TV_LANDSCAPE,
        TV_PORTRAIT,
        TV_LANDSCAPE_FULL,
        TV_PORTRAIT_FULL,
};

int tvenc_set_mode(int mode);
int tvenc_on(struct msm_panel_data *panel);
int tvenc_off(struct msm_panel_data *panel);
void tvenc_enable_amplifier(int on_off);

#endif /* __TV_H */
