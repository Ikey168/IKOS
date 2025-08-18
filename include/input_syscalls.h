/* IKOS Input System Calls Header
 * System call interface for user-space input handling
 */

#ifndef INPUT_SYSCALLS_H
#define INPUT_SYSCALLS_H

#include "input.h"
#include <stdint.h>

/* Input system call numbers (to be added to syscalls.h) */
#define SYS_INPUT_REGISTER      200
#define SYS_INPUT_UNREGISTER    201
#define SYS_INPUT_REQUEST_FOCUS 202
#define SYS_INPUT_RELEASE_FOCUS 203
#define SYS_INPUT_POLL          204
#define SYS_INPUT_WAIT          205
#define SYS_INPUT_GET_STATE     206
#define SYS_INPUT_CONFIGURE     207

/* Input configuration structures - forward declarations */
/* (Actual definitions are in input.h) */

typedef struct {
    input_device_type_t device_type;
    union {
        input_keyboard_config_t keyboard;
        input_mouse_config_t mouse;
    } config;
} input_device_config_t;

/* ================================
 * System Call Interface
 * ================================ */

/* Register application for input events
 * Parameters:
 *   subscription_mask: Bitmask of input types to subscribe to
 * Returns: 0 on success, negative error code on failure
 */
long sys_input_register(uint32_t subscription_mask);

/* Unregister from input events
 * Returns: 0 on success, negative error code on failure
 */
long sys_input_unregister(void);

/* Request input focus
 * Returns: 0 on success, negative error code on failure
 */
long sys_input_request_focus(void);

/* Release input focus
 * Returns: 0 on success, negative error code on failure
 */
long sys_input_release_focus(void);

/* Poll for input events (non-blocking)
 * Parameters:
 *   events: Buffer to store events
 *   max_events: Maximum number of events to return
 * Returns: Number of events returned, or negative error code
 */
long sys_input_poll(input_event_t* events, uint32_t max_events);

/* Wait for input events (blocking)
 * Parameters:
 *   events: Buffer to store events
 *   max_events: Maximum number of events to return
 *   timeout_ms: Timeout in milliseconds (0 = no timeout)
 * Returns: Number of events returned, or negative error code
 */
long sys_input_wait(input_event_t* events, uint32_t max_events, uint32_t timeout_ms);

/* Get current input state
 * Parameters:
 *   state: Buffer to store input state
 * Returns: 0 on success, negative error code on failure
 */
long sys_input_get_state(input_state_t* state);

/* Configure input device
 * Parameters:
 *   device_id: Device to configure
 *   config: Configuration data
 * Returns: 0 on success, negative error code on failure
 */
long sys_input_configure(uint32_t device_id, input_device_config_t* config);

/* ================================
 * Kernel System Call Handlers
 * ================================ */

/* System call handler for input_register */
long sys_input_register_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/* System call handler for input_unregister */
long sys_input_unregister_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/* System call handler for input_request_focus */
long sys_input_request_focus_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/* System call handler for input_release_focus */
long sys_input_release_focus_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/* System call handler for input_poll */
long sys_input_poll_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/* System call handler for input_wait */
long sys_input_wait_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/* System call handler for input_get_state */
long sys_input_get_state_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/* System call handler for input_configure */
long sys_input_configure_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/* ================================
 * Helper Functions
 * ================================ */

/* Validate user-space pointer */
bool is_user_address_input(const void* ptr, size_t size);

/* Copy events to user space */
int copy_events_to_user(input_event_t* user_events, const input_event_t* kernel_events, 
                       size_t count);

/* Copy events from user space */
int copy_events_from_user(input_event_t* kernel_events, const input_event_t* user_events,
                         size_t count);

/* Copy state to user space */
int copy_state_to_user(input_state_t* user_state, const input_state_t* kernel_state);

/* Copy config from user space */
int copy_config_from_user(input_device_config_t* kernel_config, 
                         const input_device_config_t* user_config);

#endif /* INPUT_SYSCALLS_H */
