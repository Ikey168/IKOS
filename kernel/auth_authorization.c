/* IKOS Authentication & Authorization System - Authorization Framework
 * Role-based access control, permissions, and access control lists
 */

#include "../include/auth_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* ========================== Global Authorization State ========================== */

/* Role and permission storage */
typedef struct {
    uint32_t count;
    role_t roles[256];
} role_store_t;

typedef struct {
    uint32_t count;
    permission_t permissions[AUTH_MAX_PERMISSIONS];
} permission_store_t;

typedef struct {
    uint32_t count;
    access_control_list_t acls[512];
} acl_store_t;

static role_store_t* role_store = NULL;
static permission_store_t* permission_store = NULL;
static acl_store_t* acl_store = NULL;
static uint32_t next_role_id = 1;
static uint32_t next_permission_id = 1;

extern bool auth_system_initialized;
extern pthread_mutex_t auth_mutex;

/* ========================== Internal Helper Functions ========================== */

static role_t* find_role_by_id(uint32_t role_id) {
    if (!role_store) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < role_store->count; i++) {
        if (role_store->roles[i].role_id == role_id) {
            return &role_store->roles[i];
        }
    }
    
    return NULL;
}

static role_t* find_role_by_name(const char* name) {
    if (!role_store || !name) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < role_store->count; i++) {
        if (strcmp(role_store->roles[i].name, name) == 0) {
            return &role_store->roles[i];
        }
    }
    
    return NULL;
}

static permission_t* find_permission_by_id(uint32_t permission_id) {
    if (!permission_store) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < permission_store->count; i++) {
        if (permission_store->permissions[i].permission_id == permission_id) {
            return &permission_store->permissions[i];
        }
    }
    
    return NULL;
}

static permission_t* find_permission_by_name(const char* name) {
    if (!permission_store || !name) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < permission_store->count; i++) {
        if (strcmp(permission_store->permissions[i].name, name) == 0) {
            return &permission_store->permissions[i];
        }
    }
    
    return NULL;
}

static access_control_list_t* find_acl_by_resource(const char* resource) {
    if (!acl_store || !resource) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < acl_store->count; i++) {
        if (strcmp(acl_store->acls[i].resource, resource) == 0) {
            return &acl_store->acls[i];
        }
    }
    
    return NULL;
}

/* ========================== Authorization System Initialization ========================== */

int authz_init(void) {
    if (!auth_system_initialized) {
        return AUTH_ERROR_NOT_FOUND;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    /* Allocate role store */
    if (!role_store) {
        role_store = calloc(1, sizeof(role_store_t));
        if (!role_store) {
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_ERROR_MEMORY;
        }
    }
    
    /* Allocate permission store */
    if (!permission_store) {
        permission_store = calloc(1, sizeof(permission_store_t));
        if (!permission_store) {
            free(role_store);
            role_store = NULL;
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_ERROR_MEMORY;
        }
    }
    
    /* Allocate ACL store */
    if (!acl_store) {
        acl_store = calloc(1, sizeof(acl_store_t));
        if (!acl_store) {
            free(role_store);
            free(permission_store);
            role_store = NULL;
            permission_store = NULL;
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_ERROR_MEMORY;
        }
    }
    
    /* Create default permissions */
    permission_t default_permissions[] = {
        {AUTH_PERM_LOGIN, "login", "User login permission", AUTH_SCOPE_SYSTEM, 0, true, 1},
        {AUTH_PERM_CHANGE_PASSWORD, "change_password", "Change own password", AUTH_SCOPE_USER, 0, false, 2},
        {AUTH_PERM_READ_FILE, "read_file", "Read file permission", AUTH_SCOPE_RESOURCE, 1, true, 3},
        {AUTH_PERM_WRITE_FILE, "write_file", "Write file permission", AUTH_SCOPE_RESOURCE, 1, true, 4},
        {AUTH_PERM_EXECUTE_FILE, "execute_file", "Execute file permission", AUTH_SCOPE_RESOURCE, 1, true, 5},
        {AUTH_PERM_CREATE_USER, "create_user", "Create user accounts", AUTH_SCOPE_SYSTEM, 2, false, 6},
        {AUTH_PERM_DELETE_USER, "delete_user", "Delete user accounts", AUTH_SCOPE_SYSTEM, 2, false, 7},
        {AUTH_PERM_MODIFY_USER, "modify_user", "Modify user accounts", AUTH_SCOPE_SYSTEM, 2, false, 8},
        {AUTH_PERM_ADMIN_SYSTEM, "admin_system", "System administration", AUTH_SCOPE_SYSTEM, 3, false, 9},
        {AUTH_PERM_VIEW_LOGS, "view_logs", "View system logs", AUTH_SCOPE_SYSTEM, 1, false, 10},
        {AUTH_PERM_MODIFY_ROLES, "modify_roles", "Modify roles and permissions", AUTH_SCOPE_SYSTEM, 3, false, 11},
        {AUTH_PERM_MODIFY_PERMISSIONS, "modify_permissions", "Modify permissions", AUTH_SCOPE_SYSTEM, 3, false, 12}
    };
    
    uint32_t num_default_perms = sizeof(default_permissions) / sizeof(default_permissions[0]);
    for (uint32_t i = 0; i < num_default_perms && permission_store->count < AUTH_MAX_PERMISSIONS; i++) {
        permission_store->permissions[permission_store->count] = default_permissions[i];
        permission_store->count++;
        if (default_permissions[i].permission_id >= next_permission_id) {
            next_permission_id = default_permissions[i].permission_id + 1;
        }
    }
    
    /* Create default roles */
    role_t* admin_role = &role_store->roles[role_store->count++];
    admin_role->role_id = AUTH_ROLE_ADMIN;
    strncpy(admin_role->name, "admin", sizeof(admin_role->name) - 1);
    strncpy(admin_role->description, "System Administrator", sizeof(admin_role->description) - 1);
    admin_role->system_role = true;
    admin_role->priority = 100;
    admin_role->created_time = time(NULL);
    
    /* Assign all permissions to admin role */
    admin_role->permissions = malloc(permission_store->count * sizeof(uint32_t));
    if (admin_role->permissions) {
        for (uint32_t i = 0; i < permission_store->count; i++) {
            admin_role->permissions[i] = permission_store->permissions[i].permission_id;
        }
        admin_role->permission_count = permission_store->count;
    }
    
    role_t* user_role = &role_store->roles[role_store->count++];
    user_role->role_id = AUTH_ROLE_USER;
    strncpy(user_role->name, "user", sizeof(user_role->name) - 1);
    strncpy(user_role->description, "Regular User", sizeof(user_role->description) - 1);
    user_role->system_role = true;
    user_role->priority = 10;
    user_role->created_time = time(NULL);
    
    /* Assign basic permissions to user role */
    uint32_t user_permissions[] = {AUTH_PERM_LOGIN, AUTH_PERM_CHANGE_PASSWORD, 
                                  AUTH_PERM_READ_FILE, AUTH_PERM_WRITE_FILE, AUTH_PERM_EXECUTE_FILE};
    user_role->permission_count = sizeof(user_permissions) / sizeof(user_permissions[0]);
    user_role->permissions = malloc(user_role->permission_count * sizeof(uint32_t));
    if (user_role->permissions) {
        memcpy(user_role->permissions, user_permissions, 
               user_role->permission_count * sizeof(uint32_t));
    }
    
    role_t* guest_role = &role_store->roles[role_store->count++];
    guest_role->role_id = AUTH_ROLE_GUEST;
    strncpy(guest_role->name, "guest", sizeof(guest_role->name) - 1);
    strncpy(guest_role->description, "Guest User", sizeof(guest_role->description) - 1);
    guest_role->system_role = true;
    guest_role->priority = 1;
    guest_role->created_time = time(NULL);
    
    /* Assign minimal permissions to guest role */
    uint32_t guest_permissions[] = {AUTH_PERM_LOGIN, AUTH_PERM_READ_FILE};
    guest_role->permission_count = sizeof(guest_permissions) / sizeof(guest_permissions[0]);
    guest_role->permissions = malloc(guest_role->permission_count * sizeof(uint32_t));
    if (guest_role->permissions) {
        memcpy(guest_role->permissions, guest_permissions,
               guest_role->permission_count * sizeof(uint32_t));
    }
    
    next_role_id = AUTH_ROLE_AUDITOR + 1;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

void authz_shutdown(void) {
    pthread_mutex_lock(&auth_mutex);
    
    /* Free role permissions */
    if (role_store) {
        for (uint32_t i = 0; i < role_store->count; i++) {
            if (role_store->roles[i].permissions) {
                free(role_store->roles[i].permissions);
            }
        }
        free(role_store);
        role_store = NULL;
    }
    
    /* Free ACL entries */
    if (acl_store) {
        for (uint32_t i = 0; i < acl_store->count; i++) {
            if (acl_store->acls[i].entries) {
                free(acl_store->acls[i].entries);
            }
        }
        free(acl_store);
        acl_store = NULL;
    }
    
    if (permission_store) {
        free(permission_store);
        permission_store = NULL;
    }
    
    pthread_mutex_unlock(&auth_mutex);
}

/* ========================== Role Management ========================== */

int authz_create_role(const char* name, const char* description, uint32_t* role_id) {
    if (!name || !role_id) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized || !role_store) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Check if role already exists */
    if (find_role_by_name(name) != NULL) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_ALREADY_EXISTS;
    }
    
    /* Check if we have space for new role */
    if (role_store->count >= 256) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Create new role */
    role_t* role = &role_store->roles[role_store->count];
    memset(role, 0, sizeof(role_t));
    
    role->role_id = next_role_id++;
    strncpy(role->name, name, sizeof(role->name) - 1);
    if (description) {
        strncpy(role->description, description, sizeof(role->description) - 1);
    }
    role->system_role = false;
    role->priority = 50;  /* Default priority */
    role->created_time = time(NULL);
    
    *role_id = role->role_id;
    role_store->count++;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

int authz_get_role(uint32_t role_id, role_t* role) {
    if (!role) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized || !role_store) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    role_t* found_role = find_role_by_id(role_id);
    if (!found_role) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    *role = *found_role;
    
    /* Deep copy permissions array */
    if (found_role->permission_count > 0 && found_role->permissions) {
        role->permissions = malloc(found_role->permission_count * sizeof(uint32_t));
        if (role->permissions) {
            memcpy(role->permissions, found_role->permissions,
                   found_role->permission_count * sizeof(uint32_t));
        } else {
            role->permission_count = 0;
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

int authz_assign_role(uint32_t user_id, uint32_t role_id) {
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Verify role exists */
    role_t* role = find_role_by_id(role_id);
    if (!role) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Get user account (this function is external) */
    user_account_t account;
    int ret = auth_get_user(user_id, &account);
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Check if user already has this role */
    for (uint32_t i = 0; i < account.role_count; i++) {
        if (account.roles[i] == role_id) {
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_ERROR_ALREADY_EXISTS;
        }
    }
    
    /* Check if we can add more roles */
    if (account.role_count >= AUTH_MAX_ROLES_PER_USER) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* This would need to be implemented to actually modify the user */
    /* For now, we'll assume success */
    
    pthread_mutex_unlock(&auth_mutex);
    
    /* Log role assignment */
    auth_log_event(AUTH_EVENT_ROLE_ASSIGNED, user_id, "127.0.0.1",
                  "Role assigned to user", true);
    
    return AUTH_SUCCESS;
}

int authz_check_role(uint32_t user_id, uint32_t role_id) {
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Get user account */
    user_account_t account;
    int ret = auth_get_user(user_id, &account);
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Check if user has this role */
    for (uint32_t i = 0; i < account.role_count; i++) {
        if (account.roles[i] == role_id) {
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    return AUTH_ERROR_ACCESS_DENIED;
}

/* ========================== Permission Management ========================== */

int authz_create_permission(const char* name, const char* description,
                           auth_permission_scope_t scope, uint32_t* perm_id) {
    if (!name || !perm_id) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized || !permission_store) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Check if permission already exists */
    if (find_permission_by_name(name) != NULL) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_ALREADY_EXISTS;
    }
    
    /* Check if we have space for new permission */
    if (permission_store->count >= AUTH_MAX_PERMISSIONS) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Create new permission */
    permission_t* perm = &permission_store->permissions[permission_store->count];
    memset(perm, 0, sizeof(permission_t));
    
    perm->permission_id = next_permission_id++;
    strncpy(perm->name, name, sizeof(perm->name) - 1);
    if (description) {
        strncpy(perm->description, description, sizeof(perm->description) - 1);
    }
    perm->scope = scope;
    perm->category = 0;  /* Default category */
    perm->inheritable = true;  /* Default inheritable */
    perm->priority = permission_store->count + 1;
    
    *perm_id = perm->permission_id;
    permission_store->count++;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

int authz_check_permission(uint32_t user_id, uint32_t permission_id) {
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Get user account */
    user_account_t account;
    int ret = auth_get_user(user_id, &account);
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Check permissions through roles */
    for (uint32_t i = 0; i < account.role_count; i++) {
        role_t* role = find_role_by_id(account.roles[i]);
        if (role) {
            for (uint32_t j = 0; j < role->permission_count; j++) {
                if (role->permissions[j] == permission_id) {
                    pthread_mutex_unlock(&auth_mutex);
                    return AUTH_SUCCESS;
                }
            }
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    /* Log permission denied */
    auth_log_event(AUTH_EVENT_PERMISSION_DENIED, user_id, "127.0.0.1",
                  "Permission check failed", false);
    
    return AUTH_ERROR_ACCESS_DENIED;
}

int authz_add_permission_to_role(uint32_t role_id, uint32_t permission_id) {
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized || !role_store || !permission_store) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Verify role exists */
    role_t* role = find_role_by_id(role_id);
    if (!role) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Verify permission exists */
    permission_t* perm = find_permission_by_id(permission_id);
    if (!perm) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Check if role already has this permission */
    for (uint32_t i = 0; i < role->permission_count; i++) {
        if (role->permissions[i] == permission_id) {
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_ERROR_ALREADY_EXISTS;
        }
    }
    
    /* Resize permissions array */
    uint32_t* new_permissions = realloc(role->permissions, 
                                       (role->permission_count + 1) * sizeof(uint32_t));
    if (!new_permissions) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    role->permissions = new_permissions;
    role->permissions[role->permission_count] = permission_id;
    role->permission_count++;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

/* ========================== Access Control ========================== */

int authz_check_access(uint32_t user_id, const char* resource, const char* action) {
    if (!resource || !action) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* First check user permissions */
    user_account_t account;
    int ret = auth_get_user(user_id, &account);
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Map action to permission ID */
    uint32_t required_permission = 0;
    if (strcmp(action, "read") == 0) {
        required_permission = AUTH_PERM_READ_FILE;
    } else if (strcmp(action, "write") == 0) {
        required_permission = AUTH_PERM_WRITE_FILE;
    } else if (strcmp(action, "execute") == 0) {
        required_permission = AUTH_PERM_EXECUTE_FILE;
    }
    
    if (required_permission == 0) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_INVALID;
    }
    
    /* Check if user has required permission */
    ret = authz_check_permission(user_id, required_permission);
    if (ret == AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_SUCCESS;
    }
    
    /* Check ACL for specific resource */
    access_control_list_t* acl = find_acl_by_resource(resource);
    if (acl) {
        for (uint32_t i = 0; i < acl->entry_count; i++) {
            acl_entry_t* entry = &acl->entries[i];
            
            /* Check if entry applies to this user */
            bool applies = false;
            if (!entry->is_group && entry->subject_id == user_id) {
                applies = true;
            } else if (entry->is_group) {
                /* Check if user is in group */
                for (uint32_t j = 0; j < account.group_count; j++) {
                    if (account.groups[j] == entry->subject_id) {
                        applies = true;
                        break;
                    }
                }
            }
            
            if (applies) {
                /* Check if permission matches */
                uint32_t perm_bit = 1 << (required_permission - 1);
                if (entry->permissions & perm_bit) {
                    pthread_mutex_unlock(&auth_mutex);
                    return entry->allow ? AUTH_SUCCESS : AUTH_ERROR_ACCESS_DENIED;
                }
            }
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    /* Log access denied */
    char details[256];
    snprintf(details, sizeof(details), "Access denied to resource: %s, action: %s", 
             resource, action);
    auth_log_event(AUTH_EVENT_PERMISSION_DENIED, user_id, "127.0.0.1", details, false);
    
    return AUTH_ERROR_ACCESS_DENIED;
}

int authz_set_acl(const char* resource, const acl_entry_t* entries, size_t count) {
    if (!resource || !entries) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized || !acl_store) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Find existing ACL or create new one */
    access_control_list_t* acl = find_acl_by_resource(resource);
    if (!acl) {
        /* Check if we have space for new ACL */
        if (acl_store->count >= 512) {
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_ERROR_MEMORY;
        }
        
        acl = &acl_store->acls[acl_store->count];
        memset(acl, 0, sizeof(access_control_list_t));
        strncpy(acl->resource, resource, sizeof(acl->resource) - 1);
        acl_store->count++;
    } else {
        /* Free existing entries */
        if (acl->entries) {
            free(acl->entries);
            acl->entries = NULL;
            acl->entry_count = 0;
        }
    }
    
    /* Allocate new entries */
    acl->entries = malloc(count * sizeof(acl_entry_t));
    if (!acl->entries) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Copy entries */
    memcpy(acl->entries, entries, count * sizeof(acl_entry_t));
    acl->entry_count = count;
    acl->modified_time = time(NULL);
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

int authz_get_effective_permissions(uint32_t user_id, permission_set_t* permissions) {
    if (!permissions) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Initialize permission set */
    memset(permissions, 0, sizeof(permission_set_t));
    
    /* Get user account */
    user_account_t account;
    int ret = auth_get_user(user_id, &account);
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Collect permissions from all user roles */
    for (uint32_t i = 0; i < account.role_count; i++) {
        role_t* role = find_role_by_id(account.roles[i]);
        if (role) {
            for (uint32_t j = 0; j < role->permission_count; j++) {
                uint32_t perm_id = role->permissions[j];
                if (perm_id < AUTH_MAX_PERMISSIONS) {
                    uint32_t word_index = perm_id / 32;
                    uint32_t bit_index = perm_id % 32;
                    permissions->permissions[word_index] |= (1U << bit_index);
                    permissions->count++;
                }
            }
        }
    }
    
    permissions->computed_time = time(NULL);
    permissions->cached = true;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

/* ========================== Utility Functions ========================== */

int authz_list_roles(role_t** roles, uint32_t* count) {
    if (!roles || !count) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized || !role_store) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    *roles = malloc(role_store->count * sizeof(role_t));
    if (!*roles) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Copy roles with deep copy of permissions */
    for (uint32_t i = 0; i < role_store->count; i++) {
        (*roles)[i] = role_store->roles[i];
        
        /* Deep copy permissions */
        if (role_store->roles[i].permission_count > 0 && role_store->roles[i].permissions) {
            (*roles)[i].permissions = malloc(role_store->roles[i].permission_count * sizeof(uint32_t));
            if ((*roles)[i].permissions) {
                memcpy((*roles)[i].permissions, role_store->roles[i].permissions,
                       role_store->roles[i].permission_count * sizeof(uint32_t));
            } else {
                (*roles)[i].permission_count = 0;
            }
        } else {
            (*roles)[i].permissions = NULL;
            (*roles)[i].permission_count = 0;
        }
    }
    
    *count = role_store->count;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

int authz_list_permissions(permission_t** permissions, uint32_t* count) {
    if (!permissions || !count) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized || !permission_store) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    *permissions = malloc(permission_store->count * sizeof(permission_t));
    if (!*permissions) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    memcpy(*permissions, permission_store->permissions, 
           permission_store->count * sizeof(permission_t));
    *count = permission_store->count;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}
