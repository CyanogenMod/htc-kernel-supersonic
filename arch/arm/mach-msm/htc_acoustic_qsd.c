/* arch/arm/mach-msm/htc_acoustic_qsd.c
 *
 * Copyright (C) 2009 HTC Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/gpio.h>

#include <mach/msm_smd.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_iomap.h>
#include <mach/htc_acoustic_qsd.h>
#include <mach/msm_qdsp6_audio.h>
#include <mach/htc_headset_mgr.h>

#include "smd_private.h"

#define ACOUSTIC_IOCTL_MAGIC 'p'
#define ACOUSTIC_UPDATE_ADIE \
	_IOW(ACOUSTIC_IOCTL_MAGIC, 24, unsigned int)

#define HTCACOUSTICPROG 0x30100003
#define HTCACOUSTICVERS 0
#define ONCRPC_ALLOC_ACOUSTIC_MEM_PROC		(1)
#define ONCRPC_UPDATE_ADIE_PROC			(2)
#define ONCRPC_ENABLE_AUX_PGA_LOOPBACK_PROC	(3)
#define ONCRPC_FORCE_HEADSET_SPEAKER_PROC	(4)
#define ONCRPC_SET_AUX_PGA_GAIN_PROC		(5)

#define HTC_ACOUSTIC_TABLE_SIZE        (0x20000)

#define D(fmt, args...) printk(KERN_INFO "htc-acoustic: "fmt, ##args)
#define E(fmt, args...) printk(KERN_ERR "htc-acoustic: "fmt, ##args)

static uint32_t htc_acoustic_vir_addr;
static struct msm_rpc_endpoint *endpoint;
static struct mutex api_lock;
static struct mutex rpc_connect_lock;
static struct qsd_acoustic_ops *the_ops;
struct class *htc_class;
static int hac_enable_flag;
struct mutex acoustic_lock;

void acoustic_register_ops(struct qsd_acoustic_ops *ops)
{
	the_ops = ops;
}

static int is_rpc_connect(void)
{
	mutex_lock(&rpc_connect_lock);
	if (endpoint == NULL) {
		endpoint = msm_rpc_connect(HTCACOUSTICPROG,
				HTCACOUSTICVERS, 0);
		if (IS_ERR(endpoint)) {
			pr_err("%s: init rpc failed! rc = %ld\n",
				__func__, PTR_ERR(endpoint));
			mutex_unlock(&rpc_connect_lock);
			return -1;
		}
	}
	mutex_unlock(&rpc_connect_lock);
	return 0;
}

int enable_mic_bias(int on)
{
	D("%s called %d\n", __func__, on);
	if (the_ops->enable_mic_bias)
		the_ops->enable_mic_bias(on);

	return 0;
}

int enable_mos_test(int enable)
{
	static int mos_test_enable;
	int res = 0;
	if (enable != mos_test_enable) {
		D("%s called %d\n", __func__, enable);
		if (enable)
			res = q6audio_reinit_acdb("default_mos.acdb");
		else
			res = q6audio_reinit_acdb("default.acdb");
		mos_test_enable = enable;
	}
	return res;
}
EXPORT_SYMBOL(enable_mos_test);

int force_headset_speaker_on(int enable)
{
	struct speaker_headset_req {
		struct rpc_request_hdr hdr;
		uint32_t enable;
	} spkr_req;

	D("%s called %d\n", __func__, enable);

	if (is_rpc_connect() == -1)
		return -1;

	spkr_req.enable = cpu_to_be32(enable);
	return  msm_rpc_call(endpoint,
		ONCRPC_FORCE_HEADSET_SPEAKER_PROC,
		&spkr_req, sizeof(spkr_req), 5 * HZ);
}
EXPORT_SYMBOL(force_headset_speaker_on);

int enable_aux_loopback(uint32_t enable)
{
	struct aux_loopback_req {
		struct rpc_request_hdr hdr;
		uint32_t enable;
	} aux_req;

	D("%s called %d\n", __func__, enable);

	if (is_rpc_connect() == -1)
		return -1;

	aux_req.enable = cpu_to_be32(enable);
	return  msm_rpc_call(endpoint,
		ONCRPC_ENABLE_AUX_PGA_LOOPBACK_PROC,
		&aux_req, sizeof(aux_req), 5 * HZ);
}
EXPORT_SYMBOL(enable_aux_loopback);

int set_aux_gain(int level)
{
	struct aux_gain_req {
		struct rpc_request_hdr hdr;
		int level;
	} aux_req;

	D("%s called %d\n", __func__, level);

	if (is_rpc_connect() == -1)
		return -1;

	aux_req.level = cpu_to_be32(level);
	return  msm_rpc_call(endpoint,
		ONCRPC_SET_AUX_PGA_GAIN_PROC,
		&aux_req, sizeof(aux_req), 5 * HZ);
}
EXPORT_SYMBOL(set_aux_gain);

static int acoustic_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long pgoff;
	int rc = -EINVAL;
	size_t size;

	D("mmap\n");

	mutex_lock(&api_lock);

	size = vma->vm_end - vma->vm_start;

	if (vma->vm_pgoff != 0) {
		E("mmap failed: page offset %lx\n", vma->vm_pgoff);
		goto done;
	}

	if (!htc_acoustic_vir_addr) {
		E("mmap failed: smem region not allocated\n");
		rc = -EIO;
		goto done;
	}

	pgoff = MSM_SHARED_RAM_PHYS +
		(htc_acoustic_vir_addr - (uint32_t)MSM_SHARED_RAM_BASE);
	pgoff = ((pgoff + 4095) & ~4095);
	htc_acoustic_vir_addr = ((htc_acoustic_vir_addr + 4095) & ~4095);

	if (pgoff <= 0) {
		E("pgoff wrong. %ld\n", pgoff);
		goto done;
	}

	if (size <= HTC_ACOUSTIC_TABLE_SIZE) {
		pgoff = pgoff >> PAGE_SHIFT;
	} else {
		E("size > HTC_ACOUSTIC_TABLE_SIZE  %d\n", size);
		goto done;
	}

	vma->vm_flags |= VM_IO | VM_RESERVED;
	rc = io_remap_pfn_range(vma, vma->vm_start, pgoff,
				size, vma->vm_page_prot);

	if (rc < 0)
		E("mmap failed: remap error %d\n", rc);

done:	mutex_unlock(&api_lock);
	return rc;
}

static int acoustic_open(struct inode *inode, struct file *file)
{
	int reply_value;
	int rc = -EIO;
	struct set_smem_req {
		struct rpc_request_hdr hdr;
		uint32_t size;
	} req_smem;

	struct set_smem_rep {
		struct rpc_reply_hdr hdr;
		int n;
	} rep_smem;

	D("open\n");

	mutex_lock(&api_lock);

	if (!htc_acoustic_vir_addr) {
		if (is_rpc_connect() == -1)
			goto done;

		req_smem.size = cpu_to_be32(HTC_ACOUSTIC_TABLE_SIZE);
		rc = msm_rpc_call_reply(endpoint,
					ONCRPC_ALLOC_ACOUSTIC_MEM_PROC,
					&req_smem, sizeof(req_smem),
					&rep_smem, sizeof(rep_smem),
					5 * HZ);

		reply_value = be32_to_cpu(rep_smem.n);
		if (reply_value != 0 || rc < 0) {
			E("open failed: ALLOC_ACOUSTIC_MEM_PROC error %d.\n",
			rc);
			goto done;
		}
		htc_acoustic_vir_addr =
			(uint32_t)smem_alloc(SMEM_ID_VENDOR1,
					HTC_ACOUSTIC_TABLE_SIZE);
		if (!htc_acoustic_vir_addr) {
			E("open failed: smem_alloc error\n");
			goto done;
		}
	}

	rc = 0;
done:
	mutex_unlock(&api_lock);
	return rc;
}

static int acoustic_release(struct inode *inode, struct file *file)
{
	D("release\n");
	return 0;
}

static long acoustic_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	int rc, reply_value;

	D("ioctl\n");

	mutex_lock(&api_lock);

	switch (cmd) {
	case ACOUSTIC_UPDATE_ADIE: {
		struct update_adie_req {
			struct rpc_request_hdr hdr;
			int id;
		} adie_req;

		struct update_adie_rep {
			struct rpc_reply_hdr hdr;
			int ret;
		} adie_rep;

		D("ioctl: ACOUSTIC_UPDATE_ADIE called %d.\n", current->pid);

		adie_req.id = cpu_to_be32(-1); /* update all codecs */
		rc = msm_rpc_call_reply(endpoint,
					ONCRPC_UPDATE_ADIE_PROC, &adie_req,
					sizeof(adie_req), &adie_rep,
					sizeof(adie_rep), 5 * HZ);

		reply_value = be32_to_cpu(adie_rep.ret);
		if (reply_value != 0 || rc < 0) {
			E("ioctl failed: ONCRPC_UPDATE_ADIE_PROC "\
				"error %d.\n", rc);
			if (rc >= 0)
				rc = -EIO;
			break;
		}
		D("ioctl: ONCRPC_UPDATE_ADIE_PROC success.\n");
		break;
	}
	default:
		E("ioctl: invalid command\n");
		rc = -EINVAL;
	}

	mutex_unlock(&api_lock);
	return rc;
}

struct rpc_set_uplink_mute_args {
	int mute;
};

static struct file_operations acoustic_fops = {
	.owner = THIS_MODULE,
	.open = acoustic_open,
	.release = acoustic_release,
	.mmap = acoustic_mmap,
	.unlocked_ioctl = acoustic_ioctl,
};

static struct miscdevice acoustic_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "htc-acoustic",
	.fops = &acoustic_fops,
};

static ssize_t htc_show(struct device *dev,
				struct device_attribute *attr,  char *buf)
{

	char *s = buf;
	mutex_lock(&acoustic_lock);
	s += sprintf(s, "%d\n", hac_enable_flag);
	mutex_unlock(&acoustic_lock);
	return s - buf;

}
static ssize_t htc_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{

	if (count == (strlen("enable") + 1) &&
		strncmp(buf, "enable", strlen("enable")) == 0) {
		mutex_lock(&acoustic_lock);

		if (hac_enable_flag == 0)
			pr_info("Enable HAC\n");
		hac_enable_flag = 1;

		mutex_unlock(&acoustic_lock);
		return count;
	}
	if (count == (strlen("disable") + 1) &&
		strncmp(buf, "disable", strlen("disable")) == 0) {
		mutex_lock(&acoustic_lock);

		if (hac_enable_flag == 1)
			pr_info("Disable HAC\n");
		hac_enable_flag = 0;

		mutex_unlock(&acoustic_lock);
		return count;
	}
	pr_err("hac_flag_store: invalid argument\n");
	return -EINVAL;

}

static DEVICE_ATTR(flag, 0666, htc_show, htc_store);

static int __init acoustic_init(void)
{
	int ret = 0;
	mutex_init(&api_lock);
	mutex_init(&rpc_connect_lock);
	ret = misc_register(&acoustic_misc);
	if (ret < 0) {
		pr_err("failed to register misc device!\n");
		return ret;
	}

	htc_class = class_create(THIS_MODULE, "htc_acoustic");
	if (IS_ERR(htc_class)) {
		ret = PTR_ERR(htc_class);
		htc_class = NULL;
		goto err_create_class;
	}
	acoustic_misc.this_device =
			device_create(htc_class, NULL, 0 , NULL, "hac");
	if (IS_ERR(acoustic_misc.this_device)) {
		ret = PTR_ERR(acoustic_misc.this_device);
		acoustic_misc.this_device = NULL;
		goto err_create_class;
	}

	ret = device_create_file(acoustic_misc.this_device, &dev_attr_flag);
	if (ret < 0)
		goto err_create_class_device;

	mutex_init(&acoustic_lock);

#if defined(CONFIG_HTC_HEADSET_MGR)
	struct headset_notifier notifier;
	notifier.id = HEADSET_REG_MIC_BIAS;
	notifier.func = enable_mic_bias;
	headset_notifier_register(&notifier);
#endif

	return 0;

err_create_class_device:
	device_destroy(htc_class, 0);
err_create_class:
	return ret;

}

static void __exit acoustic_exit(void)
{
	device_remove_file(acoustic_misc.this_device, &dev_attr_flag);
	device_destroy(htc_class, 0);
	class_destroy(htc_class);

	misc_deregister(&acoustic_misc);
}

module_init(acoustic_init);
module_exit(acoustic_exit);

