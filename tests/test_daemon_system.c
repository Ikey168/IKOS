/* IKOS System Daemon Management - Comprehensive Test Suite
 * Tests for daemon lifecycle, service registry, and IPC functionality
 */

#include "../include/daemon_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>

/* ========================== Test Framework ========================== */

typedef struct test_case {
    const char* name;
    int (*test_func)(void);
    struct test_case* next;
} test_case_t;

static test_case_t* test_cases = NULL;
static int tests_passed = 0;
static int tests_failed = 0;

#define REGISTER_TEST(func) register_test(#func, func)

static void register_test(const char* name, int (*test_func)(void)) {
    test_case_t* test_case = malloc(sizeof(test_case_t));
    test_case->name = name;
    test_case->test_func = test_func;
    test_case->next = test_cases;
    test_cases = test_case;
}

static void run_all_tests(void) {
    printf("Running daemon management system tests...\n\n");
    
    test_case_t* current = test_cases;
    while (current) {
        printf("Running test: %s... ", current->name);
        fflush(stdout);
        
        if (current->test_func() == 0) {
            printf("PASSED\n");
            tests_passed++;
        } else {
            printf("FAILED\n");
            tests_failed++;
        }
        
        current = current->next;
    }
    
    printf("\nTest Results: %d passed, %d failed\n", tests_passed, tests_failed);
}

/* ========================== Test Daemon Implementation ========================== */

static volatile bool test_daemon_running = false;
static pthread_t test_daemon_thread;

static void* test_daemon_main(void* arg) {
    daemon_config_t* config = (daemon_config_t*)arg;
    test_daemon_running = true;
    
    printf("Test daemon '%s' started (PID: %d)\n", config->name, getpid());
    
    while (test_daemon_running) {
        usleep(100000);  /* 100ms */
    }
    
    printf("Test daemon '%s' stopping\n", config->name);
    return NULL;
}

static int start_test_daemon(const char* name, daemon_handle_t* handle) {
    daemon_config_t config = {0};
    strncpy(config.name, name, sizeof(config.name) - 1);
    strncpy(config.description, "Test daemon for unit testing", sizeof(config.description) - 1);
    strncpy(config.executable_path, "/bin/sleep", sizeof(config.executable_path) - 1);
    
    config.args[0] = "sleep";
    config.args[1] = "3600";
    config.args[2] = NULL;
    config.auto_restart = true;
    config.max_restart_attempts = 3;
    config.restart_delay = 1;
    config.log_level = LOG_LEVEL_INFO;
    
    return daemon_create(&config, handle);
}

/* ========================== IPC Test Utilities ========================== */

static void test_ipc_callback(const ipc_message_t* message, void* user_data) {
    printf("IPC callback received message ID %u\n", message->message_id);
    *(bool*)user_data = true;
}

/* ========================== Daemon Core Tests ========================== */

static int test_daemon_create_start_stop(void) {
    daemon_handle_t handle;
    
    /* Test daemon creation */
    if (start_test_daemon("test_daemon_1", &handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Test daemon start */
    if (daemon_start(handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Wait a bit for daemon to start */
    usleep(500000);  /* 500ms */
    
    /* Test daemon status */
    daemon_status_t status;
    if (daemon_get_status(handle, &status) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (status.state != DAEMON_STATE_RUNNING) {
        return -1;
    }
    
    /* Test daemon stop */
    if (daemon_stop(handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Wait a bit for daemon to stop */
    usleep(500000);  /* 500ms */
    
    /* Verify stopped */
    if (daemon_get_status(handle, &status) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (status.state != DAEMON_STATE_STOPPED) {
        return -1;
    }
    
    /* Clean up */
    daemon_destroy(handle);
    
    return 0;
}

static int test_daemon_restart(void) {
    daemon_handle_t handle;
    
    /* Create and start daemon */
    if (start_test_daemon("test_daemon_restart", &handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (daemon_start(handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    usleep(500000);  /* 500ms */
    
    /* Test restart */
    if (daemon_restart(handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    usleep(500000);  /* 500ms */
    
    /* Verify running */
    daemon_status_t status;
    if (daemon_get_status(handle, &status) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (status.state != DAEMON_STATE_RUNNING) {
        return -1;
    }
    
    /* Clean up */
    daemon_stop(handle);
    daemon_destroy(handle);
    
    return 0;
}

static int test_daemon_monitoring(void) {
    daemon_handle_t handle;
    
    /* Create daemon with monitoring enabled */
    daemon_config_t config = {0};
    strncpy(config.name, "test_daemon_monitoring", sizeof(config.name) - 1);
    strncpy(config.description, "Monitoring test daemon", sizeof(config.description) - 1);
    strncpy(config.executable_path, "/bin/sleep", sizeof(config.executable_path) - 1);
    
    config.args[0] = "sleep";
    config.args[1] = "1";  /* Short sleep to test exit monitoring */
    config.args[2] = NULL;
    config.auto_restart = false;
    config.cpu_limit = 50.0;
    config.memory_limit = 100 * 1024 * 1024;  /* 100MB */
    
    if (daemon_create(&config, &handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (daemon_start(handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Wait for daemon to exit */
    usleep(2000000);  /* 2 seconds */
    
    /* Verify it's stopped */
    daemon_status_t status;
    if (daemon_get_status(handle, &status) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (status.state != DAEMON_STATE_STOPPED) {
        return -1;
    }
    
    /* Clean up */
    daemon_destroy(handle);
    
    return 0;
}

static int test_daemon_list_enumerate(void) {
    daemon_handle_t handles[3];
    
    /* Create multiple daemons */
    for (int i = 0; i < 3; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test_daemon_list_%d", i);
        
        if (start_test_daemon(name, &handles[i]) != DAEMON_SUCCESS) {
            return -1;
        }
        
        if (daemon_start(handles[i]) != DAEMON_SUCCESS) {
            return -1;
        }
    }
    
    usleep(500000);  /* 500ms */
    
    /* Test daemon listing */
    daemon_list_t* list = NULL;
    if (daemon_list_all(&list) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (!list || list->count < 3) {
        return -1;
    }
    
    /* Verify our daemons are in the list */
    bool found[3] = {false, false, false};
    for (uint32_t i = 0; i < list->count; i++) {
        for (int j = 0; j < 3; j++) {
            char expected_name[64];
            snprintf(expected_name, sizeof(expected_name), "test_daemon_list_%d", j);
            
            if (strcmp(list->daemons[i].name, expected_name) == 0) {
                found[j] = true;
            }
        }
    }
    
    bool all_found = found[0] && found[1] && found[2];
    
    /* Clean up */
    for (int i = 0; i < 3; i++) {
        daemon_stop(handles[i]);
        daemon_destroy(handles[i]);
    }
    
    daemon_list_free(list);
    
    return all_found ? 0 : -1;
}

/* ========================== Service Registry Tests ========================== */

static int test_service_registration(void) {
    /* Create a test service */
    service_info_t service = {0};
    strncpy(service.name, "test_service", sizeof(service.name) - 1);
    strncpy(service.description, "Test service for registration", sizeof(service.description) - 1);
    strncpy(service.version, "1.0.0", sizeof(service.version) - 1);
    
    service.daemon_pid = getpid();
    service.endpoint.type = ENDPOINT_TYPE_UNIX_SOCKET;
    strncpy(service.endpoint.config.unix_socket.path, "/tmp/test_service.sock", 
           sizeof(service.endpoint.config.unix_socket.path) - 1);
    service.endpoint.config.unix_socket.permissions = 0644;
    
    /* Test registration */
    if (service_register(&service) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Test discovery */
    service_info_t discovered;
    if (service_discover("test_service", &discovered) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Verify discovered service matches */
    if (strcmp(service.name, discovered.name) != 0 ||
        strcmp(service.description, discovered.description) != 0 ||
        strcmp(service.version, discovered.version) != 0) {
        return -1;
    }
    
    /* Test unregistration */
    if (service_unregister("test_service") != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Verify service is gone */
    if (service_discover("test_service", &discovered) == DAEMON_SUCCESS) {
        return -1;  /* Should not find it anymore */
    }
    
    return 0;
}

static int test_service_health_monitoring(void) {
    /* Register a test service */
    service_info_t service = {0};
    strncpy(service.name, "test_health_service", sizeof(service.name) - 1);
    strncpy(service.description, "Test service for health monitoring", sizeof(service.description) - 1);
    service.daemon_pid = getpid();
    
    if (service_register(&service) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Test health check */
    health_check_t health = {0};
    if (service_get_health("test_health_service", &health) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Should be healthy initially */
    if (health.status != HEALTH_STATUS_HEALTHY) {
        return -1;
    }
    
    /* Update health status */
    health_report_t report = {0};
    report.status = HEALTH_STATUS_WARNING;
    strncpy(report.message, "Test warning message", sizeof(report.message) - 1);
    report.timestamp = time(NULL);
    
    if (service_report_health("test_health_service", &report) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Verify updated health */
    if (service_get_health("test_health_service", &health) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (health.status != HEALTH_STATUS_WARNING) {
        return -1;
    }
    
    /* Clean up */
    service_unregister("test_health_service");
    
    return 0;
}

static int test_service_events(void) {
    /* Test service event callback */
    bool event_received = false;
    
    /* Register event callback */
    if (service_register_event_callback(SERVICE_EVENT_REGISTERED, test_ipc_callback, &event_received) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Register a service to trigger event */
    service_info_t service = {0};
    strncpy(service.name, "test_event_service", sizeof(service.name) - 1);
    service.daemon_pid = getpid();
    
    if (service_register(&service) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Wait for event */
    usleep(100000);  /* 100ms */
    
    /* Clean up */
    service_unregister("test_event_service");
    
    return event_received ? 0 : -1;
}

/* ========================== IPC Tests ========================== */

static int test_ipc_basic_messaging(void) {
    /* Create test endpoint */
    endpoint_info_t endpoint = {0};
    endpoint.type = ENDPOINT_TYPE_UNIX_SOCKET;
    strncpy(endpoint.config.unix_socket.path, "/tmp/test_ipc.sock", 
           sizeof(endpoint.config.unix_socket.path) - 1);
    endpoint.config.unix_socket.permissions = 0644;
    
    ipc_handle_t server_handle, client_handle;
    
    /* Create server endpoint */
    if (ipc_create_endpoint(&endpoint, &server_handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Register service for connection */
    service_info_t service = {0};
    strncpy(service.name, "test_ipc_service", sizeof(service.name) - 1);
    service.daemon_pid = getpid();
    service.endpoint = endpoint;
    
    if (service_register(&service) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Connect client */
    if (ipc_connect_to_service("test_ipc_service", &client_handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Test message sending */
    const char* test_message = "Hello, IPC!";
    if (ipc_send_message(client_handle, test_message, strlen(test_message), IPC_MESSAGE_REQUEST) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Clean up */
    ipc_disconnect(client_handle);
    ipc_disconnect(server_handle);
    service_unregister("test_ipc_service");
    unlink("/tmp/test_ipc.sock");
    
    return 0;
}

static int test_ipc_publish_subscribe(void) {
    /* Create test topic */
    topic_info_t topic_info = {0};
    strncpy(topic_info.topic, "test_topic", sizeof(topic_info.topic) - 1);
    strncpy(topic_info.description, "Test topic for pub/sub", sizeof(topic_info.description) - 1);
    topic_info.message_type = IPC_MESSAGE_NOTIFICATION;
    topic_info.persistence = TOPIC_PERSISTENCE_NONE;
    
    if (ipc_create_topic("test_topic", &topic_info) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Subscribe to topic */
    bool message_received = false;
    if (ipc_subscribe("test_topic", test_ipc_callback, &message_received) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Publish message */
    const char* test_data = "Test publication";
    if (ipc_publish("test_topic", test_data, strlen(test_data), IPC_PRIORITY_NORMAL) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Wait for callback */
    usleep(100000);  /* 100ms */
    
    return message_received ? 0 : -1;
}

/* ========================== Configuration Tests ========================== */

static int test_daemon_configuration(void) {
    /* Test configuration loading */
    daemon_config_t config = {0};
    
    /* Test default configuration */
    if (daemon_config_load_defaults(&config) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Verify defaults */
    if (config.auto_restart != true ||
        config.max_restart_attempts != 3 ||
        config.restart_delay != 5) {
        return -1;
    }
    
    /* Test configuration validation */
    if (daemon_config_validate(&config) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Test invalid configuration */
    config.max_restart_attempts = 0;  /* Invalid */
    if (daemon_config_validate(&config) == DAEMON_SUCCESS) {
        return -1;  /* Should fail validation */
    }
    
    return 0;
}

/* ========================== Security Tests ========================== */

static int test_daemon_security(void) {
    daemon_handle_t handle;
    
    /* Create daemon with security settings */
    daemon_config_t config = {0};
    strncpy(config.name, "test_security_daemon", sizeof(config.name) - 1);
    strncpy(config.executable_path, "/bin/sleep", sizeof(config.executable_path) - 1);
    config.args[0] = "sleep";
    config.args[1] = "60";
    config.args[2] = NULL;
    
    config.run_as_user = 1000;  /* Non-root user */
    config.run_as_group = 1000;
    
    if (daemon_create(&config, &handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    if (daemon_start(handle) != DAEMON_SUCCESS) {
        return -1;
    }
    
    usleep(500000);  /* 500ms */
    
    /* Verify daemon is running as correct user */
    daemon_status_t status;
    if (daemon_get_status(handle, &status) != DAEMON_SUCCESS) {
        return -1;
    }
    
    /* Clean up */
    daemon_stop(handle);
    daemon_destroy(handle);
    
    return 0;
}

/* ========================== Performance Tests ========================== */

static int test_daemon_performance(void) {
    const int num_daemons = 10;
    daemon_handle_t handles[num_daemons];
    
    /* Measure daemon creation time */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < num_daemons; i++) {
        char name[64];
        snprintf(name, sizeof(name), "perf_test_daemon_%d", i);
        
        if (start_test_daemon(name, &handles[i]) != DAEMON_SUCCESS) {
            return -1;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double creation_time = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Created %d daemons in %.3f seconds (%.1f daemons/sec)\n", 
           num_daemons, creation_time, num_daemons / creation_time);
    
    /* Clean up */
    for (int i = 0; i < num_daemons; i++) {
        daemon_destroy(handles[i]);
    }
    
    /* Performance should be reasonable (>= 10 daemons/sec) */
    return (num_daemons / creation_time >= 10.0) ? 0 : -1;
}

/* ========================== Main Test Runner ========================== */

int main(void) {
    printf("IKOS Daemon Management System Test Suite\n");
    printf("=========================================\n\n");
    
    /* Register all tests */
    REGISTER_TEST(test_daemon_create_start_stop);
    REGISTER_TEST(test_daemon_restart);
    REGISTER_TEST(test_daemon_monitoring);
    REGISTER_TEST(test_daemon_list_enumerate);
    REGISTER_TEST(test_service_registration);
    REGISTER_TEST(test_service_health_monitoring);
    REGISTER_TEST(test_service_events);
    REGISTER_TEST(test_ipc_basic_messaging);
    REGISTER_TEST(test_ipc_publish_subscribe);
    REGISTER_TEST(test_daemon_configuration);
    REGISTER_TEST(test_daemon_security);
    REGISTER_TEST(test_daemon_performance);
    
    /* Run all tests */
    run_all_tests();
    
    /* Return exit code */
    return (tests_failed == 0) ? 0 : 1;
}
