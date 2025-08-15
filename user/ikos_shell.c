/* IKOS Basic Shell Implementation
 * A command-line interface (CLI) shell for user interaction
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#include "ikos_shell.h"
#include "fs_user_api.h"

/* Global shell state */
static shell_state_t shell_state = {0};

/* Built-in command table */
static const shell_command_t builtin_commands[] = {
    {"exit", shell_cmd_exit, "Exit the shell", "exit [code]"},
    {"cd", shell_cmd_cd, "Change directory", "cd [directory]"},
    {"pwd", shell_cmd_pwd, "Print working directory", "pwd"},
    {"echo", shell_cmd_echo, "Display text", "echo [text...]"},
    {"set", shell_cmd_set, "Set environment variable", "set [var=value]"},
    {"unset", shell_cmd_unset, "Unset environment variable", "unset <var>"},
    {"export", shell_cmd_export, "Export environment variable", "export <var=value>"},
    {"alias", shell_cmd_alias, "Create command alias", "alias [name=command]"},
    {"unalias", shell_cmd_unalias, "Remove command alias", "unalias <name>"},
    {"history", shell_cmd_history, "Show command history", "history"},
    {"clear", shell_cmd_clear, "Clear screen", "clear"},
    {"help", shell_cmd_help, "Show help information", "help [command]"},
    {NULL, NULL, NULL, NULL}
};

/* ===== Core Shell Functions ===== */

int shell_init(void) {
    /* Initialize shell state */
    memset(&shell_state, 0, sizeof(shell_state));
    
    /* Set initial directory */
    if (getcwd(shell_state.current_directory, sizeof(shell_state.current_directory)) == NULL) {
        strcpy(shell_state.current_directory, "/");
    }
    
    /* Initialize environment variables */
    shell_set_env("HOME", "/home");
    shell_set_env("PATH", "/bin:/usr/bin:/usr/local/bin");
    shell_set_env("PS1", "$ ");
    shell_set_env("PWD", shell_state.current_directory);
    
    /* Set up signal handlers */
    shell_setup_signals();
    
    /* Update prompt */
    shell_update_prompt();
    
    printf("IKOS Shell v1.0 - Type 'help' for available commands\n");
    
    return 0;
}

void shell_cleanup(void) {
    /* Clean up environment variables */
    for (int i = 0; i < shell_state.env_count; i++) {
        if (shell_state.environment[i]) {
            free(shell_state.environment[i]);
        }
    }
    
    printf("\nShell exiting...\n");
}

void shell_run(void) {
    char input[SHELL_MAX_INPUT_LENGTH];
    
    while (!shell_state.exit_requested) {
        shell_print_prompt();
        
        /* Read input */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            continue;
        }
        
        /* Remove trailing newline */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }
        
        /* Skip empty lines */
        char* trimmed = shell_trim_whitespace(input);
        if (strlen(trimmed) == 0) {
            continue;
        }
        
        /* Add to history */
        shell_add_history(trimmed);
        
        /* Execute command */
        int result = shell_execute_command(trimmed);
        shell_state.last_exit_code = result;
    }
}

int shell_execute_command(const char* input) {
    if (!input || strlen(input) == 0) {
        return 0;
    }
    
    /* Expand aliases */
    char* expanded = shell_expand_aliases(input);
    if (!expanded) {
        expanded = strdup(input);
    }
    
    /* Expand variables */
    char* final_input = shell_expand_variables(expanded);
    free(expanded);
    if (!final_input) {
        return -1;
    }
    
    /* Parse command */
    char* argv[SHELL_MAX_ARGS];
    int argc = shell_parse_command(final_input, argv, SHELL_MAX_ARGS);
    
    if (argc == 0) {
        free(final_input);
        return 0;
    }
    
    /* Check for built-in commands */
    for (int i = 0; builtin_commands[i].name; i++) {
        if (strcmp(argv[0], builtin_commands[i].name) == 0) {
            int result = builtin_commands[i].handler(argc, argv);
            free(final_input);
            return result;
        }
    }
    
    /* Check for filesystem commands */
    int fs_result = fs_execute_command(final_input);
    if (fs_result != 1) {  /* fs_execute_command returns 1 for "command not found" */
        free(final_input);
        return fs_result;
    }
    
    /* Execute as external command */
    int result = shell_execute_external(argc, argv);
    free(final_input);
    return result;
}

/* ===== Command Parsing ===== */

int shell_parse_command(const char* input, char** argv, int max_args) {
    if (!input) return 0;
    
    char* input_copy = strdup(input);
    char* token;
    int argc = 0;
    
    token = strtok(input_copy, " \t");
    while (token && argc < max_args - 1) {
        argv[argc] = strdup(token);
        argc++;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    
    free(input_copy);
    return argc;
}

char* shell_expand_aliases(const char* input) {
    if (!input) return NULL;
    
    /* Extract first word (command) */
    char* input_copy = strdup(input);
    char* command = strtok(input_copy, " \t");
    
    if (!command) {
        free(input_copy);
        return NULL;
    }
    
    /* Check if it's an alias */
    char* alias_value = shell_get_alias(command);
    if (alias_value) {
        /* Find the rest of the command line */
        const char* rest = input + strlen(command);
        while (*rest == ' ' || *rest == '\t') rest++;
        
        /* Build expanded command */
        size_t total_len = strlen(alias_value) + strlen(rest) + 2;
        char* expanded = malloc(total_len);
        if (expanded) {
            snprintf(expanded, total_len, "%s %s", alias_value, rest);
        }
        
        free(input_copy);
        return expanded;
    }
    
    free(input_copy);
    return NULL;
}

char* shell_expand_variables(const char* input) {
    if (!input) return NULL;
    
    char* result = malloc(SHELL_MAX_INPUT_LENGTH);
    if (!result) return NULL;
    
    const char* src = input;
    char* dst = result;
    size_t remaining = SHELL_MAX_INPUT_LENGTH - 1;
    
    while (*src && remaining > 0) {
        if (*src == '$' && *(src + 1) != '\0') {
            src++; /* Skip $ */
            
            /* Extract variable name */
            const char* var_start = src;
            while (*src && (isalnum(*src) || *src == '_')) {
                src++;
            }
            
            size_t var_len = src - var_start;
            if (var_len > 0) {
                char var_name[256];
                strncpy(var_name, var_start, var_len);
                var_name[var_len] = '\0';
                
                char* var_value = shell_get_env(var_name);
                if (var_value) {
                    size_t value_len = strlen(var_value);
                    if (value_len <= remaining) {
                        strcpy(dst, var_value);
                        dst += value_len;
                        remaining -= value_len;
                    }
                }
            }
        } else {
            *dst++ = *src++;
            remaining--;
        }
    }
    
    *dst = '\0';
    return result;
}

/* ===== Built-in Commands ===== */

int shell_cmd_exit(int argc, char* argv[]) {
    int exit_code = 0;
    
    if (argc > 1) {
        exit_code = atoi(argv[1]);
    }
    
    shell_state.exit_requested = true;
    shell_state.last_exit_code = exit_code;
    
    return exit_code;
}

int shell_cmd_cd(int argc, char* argv[]) {
    const char* path = (argc > 1) ? argv[1] : shell_get_env("HOME");
    
    if (!path) {
        path = "/";
    }
    
    if (chdir(path) == 0) {
        /* Update PWD environment variable */
        if (getcwd(shell_state.current_directory, sizeof(shell_state.current_directory))) {
            shell_set_env("PWD", shell_state.current_directory);
            shell_update_prompt();
        }
        return 0;
    } else {
        shell_print_error("cd: cannot change directory");
        return 1;
    }
}

int shell_cmd_pwd(int argc, char* argv[]) {
    (void)argc; (void)argv; /* Suppress unused warnings */
    
    char* pwd = shell_get_env("PWD");
    if (pwd) {
        printf("%s\n", pwd);
    } else {
        printf("%s\n", shell_state.current_directory);
    }
    
    return 0;
}

int shell_cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}

int shell_cmd_set(int argc, char* argv[]) {
    if (argc == 1) {
        /* Show all environment variables */
        for (int i = 0; i < shell_state.env_count; i++) {
            if (shell_state.environment[i]) {
                printf("%s\n", shell_state.environment[i]);
            }
        }
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        char* eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            shell_set_env(argv[i], eq + 1);
            *eq = '='; /* Restore original string */
        } else {
            shell_print_error("set: invalid format, use var=value");
            return 1;
        }
    }
    
    return 0;
}

int shell_cmd_unset(int argc, char* argv[]) {
    if (argc < 2) {
        shell_print_error("unset: missing variable name");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        shell_unset_env(argv[i]);
    }
    
    return 0;
}

int shell_cmd_export(int argc, char* argv[]) {
    if (argc < 2) {
        shell_print_error("export: missing variable assignment");
        return 1;
    }
    
    /* For now, export is the same as set */
    return shell_cmd_set(argc, argv);
}

int shell_cmd_alias(int argc, char* argv[]) {
    if (argc == 1) {
        /* Show all aliases */
        shell_show_aliases();
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        char* eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            shell_add_alias(argv[i], eq + 1);
            *eq = '='; /* Restore original string */
        } else {
            /* Show specific alias */
            char* alias_value = shell_get_alias(argv[i]);
            if (alias_value) {
                printf("alias %s='%s'\n", argv[i], alias_value);
            } else {
                printf("alias: %s: not found\n", argv[i]);
            }
        }
    }
    
    return 0;
}

int shell_cmd_unalias(int argc, char* argv[]) {
    if (argc < 2) {
        shell_print_error("unalias: missing alias name");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        if (shell_remove_alias(argv[i]) != 0) {
            printf("unalias: %s: not found\n", argv[i]);
        }
    }
    
    return 0;
}

int shell_cmd_history(int argc, char* argv[]) {
    (void)argc; (void)argv; /* Suppress unused warnings */
    
    shell_show_history();
    return 0;
}

int shell_cmd_clear(int argc, char* argv[]) {
    (void)argc; (void)argv; /* Suppress unused warnings */
    
    printf("\033[2J\033[H"); /* ANSI escape codes to clear screen */
    return 0;
}

int shell_cmd_help(int argc, char* argv[]) {
    if (argc == 1) {
        printf("IKOS Shell Built-in Commands:\n");
        printf("=============================\n");
        
        for (int i = 0; builtin_commands[i].name; i++) {
            printf("  %-12s - %s\n", builtin_commands[i].name, builtin_commands[i].description);
        }
        
        printf("\nFilesystem Commands:\n");
        printf("===================\n");
        printf("  ls, mkdir, rmdir, cd, pwd, touch, rm, cp, mv, cat, echo,\n");
        printf("  stat, chmod, find - Use 'help <command>' for details\n");
        
        printf("\nUse 'help <command>' for detailed usage information.\n");
    } else {
        const char* cmd = argv[1];
        
        /* Check built-in commands */
        for (int i = 0; builtin_commands[i].name; i++) {
            if (strcmp(builtin_commands[i].name, cmd) == 0) {
                printf("%s - %s\n", builtin_commands[i].name, builtin_commands[i].description);
                printf("Usage: %s\n", builtin_commands[i].usage);
                return 0;
            }
        }
        
        /* Try filesystem command help */
        char help_cmd[256];
        snprintf(help_cmd, sizeof(help_cmd), "help %s", cmd);
        int result = fs_execute_command(help_cmd);
        if (result == 1) {
            printf("help: no help available for '%s'\n", cmd);
        }
    }
    
    return 0;
}

/* ===== Process Management ===== */

int shell_execute_external(int argc, char* argv[]) {
    if (argc == 0) return -1;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        /* Child process */
        execvp(argv[0], argv);
        
        /* If execvp returns, there was an error */
        fprintf(stderr, "shell: %s: command not found\n", argv[0]);
        exit(127);
    } else if (pid > 0) {
        /* Parent process */
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return -1;
        }
    } else {
        /* Fork failed */
        shell_print_error("shell: failed to create process");
        return -1;
    }
}

/* ===== Environment Management ===== */

char* shell_get_env(const char* name) {
    for (int i = 0; i < shell_state.env_count; i++) {
        if (shell_state.environment[i]) {
            char* eq = strchr(shell_state.environment[i], '=');
            if (eq) {
                size_t name_len = eq - shell_state.environment[i];
                if (strncmp(shell_state.environment[i], name, name_len) == 0 && 
                    strlen(name) == name_len) {
                    return eq + 1;
                }
            }
        }
    }
    return NULL;
}

int shell_set_env(const char* name, const char* value) {
    if (!name || !value) return -1;
    
    /* Check if variable already exists */
    for (int i = 0; i < shell_state.env_count; i++) {
        if (shell_state.environment[i]) {
            char* eq = strchr(shell_state.environment[i], '=');
            if (eq) {
                size_t name_len = eq - shell_state.environment[i];
                if (strncmp(shell_state.environment[i], name, name_len) == 0 && 
                    strlen(name) == name_len) {
                    /* Update existing variable */
                    free(shell_state.environment[i]);
                    size_t total_len = strlen(name) + strlen(value) + 2;
                    shell_state.environment[i] = malloc(total_len);
                    if (shell_state.environment[i]) {
                        snprintf(shell_state.environment[i], total_len, "%s=%s", name, value);
                    }
                    return 0;
                }
            }
        }
    }
    
    /* Add new variable */
    if (shell_state.env_count < 255) {
        size_t total_len = strlen(name) + strlen(value) + 2;
        shell_state.environment[shell_state.env_count] = malloc(total_len);
        if (shell_state.environment[shell_state.env_count]) {
            snprintf(shell_state.environment[shell_state.env_count], total_len, "%s=%s", name, value);
            shell_state.env_count++;
            return 0;
        }
    }
    
    return -1;
}

int shell_unset_env(const char* name) {
    if (!name) return -1;
    
    for (int i = 0; i < shell_state.env_count; i++) {
        if (shell_state.environment[i]) {
            char* eq = strchr(shell_state.environment[i], '=');
            if (eq) {
                size_t name_len = eq - shell_state.environment[i];
                if (strncmp(shell_state.environment[i], name, name_len) == 0 && 
                    strlen(name) == name_len) {
                    /* Remove this variable */
                    free(shell_state.environment[i]);
                    
                    /* Shift remaining variables */
                    for (int j = i; j < shell_state.env_count - 1; j++) {
                        shell_state.environment[j] = shell_state.environment[j + 1];
                    }
                    shell_state.env_count--;
                    return 0;
                }
            }
        }
    }
    
    return -1;
}

/* ===== History Management ===== */

void shell_add_history(const char* command) {
    if (!command || strlen(command) == 0) return;
    
    /* Don't add duplicate consecutive commands */
    if (shell_state.history_count > 0) {
        int last_index = (shell_state.history_count - 1) % SHELL_HISTORY_SIZE;
        if (strcmp(shell_state.history[last_index], command) == 0) {
            return;
        }
    }
    
    int index = shell_state.history_count % SHELL_HISTORY_SIZE;
    strncpy(shell_state.history[index], command, SHELL_MAX_INPUT_LENGTH - 1);
    shell_state.history[index][SHELL_MAX_INPUT_LENGTH - 1] = '\0';
    shell_state.history_count++;
}

void shell_show_history(void) {
    int start = (shell_state.history_count > SHELL_HISTORY_SIZE) ? 
                shell_state.history_count - SHELL_HISTORY_SIZE : 0;
    
    for (int i = start; i < shell_state.history_count; i++) {
        int index = i % SHELL_HISTORY_SIZE;
        printf("%4d  %s\n", i + 1, shell_state.history[index]);
    }
}

char* shell_get_history(int index) {
    if (index < 0 || index >= shell_state.history_count) {
        return NULL;
    }
    
    int array_index = index % SHELL_HISTORY_SIZE;
    return shell_state.history[array_index];
}

/* ===== Alias Management ===== */

int shell_add_alias(const char* name, const char* command) {
    if (!name || !command) return -1;
    
    /* Check if alias already exists */
    for (int i = 0; i < shell_state.alias_count; i++) {
        if (shell_state.aliases[i].active && 
            strcmp(shell_state.aliases[i].name, name) == 0) {
            /* Update existing alias */
            strncpy(shell_state.aliases[i].command, command, 
                    SHELL_MAX_INPUT_LENGTH - 1);
            shell_state.aliases[i].command[SHELL_MAX_INPUT_LENGTH - 1] = '\0';
            return 0;
        }
    }
    
    /* Add new alias */
    for (int i = 0; i < SHELL_MAX_ALIASES; i++) {
        if (!shell_state.aliases[i].active) {
            strncpy(shell_state.aliases[i].name, name, SHELL_MAX_ALIAS_NAME - 1);
            shell_state.aliases[i].name[SHELL_MAX_ALIAS_NAME - 1] = '\0';
            strncpy(shell_state.aliases[i].command, command, 
                    SHELL_MAX_INPUT_LENGTH - 1);
            shell_state.aliases[i].command[SHELL_MAX_INPUT_LENGTH - 1] = '\0';
            shell_state.aliases[i].active = true;
            
            if (i >= shell_state.alias_count) {
                shell_state.alias_count = i + 1;
            }
            return 0;
        }
    }
    
    return -1; /* No space for new alias */
}

int shell_remove_alias(const char* name) {
    if (!name) return -1;
    
    for (int i = 0; i < shell_state.alias_count; i++) {
        if (shell_state.aliases[i].active && 
            strcmp(shell_state.aliases[i].name, name) == 0) {
            shell_state.aliases[i].active = false;
            return 0;
        }
    }
    
    return -1; /* Alias not found */
}

char* shell_get_alias(const char* name) {
    if (!name) return NULL;
    
    for (int i = 0; i < shell_state.alias_count; i++) {
        if (shell_state.aliases[i].active && 
            strcmp(shell_state.aliases[i].name, name) == 0) {
            return shell_state.aliases[i].command;
        }
    }
    
    return NULL;
}

void shell_show_aliases(void) {
    bool found = false;
    
    for (int i = 0; i < shell_state.alias_count; i++) {
        if (shell_state.aliases[i].active) {
            printf("alias %s='%s'\n", shell_state.aliases[i].name, 
                   shell_state.aliases[i].command);
            found = true;
        }
    }
    
    if (!found) {
        printf("No aliases defined.\n");
    }
}

/* ===== Utility Functions ===== */

void shell_update_prompt(void) {
    char* ps1 = shell_get_env("PS1");
    if (ps1) {
        strncpy(shell_state.prompt, ps1, SHELL_PROMPT_SIZE - 1);
        shell_state.prompt[SHELL_PROMPT_SIZE - 1] = '\0';
    } else {
        strcpy(shell_state.prompt, "$ ");
    }
}

void shell_print_prompt(void) {
    printf("%s", shell_state.prompt);
    fflush(stdout);
}

bool shell_is_builtin(const char* command) {
    if (!command) return false;
    
    for (int i = 0; builtin_commands[i].name; i++) {
        if (strcmp(builtin_commands[i].name, command) == 0) {
            return true;
        }
    }
    
    return false;
}

void shell_print_error(const char* message) {
    fprintf(stderr, "shell: %s\n", message);
}

void shell_print_warning(const char* message) {
    fprintf(stderr, "shell: warning: %s\n", message);
}

char* shell_trim_whitespace(char* str) {
    if (!str) return str;
    
    /* Trim leading whitespace */
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    
    /* Trim trailing whitespace */
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) {
        *end-- = '\0';
    }
    
    return str;
}

/* ===== Signal Handling ===== */

void shell_setup_signals(void) {
    signal(SIGINT, shell_handle_sigint);
    signal(SIGTERM, shell_handle_sigterm);
}

void shell_handle_sigint(int sig) {
    (void)sig; /* Suppress unused warning */
    
    printf("\n");
    shell_print_prompt();
    fflush(stdout);
}

void shell_handle_sigterm(int sig) {
    (void)sig; /* Suppress unused warning */
    
    shell_state.exit_requested = true;
}

/* ===== Main Function ===== */

int main(void) {
    if (shell_init() != 0) {
        fprintf(stderr, "Failed to initialize shell\n");
        return 1;
    }
    
    shell_run();
    
    shell_cleanup();
    return shell_state.last_exit_code;
}
