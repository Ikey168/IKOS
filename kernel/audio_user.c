/* IKOS Audio User Library Implementation
 * 
 * Implementation of the user-space audio library for IKOS.
 * This provides convenient functions for applications to use audio.
 */

#include "audio_user.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* Global library state */
static bool g_audio_lib_initialized = false;
static uint32_t g_device_count = 0;

/* Implementation of Audio Library API Functions */

int audio_lib_init(void) {
    if (g_audio_lib_initialized) {
        return AUDIO_SUCCESS;
    }
    
    printf("Initializing IKOS Audio Library v%d.%d\n", 
           AUDIO_USER_VERSION_MAJOR, AUDIO_USER_VERSION_MINOR);
    
    /* Get device count to validate audio system */
    int count = audio_get_device_count();
    if (count < 0) {
        printf("Failed to initialize audio library: no audio system\n");
        return count;
    }
    
    g_device_count = (uint32_t)count;
    g_audio_lib_initialized = true;
    
    printf("Audio library initialized with %d devices\n", g_device_count);
    return AUDIO_SUCCESS;
}

void audio_lib_cleanup(void) {
    if (!g_audio_lib_initialized) {
        return;
    }
    
    printf("Cleaning up IKOS Audio Library\n");
    g_audio_lib_initialized = false;
    g_device_count = 0;
}

int audio_get_device_count(void) {
    return syscall1(SYS_AUDIO_GET_DEVICE_COUNT, 0);
}

int audio_get_device_info(uint32_t device_id, audio_device_info_t* info) {
    if (!info) {
        return AUDIO_ERROR_INVALID;
    }
    
    return syscall2(SYS_AUDIO_GET_DEVICE_INFO, device_id, (uint32_t)info);
}

int audio_stream_open(uint32_t device_id, uint32_t direction, 
                      audio_format_t* format, audio_stream_t** stream) {
    if (!format || !stream) {
        return AUDIO_ERROR_INVALID;
    }
    
    /* Allocate stream structure */
    audio_stream_t* new_stream = malloc(sizeof(audio_stream_t));
    if (!new_stream) {
        return AUDIO_ERROR_NO_MEMORY;
    }
    
    /* Initialize stream */
    memset(new_stream, 0, sizeof(audio_stream_t));
    new_stream->device_id = device_id;
    new_stream->direction = direction;
    new_stream->format = *format;
    new_stream->is_open = false;
    new_stream->is_running = false;
    
    /* Open stream through system call */
    int result = syscall3(SYS_AUDIO_STREAM_OPEN, device_id, direction, (uint32_t)format);
    if (result < 0) {
        free(new_stream);
        return result;
    }
    
    /* Store stream ID returned by kernel */
    new_stream->stream_id = (uint32_t)result;
    new_stream->is_open = true;
    
    *stream = new_stream;
    return AUDIO_SUCCESS;
}

int audio_stream_close(audio_stream_t* stream) {
    if (!stream || !stream->is_open) {
        return AUDIO_ERROR_INVALID;
    }
    
    /* Stop stream if running */
    if (stream->is_running) {
        audio_stream_stop(stream);
    }
    
    /* Close stream through system call */
    int result = syscall1(SYS_AUDIO_STREAM_CLOSE, stream->stream_id);
    
    /* Free stream structure */
    stream->is_open = false;
    free(stream);
    
    return result;
}

int audio_stream_start(audio_stream_t* stream) {
    if (!stream || !stream->is_open) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (stream->is_running) {
        return AUDIO_ERROR_RUNNING;
    }
    
    int result = syscall1(SYS_AUDIO_STREAM_START, stream->stream_id);
    if (result == AUDIO_SUCCESS) {
        stream->is_running = true;
    }
    
    return result;
}

int audio_stream_stop(audio_stream_t* stream) {
    if (!stream || !stream->is_open) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (!stream->is_running) {
        return AUDIO_ERROR_STOPPED;
    }
    
    int result = syscall1(SYS_AUDIO_STREAM_STOP, stream->stream_id);
    if (result == AUDIO_SUCCESS) {
        stream->is_running = false;
    }
    
    return result;
}

int audio_stream_write(audio_stream_t* stream, const void* data, uint32_t size) {
    if (!stream || !stream->is_open || !data || size == 0) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (stream->direction != AUDIO_DIRECTION_PLAYBACK) {
        return AUDIO_ERROR_INVALID;
    }
    
    return syscall3(SYS_AUDIO_STREAM_WRITE, stream->stream_id, (uint32_t)data, size);
}

int audio_stream_read(audio_stream_t* stream, void* data, uint32_t size) {
    if (!stream || !stream->is_open || !data || size == 0) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (stream->direction != AUDIO_DIRECTION_CAPTURE) {
        return AUDIO_ERROR_INVALID;
    }
    
    return syscall3(SYS_AUDIO_STREAM_READ, stream->stream_id, (uint32_t)data, size);
}

int audio_set_volume(uint32_t device_id, uint32_t volume) {
    if (volume > 100) {
        return AUDIO_ERROR_INVALID;
    }
    
    return syscall2(SYS_AUDIO_SET_VOLUME, device_id, volume);
}

int audio_get_volume(uint32_t device_id) {
    return syscall1(SYS_AUDIO_GET_VOLUME, device_id);
}

int audio_set_mute(uint32_t device_id, bool mute) {
    return syscall2(SYS_AUDIO_SET_MUTE, device_id, mute ? 1 : 0);
}

bool audio_get_mute(uint32_t device_id) {
    int result = syscall1(SYS_AUDIO_GET_MUTE, device_id);
    return result > 0;
}

int audio_play_tone(uint32_t device_id, uint32_t frequency, uint32_t duration) {
    return syscall3(SYS_AUDIO_PLAY_TONE, device_id, frequency, duration);
}

/* Helper Functions */

const char* audio_error_string(int error) {
    switch (error) {
        case AUDIO_SUCCESS:     return "Success";
        case AUDIO_ERROR_INVALID:   return "Invalid parameter";
        case AUDIO_ERROR_NO_MEMORY: return "Out of memory";
        case AUDIO_ERROR_NO_DEVICE: return "No such device";
        case AUDIO_ERROR_BUSY:      return "Device busy";
        case AUDIO_ERROR_NOT_OPEN:  return "Stream not open";
        case AUDIO_ERROR_RUNNING:   return "Stream already running";
        case AUDIO_ERROR_STOPPED:   return "Stream not running";
        case AUDIO_ERROR_TIMEOUT:   return "Operation timeout";
        case AUDIO_ERROR_OVERFLOW:  return "Buffer overflow";
        case AUDIO_ERROR_UNDERRUN:  return "Buffer underrun";
        default:                    return "Unknown error";
    }
}

bool audio_device_supports_format(const audio_device_info_t* info, 
                                  uint32_t direction, uint32_t format) {
    if (!info) {
        return false;
    }
    
    uint32_t formats;
    if (direction == AUDIO_DIRECTION_PLAYBACK) {
        formats = info->playback_caps.formats;
    } else if (direction == AUDIO_DIRECTION_CAPTURE) {
        formats = info->capture_caps.formats;
    } else {
        return false;
    }
    
    return (formats & (1 << format)) != 0;
}

uint32_t audio_calculate_frame_size(uint16_t channels, uint16_t format) {
    uint32_t sample_size;
    
    switch (format) {
        case AUDIO_FORMAT_PCM_U8:
            sample_size = 1;
            break;
        case AUDIO_FORMAT_PCM_S16_LE:
        case AUDIO_FORMAT_PCM_S16_BE:
            sample_size = 2;
            break;
        case AUDIO_FORMAT_PCM_S24_LE:
            sample_size = 3;
            break;
        case AUDIO_FORMAT_PCM_S32_LE:
            sample_size = 4;
            break;
        default:
            return 0;
    }
    
    return channels * sample_size;
}

/* Wave File Functions */

void audio_create_wave_header(wave_header_t* header, uint32_t sample_rate,
                              uint16_t channels, uint16_t bits_per_sample, 
                              uint32_t data_size) {
    if (!header) {
        return;
    }
    
    memset(header, 0, sizeof(wave_header_t));
    
    /* RIFF header */
    memcpy(header->riff, "RIFF", 4);
    header->file_size = sizeof(wave_header_t) + data_size - 8;
    memcpy(header->wave, "WAVE", 4);
    
    /* Format chunk */
    memcpy(header->fmt, "fmt ", 4);
    header->fmt_size = 16;
    header->format = 1; /* PCM */
    header->channels = channels;
    header->sample_rate = sample_rate;
    header->bits_per_sample = bits_per_sample;
    header->byte_rate = sample_rate * channels * (bits_per_sample / 8);
    header->block_align = channels * (bits_per_sample / 8);
    
    /* Data chunk */
    memcpy(header->data, "data", 4);
    header->data_size = data_size;
}

int audio_play_wave_file(uint32_t device_id, const char* filename) {
    if (!filename) {
        return AUDIO_ERROR_INVALID;
    }
    
    /* Open file */
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open wave file: %s\n", filename);
        return AUDIO_ERROR_INVALID;
    }
    
    /* Read wave header */
    wave_header_t header;
    if (read(fd, &header, sizeof(header)) != sizeof(header)) {
        close(fd);
        return AUDIO_ERROR_INVALID;
    }
    
    /* Validate wave file */
    if (memcmp(header.riff, "RIFF", 4) != 0 || 
        memcmp(header.wave, "WAVE", 4) != 0 ||
        memcmp(header.data, "data", 4) != 0) {
        printf("Invalid wave file format\n");
        close(fd);
        return AUDIO_ERROR_INVALID;
    }
    
    /* Set up audio format */
    audio_format_t format;
    format.sample_rate = header.sample_rate;
    format.channels = header.channels;
    format.format = (header.bits_per_sample == 8) ? AUDIO_FORMAT_PCM_U8 : AUDIO_FORMAT_PCM_S16_LE;
    format.frame_size = audio_calculate_frame_size(format.channels, format.format);
    format.period_size = 1024;
    format.buffer_size = 4096;
    
    /* Open audio stream */
    audio_stream_t* stream;
    int result = audio_stream_open(device_id, AUDIO_DIRECTION_PLAYBACK, &format, &stream);
    if (result != AUDIO_SUCCESS) {
        close(fd);
        return result;
    }
    
    /* Start streaming */
    result = audio_stream_start(stream);
    if (result != AUDIO_SUCCESS) {
        audio_stream_close(stream);
        close(fd);
        return result;
    }
    
    /* Play audio data */
    char buffer[4096];
    uint32_t remaining = header.data_size;
    
    while (remaining > 0) {
        uint32_t to_read = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
        ssize_t bytes_read = read(fd, buffer, to_read);
        
        if (bytes_read <= 0) {
            break;
        }
        
        int write_result = audio_stream_write(stream, buffer, bytes_read);
        if (write_result < 0) {
            printf("Audio write error: %s\n", audio_error_string(write_result));
            break;
        }
        
        remaining -= bytes_read;
    }
    
    /* Cleanup */
    audio_stream_stop(stream);
    audio_stream_close(stream);
    close(fd);
    
    return AUDIO_SUCCESS;
}

/* Audio Buffer Functions */

audio_buffer_t* audio_buffer_create(uint32_t size) {
    audio_buffer_t* buffer = malloc(sizeof(audio_buffer_t));
    if (!buffer) {
        return NULL;
    }
    
    buffer->data = malloc(size);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }
    
    buffer->size = size;
    buffer->used = 0;
    buffer->position = 0;
    
    return buffer;
}

void audio_buffer_destroy(audio_buffer_t* buffer) {
    if (!buffer) {
        return;
    }
    
    if (buffer->data) {
        free(buffer->data);
    }
    
    free(buffer);
}

int audio_buffer_write(audio_buffer_t* buffer, const void* data, uint32_t size) {
    if (!buffer || !data || size == 0) {
        return AUDIO_ERROR_INVALID;
    }
    
    uint32_t available = buffer->size - buffer->used;
    if (size > available) {
        return AUDIO_ERROR_OVERFLOW;
    }
    
    uint32_t write_pos = (buffer->position + buffer->used) % buffer->size;
    uint32_t first_chunk = buffer->size - write_pos;
    
    if (size <= first_chunk) {
        memcpy((char*)buffer->data + write_pos, data, size);
    } else {
        memcpy((char*)buffer->data + write_pos, data, first_chunk);
        memcpy(buffer->data, (char*)data + first_chunk, size - first_chunk);
    }
    
    buffer->used += size;
    return size;
}

int audio_buffer_read(audio_buffer_t* buffer, void* data, uint32_t size) {
    if (!buffer || !data || size == 0) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (size > buffer->used) {
        size = buffer->used;
    }
    
    if (size == 0) {
        return 0;
    }
    
    uint32_t first_chunk = buffer->size - buffer->position;
    
    if (size <= first_chunk) {
        memcpy(data, (char*)buffer->data + buffer->position, size);
    } else {
        memcpy(data, (char*)buffer->data + buffer->position, first_chunk);
        memcpy((char*)data + first_chunk, buffer->data, size - first_chunk);
    }
    
    buffer->position = (buffer->position + size) % buffer->size;
    buffer->used -= size;
    
    return size;
}

void audio_buffer_reset(audio_buffer_t* buffer) {
    if (!buffer) {
        return;
    }
    
    buffer->used = 0;
    buffer->position = 0;
}

uint32_t audio_buffer_available(const audio_buffer_t* buffer) {
    if (!buffer) {
        return 0;
    }
    
    return buffer->size - buffer->used;
}

uint32_t audio_buffer_used(const audio_buffer_t* buffer) {
    if (!buffer) {
        return 0;
    }
    
    return buffer->used;
}
