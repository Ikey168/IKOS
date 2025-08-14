/* IKOS File System Commands Implementation
 * Command-line utilities for filesystem operations
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "fs_user_api.h"

/* ===== Command Function Prototypes ===== */
int cmd_mkdir(int argc, char* argv[]);
int cmd_rmdir(int argc, char* argv[]);
int cmd_ls(int argc, char* argv[]);
int cmd_cd(int argc, char* argv[]);
int cmd_pwd(int argc, char* argv[]);
int cmd_touch(int argc, char* argv[]);
int cmd_rm(int argc, char* argv[]);
int cmd_cp(int argc, char* argv[]);
int cmd_mv(int argc, char* argv[]);
int cmd_cat(int argc, char* argv[]);
int cmd_echo(int argc, char* argv[]);
int cmd_stat(int argc, char* argv[]);
int cmd_chmod(int argc, char* argv[]);
int cmd_find(int argc, char* argv[]);
int cmd_help(int argc, char* argv[]);

/* Command table */
static const fs_command_t commands[] = {
    {"mkdir", cmd_mkdir, "Create directories", "mkdir [-p] <directory>..."},
    {"rmdir", cmd_rmdir, "Remove empty directories", "rmdir <directory>..."},
    {"ls", cmd_ls, "List directory contents", "ls [-l] [-a] [directory]..."},
    {"cd", cmd_cd, "Change current directory", "cd [directory]"},
    {"pwd", cmd_pwd, "Print working directory", "pwd"},
    {"touch", cmd_touch, "Create empty files or update timestamps", "touch <file>..."},
    {"rm", cmd_rm, "Remove files", "rm [-r] <file>..."},
    {"cp", cmd_cp, "Copy files", "cp <source> <destination>"},
    {"mv", cmd_mv, "Move/rename files", "mv <source> <destination>"},
    {"cat", cmd_cat, "Display file contents", "cat <file>..."},
    {"echo", cmd_echo, "Write text to file", "echo <text> [> file]"},
    {"stat", cmd_stat, "Display file statistics", "stat <file>..."},
    {"chmod", cmd_chmod, "Change file permissions", "chmod <mode> <file>..."},
    {"find", cmd_find, "Find files and directories", "find <directory> [-name pattern]"},
    {"help", cmd_help, "Show command help", "help [command]"},
    {NULL, NULL, NULL, NULL}
};

/* ===== Utility Functions ===== */

static void print_error(const char* command, const char* message) {
    printf("%s: %s\\n", command, message);
}

static void print_permission_string(uint32_t perm, char* str) {
    str[0] = (perm & FS_PERM_RUSR) ? 'r' : '-';
    str[1] = (perm & FS_PERM_WUSR) ? 'w' : '-';
    str[2] = (perm & FS_PERM_XUSR) ? 'x' : '-';
    str[3] = (perm & FS_PERM_RGRP) ? 'r' : '-';
    str[4] = (perm & FS_PERM_WGRP) ? 'w' : '-';
    str[5] = (perm & FS_PERM_XGRP) ? 'x' : '-';
    str[6] = (perm & FS_PERM_ROTH) ? 'r' : '-';
    str[7] = (perm & FS_PERM_WOTH) ? 'w' : '-';
    str[8] = (perm & FS_PERM_XOTH) ? 'x' : '-';
    str[9] = '\0';
}

static bool has_option(int argc, char* argv[], const char* option) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], option) == 0) {
            return true;
        }
    }
    return false;
}

/* ===== Command Implementations ===== */

int cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        print_error("mkdir", "missing operand");
        printf("Usage: %s\\n", commands[0].usage);
        return 1;
    }
    
    bool create_parents = has_option(argc, argv, "-p");
    int result = 0;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue; /* Skip options */
        
        if (create_parents) {
            /* Create parent directories if needed */
            char* path_copy = strdup(argv[i]);
            char* parent = fs_dirname(path_copy);
            
            if (parent && strcmp(parent, ".") != 0 && strcmp(parent, "/") != 0) {
                if (!fs_exists(parent)) {
                    char* mkdir_args[] = {"mkdir", "-p", parent};
                    cmd_mkdir(3, mkdir_args);
                }
            }
            
            free(path_copy);
            free(parent);
        }
        
        if (fs_mkdir(argv[i], FS_PERM_755) != 0) {
            print_error("mkdir", fs_error_string(fs_get_last_error()));
            result = 1;
        }
    }
    
    return result;
}

int cmd_rmdir(int argc, char* argv[]) {
    if (argc < 2) {
        print_error("rmdir", "missing operand");
        printf("Usage: %s\\n", commands[1].usage);
        return 1;
    }
    
    int result = 0;
    
    for (int i = 1; i < argc; i++) {
        if (fs_rmdir(argv[i]) != 0) {
            print_error("rmdir", fs_error_string(fs_get_last_error()));
            result = 1;
        }
    }
    
    return result;
}

int cmd_ls(int argc, char* argv[]) {
    bool long_format = has_option(argc, argv, "-l");
    bool show_all = has_option(argc, argv, "-a");
    const char* path = ".";
    
    /* Find the directory path (first non-option argument) */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            path = argv[i];
            break;
        }
    }
    
    fs_dirent_t entries[256];
    int count = fs_ls(path, entries, 256);
    
    if (count < 0) {
        print_error("ls", fs_error_string(fs_get_last_error()));
        return 1;
    }
    
    for (int i = 0; i < count; i++) {
        /* Skip hidden files unless -a is specified */
        if (!show_all && entries[i].name[0] == '.') {
            continue;
        }
        
        if (long_format) {
            char perm_str[10];
            char size_str[32];
            char time_str[32];
            
            print_permission_string(entries[i].permissions, perm_str);
            fs_format_size(entries[i].size, size_str, sizeof(size_str));
            fs_format_time(entries[i].mtime, time_str, sizeof(time_str));
            
            printf("%c%s %8s %s %s\\n",
                   fs_type_char(entries[i].type),
                   perm_str,
                   size_str,
                   time_str,
                   entries[i].name);
        } else {
            printf("%s  ", entries[i].name);
        }
    }
    
    if (!long_format && count > 0) {
        printf("\\n");
    }
    
    return 0;
}

int cmd_cd(int argc, char* argv[]) {
    const char* path = (argc > 1) ? argv[1] : "/";
    
    if (fs_chdir(path) != 0) {
        print_error("cd", fs_error_string(fs_get_last_error()));
        return 1;
    }
    
    return 0;
}

int cmd_pwd(int argc, char* argv[]) {
    char cwd[VFS_MAX_PATH_LENGTH];
    
    if (fs_getcwd(cwd, sizeof(cwd))) {
        printf("%s\\n", cwd);
        return 0;
    } else {
        print_error("pwd", fs_error_string(fs_get_last_error()));
        return 1;
    }
}

int cmd_touch(int argc, char* argv[]) {
    if (argc < 2) {
        print_error("touch", "missing operand");
        printf("Usage: %s\\n", commands[5].usage);
        return 1;
    }
    
    int result = 0;
    
    for (int i = 1; i < argc; i++) {
        if (fs_touch(argv[i]) != 0) {
            print_error("touch", fs_error_string(fs_get_last_error()));
            result = 1;
        }
    }
    
    return result;
}

int cmd_rm(int argc, char* argv[]) {
    if (argc < 2) {
        print_error("rm", "missing operand");
        printf("Usage: %s\\n", commands[6].usage);
        return 1;
    }
    
    bool recursive = has_option(argc, argv, "-r");
    int result = 0;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue; /* Skip options */
        
        if (recursive && fs_is_directory(argv[i])) {
            /* Remove directory recursively */
            fs_dirent_t entries[256];
            int count = fs_ls(argv[i], entries, 256);
            
            for (int j = 0; j < count; j++) {
                if (strcmp(entries[j].name, ".") == 0 || strcmp(entries[j].name, "..") == 0) {
                    continue;
                }
                
                char full_path[VFS_MAX_PATH_LENGTH];
                snprintf(full_path, sizeof(full_path), "%s/%s", argv[i], entries[j].name);
                
                char* rm_args[] = {"rm", "-r", full_path};
                cmd_rm(3, rm_args);
            }
            
            if (fs_rmdir(argv[i]) != 0) {
                print_error("rm", fs_error_string(fs_get_last_error()));
                result = 1;
            }
        } else {
            if (fs_unlink(argv[i]) != 0) {
                print_error("rm", fs_error_string(fs_get_last_error()));
                result = 1;
            }
        }
    }
    
    return result;
}

int cmd_cp(int argc, char* argv[]) {
    if (argc != 3) {
        print_error("cp", "wrong number of arguments");
        printf("Usage: %s\\n", commands[7].usage);
        return 1;
    }
    
    if (fs_copy(argv[1], argv[2]) < 0) {
        print_error("cp", fs_error_string(fs_get_last_error()));
        return 1;
    }
    
    return 0;
}

int cmd_mv(int argc, char* argv[]) {
    if (argc != 3) {
        print_error("mv", "wrong number of arguments");
        printf("Usage: %s\\n", commands[8].usage);
        return 1;
    }
    
    if (fs_rename(argv[1], argv[2]) != 0) {
        print_error("mv", fs_error_string(fs_get_last_error()));
        return 1;
    }
    
    return 0;
}

int cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        print_error("cat", "missing operand");
        printf("Usage: %s\\n", commands[9].usage);
        return 1;
    }
    
    int result = 0;
    
    for (int i = 1; i < argc; i++) {
        char buffer[4096];
        ssize_t bytes_read = fs_read_file(argv[i], buffer, sizeof(buffer) - 1);
        
        if (bytes_read < 0) {
            print_error("cat", fs_error_string(fs_get_last_error()));
            result = 1;
        } else {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
        }
    }
    
    return result;
}

int cmd_echo(int argc, char* argv[]) {
    if (argc < 2) {
        printf("\\n");
        return 0;
    }
    
    /* Check for output redirection */
    bool redirect = false;
    char* output_file = NULL;
    int text_end = argc;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            redirect = true;
            output_file = argv[i + 1];
            text_end = i;
            break;
        }
    }
    
    /* Build the text string */
    char text[1024] = "";
    for (int i = 1; i < text_end; i++) {
        if (i > 1) strcat(text, " ");
        strcat(text, argv[i]);
    }
    strcat(text, "\n");
    
    if (redirect && output_file) {
        if (fs_write_file(output_file, text, strlen(text)) < 0) {
            print_error("echo", fs_error_string(fs_get_last_error()));
            return 1;
        }
    } else {
        printf("%s", text);
    }
    
    return 0;
}

int cmd_stat(int argc, char* argv[]) {
    if (argc < 2) {
        print_error("stat", "missing operand");
        printf("Usage: %s\\n", commands[11].usage);
        return 1;
    }
    
    int result = 0;
    
    for (int i = 1; i < argc; i++) {
        vfs_stat_t stat;
        if (fs_stat(argv[i], &stat) != 0) {
            print_error("stat", fs_error_string(fs_get_last_error()));
            result = 1;
            continue;
        }
        
        char perm_str[10];
        char size_str[32];
        char time_str[32];
        
        print_permission_string(stat.st_perm, perm_str);
        fs_format_size(stat.st_size, size_str, sizeof(size_str));
        fs_format_time(stat.st_mtime, time_str, sizeof(time_str));
        
        printf("  File: %s\\n", argv[i]);
        printf("  Size: %s\\n", size_str);
        printf("  Type: %s\\n", fs_type_string(stat.st_mode));
        printf("Access: %c%s\\n", fs_type_char(stat.st_mode), perm_str);
        printf("Modify: %s\\n", time_str);
        printf("\\n");
    }
    
    return result;
}

int cmd_chmod(int argc, char* argv[]) {
    if (argc < 3) {
        print_error("chmod", "missing operand");
        printf("Usage: %s\\n", commands[12].usage);
        return 1;
    }
    
    /* Parse mode (simple octal parsing) */
    uint32_t mode = 0;
    const char* mode_str = argv[1];
    
    if (strlen(mode_str) == 3) {
        for (int i = 0; i < 3; i++) {
            if (mode_str[i] >= '0' && mode_str[i] <= '7') {
                mode = (mode << 3) | (mode_str[i] - '0');
            } else {
                print_error("chmod", "invalid mode");
                return 1;
            }
        }
    } else {
        print_error("chmod", "invalid mode format");
        return 1;
    }
    
    int result = 0;
    
    for (int i = 2; i < argc; i++) {
        if (fs_chmod(argv[i], mode) != 0) {
            print_error("chmod", fs_error_string(fs_get_last_error()));
            result = 1;
        }
    }
    
    return result;
}

int cmd_find(int argc, char* argv[]) {
    if (argc < 2) {
        print_error("find", "missing operand");
        printf("Usage: %s\\n", commands[13].usage);
        return 1;
    }
    
    const char* search_dir = argv[1];
    const char* pattern = NULL;
    
    /* Check for -name option */
    for (int i = 2; i < argc - 1; i++) {
        if (strcmp(argv[i], "-name") == 0) {
            pattern = argv[i + 1];
            break;
        }
    }
    
    /* Simple recursive directory traversal */
    fs_dirent_t entries[256];
    int count = fs_ls(search_dir, entries, 256);
    
    if (count < 0) {
        print_error("find", fs_error_string(fs_get_last_error()));
        return 1;
    }
    
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, ".") == 0 || strcmp(entries[i].name, "..") == 0) {
            continue;
        }
        
        char full_path[VFS_MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", search_dir, entries[i].name);
        
        /* Check if name matches pattern (simple substring match) */
        bool matches = true;
        if (pattern) {
            matches = (strstr(entries[i].name, pattern) != NULL);
        }
        
        if (matches) {
            printf("%s\\n", full_path);
        }
        
        /* Recurse into subdirectories */
        if (entries[i].type == VFS_FILE_TYPE_DIRECTORY) {
            char* find_args[] = {"find", full_path};
            if (pattern) {
                char* find_args_with_pattern[] = {"find", full_path, "-name", (char*)pattern};
                cmd_find(4, find_args_with_pattern);
            } else {
                cmd_find(2, find_args);
            }
        }
    }
    
    return 0;
}

int cmd_help(int argc, char* argv[]) {
    if (argc == 1) {
        printf("Available commands:\\n");
        for (int i = 0; commands[i].name; i++) {
            printf("  %-10s - %s\\n", commands[i].name, commands[i].description);
        }
        printf("\\nUse 'help <command>' for detailed usage information.\\n");
    } else {
        const char* cmd_name = argv[1];
        for (int i = 0; commands[i].name; i++) {
            if (strcmp(commands[i].name, cmd_name) == 0) {
                printf("%s - %s\\n", commands[i].name, commands[i].description);
                printf("Usage: %s\\n", commands[i].usage);
                return 0;
            }
        }
        printf("Unknown command: %s\\n", cmd_name);
        return 1;
    }
    
    return 0;
}

/* ===== Command Dispatcher ===== */

int fs_execute_command(const char* cmdline) {
    if (!cmdline || strlen(cmdline) == 0) {
        return 0;
    }
    
    /* Parse command line into arguments */
    char* line_copy = strdup(cmdline);
    char* argv[64];
    int argc = 0;
    
    char* token = strtok(line_copy, " \t\n");
    while (token && argc < 63) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
    
    if (argc == 0) {
        free(line_copy);
        return 0;
    }
    
    /* Find and execute command */
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(commands[i].name, argv[0]) == 0) {
            int result = commands[i].handler(argc, argv);
            free(line_copy);
            return result;
        }
    }
    
    printf("Command not found: %s\\n", argv[0]);
    free(line_copy);
    return 1;
}

/* ===== Main Shell Function ===== */

void fs_shell(void) {
    char input[512];
    
    printf("IKOS File System Shell\\n");
    printf("Type 'help' for available commands.\\n\\n");
    
    while (1) {
        char cwd[VFS_MAX_PATH_LENGTH];
        fs_getcwd(cwd, sizeof(cwd));
        printf("%s$ ", cwd);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        /* Remove trailing newline */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }
        
        /* Check for exit commands */
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }
        
        fs_execute_command(input);
    }
    
    printf("Goodbye!\\n");
}
