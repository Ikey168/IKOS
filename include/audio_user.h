/* IKOS Audio User Library
 * 
 * User-space library for audio applications in IKOS.
 * This library provides a convenient interface for applications
 * to interact with the audio system through system calls.
 */

#ifndef AUDIO_USER_H
#define AUDIO_USER_H

#include <stdint.h>
#include <stdbool.h>

/* Audio User Library API Version */
#define AUDIO_USER_VERSION_MAJOR 1
#define AUDIO_USER_VERSION_MINOR 0

/* Audio System Call Numbers (must match kernel) */
#define SYS_AUDIO_GET_DEVICE_COUNT  300
#define SYS_AUDIO_GET_DEVICE_INFO   301
#define SYS_AUDIO_STREAM_OPEN       302
#define SYS_AUDIO_STREAM_CLOSE      303
#define SYS_AUDIO_STREAM_START      304
#define SYS_AUDIO_STREAM_STOP       305
#define SYS_AUDIO_STREAM_WRITE      306
#define SYS_AUDIO_STREAM_READ       307
#define SYS_AUDIO_SET_VOLUME        308
#define SYS_AUDIO_GET_VOLUME        309
#define SYS_AUDIO_SET_MUTE          310
#define SYS_AUDIO_GET_MUTE          311
#define SYS_AUDIO_PLAY_TONE         312

/* Audio Error Codes */
#define AUDIO_SUCCESS           0
#define AUDIO_ERROR_INVALID    -1
#define AUDIO_ERROR_NO_MEMORY  -2
#define AUDIO_ERROR_NO_DEVICE  -3
#define AUDIO_ERROR_BUSY       -4
#define AUDIO_ERROR_NOT_OPEN   -5
#define AUDIO_ERROR_RUNNING    -6
#define AUDIO_ERROR_STOPPED    -7
#define AUDIO_ERROR_TIMEOUT    -8
#define AUDIO_ERROR_OVERFLOW   -9
#define AUDIO_ERROR_UNDERRUN   -10

/* Audio Device Classes */
#define AUDIO_CLASS_PCM         1
#define AUDIO_CLASS_MIDI        2
#define AUDIO_CLASS_MIXER       3

/* Audio Device Types */
#define AUDIO_TYPE_PLAYBACK     1
#define AUDIO_TYPE_CAPTURE      2
#define AUDIO_TYPE_DUPLEX       3

/* Audio Stream Directions */
#define AUDIO_DIRECTION_PLAYBACK 1
#define AUDIO_DIRECTION_CAPTURE  2

/* Audio Formats */
#define AUDIO_FORMAT_PCM_U8     1
#define AUDIO_FORMAT_PCM_S16_LE 2
#define AUDIO_FORMAT_PCM_S16_BE 3
#define AUDIO_FORMAT_PCM_S24_LE 4
#define AUDIO_FORMAT_PCM_S32_LE 5

/* Audio Device Capabilities */
#define AUDIO_CAP_PLAYBACK      (1 << 0)
#define AUDIO_CAP_CAPTURE       (1 << 1)
#define AUDIO_CAP_VOLUME        (1 << 2)
#define AUDIO_CAP_MUTE          (1 << 3)
#define AUDIO_CAP_TONE          (1 << 4)

/* Audio Device Info Structure */
typedef struct audio_device_info {
    uint32_t device_id;
    char name[64];
    uint32_t class;
    uint32_t type;
    uint32_t capabilities;
    struct {
        uint32_t formats;
        uint32_t min_rate;
        uint32_t max_rate;
        uint16_t min_channels;
        uint16_t max_channels;
    } playback_caps;
    struct {
        uint32_t formats;
        uint32_t min_rate;
        uint32_t max_rate;
        uint16_t min_channels;
        uint16_t max_channels;
    } capture_caps;
    bool enabled;
    bool connected;
} audio_device_info_t;

/* Audio Format Structure */
typedef struct audio_format {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t format;
    uint16_t frame_size;
    uint16_t period_size;
    uint32_t buffer_size;
} audio_format_t;

/* Audio Stream Handle */
typedef struct audio_stream {
    uint32_t stream_id;
    uint32_t device_id;
    uint32_t direction;
    audio_format_t format;
    bool is_open;
    bool is_running;
} audio_stream_t;

/* System call wrapper functions */
static inline int syscall1(int num, uint32_t arg1) {
    int result;
    asm volatile ("int $0x80" : "=a"(result) : "a"(num), "b"(arg1));
    return result;
}

static inline int syscall2(int num, uint32_t arg1, uint32_t arg2) {
    int result;
    asm volatile ("int $0x80" : "=a"(result) : "a"(num), "b"(arg1), "c"(arg2));
    return result;
}

static inline int syscall3(int num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int result;
    asm volatile ("int $0x80" : "=a"(result) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
    return result;
}

static inline int syscall4(int num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
    int result;
    asm volatile ("int $0x80" : "=a"(result) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return result;
}

/* Audio Library API Functions */

/* Initialize the audio library */
int audio_lib_init(void);

/* Cleanup the audio library */
void audio_lib_cleanup(void);

/* Get the number of audio devices */
int audio_get_device_count(void);

/* Get information about a specific audio device */
int audio_get_device_info(uint32_t device_id, audio_device_info_t* info);

/* Open an audio stream */
int audio_stream_open(uint32_t device_id, uint32_t direction, 
                      audio_format_t* format, audio_stream_t** stream);

/* Close an audio stream */
int audio_stream_close(audio_stream_t* stream);

/* Start audio streaming */
int audio_stream_start(audio_stream_t* stream);

/* Stop audio streaming */
int audio_stream_stop(audio_stream_t* stream);

/* Write audio data to a playback stream */
int audio_stream_write(audio_stream_t* stream, const void* data, uint32_t size);

/* Read audio data from a capture stream */
int audio_stream_read(audio_stream_t* stream, void* data, uint32_t size);

/* Set device volume (0-100) */
int audio_set_volume(uint32_t device_id, uint32_t volume);

/* Get device volume */
int audio_get_volume(uint32_t device_id);

/* Set device mute state */
int audio_set_mute(uint32_t device_id, bool mute);

/* Get device mute state */
bool audio_get_mute(uint32_t device_id);

/* Play a tone */
int audio_play_tone(uint32_t device_id, uint32_t frequency, uint32_t duration);

/* Helper functions */

/* Get a human-readable error string */
const char* audio_error_string(int error);

/* Check if device supports a format */
bool audio_device_supports_format(const audio_device_info_t* info, 
                                  uint32_t direction, uint32_t format);

/* Calculate frame size for a format */
uint32_t audio_calculate_frame_size(uint16_t channels, uint16_t format);

/* Convert between sample rates */
int audio_convert_sample_rate(const void* input, void* output,
                              uint32_t frames, uint32_t in_rate, uint32_t out_rate);

/* Simple wave file functions */
typedef struct wave_header {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
} __attribute__((packed)) wave_header_t;

/* Create a wave header */
void audio_create_wave_header(wave_header_t* header, uint32_t sample_rate,
                              uint16_t channels, uint16_t bits_per_sample, 
                              uint32_t data_size);

/* Play a wave file */
int audio_play_wave_file(uint32_t device_id, const char* filename);

/* Record to a wave file */
int audio_record_wave_file(uint32_t device_id, const char* filename, uint32_t duration);

/* Audio Buffer Management */
typedef struct audio_buffer {
    void* data;
    uint32_t size;
    uint32_t used;
    uint32_t position;
} audio_buffer_t;

/* Create an audio buffer */
audio_buffer_t* audio_buffer_create(uint32_t size);

/* Destroy an audio buffer */
void audio_buffer_destroy(audio_buffer_t* buffer);

/* Write data to buffer */
int audio_buffer_write(audio_buffer_t* buffer, const void* data, uint32_t size);

/* Read data from buffer */
int audio_buffer_read(audio_buffer_t* buffer, void* data, uint32_t size);

/* Reset buffer */
void audio_buffer_reset(audio_buffer_t* buffer);

/* Get available space in buffer */
uint32_t audio_buffer_available(const audio_buffer_t* buffer);

/* Get used space in buffer */
uint32_t audio_buffer_used(const audio_buffer_t* buffer);

#endif /* AUDIO_USER_H */
