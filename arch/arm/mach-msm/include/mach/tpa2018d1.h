/*
 * Definitions for tpa2018d1 speaker amp chip.
 */
#ifndef TPA2018D1_H
#define TPA2018D1_H

#include <linux/ioctl.h>

#define TPA2018D1_I2C_NAME "tpa2018d1"
#define TPA2018D1_CMD_LEN 8

struct tpa2018d1_platform_data {
        uint32_t gpio_tpa2018_spk_en;
};

struct tpa2018d1_config_data {
	unsigned char *cmd_data;  /* [mode][cmd_len][cmds..] */
	unsigned int mode_num;
	unsigned int data_len;
};

enum tpa2018d1_mode {
	TPA2018_MODE_OFF,
	TPA2018_MODE_PLAYBACK,
	TPA2018_MODE_RINGTONE,
	TPA2018_MODE_VOICE_CALL,
	TPA2018_NUM_MODES,
};

#define TPA2018_IOCTL_MAGIC 'a'
#define TPA2018_SET_CONFIG      _IOW(TPA2018_IOCTL_MAGIC, 0x01, unsigned)
#define TPA2018_READ_CONFIG     _IOW(TPA2018_IOCTL_MAGIC, 0x02, unsigned)
#define TPA2018_SET_PARAM       _IOW(TPA2018_IOCTL_MAGIC, 0x03, unsigned)
#define TPA2018_SET_MODE        _IOW(TPA2018_IOCTL_MAGIC, 0x04, unsigned)
void set_speaker_amp(int on);

#endif
