/* IKOS Audio System Framework Header
 * 
 * Comprehensive audio driver framework for IKOS operating system.
 * This implementation provides:
 * - Audio device enumeration and management
 * - Audio playback and recording APIs
 * - Support for standard audio codecs
 * - Audio buffer management and streaming
 * - User-space audio application interface
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>

/* Audio Constants */
#define AUDIO_MAX_DEVICES       16      /* Maximum audio devices */
#define AUDIO_MAX_CHANNELS      8       /* Maximum audio channels */
#define AUDIO_MAX_BUFFERS       32      /* Maximum audio buffers */
#define AUDIO_MAX_STREAMS       16      /* Maximum audio streams */
#define AUDIO_BUFFER_SIZE       4096    /* Default buffer size */
#define AUDIO_MAX_SAMPLE_RATE   192000  /* Maximum sample rate */
#define AUDIO_MIN_SAMPLE_RATE   8000    /* Minimum sample rate */

/* Audio Sample Formats */
#define AUDIO_FORMAT_PCM8       0       /* 8-bit PCM */
#define AUDIO_FORMAT_PCM16      1       /* 16-bit PCM */
#define AUDIO_FORMAT_PCM24      2       /* 24-bit PCM */
#define AUDIO_FORMAT_PCM32      3       /* 32-bit PCM */
#define AUDIO_FORMAT_FLOAT32    4       /* 32-bit float */
#define AUDIO_FORMAT_COMPRESSED 5       /* Compressed format */

/* Audio Device Types */
#define AUDIO_DEVICE_PLAYBACK   0x01    /* Playback device */
#define AUDIO_DEVICE_CAPTURE    0x02    /* Capture device */
#define AUDIO_DEVICE_DUPLEX     0x03    /* Full duplex device */

/* Audio Device Classes */
#define AUDIO_CLASS_INTEGRATED  0       /* Integrated audio */
#define AUDIO_CLASS_PCI         1       /* PCI audio card */
#define AUDIO_CLASS_USB         2       /* USB audio device */
#define AUDIO_CLASS_BLUETOOTH   3       /* Bluetooth audio */
#define AUDIO_CLASS_NETWORK     4       /* Network audio */

/* Audio Stream States */
#define AUDIO_STREAM_IDLE       0       /* Stream idle */
#define AUDIO_STREAM_PREPARED   1       /* Stream prepared */
#define AUDIO_STREAM_RUNNING    2       /* Stream running */
#define AUDIO_STREAM_PAUSED     3       /* Stream paused */
#define AUDIO_STREAM_STOPPED    4       /* Stream stopped */
#define AUDIO_STREAM_ERROR      5       /* Stream error */

/* Audio Result Codes */
#define AUDIO_SUCCESS           0       /* Operation successful */
#define AUDIO_ERROR_INVALID     -1      /* Invalid parameter */
#define AUDIO_ERROR_NO_MEMORY   -2      /* Out of memory */
#define AUDIO_ERROR_NO_DEVICE   -3      /* No device available */
#define AUDIO_ERROR_BUSY        -4      /* Device busy */
#define AUDIO_ERROR_NOT_READY   -5      /* Device not ready */
#define AUDIO_ERROR_IO          -6      /* I/O error */
#define AUDIO_ERROR_FORMAT      -7      /* Unsupported format */
#define AUDIO_ERROR_UNDERRUN    -8      /* Buffer underrun */
#define AUDIO_ERROR_OVERRUN     -9      /* Buffer overrun */

/* Audio Hardware Interface Types */
#define AUDIO_HW_AC97           1       /* AC97 codec */
#define AUDIO_HW_HDA            2       /* Intel HDA */
#define AUDIO_HW_SB16           3       /* Sound Blaster 16 */
#define AUDIO_HW_ES1371         4       /* Ensoniq ES1371 */
#define AUDIO_HW_USB_AUDIO      5       /* USB Audio Class */

/* Audio Buffer Structure */
typedef struct audio_buffer {
    void*    data;                      /* Buffer data */
    uint32_t size;                      /* Buffer size */
    uint32_t used;                      /* Used bytes */
    uint32_t frames;                    /* Number of frames */
    uint64_t timestamp;                 /* Buffer timestamp */
    uint32_t flags;                     /* Buffer flags */
    struct audio_buffer* next;          /* Next buffer in chain */
} audio_buffer_t;

/* Audio Format Structure */
typedef struct audio_format {
    uint32_t sample_rate;               /* Sample rate (Hz) */
    uint16_t channels;                  /* Number of channels */
    uint16_t format;                    /* Sample format */
    uint16_t frame_size;                /* Bytes per frame */
    uint16_t period_size;               /* Period size in frames */
    uint32_t buffer_size;               /* Buffer size in frames */
} audio_format_t;

/* Audio Device Capabilities */
typedef struct audio_caps {
    uint32_t formats;                   /* Supported formats bitmask */
    uint32_t min_rate;                  /* Minimum sample rate */
    uint32_t max_rate;                  /* Maximum sample rate */
    uint16_t min_channels;              /* Minimum channels */
    uint16_t max_channels;              /* Maximum channels */
    uint16_t min_period;                /* Minimum period size */
    uint16_t max_period;                /* Maximum period size */
    uint32_t min_buffer;                /* Minimum buffer size */
    uint32_t max_buffer;                /* Maximum buffer size */
} audio_caps_t;

/* Audio Stream Structure */
typedef struct audio_stream {
    uint32_t stream_id;                 /* Stream ID */
    uint32_t device_id;                 /* Device ID */
    uint32_t direction;                 /* Stream direction */
    uint32_t state;                     /* Stream state */
    audio_format_t format;              /* Stream format */
    audio_buffer_t* buffers;            /* Buffer chain */
    uint32_t buffer_count;              /* Number of buffers */
    uint64_t frames_processed;          /* Frames processed */
    void* user_data;                    /* User data */
    void (*callback)(struct audio_stream* stream, void* data); /* Callback */
    struct audio_device* device;        /* Associated device */
} audio_stream_t;

/* Audio Device Structure */
typedef struct audio_device {
    uint32_t device_id;                 /* Device ID */
    char name[64];                      /* Device name */
    uint32_t class;                     /* Device class */
    uint32_t type;                      /* Device type */
    uint32_t capabilities;              /* Device capabilities */
    audio_caps_t playback_caps;         /* Playback capabilities */
    audio_caps_t capture_caps;          /* Capture capabilities */
    audio_stream_t* streams[AUDIO_MAX_STREAMS]; /* Active streams */
    uint32_t stream_count;              /* Number of streams */
    void* private_data;                 /* Driver private data */
    struct audio_driver* driver;        /* Associated driver */
    bool enabled;                       /* Device enabled */
    bool connected;                     /* Device connected */
} audio_device_t;

/* Audio Driver Structure */
typedef struct audio_driver {
    const char* name;                   /* Driver name */
    uint32_t type;                      /* Hardware type */
    
    /* Driver operations */
    int (*probe)(struct audio_device* device);
    int (*remove)(struct audio_device* device);
    int (*suspend)(struct audio_device* device);
    int (*resume)(struct audio_device* device);
    
    /* Stream operations */
    int (*stream_open)(struct audio_stream* stream);
    int (*stream_close)(struct audio_stream* stream);
    int (*stream_start)(struct audio_stream* stream);
    int (*stream_stop)(struct audio_stream* stream);
    int (*stream_pause)(struct audio_stream* stream);
    
    /* Buffer operations */
    int (*buffer_alloc)(struct audio_stream* stream, uint32_t size);
    int (*buffer_free)(struct audio_stream* stream);
    int (*buffer_queue)(struct audio_stream* stream, audio_buffer_t* buffer);
    
    /* Control operations */
    int (*set_volume)(struct audio_device* device, uint32_t volume);
    int (*get_volume)(struct audio_device* device, uint32_t* volume);
    int (*set_mute)(struct audio_device* device, bool mute);
    int (*get_mute)(struct audio_device* device, bool* mute);
    
    /* Driver data */
    void* private_data;
    struct audio_driver* next;
} audio_driver_t;

/* Audio System State */
typedef struct audio_system {
    audio_device_t* devices[AUDIO_MAX_DEVICES];
    audio_driver_t* drivers;
    uint32_t device_count;
    uint32_t next_stream_id;
    bool initialized;
} audio_system_t;

/* Audio Core Functions */
int audio_init(void);
void audio_shutdown(void);
int audio_register_driver(audio_driver_t* driver);
void audio_unregister_driver(audio_driver_t* driver);

/* Device Management */
int audio_register_device(audio_device_t* device);
void audio_unregister_device(audio_device_t* device);
audio_device_t* audio_find_device(uint32_t device_id);
audio_device_t* audio_get_default_device(uint32_t type);
uint32_t audio_get_device_count(void);
int audio_enumerate_devices(audio_device_t** devices, uint32_t max_count);

/* Stream Management */
int audio_stream_open(uint32_t device_id, uint32_t direction, audio_format_t* format, audio_stream_t** stream);
int audio_stream_close(audio_stream_t* stream);
int audio_stream_start(audio_stream_t* stream);
int audio_stream_stop(audio_stream_t* stream);
int audio_stream_pause(audio_stream_t* stream);
int audio_stream_resume(audio_stream_t* stream);

/* Buffer Management */
int audio_buffer_alloc(audio_stream_t* stream, uint32_t size, audio_buffer_t** buffer);
void audio_buffer_free(audio_buffer_t* buffer);
int audio_buffer_queue(audio_stream_t* stream, audio_buffer_t* buffer);
int audio_buffer_dequeue(audio_stream_t* stream, audio_buffer_t** buffer);

/* Playback Functions */
int audio_play_buffer(uint32_t device_id, void* data, uint32_t size, audio_format_t* format);
int audio_play_file(uint32_t device_id, const char* filename);
int audio_play_tone(uint32_t device_id, uint32_t frequency, uint32_t duration);

/* Recording Functions */
int audio_record_start(uint32_t device_id, audio_format_t* format, audio_stream_t** stream);
int audio_record_stop(audio_stream_t* stream);
int audio_record_to_file(uint32_t device_id, const char* filename, uint32_t duration);

/* Volume Control */
int audio_set_master_volume(uint32_t volume);
int audio_get_master_volume(uint32_t* volume);
int audio_set_device_volume(uint32_t device_id, uint32_t volume);
int audio_get_device_volume(uint32_t device_id, uint32_t* volume);
int audio_set_mute(uint32_t device_id, bool mute);
int audio_get_mute(uint32_t device_id, bool* mute);

/* Format Conversion */
int audio_convert_format(void* src, audio_format_t* src_fmt, void* dst, audio_format_t* dst_fmt, uint32_t frames);
uint32_t audio_format_frame_size(audio_format_t* format);
uint32_t audio_format_duration(uint32_t frames, audio_format_t* format);

/* Utility Functions */
const char* audio_format_string(uint16_t format);
const char* audio_device_type_string(uint32_t type);
const char* audio_stream_state_string(uint32_t state);
void audio_dump_device_info(audio_device_t* device);
void audio_dump_stream_info(audio_stream_t* stream);

/* System Call Interface */
int sys_audio_get_device_count(void);
int sys_audio_get_device_info(uint32_t device_id, void* user_info);
int sys_audio_stream_open(uint32_t device_id, uint32_t direction, void* user_format);
int sys_audio_stream_close(uint32_t stream_id);
int sys_audio_stream_write(uint32_t stream_id, void* user_data, uint32_t size);
int sys_audio_stream_read(uint32_t stream_id, void* user_data, uint32_t size);
int sys_audio_set_volume(uint32_t device_id, uint32_t volume);
int sys_audio_get_volume(uint32_t device_id);

#endif /* AUDIO_H */
