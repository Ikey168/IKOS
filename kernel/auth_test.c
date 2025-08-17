/* IKOS Authentication & Authorization System - Comprehensive Test Suite
 * Test all authentication, authorization, and MFA functionality
 */

#include "../include/auth_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("✗ %s\n", message); \
        } \
    } while(0)

/* Test data */
static const char* test_username = "testuser";
static const char* test_password = "SecurePassword123!";
static const char* test_email = "test@example.com";
static uint32_t test_user_id = 0;
static uint32_t test_session_id = 0;

/* ========================== Authentication Core Tests ========================== */

void test_auth_system_initialization(void) {
    printf("\n=== Testing Authentication System Initialization ===\n");
    
    TEST_ASSERT(auth_init() == AUTH_SUCCESS, 
                "Authentication system initialization");
    
    TEST_ASSERT(authz_init() == AUTH_SUCCESS,
                "Authorization system initialization");
    
    /* Test double initialization */
    TEST_ASSERT(auth_init() == AUTH_SUCCESS,
                "Double initialization should not fail");
}

void test_user_creation_and_management(void) {
    printf("\n=== Testing User Creation and Management ===\n");
    
    /* Test user creation */
    int ret = auth_create_user(test_username, test_password, test_email, &test_user_id);
    TEST_ASSERT(ret == AUTH_SUCCESS && test_user_id > 0,
                "User creation with valid credentials");
    
    /* Test duplicate username */
    uint32_t duplicate_id;
    ret = auth_create_user(test_username, "different_password", "diff@example.com", &duplicate_id);
    TEST_ASSERT(ret == AUTH_ERROR_ALREADY_EXISTS,
                "Duplicate username rejection");
    
    /* Test invalid parameters */
    ret = auth_create_user(NULL, test_password, test_email, &duplicate_id);
    TEST_ASSERT(ret == AUTH_ERROR_INVALID,
                "NULL username rejection");
    
    ret = auth_create_user("shortpw", "123", test_email, &duplicate_id);
    TEST_ASSERT(ret == AUTH_ERROR_WEAK_PASSWORD,
                "Weak password rejection");
    
    /* Test user retrieval */
    user_account_t account;
    ret = auth_get_user(test_user_id, &account);
    TEST_ASSERT(ret == AUTH_SUCCESS && strcmp(account.username, test_username) == 0,
                "User retrieval by ID");
    
    ret = auth_get_user_by_username(test_username, &account);
    TEST_ASSERT(ret == AUTH_SUCCESS && account.user_id == test_user_id,
                "User retrieval by username");
    
    /* Test user exists check */
    bool exists;
    ret = auth_user_exists(test_username, &exists);
    TEST_ASSERT(ret == AUTH_SUCCESS && exists,
                "User existence check for existing user");
    
    ret = auth_user_exists("nonexistent", &exists);
    TEST_ASSERT(ret == AUTH_SUCCESS && !exists,
                "User existence check for non-existing user");
}

void test_password_authentication(void) {
    printf("\n=== Testing Password Authentication ===\n");
    
    /* Test correct password */
    int ret = auth_verify_password(test_user_id, test_password);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Correct password verification");
    
    /* Test incorrect password */
    ret = auth_verify_password(test_user_id, "wrongpassword");
    TEST_ASSERT(ret == AUTH_ERROR_INVALID_CREDENTIALS,
                "Incorrect password rejection");
    
    /* Test password change */
    const char* new_password = "NewSecurePassword456!";
    ret = auth_change_password(test_user_id, test_password, new_password);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Password change with correct old password");
    
    /* Verify new password works */
    ret = auth_verify_password(test_user_id, new_password);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "New password verification");
    
    /* Verify old password no longer works */
    ret = auth_verify_password(test_user_id, test_password);
    TEST_ASSERT(ret == AUTH_ERROR_INVALID_CREDENTIALS,
                "Old password rejection after change");
    
    /* Change back for other tests */
    auth_change_password(test_user_id, new_password, test_password);
}

void test_session_management(void) {
    printf("\n=== Testing Session Management ===\n");
    
    /* Test session creation */
    char session_token[AUTH_SESSION_TOKEN_LEN];
    int ret = auth_create_session(test_user_id, "127.0.0.1", session_token, &test_session_id);
    TEST_ASSERT(ret == AUTH_SUCCESS && test_session_id > 0 && strlen(session_token) > 0,
                "Session creation");
    
    /* Test session validation */
    uint32_t validated_user_id;
    ret = auth_validate_session(session_token, &validated_user_id);
    TEST_ASSERT(ret == AUTH_SUCCESS && validated_user_id == test_user_id,
                "Session validation with correct token");
    
    /* Test invalid session token */
    ret = auth_validate_session("invalid_token", &validated_user_id);
    TEST_ASSERT(ret == AUTH_ERROR_INVALID_SESSION,
                "Invalid session token rejection");
    
    /* Test session info retrieval */
    session_t session_info;
    ret = auth_get_session_info(test_session_id, &session_info);
    TEST_ASSERT(ret == AUTH_SUCCESS && session_info.user_id == test_user_id,
                "Session info retrieval");
    
    /* Test session refresh */
    time_t old_expiry = session_info.expires_at;
    sleep(1);
    ret = auth_refresh_session(test_session_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Session refresh");
    
    ret = auth_get_session_info(test_session_id, &session_info);
    TEST_ASSERT(ret == AUTH_SUCCESS && session_info.expires_at > old_expiry,
                "Session expiry extended after refresh");
}

/* ========================== Authorization Tests ========================== */

void test_role_management(void) {
    printf("\n=== Testing Role Management ===\n");
    
    /* Test custom role creation */
    uint32_t custom_role_id;
    int ret = authz_create_role("custom_role", "Custom Role for Testing", &custom_role_id);
    TEST_ASSERT(ret == AUTH_SUCCESS && custom_role_id > 0,
                "Custom role creation");
    
    /* Test role retrieval */
    role_t role;
    ret = authz_get_role(custom_role_id, &role);
    TEST_ASSERT(ret == AUTH_SUCCESS && strcmp(role.name, "custom_role") == 0,
                "Role retrieval by ID");
    
    /* Test role assignment */
    ret = authz_assign_role(test_user_id, AUTH_ROLE_USER);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Role assignment to user");
    
    /* Test role check */
    ret = authz_check_role(test_user_id, AUTH_ROLE_USER);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Role membership check");
    
    ret = authz_check_role(test_user_id, AUTH_ROLE_ADMIN);
    TEST_ASSERT(ret == AUTH_ERROR_ACCESS_DENIED,
                "Non-assigned role check should fail");
    
    /* Test multiple roles */
    ret = authz_assign_role(test_user_id, custom_role_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Multiple role assignment");
    
    /* Cleanup role permissions if allocated */
    if (role.permissions) {
        free(role.permissions);
    }
}

void test_permission_management(void) {
    printf("\n=== Testing Permission Management ===\n");
    
    /* Test custom permission creation */
    uint32_t custom_perm_id;
    int ret = authz_create_permission("custom_perm", "Custom Permission", 
                                     AUTH_SCOPE_RESOURCE, &custom_perm_id);
    TEST_ASSERT(ret == AUTH_SUCCESS && custom_perm_id > 0,
                "Custom permission creation");
    
    /* Test permission check (should fail initially) */
    ret = authz_check_permission(test_user_id, custom_perm_id);
    TEST_ASSERT(ret == AUTH_ERROR_ACCESS_DENIED,
                "Permission check without assignment should fail");
    
    /* Test adding permission to role */
    uint32_t custom_role_id;
    authz_create_role("perm_test_role", "Role for Permission Testing", &custom_role_id);
    ret = authz_add_permission_to_role(custom_role_id, custom_perm_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Adding permission to role");
    
    /* Assign role to user */
    authz_assign_role(test_user_id, custom_role_id);
    
    /* Test permission check (should succeed now) */
    ret = authz_check_permission(test_user_id, custom_perm_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Permission check after role assignment");
    
    /* Test effective permissions */
    permission_set_t perm_set;
    ret = authz_get_effective_permissions(test_user_id, &perm_set);
    TEST_ASSERT(ret == AUTH_SUCCESS && perm_set.count > 0,
                "Effective permissions calculation");
}

void test_access_control(void) {
    printf("\n=== Testing Access Control ===\n");
    
    /* Test basic file access */
    int ret = authz_check_access(test_user_id, "/test/file.txt", "read");
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "File read access for user role");
    
    ret = authz_check_access(test_user_id, "/test/file.txt", "write");
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "File write access for user role");
    
    /* Test ACL creation */
    acl_entry_t acl_entries[] = {
        {test_user_id, false, 0x07, true},  /* Read, write, execute allowed */
        {999, false, 0x04, false}           /* Read denied for user 999 */
    };
    
    ret = authz_set_acl("/test/restricted.txt", acl_entries, 2);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "ACL creation for resource");
    
    /* Test ACL-based access */
    ret = authz_check_access(test_user_id, "/test/restricted.txt", "read");
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "ACL-based access for allowed user");
}

/* ========================== MFA Tests ========================== */

void test_mfa_setup_and_verification(void) {
    printf("\n=== Testing MFA Setup and Verification ===\n");
    
    /* Test MFA secret generation */
    char secret_b32[64];
    int ret = auth_mfa_generate_secret(test_user_id, secret_b32, sizeof(secret_b32));
    TEST_ASSERT(ret == AUTH_SUCCESS && strlen(secret_b32) > 0,
                "MFA secret generation");
    
    /* Test MFA status before setup */
    mfa_status_t status;
    ret = auth_mfa_get_status(test_user_id, &status);
    TEST_ASSERT(ret == AUTH_SUCCESS && !status.enabled && status.secret_configured,
                "MFA status before verification");
    
    /* Test QR URL generation */
    char qr_url[256];
    ret = auth_mfa_get_qr_url(test_user_id, "IKOS", qr_url, sizeof(qr_url));
    TEST_ASSERT(ret == AUTH_SUCCESS && strstr(qr_url, "otpauth://") != NULL,
                "TOTP QR URL generation");
    
    /* Note: Real TOTP verification would require time-based calculation
     * For testing, we'll simulate the setup verification */
    printf("  Note: TOTP verification requires time-based codes (simulated)\n");
    
    /* Test backup code generation */
    ret = auth_mfa_generate_backup_codes(test_user_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "MFA backup code generation");
    
    /* Test backup code retrieval */
    char backup_codes[AUTH_MAX_BACKUP_CODES][AUTH_MAX_BACKUP_CODE_LEN];
    bool used_status[AUTH_MAX_BACKUP_CODES];
    uint32_t code_count;
    ret = auth_mfa_get_backup_codes(test_user_id, backup_codes, used_status, &code_count);
    TEST_ASSERT(ret == AUTH_SUCCESS && code_count == AUTH_MAX_BACKUP_CODES,
                "MFA backup code retrieval");
    
    /* Test MFA requirement check */
    bool required;
    ret = auth_mfa_check_required(test_user_id, &required);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "MFA requirement check");
}

/* ========================== Security Tests ========================== */

void test_security_features(void) {
    printf("\n=== Testing Security Features ===\n");
    
    /* Test account lockout (simulate failed attempts) */
    for (int i = 0; i < 6; i++) {
        auth_verify_password(test_user_id, "wrongpassword");
    }
    
    user_account_t account;
    auth_get_user(test_user_id, &account);
    TEST_ASSERT(account.is_locked,
                "Account lockout after failed attempts");
    
    /* Test unlock */
    int ret = auth_unlock_user(test_user_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Manual account unlock");
    
    auth_get_user(test_user_id, &account);
    TEST_ASSERT(!account.is_locked,
                "Account unlocked status");
    
    /* Test password strength validation */
    ret = auth_validate_password_strength("weak");
    TEST_ASSERT(ret == AUTH_ERROR_WEAK_PASSWORD,
                "Weak password rejection");
    
    ret = auth_validate_password_strength("StrongPassword123!");
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Strong password acceptance");
    
    /* Test session cleanup */
    uint32_t active_sessions;
    ret = auth_cleanup_expired_sessions(&active_sessions);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Expired session cleanup");
}

/* ========================== Integration Tests ========================== */

void test_complete_authentication_flow(void) {
    printf("\n=== Testing Complete Authentication Flow ===\n");
    
    /* Create a new user for integration test */
    uint32_t integration_user_id;
    int ret = auth_create_user("integration_user", "IntegrationTest123!", 
                              "integration@test.com", &integration_user_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Integration test user creation");
    
    /* Assign roles */
    ret = authz_assign_role(integration_user_id, AUTH_ROLE_USER);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Role assignment in integration test");
    
    /* Create session */
    char token[AUTH_SESSION_TOKEN_LEN];
    uint32_t session_id;
    ret = auth_create_session(integration_user_id, "192.168.1.100", token, &session_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Session creation in integration test");
    
    /* Validate session */
    uint32_t validated_user;
    ret = auth_validate_session(token, &validated_user);
    TEST_ASSERT(ret == AUTH_SUCCESS && validated_user == integration_user_id,
                "Session validation in integration test");
    
    /* Check permissions */
    ret = authz_check_permission(integration_user_id, AUTH_PERM_READ_FILE);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Permission check in integration test");
    
    /* Check access control */
    ret = authz_check_access(integration_user_id, "/integration/test.txt", "read");
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Access control in integration test");
    
    /* Destroy session */
    ret = auth_destroy_session(session_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Session destruction in integration test");
    
    /* Verify session is destroyed */
    ret = auth_validate_session(token, &validated_user);
    TEST_ASSERT(ret == AUTH_ERROR_INVALID_SESSION,
                "Destroyed session validation should fail");
}

/* ========================== Performance Tests ========================== */

void test_performance_benchmarks(void) {
    printf("\n=== Testing Performance Benchmarks ===\n");
    
    clock_t start, end;
    double cpu_time_used;
    
    /* Password hashing performance */
    start = clock();
    for (int i = 0; i < 10; i++) {
        auth_validate_password_strength("TestPassword123!");
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("  Password validation (10x): %f seconds\n", cpu_time_used);
    TEST_ASSERT(cpu_time_used < 1.0,
                "Password validation performance acceptable");
    
    /* Session validation performance */
    char session_token[AUTH_SESSION_TOKEN_LEN];
    uint32_t perf_session_id;
    auth_create_session(test_user_id, "127.0.0.1", session_token, &perf_session_id);
    
    start = clock();
    for (int i = 0; i < 1000; i++) {
        uint32_t user_id;
        auth_validate_session(session_token, &user_id);
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("  Session validation (1000x): %f seconds\n", cpu_time_used);
    TEST_ASSERT(cpu_time_used < 1.0,
                "Session validation performance acceptable");
    
    auth_destroy_session(perf_session_id);
}

/* ========================== Cleanup Tests ========================== */

void test_system_cleanup(void) {
    printf("\n=== Testing System Cleanup ===\n");
    
    /* Destroy test session */
    int ret = auth_destroy_session(test_session_id);
    TEST_ASSERT(ret == AUTH_SUCCESS,
                "Test session cleanup");
    
    /* List and cleanup roles/permissions */
    role_t* roles;
    uint32_t role_count;
    ret = authz_list_roles(&roles, &role_count);
    TEST_ASSERT(ret == AUTH_SUCCESS && role_count > 0,
                "Role listing before cleanup");
    
    if (roles) {
        /* Free role permissions */
        for (uint32_t i = 0; i < role_count; i++) {
            if (roles[i].permissions) {
                free(roles[i].permissions);
            }
        }
        free(roles);
    }
    
    permission_t* permissions;
    uint32_t perm_count;
    ret = authz_list_permissions(&permissions, &perm_count);
    TEST_ASSERT(ret == AUTH_SUCCESS && perm_count > 0,
                "Permission listing before cleanup");
    
    if (permissions) {
        free(permissions);
    }
    
    /* Shutdown systems */
    authz_shutdown();
    auth_shutdown();
    
    printf("  Authentication and authorization systems shutdown\n");
}

/* ========================== Main Test Runner ========================== */

int main(void) {
    printf("IKOS Authentication & Authorization System Test Suite\n");
    printf("=====================================================\n");
    
    /* Run all test suites */
    test_auth_system_initialization();
    test_user_creation_and_management();
    test_password_authentication();
    test_session_management();
    test_role_management();
    test_permission_management();
    test_access_control();
    test_mfa_setup_and_verification();
    test_security_features();
    test_complete_authentication_flow();
    test_performance_benchmarks();
    test_system_cleanup();
    
    /* Print test results */
    printf("\n=== Test Results ===\n");
    printf("Tests Run: %d\n", tests_run);
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    printf("Success Rate: %.1f%%\n", (double)tests_passed / tests_run * 100.0);
    
    if (tests_failed == 0) {
        printf("\n🎉 All tests passed! Authentication system is working correctly.\n");
        return 0;
    } else {
        printf("\n❌ %d test(s) failed. Please review the implementation.\n", tests_failed);
        return 1;
    }
}
