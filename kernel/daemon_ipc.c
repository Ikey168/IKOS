/* IKOS System Daemon Management - Inter-Process Communication
 * Message passing, publish-subscribe, and IPC endpoint management
 */

#include "../include/daemon_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>
#include <mqueue.h>

/* ========================== IPC System State ========================== */

typedef struct ipc_connection {
    ipc_handle_t handle;
    int socket_fd;
    endpoint_type_t type;
    char service_name[SERVICE_MAX_NAME];
    uint32_t local_pid;
    uint32_t remote_pid;
    bool authenticated;
    time_t last_activity;
    struct ipc_connection* next;
} ipc_connection_t;

typedef struct topic_subscription {
    char topic[IPC_MAX_TOPIC_NAME];
    ipc_callback_t callback;
    void* user_data;
    struct topic_subscription* next;
} topic_subscription_t;

typedef struct topic_entry {
    topic_info_t info;
    topic_subscription_t* subscribers;
    struct topic_entry* next;
} topic_entry_t;

typedef struct message_queue {
    ipc_message_t** messages;
    uint32_t capacity;
    uint32_t count;
    uint32_t head;
    uint32_t tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} message_queue_t;

static ipc_connection_t* connections = NULL;
static topic_entry_t* topics = NULL;
static pthread_mutex_t ipc_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool ipc_system_initialized = false;
static uint32_t next_handle = 1;
static uint32_t next_message_id = 1;
static pthread_t message_dispatcher_thread;
static bool message_dispatcher_running = false;

/* ========================== Internal Helper Functions ========================== */

static ipc_connection_t* find_connection_by_handle(ipc_handle_t handle) {
    ipc_connection_t* current = connections;
    while (current) {
        if (current->handle == handle) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static topic_entry_t* find_topic_by_name(const char* topic_name) {
    topic_entry_t* current = topics;
    while (current) {
        if (strcmp(current->info.topic, topic_name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static uint32_t generate_message_id(void) {
    return __sync_fetch_and_add(&next_message_id, 1);
}

static int send_raw_message(int socket_fd, const void* data, size_t size) {
    size_t sent = 0;
    const uint8_t* buffer = (const uint8_t*)data;
    
    while (sent < size) {
        ssize_t result = send(socket_fd, buffer + sent, size - sent, MSG_NOSIGNAL);
        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return DAEMON_ERROR_COMMUNICATION;
        }
        sent += result;
    }
    
    return DAEMON_SUCCESS;
}

static int receive_raw_message(int socket_fd, void* buffer, size_t size) {
    size_t received = 0;
    uint8_t* data = (uint8_t*)buffer;
    
    while (received < size) {
        ssize_t result = recv(socket_fd, data + received, size - received, 0);
        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return DAEMON_ERROR_COMMUNICATION;
        }
        if (result == 0) {
            return DAEMON_ERROR_COMMUNICATION;  /* Connection closed */
        }
        received += result;
    }
    
    return DAEMON_SUCCESS;
}

static void calculate_message_checksum(ipc_message_t* message) {
    /* Simple checksum calculation - in production, use a proper hash */
    uint32_t checksum = 0;
    uint8_t* data = (uint8_t*)message;
    size_t header_size = sizeof(ipc_message_t) - sizeof(message->payload);
    
    for (size_t i = 0; i < header_size; i++) {
        checksum += data[i];
    }
    
    for (uint32_t i = 0; i < message->payload_size; i++) {
        checksum += message->payload[i];
    }
    
    /* Store first 16 bytes of checksum */
    memcpy(message->checksum, &checksum, sizeof(uint32_t));
}

static bool verify_message_checksum(const ipc_message_t* message) {
    uint32_t calculated_checksum = 0;
    uint8_t* data = (uint8_t*)message;
    size_t header_size = sizeof(ipc_message_t) - sizeof(message->payload);
    
    for (size_t i = 0; i < header_size - sizeof(message->checksum); i++) {
        calculated_checksum += data[i];
    }
    
    for (uint32_t i = 0; i < message->payload_size; i++) {
        calculated_checksum += message->payload[i];
    }
    
    uint32_t stored_checksum;
    memcpy(&stored_checksum, message->checksum, sizeof(uint32_t));
    
    return calculated_checksum == stored_checksum;
}

static void* message_dispatcher_func(void* arg) {
    (void)arg;
    
    while (message_dispatcher_running) {
        pthread_mutex_lock(&ipc_mutex);
        
        /* Process incoming messages on all connections */
        ipc_connection_t* conn = connections;
        while (conn) {
            if (conn->socket_fd >= 0) {
                /* Check for incoming data */
                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(conn->socket_fd, &read_fds);
                
                struct timeval timeout = {0, 0};  /* Non-blocking */
                
                if (select(conn->socket_fd + 1, &read_fds, NULL, NULL, &timeout) > 0) {
                    if (FD_ISSET(conn->socket_fd, &read_fds)) {
                        /* Receive message header first */
                        ipc_message_t header;
                        if (receive_raw_message(conn->socket_fd, &header, 
                                              sizeof(ipc_message_t) - sizeof(header.payload)) == DAEMON_SUCCESS) {
                            
                            /* Validate message size */
                            if (header.payload_size <= IPC_MAX_MESSAGE_SIZE) {
                                /* Allocate complete message */
                                ipc_message_t* message = malloc(sizeof(ipc_message_t) + header.payload_size);
                                if (message) {
                                    *message = header;
                                    
                                    /* Receive payload */
                                    if (header.payload_size > 0) {
                                        if (receive_raw_message(conn->socket_fd, message->payload, 
                                                              header.payload_size) != DAEMON_SUCCESS) {
                                            free(message);
                                            continue;
                                        }
                                    }
                                    
                                    /* Verify checksum */
                                    if (verify_message_checksum(message)) {
                                        /* Update connection activity */
                                        conn->last_activity = time(NULL);
                                        
                                        /* Handle message based on type */
                                        if (message->type == IPC_MESSAGE_NOTIFICATION ||
                                            message->type == IPC_MESSAGE_BROADCAST) {
                                            /* Find topic subscribers */
                                            topic_entry_t* topic = topics;
                                            while (topic) {
                                                topic_subscription_t* sub = topic->subscribers;
                                                while (sub) {
                                                    sub->callback(message, sub->user_data);
                                                    sub = sub->next;
                                                }
                                                topic = topic->next;
                                            }
                                        }
                                    }
                                    
                                    free(message);
                                }
                            }
                        }
                    }
                }
            }
            conn = conn->next;
        }
        
        pthread_mutex_unlock(&ipc_mutex);
        
        usleep(10000);  /* 10ms */
    }
    
    return NULL;
}

/* ========================== IPC System Initialization ========================== */

static int ipc_system_init(void) {
    if (ipc_system_initialized) {
        return DAEMON_SUCCESS;
    }
    
    /* Create IPC directories */
    mkdir("/var/run/ipc", 0755);
    mkdir("/var/run/ipc/sockets", 0755);
    mkdir("/dev/mqueue", 0755);
    
    /* Start message dispatcher thread */
    message_dispatcher_running = true;
    if (pthread_create(&message_dispatcher_thread, NULL, message_dispatcher_func, NULL) != 0) {
        message_dispatcher_running = false;
        return DAEMON_ERROR_PROCESS;
    }
    
    ipc_system_initialized = true;
    return DAEMON_SUCCESS;
}

static void ipc_system_cleanup(void) {
    if (!ipc_system_initialized) {
        return;
    }
    
    /* Stop message dispatcher */
    message_dispatcher_running = false;
    pthread_join(message_dispatcher_thread, NULL);
    
    /* Close all connections */
    pthread_mutex_lock(&ipc_mutex);
    
    ipc_connection_t* conn = connections;
    while (conn) {
        ipc_connection_t* next = conn->next;
        
        if (conn->socket_fd >= 0) {
            close(conn->socket_fd);
        }
        
        free(conn);
        conn = next;
    }
    connections = NULL;
    
    /* Free topics and subscriptions */
    topic_entry_t* topic = topics;
    while (topic) {
        topic_entry_t* next_topic = topic->next;
        
        topic_subscription_t* sub = topic->subscribers;
        while (sub) {
            topic_subscription_t* next_sub = sub->next;
            free(sub);
            sub = next_sub;
        }
        
        free(topic);
        topic = next_topic;
    }
    topics = NULL;
    
    pthread_mutex_unlock(&ipc_mutex);
    
    ipc_system_initialized = false;
}

/* ========================== IPC Connection Management ========================== */

int ipc_connect_to_service(const char* service_name, ipc_handle_t* handle) {
    if (!service_name || !handle) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (ipc_system_init() != DAEMON_SUCCESS) {
        return DAEMON_ERROR_PROCESS;
    }
    
    /* Discover service */
    service_info_t service_info;
    int ret = service_discover(service_name, &service_info);
    if (ret != DAEMON_SUCCESS) {
        return ret;
    }
    
    /* Create connection based on endpoint type */
    int socket_fd = -1;
    
    switch (service_info.endpoint.type) {
        case ENDPOINT_TYPE_UNIX_SOCKET: {
            socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (socket_fd < 0) {
                return DAEMON_ERROR_COMMUNICATION;
            }
            
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, service_info.endpoint.config.unix_socket.path, 
                   sizeof(addr.sun_path) - 1);
            
            if (connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(socket_fd);
                return DAEMON_ERROR_COMMUNICATION;
            }
            break;
        }
        
        case ENDPOINT_TYPE_TCP_SOCKET: {
            socket_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (socket_fd < 0) {
                return DAEMON_ERROR_COMMUNICATION;
            }
            
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(service_info.endpoint.config.tcp_socket.address);
            addr.sin_port = htons(service_info.endpoint.config.tcp_socket.port);
            
            if (connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(socket_fd);
                return DAEMON_ERROR_COMMUNICATION;
            }
            break;
        }
        
        default:
            return DAEMON_ERROR_INVALID;
    }
    
    /* Create connection object */
    pthread_mutex_lock(&ipc_mutex);
    
    ipc_connection_t* connection = malloc(sizeof(ipc_connection_t));
    if (!connection) {
        close(socket_fd);
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    connection->handle = next_handle++;
    connection->socket_fd = socket_fd;
    connection->type = service_info.endpoint.type;
    strncpy(connection->service_name, service_name, sizeof(connection->service_name) - 1);
    connection->local_pid = getpid();
    connection->remote_pid = service_info.daemon_pid;
    connection->authenticated = false;
    connection->last_activity = time(NULL);
    
    /* Add to connections list */
    connection->next = connections;
    connections = connection;
    
    *handle = connection->handle;
    
    pthread_mutex_unlock(&ipc_mutex);
    
    return DAEMON_SUCCESS;
}

int ipc_disconnect(ipc_handle_t handle) {
    pthread_mutex_lock(&ipc_mutex);
    
    ipc_connection_t* prev = NULL;
    ipc_connection_t* current = connections;
    
    while (current) {
        if (current->handle == handle) {
            /* Remove from list */
            if (prev) {
                prev->next = current->next;
            } else {
                connections = current->next;
            }
            
            /* Close socket */
            if (current->socket_fd >= 0) {
                close(current->socket_fd);
            }
            
            free(current);
            
            pthread_mutex_unlock(&ipc_mutex);
            return DAEMON_SUCCESS;
        }
        
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&ipc_mutex);
    
    return DAEMON_ERROR_NOT_FOUND;
}

int ipc_create_endpoint(const endpoint_info_t* endpoint, ipc_handle_t* handle) {
    if (!endpoint || !handle) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (ipc_system_init() != DAEMON_SUCCESS) {
        return DAEMON_ERROR_PROCESS;
    }
    
    int socket_fd = -1;
    
    switch (endpoint->type) {
        case ENDPOINT_TYPE_UNIX_SOCKET: {
            socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (socket_fd < 0) {
                return DAEMON_ERROR_COMMUNICATION;
            }
            
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, endpoint->config.unix_socket.path, sizeof(addr.sun_path) - 1);
            
            /* Remove existing socket file */
            unlink(addr.sun_path);
            
            if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(socket_fd);
                return DAEMON_ERROR_COMMUNICATION;
            }
            
            /* Set permissions */
            if (chmod(addr.sun_path, endpoint->config.unix_socket.permissions) < 0) {
                close(socket_fd);
                unlink(addr.sun_path);
                return DAEMON_ERROR_PERMISSION;
            }
            
            if (listen(socket_fd, 10) < 0) {
                close(socket_fd);
                unlink(addr.sun_path);
                return DAEMON_ERROR_COMMUNICATION;
            }
            break;
        }
        
        case ENDPOINT_TYPE_TCP_SOCKET: {
            socket_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (socket_fd < 0) {
                return DAEMON_ERROR_COMMUNICATION;
            }
            
            int opt = 1;
            if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                close(socket_fd);
                return DAEMON_ERROR_COMMUNICATION;
            }
            
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(endpoint->config.tcp_socket.address);
            addr.sin_port = htons(endpoint->config.tcp_socket.port);
            
            if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(socket_fd);
                return DAEMON_ERROR_COMMUNICATION;
            }
            
            if (listen(socket_fd, 10) < 0) {
                close(socket_fd);
                return DAEMON_ERROR_COMMUNICATION;
            }
            break;
        }
        
        default:
            return DAEMON_ERROR_INVALID;
    }
    
    /* Create connection object */
    pthread_mutex_lock(&ipc_mutex);
    
    ipc_connection_t* connection = malloc(sizeof(ipc_connection_t));
    if (!connection) {
        close(socket_fd);
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    connection->handle = next_handle++;
    connection->socket_fd = socket_fd;
    connection->type = endpoint->type;
    connection->service_name[0] = '\0';
    connection->local_pid = getpid();
    connection->remote_pid = 0;
    connection->authenticated = false;
    connection->last_activity = time(NULL);
    
    /* Add to connections list */
    connection->next = connections;
    connections = connection;
    
    *handle = connection->handle;
    
    pthread_mutex_unlock(&ipc_mutex);
    
    return DAEMON_SUCCESS;
}

/* ========================== Message Passing ========================== */

int ipc_send_message(ipc_handle_t handle, const void* data, size_t size, message_type_t type) {
    if (!data || size == 0 || size > IPC_MAX_MESSAGE_SIZE) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&ipc_mutex);
    
    ipc_connection_t* conn = find_connection_by_handle(handle);
    if (!conn || conn->socket_fd < 0) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    /* Create message */
    ipc_message_t* message = malloc(sizeof(ipc_message_t) + size);
    if (!message) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    message->message_id = generate_message_id();
    message->correlation_id = 0;
    message->sender_pid = conn->local_pid;
    message->receiver_pid = conn->remote_pid;
    message->type = type;
    message->priority = IPC_PRIORITY_NORMAL;
    message->payload_size = size;
    message->timestamp = time(NULL);
    message->expiry_time = message->timestamp + 300;  /* 5 minutes */
    message->requires_response = false;
    message->encrypted = false;
    
    memcpy(message->payload, data, size);
    calculate_message_checksum(message);
    
    /* Send message */
    int ret = send_raw_message(conn->socket_fd, message, sizeof(ipc_message_t) + size);
    
    free(message);
    
    if (ret == DAEMON_SUCCESS) {
        conn->last_activity = time(NULL);
    }
    
    pthread_mutex_unlock(&ipc_mutex);
    
    return ret;
}

int ipc_receive_message(ipc_handle_t handle, void* buffer, size_t buffer_size, 
                       size_t* received, uint32_t timeout_ms) {
    if (!buffer || buffer_size == 0 || !received) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&ipc_mutex);
    
    ipc_connection_t* conn = find_connection_by_handle(handle);
    if (!conn || conn->socket_fd < 0) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    /* Set socket timeout */
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(conn->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_COMMUNICATION;
    }
    
    /* Receive message header */
    ipc_message_t header;
    int ret = receive_raw_message(conn->socket_fd, &header, 
                                sizeof(ipc_message_t) - sizeof(header.payload));
    
    if (ret != DAEMON_SUCCESS) {
        pthread_mutex_unlock(&ipc_mutex);
        return ret;
    }
    
    /* Validate message */
    if (header.payload_size > buffer_size) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    /* Receive payload */
    if (header.payload_size > 0) {
        ret = receive_raw_message(conn->socket_fd, buffer, header.payload_size);
        if (ret != DAEMON_SUCCESS) {
            pthread_mutex_unlock(&ipc_mutex);
            return ret;
        }
    }
    
    *received = header.payload_size;
    conn->last_activity = time(NULL);
    
    pthread_mutex_unlock(&ipc_mutex);
    
    return DAEMON_SUCCESS;
}

int ipc_send_request(ipc_handle_t handle, const void* request, size_t request_size,
                    void* response, size_t response_buffer_size, size_t* response_size, 
                    uint32_t timeout_ms) {
    /* Send request */
    int ret = ipc_send_message(handle, request, request_size, IPC_MESSAGE_REQUEST);
    if (ret != DAEMON_SUCCESS) {
        return ret;
    }
    
    /* Receive response */
    return ipc_receive_message(handle, response, response_buffer_size, response_size, timeout_ms);
}

/* ========================== Publish-Subscribe System ========================== */

int ipc_create_topic(const char* topic_name, const topic_info_t* info) {
    if (!topic_name || !info) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (ipc_system_init() != DAEMON_SUCCESS) {
        return DAEMON_ERROR_PROCESS;
    }
    
    pthread_mutex_lock(&ipc_mutex);
    
    /* Check if topic already exists */
    if (find_topic_by_name(topic_name) != NULL) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_ALREADY_EXISTS;
    }
    
    /* Create new topic */
    topic_entry_t* topic = malloc(sizeof(topic_entry_t));
    if (!topic) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    topic->info = *info;
    strncpy(topic->info.topic, topic_name, sizeof(topic->info.topic) - 1);
    topic->subscribers = NULL;
    
    /* Add to topics list */
    topic->next = topics;
    topics = topic;
    
    pthread_mutex_unlock(&ipc_mutex);
    
    return DAEMON_SUCCESS;
}

int ipc_subscribe(const char* topic_name, ipc_callback_t callback, void* user_data) {
    if (!topic_name || !callback) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&ipc_mutex);
    
    topic_entry_t* topic = find_topic_by_name(topic_name);
    if (!topic) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    /* Create subscription */
    topic_subscription_t* subscription = malloc(sizeof(topic_subscription_t));
    if (!subscription) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    strncpy(subscription->topic, topic_name, sizeof(subscription->topic) - 1);
    subscription->callback = callback;
    subscription->user_data = user_data;
    
    /* Add to topic's subscriber list */
    subscription->next = topic->subscribers;
    topic->subscribers = subscription;
    topic->info.subscriber_count++;
    
    pthread_mutex_unlock(&ipc_mutex);
    
    return DAEMON_SUCCESS;
}

int ipc_publish(const char* topic_name, const void* data, size_t size, message_priority_t priority) {
    if (!topic_name || !data || size == 0) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&ipc_mutex);
    
    topic_entry_t* topic = find_topic_by_name(topic_name);
    if (!topic) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    /* Create message for publication */
    ipc_message_t* message = malloc(sizeof(ipc_message_t) + size);
    if (!message) {
        pthread_mutex_unlock(&ipc_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    message->message_id = generate_message_id();
    message->correlation_id = 0;
    message->sender_pid = getpid();
    message->receiver_pid = 0;  /* Broadcast */
    message->type = IPC_MESSAGE_NOTIFICATION;
    message->priority = priority;
    message->payload_size = size;
    message->timestamp = time(NULL);
    message->expiry_time = message->timestamp + 300;
    message->requires_response = false;
    message->encrypted = false;
    
    memcpy(message->payload, data, size);
    calculate_message_checksum(message);
    
    /* Deliver to all subscribers */
    topic_subscription_t* subscription = topic->subscribers;
    while (subscription) {
        subscription->callback(message, subscription->user_data);
        subscription = subscription->next;
    }
    
    free(message);
    
    pthread_mutex_unlock(&ipc_mutex);
    
    return DAEMON_SUCCESS;
}

/* ========================== Utility Functions ========================== */

const char* health_status_to_string(health_status_t status) {
    switch (status) {
        case HEALTH_STATUS_UNKNOWN: return "unknown";
        case HEALTH_STATUS_HEALTHY: return "healthy";
        case HEALTH_STATUS_WARNING: return "warning";
        case HEALTH_STATUS_CRITICAL: return "critical";
        case HEALTH_STATUS_FAILURE: return "failure";
        default: return "invalid";
    }
}
