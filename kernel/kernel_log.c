/* IKOS Kernel Debugging & Logging System Implementation - Issue #16
 * 
 * Complete implementation of the kernel logging system with serial port output,
 * buffering, and comprehensive debugging support for tracking kernel execution.
 */

#include "../include/kernel_log.h"
#include "../include/boot.h"
#include <string.h>

/* ================================
 * Internal Data Structures
 * ================================ */

/* Log buffer for storing messages */
typedef struct {
    log_entry_t* entries;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    bool overrun;
} log_buffer_t;

/* ================================
 * Global Variables
 * ================================ */

static log_config_t g_log_config;
static log_buffer_t g_log_buffer;
static log_stats_t g_log_stats;
static bool g_log_initialized = false;
static uint64_t g_system_ticks = 0; /* Simple tick counter for timestamps */

/* Log level names */
static const char* g_level_names[] = {
    "PANIC", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"
};

/* Log category names */
static const char* g_category_names[] = {
    "KERNEL", "MEMORY", "IPC", "DEVICE", "SCHED", "IRQ", "BOOT", "PROC", "USB"
};

/* Default configuration */
const log_config_t klog_default_config = {
    .global_level = LOG_LEVEL_INFO,
    .category_levels = {
        [LOG_CAT_KERNEL]   = LOG_LEVEL_INFO,
        [LOG_CAT_MEMORY]   = LOG_LEVEL_WARN,
        [LOG_CAT_IPC]      = LOG_LEVEL_INFO,
        [LOG_CAT_DEVICE]   = LOG_LEVEL_INFO,
        [LOG_CAT_SCHEDULE] = LOG_LEVEL_WARN,
        [LOG_CAT_INTERRUPT] = LOG_LEVEL_WARN,
        [LOG_CAT_BOOT]     = LOG_LEVEL_INFO,
        [LOG_CAT_PROCESS]  = LOG_LEVEL_INFO,
        [LOG_CAT_USB]      = LOG_LEVEL_INFO
    },
    .output_targets = LOG_OUTPUT_SERIAL | LOG_OUTPUT_VGA,
    .timestamps_enabled = true,
    .colors_enabled = true,
    .category_names_enabled = true,
    .function_names_enabled = true,
    .serial_port = SERIAL_COM1_BASE,
    .buffer_size = 1024
};

/* ================================
 * Internal Helper Functions
 * ================================ */

/**
 * Simple string formatting function (limited printf implementation)
 */
static int klog_sprintf(char* buffer, size_t size, const char* format, va_list args) {
    char* buf = buffer;
    const char* fmt = format;
    size_t remaining = size - 1; /* Reserve space for null terminator */
    
    while (*fmt && remaining > 0) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++; /* Skip '%' */
            
            switch (*fmt) {
                case 's': {
                    const char* str = va_arg(args, const char*);
                    if (str) {
                        while (*str && remaining > 0) {
                            *buf++ = *str++;
                            remaining--;
                        }
                    }
                    break;
                }
                case 'd': {
                    int value = va_arg(args, int);
                    if (value < 0) {
                        if (remaining > 0) {
                            *buf++ = '-';
                            remaining--;
                            value = -value;
                        }
                    }
                    
                    /* Convert to string (simple implementation) */
                    char temp[16];
                    int i = 0;
                    if (value == 0) {
                        temp[i++] = '0';
                    } else {
                        while (value > 0 && i < sizeof(temp) - 1) {
                            temp[i++] = '0' + (value % 10);
                            value /= 10;
                        }
                    }
                    
                    /* Copy reversed */
                    while (i > 0 && remaining > 0) {
                        *buf++ = temp[--i];
                        remaining--;
                    }
                    break;
                }
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    const char* hex = "0123456789abcdef";
                    
                    /* Convert to hex string */
                    char temp[16];
                    int i = 0;
                    if (value == 0) {
                        temp[i++] = '0';
                    } else {
                        while (value > 0 && i < sizeof(temp) - 1) {
                            temp[i++] = hex[value & 0xF];
                            value >>= 4;
                        }
                    }
                    
                    /* Copy reversed */
                    while (i > 0 && remaining > 0) {
                        *buf++ = temp[--i];
                        remaining--;
                    }
                    break;
                }
                case 'p': {
                    void* ptr = va_arg(args, void*);
                    uintptr_t value = (uintptr_t)ptr;
                    const char* hex = "0123456789abcdef";
                    
                    /* Add 0x prefix */
                    if (remaining >= 2) {
                        *buf++ = '0';
                        *buf++ = 'x';
                        remaining -= 2;
                    }
                    
                    /* Convert to hex string */
                    char temp[32];
                    int i = 0;
                    if (value == 0) {
                        temp[i++] = '0';
                    } else {
                        while (value > 0 && i < sizeof(temp) - 1) {
                            temp[i++] = hex[value & 0xF];
                            value >>= 4;
                        }
                    }
                    
                    /* Copy reversed */
                    while (i > 0 && remaining > 0) {
                        *buf++ = temp[--i];
                        remaining--;
                    }
                    break;
                }
                case 'c': {
                    int c = va_arg(args, int);
                    if (remaining > 0) {
                        *buf++ = (char)c;
                        remaining--;
                    }
                    break;
                }
                case '%': {
                    if (remaining > 0) {
                        *buf++ = '%';
                        remaining--;
                    }
                    break;
                }
                default:
                    /* Unknown format specifier, just copy */
                    if (remaining >= 2) {
                        *buf++ = '%';
                        *buf++ = *fmt;
                        remaining -= 2;
                    }
                    break;
            }
        } else {
            *buf++ = *fmt;
            remaining--;
        }
        fmt++;
    }
    
    *buf = '\0';
    return buf - buffer;
}

/**
 * Initialize circular buffer
 */
static int init_log_buffer(uint32_t size) {
    /* Allocate buffer memory (simplified - using static buffer for now) */
    static log_entry_t static_buffer[1024];
    
    g_log_buffer.entries = static_buffer;
    g_log_buffer.capacity = (size < 1024) ? size : 1024;
    g_log_buffer.head = 0;
    g_log_buffer.tail = 0;
    g_log_buffer.count = 0;
    g_log_buffer.overrun = false;
    
    return 0;
}

/**
 * Add entry to circular buffer
 */
static void add_to_buffer(const log_entry_t* entry) {
    if (!g_log_buffer.entries) {
        return;
    }
    
    /* Copy entry to buffer */
    g_log_buffer.entries[g_log_buffer.head] = *entry;
    
    g_log_buffer.head = (g_log_buffer.head + 1) % g_log_buffer.capacity;
    
    if (g_log_buffer.count < g_log_buffer.capacity) {
        g_log_buffer.count++;
    } else {
        /* Buffer full, overwrite oldest */
        g_log_buffer.tail = (g_log_buffer.tail + 1) % g_log_buffer.capacity;
        g_log_buffer.overrun = true;
        g_log_stats.buffer_overruns++;
    }
}

/* ================================
 * Serial Port Implementation
 * ================================ */

/**
 * Initialize serial port for logging
 */
int klog_serial_init(uint16_t port, uint32_t baud_rate) {
    /* Set baud rate (simplified - using default 38400) */
    /* In real implementation, would calculate divisor from baud_rate */
    
    /* Disable interrupts */
    outb(port + 1, 0x00);
    
    /* Enable DLAB (Divisor Latch Access Bit) */
    outb(port + 3, 0x80);
    
    /* Set divisor for 38400 baud (115200 / 38400 = 3) */
    outb(port + 0, 0x03);
    outb(port + 1, 0x00);
    
    /* 8 bits, no parity, one stop bit */
    outb(port + 3, 0x03);
    
    /* Enable FIFO, clear, with 14-byte threshold */
    outb(port + 2, 0xC7);
    
    /* Enable IRQs, set RTS/DSR */
    outb(port + 4, 0x0B);
    
    return 0;
}

/**
 * Send character via serial port
 */
void klog_serial_putchar(char c) {
    uint16_t port = g_log_config.serial_port;
    
    /* Wait for transmitter to be ready */
    while ((inb(port + SERIAL_STATUS_PORT) & SERIAL_READY_BIT) == 0) {
        /* Wait */
    }
    
    /* Send character */
    outb(port + SERIAL_DATA_PORT, c);
    
    g_log_stats.serial_bytes_sent++;
}

/**
 * Send string via serial port
 */
void klog_serial_puts(const char* str) {
    while (*str) {
        klog_serial_putchar(*str);
        str++;
    }
}

/* ================================
 * VGA Output Functions
 * ================================ */

static uint16_t vga_cursor_pos = 0;

/**
 * Output character to VGA text mode
 */
static void vga_putchar(char c, uint8_t color) {
    volatile uint16_t* vga_memory = (volatile uint16_t*)VGA_TEXT_BUFFER;
    
    if (c == '\n') {
        vga_cursor_pos = (vga_cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
    } else {
        vga_memory[vga_cursor_pos] = (color << 8) | c;
        vga_cursor_pos++;
    }
    
    /* Wrap around if we reach the end */
    if (vga_cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        vga_cursor_pos = 0;
    }
}

/**
 * Output string to VGA with color
 */
static void vga_puts(const char* str, uint8_t color) {
    while (*str) {
        vga_putchar(*str, color);
        str++;
    }
}

/**
 * Get VGA color for log level
 */
static uint8_t get_vga_color(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_PANIC: return 0x0C; /* Light red */
        case LOG_LEVEL_ERROR: return 0x04; /* Red */
        case LOG_LEVEL_WARN:  return 0x0E; /* Yellow */
        case LOG_LEVEL_INFO:  return 0x0F; /* White */
        case LOG_LEVEL_DEBUG: return 0x0B; /* Light cyan */
        case LOG_LEVEL_TRACE: return 0x08; /* Dark gray */
        default:              return 0x07; /* Light gray */
    }
}

/* ================================
 * Core Logging Functions
 * ================================ */

/**
 * Initialize the logging system
 */
int klog_init(const log_config_t* config) {
    if (g_log_initialized) {
        return 0; /* Already initialized */
    }
    
    /* Copy configuration */
    if (config) {
        g_log_config = *config;
    } else {
        g_log_config = klog_default_config;
    }
    
    /* Initialize statistics */
    memset(&g_log_stats, 0, sizeof(log_stats_t));
    
    /* Initialize circular buffer */
    if (g_log_config.output_targets & LOG_OUTPUT_BUFFER) {
        init_log_buffer(g_log_config.buffer_size);
    }
    
    /* Initialize serial port */
    if (g_log_config.output_targets & LOG_OUTPUT_SERIAL) {
        klog_serial_init(g_log_config.serial_port, 38400);
    }
    
    g_log_initialized = true;
    
    /* Log initialization message */
    klog_info(LOG_CAT_KERNEL, "Kernel logging system initialized");
    
    return 0;
}

/**
 * Shutdown the logging system
 */
void klog_shutdown(void) {
    if (!g_log_initialized) {
        return;
    }
    
    klog_info(LOG_CAT_KERNEL, "Kernel logging system shutting down");
    
    g_log_initialized = false;
}

/**
 * Check if message should be logged
 */
bool klog_should_log(log_level_t level, log_category_t category) {
    if (!g_log_initialized) {
        return false;
    }
    
    /* Check global level */
    if (level > g_log_config.global_level) {
        return false;
    }
    
    /* Check category-specific level */
    if (category < LOG_CAT_MAX && level > g_log_config.category_levels[category]) {
        return false;
    }
    
    return true;
}

/**
 * Format and output log message
 */
static void output_log_message(const log_entry_t* entry) {
    char formatted_message[512];
    char* msg = formatted_message;
    size_t remaining = sizeof(formatted_message) - 1;
    
    /* Build formatted message */
    if (g_log_config.timestamps_enabled) {
        /* Add timestamp */
        int len = snprintf(msg, remaining, "[%08lu] ", (unsigned long)entry->timestamp);
        msg += len;
        remaining -= len;
    }
    
    /* Add level */
    if (g_log_config.colors_enabled && (g_log_config.output_targets & LOG_OUTPUT_SERIAL)) {
        const char* color = "";
        switch (entry->level) {
            case LOG_LEVEL_PANIC: color = LOG_COLOR_PANIC; break;
            case LOG_LEVEL_ERROR: color = LOG_COLOR_ERROR; break;
            case LOG_LEVEL_WARN:  color = LOG_COLOR_WARN; break;
            case LOG_LEVEL_INFO:  color = LOG_COLOR_INFO; break;
            case LOG_LEVEL_DEBUG: color = LOG_COLOR_DEBUG; break;
            case LOG_LEVEL_TRACE: color = LOG_COLOR_TRACE; break;
        }
        
        int len = snprintf(msg, remaining, "%s%-5s%s ", 
                          color, g_level_names[entry->level], LOG_COLOR_RESET);
        msg += len;
        remaining -= len;
    } else {
        int len = snprintf(msg, remaining, "%-5s ", g_level_names[entry->level]);
        msg += len;
        remaining -= len;
    }
    
    /* Add category */
    if (g_log_config.category_names_enabled) {
        int len = snprintf(msg, remaining, "[%s] ", g_category_names[entry->category]);
        msg += len;
        remaining -= len;
    }
    
    /* Add function name */
    if (g_log_config.function_names_enabled && entry->function) {
        int len = snprintf(msg, remaining, "%s:%d ", entry->function, entry->line);
        msg += len;
        remaining -= len;
    }
    
    /* Add actual message */
    int len = snprintf(msg, remaining, "%s\n", entry->message);
    msg += len;
    
    /* Output to configured targets */
    if (g_log_config.output_targets & LOG_OUTPUT_SERIAL) {
        klog_serial_puts(formatted_message);
    }
    
    if (g_log_config.output_targets & LOG_OUTPUT_VGA) {
        uint8_t color = get_vga_color(entry->level);
        vga_puts(formatted_message, color);
    }
}

/**
 * Write a log message
 */
void klog_write(log_level_t level, log_category_t category, 
                const char* function, uint16_t line, 
                const char* format, ...) {
    if (!klog_should_log(level, category)) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    klog_vwrite(level, category, function, line, format, args);
    va_end(args);
}

/**
 * Write a log message with va_list
 */
void klog_vwrite(log_level_t level, log_category_t category,
                 const char* function, uint16_t line,
                 const char* format, va_list args) {
    if (!klog_should_log(level, category)) {
        return;
    }
    
    /* Create log entry */
    log_entry_t entry;
    entry.timestamp = klog_get_timestamp();
    entry.level = level;
    entry.category = category;
    entry.function = function;
    entry.line = line;
    
    /* Format message */
    klog_sprintf(entry.message, sizeof(entry.message), format, args);
    
    /* Add to buffer if enabled */
    if (g_log_config.output_targets & LOG_OUTPUT_BUFFER) {
        add_to_buffer(&entry);
    }
    
    /* Output formatted message */
    output_log_message(&entry);
    
    /* Update statistics */
    g_log_stats.total_messages++;
    if (level < 6) {
        g_log_stats.messages_by_level[level]++;
    }
    if (category < LOG_CAT_MAX) {
        g_log_stats.messages_by_category[category]++;
    }
}

/* ================================
 * Configuration Functions
 * ================================ */

void klog_set_level(log_level_t level) {
    g_log_config.global_level = level;
}

void klog_set_category_level(log_category_t category, log_level_t level) {
    if (category < LOG_CAT_MAX) {
        g_log_config.category_levels[category] = level;
    }
}

void klog_set_output(log_output_t target, bool enabled) {
    if (enabled) {
        g_log_config.output_targets |= target;
    } else {
        g_log_config.output_targets &= ~target;
    }
}

void klog_set_timestamps(bool enabled) {
    g_log_config.timestamps_enabled = enabled;
}

void klog_set_colors(bool enabled) {
    g_log_config.colors_enabled = enabled;
}

/* ================================
 * Utility Functions
 * ================================ */

const char* klog_level_name(log_level_t level) {
    if (level < 6) {
        return g_level_names[level];
    }
    return "UNKNOWN";
}

const char* klog_category_name(log_category_t category) {
    if (category < LOG_CAT_MAX) {
        return g_category_names[category];
    }
    return "UNKNOWN";
}

uint64_t klog_get_timestamp(void) {
    return ++g_system_ticks;  /* Simple tick counter */
}

void klog_get_stats(log_stats_t* stats) {
    if (stats) {
        *stats = g_log_stats;
    }
}

void klog_reset_stats(void) {
    memset(&g_log_stats, 0, sizeof(log_stats_t));
}

void klog_print_stats(void) {
    klog_info(LOG_CAT_KERNEL, "=== Logging Statistics ===");
    klog_info(LOG_CAT_KERNEL, "Total messages: %lu", (unsigned long)g_log_stats.total_messages);
    klog_info(LOG_CAT_KERNEL, "PANIC: %lu, ERROR: %lu, WARN: %lu", 
              (unsigned long)g_log_stats.messages_by_level[0],
              (unsigned long)g_log_stats.messages_by_level[1],
              (unsigned long)g_log_stats.messages_by_level[2]);
    klog_info(LOG_CAT_KERNEL, "INFO: %lu, DEBUG: %lu, TRACE: %lu",
              (unsigned long)g_log_stats.messages_by_level[3],
              (unsigned long)g_log_stats.messages_by_level[4],
              (unsigned long)g_log_stats.messages_by_level[5]);
    klog_info(LOG_CAT_KERNEL, "Serial bytes sent: %lu", (unsigned long)g_log_stats.serial_bytes_sent);
    klog_info(LOG_CAT_KERNEL, "Buffer overruns: %lu", (unsigned long)g_log_stats.buffer_overruns);
}

/* ================================
 * Debugging Support Functions
 * ================================ */

void klog_dump_memory(const void* ptr, size_t size, const char* label) {
    const uint8_t* bytes = (const uint8_t*)ptr;
    
    klog_debug(LOG_CAT_KERNEL, "=== Memory Dump: %s ===", label ? label : "Unknown");
    klog_debug(LOG_CAT_KERNEL, "Address: %p, Size: %d bytes", ptr, (int)size);
    
    for (size_t i = 0; i < size; i += 16) {
        char hex_line[64] = {0};
        char ascii_line[17] = {0};
        
        for (size_t j = 0; j < 16 && (i + j) < size; j++) {
            uint8_t byte = bytes[i + j];
            snprintf(hex_line + j * 3, 4, "%02x ", byte);
            ascii_line[j] = (byte >= 32 && byte < 127) ? byte : '.';
        }
        
        klog_debug(LOG_CAT_KERNEL, "%08x: %-48s |%s|", (unsigned int)(i), hex_line, ascii_line);
    }
}

void klog_dump_system_state(void) {
    klog_info(LOG_CAT_KERNEL, "=== System State Dump ===");
    klog_info(LOG_CAT_KERNEL, "Logging system initialized: %s", g_log_initialized ? "Yes" : "No");
    klog_info(LOG_CAT_KERNEL, "Global log level: %s", klog_level_name(g_log_config.global_level));
    klog_info(LOG_CAT_KERNEL, "Output targets: 0x%02x", g_log_config.output_targets);
    klog_info(LOG_CAT_KERNEL, "Serial port: 0x%04x", g_log_config.serial_port);
    klog_print_stats();
}

/* Placeholder for actual port I/O functions - these would be implemented elsewhere */
void outb(uint16_t port, uint8_t value) {
    /* Assembly implementation would go here */
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    /* Assembly implementation would go here */
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Simple snprintf implementation */
int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = klog_sprintf(buffer, size, format, args);
    va_end(args);
    return result;
}
