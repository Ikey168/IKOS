/* IKOS System Daemon Management - Header File
 * Comprehensive daemon management, service registration, and IPC framework
 */

#ifndef DAEMON_SYSTEM_H
#define DAEMON_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <pthread.h>

/* ========================== Constants and Limits ========================== */

#define DAEMON_MAX_NAME 64
#define DAEMON_MAX_DESCRIPTION 256
#define DAEMON_MAX_EXECUTABLE 512
#define SERVICE_MAX_NAME 64
#define SERVICE_MAX_DESCRIPTION 256
#define SERVICE_MAX_VERSION 32
#define IPC_MAX_MESSAGE_SIZE 65536
#define IPC_MAX_TOPIC_NAME 128
#define DAEMON_MAX_DEPENDENCIES 16
#define DAEMON_MAX_ARGUMENTS 32
#define DAEMON_MAX_ENVIRONMENT 64
#define PATH_MAX 4096

/* ========================== Error Codes ========================== */

typedef enum {
    DAEMON_SUCCESS = 0,
    DAEMON_ERROR_INVALID = -1,
    DAEMON_ERROR_NOT_FOUND = -2,
    DAEMON_ERROR_ALREADY_EXISTS = -3,
    DAEMON_ERROR_PERMISSION = -4,
    DAEMON_ERROR_MEMORY = -5,
    DAEMON_ERROR_IO = -6,
    DAEMON_ERROR_TIMEOUT = -7,
    DAEMON_ERROR_BUSY = -8,
    DAEMON_ERROR_DEPENDENCY = -9,
    DAEMON_ERROR_RESOURCE_LIMIT = -10,
    DAEMON_ERROR_COMMUNICATION = -11,
    DAEMON_ERROR_AUTHENTICATION = -12,
    DAEMON_ERROR_CONFIGURATION = -13,
    DAEMON_ERROR_PROCESS = -14,
    DAEMON_ERROR_SIGNAL = -15
} daemon_error_t;

/* ========================== Daemon States and Types ========================== */

typedef enum {
    DAEMON_STATE_STOPPED = 0,
    DAEMON_STATE_STARTING = 1,
    DAEMON_STATE_RUNNING = 2,
    DAEMON_STATE_STOPPING = 3,
    DAEMON_STATE_FAILED = 4,
    DAEMON_STATE_RESTARTING = 5,
    DAEMON_STATE_UNKNOWN = 6
} daemon_state_t;

typedef enum {
    DAEMON_TYPE_SYSTEM = 0,      /* Core system services */
    DAEMON_TYPE_SERVICE = 1,     /* Application services */
    DAEMON_TYPE_MONITOR = 2,     /* Monitoring services */
    DAEMON_TYPE_USER = 3,        /* User-specific services */
    DAEMON_TYPE_TEMPORARY = 4    /* Temporary/one-shot services */
} daemon_type_t;

typedef enum {
    SERVICE_TYPE_LOGGER = 0,
    SERVICE_TYPE_NETWORK = 1,
    SERVICE_TYPE_DEVICE = 2,
    SERVICE_TYPE_AUTHENTICATION = 3,
    SERVICE_TYPE_FILE_SYSTEM = 4,
    SERVICE_TYPE_DATABASE = 5,
    SERVICE_TYPE_WEB_SERVER = 6,
    SERVICE_TYPE_CUSTOM = 7
} service_type_t;

typedef enum {
    RESTART_POLICY_NEVER = 0,
    RESTART_POLICY_ALWAYS = 1,
    RESTART_POLICY_ON_FAILURE = 2,
    RESTART_POLICY_UNLESS_STOPPED = 3
} restart_policy_t;

/* ========================== Resource Management ========================== */

typedef struct {
    uint64_t max_memory_bytes;      /* Maximum memory usage */
    uint32_t max_cpu_percent;       /* Maximum CPU usage percentage */
    uint32_t max_open_files;        /* Maximum open file descriptors */
    uint32_t max_processes;         /* Maximum child processes */
    uint32_t max_threads;           /* Maximum threads */
    uint64_t max_disk_io_bytes;     /* Maximum disk I/O per second */
    uint64_t max_network_io_bytes;  /* Maximum network I/O per second */
} resource_limits_t;

typedef struct {
    uint64_t memory_usage_bytes;    /* Current memory usage */
    uint32_t cpu_usage_percent;     /* Current CPU usage */
    uint32_t open_files_count;      /* Current open files */
    uint32_t process_count;         /* Current child processes */
    uint32_t thread_count;          /* Current threads */
    uint64_t disk_io_bytes;         /* Disk I/O since start */
    uint64_t network_io_bytes;      /* Network I/O since start */
    time_t last_update;             /* Last update timestamp */
} resource_usage_t;

/* ========================== Daemon Configuration ========================== */

typedef struct {
    char name[DAEMON_MAX_NAME];
    char description[DAEMON_MAX_DESCRIPTION];
    char executable[PATH_MAX];
    char working_directory[PATH_MAX];
    char pid_file[PATH_MAX];
    char log_file[PATH_MAX];
    char error_log_file[PATH_MAX];
    
    daemon_type_t type;
    restart_policy_t restart_policy;
    
    uid_t user_id;
    gid_t group_id;
    
    bool auto_start;                /* Start automatically on boot */
    bool auto_restart;              /* Restart on failure */
    uint32_t restart_delay_seconds; /* Delay between restarts */
    uint32_t max_restart_attempts;  /* Maximum restart attempts */
    uint32_t startup_timeout_seconds; /* Startup timeout */
    uint32_t shutdown_timeout_seconds; /* Shutdown timeout */
    
    resource_limits_t limits;
    
    /* Command line arguments */
    uint32_t argc;
    char* argv[DAEMON_MAX_ARGUMENTS];
    
    /* Environment variables */
    uint32_t env_count;
    char* env_vars[DAEMON_MAX_ENVIRONMENT];
    
    /* Dependencies */
    uint32_t dependency_count;
    char dependencies[DAEMON_MAX_DEPENDENCIES][DAEMON_MAX_NAME];
    
    /* Service configuration */
    bool provides_service;
    char service_name[SERVICE_MAX_NAME];
    service_type_t service_type;
} daemon_config_t;

/* ========================== Daemon Status and Statistics ========================== */

typedef struct {
    char name[DAEMON_MAX_NAME];
    daemon_state_t state;
    pid_t pid;
    pid_t parent_pid;
    
    time_t start_time;
    time_t last_restart_time;
    uint32_t restart_count;
    uint32_t failure_count;
    
    resource_usage_t resource_usage;
    
    int exit_code;
    char last_error[256];
    
    bool service_active;
    char service_endpoint[256];
    
    /* Performance metrics */
    uint64_t messages_processed;
    uint64_t bytes_processed;
    double average_response_time_ms;
    
    /* Health status */
    bool health_check_enabled;
    time_t last_health_check;
    bool health_status;
} daemon_status_t;

/* ========================== Service Registry ========================== */

typedef enum {
    ENDPOINT_TYPE_UNIX_SOCKET = 0,
    ENDPOINT_TYPE_TCP_SOCKET = 1,
    ENDPOINT_TYPE_UDP_SOCKET = 2,
    ENDPOINT_TYPE_SHARED_MEMORY = 3,
    ENDPOINT_TYPE_MESSAGE_QUEUE = 4,
    ENDPOINT_TYPE_PIPE = 5
} endpoint_type_t;

typedef struct {
    endpoint_type_t type;
    union {
        struct {
            char path[PATH_MAX];
            mode_t permissions;
        } unix_socket;
        
        struct {
            uint32_t address;   /* IPv4 address */
            uint16_t port;
            bool secure;        /* TLS/SSL enabled */
        } tcp_socket;
        
        struct {
            uint32_t address;   /* IPv4 address */
            uint16_t port;
        } udp_socket;
        
        struct {
            char name[64];
            size_t size;
            mode_t permissions;
        } shared_memory;
        
        struct {
            char name[64];
            size_t max_messages;
            size_t max_message_size;
        } message_queue;
        
        struct {
            char name[PATH_MAX];
            bool bidirectional;
        } pipe;
    } config;
} endpoint_info_t;

typedef uint32_t capability_flags_t;
#define CAPABILITY_READ         (1 << 0)
#define CAPABILITY_WRITE        (1 << 1)
#define CAPABILITY_EXECUTE      (1 << 2)
#define CAPABILITY_ADMIN        (1 << 3)
#define CAPABILITY_MONITOR      (1 << 4)
#define CAPABILITY_CONFIGURE    (1 << 5)
#define CAPABILITY_BROADCAST    (1 << 6)
#define CAPABILITY_ENCRYPT      (1 << 7)

typedef struct {
    char name[SERVICE_MAX_NAME];
    char description[SERVICE_MAX_DESCRIPTION];
    char version[SERVICE_MAX_VERSION];
    service_type_t type;
    endpoint_info_t endpoint;
    capability_flags_t capabilities;
    
    uint32_t daemon_pid;
    char daemon_name[DAEMON_MAX_NAME];
    
    bool active;
    time_t registration_time;
    time_t last_heartbeat;
    
    /* Service metadata */
    uint32_t max_clients;
    uint32_t current_clients;
    bool authentication_required;
    bool encryption_required;
    
    /* Performance metrics */
    uint64_t requests_handled;
    uint64_t bytes_transferred;
    double average_response_time_ms;
    
    /* Health monitoring */
    bool health_check_enabled;
    char health_check_endpoint[256];
    uint32_t health_check_interval_seconds;
} service_info_t;

/* ========================== Inter-Process Communication ========================== */

typedef uint32_t ipc_handle_t;

typedef enum {
    IPC_MESSAGE_REQUEST = 0,
    IPC_MESSAGE_RESPONSE = 1,
    IPC_MESSAGE_NOTIFICATION = 2,
    IPC_MESSAGE_BROADCAST = 3,
    IPC_MESSAGE_ERROR = 4,
    IPC_MESSAGE_HEARTBEAT = 5
} message_type_t;

typedef enum {
    IPC_PRIORITY_LOW = 0,
    IPC_PRIORITY_NORMAL = 1,
    IPC_PRIORITY_HIGH = 2,
    IPC_PRIORITY_URGENT = 3
} message_priority_t;

typedef struct {
    uint32_t message_id;
    uint32_t correlation_id;    /* For request-response correlation */
    uint32_t sender_pid;
    uint32_t receiver_pid;
    message_type_t type;
    message_priority_t priority;
    uint32_t payload_size;
    time_t timestamp;
    time_t expiry_time;
    bool requires_response;
    bool encrypted;
    uint8_t checksum[16];       /* Message integrity check */
    uint8_t payload[];
} ipc_message_t;

typedef struct {
    char topic[IPC_MAX_TOPIC_NAME];
    uint32_t subscriber_count;
    bool persistent;            /* Store messages for offline subscribers */
    bool ordered;              /* Maintain message order */
    uint32_t max_queue_size;   /* Maximum queued messages */
} topic_info_t;

typedef void (*ipc_callback_t)(const ipc_message_t* message, void* user_data);
typedef void (*daemon_event_callback_t)(const char* daemon_name, daemon_state_t old_state, 
                                       daemon_state_t new_state, void* user_data);

/* ========================== Health Monitoring ========================== */

typedef enum {
    HEALTH_STATUS_UNKNOWN = 0,
    HEALTH_STATUS_HEALTHY = 1,
    HEALTH_STATUS_WARNING = 2,
    HEALTH_STATUS_CRITICAL = 3,
    HEALTH_STATUS_FAILURE = 4
} health_status_t;

typedef struct {
    char daemon_name[DAEMON_MAX_NAME];
    health_status_t status;
    time_t timestamp;
    char message[256];
    
    /* Detailed metrics */
    double cpu_usage_percent;
    uint64_t memory_usage_bytes;
    uint32_t open_files_count;
    double response_time_ms;
    uint32_t error_count;
    
    /* Thresholds */
    double cpu_warning_threshold;
    double cpu_critical_threshold;
    uint64_t memory_warning_threshold;
    uint64_t memory_critical_threshold;
} health_report_t;

/* ========================== Configuration Management ========================== */

typedef struct {
    char section[64];
    char key[64];
    char value[256];
    char description[256];
    bool requires_restart;      /* Daemon restart required for change */
    bool runtime_configurable; /* Can be changed at runtime */
} config_entry_t;

typedef struct {
    char daemon_name[DAEMON_MAX_NAME];
    uint32_t entry_count;
    config_entry_t* entries;
    time_t last_modified;
    char config_file_path[PATH_MAX];
} daemon_configuration_t;

/* ========================== Core Daemon Management API ========================== */

/* Daemon lifecycle management */
int daemon_create(const daemon_config_t* config);
int daemon_destroy(const char* name);
int daemon_start(const char* name);
int daemon_stop(const char* name);
int daemon_restart(const char* name);
int daemon_reload_config(const char* name);
int daemon_send_signal(const char* name, int signal);

/* Daemon status and monitoring */
int daemon_get_status(const char* name, daemon_status_t* status);
int daemon_list_all(char*** daemon_names, uint32_t* count);
int daemon_wait_for_state(const char* name, daemon_state_t state, uint32_t timeout_seconds);
int daemon_is_running(const char* name, bool* running);

/* Daemon configuration */
int daemon_get_config(const char* name, daemon_config_t* config);
int daemon_set_config(const char* name, const daemon_config_t* config);
int daemon_add_dependency(const char* name, const char* dependency);
int daemon_remove_dependency(const char* name, const char* dependency);

/* Resource management */
int daemon_set_resource_limits(const char* name, const resource_limits_t* limits);
int daemon_get_resource_usage(const char* name, resource_usage_t* usage);
int daemon_monitor_resources(const char* name, bool enable);

/* ========================== Service Registry API ========================== */

/* Service registration */
int service_register(const char* daemon_name, const service_info_t* info);
int service_unregister(const char* service_name);
int service_update_info(const char* service_name, const service_info_t* info);

/* Service discovery */
int service_discover(const char* service_name, service_info_t* info);
int service_list_all(service_info_t** services, uint32_t* count);
int service_list_by_type(service_type_t type, service_info_t** services, uint32_t* count);
int service_find_by_capability(capability_flags_t capabilities, service_info_t** services, uint32_t* count);

/* Service health and monitoring */
int service_heartbeat(const char* service_name);
int service_health_check(const char* service_name, health_report_t* report);
int service_get_metrics(const char* service_name, service_info_t* metrics);

/* ========================== Inter-Process Communication API ========================== */

/* IPC connection management */
int ipc_connect_to_service(const char* service_name, ipc_handle_t* handle);
int ipc_disconnect(ipc_handle_t handle);
int ipc_create_endpoint(const endpoint_info_t* endpoint, ipc_handle_t* handle);

/* Message passing */
int ipc_send_message(ipc_handle_t handle, const void* data, size_t size, message_type_t type);
int ipc_send_message_async(ipc_handle_t handle, const void* data, size_t size, message_type_t type);
int ipc_receive_message(ipc_handle_t handle, void* buffer, size_t buffer_size, size_t* received, uint32_t timeout_ms);
int ipc_send_request(ipc_handle_t handle, const void* request, size_t request_size, 
                    void* response, size_t response_buffer_size, size_t* response_size, uint32_t timeout_ms);

/* Publish-Subscribe system */
int ipc_create_topic(const char* topic_name, const topic_info_t* info);
int ipc_delete_topic(const char* topic_name);
int ipc_subscribe(const char* topic_name, ipc_callback_t callback, void* user_data);
int ipc_unsubscribe(const char* topic_name, ipc_callback_t callback);
int ipc_publish(const char* topic_name, const void* data, size_t size, message_priority_t priority);

/* Broadcast messaging */
int ipc_broadcast_to_type(service_type_t type, const void* data, size_t size);
int ipc_broadcast_to_all(const void* data, size_t size);

/* ========================== Health Monitoring API ========================== */

/* Health check management */
int health_register_daemon(const char* daemon_name, uint32_t check_interval_seconds);
int health_unregister_daemon(const char* daemon_name);
int health_check_daemon(const char* daemon_name, health_report_t* report);
int health_get_system_status(health_report_t** reports, uint32_t* count);

/* Threshold management */
int health_set_cpu_thresholds(const char* daemon_name, double warning, double critical);
int health_set_memory_thresholds(const char* daemon_name, uint64_t warning, uint64_t critical);
int health_set_response_time_threshold(const char* daemon_name, double warning_ms, double critical_ms);

/* Alert management */
int health_register_alert_callback(daemon_event_callback_t callback, void* user_data);
int health_unregister_alert_callback(daemon_event_callback_t callback);

/* ========================== Configuration Management API ========================== */

/* Configuration loading and saving */
int config_load_daemon_config(const char* config_file, daemon_config_t* config);
int config_save_daemon_config(const char* config_file, const daemon_config_t* config);
int config_reload_all_daemons(void);

/* Runtime configuration */
int config_get_value(const char* daemon_name, const char* section, const char* key, char* value, size_t value_size);
int config_set_value(const char* daemon_name, const char* section, const char* key, const char* value);
int config_list_entries(const char* daemon_name, config_entry_t** entries, uint32_t* count);

/* ========================== System Management API ========================== */

/* System initialization and shutdown */
int daemon_system_init(void);
int daemon_system_shutdown(void);
int daemon_system_start_all(void);
int daemon_system_stop_all(void);

/* Dependency management */
int daemon_resolve_dependencies(const char* daemon_name, char*** start_order, uint32_t* count);
int daemon_check_circular_dependencies(bool* has_circular);
int daemon_get_dependency_graph(char*** dependencies, uint32_t* count);

/* Statistics and reporting */
int daemon_get_system_statistics(uint32_t* total_daemons, uint32_t* running_daemons, 
                                uint32_t* failed_daemons, uint32_t* total_services);
int daemon_export_status_report(const char* output_file);

/* ========================== Event System ========================== */

typedef enum {
    DAEMON_EVENT_STARTED = 0,
    DAEMON_EVENT_STOPPED = 1,
    DAEMON_EVENT_FAILED = 2,
    DAEMON_EVENT_RESTARTED = 3,
    DAEMON_EVENT_CONFIG_CHANGED = 4,
    DAEMON_EVENT_RESOURCE_WARNING = 5,
    DAEMON_EVENT_RESOURCE_CRITICAL = 6,
    DAEMON_EVENT_SERVICE_REGISTERED = 7,
    DAEMON_EVENT_SERVICE_UNREGISTERED = 8,
    DAEMON_EVENT_IPC_ERROR = 9
} daemon_event_type_t;

typedef struct {
    daemon_event_type_t type;
    char daemon_name[DAEMON_MAX_NAME];
    time_t timestamp;
    char message[256];
    void* data;
    size_t data_size;
} daemon_event_t;

/* Event management */
int daemon_register_event_handler(daemon_event_type_t type, daemon_event_callback_t callback, void* user_data);
int daemon_unregister_event_handler(daemon_event_type_t type, daemon_event_callback_t callback);
int daemon_emit_event(const daemon_event_t* event);

/* ========================== Utility Functions ========================== */

/* String conversion utilities */
const char* daemon_state_to_string(daemon_state_t state);
const char* daemon_type_to_string(daemon_type_t type);
const char* service_type_to_string(service_type_t type);
const char* health_status_to_string(health_status_t status);

daemon_state_t daemon_state_from_string(const char* state_str);
daemon_type_t daemon_type_from_string(const char* type_str);
service_type_t service_type_from_string(const char* type_str);

/* Validation utilities */
int daemon_validate_config(const daemon_config_t* config);
int daemon_validate_name(const char* name);
int service_validate_info(const service_info_t* info);

/* File and path utilities */
int daemon_create_pid_file(const char* daemon_name, pid_t pid);
int daemon_remove_pid_file(const char* daemon_name);
int daemon_read_pid_file(const char* daemon_name, pid_t* pid);

/* Logging integration */
int daemon_log_event(const char* daemon_name, const char* level, const char* message);
int daemon_log_error(const char* daemon_name, const char* error_message);
int daemon_log_info(const char* daemon_name, const char* info_message);

#endif /* DAEMON_SYSTEM_H */
