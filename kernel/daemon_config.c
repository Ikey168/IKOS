/* IKOS System Daemon Management - Configuration Management
 * Configuration loading, validation, and management utilities
 */

#include "../include/daemon_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

/* ========================== Configuration Constants ========================== */

#define CONFIG_LINE_MAX 1024
#define CONFIG_MAX_SECTIONS 64
#define CONFIG_MAX_KEYS_PER_SECTION 128

static const char* CONFIG_DIR = "/etc/ikos/daemons";
static const char* CONFIG_USER_DIR = "~/.config/ikos/daemons";

/* ========================== Configuration Parsing ========================== */

typedef struct config_entry {
    char key[128];
    char value[512];
    struct config_entry* next;
} config_entry_t;

typedef struct config_section {
    char name[128];
    config_entry_t* entries;
    struct config_section* next;
} config_section_t;

typedef struct config_file {
    char filename[512];
    config_section_t* sections;
} config_file_t;

static void trim_whitespace(char* str) {
    /* Trim leading whitespace */
    char* start = str;
    while (isspace(*start)) start++;
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    
    /* Trim trailing whitespace */
    char* end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) {
        *end = '\0';
        end--;
    }
}

static config_section_t* find_section(config_file_t* config, const char* section_name) {
    config_section_t* section = config->sections;
    while (section) {
        if (strcmp(section->name, section_name) == 0) {
            return section;
        }
        section = section->next;
    }
    return NULL;
}

static config_entry_t* find_entry(config_section_t* section, const char* key) {
    config_entry_t* entry = section->entries;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static const char* get_config_value(config_file_t* config, const char* section_name, 
                                   const char* key, const char* default_value) {
    config_section_t* section = find_section(config, section_name);
    if (!section) {
        return default_value;
    }
    
    config_entry_t* entry = find_entry(section, key);
    if (!entry) {
        return default_value;
    }
    
    return entry->value;
}

static int parse_config_file(const char* filename, config_file_t* config) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    memset(config, 0, sizeof(config_file_t));
    strncpy(config->filename, filename, sizeof(config->filename) - 1);
    
    char line[CONFIG_LINE_MAX];
    config_section_t* current_section = NULL;
    int line_number = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        trim_whitespace(line);
        
        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        /* Check for section header [section_name] */
        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            line[strlen(line) - 1] = '\0';  /* Remove ] */
            char* section_name = line + 1;   /* Skip [ */
            
            /* Create new section */
            config_section_t* section = malloc(sizeof(config_section_t));
            if (!section) {
                fclose(file);
                return DAEMON_ERROR_MEMORY;
            }
            
            strncpy(section->name, section_name, sizeof(section->name) - 1);
            section->entries = NULL;
            section->next = config->sections;
            config->sections = section;
            current_section = section;
            
            continue;
        }
        
        /* Parse key=value pairs */
        char* equals = strchr(line, '=');
        if (!equals) {
            continue;  /* Skip malformed lines */
        }
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        trim_whitespace(key);
        trim_whitespace(value);
        
        /* Need a current section */
        if (!current_section) {
            continue;
        }
        
        /* Create new entry */
        config_entry_t* entry = malloc(sizeof(config_entry_t));
        if (!entry) {
            fclose(file);
            return DAEMON_ERROR_MEMORY;
        }
        
        strncpy(entry->key, key, sizeof(entry->key) - 1);
        strncpy(entry->value, value, sizeof(entry->value) - 1);
        entry->next = current_section->entries;
        current_section->entries = entry;
    }
    
    fclose(file);
    return DAEMON_SUCCESS;
}

static void free_config_file(config_file_t* config) {
    config_section_t* section = config->sections;
    while (section) {
        config_section_t* next_section = section->next;
        
        config_entry_t* entry = section->entries;
        while (entry) {
            config_entry_t* next_entry = entry->next;
            free(entry);
            entry = next_entry;
        }
        
        free(section);
        section = next_section;
    }
    
    memset(config, 0, sizeof(config_file_t));
}

/* ========================== Default Configuration ========================== */

int daemon_config_load_defaults(daemon_config_t* config) {
    if (!config) {
        return DAEMON_ERROR_INVALID;
    }
    
    memset(config, 0, sizeof(daemon_config_t));
    
    /* Set reasonable defaults */
    config->auto_restart = true;
    config->max_restart_attempts = 3;
    config->restart_delay = 5;
    config->startup_timeout = 30;
    config->shutdown_timeout = 10;
    config->log_level = LOG_LEVEL_INFO;
    config->log_to_syslog = true;
    config->log_to_file = false;
    config->run_as_user = 0;  /* Root by default */
    config->run_as_group = 0;
    config->cpu_limit = 0.0;  /* No limit */
    config->memory_limit = 0; /* No limit */
    config->file_descriptor_limit = 1024;
    config->core_dump_enabled = false;
    config->priority = 0;  /* Normal priority */
    
    return DAEMON_SUCCESS;
}

/* ========================== Configuration File Loading ========================== */

int daemon_config_load(const char* daemon_name, daemon_config_t* config) {
    if (!daemon_name || !config) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Load defaults first */
    int ret = daemon_config_load_defaults(config);
    if (ret != DAEMON_SUCCESS) {
        return ret;
    }
    
    /* Build configuration file path */
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/%s.conf", CONFIG_DIR, daemon_name);
    
    /* Try to parse configuration file */
    config_file_t config_file;
    ret = parse_config_file(config_path, &config_file);
    if (ret != DAEMON_SUCCESS) {
        /* Try user configuration directory */
        char user_config_path[512];
        const char* home = getenv("HOME");
        if (home) {
            snprintf(user_config_path, sizeof(user_config_path), 
                    "%s/.config/ikos/daemons/%s.conf", home, daemon_name);
            ret = parse_config_file(user_config_path, &config_file);
        }
        
        if (ret != DAEMON_SUCCESS) {
            /* Use defaults if no config file found */
            return DAEMON_SUCCESS;
        }
    }
    
    /* Apply configuration values */
    const char* value;
    
    /* Basic configuration */
    value = get_config_value(&config_file, "daemon", "description", NULL);
    if (value) {
        strncpy(config->description, value, sizeof(config->description) - 1);
    }
    
    value = get_config_value(&config_file, "daemon", "executable", NULL);
    if (value) {
        strncpy(config->executable_path, value, sizeof(config->executable_path) - 1);
    }
    
    value = get_config_value(&config_file, "daemon", "working_directory", NULL);
    if (value) {
        strncpy(config->working_directory, value, sizeof(config->working_directory) - 1);
    }
    
    /* Arguments */
    for (int i = 0; i < DAEMON_MAX_ARGS - 1; i++) {
        char key[32];
        snprintf(key, sizeof(key), "arg%d", i);
        value = get_config_value(&config_file, "daemon", key, NULL);
        if (value) {
            config->args[i] = strdup(value);
        } else {
            break;
        }
    }
    
    /* Environment variables */
    int env_count = 0;
    for (int i = 0; i < DAEMON_MAX_ENV_VARS && env_count < DAEMON_MAX_ENV_VARS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "env%d", i);
        value = get_config_value(&config_file, "daemon", key, NULL);
        if (value) {
            config->env[env_count] = strdup(value);
            env_count++;
        }
    }
    
    /* Restart configuration */
    value = get_config_value(&config_file, "restart", "auto_restart", "true");
    config->auto_restart = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    
    value = get_config_value(&config_file, "restart", "max_attempts", "3");
    config->max_restart_attempts = atoi(value);
    
    value = get_config_value(&config_file, "restart", "delay", "5");
    config->restart_delay = atoi(value);
    
    /* Timeouts */
    value = get_config_value(&config_file, "timeouts", "startup", "30");
    config->startup_timeout = atoi(value);
    
    value = get_config_value(&config_file, "timeouts", "shutdown", "10");
    config->shutdown_timeout = atoi(value);
    
    /* Logging */
    value = get_config_value(&config_file, "logging", "level", "info");
    if (strcmp(value, "debug") == 0) config->log_level = LOG_LEVEL_DEBUG;
    else if (strcmp(value, "info") == 0) config->log_level = LOG_LEVEL_INFO;
    else if (strcmp(value, "warning") == 0) config->log_level = LOG_LEVEL_WARNING;
    else if (strcmp(value, "error") == 0) config->log_level = LOG_LEVEL_ERROR;
    else if (strcmp(value, "critical") == 0) config->log_level = LOG_LEVEL_CRITICAL;
    
    value = get_config_value(&config_file, "logging", "syslog", "true");
    config->log_to_syslog = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    
    value = get_config_value(&config_file, "logging", "file", "false");
    config->log_to_file = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    
    value = get_config_value(&config_file, "logging", "file_path", NULL);
    if (value) {
        strncpy(config->log_file_path, value, sizeof(config->log_file_path) - 1);
    }
    
    /* Security */
    value = get_config_value(&config_file, "security", "user", "0");
    config->run_as_user = atoi(value);
    
    value = get_config_value(&config_file, "security", "group", "0");
    config->run_as_group = atoi(value);
    
    /* Resource limits */
    value = get_config_value(&config_file, "limits", "cpu_percent", "0");
    config->cpu_limit = atof(value);
    
    value = get_config_value(&config_file, "limits", "memory_mb", "0");
    config->memory_limit = atoi(value) * 1024 * 1024;
    
    value = get_config_value(&config_file, "limits", "file_descriptors", "1024");
    config->file_descriptor_limit = atoi(value);
    
    value = get_config_value(&config_file, "limits", "core_dump", "false");
    config->core_dump_enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    
    value = get_config_value(&config_file, "limits", "priority", "0");
    config->priority = atoi(value);
    
    free_config_file(&config_file);
    return DAEMON_SUCCESS;
}

/* ========================== Configuration Validation ========================== */

int daemon_config_validate(const daemon_config_t* config) {
    if (!config) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Validate required fields */
    if (strlen(config->name) == 0) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (strlen(config->executable_path) == 0) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Check if executable exists and is executable */
    if (access(config->executable_path, X_OK) != 0) {
        return DAEMON_ERROR_NOT_FOUND;
    }
    
    /* Validate restart configuration */
    if (config->max_restart_attempts < 0 || config->max_restart_attempts > 100) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (config->restart_delay < 0 || config->restart_delay > 3600) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Validate timeouts */
    if (config->startup_timeout < 1 || config->startup_timeout > 3600) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (config->shutdown_timeout < 1 || config->shutdown_timeout > 300) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Validate resource limits */
    if (config->cpu_limit < 0.0 || config->cpu_limit > 100.0) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (config->memory_limit > 0 && config->memory_limit < 1024 * 1024) {  /* Min 1MB */
        return DAEMON_ERROR_INVALID;
    }
    
    if (config->file_descriptor_limit < 3 || config->file_descriptor_limit > 65536) {
        return DAEMON_ERROR_INVALID;
    }
    
    if (config->priority < -20 || config->priority > 19) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Validate working directory if specified */
    if (strlen(config->working_directory) > 0) {
        struct stat st;
        if (stat(config->working_directory, &st) != 0) {
            return DAEMON_ERROR_NOT_FOUND;
        }
        
        if (!S_ISDIR(st.st_mode)) {
            return DAEMON_ERROR_INVALID;
        }
    }
    
    /* Validate log file path if logging to file */
    if (config->log_to_file && strlen(config->log_file_path) > 0) {
        /* Check if directory exists and is writable */
        char dir_path[512];
        strncpy(dir_path, config->log_file_path, sizeof(dir_path) - 1);
        
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            
            if (access(dir_path, W_OK) != 0) {
                return DAEMON_ERROR_PERMISSION;
            }
        }
    }
    
    return DAEMON_SUCCESS;
}

/* ========================== Configuration Saving ========================== */

int daemon_config_save(const char* daemon_name, const daemon_config_t* config) {
    if (!daemon_name || !config) {
        return DAEMON_ERROR_INVALID;
    }
    
    /* Validate configuration first */
    int ret = daemon_config_validate(config);
    if (ret != DAEMON_SUCCESS) {
        return ret;
    }
    
    /* Create configuration directory if needed */
    if (mkdir(CONFIG_DIR, 0755) != 0 && errno != EEXIST) {
        return DAEMON_ERROR_PERMISSION;
    }
    
    /* Build configuration file path */
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/%s.conf", CONFIG_DIR, daemon_name);
    
    /* Open configuration file for writing */
    FILE* file = fopen(config_path, "w");
    if (!file) {
        return DAEMON_ERROR_PERMISSION;
    }
    
    /* Write configuration */
    fprintf(file, "# IKOS Daemon Configuration: %s\n", daemon_name);
    fprintf(file, "# Generated automatically - modify with caution\n\n");
    
    /* Daemon section */
    fprintf(file, "[daemon]\n");
    fprintf(file, "description = %s\n", config->description);
    fprintf(file, "executable = %s\n", config->executable_path);
    if (strlen(config->working_directory) > 0) {
        fprintf(file, "working_directory = %s\n", config->working_directory);
    }
    
    /* Arguments */
    for (int i = 0; i < DAEMON_MAX_ARGS && config->args[i]; i++) {
        fprintf(file, "arg%d = %s\n", i, config->args[i]);
    }
    
    /* Environment variables */
    for (int i = 0; i < DAEMON_MAX_ENV_VARS && config->env[i]; i++) {
        fprintf(file, "env%d = %s\n", i, config->env[i]);
    }
    
    fprintf(file, "\n");
    
    /* Restart section */
    fprintf(file, "[restart]\n");
    fprintf(file, "auto_restart = %s\n", config->auto_restart ? "true" : "false");
    fprintf(file, "max_attempts = %d\n", config->max_restart_attempts);
    fprintf(file, "delay = %d\n", config->restart_delay);
    fprintf(file, "\n");
    
    /* Timeouts section */
    fprintf(file, "[timeouts]\n");
    fprintf(file, "startup = %d\n", config->startup_timeout);
    fprintf(file, "shutdown = %d\n", config->shutdown_timeout);
    fprintf(file, "\n");
    
    /* Logging section */
    fprintf(file, "[logging]\n");
    const char* log_level_str = "info";
    switch (config->log_level) {
        case LOG_LEVEL_DEBUG: log_level_str = "debug"; break;
        case LOG_LEVEL_INFO: log_level_str = "info"; break;
        case LOG_LEVEL_WARNING: log_level_str = "warning"; break;
        case LOG_LEVEL_ERROR: log_level_str = "error"; break;
        case LOG_LEVEL_CRITICAL: log_level_str = "critical"; break;
    }
    fprintf(file, "level = %s\n", log_level_str);
    fprintf(file, "syslog = %s\n", config->log_to_syslog ? "true" : "false");
    fprintf(file, "file = %s\n", config->log_to_file ? "true" : "false");
    if (strlen(config->log_file_path) > 0) {
        fprintf(file, "file_path = %s\n", config->log_file_path);
    }
    fprintf(file, "\n");
    
    /* Security section */
    fprintf(file, "[security]\n");
    fprintf(file, "user = %d\n", config->run_as_user);
    fprintf(file, "group = %d\n", config->run_as_group);
    fprintf(file, "\n");
    
    /* Limits section */
    fprintf(file, "[limits]\n");
    fprintf(file, "cpu_percent = %.1f\n", config->cpu_limit);
    fprintf(file, "memory_mb = %d\n", (int)(config->memory_limit / (1024 * 1024)));
    fprintf(file, "file_descriptors = %d\n", config->file_descriptor_limit);
    fprintf(file, "core_dump = %s\n", config->core_dump_enabled ? "true" : "false");
    fprintf(file, "priority = %d\n", config->priority);
    
    fclose(file);
    
    /* Set proper permissions */
    if (chmod(config_path, 0644) != 0) {
        return DAEMON_ERROR_PERMISSION;
    }
    
    return DAEMON_SUCCESS;
}

/* ========================== Configuration Utilities ========================== */

int daemon_config_copy(const daemon_config_t* src, daemon_config_t* dst) {
    if (!src || !dst) {
        return DAEMON_ERROR_INVALID;
    }
    
    *dst = *src;
    
    /* Deep copy dynamically allocated strings */
    for (int i = 0; i < DAEMON_MAX_ARGS && src->args[i]; i++) {
        dst->args[i] = strdup(src->args[i]);
        if (!dst->args[i]) {
            /* Clean up on failure */
            for (int j = 0; j < i; j++) {
                free(dst->args[j]);
            }
            return DAEMON_ERROR_MEMORY;
        }
    }
    
    for (int i = 0; i < DAEMON_MAX_ENV_VARS && src->env[i]; i++) {
        dst->env[i] = strdup(src->env[i]);
        if (!dst->env[i]) {
            /* Clean up on failure */
            for (int j = 0; j < i; j++) {
                free(dst->env[j]);
            }
            for (int j = 0; j < DAEMON_MAX_ARGS && dst->args[j]; j++) {
                free(dst->args[j]);
            }
            return DAEMON_ERROR_MEMORY;
        }
    }
    
    return DAEMON_SUCCESS;
}

void daemon_config_free(daemon_config_t* config) {
    if (!config) {
        return;
    }
    
    /* Free dynamically allocated strings */
    for (int i = 0; i < DAEMON_MAX_ARGS && config->args[i]; i++) {
        free(config->args[i]);
        config->args[i] = NULL;
    }
    
    for (int i = 0; i < DAEMON_MAX_ENV_VARS && config->env[i]; i++) {
        free(config->env[i]);
        config->env[i] = NULL;
    }
}

/* ========================== Configuration Templates ========================== */

int daemon_config_create_template(const char* daemon_name, daemon_type_t type) {
    if (!daemon_name) {
        return DAEMON_ERROR_INVALID;
    }
    
    daemon_config_t config;
    daemon_config_load_defaults(&config);
    
    strncpy(config.name, daemon_name, sizeof(config.name) - 1);
    
    /* Set type-specific defaults */
    switch (type) {
        case DAEMON_TYPE_SERVICE:
            strncpy(config.description, "System service daemon", sizeof(config.description) - 1);
            config.auto_restart = true;
            config.max_restart_attempts = 5;
            break;
            
        case DAEMON_TYPE_WORKER:
            strncpy(config.description, "Background worker daemon", sizeof(config.description) - 1);
            config.auto_restart = true;
            config.max_restart_attempts = 3;
            break;
            
        case DAEMON_TYPE_MONITOR:
            strncpy(config.description, "System monitoring daemon", sizeof(config.description) - 1);
            config.auto_restart = true;
            config.max_restart_attempts = 10;
            config.restart_delay = 1;
            break;
            
        case DAEMON_TYPE_ONESHOT:
            strncpy(config.description, "One-shot task daemon", sizeof(config.description) - 1);
            config.auto_restart = false;
            config.max_restart_attempts = 0;
            break;
            
        default:
            strncpy(config.description, "Generic daemon", sizeof(config.description) - 1);
            break;
    }
    
    return daemon_config_save(daemon_name, &config);
}
