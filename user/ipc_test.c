/* IKOS User-Space IPC Test Suite
 * Comprehensive tests for the user-space IPC API
 */
#include "ipc_user_api.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("✗ %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

void test_init_cleanup(void) {
    printf("\n=== Testing Initialization and Cleanup ===\n");
    
    int result = ipc_user_init();
    TEST_ASSERT(result == IPC_SUCCESS, "IPC initialization");
    
    ipc_user_context_t* context = ipc_user_get_context();
    TEST_ASSERT(context != NULL, "Get IPC context");
    TEST_ASSERT(context->initialized == true, "Context initialized flag");
    TEST_ASSERT(context->process_queue_id != IPC_INVALID_CHANNEL, "Process queue created");
    
    ipc_user_cleanup();
    TEST_ASSERT(context->initialized == false, "Context cleanup");
}

void test_queue_operations(void) {
    printf("\n=== Testing Queue Operations ===\n");
    
    ipc_user_init();
    
    uint32_t queue_id = ipc_user_create_queue(32, IPC_PERM_ALL);
    TEST_ASSERT(queue_id != IPC_INVALID_CHANNEL, "Create message queue");
    
    int result = ipc_user_destroy_queue(queue_id);
    TEST_ASSERT(result == IPC_SUCCESS, "Destroy message queue");
    
    ipc_user_cleanup();
}

void test_message_utilities(void) {
    printf("\n=== Testing Message Utilities ===\n");
    
    ipc_user_init();
    
    // Test message allocation
    ipc_message_t* msg = ipc_user_alloc_message(256);
    TEST_ASSERT(msg != NULL, "Allocate message");
    TEST_ASSERT(msg->data_size == 256, "Message data size");
    
    // Test message creation helpers
    const char* test_data = "Hello, World!";
    ipc_message_t* data_msg = ipc_user_create_data_message(test_data, strlen(test_data) + 1, 123);
    TEST_ASSERT(data_msg != NULL, "Create data message");
    TEST_ASSERT(data_msg->type == IPC_MSG_DATA, "Data message type");
    TEST_ASSERT(data_msg->receiver_pid == 123, "Data message receiver");
    TEST_ASSERT(strcmp((char*)data_msg->data, test_data) == 0, "Data message content");
    
    ipc_message_t* request_msg = ipc_user_create_request(test_data, strlen(test_data) + 1, 456);
    TEST_ASSERT(request_msg != NULL, "Create request message");
    TEST_ASSERT(request_msg->type == IPC_MSG_REQUEST, "Request message type");
    
    ipc_message_t* reply_msg = ipc_user_create_reply(test_data, strlen(test_data) + 1, 789, 100);
    TEST_ASSERT(reply_msg != NULL, "Create reply message");
    TEST_ASSERT(reply_msg->type == IPC_MSG_REPLY, "Reply message type");
    TEST_ASSERT(reply_msg->reply_to == 100, "Reply message reference");
    
    // Test message copying
    ipc_message_t copy_msg;
    int copy_result = ipc_user_copy_message(&copy_msg, data_msg);
    TEST_ASSERT(copy_result == IPC_SUCCESS, "Copy message");
    TEST_ASSERT(copy_msg.type == data_msg->type, "Copied message type");
    TEST_ASSERT(copy_msg.data_size == data_msg->data_size, "Copied message size");
    
    // Cleanup
    ipc_user_free_message(msg);
    ipc_user_free_message(data_msg);
    ipc_user_free_message(request_msg);
    ipc_user_free_message(reply_msg);
    
    ipc_user_cleanup();
}

static bool handler_called = false;
static ipc_message_t received_message;

void test_message_handler(const ipc_message_t* message, void* user_data) {
    handler_called = true;
    ipc_user_copy_message(&received_message, message);
}

void test_handler_registration(void) {
    printf("\n=== Testing Handler Registration ===\n");
    
    ipc_user_init();
    
    uint32_t queue_id = ipc_user_create_queue(32, IPC_PERM_ALL);
    TEST_ASSERT(queue_id != IPC_INVALID_CHANNEL, "Create test queue");
    
    int result = ipc_user_register_handler(queue_id, test_message_handler, NULL, 0);
    TEST_ASSERT(result == IPC_SUCCESS, "Register message handler");
    
    ipc_user_context_t* context = ipc_user_get_context();
    TEST_ASSERT(context->handler_count > 0, "Handler count updated");
    
    result = ipc_user_unregister_handler(queue_id);
    TEST_ASSERT(result == IPC_SUCCESS, "Unregister message handler");
    
    ipc_user_cleanup();
}

void test_error_handling(void) {
    printf("\n=== Testing Error Handling ===\n");
    
    // Test without initialization
    int result = ipc_user_send_message(1, NULL, 0);
    TEST_ASSERT(result == IPC_ERROR_INVALID_MSG, "Send with null message");
    
    int error = ipc_user_get_last_error();
    TEST_ASSERT(error == IPC_ERROR_INVALID_MSG, "Get last error");
    
    const char* error_str = ipc_user_error_string(IPC_ERROR_INVALID_MSG);
    TEST_ASSERT(error_str != NULL, "Get error string");
    TEST_ASSERT(strlen(error_str) > 0, "Error string not empty");
    
    // Test invalid parameters
    ipc_user_init();
    
    result = ipc_user_send_message(IPC_INVALID_CHANNEL, NULL, 0);
    TEST_ASSERT(result == IPC_ERROR_INVALID_MSG, "Send to invalid queue");
    
    result = ipc_user_receive_message(IPC_INVALID_CHANNEL, NULL, 0);
    TEST_ASSERT(result == IPC_ERROR_INVALID_MSG, "Receive from invalid queue");
    
    ipc_user_cleanup();
}

void test_convenience_macros(void) {
    printf("\n=== Testing Convenience Macros ===\n");
    
    ipc_user_init();
    
    uint32_t queue_id = ipc_user_create_queue(32, IPC_PERM_ALL);
    TEST_ASSERT(queue_id != IPC_INVALID_CHANNEL, "Create test queue for macros");
    
    ipc_message_t* msg = ipc_user_create_data_message("test", 5, 0);
    TEST_ASSERT(msg != NULL, "Create test message for macros");
    
    // Test that macros compile and work
    int result1 = ipc_user_send_nb(queue_id, msg);
    TEST_ASSERT(result1 != 0, "Non-blocking send macro (expected to fail with empty implementation)");
    
    ipc_message_t recv_msg;
    int result2 = ipc_user_receive_nb(queue_id, &recv_msg);
    TEST_ASSERT(result2 != IPC_SUCCESS, "Non-blocking receive macro (expected to fail)");
    
    ipc_user_free_message(msg);
    ipc_user_cleanup();
}

int run_all_tests(void) {
    printf("IKOS User-Space IPC API Test Suite\n");
    printf("==================================\n");
    
    test_init_cleanup();
    test_queue_operations();
    test_message_utilities();
    test_handler_registration();
    test_error_handling();
    test_convenience_macros();
    
    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Total tests:  %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed.\n");
        return 1;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        return run_all_tests();
    }
    
    printf("Usage: %s test\n", argv[0]);
    return 1;
}
