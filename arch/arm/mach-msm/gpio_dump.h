#ifndef __ASM__ARCH_MSM_GPIO_DUMP_H
#define __ASM__ARCH_MSM_GPIO_DUMP_H

#define HTC_GPIO_CFG_BASE_ADDR	    (MSM_SHARED_RAM_BASE + 0x000F8000)
#define HTC_GPIO_CFG_SIZE	    (0x000007F8)
#define HTC_GPIO_CFG_ADDR(mode)	    (HTC_GPIO_CFG_BASE_ADDR + \
				    ((HTC_GPIO_CFG_SIZE / 2) * mode))

#define GPIO_CFG_INVALID    15
#define GPIO_CFG_OWNER      14
#define GPIO_CFG_DRVSTR     11
#define GPIO_CFG_PULL       9
#define GPIO_CFG_DIR        8
#define GPIO_CFG_RMT        4
#define GPIO_CFG_FUNC       0

unsigned int htc_smem_gpio_cfg(unsigned int num, unsigned int mode);

#endif
