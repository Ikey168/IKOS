/* IKOS Logging & Debugging Service - Core Implementation
 * Provides comprehensive logging infrastructure for kernel and user-space
 */

#include "../include/logging_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

/* ========================== Global State ========================== */

static logger_context_t g_logger_ctx = {0};
static atomic_uint g_sequence_counter = 0;
static bool g_logger_initialized = false;

/* Thread-local storage for context */
static __thread log_context_t tls_context = {0};
static __thread bool tls_context_valid = false;

/* Default configuration */
static const logger_config_t default_config = {
    .global_level = LOG_LEVEL_INFO,
    .default_outputs = LOG_OUTPUT_CONSOLE,
    .enable_timestamps = true,
    .enable_context = true,
    .enable_location = false,
    .enable_colors = true,
    .buffer_size = LOG_DEFAULT_BUFFER_SIZE,
    .max_buffers = LOG_MAX_BUFFERS,
    .async_logging = true,
    .flush_interval = 1000,
    .lazy_formatting = true,
    .batch_processing = true,
    .batch_size = 10,
    .rate_limit = 1000,
    .enable_symbols = true,
    .enable_stacktrace = false,
    .max_stack_depth = 16,
    .kernel_symbols = true,
    .filter_sensitive = true,
    .audit_logging = false,
    .max_message_size = LOG_MAX_MESSAGE_SIZE,
    .log_directory = "/var/log"
};

/* ========================== Utility Functions ========================== */

static uint32_t calculate_checksum(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum = (checksum << 1) ^ bytes[i];
    }
    return checksum;
}

static inline uint64_t get_high_res_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static void init_tls_context(void) {
    if (!tls_context_valid) {
        tls_context.process_id = getpid();
        tls_context.thread_id = pthread_self();
        tls_context.user_id = getuid();
        tls_context.group_id = getgid();
        
        /* Get process name - simplified for this implementation */
        snprintf(tls_context.process_name, sizeof(tls_context.process_name), "proc_%d", tls_context.process_id);
        snprintf(tls_context.thread_name, sizeof(tls_context.thread_name), "thread_%d", tls_context.thread_id);
        
        tls_context_valid = true;
    }
}

/* ========================== Core Logging Functions ========================== */

void log_get_timestamp(log_timestamp_t* timestamp) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    timestamp->seconds = ts.tv_sec;
    timestamp->nanoseconds = ts.tv_nsec;
    timestamp->cpu_id = sched_getcpu(); /* Simplified - may need platform-specific code */
}

void log_get_context(log_context_t* context) {
    init_tls_context();
    *context = tls_context;
}

const char* log_level_to_string(log_level_t level) {
    static const char* level_strings[] = {
        "DEBUG", "INFO", "NOTICE", "WARN", "ERROR", "CRIT", "ALERT", "EMERG"
    };
    
    if (level >= 0 && level < LOG_LEVEL_MAX) {
        return level_strings[level];
    }
    return "UNKNOWN";
}

log_level_t log_level_from_string(const char* level_str) {
    if (!level_str) return LOG_LEVEL_INFO;
    
    if (strcasecmp(level_str, "DEBUG") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(level_str, "INFO") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(level_str, "NOTICE") == 0) return LOG_LEVEL_NOTICE;
    if (strcasecmp(level_str, "WARN") == 0 || strcasecmp(level_str, "WARNING") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(level_str, "ERROR") == 0 || strcasecmp(level_str, "ERR") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(level_str, "CRIT") == 0 || strcasecmp(level_str, "CRITICAL") == 0) return LOG_LEVEL_CRIT;
    if (strcasecmp(level_str, "ALERT") == 0) return LOG_LEVEL_ALERT;
    if (strcasecmp(level_str, "EMERG") == 0 || strcasecmp(level_str, "EMERGENCY") == 0) return LOG_LEVEL_EMERG;
    
    return LOG_LEVEL_INFO; /* Default */
}

const char* log_facility_to_string(log_facility_t facility) {
    static const char* facility_strings[] = {
        "KERNEL", "USER", "MAIL", "DAEMON", "AUTH", "SYSLOG", "LPR", "NEWS",
        "UUCP", "CRON", "AUTHPRIV", "FTP", "12", "13", "14", "15",
        "LOCAL0", "LOCAL1", "LOCAL2", "LOCAL3", "LOCAL4", "LOCAL5", "LOCAL6", "LOCAL7"
    };
    
    if (facility >= 0 && facility < 24) {
        return facility_strings[facility];
    }
    return "UNKNOWN";
}

/* ========================== Message Creation and Formatting ========================== */

static log_message_t* create_log_message(log_level_t level, log_facility_t facility,
                                        log_flags_t flags, const log_location_t* location,
                                        const char* format, va_list args) {
    /* Calculate required buffer size */
    va_list args_copy;
    va_copy(args_copy, args);
    int format_len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (format_len < 0) {
        return NULL;
    }
    
    size_t format_size = format_len + 1;
    size_t total_size = sizeof(log_message_t) + format_size;
    
    /* Allocate message */
    log_message_t* msg = (log_message_t*)malloc(total_size);
    if (!msg) {
        return NULL;
    }
    
    /* Initialize message header */
    memset(msg, 0, sizeof(log_message_t));
    msg->magic = LOG_MESSAGE_MAGIC;
    msg->size = total_size;
    msg->sequence = atomic_fetch_add(&g_sequence_counter, 1);
    
    /* Set metadata */
    log_get_timestamp(&msg->timestamp);
    log_get_context(&msg->context);
    
    if (location && (flags & LOG_FLAG_LOCATION)) {
        msg->location = *location;
    }
    
    /* Set message properties */
    msg->level = level;
    msg->facility = facility;
    msg->flags = flags;
    msg->format_length = format_size;
    msg->data_length = format_size;
    
    /* Format message */
    vsnprintf(msg->data, format_size, format, args);
    
    /* Calculate checksum */
    msg->checksum = calculate_checksum(msg, total_size);
    
    return msg;
}

int log_format_message(const log_message_t* message, char* buffer, size_t size) {
    if (!message || !buffer || size == 0) {
        return LOG_ERROR_INVALID;
    }
    
    char timestamp_str[64];
    char level_str[16];
    char facility_str[16];
    char context_str[128];
    char location_str[256];
    
    /* Format timestamp */
    struct tm* tm_info = localtime((time_t*)&message->timestamp.seconds);
    snprintf(timestamp_str, sizeof(timestamp_str), "%04d-%02d-%02d %02d:%02d:%02d.%06u",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             message->timestamp.nanoseconds / 1000);
    
    /* Format level and facility */
    snprintf(level_str, sizeof(level_str), "%s", log_level_to_string(message->level));
    snprintf(facility_str, sizeof(facility_str), "%s", log_facility_to_string(message->facility));
    
    /* Format context */
    if (g_logger_ctx.config.enable_context) {
        snprintf(context_str, sizeof(context_str), "[%s:%d:%d]",
                message->context.process_name, message->context.process_id, message->context.thread_id);
    } else {
        context_str[0] = '\0';
    }
    
    /* Format location */
    if ((message->flags & LOG_FLAG_LOCATION) && message->location.file) {
        snprintf(location_str, sizeof(location_str), " at %s:%s:%d",
                message->location.file, message->location.function, message->location.line);
    } else {
        location_str[0] = '\0';
    }
    
    /* Build final message */
    int ret = snprintf(buffer, size, "%s %s.%s%s: %s%s\n",
                      g_logger_ctx.config.enable_timestamps ? timestamp_str : "",
                      facility_str, level_str, context_str,
                      message->data, location_str);
    
    return (ret >= 0 && ret < size) ? LOG_SUCCESS : LOG_ERROR_TRUNCATED;
}

/* ========================== Buffer Management ========================== */

log_buffer_t* log_buffer_create(const char* name, const log_buffer_config_t* config) {
    if (!name || !config) {
        return NULL;
    }
    
    log_buffer_t* buffer = (log_buffer_t*)malloc(sizeof(log_buffer_t));
    if (!buffer) {
        return NULL;
    }
    
    memset(buffer, 0, sizeof(log_buffer_t));
    
    /* Copy configuration */
    buffer->config = *config;
    
    /* Allocate buffer memory */
    buffer->data = (char*)malloc(config->size);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }
    
    /* Initialize buffer state */
    buffer->head = 0;
    buffer->tail = 0;
    buffer->used = 0;
    
    /* Create synchronization objects - simplified for this implementation */
    buffer->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init((pthread_mutex_t*)buffer->mutex, NULL);
    
    /* Set buffer properties */
    strncpy(buffer->name, name, sizeof(buffer->name) - 1);
    buffer->name[sizeof(buffer->name) - 1] = '\0';
    buffer->active = true;
    
    return buffer;
}

void log_buffer_destroy(log_buffer_t* buffer) {
    if (!buffer) {
        return;
    }
    
    buffer->active = false;
    
    if (buffer->mutex) {
        pthread_mutex_destroy((pthread_mutex_t*)buffer->mutex);
        free(buffer->mutex);
    }
    
    free(buffer->data);
    free(buffer);
}

int log_buffer_write(log_buffer_t* buffer, const log_message_t* message) {
    if (!buffer || !message || !buffer->active) {
        return LOG_ERROR_INVALID;
    }
    
    size_t msg_size = message->size;
    if (msg_size > buffer->config.max_message) {
        return LOG_ERROR_INVALID;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)buffer->mutex);
    
    /* Check if buffer has space */
    if (buffer->used + msg_size > buffer->config.size) {
        if (!buffer->config.overwrite) {
            pthread_mutex_unlock((pthread_mutex_t*)buffer->mutex);
            buffer->stats.messages_dropped++;
            return LOG_ERROR_FULL;
        }
        
        /* Advance tail to make space */
        while (buffer->used + msg_size > buffer->config.size) {
            /* Find next message boundary */
            log_message_t* old_msg = (log_message_t*)(buffer->data + buffer->tail);
            if (old_msg->magic != LOG_MESSAGE_MAGIC) {
                /* Corrupted buffer - reset */
                buffer->head = buffer->tail = buffer->used = 0;
                break;
            }
            
            buffer->tail = (buffer->tail + old_msg->size) % buffer->config.size;
            buffer->used -= old_msg->size;
            buffer->stats.messages_dropped++;
        }
    }
    
    /* Write message to buffer */
    size_t write_pos = buffer->head;
    size_t remaining = buffer->config.size - write_pos;
    
    if (msg_size <= remaining) {
        /* Single write */
        memcpy(buffer->data + write_pos, message, msg_size);
    } else {
        /* Wrap-around write */
        memcpy(buffer->data + write_pos, message, remaining);
        memcpy(buffer->data, (char*)message + remaining, msg_size - remaining);
    }
    
    /* Update buffer state */
    buffer->head = (buffer->head + msg_size) % buffer->config.size;
    buffer->used += msg_size;
    
    /* Update statistics */
    buffer->stats.messages_written++;
    buffer->stats.bytes_written += msg_size;
    
    if (buffer->used > buffer->stats.peak_size) {
        buffer->stats.peak_size = buffer->used;
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)buffer->mutex);
    
    return LOG_SUCCESS;
}

int log_buffer_read(log_buffer_t* buffer, log_message_t* message, size_t max_size) {
    if (!buffer || !message || max_size == 0) {
        return LOG_ERROR_INVALID;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)buffer->mutex);
    
    if (buffer->used == 0) {
        pthread_mutex_unlock((pthread_mutex_t*)buffer->mutex);
        return LOG_ERROR_EMPTY;
    }
    
    /* Get message at tail */
    size_t read_pos = buffer->tail;
    log_message_t* stored_msg = (log_message_t*)(buffer->data + read_pos);
    
    /* Validate message */
    if (stored_msg->magic != LOG_MESSAGE_MAGIC) {
        pthread_mutex_unlock((pthread_mutex_t*)buffer->mutex);
        return LOG_ERROR_CHECKSUM;
    }
    
    size_t msg_size = stored_msg->size;
    if (msg_size > max_size) {
        pthread_mutex_unlock((pthread_mutex_t*)buffer->mutex);
        return LOG_ERROR_TRUNCATED;
    }
    
    /* Read message from buffer */
    size_t remaining = buffer->config.size - read_pos;
    
    if (msg_size <= remaining) {
        /* Single read */
        memcpy(message, buffer->data + read_pos, msg_size);
    } else {
        /* Wrap-around read */
        memcpy(message, buffer->data + read_pos, remaining);
        memcpy((char*)message + remaining, buffer->data, msg_size - remaining);
    }
    
    /* Update buffer state */
    buffer->tail = (buffer->tail + msg_size) % buffer->config.size;
    buffer->used -= msg_size;
    
    /* Update statistics */
    buffer->stats.messages_read++;
    buffer->stats.bytes_read += msg_size;
    
    pthread_mutex_unlock((pthread_mutex_t*)buffer->mutex);
    
    return LOG_SUCCESS;
}

void log_buffer_clear(log_buffer_t* buffer) {
    if (!buffer) {
        return;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)buffer->mutex);
    
    buffer->head = 0;
    buffer->tail = 0;
    buffer->used = 0;
    
    pthread_mutex_unlock((pthread_mutex_t*)buffer->mutex);
}

/* ========================== Output Management ========================== */

static int console_output_handler(const log_message_t* message, void* context) {
    (void)context; /* Unused */
    
    char buffer[2048];
    int ret = log_format_message(message, buffer, sizeof(buffer));
    if (ret == LOG_SUCCESS || ret == LOG_ERROR_TRUNCATED) {
        printf("%s", buffer);
        fflush(stdout);
        return LOG_SUCCESS;
    }
    
    return ret;
}

static int file_output_handler(const log_message_t* message, void* context) {
    FILE* file = (FILE*)context;
    if (!file) {
        return LOG_ERROR_INVALID;
    }
    
    char buffer[2048];
    int ret = log_format_message(message, buffer, sizeof(buffer));
    if (ret == LOG_SUCCESS || ret == LOG_ERROR_TRUNCATED) {
        fprintf(file, "%s", buffer);
        fflush(file);
        return LOG_SUCCESS;
    }
    
    return ret;
}

log_output_t* log_output_create(const log_output_config_t* config) {
    if (!config) {
        return NULL;
    }
    
    log_output_t* output = (log_output_t*)malloc(sizeof(log_output_t));
    if (!output) {
        return NULL;
    }
    
    memset(output, 0, sizeof(log_output_t));
    output->config = *config;
    
    /* Set default handler based on type */
    switch (config->type) {
        case LOG_OUTPUT_CONSOLE:
            output->handler = console_output_handler;
            output->context = NULL;
            break;
            
        case LOG_OUTPUT_FILE:
            /* File handling will be implemented in output management functions */
            output->handler = file_output_handler;
            break;
            
        default:
            output->handler = console_output_handler;
            break;
    }
    
    output->active = true;
    
    return output;
}

void log_output_destroy(log_output_t* output) {
    if (!output) {
        return;
    }
    
    output->active = false;
    
    /* Close file if it's a file output */
    if (output->config.type == LOG_OUTPUT_FILE && output->context) {
        fclose((FILE*)output->context);
    }
    
    free(output);
}

int log_output_write(log_output_t* output, const log_message_t* message) {
    if (!output || !message || !output->active) {
        return LOG_ERROR_INVALID;
    }
    
    /* Check level filter */
    if (message->level < output->config.min_level || message->level > output->config.max_level) {
        return LOG_SUCCESS; /* Filtered out */
    }
    
    /* Call handler */
    int ret = output->handler(message, output->context);
    if (ret == LOG_SUCCESS) {
        output->messages_sent++;
        output->bytes_sent += message->size;
    } else {
        output->errors++;
    }
    
    return ret;
}

/* ========================== Core API Implementation ========================== */

void logger_default_config(logger_config_t* config) {
    if (config) {
        *config = default_config;
    }
}

int logger_init(const logger_config_t* config) {
    if (g_logger_initialized) {
        return LOG_SUCCESS;
    }
    
    memset(&g_logger_ctx, 0, sizeof(g_logger_ctx));
    
    /* Use provided config or default */
    if (config) {
        g_logger_ctx.config = *config;
    } else {
        g_logger_ctx.config = default_config;
    }
    
    /* Initialize global mutex */
    g_logger_ctx.global_mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_logger_ctx.global_mutex) {
        return LOG_ERROR_MEMORY;
    }
    pthread_mutex_init((pthread_mutex_t*)g_logger_ctx.global_mutex, NULL);
    
    /* Create default console output */
    log_output_config_t console_config = {
        .type = LOG_OUTPUT_CONSOLE,
        .min_level = g_logger_ctx.config.global_level,
        .max_level = LOG_LEVEL_EMERG,
        .enabled = true,
        .async = false,
        .buffer_size = 0
    };
    strcpy(console_config.name, "console");
    
    g_logger_ctx.outputs = log_output_create(&console_config);
    if (g_logger_ctx.outputs) {
        g_logger_ctx.output_count = 1;
    }
    
    g_logger_ctx.initialized = true;
    g_logger_initialized = true;
    
    return LOG_SUCCESS;
}

void logger_shutdown(void) {
    if (!g_logger_initialized) {
        return;
    }
    
    g_logger_ctx.shutdown = true;
    
    /* Destroy outputs */
    log_output_t* output = g_logger_ctx.outputs;
    while (output) {
        log_output_t* next = output->next;
        log_output_destroy(output);
        output = next;
    }
    
    /* Destroy buffers */
    for (uint32_t i = 0; i < g_logger_ctx.buffer_count; i++) {
        if (g_logger_ctx.buffers[i]) {
            log_buffer_destroy(g_logger_ctx.buffers[i]);
        }
    }
    
    free(g_logger_ctx.buffers);
    
    /* Destroy global mutex */
    if (g_logger_ctx.global_mutex) {
        pthread_mutex_destroy((pthread_mutex_t*)g_logger_ctx.global_mutex);
        free(g_logger_ctx.global_mutex);
    }
    
    memset(&g_logger_ctx, 0, sizeof(g_logger_ctx));
    g_logger_initialized = false;
}

bool logger_is_initialized(void) {
    return g_logger_initialized;
}

int log_message(log_level_t level, log_facility_t facility, const char* format, ...) {
    if (!g_logger_initialized || level < g_logger_ctx.config.global_level) {
        return LOG_SUCCESS; /* Filtered out */
    }
    
    va_list args;
    va_start(args, format);
    int ret = log_message_args(level, facility, format, args);
    va_end(args);
    
    return ret;
}

int log_message_args(log_level_t level, log_facility_t facility, const char* format, va_list args) {
    if (!g_logger_initialized || !format) {
        return LOG_ERROR_INVALID;
    }
    
    /* Create message */
    log_message_t* message = create_log_message(level, facility, LOG_FLAG_NONE, NULL, format, args);
    if (!message) {
        return LOG_ERROR_MEMORY;
    }
    
    /* Send to outputs */
    log_output_t* output = g_logger_ctx.outputs;
    while (output) {
        log_output_write(output, message);
        output = output->next;
    }
    
    /* Update statistics */
    g_logger_ctx.stats.total_messages++;
    if (level < LOG_LEVEL_MAX) {
        g_logger_ctx.stats.messages_by_level[level]++;
    }
    g_logger_ctx.stats.bytes_logged += message->size;
    
    free(message);
    
    return LOG_SUCCESS;
}

int log_message_ext(log_level_t level, log_facility_t facility, log_flags_t flags,
                   const log_location_t* location, const char* format, ...) {
    if (!g_logger_initialized || level < g_logger_ctx.config.global_level) {
        return LOG_SUCCESS; /* Filtered out */
    }
    
    va_list args;
    va_start(args, format);
    
    /* Create message */
    log_message_t* message = create_log_message(level, facility, flags, location, format, args);
    va_end(args);
    
    if (!message) {
        return LOG_ERROR_MEMORY;
    }
    
    /* Send to outputs */
    log_output_t* output = g_logger_ctx.outputs;
    while (output) {
        log_output_write(output, message);
        output = output->next;
    }
    
    /* Update statistics */
    g_logger_ctx.stats.total_messages++;
    if (level < LOG_LEVEL_MAX) {
        g_logger_ctx.stats.messages_by_level[level]++;
    }
    g_logger_ctx.stats.bytes_logged += message->size;
    
    free(message);
    
    return LOG_SUCCESS;
}

/* Kernel logging functions */
int klog_message(log_level_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = log_message_args(level, LOG_FACILITY_KERNEL, format, args);
    va_end(args);
    
    return ret;
}

int klog_message_ext(log_level_t level, log_flags_t flags, const log_location_t* location, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    log_message_t* message = create_log_message(level, LOG_FACILITY_KERNEL, flags | LOG_FLAG_KERNEL, location, format, args);
    va_end(args);
    
    if (!message) {
        return LOG_ERROR_MEMORY;
    }
    
    /* Send to outputs */
    log_output_t* output = g_logger_ctx.outputs;
    while (output) {
        log_output_write(output, message);
        output = output->next;
    }
    
    /* Update statistics */
    g_logger_ctx.stats.total_messages++;
    if (level < LOG_LEVEL_MAX) {
        g_logger_ctx.stats.messages_by_level[level]++;
    }
    g_logger_ctx.stats.bytes_logged += message->size;
    
    free(message);
    
    return LOG_SUCCESS;
}

int logger_get_stats(logger_stats_t* stats) {
    if (!stats || !g_logger_initialized) {
        return LOG_ERROR_INVALID;
    }
    
    *stats = g_logger_ctx.stats;
    
    /* Calculate derived statistics */
    if (stats->total_messages > 0) {
        stats->avg_message_size = (double)stats->bytes_logged / stats->total_messages;
    }
    
    return LOG_SUCCESS;
}

void logger_reset_stats(void) {
    if (g_logger_initialized) {
        memset(&g_logger_ctx.stats, 0, sizeof(g_logger_ctx.stats));
    }
}
