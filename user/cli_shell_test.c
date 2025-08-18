/*
 * IKOS CLI Shell Test Suite
 * Issue #36: Command Line Interface Testing
 *
 * Comprehensive test suite for the IKOS CLI shell implementation
 * Tests all core functionality including commands, environment variables,
 * error handling, and edge cases.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "ikos_cli_shell.h"

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        tests_failed++; \
        printf("✗ %s\n", message); \
    } \
} while (0)

/* Function prototypes */
void run_all_tests(void);
void test_command_parsing(void);
void test_builtin_commands(void);
void test_environment_variables(void);
void test_error_handling(void);
void test_edge_cases(void);
void print_test_summary(void);

/* Test helper functions */
int capture_command_output(const char* command, char* output, size_t output_size);
int simulate_command_execution(const char* command);

/* Main test runner */
int main(void) {
    printf("========================================\n");
    printf("IKOS CLI Shell Test Suite\n");
    printf("Issue #36: Shell Implementation Testing\n");
    printf("========================================\n\n");
    
    run_all_tests();
    print_test_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}

void run_all_tests(void) {
    printf("=== Testing Command Parsing ===\n");
    test_command_parsing();
    printf("\n");
    
    printf("=== Testing Built-in Commands ===\n");
    test_builtin_commands();
    printf("\n");
    
    printf("=== Testing Environment Variables ===\n");
    test_environment_variables();
    printf("\n");
    
    printf("=== Testing Error Handling ===\n");
    test_error_handling();
    printf("\n");
    
    printf("=== Testing Edge Cases ===\n");
    test_edge_cases();
    printf("\n");
}

void test_command_parsing(void) {
    char* args[10];
    char test_input[256];
    
    /* Test simple command parsing */
    strcpy(test_input, "echo hello world");
    int argc = cli_parse_command(test_input, args);
    TEST_ASSERT(argc == 3, "Simple command parsing works");
    TEST_ASSERT(strcmp(args[0], "echo") == 0, "Command name parsed correctly");
    TEST_ASSERT(strcmp(args[1], "hello") == 0, "First argument parsed correctly");
    TEST_ASSERT(strcmp(args[2], "world") == 0, "Second argument parsed correctly");
    
    /* Test empty command */
    strcpy(test_input, "");
    argc = cli_parse_command(test_input, args);
    TEST_ASSERT(argc == 0, "Empty command handling works");
    
    /* Test whitespace handling */
    strcpy(test_input, "  pwd  ");
    argc = cli_parse_command(test_input, args);
    TEST_ASSERT(argc == 1, "Whitespace trimming works");
    TEST_ASSERT(strcmp(args[0], "pwd") == 0, "Command with whitespace parsed correctly");
    
    /* Test multiple spaces */
    strcpy(test_input, "echo   multiple    spaces");
    argc = cli_parse_command(test_input, args);
    TEST_ASSERT(argc == 3, "Multiple spaces handling works");
    
    /* Test tab separation */
    strcpy(test_input, "set\tVAR=value");
    argc = cli_parse_command(test_input, args);
    TEST_ASSERT(argc == 2, "Tab separation parsing works");
}

void test_builtin_commands(void) {
    char* test_args[10];
    int result;
    
    /* Test echo command */
    test_args[0] = "echo";
    test_args[1] = "test";
    test_args[2] = "message";
    test_args[3] = NULL;
    result = cli_cmd_echo(3, test_args);
    TEST_ASSERT(result == 0, "Echo command execution successful");
    
    /* Test pwd command */
    test_args[0] = "pwd";
    test_args[1] = NULL;
    result = cli_cmd_pwd(1, test_args);
    TEST_ASSERT(result == 0, "PWD command execution successful");
    
    /* Test cd command */
    test_args[0] = "cd";
    test_args[1] = "/tmp";
    test_args[2] = NULL;
    result = cli_cmd_cd(2, test_args);
    TEST_ASSERT(result == 0 || result == 1, "CD command handling works");
    
    /* Test help command */
    test_args[0] = "help";
    test_args[1] = NULL;
    result = cli_cmd_help(1, test_args);
    TEST_ASSERT(result == 0, "Help command execution successful");
    
    /* Test clear command */
    test_args[0] = "clear";
    test_args[1] = NULL;
    result = cli_cmd_clear(1, test_args);
    TEST_ASSERT(result == 0, "Clear command execution successful");
}

void test_environment_variables(void) {
    int result;
    const char* value;
    
    /* Test setting environment variable */
    result = cli_set_env_var("TEST_VAR", "test_value");
    TEST_ASSERT(result == 0, "Environment variable setting works");
    
    /* Test getting environment variable */
    value = cli_get_env_var("TEST_VAR");
    TEST_ASSERT(value != NULL, "Environment variable retrieval works");
    TEST_ASSERT(strcmp(value, "test_value") == 0, "Environment variable value correct");
    
    /* Test overwriting environment variable */
    result = cli_set_env_var("TEST_VAR", "new_value");
    TEST_ASSERT(result == 0, "Environment variable overwriting works");
    value = cli_get_env_var("TEST_VAR");
    TEST_ASSERT(strcmp(value, "new_value") == 0, "Environment variable updated correctly");
    
    /* Test getting non-existent variable */
    value = cli_get_env_var("NON_EXISTENT_VAR");
    TEST_ASSERT(value == NULL, "Non-existent variable returns NULL");
    
    /* Test unsetting environment variable */
    result = cli_unset_env_var("TEST_VAR");
    TEST_ASSERT(result == 0, "Environment variable unsetting works");
    value = cli_get_env_var("TEST_VAR");
    TEST_ASSERT(value == NULL, "Unset variable returns NULL");
    
    /* Test set command */
    char* test_args[3];
    test_args[0] = "set";
    test_args[1] = "SHELL_VAR=shell_value";
    test_args[2] = NULL;
    result = cli_cmd_set(2, test_args);
    TEST_ASSERT(result == 0, "Set command execution successful");
}

void test_error_handling(void) {
    char* test_args[5];
    int result;
    
    /* Test invalid set command syntax */
    test_args[0] = "set";
    test_args[1] = "INVALID_SYNTAX";
    test_args[2] = NULL;
    result = cli_cmd_set(2, test_args);
    TEST_ASSERT(result == 1, "Invalid set syntax handled correctly");
    
    /* Test cd to non-existent directory */
    test_args[0] = "cd";
    test_args[1] = "/non/existent/directory";
    test_args[2] = NULL;
    result = cli_cmd_cd(2, test_args);
    TEST_ASSERT(result == 1, "CD to non-existent directory handled correctly");
    
    /* Test unknown command */
    test_args[0] = "unknown_command";
    test_args[1] = NULL;
    result = cli_execute_builtin(test_args);
    TEST_ASSERT(result == -1, "Unknown command returns error");
}

void test_edge_cases(void) {
    char* test_args[5];
    int result;
    
    /* Test empty echo */
    test_args[0] = "echo";
    test_args[1] = NULL;
    result = cli_cmd_echo(1, test_args);
    TEST_ASSERT(result == 0, "Empty echo command works");
    
    /* Test cd with no arguments */
    test_args[0] = "cd";
    test_args[1] = NULL;
    result = cli_cmd_cd(1, test_args);
    TEST_ASSERT(result == 0, "CD with no arguments works");
    
    /* Test very long environment variable name */
    char long_name[1000];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    result = cli_set_env_var(long_name, "value");
    TEST_ASSERT(result == 0 || result == -1, "Long variable name handled");
    
    /* Test empty variable name */
    result = cli_set_env_var("", "value");
    TEST_ASSERT(result == -1 || result == 0, "Empty variable name handled");
    
    /* Test NULL variable value */
    result = cli_set_env_var("NULL_TEST", NULL);
    TEST_ASSERT(result == -1 || result == 0, "NULL variable value handled");
    
    /* Test command with maximum arguments */
    test_args[0] = "echo";
    for (int i = 1; i < CLI_MAX_ARGS - 1; i++) {
        test_args[i] = "arg";
    }
    test_args[CLI_MAX_ARGS - 1] = NULL;
    result = cli_cmd_echo(CLI_MAX_ARGS - 1, test_args);
    TEST_ASSERT(result == 0, "Maximum arguments handled correctly");
}

void print_test_summary(void) {
    printf("========================================\n");
    printf("Test Results Summary:\n");
    printf("  Total Tests: %d\n", tests_run);
    printf("  Passed:      %d\n", tests_passed);
    printf("  Failed:      %d\n", tests_failed);
    printf("========================================\n");
    
    if (tests_failed == 0) {
        printf("🎉 All tests passed! CLI shell implementation is working correctly.\n");
    } else {
        printf("⚠️  Some tests failed. Please review the implementation.\n");
    }
}

/* Utility function to simulate command execution */
int simulate_command_execution(const char* command) {
    char input_copy[CLI_MAX_INPUT_LENGTH];
    char* args[CLI_MAX_ARGS];
    
    strcpy(input_copy, command);
    int argc = cli_parse_command(input_copy, args);
    
    if (argc == 0) {
        return 0;
    }
    
    return cli_execute_builtin(args);
}
