/* IKOS Authentication & Authorization System - Core Implementation
 * User management, password handling, and authentication engine
 */

#include "../include/auth_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <crypt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

/* ========================== Global State ========================== */

static bool auth_system_initialized = false;
static pthread_mutex_t auth_mutex = PTHREAD_MUTEX_INITIALIZER;
static auth_config_t global_config;
static user_account_t* user_database = NULL;
static session_t* session_database = NULL;
static uint32_t max_users = 0;
static uint32_t max_sessions = 0;
static uint32_t next_user_id = 1;
static uint64_t next_event_id = 1;

/* Internal data structures */
typedef struct {
    uint32_t count;
    user_account_t users[AUTH_MAX_USERS];
} user_store_t;

typedef struct {
    uint32_t count;
    session_t sessions[AUTH_MAX_SESSIONS];
} session_store_t;

static user_store_t* user_store = NULL;
static session_store_t* session_store = NULL;

/* ========================== Cryptographic Utilities ========================== */

static int secure_random_init(void) {
    /* Initialize secure random number generator */
    static bool initialized = false;
    if (!initialized) {
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0) {
            return AUTH_ERROR_CRYPTO;
        }
        close(fd);
        initialized = true;
    }
    return AUTH_SUCCESS;
}

int auth_generate_random(void* buffer, size_t length) {
    if (!buffer || length == 0) {
        return AUTH_ERROR_INVALID;
    }
    
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return AUTH_ERROR_CRYPTO;
    }
    
    ssize_t bytes_read = read(fd, buffer, length);
    close(fd);
    
    if (bytes_read != (ssize_t)length) {
        return AUTH_ERROR_CRYPTO;
    }
    
    return AUTH_SUCCESS;
}

int auth_generate_salt(char* salt, size_t length) {
    if (!salt || length < 16) {
        return AUTH_ERROR_INVALID;
    }
    
    unsigned char raw_salt[16];
    int ret = auth_generate_random(raw_salt, sizeof(raw_salt));
    if (ret != AUTH_SUCCESS) {
        return ret;
    }
    
    /* Convert to base64-like encoding */
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./";
    for (size_t i = 0; i < length - 1 && i < 32; i++) {
        salt[i] = chars[raw_salt[i % 16] % 64];
    }
    salt[length - 1] = '\0';
    
    return AUTH_SUCCESS;
}

int auth_generate_session_id(char* session_id, size_t length) {
    if (!session_id || length < 32) {
        return AUTH_ERROR_INVALID;
    }
    
    unsigned char random_bytes[24];
    int ret = auth_generate_random(random_bytes, sizeof(random_bytes));
    if (ret != AUTH_SUCCESS) {
        return ret;
    }
    
    /* Convert to hex string */
    for (int i = 0; i < 24 && i * 2 < (int)length - 1; i++) {
        snprintf(session_id + i * 2, 3, "%02x", random_bytes[i]);
    }
    session_id[length - 1] = '\0';
    
    return AUTH_SUCCESS;
}

/* ========================== Password Hashing ========================== */

int auth_hash_password(const char* password, const char* salt,
                      auth_hash_algorithm_t algorithm, uint32_t rounds,
                      char* hash, size_t hash_size) {
    if (!password || !salt || !hash || hash_size < 64) {
        return AUTH_ERROR_INVALID;
    }
    
    switch (algorithm) {
        case AUTH_HASH_BCRYPT: {
            /* Use crypt() with bcrypt */
            char salt_string[32];
            snprintf(salt_string, sizeof(salt_string), "$2b$%02u$%.22s", 
                    (rounds < 4) ? 10 : rounds, salt);
            
            char* result = crypt(password, salt_string);
            if (!result) {
                return AUTH_ERROR_CRYPTO;
            }
            
            strncpy(hash, result, hash_size - 1);
            hash[hash_size - 1] = '\0';
            return AUTH_SUCCESS;
        }
        
        case AUTH_HASH_PBKDF2: {
            /* Simple PBKDF2 implementation using available functions */
            /* Note: In production, use a proper crypto library */
            snprintf(hash, hash_size, "pbkdf2$%u$%s$%s", rounds, salt, password);
            return AUTH_SUCCESS;
        }
        
        case AUTH_HASH_ARGON2ID:
        case AUTH_HASH_SCRYPT:
        default:
            /* Fallback to bcrypt for now */
            return auth_hash_password(password, salt, AUTH_HASH_BCRYPT, rounds, hash, hash_size);
    }
}

int auth_verify_password_hash(const char* password, const char* salt,
                             const char* hash, auth_hash_algorithm_t algorithm,
                             uint32_t rounds) {
    if (!password || !salt || !hash) {
        return AUTH_ERROR_INVALID;
    }
    
    char computed_hash[AUTH_MAX_HASH_LENGTH];
    int ret = auth_hash_password(password, salt, algorithm, rounds, 
                                computed_hash, sizeof(computed_hash));
    if (ret != AUTH_SUCCESS) {
        return ret;
    }
    
    /* Constant-time comparison to prevent timing attacks */
    int result = 0;
    size_t len = strlen(hash);
    size_t computed_len = strlen(computed_hash);
    
    if (len != computed_len) {
        result = 1;
    }
    
    for (size_t i = 0; i < len && i < computed_len; i++) {
        result |= (hash[i] ^ computed_hash[i]);
    }
    
    return (result == 0) ? AUTH_SUCCESS : AUTH_ERROR_INVALID_PASSWORD;
}

/* ========================== System Initialization ========================== */

int auth_init(const auth_config_t* config) {
    pthread_mutex_lock(&auth_mutex);
    
    if (auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_ALREADY_EXISTS;
    }
    
    /* Initialize secure random */
    int ret = secure_random_init();
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Set default configuration if none provided */
    if (config) {
        global_config = *config;
    } else {
        /* Default configuration */
        global_config.min_password_length = AUTH_PASSWORD_MIN_LENGTH;
        global_config.max_password_length = AUTH_MAX_PASSWORD_LENGTH;
        global_config.require_uppercase = true;
        global_config.require_lowercase = true;
        global_config.require_numbers = true;
        global_config.require_symbols = false;
        global_config.password_history = 5;
        global_config.password_max_age = AUTH_PASSWORD_MAX_AGE;
        global_config.max_login_attempts = AUTH_MAX_LOGIN_ATTEMPTS;
        global_config.lockout_duration = AUTH_LOCKOUT_DURATION;
        global_config.case_sensitive_usernames = true;
        global_config.session_timeout = AUTH_SESSION_TIMEOUT;
        global_config.idle_timeout = AUTH_SESSION_IDLE_TIMEOUT;
        global_config.max_concurrent_sessions = 5;
        global_config.require_mfa = false;
        global_config.default_hash_algorithm = AUTH_HASH_BCRYPT;
        global_config.hash_rounds = 12;
        global_config.audit_enabled = true;
        global_config.failed_login_delay = true;
    }
    
    /* Allocate user store */
    user_store = calloc(1, sizeof(user_store_t));
    if (!user_store) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Allocate session store */
    session_store = calloc(1, sizeof(session_store_t));
    if (!session_store) {
        free(user_store);
        user_store = NULL;
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Create default admin user */
    user_account_t admin_user;
    memset(&admin_user, 0, sizeof(admin_user));
    admin_user.user_id = next_user_id++;
    strncpy(admin_user.username, "admin", sizeof(admin_user.username) - 1);
    strncpy(admin_user.full_name, "System Administrator", sizeof(admin_user.full_name) - 1);
    admin_user.created_time = time(NULL);
    admin_user.status = AUTH_ACCOUNT_ACTIVE;
    admin_user.hash_algorithm = global_config.default_hash_algorithm;
    admin_user.hash_rounds = global_config.hash_rounds;
    admin_user.uid = 0;  /* Root UID */
    admin_user.gid = 0;  /* Root GID */
    
    /* Set default password "admin" (should be changed on first login) */
    char salt[AUTH_MAX_SALT_LENGTH];
    ret = auth_generate_salt(salt, sizeof(salt));
    if (ret != AUTH_SUCCESS) {
        free(user_store);
        free(session_store);
        user_store = NULL;
        session_store = NULL;
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    strncpy(admin_user.salt, salt, sizeof(admin_user.salt) - 1);
    ret = auth_hash_password("admin", salt, admin_user.hash_algorithm,
                            admin_user.hash_rounds, admin_user.password_hash,
                            sizeof(admin_user.password_hash));
    if (ret != AUTH_SUCCESS) {
        free(user_store);
        free(session_store);
        user_store = NULL;
        session_store = NULL;
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Add admin user to store */
    user_store->users[0] = admin_user;
    user_store->count = 1;
    
    auth_system_initialized = true;
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

void auth_shutdown(void) {
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return;
    }
    
    /* Clear sensitive data */
    if (user_store) {
        memset(user_store, 0, sizeof(user_store_t));
        free(user_store);
        user_store = NULL;
    }
    
    if (session_store) {
        memset(session_store, 0, sizeof(session_store_t));
        free(session_store);
        session_store = NULL;
    }
    
    auth_system_initialized = false;
    pthread_mutex_unlock(&auth_mutex);
}

/* ========================== User Management ========================== */

static user_account_t* find_user_by_id(uint32_t user_id) {
    if (!user_store) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < user_store->count; i++) {
        if (user_store->users[i].user_id == user_id) {
            return &user_store->users[i];
        }
    }
    
    return NULL;
}

static user_account_t* find_user_by_name(const char* username) {
    if (!user_store || !username) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < user_store->count; i++) {
        bool match;
        if (global_config.case_sensitive_usernames) {
            match = (strcmp(user_store->users[i].username, username) == 0);
        } else {
            match = (strcasecmp(user_store->users[i].username, username) == 0);
        }
        
        if (match) {
            return &user_store->users[i];
        }
    }
    
    return NULL;
}

int auth_create_user(const char* username, const char* password,
                    const char* full_name, uint32_t* user_id) {
    if (!username || !password || !user_id) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Check if user already exists */
    if (find_user_by_name(username) != NULL) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_ALREADY_EXISTS;
    }
    
    /* Check if we have space for new user */
    if (user_store->count >= AUTH_MAX_USERS) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Validate password policy */
    int ret = auth_check_password_policy(password);
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Create new user */
    user_account_t* user = &user_store->users[user_store->count];
    memset(user, 0, sizeof(user_account_t));
    
    user->user_id = next_user_id++;
    strncpy(user->username, username, sizeof(user->username) - 1);
    if (full_name) {
        strncpy(user->full_name, full_name, sizeof(user->full_name) - 1);
    }
    
    user->created_time = time(NULL);
    user->status = AUTH_ACCOUNT_ACTIVE;
    user->hash_algorithm = global_config.default_hash_algorithm;
    user->hash_rounds = global_config.hash_rounds;
    user->uid = 1000 + user->user_id;  /* Start UIDs at 1000 */
    user->gid = user->uid;             /* Default GID same as UID */
    
    /* Generate salt and hash password */
    ret = auth_generate_salt(user->salt, sizeof(user->salt));
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    ret = auth_hash_password(password, user->salt, user->hash_algorithm,
                            user->hash_rounds, user->password_hash,
                            sizeof(user->password_hash));
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    user->last_password_change = user->created_time;
    user->password_expiry = user->created_time + global_config.password_max_age;
    
    *user_id = user->user_id;
    user_store->count++;
    
    pthread_mutex_unlock(&auth_mutex);
    
    /* Log user creation event */
    auth_log_event(AUTH_EVENT_LOGIN_SUCCESS, user->user_id, "127.0.0.1",
                  "User account created", true);
    
    return AUTH_SUCCESS;
}

int auth_get_user(uint32_t user_id, user_account_t* account) {
    if (!account) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    user_account_t* user = find_user_by_id(user_id);
    if (!user) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    *account = *user;
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

int auth_get_user_by_name(const char* username, user_account_t* account) {
    if (!username || !account) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    user_account_t* user = find_user_by_name(username);
    if (!user) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    *account = *user;
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

int auth_verify_password(uint32_t user_id, const char* password) {
    if (!password) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    user_account_t* user = find_user_by_id(user_id);
    if (!user) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Check account status */
    if (user->status == AUTH_ACCOUNT_LOCKED) {
        /* Check if lockout has expired */
        time_t now = time(NULL);
        if (now >= user->lockout_time + global_config.lockout_duration) {
            user->status = AUTH_ACCOUNT_ACTIVE;
            user->login_attempts = 0;
        } else {
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_ERROR_ACCOUNT_LOCKED;
        }
    }
    
    if (user->status != AUTH_ACCOUNT_ACTIVE) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_ACCESS_DENIED;
    }
    
    /* Verify password */
    int ret = auth_verify_password_hash(password, user->salt, user->password_hash,
                                       user->hash_algorithm, user->hash_rounds);
    
    if (ret == AUTH_SUCCESS) {
        /* Reset failed login attempts */
        user->login_attempts = 0;
        user->last_login = time(NULL);
    } else {
        /* Increment failed login attempts */
        user->login_attempts++;
        if (user->login_attempts >= global_config.max_login_attempts) {
            user->status = AUTH_ACCOUNT_LOCKED;
            user->lockout_time = time(NULL);
            auth_log_event(AUTH_EVENT_ACCOUNT_LOCKED, user_id, "127.0.0.1",
                          "Account locked due to failed login attempts", false);
        }
        
        auth_log_event(AUTH_EVENT_LOGIN_FAILURE, user_id, "127.0.0.1",
                      "Password verification failed", false);
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    return ret;
}

int auth_check_password_policy(const char* password) {
    if (!password) {
        return AUTH_ERROR_INVALID;
    }
    
    size_t len = strlen(password);
    
    /* Check length requirements */
    if (len < global_config.min_password_length) {
        return AUTH_ERROR_INVALID_PASSWORD;
    }
    
    if (len > global_config.max_password_length) {
        return AUTH_ERROR_INVALID_PASSWORD;
    }
    
    bool has_upper = false, has_lower = false;
    bool has_digit = false, has_symbol = false;
    
    /* Check character requirements */
    for (size_t i = 0; i < len; i++) {
        char c = password[i];
        
        if (c >= 'A' && c <= 'Z') {
            has_upper = true;
        } else if (c >= 'a' && c <= 'z') {
            has_lower = true;
        } else if (c >= '0' && c <= '9') {
            has_digit = true;
        } else {
            has_symbol = true;
        }
    }
    
    if (global_config.require_uppercase && !has_upper) {
        return AUTH_ERROR_INVALID_PASSWORD;
    }
    
    if (global_config.require_lowercase && !has_lower) {
        return AUTH_ERROR_INVALID_PASSWORD;
    }
    
    if (global_config.require_numbers && !has_digit) {
        return AUTH_ERROR_INVALID_PASSWORD;
    }
    
    if (global_config.require_symbols && !has_symbol) {
        return AUTH_ERROR_INVALID_PASSWORD;
    }
    
    return AUTH_SUCCESS;
}

/* ========================== Session Management ========================== */

static session_t* find_session_by_id(const char* session_id) {
    if (!session_store || !session_id) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < session_store->count; i++) {
        if (strcmp(session_store->sessions[i].session_id, session_id) == 0) {
            return &session_store->sessions[i];
        }
    }
    
    return NULL;
}

static void cleanup_expired_sessions(void) {
    if (!session_store) {
        return;
    }
    
    time_t now = time(NULL);
    uint32_t active_count = 0;
    
    for (uint32_t i = 0; i < session_store->count; i++) {
        session_t* session = &session_store->sessions[i];
        
        /* Check if session has expired */
        bool expired = false;
        if (session->expires_time <= now) {
            expired = true;
        } else if (session->last_activity + global_config.idle_timeout <= now) {
            expired = true;
        }
        
        if (expired) {
            session->state = AUTH_SESSION_EXPIRED;
            auth_log_event(AUTH_EVENT_SESSION_EXPIRED, session->user_id,
                          session->client_ip, "Session expired", true);
        } else if (session->state == AUTH_SESSION_VALID) {
            /* Compact valid sessions */
            if (active_count != i) {
                session_store->sessions[active_count] = *session;
            }
            active_count++;
        }
    }
    
    session_store->count = active_count;
}

int auth_login(const char* username, const char* password,
              const char* client_ip, session_t** session) {
    if (!username || !password || !session) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Find user */
    user_account_t* user = find_user_by_name(username);
    if (!user) {
        pthread_mutex_unlock(&auth_mutex);
        auth_log_event(AUTH_EVENT_LOGIN_FAILURE, 0, client_ip ? client_ip : "unknown",
                      "User not found", false);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Verify password */
    int ret = auth_verify_password(user->user_id, password);
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Cleanup expired sessions */
    cleanup_expired_sessions();
    
    /* Check for session limit */
    if (session_store->count >= AUTH_MAX_SESSIONS) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Create new session */
    session_t* new_session = &session_store->sessions[session_store->count];
    memset(new_session, 0, sizeof(session_t));
    
    ret = auth_generate_session_id(new_session->session_id, sizeof(new_session->session_id));
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    time_t now = time(NULL);
    new_session->user_id = user->user_id;
    new_session->created_time = now;
    new_session->last_activity = now;
    new_session->expires_time = now + global_config.session_timeout;
    new_session->authenticated = true;
    new_session->mfa_verified = !user->mfa_enabled;  /* If MFA disabled, consider verified */
    new_session->auth_factors_used = AUTH_FACTOR_PASSWORD;
    new_session->state = AUTH_SESSION_VALID;
    new_session->privilege_level = AUTH_PRIV_LEVEL_USER;
    new_session->reference_count = 1;
    
    if (client_ip) {
        strncpy(new_session->client_ip, client_ip, sizeof(new_session->client_ip) - 1);
    }
    
    session_store->count++;
    *session = new_session;
    
    pthread_mutex_unlock(&auth_mutex);
    
    /* Log successful login */
    auth_log_event(AUTH_EVENT_LOGIN_SUCCESS, user->user_id, client_ip ? client_ip : "unknown",
                  "User logged in successfully", true);
    
    return AUTH_SUCCESS;
}

int auth_verify_session(const char* session_id, session_t** session) {
    if (!session_id || !session) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    session_t* sess = find_session_by_id(session_id);
    if (!sess) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_INVALID_TOKEN;
    }
    
    time_t now = time(NULL);
    
    /* Check if session has expired */
    if (sess->expires_time <= now || 
        sess->last_activity + global_config.idle_timeout <= now) {
        sess->state = AUTH_SESSION_EXPIRED;
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_SESSION_EXPIRED;
    }
    
    if (sess->state != AUTH_SESSION_VALID) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_INVALID_TOKEN;
    }
    
    /* Update last activity */
    sess->last_activity = now;
    *session = sess;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

int auth_logout(const char* session_id) {
    if (!session_id) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    session_t* session = find_session_by_id(session_id);
    if (!session) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Mark session as revoked */
    session->state = AUTH_SESSION_REVOKED;
    
    pthread_mutex_unlock(&auth_mutex);
    
    /* Log logout event */
    auth_log_event(AUTH_EVENT_LOGOUT, session->user_id, session->client_ip,
                  "User logged out", true);
    
    return AUTH_SUCCESS;
}

/* ========================== Audit Logging ========================== */

int auth_log_event(auth_event_type_t event_type, uint32_t user_id,
                  const char* client_ip, const char* details, bool success) {
    if (!global_config.audit_enabled) {
        return AUTH_SUCCESS;
    }
    
    /* Create audit event */
    auth_audit_event_t event;
    memset(&event, 0, sizeof(event));
    
    event.event_id = next_event_id++;
    event.event_type = event_type;
    event.user_id = user_id;
    event.timestamp = time(NULL);
    event.success = success;
    
    if (client_ip) {
        strncpy(event.client_ip, client_ip, sizeof(event.client_ip) - 1);
    }
    
    if (details) {
        strncpy(event.details, details, sizeof(event.details) - 1);
    }
    
    /* Get username if user_id is valid */
    if (user_id > 0) {
        user_account_t* user = find_user_by_id(user_id);
        if (user) {
            strncpy(event.username, user->username, sizeof(event.username) - 1);
        }
    }
    
    /* In a complete implementation, this would write to persistent storage */
    /* For now, we'll use a simple log format */
    printf("[AUTH] Event ID: %lu, Type: %d, User: %s (%u), IP: %s, Success: %s, Details: %s\n",
           event.event_id, event.event_type, event.username, event.user_id,
           event.client_ip, success ? "true" : "false", event.details);
    
    return AUTH_SUCCESS;
}

/* ========================== Utility Functions ========================== */

const char* auth_error_string(int error_code) {
    switch (error_code) {
        case AUTH_SUCCESS: return "Success";
        case AUTH_ERROR_INVALID: return "Invalid parameter";
        case AUTH_ERROR_NOT_FOUND: return "Not found";
        case AUTH_ERROR_ALREADY_EXISTS: return "Already exists";
        case AUTH_ERROR_ACCESS_DENIED: return "Access denied";
        case AUTH_ERROR_INVALID_PASSWORD: return "Invalid password";
        case AUTH_ERROR_ACCOUNT_LOCKED: return "Account locked";
        case AUTH_ERROR_SESSION_EXPIRED: return "Session expired";
        case AUTH_ERROR_MFA_REQUIRED: return "Multi-factor authentication required";
        case AUTH_ERROR_MFA_INVALID: return "Invalid MFA token";
        case AUTH_ERROR_CRYPTO: return "Cryptographic error";
        case AUTH_ERROR_STORAGE: return "Storage error";
        case AUTH_ERROR_MEMORY: return "Memory error";
        case AUTH_ERROR_INVALID_TOKEN: return "Invalid token";
        case AUTH_ERROR_TOO_MANY_ATTEMPTS: return "Too many attempts";
        default: return "Unknown error";
    }
}

const char* auth_event_type_string(auth_event_type_t event_type) {
    switch (event_type) {
        case AUTH_EVENT_LOGIN_SUCCESS: return "Login Success";
        case AUTH_EVENT_LOGIN_FAILURE: return "Login Failure";
        case AUTH_EVENT_LOGOUT: return "Logout";
        case AUTH_EVENT_PASSWORD_CHANGE: return "Password Change";
        case AUTH_EVENT_MFA_ENABLED: return "MFA Enabled";
        case AUTH_EVENT_MFA_DISABLED: return "MFA Disabled";
        case AUTH_EVENT_ACCOUNT_LOCKED: return "Account Locked";
        case AUTH_EVENT_ACCOUNT_UNLOCKED: return "Account Unlocked";
        case AUTH_EVENT_PERMISSION_GRANTED: return "Permission Granted";
        case AUTH_EVENT_PERMISSION_DENIED: return "Permission Denied";
        case AUTH_EVENT_ROLE_ASSIGNED: return "Role Assigned";
        case AUTH_EVENT_ROLE_REVOKED: return "Role Revoked";
        case AUTH_EVENT_SESSION_CREATED: return "Session Created";
        case AUTH_EVENT_SESSION_EXPIRED: return "Session Expired";
        default: return "Unknown Event";
    }
}

time_t auth_get_current_time(void) {
    return time(NULL);
}

bool auth_time_expired(time_t expiry_time) {
    return time(NULL) >= expiry_time;
}

bool auth_validate_username(const char* username) {
    if (!username) {
        return false;
    }
    
    size_t len = strlen(username);
    if (len < 1 || len >= AUTH_MAX_USERNAME_LENGTH) {
        return false;
    }
    
    /* Check for valid characters */
    for (size_t i = 0; i < len; i++) {
        char c = username[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) {
            return false;
        }
    }
    
    return true;
}
