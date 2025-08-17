/* IKOS Authentication & Authorization System - Complete Interface
 * Comprehensive security system for user management and access control
 */

#ifndef AUTH_SYSTEM_H
#define AUTH_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <sys/types.h>

/* ========================== Constants and Limits ========================== */

#define AUTH_MAX_USERNAME_LENGTH    64
#define AUTH_MAX_PASSWORD_LENGTH    256
#define AUTH_MAX_HASH_LENGTH        128
#define AUTH_MAX_SALT_LENGTH        32
#define AUTH_MAX_SESSION_ID_LENGTH  64
#define AUTH_MAX_ROLE_NAME_LENGTH   64
#define AUTH_MAX_PERM_NAME_LENGTH   64
#define AUTH_MAX_RESOURCE_LENGTH    256
#define AUTH_MAX_ACTION_LENGTH      64
#define AUTH_MAX_GROUPS_PER_USER    32
#define AUTH_MAX_ROLES_PER_USER     16
#define AUTH_MAX_PERMISSIONS        1024
#define AUTH_MAX_SESSIONS           256
#define AUTH_MAX_USERS              4096
#define AUTH_MAX_BACKUP_CODES       10
#define AUTH_MFA_SECRET_LENGTH      32
#define AUTH_MFA_TOKEN_LENGTH       8

/* Session timeouts (seconds) */
#define AUTH_SESSION_TIMEOUT        3600    /* 1 hour */
#define AUTH_SESSION_IDLE_TIMEOUT   1800    /* 30 minutes */
#define AUTH_SESSION_MAX_LIFETIME   86400   /* 24 hours */

/* Security limits */
#define AUTH_MAX_LOGIN_ATTEMPTS     5
#define AUTH_LOCKOUT_DURATION       900     /* 15 minutes */
#define AUTH_PASSWORD_MIN_LENGTH    8
#define AUTH_PASSWORD_MAX_AGE       7776000 /* 90 days */

/* Error codes */
#define AUTH_SUCCESS                0
#define AUTH_ERROR_INVALID         -1
#define AUTH_ERROR_NOT_FOUND       -2
#define AUTH_ERROR_ALREADY_EXISTS  -3
#define AUTH_ERROR_ACCESS_DENIED   -4
#define AUTH_ERROR_INVALID_PASSWORD -5
#define AUTH_ERROR_ACCOUNT_LOCKED  -6
#define AUTH_ERROR_SESSION_EXPIRED -7
#define AUTH_ERROR_MFA_REQUIRED    -8
#define AUTH_ERROR_MFA_INVALID     -9
#define AUTH_ERROR_CRYPTO          -10
#define AUTH_ERROR_STORAGE         -11
#define AUTH_ERROR_MEMORY          -12
#define AUTH_ERROR_INVALID_TOKEN   -13
#define AUTH_ERROR_TOO_MANY_ATTEMPTS -14

/* ========================== Enumerations ========================== */

/* Hash algorithms for password storage */
typedef enum {
    AUTH_HASH_ARGON2ID = 0,     /* Preferred: Argon2id */
    AUTH_HASH_SCRYPT   = 1,     /* Alternative: scrypt */
    AUTH_HASH_BCRYPT   = 2,     /* Legacy: bcrypt */
    AUTH_HASH_PBKDF2   = 3      /* Fallback: PBKDF2 */
} auth_hash_algorithm_t;

/* User account status */
typedef enum {
    AUTH_ACCOUNT_ACTIVE = 0,    /* Account is active */
    AUTH_ACCOUNT_LOCKED = 1,    /* Account is locked */
    AUTH_ACCOUNT_DISABLED = 2,  /* Account is disabled */
    AUTH_ACCOUNT_EXPIRED = 3,   /* Account has expired */
    AUTH_ACCOUNT_PENDING = 4    /* Account pending activation */
} auth_account_status_t;

/* Session states */
typedef enum {
    AUTH_SESSION_VALID = 0,     /* Session is valid */
    AUTH_SESSION_EXPIRED = 1,   /* Session has expired */
    AUTH_SESSION_INVALID = 2,   /* Session is invalid */
    AUTH_SESSION_REVOKED = 3    /* Session was revoked */
} auth_session_state_t;

/* Permission scopes */
typedef enum {
    AUTH_SCOPE_SYSTEM = 0,      /* System-wide permission */
    AUTH_SCOPE_USER = 1,        /* User-specific permission */
    AUTH_SCOPE_GROUP = 2,       /* Group-specific permission */
    AUTH_SCOPE_RESOURCE = 3     /* Resource-specific permission */
} auth_permission_scope_t;

/* Authentication factors */
typedef enum {
    AUTH_FACTOR_PASSWORD = 0x01, /* Password authentication */
    AUTH_FACTOR_TOTP = 0x02,     /* Time-based OTP */
    AUTH_FACTOR_SMS = 0x04,      /* SMS-based OTP */
    AUTH_FACTOR_HARDWARE = 0x08, /* Hardware token */
    AUTH_FACTOR_BIOMETRIC = 0x10 /* Biometric authentication */
} auth_factor_t;

/* Audit event types */
typedef enum {
    AUTH_EVENT_LOGIN_SUCCESS = 0,
    AUTH_EVENT_LOGIN_FAILURE = 1,
    AUTH_EVENT_LOGOUT = 2,
    AUTH_EVENT_PASSWORD_CHANGE = 3,
    AUTH_EVENT_MFA_ENABLED = 4,
    AUTH_EVENT_MFA_DISABLED = 5,
    AUTH_EVENT_ACCOUNT_LOCKED = 6,
    AUTH_EVENT_ACCOUNT_UNLOCKED = 7,
    AUTH_EVENT_PERMISSION_GRANTED = 8,
    AUTH_EVENT_PERMISSION_DENIED = 9,
    AUTH_EVENT_ROLE_ASSIGNED = 10,
    AUTH_EVENT_ROLE_REVOKED = 11,
    AUTH_EVENT_SESSION_CREATED = 12,
    AUTH_EVENT_SESSION_EXPIRED = 13
} auth_event_type_t;

/* ========================== Core Data Structures ========================== */

/* User account structure */
typedef struct {
    uint32_t        user_id;                        /* Unique user identifier */
    char            username[AUTH_MAX_USERNAME_LENGTH]; /* Username */
    char            password_hash[AUTH_MAX_HASH_LENGTH]; /* Password hash */
    char            salt[AUTH_MAX_SALT_LENGTH];     /* Password salt */
    auth_hash_algorithm_t hash_algorithm;           /* Hash algorithm used */
    uint32_t        hash_rounds;                    /* Hash iteration count */
    
    /* Account metadata */
    time_t          created_time;                   /* Account creation time */
    time_t          last_login;                     /* Last successful login */
    time_t          last_password_change;           /* Last password change */
    time_t          password_expiry;                /* Password expiration time */
    time_t          account_expiry;                 /* Account expiration time */
    
    /* Security information */
    auth_account_status_t status;                   /* Account status */
    uint32_t        login_attempts;                 /* Failed login attempts */
    time_t          lockout_time;                   /* Account lockout time */
    bool            mfa_enabled;                    /* MFA enabled flag */
    char            mfa_secret[AUTH_MFA_SECRET_LENGTH]; /* MFA secret */
    uint8_t         auth_factors;                   /* Enabled auth factors */
    
    /* Group and role memberships */
    uint32_t        group_count;                    /* Number of groups */
    uint32_t        groups[AUTH_MAX_GROUPS_PER_USER]; /* Group IDs */
    uint32_t        role_count;                     /* Number of roles */
    uint32_t        roles[AUTH_MAX_ROLES_PER_USER]; /* Role IDs */
    
    /* Additional metadata */
    char            full_name[128];                 /* User's full name */
    char            email[256];                     /* Email address */
    char            home_directory[256];            /* Home directory path */
    char            shell[64];                      /* Default shell */
    uint32_t        uid;                            /* Unix user ID */
    uint32_t        gid;                            /* Unix group ID */
} user_account_t;

/* Session structure */
typedef struct {
    char            session_id[AUTH_MAX_SESSION_ID_LENGTH]; /* Session ID */
    uint32_t        user_id;                        /* Associated user ID */
    time_t          created_time;                   /* Session creation time */
    time_t          last_activity;                  /* Last activity time */
    time_t          expires_time;                   /* Session expiration time */
    
    /* Authentication state */
    bool            authenticated;                  /* User authenticated */
    bool            mfa_verified;                   /* MFA verification status */
    uint8_t         auth_factors_used;              /* Auth factors used */
    
    /* Session metadata */
    char            client_ip[16];                  /* Client IP address */
    char            user_agent[256];                /* Client user agent */
    uint32_t        process_id;                     /* Associated process ID */
    
    /* Security information */
    uint32_t        privilege_level;                /* Current privilege level */
    time_t          privilege_expiry;               /* Privilege expiration */
    bool            elevated_privileges;            /* Elevated privileges flag */
    
    /* Session management */
    auth_session_state_t state;                     /* Session state */
    bool            persistent;                     /* Persistent session flag */
    uint32_t        reference_count;                /* Reference count */
} session_t;

/* Permission structure */
typedef struct {
    uint32_t        permission_id;                  /* Unique permission ID */
    char            name[AUTH_MAX_PERM_NAME_LENGTH]; /* Permission name */
    char            description[256];               /* Permission description */
    auth_permission_scope_t scope;                  /* Permission scope */
    uint32_t        category;                       /* Permission category */
    bool            inheritable;                    /* Inheritable flag */
    uint32_t        priority;                       /* Permission priority */
} permission_t;

/* Role structure */
typedef struct {
    uint32_t        role_id;                        /* Unique role ID */
    char            name[AUTH_MAX_ROLE_NAME_LENGTH]; /* Role name */
    char            description[256];               /* Role description */
    uint32_t        permission_count;               /* Number of permissions */
    uint32_t*       permissions;                    /* Permission IDs */
    bool            system_role;                    /* System role flag */
    uint32_t        priority;                       /* Role priority */
    time_t          created_time;                   /* Role creation time */
} role_t;

/* Access Control List entry */
typedef struct {
    uint32_t        subject_id;                     /* User or group ID */
    bool            is_group;                       /* Subject is group flag */
    uint32_t        permissions;                    /* Permission bitmask */
    bool            allow;                          /* Allow or deny */
    time_t          expiry_time;                    /* Entry expiration */
} acl_entry_t;

/* Access Control List */
typedef struct {
    char            resource[AUTH_MAX_RESOURCE_LENGTH]; /* Resource path */
    uint32_t        entry_count;                    /* Number of entries */
    acl_entry_t*    entries;                        /* ACL entries */
    uint32_t        default_permissions;            /* Default permissions */
    bool            inherited;                      /* Inherited ACL flag */
    time_t          modified_time;                  /* Last modification time */
} access_control_list_t;

/* Permission set for efficient checking */
typedef struct {
    uint32_t        permissions[AUTH_MAX_PERMISSIONS / 32]; /* Permission bitmap */
    uint32_t        count;                          /* Number of permissions */
    time_t          computed_time;                  /* When computed */
    bool            cached;                         /* Cached result flag */
} permission_set_t;

/* Authentication configuration */
typedef struct {
    /* Password policy */
    uint32_t        min_password_length;            /* Minimum password length */
    uint32_t        max_password_length;            /* Maximum password length */
    bool            require_uppercase;              /* Require uppercase chars */
    bool            require_lowercase;              /* Require lowercase chars */
    bool            require_numbers;                /* Require numeric chars */
    bool            require_symbols;                /* Require symbol chars */
    uint32_t        password_history;               /* Password history count */
    uint32_t        password_max_age;               /* Max password age (days) */
    
    /* Login policy */
    uint32_t        max_login_attempts;             /* Max failed login attempts */
    uint32_t        lockout_duration;               /* Account lockout duration */
    bool            case_sensitive_usernames;       /* Case sensitive usernames */
    
    /* Session policy */
    uint32_t        session_timeout;                /* Session timeout (seconds) */
    uint32_t        idle_timeout;                   /* Idle timeout (seconds) */
    uint32_t        max_concurrent_sessions;        /* Max concurrent sessions */
    bool            require_mfa;                    /* Require MFA for all users */
    
    /* Security settings */
    auth_hash_algorithm_t default_hash_algorithm;   /* Default hash algorithm */
    uint32_t        hash_rounds;                    /* Hash iteration count */
    bool            audit_enabled;                  /* Audit logging enabled */
    bool            failed_login_delay;             /* Delay on failed login */
} auth_config_t;

/* Audit event structure */
typedef struct {
    uint64_t        event_id;                       /* Unique event ID */
    auth_event_type_t event_type;                   /* Event type */
    uint32_t        user_id;                        /* User ID (if applicable) */
    char            username[AUTH_MAX_USERNAME_LENGTH]; /* Username */
    time_t          timestamp;                      /* Event timestamp */
    char            client_ip[16];                  /* Client IP address */
    char            details[512];                   /* Event details */
    bool            success;                        /* Success flag */
    uint32_t        error_code;                     /* Error code (if failed) */
} auth_audit_event_t;

/* ========================== Core Authentication API ========================== */

/* System initialization and configuration */
int auth_init(const auth_config_t* config);
void auth_shutdown(void);
int auth_get_config(auth_config_t* config);
int auth_set_config(const auth_config_t* config);

/* User management */
int auth_create_user(const char* username, const char* password, 
                    const char* full_name, uint32_t* user_id);
int auth_delete_user(uint32_t user_id);
int auth_get_user(uint32_t user_id, user_account_t* account);
int auth_get_user_by_name(const char* username, user_account_t* account);
int auth_update_user(uint32_t user_id, const user_account_t* account);
int auth_list_users(user_account_t** users, uint32_t* count);

/* Password management */
int auth_change_password(uint32_t user_id, const char* old_password, 
                        const char* new_password);
int auth_reset_password(uint32_t user_id, const char* new_password);
int auth_verify_password(uint32_t user_id, const char* password);
int auth_check_password_policy(const char* password);
int auth_generate_password(char* password, size_t length, uint32_t flags);

/* Account management */
int auth_lock_account(uint32_t user_id);
int auth_unlock_account(uint32_t user_id);
int auth_disable_account(uint32_t user_id);
int auth_enable_account(uint32_t user_id);
int auth_set_account_expiry(uint32_t user_id, time_t expiry_time);

/* ========================== Session Management API ========================== */

/* Authentication and session management */
int auth_login(const char* username, const char* password, 
              const char* client_ip, session_t** session);
int auth_logout(const char* session_id);
int auth_verify_session(const char* session_id, session_t** session);
int auth_refresh_session(const char* session_id);
int auth_revoke_session(const char* session_id);
int auth_revoke_all_sessions(uint32_t user_id);

/* Session information */
int auth_get_active_sessions(uint32_t user_id, session_t** sessions, uint32_t* count);
int auth_session_activity(const char* session_id);
int auth_get_session_info(const char* session_id, session_t* session);

/* ========================== Multi-Factor Authentication API ========================== */

/* MFA setup and management */
int auth_enable_mfa(uint32_t user_id, const char* secret);
int auth_disable_mfa(uint32_t user_id);
int auth_generate_mfa_secret(char* secret, size_t length);
int auth_verify_mfa_token(uint32_t user_id, const char* token);
int auth_get_mfa_qr_code(uint32_t user_id, char* qr_code, size_t length);

/* Backup codes */
int auth_generate_backup_codes(uint32_t user_id, char codes[][16], int count);
int auth_verify_backup_code(uint32_t user_id, const char* code);
int auth_invalidate_backup_code(uint32_t user_id, const char* code);

/* ========================== Authorization API ========================== */

/* Role management */
int authz_create_role(const char* name, const char* description, uint32_t* role_id);
int authz_delete_role(uint32_t role_id);
int authz_get_role(uint32_t role_id, role_t* role);
int authz_list_roles(role_t** roles, uint32_t* count);
int authz_assign_role(uint32_t user_id, uint32_t role_id);
int authz_revoke_role(uint32_t user_id, uint32_t role_id);
int authz_check_role(uint32_t user_id, uint32_t role_id);

/* Permission management */
int authz_create_permission(const char* name, const char* description, 
                           auth_permission_scope_t scope, uint32_t* perm_id);
int authz_delete_permission(uint32_t permission_id);
int authz_grant_permission(uint32_t user_id, uint32_t permission_id);
int authz_revoke_permission(uint32_t user_id, uint32_t permission_id);
int authz_check_permission(uint32_t user_id, uint32_t permission_id);
int authz_list_permissions(permission_t** permissions, uint32_t* count);

/* Role-permission management */
int authz_add_permission_to_role(uint32_t role_id, uint32_t permission_id);
int authz_remove_permission_from_role(uint32_t role_id, uint32_t permission_id);
int authz_get_role_permissions(uint32_t role_id, uint32_t** permissions, uint32_t* count);

/* Access control */
int authz_check_access(uint32_t user_id, const char* resource, const char* action);
int authz_set_acl(const char* resource, const acl_entry_t* entries, size_t count);
int authz_get_acl(const char* resource, access_control_list_t* acl);
int authz_get_effective_permissions(uint32_t user_id, permission_set_t* permissions);

/* ========================== Group Management API ========================== */

/* Group operations */
int auth_create_group(const char* name, const char* description, uint32_t* group_id);
int auth_delete_group(uint32_t group_id);
int auth_add_user_to_group(uint32_t user_id, uint32_t group_id);
int auth_remove_user_from_group(uint32_t user_id, uint32_t group_id);
int auth_get_group_members(uint32_t group_id, uint32_t** user_ids, uint32_t* count);
int auth_get_user_groups(uint32_t user_id, uint32_t** group_ids, uint32_t* count);

/* ========================== Privilege Management API ========================== */

/* Privilege escalation */
int auth_elevate_privileges(const char* session_id, uint32_t privilege_level, 
                           const char* password);
int auth_drop_privileges(const char* session_id);
int auth_check_privilege(const char* session_id, uint32_t required_level);
int auth_get_privilege_level(const char* session_id, uint32_t* level);

/* Temporary privileges */
int auth_grant_temporary_privilege(uint32_t user_id, uint32_t permission_id, 
                                  time_t duration);
int auth_revoke_temporary_privilege(uint32_t user_id, uint32_t permission_id);

/* ========================== Audit and Logging API ========================== */

/* Audit logging */
int auth_log_event(auth_event_type_t event_type, uint32_t user_id, 
                  const char* client_ip, const char* details, bool success);
int auth_get_audit_events(time_t start_time, time_t end_time, 
                         auth_audit_event_t** events, uint32_t* count);
int auth_search_audit_events(const char* criteria, auth_audit_event_t** events, 
                            uint32_t* count);

/* ========================== Cryptographic Utilities ========================== */

/* Password hashing */
int auth_hash_password(const char* password, const char* salt, 
                      auth_hash_algorithm_t algorithm, uint32_t rounds,
                      char* hash, size_t hash_size);
int auth_verify_password_hash(const char* password, const char* salt,
                             const char* hash, auth_hash_algorithm_t algorithm,
                             uint32_t rounds);
int auth_generate_salt(char* salt, size_t length);

/* Secure random generation */
int auth_generate_random(void* buffer, size_t length);
int auth_generate_session_id(char* session_id, size_t length);

/* Key derivation */
int auth_derive_key(const char* password, const char* salt, uint32_t iterations,
                   void* key, size_t key_length);

/* ========================== Utility Functions ========================== */

/* String utilities */
const char* auth_error_string(int error_code);
const char* auth_event_type_string(auth_event_type_t event_type);
const char* auth_account_status_string(auth_account_status_t status);
const char* auth_session_state_string(auth_session_state_t state);

/* Time utilities */
time_t auth_get_current_time(void);
bool auth_time_expired(time_t expiry_time);
int auth_format_time(time_t time, char* buffer, size_t size);

/* Validation utilities */
bool auth_validate_username(const char* username);
bool auth_validate_email(const char* email);
bool auth_validate_password_complexity(const char* password);

/* ========================== Debugging and Statistics ========================== */

/* System statistics */
typedef struct {
    uint32_t        total_users;                    /* Total user count */
    uint32_t        active_users;                   /* Active user count */
    uint32_t        locked_users;                   /* Locked user count */
    uint32_t        active_sessions;                /* Active session count */
    uint32_t        failed_logins_24h;              /* Failed logins in 24h */
    uint32_t        successful_logins_24h;          /* Successful logins in 24h */
    uint64_t        total_auth_requests;            /* Total auth requests */
    uint64_t        total_authz_requests;           /* Total authz requests */
    time_t          system_start_time;              /* System start time */
    time_t          last_login_time;                /* Last login time */
} auth_statistics_t;

int auth_get_statistics(auth_statistics_t* stats);
int auth_reset_statistics(void);

/* System health and debugging */
int auth_self_test(void);
int auth_check_integrity(void);
int auth_dump_state(char* buffer, size_t size);

/* ========================== Configuration Constants ========================== */

/* Predefined roles */
#define AUTH_ROLE_ADMIN         1
#define AUTH_ROLE_USER          2
#define AUTH_ROLE_GUEST         3
#define AUTH_ROLE_OPERATOR      4
#define AUTH_ROLE_AUDITOR       5

/* Predefined permissions */
#define AUTH_PERM_LOGIN                 1
#define AUTH_PERM_CHANGE_PASSWORD       2
#define AUTH_PERM_READ_FILE             3
#define AUTH_PERM_WRITE_FILE            4
#define AUTH_PERM_EXECUTE_FILE          5
#define AUTH_PERM_CREATE_USER           6
#define AUTH_PERM_DELETE_USER           7
#define AUTH_PERM_MODIFY_USER           8
#define AUTH_PERM_ADMIN_SYSTEM          9
#define AUTH_PERM_VIEW_LOGS            10
#define AUTH_PERM_MODIFY_ROLES         11
#define AUTH_PERM_MODIFY_PERMISSIONS   12

/* Privilege levels */
#define AUTH_PRIV_LEVEL_GUEST           0
#define AUTH_PRIV_LEVEL_USER            1
#define AUTH_PRIV_LEVEL_OPERATOR        2
#define AUTH_PRIV_LEVEL_ADMIN           3
#define AUTH_PRIV_LEVEL_SYSTEM          4

#endif /* AUTH_SYSTEM_H */
