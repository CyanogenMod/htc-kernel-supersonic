/* drivers/video/msm/tvfb.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/msm_mdp.h>
#include <linux/platform_device.h>

#include <asm/system.h>
#include <asm/mach-types.h>

#include "mdp_hw.h"
#include "tv.h"

#define BITS_PER_PIXEL		16
#define BYTES_PER_PIXEL		(BITS_PER_PIXEL >> 3)
#define TV_BGCOLOR		0x1080

#define MDP_IMG(w, h, f, o, m)	(struct mdp_img[]){{w, h, f, o, m}}
#define MDP_RECT(x, y, w, h)	(struct mdp_rect[]){{x, y, w, h}}

struct tvfb_info {
	struct fb_info *fb, *src_fb;
	unsigned char *fbram;
	struct msm_panel_data *fb_panel_data;
	uint32_t enabled, mode, orientation;
	struct margin *aa_margin;
	struct clk *tvenc_clk, *tvdac_clk;
	uint8_t *black;
	struct early_suspend early_suspend;
	int src_memory_id, dst_memory_id;
	int page_index;
	struct notifier_block notifier;
	spinlock_t update_lock;
	struct msmfb_callback dma_callback;
	struct msmfb_callback vsync_callback;
	struct mutex access_lock;
	struct mdp_blit_req mirror_req;
	enum {
		SLEEPING,
		AWAKE,
	} sleeping;
};

struct margin{
	uint32_t top, buttom, left, right;
};

/* The margin table of active-area for corresponding mode */
struct margin tv_aa_margin[2][NUM_OF_TV_MODE] = {
	{
		{0, 0, 30, 28},
		{0, 0, 30, 28},
		{0, 0, 24, 34},
		{0, 0, 30, 28},
		{0, 0, 24, 34},
	},
	{
		{8, 10, 0, 0},
		{8, 10, 0, 0},
		{20, 20, 0, 0},
		{8, 10, 0, 0},
		{20, 20, 0, 0},
	}
};
static struct mdp_device *mdp_dev;

inline void tvfb_set_mdp_req(struct mdp_blit_req *r,
		struct mdp_img *src, struct mdp_img *dst,
		struct mdp_rect *src_rect, struct mdp_rect *dst_rect,
		unsigned flag, unsigned alpha)
{
	r->src = *src;
	r->dst = *dst;
	r->src_rect = *src_rect;
	r->dst_rect = *dst_rect;
	r->transp_mask = MDP_TRANSP_NOP;
	r->flags = flag ;
	r->alpha = alpha;
}

int mdp_fb_mirror(struct mdp_device *mdp_dev,
		struct fb_info *src_fb, struct fb_info *dst_fb,
		struct mdp_blit_req *req);

int tv_screen_height(int mode)
{
	switch (mode) {
	case NTSC_M:
	case NTSC_J:
	case PAL_M:
		return TV_SCREEN_HEIGHT_NTSC;

	case PAL_BDGHIN:
	case PAL_N:
		return TV_SCREEN_HEIGHT_PAL;

	default:
		return 0;
	}
}

static void tvfb_handle_dma(struct msmfb_callback *callback)
{
	return ;
}

static void tvfb_handle_vsync(struct msmfb_callback *callback)
{
	unsigned h;
	unsigned addr;
	unsigned long irq_flags;
	struct tvfb_info *tvfb = container_of(callback, struct tvfb_info,
			vsync_callback);

	spin_lock_irqsave(&tvfb->update_lock, irq_flags);
	h = tv_screen_height(tvfb->mode);
	addr = tvfb->page_index ?
		h * TV_SCREEN_WIDTH * BYTES_PER_PIXEL : 0;
	mdp_dev->dma(mdp_dev, addr + tvfb->fb->fix.smem_start,
			TV_SCREEN_WIDTH * BYTES_PER_PIXEL, TV_SCREEN_WIDTH, h,
			0, 0, &tvfb->dma_callback,
			tvfb->fb_panel_data->interface_type);
	spin_unlock_irqrestore(&tvfb->update_lock, irq_flags);
}

static int tvfb_black(struct tvfb_info *tvfb)
{
	/* TODO: change to DMA based filling with single-stride */
	int i;
	uint16_t *fb_ptr = (uint16_t *)tvfb->fbram;
	unsigned height = tv_screen_height(tvfb->mode);

	for (i = 0; i < TV_SCREEN_WIDTH * height * BYTES_PER_PIXEL; i++)
		fb_ptr[i] = TV_BGCOLOR;

	return 0;
}

static int setup_tvfb_mem(struct tvfb_info *tvfb,
		struct platform_device *pdev)
{
	struct resource *resource;
	struct fb_info *fb = tvfb->fb;
	unsigned long size =
		fb->var.xres * fb->var.yres * BYTES_PER_PIXEL * 2;
	unsigned char *fbram;

	/* board file might have attached a resource describing an fb */
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource)
		return -EINVAL;

	/* check the resource is large enough to fit the fb */
	if (resource->end - resource->start < size) {
		printk(KERN_ERR "allocated resource is too small for "
				"TV framebuffer\n");
		return -ENOMEM;
	}

	fb->fix.smem_start = resource->start;
	fb->fix.smem_len = resource->end - resource->start;
	fbram = ioremap(resource->start,
			resource->end - resource->start);
	if (fbram == 0) {
		printk(KERN_ERR "tvfb: cannot allocate fbram!\n");
		return -ENOMEM;
	}
	fb->screen_base = fbram;

	return 0;
}

static void tvfb_refresh_req(struct tvfb_info *tvfb)
{
	unsigned orient;
	unsigned x, y, w, h;
	unsigned tv_yoffset, tv_height, src_yoffset;
	unsigned src_format;
	struct fb_info *src_fb = registered_fb[0];

	if (src_fb->var.bits_per_pixel == 32 || src_fb->var.bits_per_pixel == 24) {
		src_format = MDP_RGBX_8888;
	} else
		src_format = MDP_RGB_565;

	tv_height = tv_screen_height(tvfb->mode);
	tv_yoffset = tvfb->page_index * TV_SCREEN_WIDTH * tv_height *
		BYTES_PER_PIXEL;

	if (tvfb->orientation == TV_LANDSCAPE) {
		w = TV_SCREEN_WIDTH -
			tvfb->aa_margin->left - tvfb->aa_margin->right;
		h = w * src_fb->var.xres / src_fb->var.yres;
		x = tvfb->aa_margin->left;
		y = (tv_height - h) / 2 + tvfb->aa_margin->top;
		orient = MDP_ROT_270;
	} else {
		h = tv_height -
			tvfb->aa_margin->top - tvfb->aa_margin->buttom;
		w = h * src_fb->var.xres / src_fb->var.yres;
		x = (TV_SCREEN_WIDTH - w) / 2 + tvfb->aa_margin->left;
		/* 1. The x must be even number in MDP_YCRYCB_H2V1 color space.
		 *    Otherwise the red and blue component will be swapped.
		 * 2. The width of projected area also should be even number,
		 *    otherwise the there will be an area of narrow strip
		 *    appears in the right side of TV screen.
		 */
		x &= ~1 ;
		w &= ~1 ;

		y = tvfb->aa_margin->top;
		orient = MDP_ROT_NOP;
	}
	pr_info("%s: x=%d, y=%d, w=%d, h=%d\n", __func__, x, y, w, h);

	tvfb_set_mdp_req(&tvfb->mirror_req,
			MDP_IMG(src_fb->var.xres, src_fb->var.yres,
				src_format, src_yoffset, 0),
			MDP_IMG(TV_SCREEN_WIDTH, tv_height, MDP_YCRYCB_H2V1,
				tv_yoffset, 0),
			MDP_RECT(0, 0, src_fb->var.xres, src_fb->var.yres),
			MDP_RECT(x, y, w, h),
			orient,
			MDP_ALPHA_NOP
		   );
}

static int tvfb_do_mirror(struct tvfb_info *tvfb)
{
	int ret;
	unsigned src_yoffset = 0;
	unsigned tv_height = tv_screen_height(tvfb->mode);
	struct msm_panel_data *panel = tvfb->fb_panel_data;
	struct fb_info *src_fb = registered_fb[0];

	tvfb->page_index ^= 1;
	src_yoffset = src_fb->var.yoffset * src_fb->var.xres *
		(src_fb->var.bits_per_pixel >> 3);
	tvfb->mirror_req.src.offset = src_yoffset;
	tvfb->mirror_req.dst.offset = tvfb->page_index ?
		TV_SCREEN_WIDTH * tv_height * BYTES_PER_PIXEL : 0;
	ret = mdp_fb_mirror(mdp_dev, registered_fb[0], registered_fb[1],
		&tvfb->mirror_req);
	panel->request_vsync(panel, &tvfb->vsync_callback);

	return ret;
}

static int tv_notifier_cb(struct notifier_block *nfb, unsigned long action,
		void *ignored)
{
	struct tvfb_info *tvfb =
		container_of(nfb, struct tvfb_info, notifier);

	tvfb_do_mirror(tvfb);
	return NOTIFY_OK;
}

int tvfb_enable(struct tvfb_info *tvfb, int on_off)
{
	mutex_lock(&tvfb->access_lock);
	if (!!on_off) {
		tvenc_on(tvfb->fb_panel_data);
		tvenc_set_mode(tvfb->mode);
		tvfb_refresh_req(tvfb);
		fb_register_client(&tvfb->notifier);
		tvfb->enabled = 1;
	} else {
		tvfb->enabled = 0;
		fb_unregister_client(&tvfb->notifier);
		tvenc_off(tvfb->fb_panel_data);
	}
	mutex_unlock(&tvfb->access_lock);

	return 0;
}

int tvfb_set_orientation(struct tvfb_info *tvfb, int orientation)
{
	int enabled = tvfb->enabled;

	mutex_lock(&tvfb->access_lock);
	tvfb->enabled = 0;
	tvfb->orientation = orientation;
	tvfb->aa_margin = &tv_aa_margin[tvfb->orientation][tvfb->mode];
	tvfb_refresh_req(tvfb);

	tvfb_black(tvfb);
	tvfb->enabled = enabled;
	mutex_unlock(&tvfb->access_lock);
	tvfb_do_mirror(tvfb);
	return 0;
}

static int tvfb_open(struct fb_info *info, int user)
{
	return 0;
}

static int tvfb_release(struct fb_info *info, int user)
{
	return 0;
}

#define TV_FB_IOCTL_MAGIC	't'
#define TV_FB_ENABLE		_IOW(TV_FB_IOCTL_MAGIC, 1, unsigned int)
#define TV_FB_CHANGE_MODE	_IOW(TV_FB_IOCTL_MAGIC, 2, unsigned int)
#define TV_FB_SET_ORIENTATION	_IOW(TV_FB_IOCTL_MAGIC, 3, unsigned int)
#define TV_FB_GET_INFO		_IOW(TV_FB_IOCTL_MAGIC, 4, unsigned int)

static int tvfb_ioctl(struct fb_info *p, unsigned int cmd, unsigned long arg)
{
	int ret;
	int enabled;
	uint32_t info;
	void __user *argp = (void __user *)arg;
	struct tvfb_info *tvfb = p->par;

	switch (cmd) {
	case TV_FB_ENABLE:
		pr_info("[tvfb] ioctl(TV_FB_ENABLE, %d)\n", (uint32_t)argp);
		tvfb_black(tvfb);
		ret = tvfb_enable(tvfb, (uint32_t)argp);
		break;

	case TV_FB_CHANGE_MODE:
		pr_info("[tvfb] ioctl(TV_FB_CHANGE_MODE, %d)\n", (unsigned)argp);
		if ((unsigned)argp >= NUM_OF_TV_MODE)
			return -EINVAL;
		enabled = tvfb->enabled;
		mutex_lock(&tvfb->access_lock);
		tvfb->enabled = 0;
		tvfb->mode = (unsigned)argp;
		tvfb->aa_margin = &tv_aa_margin[tvfb->orientation][tvfb->mode];
		tvfb_refresh_req(tvfb);
		tvfb->enabled = enabled;
		tvfb_black(tvfb);
		ret = tvenc_set_mode((unsigned)argp);
		mutex_unlock(&tvfb->access_lock);
		break;

	case TV_FB_SET_ORIENTATION:
		pr_info("[tvfb] ioctl(TV_FB_SET_ORIENTATION)\n");
		ret = tvfb_set_orientation(tvfb, (unsigned)argp);
		break;

	case TV_FB_GET_INFO:
		mutex_lock(&tvfb->access_lock);
		info = tvfb->enabled ? 1 : 0;
		info |= (tvfb->orientation? 1 : 0) << 1;
		info |= tvfb->mode << 2;
		mutex_unlock(&tvfb->access_lock);
		pr_info("[tvfb] ioctl(TV_FB_GET_INFO), info = 0x%08x\n", info);
		ret = copy_to_user(argp, &info, sizeof(info)) ? -EFAULT : 0;
		break;

	default:
		ret = -EINVAL;
		pr_info("tvfb: unknown ioctl: %d\n", cmd);
		return -EINVAL;
}
	return ret;
}

int tvfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	return 0;
}

static struct fb_ops tv_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = tvfb_open,
	.fb_release = tvfb_release,
	.fb_pan_display = tvfb_pan_display,
	.fb_ioctl = tvfb_ioctl,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tvfb_suspend(struct early_suspend *h)
{
	struct tvfb_info *tvfb = container_of(h, struct tvfb_info,
			early_suspend);
	struct msm_panel_data *panel = tvfb->fb_panel_data;

	mutex_lock(&tvfb->access_lock);
	if (panel->suspend && tvfb->enabled) {
		panel->suspend(panel);
	}
        /* Shutdown amplifier anyway to save power consumption,
         * otherwise the amplifier will raise the current up to 8mA.
         */
	tvenc_enable_amplifier(0);
	mutex_unlock(&tvfb->access_lock);
}

static void tvfb_resume(struct early_suspend *h)
{
	int ret;
	struct tvfb_info *tvfb = container_of(h, struct tvfb_info,
			early_suspend);
	struct msm_panel_data *panel = tvfb->fb_panel_data;

	if (panel->resume && tvfb->enabled) {
		mutex_lock(&tvfb->access_lock);
		ret = panel->resume(panel);
		if (ret)
			pr_err("msmfb: panel resume failed, not resuming fb\n");
		else
			tvenc_set_mode(tvfb->mode);
		mutex_unlock(&tvfb->access_lock);
	}
	tvenc_enable_amplifier(1);
}
#endif

static unsigned PP[16];
static void setup_tvfb_info(struct fb_info *fb_info)
{
	int r;

	/* finish setting up the fb_info struct */
	strncpy(fb_info->fix.id, "msmfb", 16);
	fb_info->fix.ypanstep = 1;

	fb_info->fbops = &tv_fb_ops;
	fb_info->flags = FBINFO_DEFAULT;

	fb_info->fix.type = FB_TYPE_PACKED_PIXELS;
	fb_info->fix.visual = FB_VISUAL_TRUECOLOR;
	fb_info->fix.line_length = TV_SCREEN_WIDTH * BYTES_PER_PIXEL;

	fb_info->var.xres = TV_SCREEN_WIDTH;
	fb_info->var.yres = TV_SCREEN_HEIGHT_PAL;
	fb_info->var.width = 0;
	fb_info->var.height = 0;

	fb_info->var.xres_virtual = TV_SCREEN_WIDTH;
	fb_info->var.yres_virtual = TV_SCREEN_HEIGHT_PAL * 2;
	fb_info->var.bits_per_pixel = BITS_PER_PIXEL;
	fb_info->var.accel_flags = 0;

	fb_info->var.yoffset = 0;
	r = fb_alloc_cmap(&fb_info->cmap, 16, 0);
	fb_info->pseudo_palette = PP;

	PP[0] = 0;
	for (r = 1; r < 16; r++)
		PP[r] = 0xffffffff;
}

static int tvfb_probe(struct platform_device *pdev)
{
	int ret;
	struct fb_info *fb;
	struct tvfb_info *tvfb;
	struct msm_panel_data *panel = pdev->dev.platform_data;

	fb = framebuffer_alloc(sizeof(struct fb_info), &pdev->dev);
	if (!fb)
		return -ENOMEM;
	tvfb = fb->par;
	tvfb->fb = fb;
	tvfb->mode = NTSC_M;
	tvfb->orientation = TV_LANDSCAPE;
	tvfb->aa_margin = &tv_aa_margin[tvfb->orientation][tvfb->mode];
	tvfb->fb_panel_data = panel;
	tvfb->page_index = 0;
	tvfb->notifier.notifier_call = tv_notifier_cb;
	tvfb->vsync_callback.func = tvfb_handle_vsync;
	tvfb->dma_callback.func = tvfb_handle_dma;
	spin_lock_init(&tvfb->update_lock);
	tvfb->mode = NTSC_M;
	tvfb->src_memory_id = -1;
	tvfb->dst_memory_id = -1;

	setup_tvfb_info(fb);
	ret = setup_tvfb_mem(tvfb, pdev);
	if (ret)
		goto error_setup_fbmem;

	tvfb->black = kmalloc(TV_SCREEN_WIDTH * BYTES_PER_PIXEL, GFP_KERNEL);
	if (tvfb->black == NULL) {
		pr_err("Unable to allocate memory for black.");
		ret = -ENOMEM;
		goto error_register_framebuffer;
	}
	tvfb->fbram = fb->screen_base;
	mutex_init(&tvfb->access_lock);

#ifdef CONFIG_HAS_EARLYSUSPEND
	tvfb->early_suspend.suspend = tvfb_suspend;
	tvfb->early_suspend.resume = tvfb_resume;
	tvfb->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
	register_early_suspend(&tvfb->early_suspend);
#endif
	ret = register_framebuffer(fb);
	if (ret)
		goto error_register_framebuffer;

	return 0;

error_register_framebuffer:
	iounmap(fb->screen_base);
error_setup_fbmem:
	framebuffer_release(fb);

	return ret;
}

static int tvfb_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver tvfb_driver = {
	.probe = tvfb_probe,
	.remove = tvfb_remove,
	.driver = {
		.name	= "tvfb",
		.owner	= THIS_MODULE,
	},
};

static int tvfb_add_mdp_device(struct device *dev,
		struct class_interface *class_intf)
{
	/* might need locking if mulitple mdp devices */
	if (mdp_dev)
		return 0;
	mdp_dev = container_of(dev, struct mdp_device, dev);
	return platform_driver_register(&tvfb_driver);
}

static void tvfb_remove_mdp_device(struct device *dev,
		struct class_interface *class_intf)
{
	/* might need locking if mulitple mdp devices */
	if (dev != &mdp_dev->dev)
		return;
	platform_driver_unregister(&tvfb_driver);
	mdp_dev = NULL;
}

static struct class_interface tvfb_interface = {
	.add_dev = &tvfb_add_mdp_device,
	.remove_dev = &tvfb_remove_mdp_device,
};

static int __init tvfb_init(void)
{
	return register_mdp_client(&tvfb_interface);
}

module_init(tvfb_init);
