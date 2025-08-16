/* IKOS Kernel Debugging & Logging System - Issue #16
 * 
 * Comprehensive kernel logging interface with serial port output and debugging support.
 * Provides structured logging for tracking kernel execution and diagnosing issues.
 */

#ifndef KERNEL_LOG_H
#define KERNEL_LOG_H

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

/* ================================
 * Log Level Definitions
 * ================================ */

typedef enum {
    LOG_LEVEL_PANIC = 0,    /* System is unusable */
    LOG_LEVEL_ERROR = 1,    /* Error conditions */
    LOG_LEVEL_WARN  = 2,    /* Warning conditions */
    LOG_LEVEL_INFO  = 3,    /* Informational messages */
    LOG_LEVEL_DEBUG = 4,    /* Debug-level messages */
    LOG_LEVEL_TRACE = 5     /* Trace-level messages */
} log_level_t;

/* ================================
 * Log Categories
 * ================================ */

typedef enum {
    LOG_CAT_KERNEL   = 0,   /* General kernel messages */
    LOG_CAT_MEMORY   = 1,   /* Memory management */
    LOG_CAT_IPC      = 2,   /* Inter-process communication */
    LOG_CAT_DEVICE   = 3,   /* Device drivers */
    LOG_CAT_SCHEDULE = 4,   /* Scheduler */
    LOG_CAT_INTERRUPT = 5,  /* Interrupt handling */
    LOG_CAT_BOOT     = 6,   /* Boot process */
    LOG_CAT_PROCESS  = 7,   /* Process management */
    LOG_CAT_USB      = 8,   /* USB subsystem */
    LOG_CAT_MAX      = 9    /* Maximum category count */
} log_category_t;

/* ================================
 * Log Output Targets
 * ================================ */

typedef enum {
    LOG_OUTPUT_SERIAL  = 0x01,  /* Serial port output */
    LOG_OUTPUT_VGA     = 0x02,  /* VGA text mode */
    LOG_OUTPUT_BUFFER  = 0x04,  /* In-memory buffer */
    LOG_OUTPUT_ALL     = 0x07   /* All outputs */
} log_output_t;

/* ================================
 * Log Configuration
 * ================================ */

typedef struct {
    log_level_t global_level;           /* Global minimum log level */
    log_level_t category_levels[LOG_CAT_MAX]; /* Per-category levels */
    uint8_t output_targets;             /* Bitmask of output targets */
    bool timestamps_enabled;            /* Include timestamps */
    bool colors_enabled;                /* Use colors for levels */
    bool category_names_enabled;        /* Include category names */
    bool function_names_enabled;        /* Include function names */
    uint16_t serial_port;               /* Serial port base address */
    uint32_t buffer_size;               /* Log buffer size */
} log_config_t;

/* ================================
 * Log Entry Structure
 * ================================ */

typedef struct {
    uint64_t timestamp;     /* System timestamp */
    log_level_t level;      /* Log level */
    log_category_t category; /* Log category */
    const char* function;   /* Function name */
    uint16_t line;          /* Line number */
    char message[256];      /* Log message */
} log_entry_t;

/* ================================
 * Log Statistics
 * ================================ */

typedef struct {
    uint64_t total_messages;                    /* Total messages logged */
    uint64_t messages_by_level[6];              /* Messages per level */
    uint64_t messages_by_category[LOG_CAT_MAX]; /* Messages per category */
    uint64_t dropped_messages;                  /* Messages dropped (buffer full) */
    uint64_t serial_bytes_sent;                 /* Bytes sent via serial */
    uint64_t buffer_overruns;                   /* Buffer overrun count */
} log_stats_t;

/* ================================
 * Color Codes for Log Levels
 * ================================ */

#define LOG_COLOR_PANIC   "\033[31;1m"  /* Bright red */
#define LOG_COLOR_ERROR   "\033[31m"    /* Red */
#define LOG_COLOR_WARN    "\033[33m"    /* Yellow */
#define LOG_COLOR_INFO    "\033[32m"    /* Green */
#define LOG_COLOR_DEBUG   "\033[36m"    /* Cyan */
#define LOG_COLOR_TRACE   "\033[37m"    /* White */
#define LOG_COLOR_RESET   "\033[0m"     /* Reset */

/* ================================
 * Logging Macros
 * ================================ */

#define klog_panic(cat, fmt, ...) \
    klog_write(LOG_LEVEL_PANIC, cat, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define klog_error(cat, fmt, ...) \
    klog_write(LOG_LEVEL_ERROR, cat, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define klog_warn(cat, fmt, ...) \
    klog_write(LOG_LEVEL_WARN, cat, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define klog_info(cat, fmt, ...) \
    klog_write(LOG_LEVEL_INFO, cat, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define klog_debug(cat, fmt, ...) \
    klog_write(LOG_LEVEL_DEBUG, cat, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define klog_trace(cat, fmt, ...) \
    klog_write(LOG_LEVEL_TRACE, cat, __func__, __LINE__, fmt, ##__VA_ARGS__)

/* Convenience macros for common categories */
#define klog_kernel(level, fmt, ...) \
    klog_write(level, LOG_CAT_KERNEL, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define klog_memory(level, fmt, ...) \
    klog_write(level, LOG_CAT_MEMORY, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define klog_ipc(level, fmt, ...) \
    klog_write(level, LOG_CAT_IPC, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define klog_device(level, fmt, ...) \
    klog_write(level, LOG_CAT_DEVICE, __func__, __LINE__, fmt, ##__VA_ARGS__)

/* ================================
 * Core Logging Functions
 * ================================ */

/* Initialize the logging system */
int klog_init(const log_config_t* config);

/* Shutdown the logging system */
void klog_shutdown(void);

/* Write a log message */
void klog_write(log_level_t level, log_category_t category, 
                const char* function, uint16_t line, 
                const char* format, ...);

/* Write a log message with va_list */
void klog_vwrite(log_level_t level, log_category_t category,
                 const char* function, uint16_t line,
                 const char* format, va_list args);

/* ================================
 * Configuration Functions
 * ================================ */

/* Set global log level */
void klog_set_level(log_level_t level);

/* Set log level for specific category */
void klog_set_category_level(log_category_t category, log_level_t level);

/* Enable/disable output target */
void klog_set_output(log_output_t target, bool enabled);

/* Enable/disable timestamps */
void klog_set_timestamps(bool enabled);

/* Enable/disable colors */
void klog_set_colors(bool enabled);

/* ================================
 * Serial Port Functions
 * ================================ */

/* Initialize serial port for logging */
int klog_serial_init(uint16_t port, uint32_t baud_rate);

/* Send character via serial */
void klog_serial_putchar(char c);

/* Send string via serial */
void klog_serial_puts(const char* str);

/* ================================
 * Buffer Management Functions
 * ================================ */

/* Get log entries from buffer */
int klog_get_entries(log_entry_t* entries, int max_entries);

/* Clear log buffer */
void klog_clear_buffer(void);

/* Get buffer status */
int klog_get_buffer_status(uint32_t* used, uint32_t* total);

/* ================================
 * Debugging Support Functions
 * ================================ */

/* Dump system state for debugging */
void klog_dump_system_state(void);

/* Dump memory region */
void klog_dump_memory(const void* ptr, size_t size, const char* label);

/* Dump processor state */
void klog_dump_registers(void);

/* ================================
 * Statistics Functions
 * ================================ */

/* Get logging statistics */
void klog_get_stats(log_stats_t* stats);

/* Reset statistics */
void klog_reset_stats(void);

/* Print statistics to log */
void klog_print_stats(void);

/* ================================
 * Utility Functions
 * ================================ */

/* Get level name string */
const char* klog_level_name(log_level_t level);

/* Get category name string */
const char* klog_category_name(log_category_t category);

/* Get current timestamp */
uint64_t klog_get_timestamp(void);

/* Check if level/category should be logged */
bool klog_should_log(log_level_t level, log_category_t category);

/* ================================
 * Default Configuration
 * ================================ */

extern const log_config_t klog_default_config;

/* ================================
 * Debug Assertion Support
 * ================================ */

#ifdef DEBUG
#define kassert(expr) \
    do { \
        if (!(expr)) { \
            klog_panic(LOG_CAT_KERNEL, "Assertion failed: %s at %s:%d", \
                      #expr, __FILE__, __LINE__); \
            while(1) { /* halt */ } \
        } \
    } while(0)
#else
#define kassert(expr) ((void)0)
#endif

#define kpanic(fmt, ...) \
    do { \
        klog_panic(LOG_CAT_KERNEL, fmt, ##__VA_ARGS__); \
        while(1) { /* halt */ } \
    } while(0)

#endif /* KERNEL_LOG_H */
