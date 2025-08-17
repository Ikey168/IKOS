# IKOS Authentication & Authorization System Integration

## Overview

The IKOS Authentication & Authorization System provides comprehensive security infrastructure for the IKOS kernel, including user management, session handling, role-based access control, multi-factor authentication, and audit logging.

## Integration Points

### 1. Kernel Security Hooks

The authentication system integrates with kernel security through several hook points:

```c
/* Kernel security hook integration */
int kernel_security_check_permission(uint32_t user_id, const char* resource, const char* action) {
    /* Check if user is authenticated */
    if (!auth_is_user_authenticated(user_id)) {
        return -EACCES;
    }
    
    /* Check authorization */
    int ret = authz_check_access(user_id, resource, action);
    if (ret != AUTH_SUCCESS) {
        return -EPERM;
    }
    
    return 0;
}

/* System call authentication wrapper */
int authenticated_syscall(uint32_t syscall_num, uint32_t user_id, void* args) {
    /* Validate session */
    if (!auth_is_session_valid_for_user(user_id)) {
        return -EACCES;
    }
    
    /* Check required permissions for syscall */
    uint32_t required_perm = syscall_get_required_permission(syscall_num);
    if (authz_check_permission(user_id, required_perm) != AUTH_SUCCESS) {
        return -EPERM;
    }
    
    /* Execute syscall */
    return kernel_execute_syscall(syscall_num, args);
}
```

### 2. File System Integration

The authentication system provides access control for the Virtual File System (VFS):

```c
/* VFS permission check integration */
int vfs_check_permission(struct inode* inode, uint32_t user_id, int mask) {
    char resource_path[PATH_MAX];
    vfs_get_path(inode, resource_path, sizeof(resource_path));
    
    const char* action;
    if (mask & MAY_READ) action = "read";
    else if (mask & MAY_WRITE) action = "write";
    else if (mask & MAY_EXEC) action = "execute";
    else return -EINVAL;
    
    return (authz_check_access(user_id, resource_path, action) == AUTH_SUCCESS) ? 0 : -EACCES;
}

/* File open with authentication */
int authenticated_file_open(const char* path, int flags, uint32_t user_id) {
    /* Check read permission */
    if (flags & O_RDONLY || flags & O_RDWR) {
        if (authz_check_access(user_id, path, "read") != AUTH_SUCCESS) {
            return -EACCES;
        }
    }
    
    /* Check write permission */
    if (flags & O_WRONLY || flags & O_RDWR || flags & O_CREAT) {
        if (authz_check_access(user_id, path, "write") != AUTH_SUCCESS) {
            return -EACCES;
        }
    }
    
    return vfs_open(path, flags);
}
```

### 3. Process Management Integration

The authentication system integrates with process management for user context:

```c
/* Process creation with user context */
int process_create_with_auth(const char* program, uint32_t parent_user_id, uint32_t* new_pid) {
    /* Check execute permission */
    if (authz_check_access(parent_user_id, program, "execute") != AUTH_SUCCESS) {
        return -EACCES;
    }
    
    /* Create process */
    process_t* proc = process_create(program);
    if (!proc) {
        return -ENOMEM;
    }
    
    /* Set user context */
    proc->user_id = parent_user_id;
    proc->session_id = auth_get_current_session(parent_user_id);
    
    /* Inherit parent permissions */
    permission_set_t parent_perms;
    authz_get_effective_permissions(parent_user_id, &parent_perms);
    process_set_permissions(proc, &parent_perms);
    
    *new_pid = proc->pid;
    return 0;
}

/* Process privilege escalation */
int process_elevate_privileges(uint32_t pid, uint32_t target_user_id) {
    process_t* proc = process_get(pid);
    if (!proc) {
        return -ESRCH;
    }
    
    /* Check if current user can elevate to target user */
    if (authz_check_permission(proc->user_id, AUTH_PERM_ADMIN_SYSTEM) != AUTH_SUCCESS) {
        return -EPERM;
    }
    
    /* Update process user context */
    proc->user_id = target_user_id;
    
    /* Update permissions */
    permission_set_t new_perms;
    authz_get_effective_permissions(target_user_id, &new_perms);
    process_set_permissions(proc, &new_perms);
    
    return 0;
}
```

### 4. Network Security Integration

Authentication for network operations:

```c
/* Network socket creation with authentication */
int authenticated_socket_create(int domain, int type, int protocol, uint32_t user_id) {
    /* Check network permission */
    if (authz_check_permission(user_id, AUTH_PERM_NETWORK_ACCESS) != AUTH_SUCCESS) {
        return -EACCES;
    }
    
    /* Check for privileged ports */
    if (type == SOCK_STREAM && protocol == IPPROTO_TCP) {
        if (authz_check_permission(user_id, AUTH_PERM_BIND_PRIVILEGED) != AUTH_SUCCESS) {
            /* Restrict to non-privileged ports */
            return socket_create_restricted(domain, type, protocol);
        }
    }
    
    return socket_create(domain, type, protocol);
}

/* Network packet filtering */
int network_filter_packet(struct packet* pkt, uint32_t user_id) {
    /* Check if user can send to destination */
    char dest_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pkt->dest_addr, dest_addr, sizeof(dest_addr));
    
    if (authz_check_access(user_id, dest_addr, "network_send") != AUTH_SUCCESS) {
        return -EACCES;
    }
    
    return 0;
}
```

### 5. Memory Management Integration

Authentication for memory operations:

```c
/* Memory allocation with user limits */
void* authenticated_malloc(size_t size, uint32_t user_id) {
    /* Check memory quota */
    if (!auth_check_resource_quota(user_id, RESOURCE_MEMORY, size)) {
        return NULL;
    }
    
    /* Allocate memory */
    void* ptr = kmalloc(size);
    if (ptr) {
        /* Update user memory usage */
        auth_update_resource_usage(user_id, RESOURCE_MEMORY, size);
    }
    
    return ptr;
}

/* Shared memory access control */
int shm_attach_with_auth(int shmid, void* shmaddr, int shmflg, uint32_t user_id) {
    /* Get shared memory info */
    struct shmid_ds shmds;
    if (shmctl(shmid, IPC_STAT, &shmds) < 0) {
        return -EINVAL;
    }
    
    /* Check ownership or access permissions */
    if (shmds.shm_perm.uid != user_id) {
        if (authz_check_permission(user_id, AUTH_PERM_IPC_ACCESS) != AUTH_SUCCESS) {
            return -EACCES;
        }
    }
    
    return shmat(shmid, shmaddr, shmflg);
}
```

## Startup Integration

### Kernel Initialization Sequence

```c
int kernel_main(void) {
    /* Early kernel initialization */
    mm_init();
    scheduler_init();
    vfs_init();
    
    /* Initialize authentication system */
    if (auth_init() != AUTH_SUCCESS) {
        panic("Failed to initialize authentication system");
    }
    
    if (authz_init() != AUTH_SUCCESS) {
        panic("Failed to initialize authorization system");
    }
    
    /* Create default admin user */
    uint32_t admin_id;
    if (auth_create_user("admin", "DefaultAdminPass123!", "admin@ikos.local", &admin_id) != AUTH_SUCCESS) {
        panic("Failed to create default admin user");
    }
    
    /* Assign admin role */
    if (authz_assign_role(admin_id, AUTH_ROLE_ADMIN) != AUTH_SUCCESS) {
        panic("Failed to assign admin role");
    }
    
    /* Set up security hooks */
    security_register_hooks();
    
    /* Continue with rest of kernel initialization */
    device_init();
    interrupt_init();
    
    /* Start user space */
    init_process_start();
    
    return 0;
}
```

### Security Hook Registration

```c
void security_register_hooks(void) {
    /* File system hooks */
    vfs_register_permission_hook(vfs_check_permission);
    vfs_register_open_hook(authenticated_file_open);
    
    /* Process hooks */
    process_register_create_hook(process_create_with_auth);
    process_register_elevate_hook(process_elevate_privileges);
    
    /* Network hooks */
    network_register_socket_hook(authenticated_socket_create);
    network_register_filter_hook(network_filter_packet);
    
    /* Memory hooks */
    mm_register_alloc_hook(authenticated_malloc);
    ipc_register_shm_hook(shm_attach_with_auth);
    
    /* System call hooks */
    syscall_register_auth_wrapper(authenticated_syscall);
}
```

## Configuration

### Boot-time Configuration

```c
/* Authentication configuration structure */
struct auth_config {
    bool enforce_mfa_for_admin;
    bool require_strong_passwords;
    uint32_t session_timeout_minutes;
    uint32_t max_failed_attempts;
    uint32_t lockout_duration_minutes;
    bool enable_audit_logging;
    bool enable_session_encryption;
};

/* Default configuration */
static struct auth_config default_config = {
    .enforce_mfa_for_admin = true,
    .require_strong_passwords = true,
    .session_timeout_minutes = 60,
    .max_failed_attempts = 5,
    .lockout_duration_minutes = 15,
    .enable_audit_logging = true,
    .enable_session_encryption = true
};

/* Apply configuration */
int auth_apply_config(struct auth_config* config) {
    auth_set_session_timeout(config->session_timeout_minutes * 60);
    auth_set_lockout_policy(config->max_failed_attempts, config->lockout_duration_minutes * 60);
    auth_set_password_policy(config->require_strong_passwords);
    auth_set_mfa_policy(config->enforce_mfa_for_admin);
    auth_set_audit_enabled(config->enable_audit_logging);
    auth_set_session_encryption(config->enable_session_encryption);
    
    return AUTH_SUCCESS;
}
```

## API Usage Examples

### User Login Flow

```c
/* Complete user login with MFA */
int user_login(const char* username, const char* password, const char* mfa_code, 
               char* session_token, uint32_t* user_id) {
    /* Verify credentials */
    int ret = auth_verify_credentials(username, password, user_id);
    if (ret != AUTH_SUCCESS) {
        return ret;
    }
    
    /* Check if MFA is required */
    bool mfa_required;
    ret = auth_mfa_check_required(*user_id, &mfa_required);
    if (ret != AUTH_SUCCESS) {
        return ret;
    }
    
    /* Verify MFA if required */
    if (mfa_required) {
        if (!mfa_code) {
            return AUTH_ERROR_MFA_REQUIRED;
        }
        
        ret = auth_mfa_verify_code(*user_id, mfa_code);
        if (ret != AUTH_SUCCESS) {
            return ret;
        }
    }
    
    /* Create session */
    uint32_t session_id;
    ret = auth_create_session(*user_id, "127.0.0.1", session_token, &session_id);
    if (ret != AUTH_SUCCESS) {
        return ret;
    }
    
    /* Log successful login */
    auth_log_event(AUTH_EVENT_LOGIN_SUCCESS, *user_id, "127.0.0.1", "User login successful", true);
    
    return AUTH_SUCCESS;
}
```

### Resource Access Check

```c
/* Check if user can access a resource */
int check_resource_access(uint32_t user_id, const char* resource, const char* action) {
    /* Validate session */
    if (!auth_is_user_authenticated(user_id)) {
        return AUTH_ERROR_NOT_AUTHENTICATED;
    }
    
    /* Check access */
    int ret = authz_check_access(user_id, resource, action);
    if (ret != AUTH_SUCCESS) {
        /* Log access denied */
        char details[256];
        snprintf(details, sizeof(details), "Access denied to %s for %s", resource, action);
        auth_log_event(AUTH_EVENT_ACCESS_DENIED, user_id, "127.0.0.1", details, false);
    }
    
    return ret;
}
```

## Performance Considerations

1. **Session Caching**: Sessions are cached in memory for fast validation
2. **Permission Caching**: Effective permissions are cached per user
3. **Lazy Loading**: User data is loaded on-demand
4. **Batch Operations**: Multiple permission checks can be batched
5. **Audit Buffer**: Audit events are buffered for efficient logging

## Security Features

1. **Password Security**: bcrypt/PBKDF2 hashing with salt
2. **Session Security**: Cryptographically secure tokens with expiration
3. **MFA Support**: TOTP with backup codes
4. **Account Lockout**: Protection against brute force attacks
5. **Audit Logging**: Comprehensive security event logging
6. **Role-based Access**: Hierarchical permission system
7. **ACL Support**: Fine-grained resource access control

## Testing and Validation

The authentication system includes comprehensive tests covering:

- User management and authentication
- Session handling and validation
- Role and permission management
- Access control and ACLs
- MFA setup and verification
- Security features and lockout
- Performance benchmarks
- Integration scenarios

Run tests with:
```bash
cd kernel
make -f auth_Makefile test
```

## Future Enhancements

1. **LDAP Integration**: External authentication sources
2. **Certificate Authentication**: PKI-based authentication
3. **Single Sign-On**: SSO integration capabilities
4. **Dynamic Permissions**: Runtime permission adjustment
5. **Audit Analytics**: Security event analysis
6. **Distributed Authentication**: Multi-node authentication
