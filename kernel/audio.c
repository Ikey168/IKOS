/* IKOS Audio System Framework Implementation
 * 
 * Core audio driver framework for IKOS operating system.
 * This implementation provides:
 * - Audio device enumeration and management
 * - Audio stream handling and buffer management
 * - Audio driver registration and dispatch
 * - Audio format conversion and utilities
 */

#include "audio.h"
#include "audio_ac97.h"
#include "interrupts.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"
#include <stddef.h>
#include <stdlib.h>

/* Global Audio System State */
static audio_system_t audio_system;

/* Internal function prototypes */
static int audio_validate_format(audio_format_t* format);
static uint32_t audio_allocate_stream_id(void);
static void audio_handle_stream_callback(audio_stream_t* stream);
static int audio_setup_default_devices(void);

/* Audio System Initialization */
int audio_init(void) {
    if (audio_system.initialized) {
        return AUDIO_SUCCESS;
    }
    
    printf("[AUDIO] Initializing audio system framework\n");
    
    /* Clear system state */
    memset(&audio_system, 0, sizeof(audio_system_t));
    
    /* Initialize device array */
    for (int i = 0; i < AUDIO_MAX_DEVICES; i++) {
        audio_system.devices[i] = NULL;
    }
    
    audio_system.drivers = NULL;
    audio_system.device_count = 0;
    audio_system.next_stream_id = 1;
    
    /* Initialize AC97 driver */
    int result = ac97_init();
    if (result != AUDIO_SUCCESS) {
        printf("[AUDIO] AC97 initialization failed: %d\n", result);
        return result;
    }
    
    /* Setup default devices */
    result = audio_setup_default_devices();
    if (result != AUDIO_SUCCESS) {
        printf("[AUDIO] Default device setup failed: %d\n", result);
        return result;
    }
    
    audio_system.initialized = true;
    printf("[AUDIO] Audio system initialized successfully\n");
    return AUDIO_SUCCESS;
}

/* Audio System Shutdown */
void audio_shutdown(void) {
    if (!audio_system.initialized) {
        return;
    }
    
    printf("[AUDIO] Shutting down audio system\n");
    
    /* Stop all streams and free devices */
    for (int i = 0; i < AUDIO_MAX_DEVICES; i++) {
        if (audio_system.devices[i]) {
            audio_device_t* device = audio_system.devices[i];
            
            /* Stop all streams */
            for (int j = 0; j < AUDIO_MAX_STREAMS; j++) {
                if (device->streams[j]) {
                    audio_stream_stop(device->streams[j]);
                    audio_stream_close(device->streams[j]);
                }
            }
            
            /* Remove device */
            if (device->driver && device->driver->remove) {
                device->driver->remove(device);
            }
            
            free(device);
            audio_system.devices[i] = NULL;
        }
    }
    
    /* Shutdown drivers */
    ac97_shutdown();
    
    audio_system.initialized = false;
    printf("[AUDIO] Audio system shutdown complete\n");
}

/* Register Audio Driver */
int audio_register_driver(audio_driver_t* driver) {
    if (!driver || !driver->name) {
        return AUDIO_ERROR_INVALID;
    }
    
    printf("[AUDIO] Registering audio driver: %s\n", driver->name);
    
    /* Add to driver list */
    driver->next = audio_system.drivers;
    audio_system.drivers = driver;
    
    return AUDIO_SUCCESS;
}

/* Unregister Audio Driver */
void audio_unregister_driver(audio_driver_t* driver) {
    if (!driver) {
        return;
    }
    
    printf("[AUDIO] Unregistering audio driver: %s\n", driver->name);
    
    /* Remove from driver list */
    audio_driver_t** current = &audio_system.drivers;
    while (*current) {
        if (*current == driver) {
            *current = driver->next;
            break;
        }
        current = &(*current)->next;
    }
}

/* Register Audio Device */
int audio_register_device(audio_device_t* device) {
    if (!device || !audio_system.initialized) {
        return AUDIO_ERROR_INVALID;
    }
    
    /* Find free device slot */
    for (int i = 0; i < AUDIO_MAX_DEVICES; i++) {
        if (!audio_system.devices[i]) {
            device->device_id = i;
            audio_system.devices[i] = device;
            audio_system.device_count++;
            
            printf("[AUDIO] Registered audio device %d: %s\n", i, device->name);
            return AUDIO_SUCCESS;
        }
    }
    
    return AUDIO_ERROR_NO_MEMORY;
}

/* Unregister Audio Device */
void audio_unregister_device(audio_device_t* device) {
    if (!device || device->device_id >= AUDIO_MAX_DEVICES) {
        return;
    }
    
    printf("[AUDIO] Unregistering audio device %d: %s\n", device->device_id, device->name);
    
    audio_system.devices[device->device_id] = NULL;
    audio_system.device_count--;
}

/* Find Audio Device */
audio_device_t* audio_find_device(uint32_t device_id) {
    if (device_id >= AUDIO_MAX_DEVICES) {
        return NULL;
    }
    
    return audio_system.devices[device_id];
}

/* Get Default Audio Device */
audio_device_t* audio_get_default_device(uint32_t type) {
    for (int i = 0; i < AUDIO_MAX_DEVICES; i++) {
        audio_device_t* device = audio_system.devices[i];
        if (device && device->enabled && (device->type & type)) {
            return device;
        }
    }
    
    return NULL;
}

/* Get Device Count */
uint32_t audio_get_device_count(void) {
    return audio_system.device_count;
}

/* Enumerate Devices */
int audio_enumerate_devices(audio_device_t** devices, uint32_t max_count) {
    if (!devices || max_count == 0) {
        return AUDIO_ERROR_INVALID;
    }
    
    uint32_t count = 0;
    for (int i = 0; i < AUDIO_MAX_DEVICES && count < max_count; i++) {
        if (audio_system.devices[i]) {
            devices[count++] = audio_system.devices[i];
        }
    }
    
    return count;
}

/* Open Audio Stream */
int audio_stream_open(uint32_t device_id, uint32_t direction, audio_format_t* format, audio_stream_t** stream) {
    if (!format || !stream) {
        return AUDIO_ERROR_INVALID;
    }
    
    audio_device_t* device = audio_find_device(device_id);
    if (!device || !device->enabled) {
        return AUDIO_ERROR_NO_DEVICE;
    }
    
    /* Validate format */
    int result = audio_validate_format(format);
    if (result != AUDIO_SUCCESS) {
        return result;
    }
    
    /* Find free stream slot */
    int stream_slot = -1;
    for (int i = 0; i < AUDIO_MAX_STREAMS; i++) {
        if (!device->streams[i]) {
            stream_slot = i;
            break;
        }
    }
    
    if (stream_slot == -1) {
        return AUDIO_ERROR_BUSY;
    }
    
    /* Allocate stream */
    audio_stream_t* new_stream = (audio_stream_t*)malloc(sizeof(audio_stream_t));
    if (!new_stream) {
        return AUDIO_ERROR_NO_MEMORY;
    }
    
    /* Initialize stream */
    memset(new_stream, 0, sizeof(audio_stream_t));
    new_stream->stream_id = audio_allocate_stream_id();
    new_stream->device_id = device_id;
    new_stream->direction = direction;
    new_stream->state = AUDIO_STREAM_IDLE;
    new_stream->format = *format;
    new_stream->device = device;
    
    /* Open stream with driver */
    if (device->driver && device->driver->stream_open) {
        result = device->driver->stream_open(new_stream);
        if (result != AUDIO_SUCCESS) {
            free(new_stream);
            return result;
        }
    }
    
    /* Add to device */
    device->streams[stream_slot] = new_stream;
    device->stream_count++;
    
    new_stream->state = AUDIO_STREAM_PREPARED;
    *stream = new_stream;
    
    printf("[AUDIO] Opened stream %d on device %d\n", new_stream->stream_id, device_id);
    return AUDIO_SUCCESS;
}

/* Close Audio Stream */
int audio_stream_close(audio_stream_t* stream) {
    if (!stream) {
        return AUDIO_ERROR_INVALID;
    }
    
    printf("[AUDIO] Closing stream %d\n", stream->stream_id);
    
    /* Stop stream if running */
    if (stream->state == AUDIO_STREAM_RUNNING) {
        audio_stream_stop(stream);
    }
    
    /* Close with driver */
    audio_device_t* device = stream->device;
    if (device && device->driver && device->driver->stream_close) {
        device->driver->stream_close(stream);
    }
    
    /* Remove from device */
    if (device) {
        for (int i = 0; i < AUDIO_MAX_STREAMS; i++) {
            if (device->streams[i] == stream) {
                device->streams[i] = NULL;
                device->stream_count--;
                break;
            }
        }
    }
    
    /* Free buffers */
    audio_buffer_t* buffer = stream->buffers;
    while (buffer) {
        audio_buffer_t* next = buffer->next;
        audio_buffer_free(buffer);
        buffer = next;
    }
    
    free(stream);
    return AUDIO_SUCCESS;
}

/* Start Audio Stream */
int audio_stream_start(audio_stream_t* stream) {
    if (!stream) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (stream->state != AUDIO_STREAM_PREPARED && stream->state != AUDIO_STREAM_PAUSED) {
        return AUDIO_ERROR_NOT_READY;
    }
    
    printf("[AUDIO] Starting stream %d\n", stream->stream_id);
    
    /* Start with driver */
    audio_device_t* device = stream->device;
    if (device && device->driver && device->driver->stream_start) {
        int result = device->driver->stream_start(stream);
        if (result != AUDIO_SUCCESS) {
            return result;
        }
    }
    
    stream->state = AUDIO_STREAM_RUNNING;
    return AUDIO_SUCCESS;
}

/* Stop Audio Stream */
int audio_stream_stop(audio_stream_t* stream) {
    if (!stream) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (stream->state != AUDIO_STREAM_RUNNING && stream->state != AUDIO_STREAM_PAUSED) {
        return AUDIO_SUCCESS;
    }
    
    printf("[AUDIO] Stopping stream %d\n", stream->stream_id);
    
    /* Stop with driver */
    audio_device_t* device = stream->device;
    if (device && device->driver && device->driver->stream_stop) {
        device->driver->stream_stop(stream);
    }
    
    stream->state = AUDIO_STREAM_STOPPED;
    return AUDIO_SUCCESS;
}

/* Pause Audio Stream */
int audio_stream_pause(audio_stream_t* stream) {
    if (!stream || stream->state != AUDIO_STREAM_RUNNING) {
        return AUDIO_ERROR_INVALID;
    }
    
    printf("[AUDIO] Pausing stream %d\n", stream->stream_id);
    
    /* Pause with driver */
    audio_device_t* device = stream->device;
    if (device && device->driver && device->driver->stream_pause) {
        device->driver->stream_pause(stream);
    }
    
    stream->state = AUDIO_STREAM_PAUSED;
    return AUDIO_SUCCESS;
}

/* Resume Audio Stream */
int audio_stream_resume(audio_stream_t* stream) {
    if (!stream || stream->state != AUDIO_STREAM_PAUSED) {
        return AUDIO_ERROR_INVALID;
    }
    
    printf("[AUDIO] Resuming stream %d\n", stream->stream_id);
    return audio_stream_start(stream);
}

/* Allocate Audio Buffer */
int audio_buffer_alloc(audio_stream_t* stream, uint32_t size, audio_buffer_t** buffer) {
    if (!stream || !buffer || size == 0) {
        return AUDIO_ERROR_INVALID;
    }
    
    audio_buffer_t* new_buffer = (audio_buffer_t*)malloc(sizeof(audio_buffer_t));
    if (!new_buffer) {
        return AUDIO_ERROR_NO_MEMORY;
    }
    
    new_buffer->data = malloc(size);
    if (!new_buffer->data) {
        free(new_buffer);
        return AUDIO_ERROR_NO_MEMORY;
    }
    
    new_buffer->size = size;
    new_buffer->used = 0;
    new_buffer->frames = size / audio_format_frame_size(&stream->format);
    new_buffer->timestamp = 0;
    new_buffer->flags = 0;
    new_buffer->next = NULL;
    
    *buffer = new_buffer;
    return AUDIO_SUCCESS;
}

/* Free Audio Buffer */
void audio_buffer_free(audio_buffer_t* buffer) {
    if (!buffer) {
        return;
    }
    
    if (buffer->data) {
        free(buffer->data);
    }
    free(buffer);
}

/* Queue Audio Buffer */
int audio_buffer_queue(audio_stream_t* stream, audio_buffer_t* buffer) {
    if (!stream || !buffer) {
        return AUDIO_ERROR_INVALID;
    }
    
    /* Add to buffer chain */
    if (!stream->buffers) {
        stream->buffers = buffer;
    } else {
        audio_buffer_t* current = stream->buffers;
        while (current->next) {
            current = current->next;
        }
        current->next = buffer;
    }
    
    stream->buffer_count++;
    
    /* Queue with driver */
    audio_device_t* device = stream->device;
    if (device && device->driver && device->driver->buffer_queue) {
        return device->driver->buffer_queue(stream, buffer);
    }
    
    return AUDIO_SUCCESS;
}

/* Dequeue Audio Buffer */
int audio_buffer_dequeue(audio_stream_t* stream, audio_buffer_t** buffer) {
    if (!stream || !buffer) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (!stream->buffers) {
        *buffer = NULL;
        return AUDIO_SUCCESS;
    }
    
    *buffer = stream->buffers;
    stream->buffers = stream->buffers->next;
    stream->buffer_count--;
    
    (*buffer)->next = NULL;
    return AUDIO_SUCCESS;
}

/* Play Audio Buffer */
int audio_play_buffer(uint32_t device_id, void* data, uint32_t size, audio_format_t* format) {
    if (!data || !format || size == 0) {
        return AUDIO_ERROR_INVALID;
    }
    
    audio_stream_t* stream;
    int result = audio_stream_open(device_id, AUDIO_DEVICE_PLAYBACK, format, &stream);
    if (result != AUDIO_SUCCESS) {
        return result;
    }
    
    audio_buffer_t* buffer;
    result = audio_buffer_alloc(stream, size, &buffer);
    if (result != AUDIO_SUCCESS) {
        audio_stream_close(stream);
        return result;
    }
    
    memcpy(buffer->data, data, size);
    buffer->used = size;
    
    result = audio_buffer_queue(stream, buffer);
    if (result != AUDIO_SUCCESS) {
        audio_buffer_free(buffer);
        audio_stream_close(stream);
        return result;
    }
    
    result = audio_stream_start(stream);
    if (result != AUDIO_SUCCESS) {
        audio_stream_close(stream);
        return result;
    }
    
    /* Wait for playback to complete (simplified) */
    /* In a real implementation, this would be handled asynchronously */
    
    audio_stream_stop(stream);
    audio_stream_close(stream);
    
    return AUDIO_SUCCESS;
}

/* Play Tone */
int audio_play_tone(uint32_t device_id, uint32_t frequency, uint32_t duration) {
    audio_format_t format = {
        .sample_rate = 44100,
        .channels = 1,
        .format = AUDIO_FORMAT_PCM16,
        .frame_size = 2,
        .period_size = 1024,
        .buffer_size = 4096
    };
    
    uint32_t samples = (format.sample_rate * duration) / 1000;
    uint32_t buffer_size = samples * format.frame_size;
    
    int16_t* buffer = (int16_t*)malloc(buffer_size);
    if (!buffer) {
        return AUDIO_ERROR_NO_MEMORY;
    }
    
    /* Generate sine wave */
    for (uint32_t i = 0; i < samples; i++) {
        double t = (double)i / format.sample_rate;
        double sample = sin(2.0 * 3.14159 * frequency * t) * 16384;
        buffer[i] = (int16_t)sample;
    }
    
    int result = audio_play_buffer(device_id, buffer, buffer_size, &format);
    free(buffer);
    
    return result;
}

/* Format Frame Size */
uint32_t audio_format_frame_size(audio_format_t* format) {
    if (!format) {
        return 0;
    }
    
    uint32_t sample_size;
    switch (format->format) {
        case AUDIO_FORMAT_PCM8:
            sample_size = 1;
            break;
        case AUDIO_FORMAT_PCM16:
            sample_size = 2;
            break;
        case AUDIO_FORMAT_PCM24:
            sample_size = 3;
            break;
        case AUDIO_FORMAT_PCM32:
        case AUDIO_FORMAT_FLOAT32:
            sample_size = 4;
            break;
        default:
            return 0;
    }
    
    return sample_size * format->channels;
}

/* Utility Functions */
const char* audio_format_string(uint16_t format) {
    switch (format) {
        case AUDIO_FORMAT_PCM8: return "PCM 8-bit";
        case AUDIO_FORMAT_PCM16: return "PCM 16-bit";
        case AUDIO_FORMAT_PCM24: return "PCM 24-bit";
        case AUDIO_FORMAT_PCM32: return "PCM 32-bit";
        case AUDIO_FORMAT_FLOAT32: return "Float 32-bit";
        case AUDIO_FORMAT_COMPRESSED: return "Compressed";
        default: return "Unknown";
    }
}

const char* audio_device_type_string(uint32_t type) {
    switch (type) {
        case AUDIO_DEVICE_PLAYBACK: return "Playback";
        case AUDIO_DEVICE_CAPTURE: return "Capture";
        case AUDIO_DEVICE_DUPLEX: return "Duplex";
        default: return "Unknown";
    }
}

const char* audio_stream_state_string(uint32_t state) {
    switch (state) {
        case AUDIO_STREAM_IDLE: return "Idle";
        case AUDIO_STREAM_PREPARED: return "Prepared";
        case AUDIO_STREAM_RUNNING: return "Running";
        case AUDIO_STREAM_PAUSED: return "Paused";
        case AUDIO_STREAM_STOPPED: return "Stopped";
        case AUDIO_STREAM_ERROR: return "Error";
        default: return "Unknown";
    }
}

/* Internal Helper Functions */
static int audio_validate_format(audio_format_t* format) {
    if (!format) {
        return AUDIO_ERROR_INVALID;
    }
    
    if (format->sample_rate < AUDIO_MIN_SAMPLE_RATE || 
        format->sample_rate > AUDIO_MAX_SAMPLE_RATE) {
        return AUDIO_ERROR_FORMAT;
    }
    
    if (format->channels == 0 || format->channels > AUDIO_MAX_CHANNELS) {
        return AUDIO_ERROR_FORMAT;
    }
    
    if (format->format > AUDIO_FORMAT_COMPRESSED) {
        return AUDIO_ERROR_FORMAT;
    }
    
    return AUDIO_SUCCESS;
}

static uint32_t audio_allocate_stream_id(void) {
    return audio_system.next_stream_id++;
}

static int audio_setup_default_devices(void) {
    /* This would probe for actual hardware */
    /* For now, we'll create a dummy device */
    
    audio_device_t* device = (audio_device_t*)malloc(sizeof(audio_device_t));
    if (!device) {
        return AUDIO_ERROR_NO_MEMORY;
    }
    
    memset(device, 0, sizeof(audio_device_t));
    strcpy(device->name, "Default Audio Device");
    device->class = AUDIO_CLASS_INTEGRATED;
    device->type = AUDIO_DEVICE_DUPLEX;
    device->capabilities = AUDIO_DEVICE_PLAYBACK | AUDIO_DEVICE_CAPTURE;
    device->enabled = true;
    device->connected = true;
    
    /* Set up capabilities */
    device->playback_caps.formats = (1 << AUDIO_FORMAT_PCM16) | (1 << AUDIO_FORMAT_PCM8);
    device->playback_caps.min_rate = 8000;
    device->playback_caps.max_rate = 48000;
    device->playback_caps.min_channels = 1;
    device->playback_caps.max_channels = 2;
    
    device->capture_caps = device->playback_caps;
    
    return audio_register_device(device);
}
