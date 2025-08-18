/*
 * IKOS Operating System - CLI Shell Header
 * Issue #36: Command Line Interface Implementation
 *
 * Header file for the IKOS command-line interface shell
 * Defines structures, constants, and function prototypes
 */

#ifndef IKOS_CLI_SHELL_H
#define IKOS_CLI_SHELL_H

/*
 * IKOS Operating System - CLI Shell Header
 * Issue #36: Command Line Interface Implementation
 *
 * Header file for the IKOS command-line interface shell
 * Defines structures, constants, and function prototypes
 */

#ifndef IKOS_CLI_SHELL_H
#define IKOS_CLI_SHELL_H

/* Basic type definitions for IKOS compatibility */
#ifndef size_t
typedef unsigned long size_t;
#endif

#ifndef NULL  
#define NULL ((void*)0)
#endif

#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Shell Configuration Constants */
#define CLI_SHELL_VERSION           "1.0"
#define CLI_MAX_INPUT_LENGTH        1024
#define CLI_MAX_ARGS               64
#define CLI_MAX_PATH_LENGTH        1024
#define CLI_MAX_ENV_VARS           100
#define CLI_MAX_HISTORY            50
#define CLI_PROMPT_SIZE            64

/* Return codes */
#define CLI_SUCCESS                0
#define CLI_ERROR                  1
#define CLI_EXIT                   2

/* Shell state structure */
typedef struct {
    char current_directory[CLI_MAX_PATH_LENGTH];
    char prompt[CLI_PROMPT_SIZE];
    int last_exit_code;
    bool running;
    char history[CLI_MAX_HISTORY][CLI_MAX_INPUT_LENGTH];
    int history_count;
    int history_index;
} cli_shell_state_t;

/* Command structure */
typedef struct {
    const char* name;
    int (*handler)(int argc, char* argv[]);
    const char* description;
    const char* usage;
} cli_command_t;

/* Core shell functions */
int cli_shell_init(void);
void cli_shell_cleanup(void);
int cli_shell_run(void);
void cli_shell_show_prompt(void);

/* Command processing */
int cli_parse_command(char* input, char** args);
int cli_execute_command(char** args);
int cli_execute_builtin(char** args);
int cli_execute_external(char** args);

/* Built-in command handlers */
int cli_cmd_echo(int argc, char* argv[]);
int cli_cmd_pwd(int argc, char* argv[]);
int cli_cmd_cd(int argc, char* argv[]);
int cli_cmd_set(int argc, char* argv[]);
int cli_cmd_unset(int argc, char* argv[]);
int cli_cmd_env(int argc, char* argv[]);
int cli_cmd_help(int argc, char* argv[]);
int cli_cmd_exit(int argc, char* argv[]);
int cli_cmd_clear(int argc, char* argv[]);
int cli_cmd_history(int argc, char* argv[]);

/* Environment variable management */
int cli_set_env_var(const char* name, const char* value);
const char* cli_get_env_var(const char* name);
int cli_unset_env_var(const char* name);
void cli_list_env_vars(void);

/* History management */
void cli_add_to_history(const char* command);
void cli_show_history(void);
const char* cli_get_history_entry(int index);

#ifdef __cplusplus
}
#endif

#endif /* IKOS_CLI_SHELL_H */

/* Utility functions */
char* cli_trim_whitespace(char* str);
void cli_print_banner(void);
void cli_print_help(void);

/* Error handling */
void cli_print_error(const char* message);
void cli_print_command_not_found(const char* command);

#ifdef __cplusplus
}
#endif

#endif /* IKOS_CLI_SHELL_H */
