/* IKOS System Daemon Management - Core Implementation
 * Daemon process creation, lifecycle management, and monitoring
 */

#include "../include/daemon_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

/* ========================== Global State Management ========================== */

typedef struct daemon_instance {
    daemon_config_t config;
    daemon_status_t status;
    pid_t pid;
    time_t start_time;
    uint32_t restart_count;
    bool monitoring_enabled;
    pthread_t monitor_thread;
    struct daemon_instance* next;
} daemon_instance_t;

static daemon_instance_t* daemon_list = NULL;
static pthread_mutex_t daemon_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool daemon_system_initialized = false;
static pthread_t cleanup_thread;
static bool cleanup_thread_running = false;

/* ========================== Internal Helper Functions ========================== */

static daemon_instance_t* find_daemon_by_name(const char* name) {
    daemon_instance_t* current = daemon_list;
    while (current) {
        if (strcmp(current->config.name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static int create_daemon_directories(const daemon_config_t* config) {
    /* Create working directory */
    if (strlen(config->working_directory) > 0) {
        if (mkdir(config->working_directory, 0755) != 0 && errno != EEXIST) {
            return DAEMON_ERROR_IO;
        }
    }
    
    /* Create directory for PID file */
    char pid_dir[PATH_MAX];
    strncpy(pid_dir, config->pid_file, sizeof(pid_dir) - 1);
    char* last_slash = strrchr(pid_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (mkdir(pid_dir, 0755) != 0 && errno != EEXIST) {
            return DAEMON_ERROR_IO;
        }
    }
    
    /* Create directory for log files */
    if (strlen(config->log_file) > 0) {
        char log_dir[PATH_MAX];
        strncpy(log_dir, config->log_file, sizeof(log_dir) - 1);
        char* last_slash = strrchr(log_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (mkdir(log_dir, 0755) != 0 && errno != EEXIST) {
                return DAEMON_ERROR_IO;
            }
        }
    }
    
    return DAEMON_SUCCESS;
}

static int setup_daemon_stdio(const daemon_config_t* config) {
    /* Redirect stdin to /dev/null */
    int null_fd = open("/dev/null", O_RDONLY);
    if (null_fd < 0) {
        return DAEMON_ERROR_IO;
    }
    
    if (dup2(null_fd, STDIN_FILENO) < 0) {
        close(null_fd);
        return DAEMON_ERROR_IO;
    }
    close(null_fd);
    
    /* Redirect stdout to log file or /dev/null */
    int stdout_fd;
    if (strlen(config->log_file) > 0) {
        stdout_fd = open(config->log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    } else {
        stdout_fd = open("/dev/null", O_WRONLY);
    }
    
    if (stdout_fd < 0) {
        return DAEMON_ERROR_IO;
    }
    
    if (dup2(stdout_fd, STDOUT_FILENO) < 0) {
        close(stdout_fd);
        return DAEMON_ERROR_IO;
    }
    
    /* Redirect stderr to error log file or same as stdout */
    int stderr_fd;
    if (strlen(config->error_log_file) > 0) {
        stderr_fd = open(config->error_log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    } else {
        stderr_fd = dup(stdout_fd);
    }
    
    if (stderr_fd < 0) {
        close(stdout_fd);
        return DAEMON_ERROR_IO;
    }
    
    if (dup2(stderr_fd, STDERR_FILENO) < 0) {
        close(stdout_fd);
        close(stderr_fd);
        return DAEMON_ERROR_IO;
    }
    
    close(stdout_fd);
    close(stderr_fd);
    
    return DAEMON_SUCCESS;
}

static int apply_resource_limits(const resource_limits_t* limits) {
    struct rlimit rlim;
    
    /* Set memory limit */
    if (limits->max_memory_bytes > 0) {
        rlim.rlim_cur = limits->max_memory_bytes;
        rlim.rlim_max = limits->max_memory_bytes;
        if (setrlimit(RLIMIT_AS, &rlim) != 0) {
            return DAEMON_ERROR_RESOURCE_LIMIT;
        }
    }
    
    /* Set file descriptor limit */
    if (limits->max_open_files > 0) {
        rlim.rlim_cur = limits->max_open_files;
        rlim.rlim_max = limits->max_open_files;
        if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
            return DAEMON_ERROR_RESOURCE_LIMIT;
        }
    }
    
    /* Set process limit */
    if (limits->max_processes > 0) {
        rlim.rlim_cur = limits->max_processes;
        rlim.rlim_max = limits->max_processes;
        if (setrlimit(RLIMIT_NPROC, &rlim) != 0) {
            return DAEMON_ERROR_RESOURCE_LIMIT;
        }
    }
    
    return DAEMON_SUCCESS;
}

static pid_t fork_daemon_process(const daemon_config_t* config) {
    pid_t pid = fork();
    
    if (pid < 0) {
        return -1;  /* Fork failed */
    }
    
    if (pid == 0) {
        /* Child process - become daemon */
        
        /* Create new session */
        if (setsid() < 0) {
            exit(1);
        }
        
        /* Change working directory */
        if (strlen(config->working_directory) > 0) {
            if (chdir(config->working_directory) != 0) {
                exit(1);
            }
        }
        
        /* Set user and group */
        if (config->group_id != 0) {
            if (setgid(config->group_id) != 0) {
                exit(1);
            }
        }
        
        if (config->user_id != 0) {
            if (setuid(config->user_id) != 0) {
                exit(1);
            }
        }
        
        /* Apply resource limits */
        if (apply_resource_limits(&config->limits) != DAEMON_SUCCESS) {
            exit(1);
        }
        
        /* Setup stdio redirection */
        if (setup_daemon_stdio(config) != DAEMON_SUCCESS) {
            exit(1);
        }
        
        /* Set up environment variables */
        for (uint32_t i = 0; i < config->env_count; i++) {
            putenv(config->env_vars[i]);
        }
        
        /* Execute the daemon process */
        if (config->argc > 0) {
            execv(config->executable, config->argv);
        } else {
            char* argv[] = {(char*)config->executable, NULL};
            execv(config->executable, argv);
        }
        
        /* If we reach here, exec failed */
        exit(1);
    }
    
    /* Parent process */
    return pid;
}

static void* daemon_monitor_thread(void* arg) {
    daemon_instance_t* daemon = (daemon_instance_t*)arg;
    
    while (daemon->monitoring_enabled) {
        pthread_mutex_lock(&daemon_mutex);
        
        /* Check if process is still running */
        if (daemon->pid > 0) {
            int status;
            pid_t result = waitpid(daemon->pid, &status, WNOHANG);
            
            if (result == daemon->pid) {
                /* Process has exited */
                daemon->status.state = DAEMON_STATE_STOPPED;
                daemon->status.exit_code = WEXITSTATUS(status);
                daemon->pid = 0;
                
                /* Handle restart policy */
                if (daemon->config.auto_restart && 
                    daemon->restart_count < daemon->config.max_restart_attempts) {
                    
                    daemon->status.state = DAEMON_STATE_RESTARTING;
                    daemon->restart_count++;
                    
                    /* Wait for restart delay */
                    if (daemon->config.restart_delay_seconds > 0) {
                        sleep(daemon->config.restart_delay_seconds);
                    }
                    
                    /* Restart daemon */
                    pid_t new_pid = fork_daemon_process(&daemon->config);
                    if (new_pid > 0) {
                        daemon->pid = new_pid;
                        daemon->status.state = DAEMON_STATE_RUNNING;
                        daemon->status.last_restart_time = time(NULL);
                        daemon_create_pid_file(daemon->config.name, new_pid);
                    } else {
                        daemon->status.state = DAEMON_STATE_FAILED;
                        snprintf(daemon->status.last_error, sizeof(daemon->status.last_error),
                                "Failed to restart daemon");
                    }
                }
            } else if (result < 0 && errno != ECHILD) {
                /* Error checking process status */
                daemon->status.state = DAEMON_STATE_UNKNOWN;
                snprintf(daemon->status.last_error, sizeof(daemon->status.last_error),
                        "Error monitoring process: %s", strerror(errno));
            }
        }
        
        pthread_mutex_unlock(&daemon_mutex);
        
        /* Sleep for monitoring interval */
        sleep(1);
    }
    
    return NULL;
}

static void* cleanup_thread_func(void* arg) {
    (void)arg;
    
    while (cleanup_thread_running) {
        pthread_mutex_lock(&daemon_mutex);
        
        daemon_instance_t* current = daemon_list;
        while (current) {
            /* Clean up stopped daemons */
            if (current->status.state == DAEMON_STATE_STOPPED && current->pid == 0) {
                daemon_remove_pid_file(current->config.name);
            }
            
            /* Update resource usage statistics */
            if (current->pid > 0) {
                /* This would be implemented to read /proc/[pid]/stat and similar */
                /* For now, we'll just update timestamp */
                current->status.resource_usage.last_update = time(NULL);
            }
            
            current = current->next;
        }
        
        pthread_mutex_unlock(&daemon_mutex);
        
        /* Sleep for cleanup interval */
        sleep(10);
    }
    
    return NULL;
}

/* ========================== System Initialization ========================== */

int daemon_system_init(void) {
    pthread_mutex_lock(&daemon_mutex);
    
    if (daemon_system_initialized) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_SUCCESS;
    }
    
    /* Create necessary directories */
    mkdir("/var/run/daemons", 0755);
    mkdir("/var/log/daemons", 0755);
    mkdir("/etc/daemons", 0755);
    
    /* Start cleanup thread */
    cleanup_thread_running = true;
    if (pthread_create(&cleanup_thread, NULL, cleanup_thread_func, NULL) != 0) {
        cleanup_thread_running = false;
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_PROCESS;
    }
    
    daemon_system_initialized = true;
    pthread_mutex_unlock(&daemon_mutex);
    
    return DAEMON_SUCCESS;
}

int daemon_system_shutdown(void) {
    pthread_mutex_lock(&daemon_mutex);
    
    if (!daemon_system_initialized) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_SUCCESS;
    }
    
    /* Stop all daemons */
    daemon_instance_t* current = daemon_list;
    while (current) {
        if (current->status.state == DAEMON_STATE_RUNNING) {
            daemon_stop(current->config.name);
        }
        current = current->next;
    }
    
    /* Stop cleanup thread */
    cleanup_thread_running = false;
    pthread_mutex_unlock(&daemon_mutex);
    
    pthread_join(cleanup_thread, NULL);
    
    pthread_mutex_lock(&daemon_mutex);
    
    /* Free daemon list */
    current = daemon_list;
    while (current) {
        daemon_instance_t* next = current->next;
        
        /* Stop monitoring thread */
        current->monitoring_enabled = false;
        pthread_join(current->monitor_thread, NULL);
        
        /* Free configuration data */
        for (uint32_t i = 0; i < current->config.argc; i++) {
            free(current->config.argv[i]);
        }
        for (uint32_t i = 0; i < current->config.env_count; i++) {
            free(current->config.env_vars[i]);
        }
        
        free(current);
        current = next;
    }
    
    daemon_list = NULL;
    daemon_system_initialized = false;
    
    pthread_mutex_unlock(&daemon_mutex);
    
    return DAEMON_SUCCESS;
}

/* ========================== Daemon Lifecycle Management ========================== */

int daemon_create(const daemon_config_t* config) {
    if (!config || !config->name[0] || !config->executable[0]) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&daemon_mutex);
    
    if (!daemon_system_initialized) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_INVALID;
    }
    
    /* Check if daemon already exists */
    if (find_daemon_by_name(config->name) != NULL) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_ALREADY_EXISTS;
    }
    
    /* Validate configuration */
    if (daemon_validate_config(config) != DAEMON_SUCCESS) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_CONFIGURATION;
    }
    
    /* Create daemon instance */
    daemon_instance_t* daemon = calloc(1, sizeof(daemon_instance_t));
    if (!daemon) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_MEMORY;
    }
    
    /* Copy configuration */
    daemon->config = *config;
    
    /* Initialize status */
    strncpy(daemon->status.name, config->name, sizeof(daemon->status.name) - 1);
    daemon->status.state = DAEMON_STATE_STOPPED;
    daemon->status.pid = 0;
    daemon->status.start_time = 0;
    daemon->status.restart_count = 0;
    daemon->status.failure_count = 0;
    
    /* Create necessary directories */
    if (create_daemon_directories(config) != DAEMON_SUCCESS) {
        free(daemon);
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_IO;
    }
    
    /* Add to daemon list */
    daemon->next = daemon_list;
    daemon_list = daemon;
    
    pthread_mutex_unlock(&daemon_mutex);
    
    return DAEMON_SUCCESS;
}

int daemon_start(const char* name) {
    if (!name) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&daemon_mutex);
    
    daemon_instance_t* daemon = find_daemon_by_name(name);
    if (!daemon) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    if (daemon->status.state == DAEMON_STATE_RUNNING) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_SUCCESS;  /* Already running */
    }
    
    /* Check dependencies */
    for (uint32_t i = 0; i < daemon->config.dependency_count; i++) {
        daemon_instance_t* dep = find_daemon_by_name(daemon->config.dependencies[i]);
        if (!dep || dep->status.state != DAEMON_STATE_RUNNING) {
            pthread_mutex_unlock(&daemon_mutex);
            return DAEMON_ERROR_DEPENDENCY;
        }
    }
    
    daemon->status.state = DAEMON_STATE_STARTING;
    
    /* Fork daemon process */
    pid_t pid = fork_daemon_process(&daemon->config);
    if (pid < 0) {
        daemon->status.state = DAEMON_STATE_FAILED;
        daemon->status.failure_count++;
        snprintf(daemon->status.last_error, sizeof(daemon->status.last_error),
                "Failed to fork daemon process: %s", strerror(errno));
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_PROCESS;
    }
    
    daemon->pid = pid;
    daemon->status.pid = pid;
    daemon->status.state = DAEMON_STATE_RUNNING;
    daemon->status.start_time = time(NULL);
    daemon->restart_count = 0;
    
    /* Create PID file */
    daemon_create_pid_file(name, pid);
    
    /* Start monitoring thread */
    daemon->monitoring_enabled = true;
    if (pthread_create(&daemon->monitor_thread, NULL, daemon_monitor_thread, daemon) != 0) {
        daemon->monitoring_enabled = false;
        /* Continue anyway - monitoring is optional */
    }
    
    pthread_mutex_unlock(&daemon_mutex);
    
    return DAEMON_SUCCESS;
}

int daemon_stop(const char* name) {
    if (!name) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&daemon_mutex);
    
    daemon_instance_t* daemon = find_daemon_by_name(name);
    if (!daemon) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    if (daemon->status.state != DAEMON_STATE_RUNNING) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_SUCCESS;  /* Already stopped */
    }
    
    daemon->status.state = DAEMON_STATE_STOPPING;
    
    /* Send SIGTERM first */
    if (daemon->pid > 0) {
        kill(daemon->pid, SIGTERM);
        
        /* Wait for graceful shutdown */
        for (int i = 0; i < daemon->config.shutdown_timeout_seconds; i++) {
            int status;
            pid_t result = waitpid(daemon->pid, &status, WNOHANG);
            if (result == daemon->pid) {
                /* Process has exited */
                daemon->status.state = DAEMON_STATE_STOPPED;
                daemon->status.exit_code = WEXITSTATUS(status);
                daemon->pid = 0;
                break;
            }
            sleep(1);
        }
        
        /* If still running, send SIGKILL */
        if (daemon->pid > 0) {
            kill(daemon->pid, SIGKILL);
            waitpid(daemon->pid, NULL, 0);
            daemon->status.state = DAEMON_STATE_STOPPED;
            daemon->pid = 0;
        }
    }
    
    /* Stop monitoring */
    daemon->monitoring_enabled = false;
    
    /* Remove PID file */
    daemon_remove_pid_file(name);
    
    pthread_mutex_unlock(&daemon_mutex);
    
    return DAEMON_SUCCESS;
}

int daemon_restart(const char* name) {
    int ret = daemon_stop(name);
    if (ret != DAEMON_SUCCESS) {
        return ret;
    }
    
    /* Wait a moment for cleanup */
    sleep(1);
    
    return daemon_start(name);
}

int daemon_get_status(const char* name, daemon_status_t* status) {
    if (!name || !status) {
        return DAEMON_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&daemon_mutex);
    
    daemon_instance_t* daemon = find_daemon_by_name(name);
    if (!daemon) {
        pthread_mutex_unlock(&daemon_mutex);
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    *status = daemon->status;
    
    pthread_mutex_unlock(&daemon_mutex);
    
    return DAEMON_SUCCESS;
}

int daemon_is_running(const char* name, bool* running) {
    if (!name || !running) {
        return DAEMON_ERROR_INVALID;
    }
    
    daemon_status_t status;
    int ret = daemon_get_status(name, &status);
    if (ret != DAEMON_SUCCESS) {
        return ret;
    }
    
    *running = (status.state == DAEMON_STATE_RUNNING);
    return DAEMON_SUCCESS;
}

/* ========================== PID File Management ========================== */

int daemon_create_pid_file(const char* daemon_name, pid_t pid) {
    if (!daemon_name) {
        return DAEMON_ERROR_INVALID;
    }
    
    char pid_file_path[PATH_MAX];
    snprintf(pid_file_path, sizeof(pid_file_path), "/var/run/daemons/%s.pid", daemon_name);
    
    FILE* pid_file = fopen(pid_file_path, "w");
    if (!pid_file) {
        return DAEMON_ERROR_IO;
    }
    
    fprintf(pid_file, "%d\n", pid);
    fclose(pid_file);
    
    return DAEMON_SUCCESS;
}

int daemon_remove_pid_file(const char* daemon_name) {
    if (!daemon_name) {
        return DAEMON_ERROR_INVALID;
    }
    
    char pid_file_path[PATH_MAX];
    snprintf(pid_file_path, sizeof(pid_file_path), "/var/run/daemons/%s.pid", daemon_name);
    
    unlink(pid_file_path);  /* Ignore errors */
    
    return DAEMON_SUCCESS;
}

int daemon_read_pid_file(const char* daemon_name, pid_t* pid) {
    if (!daemon_name || !pid) {
        return DAEMON_ERROR_INVALID;
    }
    
    char pid_file_path[PATH_MAX];
    snprintf(pid_file_path, sizeof(pid_file_path), "/var/run/daemons/%s.pid", daemon_name);
    
    FILE* pid_file = fopen(pid_file_path, "r");
    if (!pid_file) {
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    if (fscanf(pid_file, "%d", pid) != 1) {
        fclose(pid_file);
        return DAEMON_ERROR_IO;
    }
    
    fclose(pid_file);
    return DAEMON_SUCCESS;
}

/* ========================== Utility Functions ========================== */

const char* daemon_state_to_string(daemon_state_t state) {
    switch (state) {
        case DAEMON_STATE_STOPPED: return "stopped";
        case DAEMON_STATE_STARTING: return "starting";
        case DAEMON_STATE_RUNNING: return "running";
        case DAEMON_STATE_STOPPING: return "stopping";
        case DAEMON_STATE_FAILED: return "failed";
        case DAEMON_STATE_RESTARTING: return "restarting";
        case DAEMON_STATE_UNKNOWN: return "unknown";
        default: return "invalid";
    }
}

const char* daemon_type_to_string(daemon_type_t type) {
    switch (type) {
        case DAEMON_TYPE_SYSTEM: return "system";
        case DAEMON_TYPE_SERVICE: return "service";
        case DAEMON_TYPE_MONITOR: return "monitor";
        case DAEMON_TYPE_USER: return "user";
        case DAEMON_TYPE_TEMPORARY: return "temporary";
        default: return "unknown";
    }
}

int daemon_validate_config(const daemon_config_t* config) {
    if (!config) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Validate name */
    if (daemon_validate_name(config->name) != DAEMON_SUCCESS) {
        return DAEMON_ERROR_CONFIGURATION;
    }
    
    /* Check if executable exists and is executable */
    if (access(config->executable, X_OK) != 0) {
        return DAEMON_ERROR_CONFIGURATION;
    }
    
    /* Validate timeout values */
    if (config->startup_timeout_seconds == 0) {
        return DAEMON_ERROR_CONFIGURATION;
    }
    
    if (config->shutdown_timeout_seconds == 0) {
        return DAEMON_ERROR_CONFIGURATION;
    }
    
    return DAEMON_SUCCESS;
}

int daemon_validate_name(const char* name) {
    if (!name || strlen(name) == 0 || strlen(name) >= DAEMON_MAX_NAME) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Check for valid characters (alphanumeric, underscore, hyphen) */
    for (const char* p = name; *p; p++) {
        if (!isalnum(*p) && *p != '_' && *p != '-') {
            return DAEMON_ERROR_INVALID;
        }
    }
    
    return DAEMON_SUCCESS;
}
