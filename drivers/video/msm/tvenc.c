/*
 * Copyright (C) 2008 HTC Corporation.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/msm_mdp.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <mach/msm_fb.h>
#ifdef CONFIG_HTC_HEADSET_MGR
#include <mach/htc_headset_mgr.h>
#endif

#include "mdp_hw.h"
#include "tv.h"

char *__iomem tvenc_base;
static struct mdp_device *mdp_dev;
#define TV_OUT(reg, v)  writel(v, tvenc_base + MSM_##reg)
#define panel_to_tv(p) container_of((p), struct tvenc_info, fb_panel_data)

struct tvenc_info {
	uint32_t			base;
	struct mdp_info                 *mdp;
	struct clk			*tvenc_clk;
	struct clk			*tvdac_clk;
	struct msm_panel_data		fb_panel_data;
	struct platform_device          fb_pdev;
	struct msm_tvenc_platform_data	*pdata;
	uint32_t fb_start;
	struct msmfb_callback		frame_start_cb;
	wait_queue_head_t		vsync_waitq;
	int				got_vsync;

	int (*video_relay)(int on_off);
	atomic_t			ref_count;
	unsigned int			irq;
	struct delayed_work detect_delay_work;
};

int tvenc_set_mode(int mode)
{
	uint32_t reg = 0;
	int ret = 0;

	TV_OUT(TV_ENC_CTL, 0);

	switch (mode) {
	case NTSC_M:
	case NTSC_J:
		if (mode == NTSC_M) {
			/* Cr gain 11, Cb gain C6, y_gain 97 */
			TV_OUT(TV_GAIN, 0x0081B697);
		} else {
			/* Cr gain 11, Cb gain C6, y_gain 97 */
			TV_OUT(TV_GAIN, 0x008bc4a3);
			reg |= TVENC_CTL_NTSCJ_MODE;
		}
		TV_OUT(TV_CGMS, 0x0);
		/*  NTSC Timing */
		TV_OUT(TV_SYNC_1, 0x0020009e);
		TV_OUT(TV_SYNC_2, 0x011306B4);
		TV_OUT(TV_SYNC_3, 0x0006000C);
		TV_OUT(TV_SYNC_4, 0x0028020D);
		TV_OUT(TV_SYNC_5, 0x005E02FB);
		TV_OUT(TV_SYNC_6, 0x0006000C);
		TV_OUT(TV_SYNC_7, 0x00000012);
		TV_OUT(TV_BURST_V1, 0x0013020D);
		TV_OUT(TV_BURST_V2, 0x0014020C);
		TV_OUT(TV_BURST_V3, 0x0013020D);
		TV_OUT(TV_BURST_V4, 0x0014020C);
		TV_OUT(TV_BURST_H, 0x00AE00F2);
		TV_OUT(TV_SOL_REQ_ODD, 0x00280208);
		TV_OUT(TV_SOL_REQ_EVEN, 0x00290209);

		reg |= TVENC_CTL_TV_MODE_NTSC_M_PAL60 |
			TVENC_CTL_Y_FILTER_EN |
			TVENC_CTL_CR_FILTER_EN |
			TVENC_CTL_CB_FILTER_EN |
			TVENC_CTL_SINX_FILTER_EN;

		TV_OUT(TV_LEVEL, 0x00000000);   /* DC offset to 0. */
		TV_OUT(TV_OFFSET, 0x008080f0);

		break;

	case PAL_BDGHIN:
		/* Cr gain 11, Cb gain C6, y_gain 97 */
		TV_OUT(TV_GAIN, 0x0088c1a0);
		TV_OUT(TV_CGMS, 0x00012345);
		TV_OUT(TV_TEST_MUX, 0x0);
		/*  PAL Timing */
		TV_OUT(TV_SYNC_1, 0x00180097);
		TV_OUT(TV_SYNC_2, 0x011f06c0);
		TV_OUT(TV_SYNC_3, 0x0005000a);
		TV_OUT(TV_SYNC_4, 0x00320271);
		TV_OUT(TV_SYNC_5, 0x005602f9);
		TV_OUT(TV_SYNC_6, 0x0005000a);
		TV_OUT(TV_SYNC_7, 0x0000000f);
		TV_OUT(TV_BURST_V1, 0x0012026e);
		TV_OUT(TV_BURST_V2, 0x0011026d);
		TV_OUT(TV_BURST_V3, 0x00100270);
		TV_OUT(TV_BURST_V4, 0x0013026f);
		TV_OUT(TV_BURST_H, 0x00af00ea);
		TV_OUT(TV_SOL_REQ_ODD, 0x0030026e);
		TV_OUT(TV_SOL_REQ_EVEN, 0x0031026f);

		reg |= TVENC_CTL_TV_MODE_PAL_BDGHIN;
		reg |= TVENC_CTL_Y_FILTER_EN |
			TVENC_CTL_CR_FILTER_EN |
			TVENC_CTL_CB_FILTER_EN | TVENC_CTL_SINX_FILTER_EN;

		TV_OUT(TV_LEVEL, 0x00000000);   /* DC offset to 0. */
		TV_OUT(TV_OFFSET, 0x008080f0);
		break;

	case PAL_M:
		/* Cr gain 11, Cb gain C6, y_gain 97 */
		TV_OUT(TV_GAIN, 0x0081b697);
		TV_OUT(TV_CGMS, 0x000af317);
		TV_OUT(TV_TEST_MUX, 0x000001c3);
		TV_OUT(TV_TEST_MODE, 0x00000002);
		/*  PAL Timing */
		TV_OUT(TV_SYNC_1, 0x0020009e);
		TV_OUT(TV_SYNC_2, 0x011306b4);
		TV_OUT(TV_SYNC_3, 0x0006000c);
		TV_OUT(TV_SYNC_4, 0x0028020D);
		TV_OUT(TV_SYNC_5, 0x005e02fb);
		TV_OUT(TV_SYNC_6, 0x0006000c);
		TV_OUT(TV_SYNC_7, 0x00000012);
		TV_OUT(TV_BURST_V1, 0x0012020b);
		TV_OUT(TV_BURST_V2, 0x0016020c);
		TV_OUT(TV_BURST_V3, 0x00150209);
		TV_OUT(TV_BURST_V4, 0x0013020c);
		TV_OUT(TV_BURST_H, 0x00bf010b);
		TV_OUT(TV_SOL_REQ_ODD, 0x00280208);
		TV_OUT(TV_SOL_REQ_EVEN, 0x00290209);

		reg |= TVENC_CTL_TV_MODE_PAL_M;
		reg |= TVENC_CTL_Y_FILTER_EN |
			TVENC_CTL_CR_FILTER_EN |
			TVENC_CTL_CB_FILTER_EN | TVENC_CTL_SINX_FILTER_EN;

		TV_OUT(TV_LEVEL, 0x00000000);   /* DC offset to 0. */
		TV_OUT(TV_OFFSET, 0x008080f0);
		break;

	case PAL_N:
		/* Cr gain 11, Cb gain C6, y_gain 97 */
		TV_OUT(TV_GAIN, 0x0081b697);
		TV_OUT(TV_CGMS, 0x000af317);
		TV_OUT(TV_TEST_MUX, 0x000001c3);
		TV_OUT(TV_TEST_MODE, 0x00000002);
		/*  PAL Timing */
		TV_OUT(TV_SYNC_1, 0x00180097);
		TV_OUT(TV_SYNC_2, 0x12006c0);
		TV_OUT(TV_SYNC_3, 0x0005000a);
		TV_OUT(TV_SYNC_4, 0x00320271);
		TV_OUT(TV_SYNC_5, 0x005602f9);
		TV_OUT(TV_SYNC_6, 0x0005000a);
		TV_OUT(TV_SYNC_7, 0x0000000f);
		TV_OUT(TV_BURST_V1, 0x0012026e);
		TV_OUT(TV_BURST_V2, 0x0011026d);
		TV_OUT(TV_BURST_V3, 0x00100270);
		TV_OUT(TV_BURST_V4, 0x0013026f);
		TV_OUT(TV_BURST_H, 0x00af00fa);
		TV_OUT(TV_SOL_REQ_ODD, 0x0030026e);
		TV_OUT(TV_SOL_REQ_EVEN, 0x0031026f);

		reg |= TVENC_CTL_TV_MODE_PAL_N;
		reg |= TVENC_CTL_Y_FILTER_EN |
			TVENC_CTL_CR_FILTER_EN |
			TVENC_CTL_CB_FILTER_EN | TVENC_CTL_SINX_FILTER_EN;

		TV_OUT(TV_LEVEL, 0x00000000);   /* DC offset to 0. */
		TV_OUT(TV_OFFSET, 0x008080f0);
		break;

	default:
		return -ENODEV;
	}

#ifdef CONFIG_MSM_MDP31
	TV_OUT(TV_DAC_INTF, 0x29);
#endif
	TV_OUT(TV_ENC_CTL, reg);
	reg |= TVENC_CTL_ENC_EN;
	TV_OUT(TV_ENC_CTL, reg);

	return ret;
}

int tvenc_off(struct msm_panel_data *panel)
{
	struct tvenc_info *tvenc = panel_to_tv(panel);
	pr_info("%s\n", __func__);

	if (atomic_read(&tvenc->ref_count) == 0)
		return 0;

	if (atomic_dec_return(&tvenc->ref_count) == 0) {
		pr_info("%s, do turn_off\n", __func__);
		TV_OUT(TV_ENC_CTL, 0);
		clk_disable(tvenc->tvenc_clk);
		clk_disable(tvenc->tvdac_clk);
	}

	return 0;
}

int tvenc_on(struct msm_panel_data *panel)
{
	struct tvenc_info *tvenc = panel_to_tv(panel);
	pr_info("%s\n", __func__);

	if (atomic_inc_return(&tvenc->ref_count) == 1) {
		clk_enable(tvenc->tvenc_clk);
		clk_enable(tvenc->tvdac_clk);
	}

	return 0;
}

int tvenc_suspend(struct msm_panel_data *panel)
{
	int ret = -1;
	struct tvenc_info *tvenc = panel_to_tv(panel);

	pr_info("%s\n", __func__);
	ret = tvenc_off(panel);
	disable_irq(tvenc->irq);

	/* Shutdown amplifier anyway to save power consumption,
	 * or the amplifier will raise the current up to 8mA.
	 */
	return ret;
}

int tvenc_resume(struct msm_panel_data *panel)
{
	int ret = -1;
	struct tvenc_info *tvenc = panel_to_tv(panel);

	pr_info("%s\n", __func__);
	ret = tvenc_on(panel);
	enable_irq(tvenc->irq);
	return ret;
}

static void tvenc_wait_vsync(struct msm_panel_data *panel)
{
	struct tvenc_info *tvenc = panel_to_tv(panel);
	int ret;

	ret = wait_event_timeout(tvenc->vsync_waitq, tvenc->got_vsync, HZ / 2);
	if (ret == 0)
		pr_err("%s: timeout waiting for VSYNC\n", __func__);
	tvenc->got_vsync = 0;
}

static void tvenc_request_vsync(struct msm_panel_data *fb_panel,
		struct msmfb_callback *vsync_cb)
{
	struct tvenc_info *tvenc = panel_to_tv(fb_panel);

	/* the vsync callback will start the dma */
	vsync_cb->func(vsync_cb);
	tvenc->got_vsync = 0;
	mdp_out_if_req_irq(mdp_dev, MSM_TV_INTERFACE, TV_OUT_FRAME_START,
			&tvenc->frame_start_cb);
	tvenc_wait_vsync(fb_panel);
}

static void tvenc_clear_vsync(struct msm_panel_data *fb_panel)
{
	struct tvenc_info *tvenc = panel_to_tv(fb_panel);
	tvenc->got_vsync = 0;
	mdp_out_if_req_irq(mdp_dev, MSM_TV_INTERFACE, 0, NULL);
}

static void tvenc_frame_start(struct msmfb_callback *cb)
{
	struct tvenc_info *tvenc;

	tvenc = container_of(cb, struct tvenc_info, frame_start_cb);

	tvenc->got_vsync = 1;
	wake_up(&tvenc->vsync_waitq);
}

static void tvenc_dma_start(void *priv, uint32_t addr, uint32_t stride,
		uint32_t width, uint32_t height, uint32_t x, uint32_t y)
{
	struct tvenc_info *tvenc = priv;
	mdp_writel(tvenc->mdp, addr >> 3, MDP_TV_OUT_BUF_ADDR);
}

static void tvenc_detect_work_func(struct work_struct *work)
{
	int value;
	struct tvenc_info *tvenc= container_of(work, struct tvenc_info,
			detect_delay_work);

	value = gpio_get_value(82);
	printk("%s, TV-detection GPIO=%d\n", __func__, value);
#ifdef CONFIG_HTC_HEADSET_MGR
	switch_send_event(BIT_TV_OUT | BIT_TV_OUT_AUDIO, !!value);
#endif
}

irqreturn_t tvenc_detect_isr(int irq, void *data)
{
	int v1, v2;
	int retry = 5;
	struct tvenc_info *tvenc = panel_to_tv(data);

	do {
		v1 = gpio_get_value(82);
		set_irq_type(tvenc->irq, v1 ?
				IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
		printk("%s: set GPIO of TV-detection with TRIGGER-%s\n",
			__func__, v1 ? "LOW": "HIGH");
		v2 = gpio_get_value(82);
	} while (v1 != v2 && retry-- > 0);
	schedule_delayed_work(&tvenc->detect_delay_work, msecs_to_jiffies(100));

        return IRQ_HANDLED;
}

static int tvenc_setup_detection(struct msm_panel_data *panel, int init)
{
        int ret;
	/* FIXME: move platform-dependent info to proper place */
        int gpio = 82;		/* GPIO: INCREDIBLEC_TV_LOAD_DET */
	int value;
        unsigned int irq;
	struct tvenc_info *tvenc = panel_to_tv(panel);

        if (!init) {
                ret = 0;
                goto uninit;
        }

        ret = gpio_request(gpio, "tv-detect");
        if (ret)
                goto err_request_gpio_failed;

        ret = gpio_direction_input(gpio);
        if (ret)
                goto err_gpio_direction_input_failed;

        tvenc->irq = ret = irq = gpio_to_irq(gpio);
        if (ret < 0)
                goto err_get_irq_num_failed;

	value = gpio_get_value(gpio);
        printk(KERN_INFO "tv detect on gpio %d now %d\n", gpio, value);
	ret = request_irq(irq, tvenc_detect_isr,
			value ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH,
			"tv-detect", panel);
	if (ret)
                goto err_request_irq_failed;

	INIT_DELAYED_WORK(&tvenc->detect_delay_work, tvenc_detect_work_func);
        return 0;

uninit:
        free_irq(gpio_to_irq(gpio), panel);
err_request_irq_failed:
err_get_irq_num_failed:
err_gpio_direction_input_failed:
        gpio_free(gpio);
err_request_gpio_failed:
        return ret;
}

void tvenc_enable_amplifier(int on_off)
{
	pr_info("%s(%d)\n", __func__, on_off);
	/* FIXME: change the platform-dependant data to proper place */
	gpio_set_value(109, !!on_off);
}

static int tvenc_probe(struct platform_device *pdev)
{
	int ret;
	struct mdp_info *mdp = container_of(mdp_dev, struct mdp_info, mdp_dev);
	struct tvenc_info *tvenc;
	struct msm_tvenc_platform_data *pdata = pdev->dev.platform_data;

	printk(KERN_INFO "%s()\n", __func__);
	tvenc = kzalloc(sizeof(struct tvenc_info), GFP_KERNEL);
	if (!tvenc)
		return -ENOMEM;

	tvenc->tvenc_clk = clk_get(NULL, "tv_enc_clk");
	if (IS_ERR(tvenc->tvenc_clk)) {
		pr_err("error: can't get tvenc_clk!\n");
		ret = PTR_ERR(tvenc->tvenc_clk);
		goto err_get_tvenc_clk;
	}

	tvenc->tvdac_clk = clk_get(NULL, "tv_dac_clk");
	if (IS_ERR(tvenc->tvdac_clk)) {
		pr_err("error: can't get tvdac_clk!\n");
		ret = PTR_ERR(tvenc->tvdac_clk);
		goto err_get_tvdac_clk;
	}

	init_waitqueue_head(&tvenc->vsync_waitq);
	tvenc->pdata = pdata;
	tvenc->video_relay = pdata->video_relay;
	tvenc->frame_start_cb.func = tvenc_frame_start;

	platform_set_drvdata(pdev, tvenc);
	mdp_out_if_register(mdp_dev, MSM_TV_INTERFACE, tvenc, TV_OUT_DMA3_DONE,
			tvenc_dma_start);

	tvenc->fb_start = pdata->fb_resource->start;
	tvenc->mdp = container_of(mdp_dev, struct mdp_info, mdp_dev);

	tvenc->fb_panel_data.suspend = tvenc_suspend;
	tvenc->fb_panel_data.resume = tvenc_resume;
	tvenc->fb_panel_data.wait_vsync = tvenc_wait_vsync;
	tvenc->fb_panel_data.request_vsync = tvenc_request_vsync;
	tvenc->fb_panel_data.clear_vsync = tvenc_clear_vsync;
	tvenc->fb_panel_data.blank = NULL;
	tvenc->fb_panel_data.unblank = NULL;
	tvenc->fb_panel_data.fb_data = pdata->fb_data;
	tvenc->fb_panel_data.interface_type = MSM_TV_INTERFACE;

	tvenc->fb_pdev.name = "tvfb";
	tvenc->fb_pdev.id = pdata->fb_id;
	tvenc->fb_pdev.resource = pdata->fb_resource;
	tvenc->fb_pdev.num_resources = 1;
	tvenc->fb_pdev.dev.platform_data = &tvenc->fb_panel_data;

	ret = platform_device_register(&tvenc->fb_pdev);
	if (ret) {
		pr_err("%s: Cannot register msm_panel pdev\n", __func__);
		goto err_plat_dev_reg;
	}

	tvenc_base = ioremap(pdev->resource[0].start,
			pdev->resource[0].end - pdev->resource[0].start + 1);
	if (!tvenc_base) {
		pr_err("tvenc_base ioremap failed!\n");
		ret = -ENOMEM;
		goto reg_ioremap_err;
	}
	tvenc_setup_detection(&tvenc->fb_panel_data, 1);

	/* starting address[31..8] of Video frame buffer is CS0 */
	mdp_writel(mdp, (pdata->fb_resource->start) >> 3, MDP_TV_OUT_BUF_ADDR);
	mdp_writel(mdp, 0x4c60674, 0xC0004);	/* flicker filter enabled */
	mdp_writel(mdp, 0x20, 0xC0010);		/* sobel treshold */
	mdp_writel(mdp, 0xeb0010, 0xC0018);	/* Y  Max, Y  min */
	mdp_writel(mdp, 0xf00010, 0xC001C);	/* Cb Max, Cb min */
	mdp_writel(mdp, 0xf00010, 0xC0020);	/* Cb Max, Cb min */
	mdp_writel(mdp, 0x67686970, 0xC000C);	/* add a few chars for CC */
	mdp_writel(mdp, 1, 0xC0000);		/* MDP tv out enable */

	return 0;

reg_ioremap_err:

err_plat_dev_reg:
	clk_put(tvenc->tvenc_clk);
err_get_tvenc_clk:
	clk_put(tvenc->tvdac_clk);
err_get_tvdac_clk:
	kfree(tvenc);

	return ret;
}

static int tvenc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver tvenc_driver = {
	.probe = tvenc_probe,
	.remove = tvenc_remove,
	.driver = {
		.name	= "msm_tv",
		.owner	= THIS_MODULE,
	},
};

static int tvenc_add_mdp_device(struct device *dev,
		struct class_interface *class_intf)
{
	/* might need locking if mulitple mdp devices */
	if (mdp_dev)
		return 0;
	mdp_dev = container_of(dev, struct mdp_device, dev);
	return platform_driver_register(&tvenc_driver);
}

static void tvenc_remove_mdp_device(struct device *dev,
		struct class_interface *class_intf)
{
	/* might need locking if mulitple mdp devices */
	if (dev != &mdp_dev->dev)
		return;
	platform_driver_unregister(&tvenc_driver);
	mdp_dev = NULL;
}

static struct class_interface tvenc_interface = {
	.add_dev = &tvenc_add_mdp_device,
	.remove_dev = &tvenc_remove_mdp_device,
};

static int __init tvenc_init(void)
{
	return register_mdp_client(&tvenc_interface);
}

module_init(tvenc_init);
