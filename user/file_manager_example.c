/* IKOS File Manager Example
 * A simple file manager using the filesystem API
 */

#include "fs_user_api.h"
#include "fs_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void show_file_info(const char* path) {
    if (!fs_exists(path)) {
        printf("File not found: %s\\n", path);
        return;
    }
    
    vfs_stat_t stat;
    if (fs_stat(path, &stat) == 0) {
        char perm_str[10];
        char size_str[32];
        char time_str[32];
        
        /* Format permission string */
        perm_str[0] = (stat.st_perm & FS_PERM_RUSR) ? 'r' : '-';
        perm_str[1] = (stat.st_perm & FS_PERM_WUSR) ? 'w' : '-';
        perm_str[2] = (stat.st_perm & FS_PERM_XUSR) ? 'x' : '-';
        perm_str[3] = (stat.st_perm & FS_PERM_RGRP) ? 'r' : '-';
        perm_str[4] = (stat.st_perm & FS_PERM_WGRP) ? 'w' : '-';
        perm_str[5] = (stat.st_perm & FS_PERM_XGRP) ? 'x' : '-';
        perm_str[6] = (stat.st_perm & FS_PERM_ROTH) ? 'r' : '-';
        perm_str[7] = (stat.st_perm & FS_PERM_WOTH) ? 'w' : '-';
        perm_str[8] = (stat.st_perm & FS_PERM_XOTH) ? 'x' : '-';
        perm_str[9] = '\\0';
        
        fs_format_size(stat.st_size, size_str, sizeof(size_str));
        fs_format_time(stat.st_mtime, time_str, sizeof(time_str));
        
        printf("\\nFile Information: %s\\n", path);
        printf("Type: %s\\n", fs_type_string(stat.st_mode));
        printf("Size: %s\\n", size_str);
        printf("Permissions: %c%s\\n", fs_type_char(stat.st_mode), perm_str);
        printf("Modified: %s\\n", time_str);
    }
}

void browse_directory(const char* path) {
    printf("\\nContents of %s:\\n", path);
    printf("%-30s %10s %s\\n", "Name", "Size", "Type");
    printf("%-30s %10s %s\\n", "----", "----", "----");
    
    fs_dirent_t entries[256];
    int count = fs_ls(path, entries, 256);
    
    if (count < 0) {
        printf("Error reading directory: %s\\n", fs_error_string(fs_get_last_error()));
        return;
    }
    
    for (int i = 0; i < count; i++) {
        char size_str[32];
        fs_format_size(entries[i].size, size_str, sizeof(size_str));
        
        printf("%-30s %10s %s\\n",
               entries[i].name,
               size_str,
               fs_type_string(entries[i].type));
    }
}

void create_sample_structure(void) {
    printf("Creating sample directory structure...\\n");
    
    /* Create directories */
    fs_mkdir("/projects", FS_PERM_755);
    fs_mkdir("/projects/ikos_os", FS_PERM_755);
    fs_mkdir("/projects/ikos_os/src", FS_PERM_755);
    fs_mkdir("/projects/ikos_os/docs", FS_PERM_755);
    fs_mkdir("/projects/ikos_os/tests", FS_PERM_755);
    
    /* Create some files */
    fs_write_file("/projects/ikos_os/README.md", 
                  "# IKOS Operating System\\n\\nA simple operating system for educational purposes.\\n", 70);
    
    fs_write_file("/projects/ikos_os/src/kernel.c",
                  "#include <stdio.h>\\n\\nint main() {\\n    printf(\\"Hello, IKOS!\\\\n\\");\\n    return 0;\\n}\\n", 80);
    
    fs_write_file("/projects/ikos_os/docs/design.txt",
                  "IKOS OS Design Document\\n\\nThis document describes the architecture of IKOS OS.\\n", 75);
    
    fs_write_file("/projects/ikos_os/tests/test_boot.c",
                  "#include <assert.h>\\n\\nvoid test_boot() {\\n    assert(1 == 1);\\n}\\n", 60);
    
    printf("Sample structure created successfully!\\n");
}

void file_manager_menu(void) {
    char input[256];
    char current_path[VFS_MAX_PATH_LENGTH];
    
    fs_getcwd(current_path, sizeof(current_path));
    
    while (1) {
        printf("\\n=== IKOS File Manager ===\\n");
        printf("Current directory: %s\\n\\n", current_path);
        printf("1. List directory contents\\n");
        printf("2. Change directory\\n");
        printf("3. Create directory\\n");
        printf("4. Create file\\n");
        printf("5. Show file info\\n");
        printf("6. Copy file\\n");
        printf("7. Move/rename file\\n");
        printf("8. Delete file\\n");
        printf("9. View file contents\\n");
        printf("10. Create sample structure\\n");
        printf("11. File system shell\\n");
        printf("0. Exit\\n\\n");
        printf("Choose an option: ");
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        int choice = atoi(input);
        
        switch (choice) {
            case 1: {
                browse_directory(current_path);
                break;
            }
            
            case 2: {
                printf("Enter directory path: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\\n")] = '\\0';
                    if (fs_chdir(input) == 0) {
                        fs_getcwd(current_path, sizeof(current_path));
                        printf("Changed to %s\\n", current_path);
                    } else {
                        printf("Error: %s\\n", fs_error_string(fs_get_last_error()));
                    }
                }
                break;
            }
            
            case 3: {
                printf("Enter directory name: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\\n")] = '\\0';
                    if (fs_mkdir(input, FS_PERM_755) == 0) {
                        printf("Directory created: %s\\n", input);
                    } else {
                        printf("Error: %s\\n", fs_error_string(fs_get_last_error()));
                    }
                }
                break;
            }
            
            case 4: {
                printf("Enter filename: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\\n")] = '\\0';
                    if (fs_touch(input) == 0) {
                        printf("File created: %s\\n", input);
                    } else {
                        printf("Error: %s\\n", fs_error_string(fs_get_last_error()));
                    }
                }
                break;
            }
            
            case 5: {
                printf("Enter file path: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\\n")] = '\\0';
                    show_file_info(input);
                }
                break;
            }
            
            case 6: {
                char source[256], dest[256];
                printf("Enter source file: ");
                if (fgets(source, sizeof(source), stdin)) {
                    source[strcspn(source, "\\n")] = '\\0';
                    printf("Enter destination: ");
                    if (fgets(dest, sizeof(dest), stdin)) {
                        dest[strcspn(dest, "\\n")] = '\\0';
                        if (fs_copy(source, dest) >= 0) {
                            printf("File copied successfully\\n");
                        } else {
                            printf("Error: %s\\n", fs_error_string(fs_get_last_error()));
                        }
                    }
                }
                break;
            }
            
            case 7: {
                char source[256], dest[256];
                printf("Enter source file: ");
                if (fgets(source, sizeof(source), stdin)) {
                    source[strcspn(source, "\\n")] = '\\0';
                    printf("Enter destination: ");
                    if (fgets(dest, sizeof(dest), stdin)) {
                        dest[strcspn(dest, "\\n")] = '\\0';
                        if (fs_rename(source, dest) == 0) {
                            printf("File moved/renamed successfully\\n");
                        } else {
                            printf("Error: %s\\n", fs_error_string(fs_get_last_error()));
                        }
                    }
                }
                break;
            }
            
            case 8: {
                printf("Enter file path: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\\n")] = '\\0';
                    if (fs_is_directory(input)) {
                        if (fs_rmdir(input) == 0) {
                            printf("Directory deleted: %s\\n", input);
                        } else {
                            printf("Error: %s\\n", fs_error_string(fs_get_last_error()));
                        }
                    } else {
                        if (fs_unlink(input) == 0) {
                            printf("File deleted: %s\\n", input);
                        } else {
                            printf("Error: %s\\n", fs_error_string(fs_get_last_error()));
                        }
                    }
                }
                break;
            }
            
            case 9: {
                printf("Enter file path: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\\n")] = '\\0';
                    
                    char buffer[4096];
                    ssize_t bytes = fs_read_file(input, buffer, sizeof(buffer) - 1);
                    if (bytes >= 0) {
                        buffer[bytes] = '\\0';
                        printf("\\nFile contents:\\n");
                        printf("=============\\n");
                        printf("%s", buffer);
                        printf("=============\\n");
                    } else {
                        printf("Error reading file: %s\\n", fs_error_string(fs_get_last_error()));
                    }
                }
                break;
            }
            
            case 10: {
                create_sample_structure();
                break;
            }
            
            case 11: {
                printf("Starting filesystem shell (type 'exit' to return)...\\n");
                fs_shell();
                fs_getcwd(current_path, sizeof(current_path));
                break;
            }
            
            case 0: {
                printf("Exiting file manager...\\n");
                return;
            }
            
            default: {
                printf("Invalid option. Please try again.\\n");
                break;
            }
        }
    }
}

int main(void) {
    printf("IKOS File Manager Example\\n");
    printf("========================\\n");
    
    /* Initialize filesystem */
    fs_init_cwd();
    
    printf("\\nFile Manager initialized. Current working directory: ");
    char cwd[VFS_MAX_PATH_LENGTH];
    if (fs_getcwd(cwd, sizeof(cwd))) {
        printf("%s\\n", cwd);
    } else {
        printf("Unknown\\n");
    }
    
    /* Start the file manager interface */
    file_manager_menu();
    
    /* Cleanup */
    fs_cleanup_cwd();
    
    return 0;
}
