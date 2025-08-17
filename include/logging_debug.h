#ifndef LOGGING_DEBUG_H
#define LOGGING_DEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>

/* ========================== Core Constants ========================== */

#define LOG_MAX_MESSAGE_SIZE    1024
#define LOG_MAX_OUTPUTS         16
#define LOG_MAX_BUFFERS         8
#define LOG_MAX_SYMBOLS         4096
#define LOG_MAX_STACK_FRAMES    32
#define LOG_MAX_NAME_LENGTH     256

/* Error codes */
#define LOG_SUCCESS             0
#define LOG_ERROR_INVALID       -1
#define LOG_ERROR_MEMORY        -2
#define LOG_ERROR_IO            -3
#define LOG_ERROR_FULL          -4
#define LOG_ERROR_NOT_FOUND     -5
#define LOG_ERROR_TRUNCATED     -6
#define LOG_ERROR_INIT          -7
#define LOG_ERROR_NOT_INIT      -8

/* ========================== Core Data Types ========================== */

/* Log levels - compatible with syslog levels */
typedef enum {
    LOG_LEVEL_DEBUG   = 0,      /* Detailed debugging information */
    LOG_LEVEL_INFO    = 1,      /* General information */
    LOG_LEVEL_NOTICE  = 2,      /* Notable events */
    LOG_LEVEL_WARN    = 3,      /* Warning conditions */
    LOG_LEVEL_ERROR   = 4,      /* Error conditions */
    LOG_LEVEL_CRIT    = 5,      /* Critical conditions */
    LOG_LEVEL_ALERT   = 6,      /* Alert conditions */
    LOG_LEVEL_EMERG   = 7,      /* Emergency conditions */
    LOG_LEVEL_MAX     = 8
} log_level_t;

/* Log facilities */
typedef enum {
    LOG_FACILITY_KERNEL   = 0,  /* Kernel messages */
    LOG_FACILITY_USER     = 1,  /* User-space applications */
    LOG_FACILITY_MAIL     = 2,  /* Mail system */
    LOG_FACILITY_DAEMON   = 3,  /* System daemons */
    LOG_FACILITY_AUTH     = 4,  /* Security/authorization */
    LOG_FACILITY_SYSLOG   = 5,  /* Internal syslog messages */
    LOG_FACILITY_LPR      = 6,  /* Line printer subsystem */
    LOG_FACILITY_NEWS     = 7,  /* Network news subsystem */
    LOG_FACILITY_UUCP     = 8,  /* UUCP subsystem */
    LOG_FACILITY_CRON     = 9,  /* Clock daemon */
    LOG_FACILITY_AUTHPRIV = 10, /* Security/authorization (private) */
    LOG_FACILITY_FTP      = 11, /* FTP daemon */
    LOG_FACILITY_LOCAL0   = 16, /* Local use facility 0 */
    LOG_FACILITY_LOCAL1   = 17, /* Local use facility 1 */
    LOG_FACILITY_LOCAL2   = 18, /* Local use facility 2 */
    LOG_FACILITY_LOCAL3   = 19, /* Local use facility 3 */
    LOG_FACILITY_LOCAL4   = 20, /* Local use facility 4 */
    LOG_FACILITY_LOCAL5   = 21, /* Local use facility 5 */
    LOG_FACILITY_LOCAL6   = 22, /* Local use facility 6 */
    LOG_FACILITY_LOCAL7   = 23  /* Local use facility 7 */
} log_facility_t;

/* Output destinations */
typedef enum {
    LOG_OUTPUT_NONE     = 0x00, /* No output */
    LOG_OUTPUT_CONSOLE  = 0x01, /* Console output */
    LOG_OUTPUT_FILE     = 0x02, /* File output */
    LOG_OUTPUT_SERIAL   = 0x04, /* Serial port output */
    LOG_OUTPUT_NETWORK  = 0x08, /* Network output */
    LOG_OUTPUT_BUFFER   = 0x10, /* Ring buffer only */
    LOG_OUTPUT_SYSLOG   = 0x20, /* System log service */
    LOG_OUTPUT_KMSG     = 0x40, /* Kernel message buffer */
    LOG_OUTPUT_ALL      = 0xFF  /* All outputs */
} log_output_t;

/* Log message flags */
typedef enum {
    LOG_FLAG_NONE       = 0x0000, /* No special flags */
    LOG_FLAG_URGENT     = 0x0001, /* Urgent message */
    LOG_FLAG_STACKTRACE = 0x0002, /* Include stack trace */
    LOG_FLAG_TIMESTAMP  = 0x0004, /* Include timestamp */
    LOG_FLAG_PROCESS    = 0x0008, /* Include process info */
    LOG_FLAG_THREAD     = 0x0010, /* Include thread info */
    LOG_FLAG_LOCATION   = 0x0020, /* Include file/line info */
    LOG_FLAG_BINARY     = 0x0040, /* Binary data */
    LOG_FLAG_COMPRESSED = 0x0080, /* Compressed message */
    LOG_FLAG_ENCRYPTED  = 0x0100, /* Encrypted message */
    LOG_FLAG_MULTILINE  = 0x0200, /* Multi-line message */
    LOG_FLAG_CONTINUOUS = 0x0400, /* Continuation of previous */
    LOG_FLAG_KERNEL     = 0x0800, /* From kernel context */
    LOG_FLAG_INTERRUPT  = 0x1000, /* From interrupt context */
    LOG_FLAG_ATOMIC     = 0x2000, /* Atomic context safe */
    LOG_FLAG_EMERGENCY  = 0x4000, /* Emergency message */
    LOG_FLAG_AUDIT      = 0x8000  /* Audit trail message */
} log_flags_t;

/* ========================== Core Structures ========================== */

/* High-resolution timestamp */
typedef struct {
    uint64_t    seconds;        /* Seconds since epoch */
    uint32_t    nanoseconds;    /* Nanoseconds */
    uint32_t    cpu_id;         /* CPU ID where logged */
} log_timestamp_t;

/* Process and thread information */
typedef struct {
    uint32_t    process_id;     /* Process ID */
    uint32_t    thread_id;      /* Thread ID */
    uint32_t    user_id;        /* User ID */
    uint32_t    group_id;       /* Group ID */
    char        process_name[16]; /* Process name */
    char        thread_name[16];  /* Thread name */
} log_context_t;

/* Source location information */
typedef struct {
    const char* file;           /* Source file name */
    const char* function;       /* Function name */
    uint32_t    line;           /* Line number */
    uint32_t    column;         /* Column number */
} log_location_t;

/* Log message structure */
typedef struct log_message {
    /* Header */
    uint32_t        magic;          /* Magic number for validation */
    uint32_t        size;           /* Total message size */
    uint32_t        sequence;       /* Sequence number */
    uint32_t        checksum;       /* Message checksum */
    
    /* Metadata */
    log_timestamp_t timestamp;      /* High-resolution timestamp */
    log_context_t   context;        /* Process/thread context */
    log_location_t  location;       /* Source location */
    
    /* Message properties */
    log_level_t     level;          /* Log level */
    log_facility_t  facility;       /* Log facility */
    log_flags_t     flags;          /* Message flags */
    uint16_t        format_length;  /* Format string length */
    uint16_t        data_length;    /* Data length */
    
    /* Variable-length data */
    char            data[];         /* Format string + arguments */
} log_message_t;

/* ========================== Buffer Management ========================== */

/* Ring buffer configuration */
typedef struct {
    size_t          size;           /* Buffer size in bytes */
    size_t          max_message;    /* Maximum message size */
    uint32_t        flags;          /* Buffer flags */
    bool            overwrite;      /* Overwrite old messages */
    bool            blocking;       /* Block on full buffer */
    uint32_t        timeout_ms;     /* Timeout for blocking operations */
} log_buffer_config_t;

/* Ring buffer statistics */
typedef struct {
    uint64_t        messages_written;   /* Total messages written */
    uint64_t        messages_read;      /* Total messages read */
    uint64_t        messages_dropped;   /* Messages dropped due to overflow */
    uint64_t        bytes_written;      /* Total bytes written */
    uint64_t        bytes_read;         /* Total bytes read */
    uint32_t        current_size;       /* Current buffer usage */
    uint32_t        peak_size;          /* Peak buffer usage */
    uint32_t        readers;            /* Active readers */
    uint32_t        writers;            /* Active writers */
} log_buffer_stats_t;

/* Ring buffer structure */
typedef struct log_buffer {
    /* Configuration */
    log_buffer_config_t config;
    
    /* Buffer data */
    char*           data;           /* Buffer memory */
    volatile size_t head;           /* Write position */
    volatile size_t tail;           /* Read position */
    volatile size_t used;           /* Used bytes */
    
    /* Synchronization */
    void*           mutex;          /* Buffer mutex */
    void*           read_event;     /* Read event */
    void*           write_event;    /* Write event */
    
    /* Statistics */
    log_buffer_stats_t stats;
    
    /* Management */
    uint32_t        id;             /* Buffer ID */
    char            name[32];       /* Buffer name */
    bool            active;         /* Buffer is active */
} log_buffer_t;

/* ========================== Output Management ========================== */

/* Output configuration */
typedef struct {
    log_output_t    type;           /* Output type */
    log_level_t     min_level;      /* Minimum log level */
    log_level_t     max_level;      /* Maximum log level */
    log_facility_t  facility_mask;  /* Facility filter */
    log_flags_t     flag_mask;      /* Flag filter */
    bool            enabled;        /* Output enabled */
    bool            async;          /* Asynchronous output */
    uint32_t        buffer_size;    /* Output buffer size */
    char            name[64];       /* Output name */
} log_output_config_t;

/* File output configuration */
typedef struct {
    char            path[256];      /* File path */
    uint64_t        max_size;       /* Maximum file size */
    uint32_t        max_files;      /* Maximum number of files */
    bool            rotate;         /* Enable rotation */
    bool            compress;       /* Compress rotated files */
    uint32_t        sync_interval;  /* Sync interval (messages) */
    mode_t          permissions;    /* File permissions */
} log_file_config_t;

/* Serial output configuration */
typedef struct {
    char            device[64];     /* Serial device path */
    uint32_t        baud_rate;      /* Baud rate */
    uint8_t         data_bits;      /* Data bits (5-8) */
    uint8_t         stop_bits;      /* Stop bits (1-2) */
    char            parity;         /* Parity (N/E/O) */
    bool            flow_control;   /* Hardware flow control */
    uint32_t        timeout_ms;     /* Write timeout */
} log_serial_config_t;

/* Network output configuration */
typedef struct {
    char            host[256];      /* Remote host */
    uint16_t        port;           /* Remote port */
    uint8_t         protocol;       /* Protocol (TCP/UDP) */
    bool            ssl;            /* Use SSL/TLS */
    uint32_t        retry_count;    /* Retry attempts */
    uint32_t        retry_delay;    /* Retry delay (ms) */
    char            cert_path[256]; /* SSL certificate path */
} log_network_config_t;

/* Output handler function */
typedef int (*log_output_handler_t)(const log_message_t* message, void* context);

/* Output structure */
typedef struct log_output {
    /* Configuration */
    log_output_config_t config;
    
    /* Type-specific configuration */
    union {
        log_file_config_t    file;
        log_serial_config_t  serial;
        log_network_config_t network;
    } type_config;
    
    /* Handler */
    log_output_handler_t handler;
    void*               context;
    
    /* State */
    bool                active;
    uint64_t            messages_sent;
    uint64_t            bytes_sent;
    uint64_t            errors;
    
    /* Management */
    struct log_output*  next;
    uint32_t            id;
} log_output_t;

/* ========================== Debug Symbol Support ========================== */

/* Symbol types */
typedef enum {
    SYMBOL_TYPE_FUNCTION = 1,   /* Function symbol */
    SYMBOL_TYPE_VARIABLE = 2,   /* Variable symbol */
    SYMBOL_TYPE_TYPE     = 3,   /* Type symbol */
    SYMBOL_TYPE_MODULE   = 4    /* Module symbol */
} symbol_type_t;

/* Debug symbol structure */
typedef struct debug_symbol {
    uint64_t        address;        /* Symbol address */
    uint64_t        size;           /* Symbol size */
    symbol_type_t   type;           /* Symbol type */
    char*           name;           /* Symbol name */
    char*           file;           /* Source file */
    uint32_t        line;           /* Line number */
    uint32_t        flags;          /* Symbol flags */
    struct debug_symbol* next;      /* Next symbol */
} debug_symbol_t;

/* Symbol table */
typedef struct {
    debug_symbol_t* symbols;        /* Symbol list */
    uint32_t        count;          /* Symbol count */
    uint64_t        base_address;   /* Base address */
    char*           module_name;    /* Module name */
    bool            loaded;         /* Table loaded */
} symbol_table_t;

/* Stack frame information */
typedef struct {
    uint64_t        address;        /* Frame address */
    uint64_t        return_address; /* Return address */
    const char*     function;       /* Function name */
    const char*     file;           /* Source file */
    uint32_t        line;           /* Line number */
    uint64_t        offset;         /* Offset from symbol */
} stack_frame_t;

/* Stack trace */
typedef struct {
    stack_frame_t*  frames;         /* Stack frames */
    uint32_t        count;          /* Frame count */
    uint32_t        max_frames;     /* Maximum frames */
    bool            truncated;      /* Trace truncated */
} stack_trace_t;

/* ========================== Logger Configuration ========================== */

/* Logger configuration */
typedef struct {
    /* Global settings */
    log_level_t     global_level;       /* Global minimum level */
    log_output_t    default_outputs;    /* Default output mask */
    bool            enable_timestamps;   /* Enable timestamps */
    bool            enable_context;      /* Enable context info */
    bool            enable_location;     /* Enable location info */
    bool            enable_colors;       /* Enable colored output */
    
    /* Buffer settings */
    size_t          buffer_size;        /* Default buffer size */
    uint32_t        max_buffers;        /* Maximum buffers */
    bool            async_logging;      /* Asynchronous logging */
    uint32_t        flush_interval;     /* Flush interval (ms) */
    
    /* Performance settings */
    bool            lazy_formatting;    /* Defer formatting */
    bool            batch_processing;   /* Batch output */
    uint32_t        batch_size;         /* Batch size */
    uint32_t        rate_limit;         /* Rate limit (msg/sec) */
    
    /* Debug settings */
    bool            enable_symbols;     /* Enable symbol resolution */
    bool            enable_stacktrace;  /* Enable stack traces */
    uint32_t        max_stack_depth;    /* Maximum stack depth */
    bool            kernel_symbols;     /* Load kernel symbols */
    
    /* Security settings */
    bool            filter_sensitive;   /* Filter sensitive data */
    bool            audit_logging;      /* Enable audit logging */
    uint32_t        max_message_size;   /* Maximum message size */
    char            log_directory[256]; /* Log directory */
} logger_config_t;

/* Logger statistics */
typedef struct {
    uint64_t        total_messages;     /* Total messages logged */
    uint64_t        messages_by_level[LOG_LEVEL_MAX]; /* Messages per level */
    uint64_t        messages_dropped;   /* Messages dropped */
    uint64_t        bytes_logged;       /* Total bytes logged */
    uint64_t        format_errors;      /* Format errors */
    uint64_t        output_errors;      /* Output errors */
    uint32_t        active_buffers;     /* Active buffers */
    uint32_t        active_outputs;     /* Active outputs */
    double          avg_message_size;   /* Average message size */
    double          messages_per_sec;   /* Messages per second */
} logger_stats_t;

/* Logger context */
typedef struct {
    /* Configuration */
    logger_config_t config;
    
    /* Buffers */
    log_buffer_t**  buffers;
    uint32_t        buffer_count;
    
    /* Outputs */
    log_output_t*   outputs;
    uint32_t        output_count;
    
    /* Symbol tables */
    symbol_table_t* kernel_symbols;
    symbol_table_t* user_symbols;
    
    /* State */
    bool            initialized;
    bool            shutdown;
    uint32_t        sequence_counter;
    
    /* Statistics */
    logger_stats_t  stats;
    
    /* Synchronization */
    void*           global_mutex;
    void*           worker_thread;
} logger_context_t;

/* ========================== Core API Functions ========================== */

/* Initialization and cleanup */
int logger_init(const logger_config_t* config);
void logger_shutdown(void);
bool logger_is_initialized(void);

/* Configuration management */
int logger_set_config(const logger_config_t* config);
int logger_get_config(logger_config_t* config);
int logger_set_level(log_level_t level);
log_level_t logger_get_level(void);

/* Basic logging functions */
int log_message(log_level_t level, log_facility_t facility, 
                const char* format, ...);
int log_message_args(log_level_t level, log_facility_t facility,
                     const char* format, va_list args);
int log_message_ext(log_level_t level, log_facility_t facility,
                    log_flags_t flags, const log_location_t* location,
                    const char* format, ...);

/* Convenience macros for common log levels */
#define log_debug(fmt, ...)   log_message(LOG_LEVEL_DEBUG, LOG_FACILITY_USER, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)    log_message(LOG_LEVEL_INFO, LOG_FACILITY_USER, fmt, ##__VA_ARGS__)
#define log_notice(fmt, ...)  log_message(LOG_LEVEL_NOTICE, LOG_FACILITY_USER, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)    log_message(LOG_LEVEL_WARN, LOG_FACILITY_USER, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...)   log_message(LOG_LEVEL_ERROR, LOG_FACILITY_USER, fmt, ##__VA_ARGS__)
#define log_crit(fmt, ...)    log_message(LOG_LEVEL_CRIT, LOG_FACILITY_USER, fmt, ##__VA_ARGS__)
#define log_alert(fmt, ...)   log_message(LOG_LEVEL_ALERT, LOG_FACILITY_USER, fmt, ##__VA_ARGS__)
#define log_emerg(fmt, ...)   log_message(LOG_LEVEL_EMERG, LOG_FACILITY_USER, fmt, ##__VA_ARGS__)

/* Kernel logging functions */
int klog_message(log_level_t level, const char* format, ...);
int klog_message_ext(log_level_t level, log_flags_t flags,
                     const log_location_t* location, const char* format, ...);

/* Kernel convenience macros */
#define klog_debug(fmt, ...)  klog_message(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define klog_info(fmt, ...)   klog_message(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define klog_warn(fmt, ...)   klog_message(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define klog_error(fmt, ...)  klog_message(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define klog_crit(fmt, ...)   klog_message(LOG_LEVEL_CRIT, fmt, ##__VA_ARGS__)

/* Advanced logging with location information */
#define LOG_LOCATION() { __FILE__, __func__, __LINE__, 0 }

#define log_at(level, fmt, ...) \
    do { \
        log_location_t loc = LOG_LOCATION(); \
        log_message_ext(level, LOG_FACILITY_USER, LOG_FLAG_LOCATION, &loc, fmt, ##__VA_ARGS__); \
    } while(0)

#define klog_at(level, fmt, ...) \
    do { \
        log_location_t loc = LOG_LOCATION(); \
        klog_message_ext(level, LOG_FLAG_LOCATION, &loc, fmt, ##__VA_ARGS__); \
    } while(0)

/* ========================== Buffer Management ========================== */

/* Buffer operations */
log_buffer_t* log_buffer_create(const char* name, const log_buffer_config_t* config);
void log_buffer_destroy(log_buffer_t* buffer);
int log_buffer_write(log_buffer_t* buffer, const log_message_t* message);
int log_buffer_read(log_buffer_t* buffer, log_message_t* message, size_t max_size);
void log_buffer_clear(log_buffer_t* buffer);
int log_buffer_get_stats(log_buffer_t* buffer, log_buffer_stats_t* stats);

/* Buffer management */
int logger_add_buffer(const char* name, const log_buffer_config_t* config);
int logger_remove_buffer(const char* name);
log_buffer_t* logger_get_buffer(const char* name);
int logger_list_buffers(char** names, uint32_t* count);

/* ========================== Output Management ========================== */

/* Output operations */
log_output_t* log_output_create(const log_output_config_t* config);
void log_output_destroy(log_output_t* output);
int log_output_write(log_output_t* output, const log_message_t* message);
int log_output_flush(log_output_t* output);

/* Output management */
int logger_add_output(const log_output_config_t* config);
int logger_remove_output(uint32_t output_id);
int logger_enable_output(uint32_t output_id, bool enabled);
int logger_list_outputs(uint32_t* ids, uint32_t* count);

/* Specific output types */
int logger_add_file_output(const char* path, log_level_t min_level,
                          const log_file_config_t* config);
int logger_add_serial_output(const char* device, log_level_t min_level,
                             const log_serial_config_t* config);
int logger_add_network_output(const char* host, uint16_t port,
                              log_level_t min_level,
                              const log_network_config_t* config);

/* ========================== Debug Symbol Support ========================== */

/* Symbol table operations */
int debug_load_symbols(const char* file_path, symbol_table_t** table);
void debug_unload_symbols(symbol_table_t* table);
debug_symbol_t* debug_find_symbol(symbol_table_t* table, uint64_t address);
debug_symbol_t* debug_find_symbol_by_name(symbol_table_t* table, const char* name);

/* Stack trace operations */
int debug_capture_stack_trace(stack_trace_t* trace, uint32_t max_frames);
void debug_free_stack_trace(stack_trace_t* trace);
int debug_format_stack_trace(const stack_trace_t* trace, char* buffer, size_t size);

/* Symbol resolution */
int debug_resolve_address(uint64_t address, char* symbol_name, size_t name_size,
                         char* file_name, size_t file_size, uint32_t* line);
int debug_addr_to_line(uint64_t address, const char** file, uint32_t* line);

/* Kernel symbol support */
int debug_load_kernel_symbols(void);
int debug_load_user_symbols(uint32_t process_id);

/* ========================== Utility Functions ========================== */

/* Message formatting */
int log_format_message(const log_message_t* message, char* buffer, size_t size);
int log_parse_message(const char* buffer, size_t size, log_message_t** message);

/* Level and facility utilities */
const char* log_level_to_string(log_level_t level);
log_level_t log_level_from_string(const char* level_str);
const char* log_facility_to_string(log_facility_t facility);
log_facility_t log_facility_from_string(const char* facility_str);

/* Time utilities */
void log_get_timestamp(log_timestamp_t* timestamp);
int log_format_timestamp(const log_timestamp_t* timestamp, char* buffer, size_t size);

/* Context utilities */
void log_get_context(log_context_t* context);
int log_format_context(const log_context_t* context, char* buffer, size_t size);

/* Statistics and monitoring */
int logger_get_stats(logger_stats_t* stats);
void logger_reset_stats(void);
int logger_get_buffer_usage(const char* name, double* usage_percent);

/* Configuration helpers */
void logger_default_config(logger_config_t* config);
int logger_load_config_file(const char* path, logger_config_t* config);
int logger_save_config_file(const char* path, const logger_config_t* config);

/* ========================== Error Codes ========================== */

#define LOG_SUCCESS           0       /* Success */
#define LOG_ERROR_INIT       -1       /* Initialization error */
#define LOG_ERROR_CONFIG     -2       /* Configuration error */
#define LOG_ERROR_MEMORY     -3       /* Memory allocation error */
#define LOG_ERROR_INVALID    -4       /* Invalid parameter */
#define LOG_ERROR_NOT_FOUND  -5       /* Resource not found */
#define LOG_ERROR_EXISTS     -6       /* Resource already exists */
#define LOG_ERROR_FULL       -7       /* Buffer full */
#define LOG_ERROR_EMPTY      -8       /* Buffer empty */
#define LOG_ERROR_IO         -9       /* I/O error */
#define LOG_ERROR_TIMEOUT    -10      /* Operation timeout */
#define LOG_ERROR_PERMISSION -11      /* Permission denied */
#define LOG_ERROR_FORMAT     -12      /* Format error */
#define LOG_ERROR_CHECKSUM   -13      /* Checksum error */
#define LOG_ERROR_TRUNCATED  -14      /* Message truncated */
#define LOG_ERROR_SHUTDOWN   -15      /* Logger shutdown */

/* ========================== Constants ========================== */

#define LOG_MESSAGE_MAGIC     0x4C4F4721  /* "LOG!" */
#define LOG_MAX_MESSAGE_SIZE  4096        /* Maximum message size */
#define LOG_MAX_BUFFERS       64          /* Maximum buffers */
#define LOG_MAX_OUTPUTS       32          /* Maximum outputs */
#define LOG_MAX_STACK_DEPTH   32          /* Maximum stack depth */
#define LOG_DEFAULT_BUFFER_SIZE 1048576   /* 1MB default buffer */
#define LOG_MIN_BUFFER_SIZE   4096        /* 4KB minimum buffer */
#define LOG_MAX_BUFFER_SIZE   67108864    /* 64MB maximum buffer */

#endif /* LOGGING_DEBUG_H */
