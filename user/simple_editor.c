/* IKOS Simple Text Editor Example
 * A basic text editor using the filesystem API
 */

#include "fs_user_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 1000
#define MAX_LINE_LENGTH 256

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int num_lines;
    bool modified;
    char filename[VFS_MAX_PATH_LENGTH];
} text_buffer_t;

static text_buffer_t buffer = {0};

void init_buffer(void) {
    buffer.num_lines = 0;
    buffer.modified = false;
    buffer.filename[0] = '\\0';
}

bool load_file(const char* filename) {
    if (!fs_exists(filename)) {
        printf("File does not exist: %s\\n", filename);
        return false;
    }
    
    int fd = fs_open(filename, FS_O_RDONLY, 0);
    if (fd < 0) {
        printf("Error opening file: %s\\n", fs_error_string(fs_get_last_error()));
        return false;
    }
    
    init_buffer();
    strcpy(buffer.filename, filename);
    
    char file_buffer[4096];
    ssize_t bytes_read = fs_read(fd, file_buffer, sizeof(file_buffer) - 1);
    fs_close(fd);
    
    if (bytes_read < 0) {
        printf("Error reading file: %s\\n", fs_error_string(fs_get_last_error()));
        return false;
    }
    
    file_buffer[bytes_read] = '\\0';
    
    /* Parse lines */
    char* line = strtok(file_buffer, "\\n");
    while (line && buffer.num_lines < MAX_LINES) {
        strncpy(buffer.lines[buffer.num_lines], line, MAX_LINE_LENGTH - 1);
        buffer.lines[buffer.num_lines][MAX_LINE_LENGTH - 1] = '\\0';
        buffer.num_lines++;
        line = strtok(NULL, "\\n");
    }
    
    printf("Loaded %d lines from %s\\n", buffer.num_lines, filename);
    return true;
}

bool save_file(const char* filename) {
    int fd = fs_open(filename, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, FS_PERM_644);
    if (fd < 0) {
        printf("Error creating file: %s\\n", fs_error_string(fs_get_last_error()));
        return false;
    }
    
    for (int i = 0; i < buffer.num_lines; i++) {
        ssize_t written = fs_write(fd, buffer.lines[i], strlen(buffer.lines[i]));
        if (written < 0) {
            printf("Error writing to file: %s\\n", fs_error_string(fs_get_last_error()));
            fs_close(fd);
            return false;
        }
        
        if (i < buffer.num_lines - 1 || strlen(buffer.lines[i]) > 0) {
            fs_write(fd, "\\n", 1);
        }
    }
    
    fs_close(fd);
    
    strcpy(buffer.filename, filename);
    buffer.modified = false;
    printf("Saved %d lines to %s\\n", buffer.num_lines, filename);
    return true;
}

void display_buffer(int start_line, int num_display_lines) {
    printf("\\n--- File: %s %s---\\n", 
           buffer.filename[0] ? buffer.filename : "[Untitled]",
           buffer.modified ? "[Modified] " : "");
    
    int end_line = start_line + num_display_lines;
    if (end_line > buffer.num_lines) {
        end_line = buffer.num_lines;
    }
    
    for (int i = start_line; i < end_line; i++) {
        printf("%3d: %s\\n", i + 1, buffer.lines[i]);
    }
    
    if (buffer.num_lines == 0) {
        printf("  1: [Empty file]\\n");
    }
    
    printf("--- Lines %d-%d of %d ---\\n", 
           start_line + 1, end_line, buffer.num_lines);
}

void insert_line(int line_num, const char* text) {
    if (line_num < 0 || line_num > buffer.num_lines || buffer.num_lines >= MAX_LINES) {
        printf("Invalid line number or buffer full\\n");
        return;
    }
    
    /* Shift lines down */
    for (int i = buffer.num_lines; i > line_num; i--) {
        strcpy(buffer.lines[i], buffer.lines[i - 1]);
    }
    
    /* Insert new line */
    strncpy(buffer.lines[line_num], text, MAX_LINE_LENGTH - 1);
    buffer.lines[line_num][MAX_LINE_LENGTH - 1] = '\\0';
    buffer.num_lines++;
    buffer.modified = true;
}

void delete_line(int line_num) {
    if (line_num < 0 || line_num >= buffer.num_lines) {
        printf("Invalid line number\\n");
        return;
    }
    
    /* Shift lines up */
    for (int i = line_num; i < buffer.num_lines - 1; i++) {
        strcpy(buffer.lines[i], buffer.lines[i + 1]);
    }
    
    buffer.num_lines--;
    buffer.modified = true;
}

void edit_line(int line_num, const char* text) {
    if (line_num < 0 || line_num >= buffer.num_lines) {
        printf("Invalid line number\\n");
        return;
    }
    
    strncpy(buffer.lines[line_num], text, MAX_LINE_LENGTH - 1);
    buffer.lines[line_num][MAX_LINE_LENGTH - 1] = '\\0';
    buffer.modified = true;
}

void show_help(void) {
    printf("\\nIKOS Simple Text Editor Commands:\\n");
    printf("=================================\\n");
    printf("l [start] [count] - List lines (default: all)\\n");
    printf("i <line> <text>   - Insert text at line number\\n");
    printf("a <text>          - Append text at end\\n");
    printf("e <line> <text>   - Edit line\\n");
    printf("d <line>          - Delete line\\n");
    printf("s [filename]      - Save file\\n");
    printf("o <filename>      - Open file\\n");
    printf("n                 - New file\\n");
    printf("f <pattern>       - Find text\\n");
    printf("r <old> <new>     - Replace text\\n");
    printf("w                 - Show file info\\n");
    printf("h                 - Show this help\\n");
    printf("q                 - Quit\\n");
    printf("\\nNote: Line numbers start from 1\\n");
}

void find_text(const char* pattern) {
    int found_count = 0;
    
    printf("\\nSearching for '%s':\\n", pattern);
    for (int i = 0; i < buffer.num_lines; i++) {
        if (strstr(buffer.lines[i], pattern)) {
            printf("%3d: %s\\n", i + 1, buffer.lines[i]);
            found_count++;
        }
    }
    
    if (found_count == 0) {
        printf("Text not found.\\n");
    } else {
        printf("Found %d occurrences.\\n", found_count);
    }
}

void replace_text(const char* old_text, const char* new_text) {
    int replaced_count = 0;
    
    for (int i = 0; i < buffer.num_lines; i++) {
        char* pos = strstr(buffer.lines[i], old_text);
        if (pos) {
            char new_line[MAX_LINE_LENGTH];
            
            /* Copy text before the match */
            size_t prefix_len = pos - buffer.lines[i];
            strncpy(new_line, buffer.lines[i], prefix_len);
            new_line[prefix_len] = '\\0';
            
            /* Add replacement text */
            strcat(new_line, new_text);
            
            /* Add text after the match */
            strcat(new_line, pos + strlen(old_text));
            
            /* Update the line */
            strcpy(buffer.lines[i], new_line);
            replaced_count++;
            buffer.modified = true;
        }
    }
    
    printf("Replaced %d occurrences.\\n", replaced_count);
}

void show_file_info(void) {
    printf("\\nFile Information:\\n");
    printf("Filename: %s\\n", buffer.filename[0] ? buffer.filename : "[Untitled]");
    printf("Lines: %d\\n", buffer.num_lines);
    printf("Modified: %s\\n", buffer.modified ? "Yes" : "No");
    
    if (buffer.filename[0] && fs_exists(buffer.filename)) {
        vfs_stat_t stat;
        if (fs_stat(buffer.filename, &stat) == 0) {
            char size_str[32];
            fs_format_size(stat.st_size, size_str, sizeof(size_str));
            printf("File size: %s\\n", size_str);
        }
    }
}

void text_editor_main(void) {
    char input[512];
    char command[64];
    char arg1[256], arg2[256], arg3[256];
    
    init_buffer();
    
    printf("IKOS Simple Text Editor\\n");
    printf("Type 'h' for help, 'q' to quit\\n");
    
    while (1) {
        printf("\\n> ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        /* Parse command */
        int args = sscanf(input, "%s %s %s %s", command, arg1, arg2, arg3);
        if (args == 0) continue;
        
        if (strcmp(command, "q") == 0) {
            if (buffer.modified) {
                printf("File has unsaved changes. Save first? (y/n): ");
                char response[10];
                if (fgets(response, sizeof(response), stdin) && response[0] == 'y') {
                    if (buffer.filename[0]) {
                        save_file(buffer.filename);
                    } else {
                        printf("Enter filename: ");
                        if (fgets(arg1, sizeof(arg1), stdin)) {
                            arg1[strcspn(arg1, "\\n")] = '\\0';
                            save_file(arg1);
                        }
                    }
                }
            }
            break;
        }
        else if (strcmp(command, "h") == 0) {
            show_help();
        }
        else if (strcmp(command, "n") == 0) {
            if (buffer.modified) {
                printf("Current file has unsaved changes. Continue? (y/n): ");
                char response[10];
                if (fgets(response, sizeof(response), stdin) && response[0] != 'y') {
                    continue;
                }
            }
            init_buffer();
            printf("New file created.\\n");
        }
        else if (strcmp(command, "o") == 0 && args >= 2) {
            if (buffer.modified) {
                printf("Current file has unsaved changes. Continue? (y/n): ");
                char response[10];
                if (fgets(response, sizeof(response), stdin) && response[0] != 'y') {
                    continue;
                }
            }
            load_file(arg1);
        }
        else if (strcmp(command, "l") == 0) {
            int start = 0, count = buffer.num_lines;
            if (args >= 2) start = atoi(arg1) - 1;
            if (args >= 3) count = atoi(arg2);
            if (start < 0) start = 0;
            display_buffer(start, count);
        }
        else if (strcmp(command, "i") == 0 && args >= 3) {
            int line_num = atoi(arg1) - 1;
            /* Reconstruct the text from remaining arguments */
            char* text_start = strstr(input, arg2);
            insert_line(line_num, text_start);
            printf("Line inserted.\\n");
        }
        else if (strcmp(command, "a") == 0 && args >= 2) {
            char* text_start = strstr(input, arg1);
            insert_line(buffer.num_lines, text_start);
            printf("Line appended.\\n");
        }
        else if (strcmp(command, "e") == 0 && args >= 3) {
            int line_num = atoi(arg1) - 1;
            char* text_start = strstr(input, arg2);
            edit_line(line_num, text_start);
            printf("Line edited.\\n");
        }
        else if (strcmp(command, "d") == 0 && args >= 2) {
            int line_num = atoi(arg1) - 1;
            delete_line(line_num);
            printf("Line deleted.\\n");
        }
        else if (strcmp(command, "s") == 0) {
            const char* filename = (args >= 2) ? arg1 : buffer.filename;
            if (filename[0]) {
                save_file(filename);
            } else {
                printf("No filename specified.\\n");
            }
        }
        else if (strcmp(command, "f") == 0 && args >= 2) {
            find_text(arg1);
        }
        else if (strcmp(command, "r") == 0 && args >= 3) {
            replace_text(arg1, arg2);
        }
        else if (strcmp(command, "w") == 0) {
            show_file_info();
        }
        else {
            printf("Unknown command or missing arguments. Type 'h' for help.\\n");
        }
    }
    
    printf("Editor closed.\\n");
}

int main(int argc, char* argv[]) {
    /* Initialize filesystem */
    fs_init_cwd();
    
    /* Load file if specified on command line */
    if (argc > 1) {
        if (fs_exists(argv[1])) {
            load_file(argv[1]);
        } else {
            printf("File '%s' does not exist. Starting with empty buffer.\\n", argv[1]);
            strcpy(buffer.filename, argv[1]);
        }
    }
    
    /* Start the editor */
    text_editor_main();
    
    /* Cleanup */
    fs_cleanup_cwd();
    
    return 0;
}
