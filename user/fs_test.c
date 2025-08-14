/* IKOS File System API Test Program
 * Tests all filesystem functionality
 */

#include "fs_user_api.h"
#include "fs_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
void test_directory_operations(void);
void test_file_operations(void);
void test_file_io(void);
void test_metadata_operations(void);
void test_path_utilities(void);
void test_commands(void);
void test_error_handling(void);

int main(void) {
    printf("IKOS File System API Test Suite\\n");
    printf("================================\\n\\n");
    
    /* Initialize filesystem */
    fs_init_cwd();
    
    /* Run test suites */
    test_directory_operations();
    test_file_operations();
    test_file_io();
    test_metadata_operations();
    test_path_utilities();
    test_commands();
    test_error_handling();
    
    /* Print summary */
    printf("\\n================================\\n");
    printf("Test Results: %d passed, %d failed\\n", tests_passed, tests_failed);
    
    if (tests_failed == 0) {
        printf("All tests passed! ✓\\n");
        return 0;
    } else {
        printf("Some tests failed! ✗\\n");
        return 1;
    }
}

void test_directory_operations(void) {
    printf("Testing Directory Operations:\\n");
    printf("-----------------------------\\n");
    
    /* Test mkdir */
    TEST_ASSERT(fs_mkdir("/test_dir", FS_PERM_755) == 0, "Create directory /test_dir");
    TEST_ASSERT(fs_exists("/test_dir"), "Directory /test_dir exists");
    TEST_ASSERT(fs_is_directory("/test_dir"), "Path /test_dir is a directory");
    
    /* Test nested mkdir */
    TEST_ASSERT(fs_mkdir("/test_dir/subdir", FS_PERM_755) == 0, "Create subdirectory");
    TEST_ASSERT(fs_exists("/test_dir/subdir"), "Subdirectory exists");
    
    /* Test chdir and getcwd */
    char original_cwd[VFS_MAX_PATH_LENGTH];
    fs_getcwd(original_cwd, sizeof(original_cwd));
    
    TEST_ASSERT(fs_chdir("/test_dir") == 0, "Change to /test_dir");
    
    char new_cwd[VFS_MAX_PATH_LENGTH];
    fs_getcwd(new_cwd, sizeof(new_cwd));
    TEST_ASSERT(strcmp(new_cwd, "/test_dir") == 0, "Current directory is /test_dir");
    
    /* Test directory listing */
    fs_dirent_t entries[10];
    int count = fs_ls(".", entries, 10);
    TEST_ASSERT(count >= 1, "Directory listing returns entries");
    
    bool found_subdir = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, "subdir") == 0) {
            found_subdir = true;
            break;
        }
    }
    TEST_ASSERT(found_subdir, "Subdirectory found in listing");
    
    /* Return to original directory */
    fs_chdir(original_cwd);
    
    /* Test rmdir */
    TEST_ASSERT(fs_rmdir("/test_dir/subdir") == 0, "Remove subdirectory");
    TEST_ASSERT(!fs_exists("/test_dir/subdir"), "Subdirectory no longer exists");
    TEST_ASSERT(fs_rmdir("/test_dir") == 0, "Remove test directory");
    TEST_ASSERT(!fs_exists("/test_dir"), "Test directory no longer exists");
    
    printf("\\n");
}

void test_file_operations(void) {
    printf("Testing File Operations:\\n");
    printf("------------------------\\n");
    
    /* Test file creation */
    TEST_ASSERT(fs_create_file("/test_file.txt", FS_PERM_644) == 0, "Create file /test_file.txt");
    TEST_ASSERT(fs_exists("/test_file.txt"), "File /test_file.txt exists");
    TEST_ASSERT(fs_is_file("/test_file.txt"), "Path /test_file.txt is a file");
    
    /* Test touch */
    TEST_ASSERT(fs_touch("/touched_file.txt") == 0, "Touch /touched_file.txt");
    TEST_ASSERT(fs_exists("/touched_file.txt"), "Touched file exists");
    
    /* Test file copy */
    const char* test_content = "Hello, World!";
    TEST_ASSERT(fs_write_file("/source.txt", test_content, strlen(test_content)) > 0, "Write to source file");
    TEST_ASSERT(fs_copy("/source.txt", "/dest.txt") > 0, "Copy file");
    TEST_ASSERT(fs_exists("/dest.txt"), "Destination file exists");
    
    /* Test file rename */
    TEST_ASSERT(fs_rename("/dest.txt", "/renamed.txt") == 0, "Rename file");
    TEST_ASSERT(fs_exists("/renamed.txt"), "Renamed file exists");
    TEST_ASSERT(!fs_exists("/dest.txt"), "Original file no longer exists");
    
    /* Test file deletion */
    TEST_ASSERT(fs_unlink("/test_file.txt") == 0, "Delete test file");
    TEST_ASSERT(fs_unlink("/touched_file.txt") == 0, "Delete touched file");
    TEST_ASSERT(fs_unlink("/source.txt") == 0, "Delete source file");
    TEST_ASSERT(fs_unlink("/renamed.txt") == 0, "Delete renamed file");
    
    printf("\\n");
}

void test_file_io(void) {
    printf("Testing File I/O:\\n");
    printf("-----------------\\n");
    
    const char* test_data = "This is test data for file I/O operations.\\n";
    const char* test_file = "/io_test.txt";
    
    /* Test write_file */
    ssize_t written = fs_write_file(test_file, test_data, strlen(test_data));
    TEST_ASSERT(written == (ssize_t)strlen(test_data), "Write entire file");
    
    /* Test read_file */
    char read_buffer[256];
    ssize_t read_bytes = fs_read_file(test_file, read_buffer, sizeof(read_buffer) - 1);
    read_buffer[read_bytes] = '\\0';
    TEST_ASSERT(read_bytes == (ssize_t)strlen(test_data), "Read correct number of bytes");
    TEST_ASSERT(strcmp(read_buffer, test_data) == 0, "Read data matches written data");
    
    /* Test append_file */
    const char* append_data = "Appended data.\\n";
    TEST_ASSERT(fs_append_file(test_file, append_data, strlen(append_data)) == 0, "Append to file");
    
    /* Test file positioning with open/read */
    int fd = fs_open(test_file, FS_O_RDONLY, 0);
    TEST_ASSERT(fd >= 0, "Open file for reading");
    
    char partial_buffer[10];
    ssize_t partial_read = fs_read(fd, partial_buffer, 5);
    TEST_ASSERT(partial_read == 5, "Read partial data");
    
    uint64_t position = fs_tell(fd);
    TEST_ASSERT(position == 5, "File position is correct");
    
    uint64_t new_pos = fs_lseek(fd, 0, FS_SEEK_SET);
    TEST_ASSERT(new_pos == 0, "Seek to beginning");
    
    fs_close(fd);
    
    /* Test file size */
    uint64_t file_size = fs_size(test_file);
    TEST_ASSERT(file_size == strlen(test_data) + strlen(append_data), "File size is correct");
    
    /* Cleanup */
    fs_unlink(test_file);
    
    printf("\\n");
}

void test_metadata_operations(void) {
    printf("Testing Metadata Operations:\\n");
    printf("----------------------------\\n");
    
    const char* test_file = "/metadata_test.txt";
    
    /* Create test file */
    fs_create_file(test_file, FS_PERM_644);
    fs_write_file(test_file, "test", 4);
    
    /* Test stat */
    vfs_stat_t stat;
    TEST_ASSERT(fs_stat(test_file, &stat) == 0, "Get file statistics");
    TEST_ASSERT(stat.st_size == 4, "File size in stat is correct");
    TEST_ASSERT(stat.st_mode == VFS_FILE_TYPE_REGULAR, "File type is regular");
    
    /* Test chmod */
    TEST_ASSERT(fs_chmod(test_file, FS_PERM_755) == 0, "Change file permissions");
    
    vfs_stat_t new_stat;
    fs_stat(test_file, &new_stat);
    TEST_ASSERT(new_stat.st_perm == FS_PERM_755, "Permissions changed correctly");
    
    /* Test permission checking */
    TEST_ASSERT(fs_can_read(test_file), "File is readable");
    TEST_ASSERT(fs_can_write(test_file), "File is writable");
    TEST_ASSERT(fs_can_execute(test_file), "File is executable");
    
    /* Cleanup */
    fs_unlink(test_file);
    
    printf("\\n");
}

void test_path_utilities(void) {
    printf("Testing Path Utilities:\\n");
    printf("-----------------------\\n");
    
    /* Test basename */
    char* basename = fs_basename("/path/to/file.txt");
    TEST_ASSERT(strcmp(basename, "file.txt") == 0, "Basename extraction");
    free(basename);
    
    /* Test dirname */
    char* dirname = fs_dirname("/path/to/file.txt");
    TEST_ASSERT(strcmp(dirname, "/path/to") == 0, "Dirname extraction");
    free(dirname);
    
    /* Test path validation */
    TEST_ASSERT(fs_is_valid_path("/valid/path"), "Valid path accepted");
    TEST_ASSERT(!fs_is_valid_path(""), "Empty path rejected");
    
    /* Test absolute path checking */
    TEST_ASSERT(fs_is_absolute_path("/absolute/path"), "Absolute path detected");
    TEST_ASSERT(!fs_is_absolute_path("relative/path"), "Relative path detected");
    
    /* Test path splitting */
    char dir[VFS_MAX_PATH_LENGTH], file[VFS_MAX_PATH_LENGTH];
    TEST_ASSERT(fs_split_path("/path/to/file.txt", dir, file) == 0, "Path splitting");
    TEST_ASSERT(strcmp(dir, "/path/to") == 0, "Directory part correct");
    TEST_ASSERT(strcmp(file, "file.txt") == 0, "Filename part correct");
    
    printf("\\n");
}

void test_commands(void) {
    printf("Testing Commands:\\n");
    printf("-----------------\\n");
    
    /* Test mkdir command */
    TEST_ASSERT(fs_execute_command("mkdir /cmd_test_dir") == 0, "mkdir command");
    TEST_ASSERT(fs_exists("/cmd_test_dir"), "Directory created by mkdir command");
    
    /* Test touch command */
    TEST_ASSERT(fs_execute_command("touch /cmd_test_file.txt") == 0, "touch command");
    TEST_ASSERT(fs_exists("/cmd_test_file.txt"), "File created by touch command");
    
    /* Test echo command with redirection */
    TEST_ASSERT(fs_execute_command("echo Hello World > /echo_test.txt") == 0, "echo command with redirection");
    TEST_ASSERT(fs_exists("/echo_test.txt"), "File created by echo command");
    
    /* Test cat command */
    TEST_ASSERT(fs_execute_command("cat /echo_test.txt") == 0, "cat command");
    
    /* Test ls command */
    TEST_ASSERT(fs_execute_command("ls /") == 0, "ls command");
    
    /* Test pwd command */
    TEST_ASSERT(fs_execute_command("pwd") == 0, "pwd command");
    
    /* Test rm command */
    TEST_ASSERT(fs_execute_command("rm /cmd_test_file.txt") == 0, "rm command");
    TEST_ASSERT(!fs_exists("/cmd_test_file.txt"), "File removed by rm command");
    
    /* Test rmdir command */
    TEST_ASSERT(fs_execute_command("rmdir /cmd_test_dir") == 0, "rmdir command");
    TEST_ASSERT(!fs_exists("/cmd_test_dir"), "Directory removed by rmdir command");
    
    /* Cleanup */
    fs_execute_command("rm /echo_test.txt");
    
    printf("\\n");
}

void test_error_handling(void) {
    printf("Testing Error Handling:\\n");
    printf("-----------------------\\n");
    
    /* Test operations on non-existent files */
    TEST_ASSERT(fs_unlink("/nonexistent.txt") != 0, "Delete non-existent file fails");
    TEST_ASSERT(fs_get_last_error() == VFS_ERROR_NOT_FOUND, "Error code is NOT_FOUND");
    
    /* Test invalid parameters */
    TEST_ASSERT(fs_mkdir(NULL, FS_PERM_755) != 0, "mkdir with NULL path fails");
    TEST_ASSERT(fs_get_last_error() == VFS_ERROR_INVALID_PARAM, "Error code is INVALID_PARAM");
    
    /* Test error string function */
    const char* error_str = fs_error_string(VFS_ERROR_NOT_FOUND);
    TEST_ASSERT(strcmp(error_str, "File or directory not found") == 0, "Error string is correct");
    
    /* Test directory operations on files */
    fs_create_file("/not_a_dir.txt", FS_PERM_644);
    TEST_ASSERT(fs_chdir("/not_a_dir.txt") != 0, "chdir on file fails");
    fs_unlink("/not_a_dir.txt");
    
    printf("\\n");
}

/* Example usage demonstration */
void demonstrate_filesystem_api(void) {
    printf("\\nFilesystem API Demonstration:\\n");
    printf("=============================\\n");
    
    /* Create a directory structure */
    printf("Creating directory structure...\\n");
    fs_execute_command("mkdir /demo");
    fs_execute_command("mkdir /demo/docs");
    fs_execute_command("mkdir /demo/src");
    
    /* Create some files */
    printf("Creating files...\\n");
    fs_execute_command("echo 'This is a README file' > /demo/README.txt");
    fs_execute_command("echo 'int main() { return 0; }' > /demo/src/main.c");
    fs_execute_command("echo 'Documentation content' > /demo/docs/manual.txt");
    
    /* Demonstrate listing */
    printf("\\nListing directory contents:\\n");
    fs_execute_command("ls -l /demo");
    
    printf("\\nListing source directory:\\n");
    fs_execute_command("ls /demo/src");
    
    /* Demonstrate file operations */
    printf("\\nCopying and moving files:\\n");
    fs_execute_command("cp /demo/README.txt /demo/docs/README_copy.txt");
    fs_execute_command("mv /demo/src/main.c /demo/src/program.c");
    
    /* Show file contents */
    printf("\\nFile contents:\\n");
    fs_execute_command("cat /demo/README.txt");
    
    /* Demonstrate metadata */
    printf("\\nFile information:\\n");
    fs_execute_command("stat /demo/README.txt");
    
    /* Cleanup */
    printf("\\nCleaning up...\\n");
    fs_execute_command("rm -r /demo");
    
    printf("Demonstration complete!\\n");
}
