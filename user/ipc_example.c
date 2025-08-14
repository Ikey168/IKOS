/* IKOS User-Space IPC Example Program
 * Demonstrates usage of the user-space IPC API
 */
#include "ipc_user_api.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Example message handler */
void handle_echo_request(const ipc_message_t* message, void* user_data) {
    printf("Echo service received: %s\n", (char*)message->data);
    
    /* Create reply message */
    ipc_message_t* reply = ipc_user_create_reply(message->data, message->data_size, 
                                                 message->sender_pid, message->msg_id);
    if (reply) {
        ipc_user_send_reply(message->sender_pid, reply);
        ipc_user_free_message(reply);
    }
}

/* Example channel handler */
void handle_broadcast_message(uint32_t channel_id, const ipc_message_t* message, void* user_data) {
    printf("Broadcast received on channel %u: %s\n", channel_id, (char*)message->data);
}

/* Echo server example */
int run_echo_server(void) {
    printf("Starting echo server...\n");
    
    /* Initialize IPC */
    if (ipc_user_init() != IPC_SUCCESS) {
        printf("Failed to initialize IPC\n");
        return -1;
    }
    
    /* Create service queue */
    uint32_t service_queue = ipc_user_create_queue(32, IPC_PERM_ALL);
    if (service_queue == IPC_INVALID_CHANNEL) {
        printf("Failed to create service queue\n");
        ipc_user_cleanup();
        return -1;
    }
    
    /* Register handler for echo requests */
    if (ipc_user_register_handler(service_queue, handle_echo_request, NULL, IPC_MSG_REQUEST) != IPC_SUCCESS) {
        printf("Failed to register handler\n");
        ipc_user_cleanup();
        return -1;
    }
    
    /* Subscribe to broadcast channel */
    if (ipc_user_subscribe_channel("system_broadcast", handle_broadcast_message, NULL) != IPC_SUCCESS) {
        printf("Failed to subscribe to broadcast channel\n");
    }
    
    printf("Echo server running (queue ID: %u)\n", service_queue);
    
    /* Main message processing loop */
    while (1) {
        ipc_user_poll_handlers();
        usleep(10000); /* 10ms delay */
    }
    
    ipc_user_cleanup();
    return 0;
}

/* Echo client example */
int run_echo_client(uint32_t server_queue_id) {
    printf("Starting echo client...\n");
    
    /* Initialize IPC */
    if (ipc_user_init() != IPC_SUCCESS) {
        printf("Failed to initialize IPC\n");
        return -1;
    }
    
    /* Create request message */
    const char* test_message = "Hello from client!";
    ipc_message_t* request = ipc_user_create_request(test_message, strlen(test_message) + 1, 0);
    if (!request) {
        printf("Failed to create request message\n");
        ipc_user_cleanup();
        return -1;
    }
    
    /* Send request and wait for reply */
    ipc_message_t reply;
    int result = ipc_user_send_request(server_queue_id, request, &reply, 5000); /* 5 second timeout */
    
    if (result == IPC_SUCCESS) {
        printf("Client received reply: %s\n", (char*)reply.data);
    } else {
        printf("Client request failed: %s\n", ipc_user_error_string(result));
    }
    
    ipc_user_free_message(request);
    ipc_user_cleanup();
    return (result == IPC_SUCCESS) ? 0 : -1;
}

/* Broadcast sender example */
int send_broadcast(void) {
    printf("Sending broadcast message...\n");
    
    /* Initialize IPC */
    if (ipc_user_init() != IPC_SUCCESS) {
        printf("Failed to initialize IPC\n");
        return -1;
    }
    
    /* Create broadcast message */
    const char* broadcast_text = "System notification: All processes please respond";
    ipc_message_t* message = ipc_user_create_data_message(broadcast_text, strlen(broadcast_text) + 1, 0);
    if (!message) {
        printf("Failed to create broadcast message\n");
        ipc_user_cleanup();
        return -1;
    }
    
    /* Send to broadcast channel */
    int result = ipc_user_send_to_channel("system_broadcast", message, IPC_FLAG_BROADCAST);
    
    if (result == IPC_SUCCESS) {
        printf("Broadcast sent successfully\n");
    } else {
        printf("Broadcast failed: %s\n", ipc_user_error_string(result));
    }
    
    ipc_user_free_message(message);
    ipc_user_cleanup();
    return (result == IPC_SUCCESS) ? 0 : -1;
}

/* Asynchronous communication example */
void handle_async_message(const ipc_message_t* message, void* user_data) {
    printf("Async message received from PID %u: %s\n", message->sender_pid, (char*)message->data);
}

int run_async_example(void) {
    printf("Starting async communication example...\n");
    
    /* Initialize IPC */
    if (ipc_user_init() != IPC_SUCCESS) {
        printf("Failed to initialize IPC\n");
        return -1;
    }
    
    /* Register default handler for async messages */
    if (ipc_user_register_default_handler(handle_async_message, NULL) != IPC_SUCCESS) {
        printf("Failed to register async handler\n");
        ipc_user_cleanup();
        return -1;
    }
    
    /* Send async message to self for testing */
    const char* async_text = "Async test message";
    ipc_message_t* message = ipc_user_create_data_message(async_text, strlen(async_text) + 1, 0);
    if (message) {
        ipc_user_send_async(0, message); /* 0 = self */
        ipc_user_free_message(message);
    }
    
    /* Poll for messages */
    for (int i = 0; i < 10; i++) {
        ipc_user_poll_handlers();
        usleep(100000); /* 100ms delay */
    }
    
    ipc_user_cleanup();
    return 0;
}

/* Main function with usage examples */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <mode>\n", argv[0]);
        printf("Modes:\n");
        printf("  server    - Run echo server\n");
        printf("  client    - Run echo client (requires server queue ID)\n");
        printf("  broadcast - Send broadcast message\n");
        printf("  async     - Run async communication example\n");
        return 1;
    }
    
    if (strcmp(argv[1], "server") == 0) {
        return run_echo_server();
    } else if (strcmp(argv[1], "client") == 0) {
        if (argc < 3) {
            printf("Client mode requires server queue ID\n");
            return 1;
        }
        uint32_t server_queue = (uint32_t)atoi(argv[2]);
        return run_echo_client(server_queue);
    } else if (strcmp(argv[1], "broadcast") == 0) {
        return send_broadcast();
    } else if (strcmp(argv[1], "async") == 0) {
        return run_async_example();
    } else {
        printf("Unknown mode: %s\n", argv[1]);
        return 1;
    }
}
