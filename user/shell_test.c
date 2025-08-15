/* IKOS Shell Test Program
 * Tests the basic shell functionality
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "ikos_shell.h"

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("✓ %s\\n", message); \
            tests_passed++; \
        } else { \
            printf("✗ %s\\n", message); \
            tests_failed++; \
        } \
    } while(0)

/* Test functions */
void test_shell_init(void);
void test_environment_management(void);
void test_alias_management(void);
void test_history_management(void);
void test_command_parsing(void);
void test_builtin_commands(void);
void test_shell_utilities(void);

int main(void) {
    printf("IKOS Shell Test Suite\\n");
    printf("====================\\n\\n");
    
    /* Initialize shell for testing */
    shell_init();
    
    /* Run test suites */
    test_shell_init();
    test_environment_management();
    test_alias_management();
    test_history_management();
    test_command_parsing();
    test_builtin_commands();
    test_shell_utilities();
    
    /* Print summary */
    printf("\\n====================\\n");
    printf("Test Results: %d passed, %d failed\\n", tests_passed, tests_failed);
    
    if (tests_failed == 0) {
        printf("All tests passed! ✓\\n");
        return 0;
    } else {
        printf("Some tests failed! ✗\\n");
        return 1;
    }
}

void test_shell_init(void) {
    printf("Testing Shell Initialization:\\n");
    printf("----------------------------\\n");
    
    /* Test environment variables are set */
    TEST_ASSERT(shell_get_env("HOME") != NULL, "HOME environment variable set");
    TEST_ASSERT(shell_get_env("PATH") != NULL, "PATH environment variable set");
    TEST_ASSERT(shell_get_env("PS1") != NULL, "PS1 environment variable set");
    TEST_ASSERT(shell_get_env("PWD") != NULL, "PWD environment variable set");
    
    printf("\\n");
}

void test_environment_management(void) {
    printf("Testing Environment Management:\\n");
    printf("------------------------------\\n");
    
    /* Test setting environment variables */
    TEST_ASSERT(shell_set_env("TEST_VAR", "test_value") == 0, "Set environment variable");
    TEST_ASSERT(strcmp(shell_get_env("TEST_VAR"), "test_value") == 0, "Get environment variable");
    
    /* Test updating environment variables */
    TEST_ASSERT(shell_set_env("TEST_VAR", "new_value") == 0, "Update environment variable");
    TEST_ASSERT(strcmp(shell_get_env("TEST_VAR"), "new_value") == 0, "Get updated environment variable");
    
    /* Test unsetting environment variables */
    TEST_ASSERT(shell_unset_env("TEST_VAR") == 0, "Unset environment variable");
    TEST_ASSERT(shell_get_env("TEST_VAR") == NULL, "Environment variable unset");
    
    printf("\\n");
}

void test_alias_management(void) {
    printf("Testing Alias Management:\\n");
    printf("------------------------\\n");
    
    /* Test adding aliases */
    TEST_ASSERT(shell_add_alias("ll", "ls -l") == 0, "Add alias");
    TEST_ASSERT(strcmp(shell_get_alias("ll"), "ls -l") == 0, "Get alias");
    
    /* Test updating aliases */
    TEST_ASSERT(shell_add_alias("ll", "ls -la") == 0, "Update alias");
    TEST_ASSERT(strcmp(shell_get_alias("ll"), "ls -la") == 0, "Get updated alias");
    
    /* Test removing aliases */
    TEST_ASSERT(shell_remove_alias("ll") == 0, "Remove alias");
    TEST_ASSERT(shell_get_alias("ll") == NULL, "Alias removed");
    
    printf("\\n");
}

void test_history_management(void) {
    printf("Testing History Management:\\n");
    printf("--------------------------\\n");
    
    /* Test adding history entries */
    shell_add_history("echo hello");
    shell_add_history("ls -l");
    shell_add_history("pwd");
    
    TEST_ASSERT(shell_get_history(0) != NULL, "Get history entry 0");
    TEST_ASSERT(strcmp(shell_get_history(0), "echo hello") == 0, "History entry 0 correct");
    TEST_ASSERT(strcmp(shell_get_history(1), "ls -l") == 0, "History entry 1 correct");
    TEST_ASSERT(strcmp(shell_get_history(2), "pwd") == 0, "History entry 2 correct");
    
    /* Test duplicate prevention */
    shell_add_history("pwd");  /* Same as last command */
    TEST_ASSERT(shell_get_history(3) == NULL, "Duplicate history entry not added");
    
    printf("\\n");
}

void test_command_parsing(void) {
    printf("Testing Command Parsing:\\n");
    printf("-----------------------\\n");
    
    /* Test basic command parsing */
    char* argv[10];
    int argc = shell_parse_command("echo hello world", argv, 10);
    
    TEST_ASSERT(argc == 3, "Parse command argument count");
    TEST_ASSERT(strcmp(argv[0], "echo") == 0, "Parse command name");
    TEST_ASSERT(strcmp(argv[1], "hello") == 0, "Parse command argument 1");
    TEST_ASSERT(strcmp(argv[2], "world") == 0, "Parse command argument 2");
    
    /* Clean up allocated strings */
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    
    /* Test variable expansion */
    shell_set_env("TEST_EXPAND", "expanded");
    char* expanded = shell_expand_variables("echo $TEST_EXPAND");
    TEST_ASSERT(expanded != NULL, "Variable expansion works");
    TEST_ASSERT(strcmp(expanded, "echo expanded") == 0, "Variable expansion correct");
    free(expanded);
    
    printf("\\n");
}

void test_builtin_commands(void) {
    printf("Testing Built-in Commands:\\n");
    printf("-------------------------\\n");
    
    /* Test echo command */
    char* echo_argv[] = {"echo", "test", "message", NULL};
    TEST_ASSERT(shell_cmd_echo(3, echo_argv) == 0, "Echo command works");
    
    /* Test set command */
    char* set_argv[] = {"set", "TEST_SET=value", NULL};
    TEST_ASSERT(shell_cmd_set(2, set_argv) == 0, "Set command works");
    TEST_ASSERT(strcmp(shell_get_env("TEST_SET"), "value") == 0, "Set command sets variable");
    
    /* Test unset command */
    char* unset_argv[] = {"unset", "TEST_SET", NULL};
    TEST_ASSERT(shell_cmd_unset(2, unset_argv) == 0, "Unset command works");
    TEST_ASSERT(shell_get_env("TEST_SET") == NULL, "Unset command removes variable");
    
    /* Test pwd command */
    char* pwd_argv[] = {"pwd", NULL};
    TEST_ASSERT(shell_cmd_pwd(1, pwd_argv) == 0, "PWD command works");
    
    printf("\\n");
}

void test_shell_utilities(void) {
    printf("Testing Shell Utilities:\\n");
    printf("-----------------------\\n");
    
    /* Test whitespace trimming */
    char test_str[] = "  hello world  ";
    char* trimmed = shell_trim_whitespace(test_str);
    TEST_ASSERT(strcmp(trimmed, "hello world") == 0, "Whitespace trimming works");
    
    /* Test builtin command detection */
    TEST_ASSERT(shell_is_builtin("echo") == true, "Built-in command detection works");
    TEST_ASSERT(shell_is_builtin("nonexistent") == false, "Non-builtin command detection works");
    
    printf("\\n");
}

/* Interactive shell demo function */
void demo_shell_interactive(void) {
    printf("\\n=== Interactive Shell Demo ===\\n");
    printf("Starting IKOS Shell demo. Type commands to test:\\n");
    printf("Available commands: echo, pwd, set, alias, history, help, exit\\n");
    printf("Try: 'echo Hello World', 'set VAR=value', 'alias ll=ls -l'\\n");
    printf("Type 'exit' to return to test program.\\n\\n");
    
    shell_run();
}

/* Command execution demo */
void demo_command_execution(void) {
    printf("\\n=== Command Execution Demo ===\\n");
    
    const char* demo_commands[] = {
        "echo Welcome to IKOS Shell!",
        "set DEMO_VAR=hello",
        "echo The variable is: $DEMO_VAR",
        "alias ll=ls -l",
        "pwd",
        "help echo",
        NULL
    };
    
    for (int i = 0; demo_commands[i]; i++) {
        printf("$ %s\\n", demo_commands[i]);
        int result = shell_execute_command(demo_commands[i]);
        printf("(exit code: %d)\\n\\n", result);
    }
}
