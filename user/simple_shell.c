/* IKOS Basic Shell Implementation
 * Simple command-line interface for IKOS OS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64

/* Simple prompt display */
void show_prompt() {
    printf("$ ");
    fflush(stdout);
}

/* Parse command line into arguments */
int parse_command(char *input, char **args) {
    int count = 0;
    char *token = strtok(input, " \t\n");
    
    while (token != NULL && count < MAX_ARGS - 1) {
        args[count] = token;
        count++;
        token = strtok(NULL, " \t\n");
    }
    args[count] = NULL;
    return count;
}

/* Built-in command: echo */
int cmd_echo(char **args) {
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s", args[i]);
        if (args[i + 1] != NULL) printf(" ");
    }
    printf("\n");
    return 0;
}

/* Built-in command: pwd */
int cmd_pwd(char **args) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        printf("/\n"); /* Default if getcwd fails */
    }
    return 0;
}

/* Built-in command: cd */
int cmd_cd(char **args) {
    char *dir = args[1];
    if (dir == NULL) {
        dir = getenv("HOME");
        if (dir == NULL) dir = "/";
    }
    
    if (chdir(dir) != 0) {
        printf("cd: cannot access '%s': No such directory\n", dir);
        return 1;
    }
    return 0;
}

/* Built-in command: set */
int cmd_set(char **args) {
    if (args[1] == NULL) {
        /* Show all environment variables */
        extern char **environ;
        for (int i = 0; environ[i] != NULL; i++) {
            printf("%s\n", environ[i]);
        }
        return 0;
    }
    
    /* Set environment variable */
    char *eq = strchr(args[1], '=');
    if (eq == NULL) {
        printf("set: invalid format, use VAR=value\n");
        return 1;
    }
    
    *eq = '\0';
    char *name = args[1];
    char *value = eq + 1;
    
    if (setenv(name, value, 1) != 0) {
        printf("set: failed to set %s\n", name);
        return 1;
    }
    return 0;
}

/* Built-in command: help */
int cmd_help(char **args) {
    printf("IKOS Shell - Basic Commands:\n");
    printf("  echo <text>    - Display text\n");
    printf("  pwd            - Show current directory\n");
    printf("  cd [dir]       - Change directory\n");
    printf("  set [VAR=val]  - Set/show environment variables\n");
    printf("  help           - Show this help\n");
    printf("  exit           - Exit shell\n");
    printf("\nPress Ctrl+C to interrupt, 'exit' to quit.\n");
    return 0;
}

/* Execute built-in commands */
int execute_builtin(char **args) {
    if (strcmp(args[0], "echo") == 0) return cmd_echo(args);
    if (strcmp(args[0], "pwd") == 0) return cmd_pwd(args);
    if (strcmp(args[0], "cd") == 0) return cmd_cd(args);
    if (strcmp(args[0], "set") == 0) return cmd_set(args);
    if (strcmp(args[0], "help") == 0) return cmd_help(args);
    if (strcmp(args[0], "exit") == 0) {
        printf("Goodbye!\n");
        exit(0);
    }
    return -1; /* Not a built-in command */
}

/* Execute external command */
int execute_external(char **args) {
    printf("shell: %s: command not found\n", args[0]);
    printf("(External command execution not implemented in this demo)\n");
    return 1;
}

/* Main shell loop */
int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    int status;
    
    printf("IKOS Shell v1.0 - Basic CLI Implementation\n");
    printf("Type 'help' for available commands, 'exit' to quit.\n\n");
    
    while (1) {
        show_prompt();
        
        /* Read input */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (feof(stdin)) {
                printf("\nGoodbye!\n");
                break;
            }
            continue;
        }
        
        /* Skip empty lines */
        if (strlen(input) <= 1) continue;
        
        /* Parse command */
        int argc = parse_command(input, args);
        if (argc == 0) continue;
        
        /* Execute command */
        status = execute_builtin(args);
        if (status == -1) {
            /* Not a built-in, try external */
            status = execute_external(args);
        }
    }
    
    return 0;
}
