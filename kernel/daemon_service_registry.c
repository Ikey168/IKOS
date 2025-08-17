/* IKOS System Daemon Management - Service Registry
 * Service registration, discovery, and health monitoring
 */

#include "../include/daemon_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

/* ========================== Service Registry State ========================== */

typedef struct service_entry {
    service_info_t info;
    bool active;
    time_t last_heartbeat;
    uint32_t client_count;
    struct service_entry* next;
} service_entry_t;

static service_entry_t* service_registry = NULL;
static pthread_mutex_t registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool registry_initialized = false;
static pthread_t heartbeat_monitor_thread;
static bool heartbeat_monitoring = false;

/* Event callback system */
typedef struct event_callback {
    daemon_event_callback_t callback;
    void* user_data;
    struct event_callback* next;
} event_callback_t;

static event_callback_t* event_callbacks = NULL;
static pthread_mutex_t callback_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ========================== Internal Helper Functions ========================== */

static service_entry_t* find_service_by_name(const char* name) {
    service_entry_t* current = service_registry;
    while (current) {
        if (strcmp(current->info.name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void emit_service_event(daemon_event_type_t type, const char* service_name, const char* message) {
    daemon_event_t event;
    event.type = type;
    strncpy(event.daemon_name, service_name, sizeof(event.daemon_name) - 1);
    event.timestamp = time(NULL);
    strncpy(event.message, message, sizeof(event.message) - 1);
    event.data = NULL;
    event.data_size = 0;
    
    pthread_mutex_lock(&callback_mutex);
    
    event_callback_t* current = event_callbacks;
    while (current) {
        current->callback(service_name, DAEMON_STATE_RUNNING, DAEMON_STATE_RUNNING, current->user_data);
        current = current->next;
    }
    
    pthread_mutex_unlock(&callback_mutex);
}

static int create_unix_socket_endpoint(const char* path, mode_t permissions) {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    
    /* Remove existing socket file */
    unlink(path);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    /* Set permissions */
    if (chmod(path, permissions) < 0) {
        close(sockfd);
        unlink(path);
        return -1;
    }
    
    if (listen(sockfd, 10) < 0) {
        close(sockfd);
        unlink(path);
        return -1;
    }
    
    return sockfd;
}

static int create_tcp_socket_endpoint(uint32_t address, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sockfd);
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(address);
    addr.sin_port = htons(port);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    if (listen(sockfd, 10) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

static void* heartbeat_monitor_func(void* arg) {
    (void)arg;
    
    while (heartbeat_monitoring) {
        time_t current_time = time(NULL);
        
        pthread_mutex_lock(&registry_mutex);
        
        service_entry_t* current = service_registry;
        while (current) {
            if (current->active) {
                /* Check if heartbeat is overdue */
                time_t heartbeat_age = current_time - current->last_heartbeat;
                
                if (heartbeat_age > 60) {  /* 60 seconds timeout */
                    current->active = false;
                    current->info.active = false;
                    
                    emit_service_event(DAEMON_EVENT_SERVICE_UNREGISTERED, 
                                     current->info.name, "Service heartbeat timeout");
                }
            }
            current = current->next;
        }
        
        pthread_mutex_unlock(&registry_mutex);
        
        sleep(10);  /* Check every 10 seconds */
    }
    
    return NULL;
}

/* ========================== Service Registry Initialization ========================== */

static int service_registry_init(void) {
    if (registry_initialized) {
        return DAEMON_SUCCESS;
    }
    
    /* Start heartbeat monitoring thread */
    heartbeat_monitoring = true;
    if (pthread_create(&heartbeat_monitor_thread, NULL, heartbeat_monitor_func, NULL) != 0) {
        heartbeat_monitoring = false;
        return DAEMON_ERROR_PROCESS;
    }
    
    registry_initialized = true;
    return DAEMON_SUCCESS;
}

static void service_registry_cleanup(void) {
    if (!registry_initialized) {
        return;
    }
    
    /* Stop heartbeat monitoring */
    heartbeat_monitoring = false;
    pthread_join(heartbeat_monitor_thread, NULL);
    
    /* Free service registry */
    pthread_mutex_lock(&registry_mutex);
    
    service_entry_t* current = service_registry;
    while (current) {
        service_entry_t* next = current->next;
        
        /* Clean up endpoint if it's a socket file */
        if (current->info.endpoint.type == ENDPOINT_TYPE_UNIX_SOCKET) {
            unlink(current->info.endpoint.config.unix_socket.path);
        }
        
        free(current);
        current = next;
    }
    
    service_registry = NULL;
    
    pthread_mutex_unlock(&registry_mutex);
    
    /* Free event callbacks */
    pthread_mutex_lock(&callback_mutex);
    
    event_callback_t* callback = event_callbacks;
    while (callback) {
        event_callback_t* next = callback->next;
        free(callback);
        callback = next;
    }
    
    event_callbacks = NULL;
    
    pthread_mutex_unlock(&callback_mutex);
    
    registry_initialized = false;
}

/* ========================== Service Registration API ========================== */

int service_register(const char* daemon_name, const service_info_t* info) {
    if (!daemon_name || !info || !info->name[0]) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (service_registry_init() != DAEMON_SUCCESS) {
        return DAEMON_ERROR_PROCESS;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    /* Check if service already registered */
    service_entry_t* existing = find_service_by_name(info->name);
    if (existing) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_ALREADY_EXISTS;
    }
    
    /* Validate service info */
    if (service_validate_info(info) != DAEMON_SUCCESS) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_CONFIGURATION;
    }
    
    /* Create new service entry */
    service_entry_t* entry = calloc(1, sizeof(service_entry_t));
    if (!entry) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    entry->info = *info;
    entry->active = true;
    entry->last_heartbeat = time(NULL);
    entry->client_count = 0;
    
    /* Set registration time and daemon info */
    entry->info.registration_time = time(NULL);
    entry->info.last_heartbeat = entry->last_heartbeat;
    entry->info.active = true;
    strncpy(entry->info.daemon_name, daemon_name, sizeof(entry->info.daemon_name) - 1);
    
    /* Add to registry */
    entry->next = service_registry;
    service_registry = entry;
    
    pthread_mutex_unlock(&registry_mutex);
    
    emit_service_event(DAEMON_EVENT_SERVICE_REGISTERED, info->name, "Service registered");
    
    return DAEMON_SUCCESS;
}

int service_unregister(const char* service_name) {
    if (!service_name) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    service_entry_t* prev = NULL;
    service_entry_t* current = service_registry;
    
    while (current) {
        if (strcmp(current->info.name, service_name) == 0) {
            /* Remove from list */
            if (prev) {
                prev->next = current->next;
            } else {
                service_registry = current->next;
            }
            
            /* Clean up endpoint */
            if (current->info.endpoint.type == ENDPOINT_TYPE_UNIX_SOCKET) {
                unlink(current->info.endpoint.config.unix_socket.path);
            }
            
            free(current);
            
            pthread_mutex_unlock(&registry_mutex);
            
            emit_service_event(DAEMON_EVENT_SERVICE_UNREGISTERED, service_name, "Service unregistered");
            
            return DAEMON_SUCCESS;
        }
        
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_ERROR_NOT_FOUND;
}

int service_update_info(const char* service_name, const service_info_t* info) {
    if (!service_name || !info) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    service_entry_t* entry = find_service_by_name(service_name);
    if (!entry) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    /* Update service info (preserve registration data) */
    time_t reg_time = entry->info.registration_time;
    char daemon_name[DAEMON_MAX_NAME];
    strncpy(daemon_name, entry->info.daemon_name, sizeof(daemon_name) - 1);
    
    entry->info = *info;
    entry->info.registration_time = reg_time;
    strncpy(entry->info.daemon_name, daemon_name, sizeof(entry->info.daemon_name) - 1);
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_SUCCESS;
}

/* ========================== Service Discovery API ========================== */

int service_discover(const char* service_name, service_info_t* info) {
    if (!service_name || !info) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    service_entry_t* entry = find_service_by_name(service_name);
    if (!entry || !entry->active) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    *info = entry->info;
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_SUCCESS;
}

int service_list_all(service_info_t** services, uint32_t* count) {
    if (!services || !count) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    /* Count active services */
    uint32_t service_count = 0;
    service_entry_t* current = service_registry;
    while (current) {
        if (current->active) {
            service_count++;
        }
        current = current->next;
    }
    
    if (service_count == 0) {
        *services = NULL;
        *count = 0;
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_SUCCESS;
    }
    
    /* Allocate array */
    service_info_t* result = malloc(service_count * sizeof(service_info_t));
    if (!result) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    /* Copy service info */
    uint32_t index = 0;
    current = service_registry;
    while (current && index < service_count) {
        if (current->active) {
            result[index] = current->info;
            index++;
        }
        current = current->next;
    }
    
    *services = result;
    *count = service_count;
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_SUCCESS;
}

int service_list_by_type(service_type_t type, service_info_t** services, uint32_t* count) {
    if (!services || !count) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    /* Count services of specified type */
    uint32_t service_count = 0;
    service_entry_t* current = service_registry;
    while (current) {
        if (current->active && current->info.type == type) {
            service_count++;
        }
        current = current->next;
    }
    
    if (service_count == 0) {
        *services = NULL;
        *count = 0;
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_SUCCESS;
    }
    
    /* Allocate array */
    service_info_t* result = malloc(service_count * sizeof(service_info_t));
    if (!result) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    /* Copy matching service info */
    uint32_t index = 0;
    current = service_registry;
    while (current && index < service_count) {
        if (current->active && current->info.type == type) {
            result[index] = current->info;
            index++;
        }
        current = current->next;
    }
    
    *services = result;
    *count = service_count;
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_SUCCESS;
}

int service_find_by_capability(capability_flags_t capabilities, service_info_t** services, uint32_t* count) {
    if (!services || !count) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    /* Count services with required capabilities */
    uint32_t service_count = 0;
    service_entry_t* current = service_registry;
    while (current) {
        if (current->active && (current->info.capabilities & capabilities) == capabilities) {
            service_count++;
        }
        current = current->next;
    }
    
    if (service_count == 0) {
        *services = NULL;
        *count = 0;
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_SUCCESS;
    }
    
    /* Allocate array */
    service_info_t* result = malloc(service_count * sizeof(service_info_t));
    if (!result) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    /* Copy matching service info */
    uint32_t index = 0;
    current = service_registry;
    while (current && index < service_count) {
        if (current->active && (current->info.capabilities & capabilities) == capabilities) {
            result[index] = current->info;
            index++;
        }
        current = current->next;
    }
    
    *services = result;
    *count = service_count;
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_SUCCESS;
}

/* ========================== Service Health Monitoring ========================== */

int service_heartbeat(const char* service_name) {
    if (!service_name) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    service_entry_t* entry = find_service_by_name(service_name);
    if (!entry) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    entry->last_heartbeat = time(NULL);
    entry->info.last_heartbeat = entry->last_heartbeat;
    entry->active = true;
    entry->info.active = true;
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_SUCCESS;
}

int service_health_check(const char* service_name, health_report_t* report) {
    if (!service_name || !report) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    service_entry_t* entry = find_service_by_name(service_name);
    if (!entry) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    /* Fill basic health report */
    memset(report, 0, sizeof(health_report_t));
    strncpy(report->daemon_name, service_name, sizeof(report->daemon_name) - 1);
    report->timestamp = time(NULL);
    
    time_t heartbeat_age = report->timestamp - entry->last_heartbeat;
    
    if (!entry->active || heartbeat_age > 60) {
        report->status = HEALTH_STATUS_CRITICAL;
        snprintf(report->message, sizeof(report->message), 
                "Service inactive or heartbeat overdue (%ld seconds)", heartbeat_age);
    } else if (heartbeat_age > 30) {
        report->status = HEALTH_STATUS_WARNING;
        snprintf(report->message, sizeof(report->message), 
                "Heartbeat delayed (%ld seconds)", heartbeat_age);
    } else {
        report->status = HEALTH_STATUS_HEALTHY;
        snprintf(report->message, sizeof(report->message), "Service healthy");
    }
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_SUCCESS;
}

int service_get_metrics(const char* service_name, service_info_t* metrics) {
    if (!service_name || !metrics) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&registry_mutex);
    
    service_entry_t* entry = find_service_by_name(service_name);
    if (!entry || !entry->active) {
        pthread_mutex_unlock(&registry_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    *metrics = entry->info;
    metrics->current_clients = entry->client_count;
    
    pthread_mutex_unlock(&registry_mutex);
    
    return DAEMON_SUCCESS;
}

/* ========================== Event Management ========================== */

int daemon_register_event_handler(daemon_event_type_t type, daemon_event_callback_t callback, void* user_data) {
    if (!callback) {
        return DAEMON_ERROR_INVALID;
    }
    
    event_callback_t* event_callback = malloc(sizeof(event_callback_t));
    if (!event_callback) {
        return DAEMON_ERROR_MEMORY;
    }
    
    event_callback->callback = callback;
    event_callback->user_data = user_data;
    
    pthread_mutex_lock(&callback_mutex);
    
    event_callback->next = event_callbacks;
    event_callbacks = event_callback;
    
    pthread_mutex_unlock(&callback_mutex);
    
    return DAEMON_SUCCESS;
}

int daemon_unregister_event_handler(daemon_event_type_t type, daemon_event_callback_t callback) {
    pthread_mutex_lock(&callback_mutex);
    
    event_callback_t* prev = NULL;
    event_callback_t* current = event_callbacks;
    
    while (current) {
        if (current->callback == callback) {
            if (prev) {
                prev->next = current->next;
            } else {
                event_callbacks = current->next;
            }
            
            free(current);
            
            pthread_mutex_unlock(&callback_mutex);
            return DAEMON_SUCCESS;
        }
        
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&callback_mutex);
    
    return DAEMON_ERROR_NOT_FOUND;
}

/* ========================== Utility Functions ========================== */

const char* service_type_to_string(service_type_t type) {
    switch (type) {
        case SERVICE_TYPE_LOGGER: return "logger";
        case SERVICE_TYPE_NETWORK: return "network";
        case SERVICE_TYPE_DEVICE: return "device";
        case SERVICE_TYPE_AUTHENTICATION: return "authentication";
        case SERVICE_TYPE_FILE_SYSTEM: return "filesystem";
        case SERVICE_TYPE_DATABASE: return "database";
        case SERVICE_TYPE_WEB_SERVER: return "webserver";
        case SERVICE_TYPE_CUSTOM: return "custom";
        default: return "unknown";
    }
}

service_type_t service_type_from_string(const char* type_str) {
    if (!type_str) return SERVICE_TYPE_CUSTOM;
    
    if (strcmp(type_str, "logger") == 0) return SERVICE_TYPE_LOGGER;
    if (strcmp(type_str, "network") == 0) return SERVICE_TYPE_NETWORK;
    if (strcmp(type_str, "device") == 0) return SERVICE_TYPE_DEVICE;
    if (strcmp(type_str, "authentication") == 0) return SERVICE_TYPE_AUTHENTICATION;
    if (strcmp(type_str, "filesystem") == 0) return SERVICE_TYPE_FILE_SYSTEM;
    if (strcmp(type_str, "database") == 0) return SERVICE_TYPE_DATABASE;
    if (strcmp(type_str, "webserver") == 0) return SERVICE_TYPE_WEB_SERVER;
    
    return SERVICE_TYPE_CUSTOM;
}

int service_validate_info(const service_info_t* info) {
    if (!info) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Validate service name */
    if (strlen(info->name) == 0 || strlen(info->name) >= SERVICE_MAX_NAME) {
        return DAEMON_ERROR_CONFIGURATION;
    }
    
    /* Validate endpoint configuration */
    switch (info->endpoint.type) {
        case ENDPOINT_TYPE_UNIX_SOCKET:
            if (strlen(info->endpoint.config.unix_socket.path) == 0) {
                return DAEMON_ERROR_CONFIGURATION;
            }
            break;
            
        case ENDPOINT_TYPE_TCP_SOCKET:
            if (info->endpoint.config.tcp_socket.port == 0) {
                return DAEMON_ERROR_CONFIGURATION;
            }
            break;
            
        case ENDPOINT_TYPE_UDP_SOCKET:
            if (info->endpoint.config.udp_socket.port == 0) {
                return DAEMON_ERROR_CONFIGURATION;
            }
            break;
            
        default:
            /* Other endpoint types are valid */
            break;
    }
    
    return DAEMON_SUCCESS;
}
