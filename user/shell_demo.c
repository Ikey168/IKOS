/* IKOS Shell Demo Program
 * Demonstrates shell functionality and features
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ikos_shell.h"

/* Demo functions */
void demo_banner(void);
void demo_basic_commands(void);
void demo_environment_variables(void);
void demo_aliases(void);
void demo_history(void);
void demo_interactive(void);

int main(void) {
    printf("\\033[2J\\033[H"); /* Clear screen */
    
    demo_banner();
    
    printf("\\nWelcome to the IKOS Shell Demo!\\n");
    printf("This demo will showcase the shell's capabilities.\\n");
    printf("Press Enter to continue through each section...\\n");
    getchar();
    
    /* Initialize shell */
    shell_init();
    
    /* Run demos */
    demo_basic_commands();
    demo_environment_variables();
    demo_aliases();
    demo_history();
    demo_interactive();
    
    return 0;
}

void demo_banner(void) {
    printf("╔══════════════════════════════════════════════════════════════╗\\n");
    printf("║                         IKOS Shell Demo                     ║\\n");
    printf("║                    Basic CLI Implementation                  ║\\n");
    printf("║                                                              ║\\n");
    printf("║  Features:                                                   ║\\n");
    printf("║  • Command execution with built-in commands                 ║\\n");
    printf("║  • Environment variable management                          ║\\n");
    printf("║  • Command aliases                                          ║\\n");
    printf("║  • Command history                                          ║\\n");
    printf("║  • Filesystem integration                                   ║\\n");
    printf("║  • Process management                                       ║\\n");
    printf("╚══════════════════════════════════════════════════════════════╝\\n");
}

void demo_basic_commands(void) {
    printf("\\n\\033[1;34m=== Basic Commands Demo ===\\033[0m\\n");
    printf("Testing fundamental shell commands...\\n\\n");
    
    const char* commands[] = {
        "echo Hello, IKOS Shell!",
        "pwd",
        "echo Welcome to the basic shell demonstration",
        "help echo",
        NULL
    };
    
    for (int i = 0; commands[i]; i++) {
        printf("\\033[1;32m$ %s\\033[0m\\n", commands[i]);
        shell_execute_command(commands[i]);
        printf("\\n");
        usleep(500000); /* 0.5 second delay */
    }
    
    printf("Press Enter to continue to environment variables demo...\\n");
    getchar();
}

void demo_environment_variables(void) {
    printf("\\n\\033[1;34m=== Environment Variables Demo ===\\033[0m\\n");
    printf("Demonstrating environment variable management...\\n\\n");
    
    const char* commands[] = {
        "set DEMO_VAR=Hello_World",
        "echo Variable DEMO_VAR is: $DEMO_VAR",
        "set PATH=/usr/bin:/bin:$PATH",
        "echo Updated PATH: $PATH",
        "export SHELL_VERSION=1.0",
        "echo Shell version: $SHELL_VERSION",
        "unset DEMO_VAR",
        "echo After unset: $DEMO_VAR",
        NULL
    };
    
    for (int i = 0; commands[i]; i++) {
        printf("\\033[1;32m$ %s\\033[0m\\n", commands[i]);
        shell_execute_command(commands[i]);
        printf("\\n");
        usleep(500000);
    }
    
    printf("Press Enter to continue to aliases demo...\\n");
    getchar();
}

void demo_aliases(void) {
    printf("\\n\\033[1;34m=== Command Aliases Demo ===\\033[0m\\n");
    printf("Creating and using command aliases...\\n\\n");
    
    const char* commands[] = {
        "alias ll=ls -l",
        "alias la=ls -la",
        "alias grep=grep --color=auto",
        "alias h=history",
        "echo Aliases created! Testing them:",
        "h",  /* Should show history */
        "unalias ll",
        "echo Alias 'll' has been removed",
        NULL
    };
    
    for (int i = 0; commands[i]; i++) {
        printf("\\033[1;32m$ %s\\033[0m\\n", commands[i]);
        shell_execute_command(commands[i]);
        printf("\\n");
        usleep(500000);
    }
    
    printf("Press Enter to continue to history demo...\\n");
    getchar();
}

void demo_history(void) {
    printf("\\n\\033[1;34m=== Command History Demo ===\\033[0m\\n");
    printf("Demonstrating command history functionality...\\n\\n");
    
    /* Add some commands to history */
    shell_add_history("echo First command");
    shell_add_history("pwd");
    shell_add_history("set TEST=value");
    shell_add_history("echo $TEST");
    shell_add_history("ls -l");
    
    printf("\\033[1;32m$ history\\033[0m\\n");
    shell_execute_command("history");
    printf("\\n");
    
    printf("Command history shows all previously executed commands.\\n");
    printf("In an interactive session, you could use arrow keys to navigate.\\n\\n");
    
    printf("Press Enter to continue to interactive demo...\\n");
    getchar();
}

void demo_interactive(void) {
    printf("\\n\\033[1;34m=== Interactive Shell Demo ===\\033[0m\\n");
    printf("Now you can try the shell interactively!\\n");
    printf("\\nAvailable commands:\\n");
    printf("  • echo <text>    - Print text\\n");
    printf("  • pwd            - Show current directory\\n");
    printf("  • cd <dir>       - Change directory\\n");
    printf("  • set VAR=value  - Set environment variable\\n");
    printf("  • echo $VAR      - Display variable value\\n");
    printf("  • alias name=cmd - Create command alias\\n");
    printf("  • history        - Show command history\\n");
    printf("  • help [cmd]     - Show help\\n");
    printf("  • clear          - Clear screen\\n");
    printf("  • exit           - Exit shell\\n");
    printf("\\nTry some commands below. Type 'exit' to finish the demo.\\n\\n");
    
    /* Start interactive shell */
    shell_run();
    
    printf("\\n\\033[1;33mThank you for trying the IKOS Shell demo!\\033[0m\\n");
    printf("This demonstrates the basic CLI implementation for Issue #36.\\n");
    printf("\\nKey features implemented:\\n");
    printf("✓ Shell prompt and command execution\\n");
    printf("✓ Built-in commands (echo, pwd, cd, set, etc.)\\n");
    printf("✓ Environment variable management\\n");
    printf("✓ Command aliases\\n");
    printf("✓ Command history\\n");
    printf("✓ Filesystem integration\\n");
    printf("✓ Process management foundation\\n");
    printf("\\nThe shell is ready for integration with the IKOS operating system!\\n");
}
