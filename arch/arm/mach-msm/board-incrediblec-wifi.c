/* arch/arm/mach-msm/board-incrediblec-wifi.c
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


/* linux/arch/arm/mach-msm/board-incrediblec-wifi.c
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>

#include "board-incrediblec.h"

int incrediblec_wifi_power(int on);
int incrediblec_wifi_reset(int on);
int incrediblec_wifi_set_carddetect(int on);

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *incrediblec_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int __init incrediblec_init_wifi_mem(void)
{
	int i;

	for(i=0;( i < WLAN_SKB_BUF_NUM );i++) {
		if (i < (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096);
		else
			wlan_static_skb[i] = dev_alloc_skb(8192);
	}
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
	return 0;
}

static struct resource incrediblec_wifi_resources[] = {
	[0] = {
		.name		= "bcm4329_wlan_irq",
		.start		= MSM_GPIO_TO_INT(INCREDIBLEC_GPIO_WIFI_IRQ),
		.end		= MSM_GPIO_TO_INT(INCREDIBLEC_GPIO_WIFI_IRQ),
		.flags          = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
	},
};

static struct wifi_platform_data incrediblec_wifi_control = {
	.set_power      = incrediblec_wifi_power,
	.set_reset      = incrediblec_wifi_reset,
	.set_carddetect = incrediblec_wifi_set_carddetect,
	.mem_prealloc   = incrediblec_wifi_mem_prealloc,
};

static struct platform_device incrediblec_wifi_device = {
        .name           = "bcm4329_wlan",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(incrediblec_wifi_resources),
        .resource       = incrediblec_wifi_resources,
        .dev            = {
                .platform_data = &incrediblec_wifi_control,
        },
};

extern unsigned char *get_wifi_nvs_ram(void);

static unsigned incrediblec_wifi_update_nvs(char *str)
{
#define NVS_LEN_OFFSET		0x0C
#define NVS_DATA_OFFSET		0x40
	unsigned char *ptr;
	unsigned len;

	if (!str)
		return -EINVAL;
	ptr = get_wifi_nvs_ram();
	/* Size in format LE assumed */
	memcpy(&len, ptr + NVS_LEN_OFFSET, sizeof(len));

	/* the last bye in NVRAM is 0, trim it */
	if (ptr[NVS_DATA_OFFSET + len -1] == 0)
		len -= 1;

	strcpy(ptr + NVS_DATA_OFFSET + len, str);
	len += strlen(str);
	memcpy(ptr + NVS_LEN_OFFSET, &len, sizeof(len));
	return 0;
}

static unsigned strip_nvs_param(char* param)
{
	unsigned char *nvs_data;

	unsigned param_len;
	int start_idx, end_idx;

	unsigned char *ptr;
	unsigned len;

	if (!param)
		return -EINVAL;
	ptr = get_wifi_nvs_ram();
	/* Size in format LE assumed */
	memcpy(&len, ptr + NVS_LEN_OFFSET, sizeof(len));

	/* the last bye in NVRAM is 0, trim it */
	if (ptr[NVS_DATA_OFFSET + len -1] == 0)
		len -= 1;

	nvs_data = ptr + NVS_DATA_OFFSET;

	param_len = strlen(param);

	/* search param */
	for (start_idx = 0; start_idx < len - param_len; start_idx++) {
		if (memcmp(&nvs_data[start_idx], param, param_len) == 0) {
			break;
		}
	}

	end_idx = 0;
	if (start_idx < len - param_len) {
		/* search end-of-line */
		for (end_idx = start_idx + param_len; end_idx < len; end_idx++) {
			if (nvs_data[end_idx] == '\n' || nvs_data[end_idx] == 0) {
				break;
			}
		}
	}

	if (start_idx < end_idx) {
		/* move the remain data forward */
		for (; end_idx + 1 < len; start_idx++, end_idx++) {
			nvs_data[start_idx] = nvs_data[end_idx+1];
		}
		len = len - (end_idx - start_idx + 1);
		memcpy(ptr + NVS_LEN_OFFSET, &len, sizeof(len));
	}
	return 0;
}

static int __init incrediblec_wifi_init(void)
{
	int ret;

	if (!machine_is_incrediblec())
		return 0;

	printk("%s: start\n", __func__);
	incrediblec_wifi_update_nvs("sd_oobonly=1\n");
	incrediblec_wifi_update_nvs("btc_params80=0\n");

	strip_nvs_param("pa0maxpwr");
	incrediblec_wifi_update_nvs("pa0maxpwr=78\n");

	strip_nvs_param("mcs2gpo0");
	incrediblec_wifi_update_nvs("mcs2gpo0=0xCCCC\n");

	strip_nvs_param("mcs2gpo1");
	incrediblec_wifi_update_nvs("mcs2gpo1=0xCCCC\n");

	strip_nvs_param("rxpo2g");
	incrediblec_wifi_update_nvs("rxpo2g=0\n");

	incrediblec_init_wifi_mem();
	ret = platform_device_register(&incrediblec_wifi_device);
        return ret;
}

device_initcall(incrediblec_wifi_init);
