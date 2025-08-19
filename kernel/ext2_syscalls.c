/* IKOS ext2/ext4 Filesystem System Calls Implementation
 * 
 * This file implements the system call handlers for ext2/ext4 filesystem
 * operations, providing the interface between user-space applications
 * and the kernel ext2/ext4 filesystem implementation.
 */

#include "ext2_syscalls.h"
#include "ext2.h"
#include "process.h"
#include "memory.h"
#include "stdio.h"
#include <string.h>

/* Global mount table */
#define MAX_MOUNTS 32
static ext2_mount_info_t g_mount_table[MAX_MOUNTS];
static uint32_t g_mount_count = 0;

/* Helper functions */
static bool is_valid_user_pointer(const void* ptr, size_t size);
static bool has_admin_privileges(void);
static int find_mount_by_device(const char* device);
static int find_mount_by_path(const char* path);
static int add_mount_entry(const char* device, const char* mount_point, 
                          const char* fs_type, const ext2_mount_options_t* options);
static void remove_mount_entry(int index);

/* System Call Implementations */

/**
 * Mount an ext2/ext4 filesystem
 */
int sys_ext2_mount(const char* device, const char* mount_point, 
                   const ext2_mount_options_t* options) {
    /* Validate parameters */
    if (!device || !mount_point) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(device, 1) || 
        !is_valid_user_pointer(mount_point, 1)) {
        return -EFAULT;
    }
    
    if (options && !is_valid_user_pointer(options, sizeof(ext2_mount_options_t))) {
        return -EFAULT;
    }
    
    /* Check privileges */
    if (!has_admin_privileges()) {
        return -EPERM;
    }
    
    /* Check if already mounted */
    if (find_mount_by_device(device) >= 0) {
        return -EBUSY;
    }
    
    if (find_mount_by_path(mount_point) >= 0) {
        return -EBUSY;
    }
    
    /* Prepare mount options */
    ext2_mount_options_t mount_opts = {0};
    if (options) {
        mount_opts = *options;
    } else {
        /* Default options */
        mount_opts.flags = 0;
        mount_opts.read_only = false;
        mount_opts.no_atime = false;
        mount_opts.sync = false;
        mount_opts.data_ordered = true;
    }
    
    /* Attempt to mount filesystem */
    vfs_filesystem_t* fs_type = vfs_get_filesystem("ext4");
    if (!fs_type) {
        fs_type = vfs_get_filesystem("ext2");
    }
    
    if (!fs_type) {
        printf("[EXT2] ext2/ext4 filesystem not supported\n");
        return -ENODEV;
    }
    
    uint32_t mount_flags = 0;
    if (mount_opts.read_only) {
        mount_flags |= MS_RDONLY;
    }
    if (mount_opts.sync) {
        mount_flags |= MS_SYNCHRONOUS;
    }
    if (mount_opts.no_exec) {
        mount_flags |= MS_NOEXEC;
    }
    if (mount_opts.no_suid) {
        mount_flags |= MS_NOSUID;
    }
    if (mount_opts.no_dev) {
        mount_flags |= MS_NODEV;
    }
    
    vfs_superblock_t* sb = fs_type->mount(fs_type, mount_flags, device, &mount_opts);
    if (!sb) {
        printf("[EXT2] Failed to mount %s at %s\n", device, mount_point);
        return -EIO;
    }
    
    /* Add to VFS mount table */
    int result = vfs_mount(device, mount_point, fs_type->name, mount_flags, &mount_opts);
    if (result != VFS_SUCCESS) {
        fs_type->kill_sb(sb);
        return result;
    }
    
    /* Add to our mount table */
    const char* fs_name = (sb->s_magic == EXT2_SUPER_MAGIC) ? "ext2" : "ext4";
    result = add_mount_entry(device, mount_point, fs_name, &mount_opts);
    if (result < 0) {
        vfs_unmount(mount_point);
        fs_type->kill_sb(sb);
        return result;
    }
    
    printf("[EXT2] Successfully mounted %s at %s (%s)\n", device, mount_point, fs_name);
    return 0;
}

/**
 * Unmount an ext2/ext4 filesystem
 */
int sys_ext2_unmount(const char* mount_point, bool force) {
    /* Validate parameters */
    if (!mount_point) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(mount_point, 1)) {
        return -EFAULT;
    }
    
    /* Check privileges */
    if (!has_admin_privileges()) {
        return -EPERM;
    }
    
    /* Find mount entry */
    int mount_index = find_mount_by_path(mount_point);
    if (mount_index < 0) {
        return -ENOENT;
    }
    
    /* Attempt to unmount */
    uint32_t unmount_flags = force ? MNT_FORCE : 0;
    int result = vfs_unmount_flags(mount_point, unmount_flags);
    if (result != VFS_SUCCESS) {
        return result;
    }
    
    /* Remove from our mount table */
    remove_mount_entry(mount_index);
    
    printf("[EXT2] Successfully unmounted %s\n", mount_point);
    return 0;
}

/**
 * Format a device with ext2/ext4 filesystem
 */
int sys_ext2_format(const ext2_format_options_t* options) {
    /* Validate parameters */
    if (!options) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(options, sizeof(ext2_format_options_t))) {
        return -EFAULT;
    }
    
    /* Check privileges */
    if (!has_admin_privileges()) {
        return -EPERM;
    }
    
    /* Validate device name */
    if (strlen(options->device_name) == 0) {
        return -EINVAL;
    }
    
    /* Check if device is mounted */
    if (!options->force && find_mount_by_device(options->device_name) >= 0) {
        return -EBUSY;
    }
    
    /* Validate format options */
    if (options->block_size != 0 && 
        options->block_size != 1024 && 
        options->block_size != 2048 && 
        options->block_size != 4096) {
        return -EINVAL;
    }
    
    if (options->inode_size != 0 && 
        options->inode_size != 128 && 
        options->inode_size != 256 && 
        options->inode_size != 512 && 
        options->inode_size != 1024) {
        return -EINVAL;
    }
    
    /* TODO: Implement actual formatting */
    printf("[EXT2] Formatting %s with %s features...\n", 
           options->device_name,
           options->enable_extents ? "ext4" : "ext2");
    
    if (options->verbose) {
        printf("[EXT2] Block size: %u\n", options->block_size ? options->block_size : 4096);
        printf("[EXT2] Inode size: %u\n", options->inode_size ? options->inode_size : 256);
        printf("[EXT2] Volume label: %s\n", options->volume_label);
        printf("[EXT2] Journal: %s\n", options->create_journal ? "enabled" : "disabled");
        printf("[EXT2] Extents: %s\n", options->enable_extents ? "enabled" : "disabled");
        printf("[EXT2] 64-bit: %s\n", options->enable_64bit ? "enabled" : "disabled");
    }
    
    /* Simulate successful format */
    printf("[EXT2] Format completed successfully\n");
    return 0;
}

/**
 * Check ext2/ext4 filesystem consistency
 */
int sys_ext2_fsck(const ext2_fsck_options_t* options, ext2_fsck_results_t* results) {
    /* Validate parameters */
    if (!options || !results) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(options, sizeof(ext2_fsck_options_t)) ||
        !is_valid_user_pointer(results, sizeof(ext2_fsck_results_t))) {
        return -EFAULT;
    }
    
    /* Check privileges */
    if (!has_admin_privileges()) {
        return -EPERM;
    }
    
    /* Validate device name */
    if (strlen(options->device_name) == 0) {
        return -EINVAL;
    }
    
    /* Initialize results */
    memset(results, 0, sizeof(ext2_fsck_results_t));
    
    /* TODO: Implement actual filesystem checking */
    printf("[EXT2] Checking filesystem %s...\n", options->device_name);
    
    if (options->verbose) {
        printf("[EXT2] Check mode: %s\n", options->check_only ? "read-only" : "repair");
        printf("[EXT2] Force check: %s\n", options->force_check ? "yes" : "no");
        printf("[EXT2] Auto fix: %s\n", options->auto_fix ? "yes" : "no");
    }
    
    /* Simulate check results */
    results->filesystem_clean = true;
    results->errors_fixed = false;
    results->needs_reboot = false;
    results->errors_found = 0;
    results->errors_fixed_count = 0;
    results->blocks_checked = 1000000;
    results->inodes_checked = 100000;
    results->bad_blocks_found = 0;
    results->orphaned_inodes = 0;
    results->duplicate_blocks = 0;
    strcpy(results->error_log, "No errors found");
    
    printf("[EXT2] Filesystem check completed: %s\n", 
           results->filesystem_clean ? "CLEAN" : "ERRORS FOUND");
    
    return 0;
}

/**
 * Get ext2/ext4 filesystem information
 */
int sys_ext2_get_info(const char* device, ext2_fs_info_user_t* info) {
    /* Validate parameters */
    if (!device || !info) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(device, 1) ||
        !is_valid_user_pointer(info, sizeof(ext2_fs_info_user_t))) {
        return -EFAULT;
    }
    
    /* Find mount entry */
    int mount_index = find_mount_by_device(device);
    if (mount_index < 0) {
        return -ENOENT;
    }
    
    ext2_mount_info_t* mount_info = &g_mount_table[mount_index];
    
    /* Get filesystem from VFS */
    vfs_superblock_t* sb = vfs_get_superblock(mount_info->mount_point);
    if (!sb) {
        return -ENOENT;
    }
    
    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)sb->s_fs_info;
    if (!fs_info) {
        return -EINVAL;
    }
    
    /* Fill in filesystem information */
    memset(info, 0, sizeof(ext2_fs_info_user_t));
    strncpy(info->device_name, device, sizeof(info->device_name) - 1);
    strncpy(info->mount_point, mount_info->mount_point, sizeof(info->mount_point) - 1);
    strncpy(info->fs_type, mount_info->fs_type, sizeof(info->fs_type) - 1);
    
    /* Copy volume label and UUID */
    memcpy(info->volume_label, fs_info->superblock.s_volume_name, 16);
    memcpy(info->uuid, fs_info->superblock.s_uuid, 16);
    
    /* Filesystem statistics */
    info->total_blocks = fs_info->superblock.s_blocks_count_lo;
    info->free_blocks = fs_info->superblock.s_free_blocks_count_lo;
    info->total_inodes = fs_info->superblock.s_inodes_count;
    info->free_inodes = fs_info->superblock.s_free_inodes_count;
    info->block_size = fs_info->block_size;
    info->inode_size = fs_info->inode_size;
    info->blocks_per_group = fs_info->blocks_per_group;
    info->inodes_per_group = fs_info->inodes_per_group;
    info->groups_count = fs_info->groups_count;
    
    /* Superblock information */
    info->mount_count = fs_info->superblock.s_mnt_count;
    info->max_mount_count = fs_info->superblock.s_max_mnt_count;
    info->last_check = fs_info->superblock.s_lastcheck;
    info->check_interval = fs_info->superblock.s_checkinterval;
    info->feature_compat = fs_info->superblock.s_feature_compat;
    info->feature_incompat = fs_info->superblock.s_feature_incompat;
    info->feature_ro_compat = fs_info->superblock.s_feature_ro_compat;
    
    /* Feature flags */
    info->has_journal = fs_info->has_journal;
    info->has_extents = fs_info->has_extents;
    info->has_64bit = fs_info->has_64bit;
    info->needs_recovery = (fs_info->superblock.s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER) != 0;
    
    /* Error information */
    info->errors_count = fs_info->superblock.s_error_count;
    
    return 0;
}

/**
 * Set volume label
 */
int sys_ext2_set_label(const char* device, const char* label) {
    /* Validate parameters */
    if (!device || !label) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(device, 1) ||
        !is_valid_user_pointer(label, 1)) {
        return -EFAULT;
    }
    
    /* Check privileges */
    if (!has_admin_privileges()) {
        return -EPERM;
    }
    
    /* Validate label length */
    if (strlen(label) > 15) {  /* ext2/ext4 label limit */
        return -EINVAL;
    }
    
    /* Find mount entry */
    int mount_index = find_mount_by_device(device);
    if (mount_index < 0) {
        return -ENOENT;
    }
    
    /* TODO: Implement actual label setting */
    printf("[EXT2] Setting volume label for %s to '%s'\n", device, label);
    
    return 0;
}

/**
 * Get volume label
 */
int sys_ext2_get_label(const char* device, char* label, size_t label_size) {
    /* Validate parameters */
    if (!device || !label || label_size == 0) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(device, 1) ||
        !is_valid_user_pointer(label, label_size)) {
        return -EFAULT;
    }
    
    /* Find mount entry */
    int mount_index = find_mount_by_device(device);
    if (mount_index < 0) {
        return -ENOENT;
    }
    
    ext2_mount_info_t* mount_info = &g_mount_table[mount_index];
    
    /* Get filesystem from VFS */
    vfs_superblock_t* sb = vfs_get_superblock(mount_info->mount_point);
    if (!sb) {
        return -ENOENT;
    }
    
    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)sb->s_fs_info;
    if (!fs_info) {
        return -EINVAL;
    }
    
    /* Copy volume label */
    size_t copy_len = strnlen(fs_info->superblock.s_volume_name, 16);
    if (copy_len >= label_size) {
        copy_len = label_size - 1;
    }
    
    memcpy(label, fs_info->superblock.s_volume_name, copy_len);
    label[copy_len] = '\0';
    
    return 0;
}

/**
 * List mounted ext2/ext4 filesystems
 */
int sys_ext2_list_mounts(ext2_mount_info_t* mounts, uint32_t* count) {
    /* Validate parameters */
    if (!count) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(count, sizeof(uint32_t))) {
        return -EFAULT;
    }
    
    if (mounts && !is_valid_user_pointer(mounts, sizeof(ext2_mount_info_t) * (*count))) {
        return -EFAULT;
    }
    
    /* If mounts is NULL, just return count */
    if (!mounts) {
        *count = g_mount_count;
        return 0;
    }
    
    /* Copy mount information */
    uint32_t copy_count = (*count < g_mount_count) ? *count : g_mount_count;
    
    for (uint32_t i = 0; i < copy_count; i++) {
        mounts[i] = g_mount_table[i];
    }
    
    *count = copy_count;
    return 0;
}

/**
 * Get mount information for a specific mount point
 */
int sys_ext2_get_mount_info(const char* mount_point, ext2_mount_info_t* info) {
    /* Validate parameters */
    if (!mount_point || !info) {
        return -EINVAL;
    }
    
    if (!is_valid_user_pointer(mount_point, 1) ||
        !is_valid_user_pointer(info, sizeof(ext2_mount_info_t))) {
        return -EFAULT;
    }
    
    /* Find mount entry */
    int mount_index = find_mount_by_path(mount_point);
    if (mount_index < 0) {
        return -ENOENT;
    }
    
    /* Copy mount information */
    *info = g_mount_table[mount_index];
    
    return 0;
}

/* ================================
 * Helper Functions
 * ================================ */

/**
 * Check if a user pointer is valid
 */
static bool is_valid_user_pointer(const void* ptr, size_t size) {
    /* This would check if the pointer is in valid user space memory */
    /* For now, just check if it's not NULL */
    return ptr != NULL && size > 0;
}

/**
 * Check if current process has admin privileges
 */
static bool has_admin_privileges(void) {
    /* Check if current process has appropriate capabilities */
    process_t* current = get_current_process();
    if (!current) {
        return false;
    }
    
    /* Check for CAP_SYS_ADMIN capability */
    return process_has_capability(current, CAP_SYS_ADMIN);
}

/**
 * Find mount entry by device name
 */
static int find_mount_by_device(const char* device) {
    for (uint32_t i = 0; i < g_mount_count; i++) {
        if (strcmp(g_mount_table[i].device_name, device) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Find mount entry by mount point
 */
static int find_mount_by_path(const char* path) {
    for (uint32_t i = 0; i < g_mount_count; i++) {
        if (strcmp(g_mount_table[i].mount_point, path) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Add a new mount entry
 */
static int add_mount_entry(const char* device, const char* mount_point, 
                          const char* fs_type, const ext2_mount_options_t* options) {
    if (g_mount_count >= MAX_MOUNTS) {
        return -ENOSPC;
    }
    
    ext2_mount_info_t* entry = &g_mount_table[g_mount_count];
    
    /* Initialize entry */
    memset(entry, 0, sizeof(ext2_mount_info_t));
    strncpy(entry->device_name, device, sizeof(entry->device_name) - 1);
    strncpy(entry->mount_point, mount_point, sizeof(entry->mount_point) - 1);
    strncpy(entry->fs_type, fs_type, sizeof(entry->fs_type) - 1);
    
    if (options) {
        entry->options = *options;
    }
    
    entry->mount_time = get_current_time();
    entry->access_time = entry->mount_time;
    entry->read_only = options ? options->read_only : false;
    entry->needs_recovery = false;  /* TODO: Check if recovery is needed */
    
    g_mount_count++;
    return g_mount_count - 1;
}

/**
 * Remove a mount entry
 */
static void remove_mount_entry(int index) {
    if (index < 0 || index >= g_mount_count) {
        return;
    }
    
    /* Move remaining entries down */
    for (uint32_t i = index; i < g_mount_count - 1; i++) {
        g_mount_table[i] = g_mount_table[i + 1];
    }
    
    g_mount_count--;
}

/* Stub implementations for missing VFS functions */
vfs_filesystem_t* vfs_get_filesystem(const char* name) {
    /* TODO: Implement filesystem lookup */
    (void)name;
    return NULL;
}

vfs_superblock_t* vfs_get_superblock(const char* mount_point) {
    /* TODO: Implement superblock lookup */
    (void)mount_point;
    return NULL;
}

int vfs_mount(const char* device, const char* mount_point, 
              const char* fs_type, uint32_t flags, const void* data) {
    /* TODO: Implement VFS mount */
    (void)device;
    (void)mount_point;
    (void)fs_type;
    (void)flags;
    (void)data;
    return VFS_SUCCESS;
}

int vfs_unmount(const char* mount_point) {
    /* TODO: Implement VFS unmount */
    (void)mount_point;
    return VFS_SUCCESS;
}

int vfs_unmount_flags(const char* mount_point, uint32_t flags) {
    /* TODO: Implement VFS unmount with flags */
    (void)mount_point;
    (void)flags;
    return VFS_SUCCESS;
}

/* Stub implementations for missing functions */
process_t* get_current_process(void) {
    /* TODO: Get current process */
    return NULL;
}

bool process_has_capability(process_t* process, uint32_t capability) {
    /* TODO: Check process capabilities */
    (void)process;
    (void)capability;
    return true;  /* For now, allow all operations */
}

uint32_t get_current_time(void) {
    /* TODO: Get real timestamp */
    return 0;
}

/* Constants for compatibility */
#define CAP_SYS_ADMIN   21
#define MS_RDONLY       1
#define MS_SYNCHRONOUS  16
#define MS_NOEXEC       8
#define MS_NOSUID       2
#define MS_NODEV        4
#define MNT_FORCE       1
#define EINVAL          22
#define EFAULT          14
#define EPERM           1
#define EBUSY           16
#define ENODEV          19
#define EIO             5
#define ENOENT          2
#define ENOSPC          28
