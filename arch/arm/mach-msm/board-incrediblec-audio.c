/* arch/arm/mach-msm/board-incrediblec-audio.c
 *
 * Copyright (C) 2009 HTC Corporation
 * Copyright (C) 2009 Google Inc.
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

#include <linux/gpio.h>
#include <linux/delay.h>
#include <mach/msm_qdsp6_audio.h>
#include <mach/htc_acoustic_qsd.h>
#include <mach/tpa6130.h>

#include "board-incrediblec.h"
#include "proc_comm.h"
#include "pmic.h"

#if 1
#define D(fmt, args...) printk(KERN_INFO "Audio: "fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static struct mutex mic_lock;
static struct mutex bt_sco_lock;
static int headset_status = 0;

struct q6_gain_info {
        int max_step;
        int gain[10];
};

static struct q6_gain_info q6_audio_hw[Q6_HW_COUNT] = {
	[Q6_HW_HANDSET] = {
		.max_step = 6,
		.gain = {-1600, -1300, -1000, -600, -300, 0, 0, 0, 0, 0},
	},
	[Q6_HW_HEADSET] = {
		.max_step = 6,
		.gain = {-2000, -1600, -1200, -800, -400, 0, 0, 0, 0, 0},
	},
	[Q6_HW_SPEAKER] = {
		.max_step = 6,
		.gain = {-1500, -1200, -900, -600, -300, 0, 0, 0, 0, 0},
	},
	[Q6_HW_TTY] = {
		.max_step = 6,
		.gain = {-2000, -1600, -1200, -800, -400, 0, 0, 0, 0, 0},
	},
	[Q6_HW_BT_SCO] = {
		.max_step = 6,
		.gain = {-2000, -1600, -1200, -800, -400, 0, 0, 0, 0, 0},
	},
	[Q6_HW_BT_A2DP] = {
		.max_step = 6,
		.gain = {-2000, -1600, -1200, -800, -400, 0, 0, 0, 0, 0},
	},
};

void incrediblec_headset_enable(int en)
{
	D("%s %d\n", __func__, en);
        /* enable audio amp */
	if (en != headset_status) {
		headset_status = en;
		if(en) {
			gpio_set_value(INCREDIBLEC_AUD_JACKHP_EN, 1);
			mdelay(10);
			set_headset_amp(1);
		} else {
			set_headset_amp(0);
			gpio_set_value(INCREDIBLEC_AUD_JACKHP_EN, 0);
		}
	}
}

void incrediblec_speaker_enable(int en)
{
	struct spkr_config_mode scm;
	memset(&scm, 0, sizeof(scm));

	D("%s %d\n", __func__, en);
	if (en) {
		mdelay(30);
		scm.is_right_chan_en = 0;
		scm.is_left_chan_en = 1;
		scm.is_stereo_en = 0;
		scm.is_hpf_en = 1;
		pmic_spkr_en_mute(LEFT_SPKR, 0);
		pmic_spkr_en_mute(RIGHT_SPKR, 0);
		pmic_set_spkr_configuration(&scm);
		pmic_spkr_en(LEFT_SPKR, 1);
		pmic_spkr_en(RIGHT_SPKR, 0);

		/* unmute */
		pmic_spkr_en_mute(LEFT_SPKR, 1);
	} else {
		pmic_spkr_en_mute(LEFT_SPKR, 0);

		pmic_spkr_en(LEFT_SPKR, 0);
		pmic_spkr_en(RIGHT_SPKR, 0);

		pmic_set_spkr_configuration(&scm);
	}
	mdelay(10);
}

void incrediblec_receiver_enable(int en)
{
	/* After XC */
	if (system_rev >= 2) {
		struct spkr_config_mode scm;
		memset(&scm, 0, sizeof(scm));

		D("%s %d\n", __func__, en);
		if (en) {
			mdelay(30);
			scm.is_right_chan_en = 1;
			scm.is_left_chan_en = 0;
			scm.is_stereo_en = 0;
			scm.is_hpf_en = 1;
			pmic_spkr_en_mute(LEFT_SPKR, 0);
			pmic_spkr_en_mute(RIGHT_SPKR, 0);
			pmic_set_spkr_configuration(&scm);
			pmic_spkr_en(LEFT_SPKR, 0);
			pmic_spkr_en(RIGHT_SPKR, 1);

			/* unmute */
			pmic_spkr_en_mute(RIGHT_SPKR, 1);
		} else {
			pmic_spkr_en_mute(RIGHT_SPKR, 0);

			pmic_spkr_en(LEFT_SPKR, 0);
			pmic_spkr_en(RIGHT_SPKR, 0);

			pmic_set_spkr_configuration(&scm);
		}
		mdelay(10);
	}
}

static uint32_t bt_sco_enable[] = {
	PCOM_GPIO_CFG(INCREDIBLEC_BT_PCM_OUT, 1, GPIO_OUTPUT,
			GPIO_NO_PULL, GPIO_2MA),
	PCOM_GPIO_CFG(INCREDIBLEC_BT_PCM_IN, 1, GPIO_INPUT,
			GPIO_NO_PULL, GPIO_2MA),
	PCOM_GPIO_CFG(INCREDIBLEC_BT_PCM_SYNC, 2, GPIO_INPUT,
			GPIO_NO_PULL, GPIO_2MA),
	PCOM_GPIO_CFG(INCREDIBLEC_BT_PCM_CLK, 2, GPIO_INPUT,
			GPIO_NO_PULL, GPIO_2MA),
};

static uint32_t bt_sco_disable[] = {
	PCOM_GPIO_CFG(INCREDIBLEC_BT_PCM_OUT, 0, GPIO_OUTPUT,
			GPIO_NO_PULL, GPIO_2MA),
	PCOM_GPIO_CFG(INCREDIBLEC_BT_PCM_IN, 0, GPIO_INPUT,
			GPIO_PULL_UP, GPIO_2MA),
	PCOM_GPIO_CFG(INCREDIBLEC_BT_PCM_SYNC, 0, GPIO_OUTPUT,
			GPIO_NO_PULL, GPIO_2MA),
	PCOM_GPIO_CFG(INCREDIBLEC_BT_PCM_CLK, 0, GPIO_OUTPUT,
			GPIO_NO_PULL, GPIO_2MA),
};

void incrediblec_bt_sco_enable(int en)
{
	static int bt_sco_refcount;
	D("%s %d\n", __func__, en);

	mutex_lock(&bt_sco_lock);
	if (en) {
		if (++bt_sco_refcount == 1)
			config_gpio_table(bt_sco_enable,
					ARRAY_SIZE(bt_sco_enable));
	} else {
		if (--bt_sco_refcount == 0) {
			config_gpio_table(bt_sco_disable,
					ARRAY_SIZE(bt_sco_disable));
			gpio_set_value(INCREDIBLEC_BT_PCM_OUT, 0);
			gpio_set_value(INCREDIBLEC_BT_PCM_SYNC, 0);
			gpio_set_value(INCREDIBLEC_BT_PCM_CLK, 0);
		}
	}
	mutex_unlock(&bt_sco_lock);
}

void incrediblec_int_mic_enable(int en)
{
	D("%s %d\n", __func__, en);
	if (en)
		pmic_mic_en(ON_CMD);
	else
		pmic_mic_en(OFF_CMD);
}

void incrediblec_ext_mic_enable(int en)
{
	static int old_state = 0, new_state = 0;

	D("%s %d\n", __func__, en);

	mutex_lock(&mic_lock);
	if (!!en)
		new_state++;
	else
		new_state--;

	if (new_state == 1 && old_state == 0)
		gpio_set_value(INCREDIBLEC_AUD_2V5_EN, 1);
	else if (new_state == 0 && old_state == 1)
		gpio_set_value(INCREDIBLEC_AUD_2V5_EN, 0);
	else
		D("%s: do nothing %d %d\n", __func__, old_state, new_state);

	old_state = new_state;
	mutex_unlock(&mic_lock);
}

void incrediblec_analog_init(void)
{
	D("%s\n", __func__);
	/* stereo pmic init */
	pmic_spkr_set_gain(LEFT_SPKR, SPKR_GAIN_PLUS12DB);
	pmic_spkr_set_gain(RIGHT_SPKR, SPKR_GAIN_PLUS12DB);
	pmic_spkr_en_right_chan(OFF_CMD);
	pmic_spkr_en_left_chan(OFF_CMD);
	pmic_spkr_add_right_left_chan(OFF_CMD);
	pmic_spkr_en_stereo(OFF_CMD);
	pmic_spkr_select_usb_with_hpf_20hz(OFF_CMD);
	pmic_spkr_bypass_mux(OFF_CMD);
	pmic_spkr_en_hpf(ON_CMD);
	pmic_spkr_en_sink_curr_from_ref_volt_cir(OFF_CMD);
	pmic_spkr_set_mux_hpf_corner_freq(SPKR_FREQ_0_73KHZ);
	pmic_mic_set_volt(MIC_VOLT_1_80V);
	pmic_set_speaker_delay(SPKR_DLY_100MS);

	gpio_direction_output(INCREDIBLEC_AUD_JACKHP_EN, 1);
	gpio_set_value(INCREDIBLEC_AUD_JACKHP_EN, 0);

	mutex_lock(&bt_sco_lock);
	config_gpio_table(bt_sco_disable,
			ARRAY_SIZE(bt_sco_disable));
	gpio_set_value(INCREDIBLEC_BT_PCM_OUT, 0);
	mutex_unlock(&bt_sco_lock);
}

int incrediblec_get_rx_vol(uint8_t hw, int level)
{
	struct q6_gain_info *info;
	int vol;

	info = &q6_audio_hw[hw];

	level = (level > 100)? 100 : ((level < 0) ? 0 : level);
	vol = info->gain[(uint32_t)((info->max_step - 1) * level / 100)];

	D("%s %d\n", __func__, vol);
	return vol;
}

static struct qsd_acoustic_ops acoustic = {
	.enable_mic_bias = incrediblec_ext_mic_enable,
};

static struct q6audio_analog_ops ops = {
	.init = incrediblec_analog_init,
	.speaker_enable = incrediblec_speaker_enable,
	.headset_enable = incrediblec_headset_enable,
	.receiver_enable = incrediblec_receiver_enable,
	.bt_sco_enable = incrediblec_bt_sco_enable,
	.int_mic_enable = incrediblec_int_mic_enable,
	.ext_mic_enable = incrediblec_ext_mic_enable,
	.get_rx_vol = incrediblec_get_rx_vol,
};

void __init incrediblec_audio_init(void)
{
	mutex_init(&mic_lock);
	mutex_init(&bt_sco_lock);
#if defined(CONFIG_QSD_AUDIO)
	q6audio_register_analog_ops(&ops);
#endif
	acoustic_register_ops(&acoustic);
	if (system_rev == 2 && incrediblec_get_engineerid() < 4)
		q6audio_set_acdb_file("default_PMIC.acdb");
}
