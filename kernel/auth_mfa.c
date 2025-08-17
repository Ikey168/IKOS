/* IKOS Authentication & Authorization System - Multi-Factor Authentication
 * TOTP, backup codes, and MFA verification
 */

#include "../include/auth_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

/* ========================== TOTP Implementation ========================== */

#define TOTP_WINDOW_SIZE 30  /* 30 second window */
#define TOTP_DIGITS 6        /* 6 digit codes */
#define TOTP_TOLERANCE 1     /* Allow 1 window before/after */

static uint32_t hotp(const uint8_t* key, size_t key_len, uint64_t counter) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int hash_len;
    
    /* Calculate HMAC-SHA1 */
    HMAC(EVP_sha1(), key, key_len, (unsigned char*)&counter, sizeof(counter),
         hash, &hash_len);
    
    /* Dynamic truncation */
    int offset = hash[hash_len - 1] & 0x0F;
    uint32_t code = ((hash[offset] & 0x7F) << 24) |
                    ((hash[offset + 1] & 0xFF) << 16) |
                    ((hash[offset + 2] & 0xFF) << 8) |
                    (hash[offset + 3] & 0xFF);
    
    /* Reduce to required digits */
    code = code % (uint32_t)pow(10, TOTP_DIGITS);
    
    return code;
}

static uint32_t totp(const uint8_t* secret, size_t secret_len, uint64_t timestamp) {
    uint64_t counter = timestamp / TOTP_WINDOW_SIZE;
    
    /* Convert to big-endian */
    counter = ((counter & 0xFF00000000000000ULL) >> 56) |
              ((counter & 0x00FF000000000000ULL) >> 40) |
              ((counter & 0x0000FF0000000000ULL) >> 24) |
              ((counter & 0x000000FF00000000ULL) >> 8) |
              ((counter & 0x00000000FF000000ULL) << 8) |
              ((counter & 0x0000000000FF0000ULL) << 24) |
              ((counter & 0x000000000000FF00ULL) << 40) |
              ((counter & 0x00000000000000FFULL) << 56);
    
    return hotp(secret, secret_len, counter);
}

/* ========================== Base32 Encoding for TOTP Secrets ========================== */

static const char base32_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static int base32_encode(const uint8_t* data, size_t len, char* encoded, size_t encoded_len) {
    if (!data || !encoded) {
        return -1;
    }
    
    size_t output_len = ((len + 4) / 5) * 8;
    if (encoded_len < output_len + 1) {
        return -1;
    }
    
    size_t i = 0, j = 0;
    uint32_t buffer = 0;
    int bits = 0;
    
    while (i < len) {
        buffer = (buffer << 8) | data[i++];
        bits += 8;
        
        while (bits >= 5) {
            encoded[j++] = base32_alphabet[(buffer >> (bits - 5)) & 0x1F];
            bits -= 5;
        }
    }
    
    if (bits > 0) {
        encoded[j++] = base32_alphabet[(buffer << (5 - bits)) & 0x1F];
    }
    
    /* Add padding */
    while (j % 8 != 0) {
        encoded[j++] = '=';
    }
    
    encoded[j] = '\0';
    return j;
}

static int base32_decode(const char* encoded, uint8_t* data, size_t data_len) {
    if (!encoded || !data) {
        return -1;
    }
    
    size_t input_len = strlen(encoded);
    size_t output_len = (input_len * 5) / 8;
    
    if (data_len < output_len) {
        return -1;
    }
    
    size_t i = 0, j = 0;
    uint32_t buffer = 0;
    int bits = 0;
    
    while (i < input_len && encoded[i] != '=') {
        char c = encoded[i++];
        int value = -1;
        
        if (c >= 'A' && c <= 'Z') {
            value = c - 'A';
        } else if (c >= '2' && c <= '7') {
            value = c - '2' + 26;
        }
        
        if (value == -1) {
            return -1;
        }
        
        buffer = (buffer << 5) | value;
        bits += 5;
        
        if (bits >= 8) {
            data[j++] = (buffer >> (bits - 8)) & 0xFF;
            bits -= 8;
        }
    }
    
    return j;
}

/* ========================== MFA Secret Management ========================== */

int auth_mfa_generate_secret(uint32_t user_id, char* secret_b32, size_t secret_len) {
    if (!secret_b32) {
        return AUTH_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&auth_mutex);
    
    if (!auth_system_initialized) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Generate random secret */
    uint8_t secret_bytes[20];  /* 160-bit secret */
    if (RAND_bytes(secret_bytes, sizeof(secret_bytes)) != 1) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_CRYPTO;
    }
    
    /* Encode to Base32 */
    int encoded_len = base32_encode(secret_bytes, sizeof(secret_bytes), 
                                   secret_b32, secret_len);
    if (encoded_len < 0) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Store secret in user account */
    user_account_t account;
    int ret = auth_get_user(user_id, &account);
    if (ret != AUTH_SUCCESS) {
        pthread_mutex_unlock(&auth_mutex);
        return ret;
    }
    
    /* Initialize MFA settings */
    account.mfa_enabled = false;  /* Not enabled until verified */
    memcpy(account.mfa_secret, secret_bytes, sizeof(secret_bytes));
    account.mfa_secret_len = sizeof(secret_bytes);
    account.mfa_backup_codes_count = 0;
    account.mfa_last_used_time = 0;
    
    /* This would need to update the actual user store */
    /* For now, we'll assume success */
    
    pthread_mutex_unlock(&auth_mutex);
    
    auth_log_event(AUTH_EVENT_MFA_SECRET_GENERATED, user_id, "127.0.0.1",
                  "MFA secret generated", true);
    
    return AUTH_SUCCESS;
}

int auth_mfa_verify_setup(uint32_t user_id, const char* totp_code) {
    if (!totp_code) {
        return AUTH_ERROR_INVALID;
    }
    
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
    
    /* Check if MFA secret exists */
    if (account.mfa_secret_len == 0) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Verify TOTP code */
    uint32_t provided_code = strtoul(totp_code, NULL, 10);
    uint64_t current_time = time(NULL);
    
    /* Check current window and tolerance windows */
    for (int i = -TOTP_TOLERANCE; i <= TOTP_TOLERANCE; i++) {
        uint64_t test_time = current_time + (i * TOTP_WINDOW_SIZE);
        uint32_t expected_code = totp(account.mfa_secret, account.mfa_secret_len, test_time);
        
        if (provided_code == expected_code) {
            /* Enable MFA for user */
            account.mfa_enabled = true;
            account.mfa_last_used_time = current_time;
            
            /* Generate backup codes */
            ret = auth_mfa_generate_backup_codes(user_id);
            
            pthread_mutex_unlock(&auth_mutex);
            
            auth_log_event(AUTH_EVENT_MFA_ENABLED, user_id, "127.0.0.1",
                          "MFA successfully enabled", true);
            
            return ret;
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    auth_log_event(AUTH_EVENT_MFA_VERIFICATION_FAILED, user_id, "127.0.0.1",
                  "MFA setup verification failed", false);
    
    return AUTH_ERROR_INVALID_CODE;
}

int auth_mfa_verify_code(uint32_t user_id, const char* totp_code) {
    if (!totp_code) {
        return AUTH_ERROR_INVALID;
    }
    
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
    
    /* Check if MFA is enabled */
    if (!account.mfa_enabled || account.mfa_secret_len == 0) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Check for replay attack (same time window) */
    uint64_t current_time = time(NULL);
    uint64_t current_window = current_time / TOTP_WINDOW_SIZE;
    uint64_t last_used_window = account.mfa_last_used_time / TOTP_WINDOW_SIZE;
    
    if (current_window == last_used_window) {
        pthread_mutex_unlock(&auth_mutex);
        auth_log_event(AUTH_EVENT_MFA_REPLAY_ATTACK, user_id, "127.0.0.1",
                      "MFA replay attack detected", false);
        return AUTH_ERROR_REPLAY_ATTACK;
    }
    
    /* Verify TOTP code */
    uint32_t provided_code = strtoul(totp_code, NULL, 10);
    
    /* Check current window and tolerance windows */
    for (int i = -TOTP_TOLERANCE; i <= TOTP_TOLERANCE; i++) {
        uint64_t test_time = current_time + (i * TOTP_WINDOW_SIZE);
        uint32_t expected_code = totp(account.mfa_secret, account.mfa_secret_len, test_time);
        
        if (provided_code == expected_code) {
            /* Update last used time */
            account.mfa_last_used_time = current_time;
            
            pthread_mutex_unlock(&auth_mutex);
            
            auth_log_event(AUTH_EVENT_MFA_SUCCESS, user_id, "127.0.0.1",
                          "MFA verification successful", true);
            
            return AUTH_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    auth_log_event(AUTH_EVENT_MFA_VERIFICATION_FAILED, user_id, "127.0.0.1",
                  "MFA verification failed", false);
    
    return AUTH_ERROR_INVALID_CODE;
}

/* ========================== Backup Codes ========================== */

int auth_mfa_generate_backup_codes(uint32_t user_id) {
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
    
    /* Generate backup codes */
    for (int i = 0; i < AUTH_MAX_BACKUP_CODES; i++) {
        uint8_t code_bytes[4];
        if (RAND_bytes(code_bytes, sizeof(code_bytes)) != 1) {
            pthread_mutex_unlock(&auth_mutex);
            return AUTH_ERROR_CRYPTO;
        }
        
        /* Convert to 8-digit code */
        uint32_t code = *((uint32_t*)code_bytes) % 100000000;
        snprintf(account.mfa_backup_codes[i], sizeof(account.mfa_backup_codes[i]),
                "%08u", code);
        account.mfa_backup_codes_used[i] = false;
    }
    
    account.mfa_backup_codes_count = AUTH_MAX_BACKUP_CODES;
    
    /* This would need to update the actual user store */
    /* For now, we'll assume success */
    
    pthread_mutex_unlock(&auth_mutex);
    
    auth_log_event(AUTH_EVENT_MFA_BACKUP_GENERATED, user_id, "127.0.0.1",
                  "MFA backup codes generated", true);
    
    return AUTH_SUCCESS;
}

int auth_mfa_verify_backup_code(uint32_t user_id, const char* backup_code) {
    if (!backup_code) {
        return AUTH_ERROR_INVALID;
    }
    
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
    
    /* Check if MFA is enabled */
    if (!account.mfa_enabled) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Find matching backup code */
    for (int i = 0; i < account.mfa_backup_codes_count; i++) {
        if (!account.mfa_backup_codes_used[i] &&
            strcmp(account.mfa_backup_codes[i], backup_code) == 0) {
            
            /* Mark code as used */
            account.mfa_backup_codes_used[i] = true;
            
            /* This would need to update the actual user store */
            
            pthread_mutex_unlock(&auth_mutex);
            
            auth_log_event(AUTH_EVENT_MFA_BACKUP_USED, user_id, "127.0.0.1",
                          "MFA backup code used", true);
            
            return AUTH_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    auth_log_event(AUTH_EVENT_MFA_VERIFICATION_FAILED, user_id, "127.0.0.1",
                  "Invalid MFA backup code", false);
    
    return AUTH_ERROR_INVALID_CODE;
}

int auth_mfa_get_backup_codes(uint32_t user_id, char backup_codes[][AUTH_MAX_BACKUP_CODE_LEN], 
                             bool* used_status, uint32_t* count) {
    if (!backup_codes || !used_status || !count) {
        return AUTH_ERROR_INVALID;
    }
    
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
    
    /* Check if MFA is enabled */
    if (!account.mfa_enabled) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Copy backup codes */
    for (int i = 0; i < account.mfa_backup_codes_count; i++) {
        strncpy(backup_codes[i], account.mfa_backup_codes[i], AUTH_MAX_BACKUP_CODE_LEN - 1);
        backup_codes[i][AUTH_MAX_BACKUP_CODE_LEN - 1] = '\0';
        used_status[i] = account.mfa_backup_codes_used[i];
    }
    
    *count = account.mfa_backup_codes_count;
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

/* ========================== MFA Management ========================== */

int auth_mfa_disable(uint32_t user_id) {
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
    
    /* Disable MFA */
    account.mfa_enabled = false;
    memset(account.mfa_secret, 0, sizeof(account.mfa_secret));
    account.mfa_secret_len = 0;
    account.mfa_backup_codes_count = 0;
    account.mfa_last_used_time = 0;
    
    /* Clear backup codes */
    for (int i = 0; i < AUTH_MAX_BACKUP_CODES; i++) {
        memset(account.mfa_backup_codes[i], 0, sizeof(account.mfa_backup_codes[i]));
        account.mfa_backup_codes_used[i] = false;
    }
    
    /* This would need to update the actual user store */
    
    pthread_mutex_unlock(&auth_mutex);
    
    auth_log_event(AUTH_EVENT_MFA_DISABLED, user_id, "127.0.0.1",
                  "MFA disabled for user", true);
    
    return AUTH_SUCCESS;
}

int auth_mfa_get_status(uint32_t user_id, mfa_status_t* status) {
    if (!status) {
        return AUTH_ERROR_INVALID;
    }
    
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
    
    /* Fill status */
    memset(status, 0, sizeof(mfa_status_t));
    status->enabled = account.mfa_enabled;
    status->secret_configured = (account.mfa_secret_len > 0);
    status->backup_codes_count = account.mfa_backup_codes_count;
    status->last_used_time = account.mfa_last_used_time;
    
    /* Count unused backup codes */
    status->backup_codes_remaining = 0;
    for (int i = 0; i < account.mfa_backup_codes_count; i++) {
        if (!account.mfa_backup_codes_used[i]) {
            status->backup_codes_remaining++;
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

/* ========================== QR Code Generation for TOTP Setup ========================== */

int auth_mfa_get_qr_url(uint32_t user_id, const char* issuer, 
                       char* qr_url, size_t url_len) {
    if (!issuer || !qr_url) {
        return AUTH_ERROR_INVALID;
    }
    
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
    
    /* Check if MFA secret exists */
    if (account.mfa_secret_len == 0) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_NOT_FOUND;
    }
    
    /* Encode secret to Base32 */
    char secret_b32[64];
    int encoded_len = base32_encode(account.mfa_secret, account.mfa_secret_len,
                                   secret_b32, sizeof(secret_b32));
    if (encoded_len < 0) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    /* Generate TOTP URL */
    int written = snprintf(qr_url, url_len,
        "otpauth://totp/%s:%s?secret=%s&issuer=%s&algorithm=SHA1&digits=%d&period=%d",
        issuer, account.username, secret_b32, issuer, TOTP_DIGITS, TOTP_WINDOW_SIZE);
    
    if (written >= (int)url_len) {
        pthread_mutex_unlock(&auth_mutex);
        return AUTH_ERROR_MEMORY;
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}

/* ========================== MFA Policy Enforcement ========================== */

int auth_mfa_check_required(uint32_t user_id, bool* required) {
    if (!required) {
        return AUTH_ERROR_INVALID;
    }
    
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
    
    /* Check if user has admin role (require MFA for admins) */
    *required = false;
    for (uint32_t i = 0; i < account.role_count; i++) {
        if (account.roles[i] == AUTH_ROLE_ADMIN) {
            *required = true;
            break;
        }
    }
    
    /* Check if user has high-privilege permissions */
    if (!*required) {
        uint32_t high_priv_perms[] = {
            AUTH_PERM_CREATE_USER, AUTH_PERM_DELETE_USER,
            AUTH_PERM_ADMIN_SYSTEM, AUTH_PERM_MODIFY_ROLES
        };
        
        for (uint32_t i = 0; i < sizeof(high_priv_perms) / sizeof(high_priv_perms[0]); i++) {
            if (authz_check_permission(user_id, high_priv_perms[i]) == AUTH_SUCCESS) {
                *required = true;
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&auth_mutex);
    
    return AUTH_SUCCESS;
}
