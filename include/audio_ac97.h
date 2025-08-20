/* IKOS AC97 Audio Codec Driver Header
 * 
 * AC97 audio codec driver for IKOS operating system.
 * This implementation provides:
 * - AC97 codec initialization and configuration
 * - Audio playback and recording support
 * - Mixer control interface
 * - Standard AC97 register access
 */

#ifndef AUDIO_AC97_H
#define AUDIO_AC97_H

#include "audio.h"
#include <stdint.h>

/* AC97 Codec Registers */
#define AC97_REG_RESET          0x00    /* Reset */
#define AC97_REG_MASTER_VOL     0x02    /* Master Volume */
#define AC97_REG_MONO_VOL       0x06    /* Mono Volume */
#define AC97_REG_PC_BEEP_VOL    0x0A    /* PC Beep Volume */
#define AC97_REG_PHONE_VOL      0x0C    /* Phone Volume */
#define AC97_REG_MIC_VOL        0x0E    /* Microphone Volume */
#define AC97_REG_LINE_IN_VOL    0x10    /* Line In Volume */
#define AC97_REG_CD_VOL         0x12    /* CD Volume */
#define AC97_REG_AUX_VOL        0x16    /* Auxiliary Volume */
#define AC97_REG_PCM_OUT_VOL    0x18    /* PCM Out Volume */
#define AC97_REG_RECORD_SELECT  0x1A    /* Record Select */
#define AC97_REG_RECORD_GAIN    0x1C    /* Record Gain */
#define AC97_REG_GENERAL_PURPOSE 0x20   /* General Purpose */
#define AC97_REG_3D_CONTROL     0x22    /* 3D Control */
#define AC97_REG_POWERDOWN      0x26    /* Powerdown Control/Status */
#define AC97_REG_EXT_AUDIO_ID   0x28    /* Extended Audio ID */
#define AC97_REG_EXT_AUDIO_CTRL 0x2A    /* Extended Audio Control */
#define AC97_REG_PCM_FRONT_DAC  0x2C    /* PCM Front DAC Rate */
#define AC97_REG_PCM_SURR_DAC   0x2E    /* PCM Surround DAC Rate */
#define AC97_REG_PCM_LFE_DAC    0x30    /* PCM LFE DAC Rate */
#define AC97_REG_PCM_LR_ADC     0x32    /* PCM LR ADC Rate */
#define AC97_REG_PCM_MIC_ADC    0x34    /* PCM MIC ADC Rate */
#define AC97_REG_VENDOR_ID1     0x7C    /* Vendor ID1 */
#define AC97_REG_VENDOR_ID2     0x7E    /* Vendor ID2 */

/* AC97 Volume Control Bits */
#define AC97_VOL_MUTE           (1 << 15) /* Mute bit */
#define AC97_VOL_MASK           0x1F      /* Volume mask */

/* AC97 Extended Audio Features */
#define AC97_EXT_VRA            (1 << 0)  /* Variable Rate Audio */
#define AC97_EXT_DRA            (1 << 1)  /* Double Rate Audio */
#define AC97_EXT_SPDIF          (1 << 2)  /* S/PDIF */
#define AC97_EXT_VRM            (1 << 3)  /* Variable Rate Microphone */
#define AC97_EXT_DSA            (1 << 4)  /* Direct Stream Architecture */

/* AC97 Sample Rates */
#define AC97_RATE_8000          8000
#define AC97_RATE_11025         11025
#define AC97_RATE_16000         16000
#define AC97_RATE_22050         22050
#define AC97_RATE_32000         32000
#define AC97_RATE_44100         44100
#define AC97_RATE_48000         48000
#define AC97_RATE_88200         88200
#define AC97_RATE_96000         96000

/* AC97 Codec Structure */
typedef struct ac97_codec {
    uint32_t base_addr;                 /* Base I/O address */
    uint32_t irq;                       /* IRQ number */
    uint16_t vendor_id;                 /* Vendor ID */
    uint16_t device_id;                 /* Device ID */
    uint32_t capabilities;              /* Codec capabilities */
    bool initialized;                   /* Initialization status */
    audio_device_t* audio_device;       /* Associated audio device */
} ac97_codec_t;

/* AC97 Driver Functions */
int ac97_init(void);
void ac97_shutdown(void);
int ac97_probe(audio_device_t* device);
int ac97_remove(audio_device_t* device);

/* Codec Operations */
int ac97_codec_init(ac97_codec_t* codec);
void ac97_codec_reset(ac97_codec_t* codec);
uint16_t ac97_read_reg(ac97_codec_t* codec, uint8_t reg);
void ac97_write_reg(ac97_codec_t* codec, uint8_t reg, uint16_t value);

/* Stream Operations */
int ac97_stream_open(audio_stream_t* stream);
int ac97_stream_close(audio_stream_t* stream);
int ac97_stream_start(audio_stream_t* stream);
int ac97_stream_stop(audio_stream_t* stream);
int ac97_stream_pause(audio_stream_t* stream);

/* Buffer Operations */
int ac97_buffer_alloc(audio_stream_t* stream, uint32_t size);
int ac97_buffer_free(audio_stream_t* stream);
int ac97_buffer_queue(audio_stream_t* stream, audio_buffer_t* buffer);

/* Volume Control */
int ac97_set_master_volume(ac97_codec_t* codec, uint8_t left, uint8_t right);
int ac97_get_master_volume(ac97_codec_t* codec, uint8_t* left, uint8_t* right);
int ac97_set_pcm_volume(ac97_codec_t* codec, uint8_t left, uint8_t right);
int ac97_get_pcm_volume(ac97_codec_t* codec, uint8_t* left, uint8_t* right);
int ac97_set_mute(ac97_codec_t* codec, bool mute);
int ac97_get_mute(ac97_codec_t* codec, bool* mute);

/* Mixer Control */
int ac97_set_record_source(ac97_codec_t* codec, uint8_t source);
int ac97_get_record_source(ac97_codec_t* codec, uint8_t* source);
int ac97_set_mic_volume(ac97_codec_t* codec, uint8_t volume);
int ac97_get_mic_volume(ac97_codec_t* codec, uint8_t* volume);

/* Sample Rate Control */
int ac97_set_sample_rate(ac97_codec_t* codec, uint32_t rate);
int ac97_get_sample_rate(ac97_codec_t* codec, uint32_t* rate);
bool ac97_supports_rate(ac97_codec_t* codec, uint32_t rate);

/* Utility Functions */
void ac97_dump_registers(ac97_codec_t* codec);
const char* ac97_vendor_string(uint16_t vendor_id);

#endif /* AUDIO_AC97_H */
