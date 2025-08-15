/* IKOS Basic Shell Implementation
 * A command-line interface (CLI) shell for user interaction
 */

#ifndef IKOS_SHELL_H
#define IKOS_SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shell Configuration */
#define SHELL_MAX_INPUT_LENGTH  1024    /* Maximum input line length */
#define SHELL_MAX_ARGS          64      /* Maximum command arguments */
#define SHELL_MAX_PATH_LENGTH   1024    /* Maximum path length */
#define SHELL_MAX_ALIAS_NAME    32      /* Maximum alias name length */
#define SHELL_MAX_ALIASES       100     /* Maximum number of aliases */
#define SHELL_HISTORY_SIZE      100     /* Command history size */
#define SHELL_PROMPT_SIZE       256     /* Shell prompt buffer size */

/* Shell Command Structure */
typedef struct shell_command {
    const char* name;
    int (*handler)(int argc, char* argv[]);
    const char* description;
    const char* usage;
} shell_command_t;

/* Shell Alias Structure */
typedef struct shell_alias {
    char name[SHELL_MAX_ALIAS_NAME];
    char command[SHELL_MAX_INPUT_LENGTH];
    bool active;
} shell_alias_t;

/* Shell State Structure */
typedef struct shell_state {
    char current_directory[SHELL_MAX_PATH_LENGTH];
    char prompt[SHELL_PROMPT_SIZE];
    char* environment[256];  /* Environment variables */
    int env_count;
    shell_alias_t aliases[SHELL_MAX_ALIASES];
    int alias_count;
    char history[SHELL_HISTORY_SIZE][SHELL_MAX_INPUT_LENGTH];
    int history_count;
    int history_index;
    bool exit_requested;
    int last_exit_code;
} shell_state_t;

/* Shell Function Prototypes */

/* Core Shell Functions */
int shell_init(void);
void shell_cleanup(void);
void shell_run(void);
int shell_execute_command(const char* input);

/* Command Parsing */
int shell_parse_command(const char* input, char** argv, int max_args);
char* shell_expand_aliases(const char* input);
char* shell_expand_variables(const char* input);

/* Built-in Commands */
int shell_cmd_exit(int argc, char* argv[]);
int shell_cmd_cd(int argc, char* argv[]);
int shell_cmd_pwd(int argc, char* argv[]);
int shell_cmd_echo(int argc, char* argv[]);
int shell_cmd_set(int argc, char* argv[]);
int shell_cmd_unset(int argc, char* argv[]);
int shell_cmd_export(int argc, char* argv[]);
int shell_cmd_alias(int argc, char* argv[]);
int shell_cmd_unalias(int argc, char* argv[]);
int shell_cmd_history(int argc, char* argv[]);
int shell_cmd_clear(int argc, char* argv[]);
int shell_cmd_help(int argc, char* argv[]);

/* Process Management */
int shell_execute_external(int argc, char* argv[]);
int shell_execute_pipeline(const char* input);

/* Environment Management */
char* shell_get_env(const char* name);
int shell_set_env(const char* name, const char* value);
int shell_unset_env(const char* name);

/* History Management */
void shell_add_history(const char* command);
void shell_show_history(void);
char* shell_get_history(int index);

/* Alias Management */
int shell_add_alias(const char* name, const char* command);
int shell_remove_alias(const char* name);
char* shell_get_alias(const char* name);
void shell_show_aliases(void);

/* Utility Functions */
void shell_update_prompt(void);
void shell_print_prompt(void);
bool shell_is_builtin(const char* command);
void shell_print_error(const char* message);
void shell_print_warning(const char* message);
char* shell_trim_whitespace(char* str);

/* Signal Handling */
void shell_setup_signals(void);
void shell_handle_sigint(int sig);
void shell_handle_sigterm(int sig);

#ifdef __cplusplus
}
#endif

#endif /* IKOS_SHELL_H */
