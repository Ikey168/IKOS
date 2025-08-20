/* IKOS Audio System Test Program
 * 
 * Test program to demonstrate and validate the audio system implementation.
 * This program tests various audio operations including device enumeration,
 * stream management, volume control, and tone generation.
 */

#include "audio_user.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

/* Test Configuration */
#define TEST_SAMPLE_RATE    44100
#define TEST_CHANNELS       2
#define TEST_FORMAT         AUDIO_FORMAT_PCM_S16_LE
#define TEST_DURATION       3000  /* 3 seconds */
#define TEST_FREQUENCY      440   /* A4 note */

/* Function prototypes */
static void print_test_header(const char* test_name);
static void print_test_result(const char* test_name, bool passed);
static void print_device_info(const audio_device_info_t* info);
static int test_device_enumeration(void);
static int test_device_info(void);
static int test_volume_control(void);
static int test_stream_operations(void);
static int test_tone_generation(void);
static int test_audio_buffer(void);
static int generate_sine_wave(void* buffer, uint32_t frames, uint32_t sample_rate, 
                              uint32_t frequency, uint16_t channels);

int main(int argc, char* argv[]) {
    printf("=== IKOS Audio System Test Program ===\n");
    printf("Testing audio system functionality...\n\n");
    
    /* Initialize audio library */
    print_test_header("Audio Library Initialization");
    int result = audio_lib_init();
    if (result != AUDIO_SUCCESS) {
        printf("FAILED: %s\n", audio_error_string(result));
        return 1;
    }
    print_test_result("Audio Library Initialization", true);
    
    /* Run test suite */
    int tests_passed = 0;
    int total_tests = 6;
    
    if (test_device_enumeration() == AUDIO_SUCCESS) tests_passed++;
    if (test_device_info() == AUDIO_SUCCESS) tests_passed++;
    if (test_volume_control() == AUDIO_SUCCESS) tests_passed++;
    if (test_stream_operations() == AUDIO_SUCCESS) tests_passed++;
    if (test_tone_generation() == AUDIO_SUCCESS) tests_passed++;
    if (test_audio_buffer() == AUDIO_SUCCESS) tests_passed++;
    
    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);
    printf("Success rate: %.1f%%\n", (float)tests_passed / total_tests * 100.0f);
    
    if (tests_passed == total_tests) {
        printf("All tests PASSED! Audio system is working correctly.\n");
    } else {
        printf("Some tests FAILED. Please check the audio system implementation.\n");
    }
    
    /* Cleanup */
    audio_lib_cleanup();
    return (tests_passed == total_tests) ? 0 : 1;
}

static void print_test_header(const char* test_name) {
    printf("--- %s ---\n", test_name);
}

static void print_test_result(const char* test_name, bool passed) {
    printf("Result: %s %s\n\n", test_name, passed ? "PASSED" : "FAILED");
}

static void print_device_info(const audio_device_info_t* info) {
    printf("  Device ID: %d\n", info->device_id);
    printf("  Name: %s\n", info->name);
    printf("  Class: %d\n", info->class);
    printf("  Type: %d\n", info->type);
    printf("  Capabilities: 0x%08X\n", info->capabilities);
    printf("  Enabled: %s\n", info->enabled ? "Yes" : "No");
    printf("  Connected: %s\n", info->connected ? "Yes" : "No");
    
    if (info->capabilities & AUDIO_CAP_PLAYBACK) {
        printf("  Playback: %d-%d Hz, %d-%d channels, formats=0x%08X\n",
               info->playback_caps.min_rate, info->playback_caps.max_rate,
               info->playback_caps.min_channels, info->playback_caps.max_channels,
               info->playback_caps.formats);
    }
    
    if (info->capabilities & AUDIO_CAP_CAPTURE) {
        printf("  Capture: %d-%d Hz, %d-%d channels, formats=0x%08X\n",
               info->capture_caps.min_rate, info->capture_caps.max_rate,
               info->capture_caps.min_channels, info->capture_caps.max_channels,
               info->capture_caps.formats);
    }
    printf("\n");
}

static int test_device_enumeration(void) {
    print_test_header("Device Enumeration Test");
    
    int device_count = audio_get_device_count();
    printf("Found %d audio devices\n", device_count);
    
    bool passed = (device_count >= 0);
    print_test_result("Device Enumeration Test", passed);
    
    return passed ? AUDIO_SUCCESS : AUDIO_ERROR_NO_DEVICE;
}

static int test_device_info(void) {
    print_test_header("Device Information Test");
    
    int device_count = audio_get_device_count();
    if (device_count <= 0) {
        printf("No devices available for testing\n");
        print_test_result("Device Information Test", false);
        return AUDIO_ERROR_NO_DEVICE;
    }
    
    bool all_passed = true;
    
    for (int i = 0; i < device_count; i++) {
        printf("Device %d:\n", i);
        
        audio_device_info_t info;
        int result = audio_get_device_info(i, &info);
        
        if (result != AUDIO_SUCCESS) {
            printf("Failed to get device info: %s\n", audio_error_string(result));
            all_passed = false;
            continue;
        }
        
        print_device_info(&info);
    }
    
    print_test_result("Device Information Test", all_passed);
    return all_passed ? AUDIO_SUCCESS : AUDIO_ERROR_INVALID;
}

static int test_volume_control(void) {
    print_test_header("Volume Control Test");
    
    int device_count = audio_get_device_count();
    if (device_count <= 0) {
        printf("No devices available for volume testing\n");
        print_test_result("Volume Control Test", false);
        return AUDIO_ERROR_NO_DEVICE;
    }
    
    uint32_t device_id = 0; /* Test with first device */
    bool passed = true;
    
    /* Test volume setting */
    printf("Setting volume to 75%%...\n");
    int result = audio_set_volume(device_id, 75);
    if (result != AUDIO_SUCCESS) {
        printf("Failed to set volume: %s\n", audio_error_string(result));
        passed = false;
    }
    
    /* Test volume getting */
    printf("Getting current volume...\n");
    int volume = audio_get_volume(device_id);
    if (volume < 0) {
        printf("Failed to get volume: %s\n", audio_error_string(volume));
        passed = false;
    } else {
        printf("Current volume: %d%%\n", volume);
    }
    
    /* Test mute functionality */
    printf("Testing mute functionality...\n");
    result = audio_set_mute(device_id, true);
    if (result != AUDIO_SUCCESS) {
        printf("Failed to set mute: %s\n", audio_error_string(result));
        passed = false;
    }
    
    bool mute_state = audio_get_mute(device_id);
    printf("Mute state: %s\n", mute_state ? "Muted" : "Unmuted");
    
    /* Unmute */
    audio_set_mute(device_id, false);
    
    print_test_result("Volume Control Test", passed);
    return passed ? AUDIO_SUCCESS : AUDIO_ERROR_INVALID;
}

static int test_stream_operations(void) {
    print_test_header("Stream Operations Test");
    
    int device_count = audio_get_device_count();
    if (device_count <= 0) {
        printf("No devices available for stream testing\n");
        print_test_result("Stream Operations Test", false);
        return AUDIO_ERROR_NO_DEVICE;
    }
    
    uint32_t device_id = 0;
    bool passed = true;
    
    /* Set up audio format */
    audio_format_t format;
    format.sample_rate = TEST_SAMPLE_RATE;
    format.channels = TEST_CHANNELS;
    format.format = TEST_FORMAT;
    format.frame_size = audio_calculate_frame_size(format.channels, format.format);
    format.period_size = 1024;
    format.buffer_size = 4096;
    
    printf("Opening audio stream...\n");
    printf("  Sample rate: %d Hz\n", format.sample_rate);
    printf("  Channels: %d\n", format.channels);
    printf("  Format: %d\n", format.format);
    printf("  Frame size: %d bytes\n", format.frame_size);
    
    /* Open stream */
    audio_stream_t* stream;
    int result = audio_stream_open(device_id, AUDIO_DIRECTION_PLAYBACK, &format, &stream);
    if (result != AUDIO_SUCCESS) {
        printf("Failed to open stream: %s\n", audio_error_string(result));
        print_test_result("Stream Operations Test", false);
        return result;
    }
    printf("Stream opened successfully (ID: %d)\n", stream->stream_id);
    
    /* Start stream */
    printf("Starting stream...\n");
    result = audio_stream_start(stream);
    if (result != AUDIO_SUCCESS) {
        printf("Failed to start stream: %s\n", audio_error_string(result));
        passed = false;
    } else {
        printf("Stream started successfully\n");
    }
    
    /* Generate and play some audio */
    if (passed) {
        printf("Playing test audio...\n");
        
        uint32_t frames_per_buffer = 1024;
        uint32_t buffer_size = frames_per_buffer * format.frame_size;
        void* audio_buffer = malloc(buffer_size);
        
        if (audio_buffer) {
            /* Generate sine wave */
            generate_sine_wave(audio_buffer, frames_per_buffer, format.sample_rate, 
                               TEST_FREQUENCY, format.channels);
            
            /* Write audio data */
            result = audio_stream_write(stream, audio_buffer, buffer_size);
            if (result < 0) {
                printf("Failed to write audio data: %s\n", audio_error_string(result));
                passed = false;
            } else {
                printf("Wrote %d bytes of audio data\n", result);
            }
            
            free(audio_buffer);
        }
    }
    
    /* Stop stream */
    printf("Stopping stream...\n");
    result = audio_stream_stop(stream);
    if (result != AUDIO_SUCCESS) {
        printf("Failed to stop stream: %s\n", audio_error_string(result));
        passed = false;
    } else {
        printf("Stream stopped successfully\n");
    }
    
    /* Close stream */
    printf("Closing stream...\n");
    result = audio_stream_close(stream);
    if (result != AUDIO_SUCCESS) {
        printf("Failed to close stream: %s\n", audio_error_string(result));
        passed = false;
    } else {
        printf("Stream closed successfully\n");
    }
    
    print_test_result("Stream Operations Test", passed);
    return passed ? AUDIO_SUCCESS : AUDIO_ERROR_INVALID;
}

static int test_tone_generation(void) {
    print_test_header("Tone Generation Test");
    
    int device_count = audio_get_device_count();
    if (device_count <= 0) {
        printf("No devices available for tone testing\n");
        print_test_result("Tone Generation Test", false);
        return AUDIO_ERROR_NO_DEVICE;
    }
    
    uint32_t device_id = 0;
    bool passed = true;
    
    /* Test different frequencies */
    uint32_t frequencies[] = {220, 440, 880, 1760}; /* A notes */
    uint32_t num_frequencies = sizeof(frequencies) / sizeof(frequencies[0]);
    
    for (uint32_t i = 0; i < num_frequencies; i++) {
        printf("Playing %d Hz tone for 500ms...\n", frequencies[i]);
        
        int result = audio_play_tone(device_id, frequencies[i], 500);
        if (result != AUDIO_SUCCESS) {
            printf("Failed to play tone: %s\n", audio_error_string(result));
            passed = false;
        }
        
        /* Small delay between tones */
        usleep(200000); /* 200ms */
    }
    
    print_test_result("Tone Generation Test", passed);
    return passed ? AUDIO_SUCCESS : AUDIO_ERROR_INVALID;
}

static int test_audio_buffer(void) {
    print_test_header("Audio Buffer Test");
    
    bool passed = true;
    
    /* Create buffer */
    printf("Creating audio buffer (4096 bytes)...\n");
    audio_buffer_t* buffer = audio_buffer_create(4096);
    if (!buffer) {
        printf("Failed to create audio buffer\n");
        print_test_result("Audio Buffer Test", false);
        return AUDIO_ERROR_NO_MEMORY;
    }
    
    /* Test buffer operations */
    char test_data[1024];
    char read_data[1024];
    
    /* Fill test data */
    for (int i = 0; i < 1024; i++) {
        test_data[i] = (char)(i & 0xFF);
    }
    
    /* Write data */
    printf("Writing 1024 bytes to buffer...\n");
    int result = audio_buffer_write(buffer, test_data, 1024);
    if (result != 1024) {
        printf("Failed to write to buffer: %d\n", result);
        passed = false;
    }
    
    printf("Buffer used: %d bytes, available: %d bytes\n", 
           audio_buffer_used(buffer), audio_buffer_available(buffer));
    
    /* Read data */
    printf("Reading 512 bytes from buffer...\n");
    result = audio_buffer_read(buffer, read_data, 512);
    if (result != 512) {
        printf("Failed to read from buffer: %d\n", result);
        passed = false;
    }
    
    /* Verify data */
    if (memcmp(test_data, read_data, 512) != 0) {
        printf("Buffer data verification failed\n");
        passed = false;
    } else {
        printf("Buffer data verified successfully\n");
    }
    
    printf("Buffer used: %d bytes, available: %d bytes\n", 
           audio_buffer_used(buffer), audio_buffer_available(buffer));
    
    /* Reset buffer */
    printf("Resetting buffer...\n");
    audio_buffer_reset(buffer);
    
    if (audio_buffer_used(buffer) != 0) {
        printf("Buffer reset failed\n");
        passed = false;
    } else {
        printf("Buffer reset successfully\n");
    }
    
    /* Cleanup */
    audio_buffer_destroy(buffer);
    
    print_test_result("Audio Buffer Test", passed);
    return passed ? AUDIO_SUCCESS : AUDIO_ERROR_INVALID;
}

static int generate_sine_wave(void* buffer, uint32_t frames, uint32_t sample_rate, 
                              uint32_t frequency, uint16_t channels) {
    if (!buffer || frames == 0 || sample_rate == 0 || frequency == 0) {
        return AUDIO_ERROR_INVALID;
    }
    
    int16_t* samples = (int16_t*)buffer;
    double phase_increment = 2.0 * M_PI * frequency / sample_rate;
    
    for (uint32_t frame = 0; frame < frames; frame++) {
        double phase = frame * phase_increment;
        int16_t sample = (int16_t)(sin(phase) * 16383); /* 50% volume */
        
        for (uint16_t ch = 0; ch < channels; ch++) {
            samples[frame * channels + ch] = sample;
        }
    }
    
    return AUDIO_SUCCESS;
}
