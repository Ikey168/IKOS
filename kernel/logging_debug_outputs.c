/* IKOS Logging & Debugging Service - Output Management
 * Handles various output destinations for log messages
 */

#include "../include/logging_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <time.h>
#include <pthread.h>

/* ========================== File Output Management ========================== */

typedef struct {
    FILE* file;
    char current_path[256];
    uint64_t current_size;
    uint32_t file_index;
    bool needs_rotation;
    pthread_mutex_t mutex;
} file_output_context_t;

static int rotate_log_file(file_output_context_t* ctx, const log_file_config_t* config) {
    if (!ctx || !config) {
        return LOG_ERROR_INVALID;
    }
    
    /* Close current file */
    if (ctx->file) {
        fclose(ctx->file);
        ctx->file = NULL;
    }
    
    /* Generate rotated filename */
    char rotated_path[512];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    snprintf(rotated_path, sizeof(rotated_path), "%s.%04d%02d%02d_%02d%02d%02d.%u",
             config->path, tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, ctx->file_index++);
    
    /* Rename current file */
    if (access(ctx->current_path, F_OK) == 0) {
        if (rename(ctx->current_path, rotated_path) != 0) {
            return LOG_ERROR_IO;
        }
        
        /* Compress if requested */
        if (config->compress) {
            char compress_cmd[1024];
            snprintf(compress_cmd, sizeof(compress_cmd), "gzip '%s' &", rotated_path);
            system(compress_cmd);
        }
    }
    
    /* Remove old files if max_files is set */
    if (config->max_files > 0) {
        /* This is a simplified implementation - in practice, you'd scan the directory */
        /* and remove the oldest files to keep only max_files */
    }
    
    /* Open new file */
    ctx->file = fopen(ctx->current_path, "w");
    if (!ctx->file) {
        return LOG_ERROR_IO;
    }
    
    /* Set permissions */
    if (chmod(ctx->current_path, config->permissions) != 0) {
        /* Non-fatal error */
    }
    
    ctx->current_size = 0;
    ctx->needs_rotation = false;
    
    return LOG_SUCCESS;
}

static int file_output_handler(const log_message_t* message, void* context) {
    log_output_t* output = (log_output_t*)context;
    if (!output || !message) {
        return LOG_ERROR_INVALID;
    }
    
    file_output_context_t* file_ctx = (file_output_context_t*)output->context;
    if (!file_ctx) {
        return LOG_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&file_ctx->mutex);
    
    /* Check if rotation is needed */
    if (file_ctx->needs_rotation || 
        (output->type_config.file.max_size > 0 && 
         file_ctx->current_size >= output->type_config.file.max_size)) {
        
        int ret = rotate_log_file(file_ctx, &output->type_config.file);
        if (ret != LOG_SUCCESS) {
            pthread_mutex_unlock(&file_ctx->mutex);
            return ret;
        }
    }
    
    /* Format and write message */
    char buffer[2048];
    int ret = log_format_message(message, buffer, sizeof(buffer));
    if (ret != LOG_SUCCESS && ret != LOG_ERROR_TRUNCATED) {
        pthread_mutex_unlock(&file_ctx->mutex);
        return ret;
    }
    
    size_t len = strlen(buffer);
    if (file_ctx->file && fwrite(buffer, 1, len, file_ctx->file) == len) {
        file_ctx->current_size += len;
        
        /* Sync periodically */
        static uint32_t write_count = 0;
        if (++write_count >= output->type_config.file.sync_interval) {
            fflush(file_ctx->file);
            fsync(fileno(file_ctx->file));
            write_count = 0;
        }
        
        pthread_mutex_unlock(&file_ctx->mutex);
        return LOG_SUCCESS;
    }
    
    pthread_mutex_unlock(&file_ctx->mutex);
    return LOG_ERROR_IO;
}

/* ========================== Serial Output Management ========================== */

typedef struct {
    int fd;
    struct termios original_termios;
    bool port_configured;
    pthread_mutex_t mutex;
} serial_output_context_t;

static int configure_serial_port(int fd, const log_serial_config_t* config) {
    struct termios tty;
    
    if (tcgetattr(fd, &tty) != 0) {
        return LOG_ERROR_IO;
    }
    
    /* Configure baud rate */
    speed_t baud;
    switch (config->baud_rate) {
        case 9600:   baud = B9600;   break;
        case 19200:  baud = B19200;  break;
        case 38400:  baud = B38400;  break;
        case 57600:  baud = B57600;  break;
        case 115200: baud = B115200; break;
        default:     baud = B9600;   break;
    }
    
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);
    
    /* Configure data bits */
    tty.c_cflag &= ~CSIZE;
    switch (config->data_bits) {
        case 5: tty.c_cflag |= CS5; break;
        case 6: tty.c_cflag |= CS6; break;
        case 7: tty.c_cflag |= CS7; break;
        case 8: tty.c_cflag |= CS8; break;
        default: tty.c_cflag |= CS8; break;
    }
    
    /* Configure stop bits */
    if (config->stop_bits == 2) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }
    
    /* Configure parity */
    switch (config->parity) {
        case 'E':
        case 'e':
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        case 'O':
        case 'o':
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
            break;
        default:
            tty.c_cflag &= ~PARENB;
            break;
    }
    
    /* Configure flow control */
    if (config->flow_control) {
        tty.c_cflag |= CRTSCTS;
    } else {
        tty.c_cflag &= ~CRTSCTS;
    }
    
    /* Other flags */
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    
    /* Set timeouts */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = config->timeout_ms / 100; /* Convert to deciseconds */
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return LOG_ERROR_IO;
    }
    
    return LOG_SUCCESS;
}

static int serial_output_handler(const log_message_t* message, void* context) {
    log_output_t* output = (log_output_t*)context;
    if (!output || !message) {
        return LOG_ERROR_INVALID;
    }
    
    serial_output_context_t* serial_ctx = (serial_output_context_t*)output->context;
    if (!serial_ctx || serial_ctx->fd < 0) {
        return LOG_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&serial_ctx->mutex);
    
    /* Format message */
    char buffer[1024]; /* Smaller buffer for serial output */
    int ret = log_format_message(message, buffer, sizeof(buffer));
    if (ret != LOG_SUCCESS && ret != LOG_ERROR_TRUNCATED) {
        pthread_mutex_unlock(&serial_ctx->mutex);
        return ret;
    }
    
    /* Write to serial port */
    size_t len = strlen(buffer);
    ssize_t written = write(serial_ctx->fd, buffer, len);
    
    pthread_mutex_unlock(&serial_ctx->mutex);
    
    if (written != (ssize_t)len) {
        return LOG_ERROR_IO;
    }
    
    /* Force immediate transmission */
    tcdrain(serial_ctx->fd);
    
    return LOG_SUCCESS;
}

/* ========================== Network Output Management ========================== */

typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    bool connected;
    uint32_t retry_count;
    time_t last_retry;
    pthread_mutex_t mutex;
} network_output_context_t;

static int network_connect(network_output_context_t* net_ctx, const log_network_config_t* config) {
    /* Create socket */
    int protocol = (config->protocol == IPPROTO_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    net_ctx->socket_fd = socket(AF_INET, protocol, 0);
    if (net_ctx->socket_fd < 0) {
        return LOG_ERROR_IO;
    }
    
    /* Configure server address */
    memset(&net_ctx->server_addr, 0, sizeof(net_ctx->server_addr));
    net_ctx->server_addr.sin_family = AF_INET;
    net_ctx->server_addr.sin_port = htons(config->port);
    
    if (inet_pton(AF_INET, config->host, &net_ctx->server_addr.sin_addr) <= 0) {
        close(net_ctx->socket_fd);
        net_ctx->socket_fd = -1;
        return LOG_ERROR_INVALID;
    }
    
    /* Connect (for TCP) */
    if (config->protocol != IPPROTO_UDP) {
        if (connect(net_ctx->socket_fd, (struct sockaddr*)&net_ctx->server_addr,
                   sizeof(net_ctx->server_addr)) < 0) {
            close(net_ctx->socket_fd);
            net_ctx->socket_fd = -1;
            return LOG_ERROR_IO;
        }
    }
    
    net_ctx->connected = true;
    net_ctx->retry_count = 0;
    
    return LOG_SUCCESS;
}

static int network_output_handler(const log_message_t* message, void* context) {
    log_output_t* output = (log_output_t*)context;
    if (!output || !message) {
        return LOG_ERROR_INVALID;
    }
    
    network_output_context_t* net_ctx = (network_output_context_t*)output->context;
    if (!net_ctx) {
        return LOG_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&net_ctx->mutex);
    
    /* Check connection and retry if needed */
    if (!net_ctx->connected) {
        time_t now = time(NULL);
        if (now - net_ctx->last_retry >= output->type_config.network.retry_delay / 1000) {
            if (net_ctx->retry_count < output->type_config.network.retry_count) {
                network_connect(net_ctx, &output->type_config.network);
                net_ctx->retry_count++;
                net_ctx->last_retry = now;
            }
        }
        
        if (!net_ctx->connected) {
            pthread_mutex_unlock(&net_ctx->mutex);
            return LOG_ERROR_IO;
        }
    }
    
    /* Format message */
    char buffer[1024];
    int ret = log_format_message(message, buffer, sizeof(buffer));
    if (ret != LOG_SUCCESS && ret != LOG_ERROR_TRUNCATED) {
        pthread_mutex_unlock(&net_ctx->mutex);
        return ret;
    }
    
    /* Send message */
    size_t len = strlen(buffer);
    ssize_t sent;
    
    if (output->type_config.network.protocol == IPPROTO_UDP) {
        sent = sendto(net_ctx->socket_fd, buffer, len, 0,
                     (struct sockaddr*)&net_ctx->server_addr,
                     sizeof(net_ctx->server_addr));
    } else {
        sent = send(net_ctx->socket_fd, buffer, len, 0);
    }
    
    if (sent != (ssize_t)len) {
        net_ctx->connected = false;
        pthread_mutex_unlock(&net_ctx->mutex);
        return LOG_ERROR_IO;
    }
    
    pthread_mutex_unlock(&net_ctx->mutex);
    return LOG_SUCCESS;
}

/* ========================== Output Management API ========================== */

int logger_add_file_output(const char* path, log_level_t min_level,
                          const log_file_config_t* config) {
    if (!path || !config) {
        return LOG_ERROR_INVALID;
    }
    
    /* Create output configuration */
    log_output_config_t output_config = {
        .type = LOG_OUTPUT_FILE,
        .min_level = min_level,
        .max_level = LOG_LEVEL_EMERG,
        .facility_mask = 0xFFFFFFFF, /* All facilities */
        .flag_mask = 0xFFFF,         /* All flags */
        .enabled = true,
        .async = true,
        .buffer_size = 8192
    };
    strncpy(output_config.name, path, sizeof(output_config.name) - 1);
    
    /* Create output */
    log_output_t* output = log_output_create(&output_config);
    if (!output) {
        return LOG_ERROR_MEMORY;
    }
    
    /* Copy file configuration */
    output->type_config.file = *config;
    
    /* Create file context */
    file_output_context_t* file_ctx = malloc(sizeof(file_output_context_t));
    if (!file_ctx) {
        log_output_destroy(output);
        return LOG_ERROR_MEMORY;
    }
    
    memset(file_ctx, 0, sizeof(file_output_context_t));
    pthread_mutex_init(&file_ctx->mutex, NULL);
    
    strncpy(file_ctx->current_path, path, sizeof(file_ctx->current_path) - 1);
    
    /* Create directory if needed */
    char* dir_end = strrchr(file_ctx->current_path, '/');
    if (dir_end) {
        *dir_end = '\0';
        mkdir(file_ctx->current_path, 0755); /* Create directory */
        *dir_end = '/';
    }
    
    /* Open file */
    file_ctx->file = fopen(file_ctx->current_path, "a");
    if (!file_ctx->file) {
        pthread_mutex_destroy(&file_ctx->mutex);
        free(file_ctx);
        log_output_destroy(output);
        return LOG_ERROR_IO;
    }
    
    /* Set permissions */
    chmod(file_ctx->current_path, config->permissions);
    
    /* Set context and handler */
    output->context = file_ctx;
    output->handler = file_output_handler;
    
    /* Add to logger context - simplified implementation */
    /* In a complete implementation, this would add to the global output list */
    
    return LOG_SUCCESS;
}

int logger_add_serial_output(const char* device, log_level_t min_level,
                             const log_serial_config_t* config) {
    if (!device || !config) {
        return LOG_ERROR_INVALID;
    }
    
    /* Create output configuration */
    log_output_config_t output_config = {
        .type = LOG_OUTPUT_SERIAL,
        .min_level = min_level,
        .max_level = LOG_LEVEL_EMERG,
        .facility_mask = 0xFFFFFFFF,
        .flag_mask = 0xFFFF,
        .enabled = true,
        .async = false, /* Serial output should be synchronous */
        .buffer_size = 1024
    };
    strncpy(output_config.name, device, sizeof(output_config.name) - 1);
    
    /* Create output */
    log_output_t* output = log_output_create(&output_config);
    if (!output) {
        return LOG_ERROR_MEMORY;
    }
    
    /* Copy serial configuration */
    output->type_config.serial = *config;
    
    /* Create serial context */
    serial_output_context_t* serial_ctx = malloc(sizeof(serial_output_context_t));
    if (!serial_ctx) {
        log_output_destroy(output);
        return LOG_ERROR_MEMORY;
    }
    
    memset(serial_ctx, 0, sizeof(serial_output_context_t));
    pthread_mutex_init(&serial_ctx->mutex, NULL);
    
    /* Open serial device */
    serial_ctx->fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_ctx->fd < 0) {
        pthread_mutex_destroy(&serial_ctx->mutex);
        free(serial_ctx);
        log_output_destroy(output);
        return LOG_ERROR_IO;
    }
    
    /* Save original settings */
    if (tcgetattr(serial_ctx->fd, &serial_ctx->original_termios) == 0) {
        serial_ctx->port_configured = true;
    }
    
    /* Configure serial port */
    int ret = configure_serial_port(serial_ctx->fd, config);
    if (ret != LOG_SUCCESS) {
        close(serial_ctx->fd);
        pthread_mutex_destroy(&serial_ctx->mutex);
        free(serial_ctx);
        log_output_destroy(output);
        return ret;
    }
    
    /* Set context and handler */
    output->context = serial_ctx;
    output->handler = serial_output_handler;
    
    return LOG_SUCCESS;
}

int logger_add_network_output(const char* host, uint16_t port, log_level_t min_level,
                              const log_network_config_t* config) {
    if (!host || !config || port == 0) {
        return LOG_ERROR_INVALID;
    }
    
    /* Create output configuration */
    log_output_config_t output_config = {
        .type = LOG_OUTPUT_NETWORK,
        .min_level = min_level,
        .max_level = LOG_LEVEL_EMERG,
        .facility_mask = 0xFFFFFFFF,
        .flag_mask = 0xFFFF,
        .enabled = true,
        .async = true,
        .buffer_size = 4096
    };
    snprintf(output_config.name, sizeof(output_config.name), "%s:%u", host, port);
    
    /* Create output */
    log_output_t* output = log_output_create(&output_config);
    if (!output) {
        return LOG_ERROR_MEMORY;
    }
    
    /* Copy network configuration */
    output->type_config.network = *config;
    
    /* Create network context */
    network_output_context_t* net_ctx = malloc(sizeof(network_output_context_t));
    if (!net_ctx) {
        log_output_destroy(output);
        return LOG_ERROR_MEMORY;
    }
    
    memset(net_ctx, 0, sizeof(network_output_context_t));
    pthread_mutex_init(&net_ctx->mutex, NULL);
    net_ctx->socket_fd = -1;
    
    /* Try initial connection */
    network_connect(net_ctx, config);
    
    /* Set context and handler */
    output->context = net_ctx;
    output->handler = network_output_handler;
    
    return LOG_SUCCESS;
}

/* ========================== Output Cleanup ========================== */

void cleanup_file_output(file_output_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->file) {
        fflush(ctx->file);
        fclose(ctx->file);
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
}

void cleanup_serial_output(serial_output_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->fd >= 0) {
        /* Restore original settings */
        if (ctx->port_configured) {
            tcsetattr(ctx->fd, TCSANOW, &ctx->original_termios);
        }
        close(ctx->fd);
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
}

void cleanup_network_output(network_output_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
}
