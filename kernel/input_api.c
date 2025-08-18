/* IKOS Input System Call Implementation
 * System call interface for user-space input handling
 */

#include "input_syscalls.h"
#include "input.h"
#include "process.h"
#include "memory.h"
#include <string.h>

/* Helper function prototypes */
static uint32_t get_current_pid(void);
static bool validate_user_pointer(const void* ptr, size_t size);

/* ================================
 * System Call Implementations
 * ================================ */

long sys_input_register(uint32_t subscription_mask) {
    uint32_t pid = get_current_pid();
    
    if (pid == 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    return input_register_app(pid, subscription_mask);
}

long sys_input_unregister(void) {
    uint32_t pid = get_current_pid();
    
    if (pid == 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    return input_unregister_app(pid);
}

long sys_input_request_focus(void) {
    uint32_t pid = get_current_pid();
    
    if (pid == 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    return input_set_focus(pid);
}

long sys_input_release_focus(void) {
    uint32_t current_focus = input_get_focus();
    uint32_t pid = get_current_pid();
    
    /* Only allow the focused process to release focus */
    if (current_focus != pid) {
        return INPUT_ERROR_NO_FOCUS;
    }
    
    return input_set_focus(0); /* Release focus */
}

long sys_input_poll(input_event_t* events, uint32_t max_events) {
    uint32_t pid = get_current_pid();
    
    if (pid == 0 || !events || max_events == 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Validate user-space pointer */
    if (!validate_user_pointer(events, sizeof(input_event_t) * max_events)) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Allocate kernel buffer for events */
    input_event_t* kernel_events = (input_event_t*)kmalloc(sizeof(input_event_t) * max_events);
    if (!kernel_events) {
        return INPUT_ERROR_NO_MEMORY;
    }
    
    /* Poll for events */
    int result = input_poll_events(pid, kernel_events, max_events);
    
    if (result > 0) {
        /* Copy events to user space */
        if (copy_events_to_user(events, kernel_events, result) != 0) {
            kfree(kernel_events);
            return INPUT_ERROR_INVALID_PARAM;
        }
    }
    
    kfree(kernel_events);
    return result;
}

long sys_input_wait(input_event_t* events, uint32_t max_events, uint32_t timeout_ms) {
    uint32_t pid = get_current_pid();
    
    if (pid == 0 || !events || max_events == 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Validate user-space pointer */
    if (!validate_user_pointer(events, sizeof(input_event_t) * max_events)) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Allocate kernel buffer for events */
    input_event_t* kernel_events = (input_event_t*)kmalloc(sizeof(input_event_t) * max_events);
    if (!kernel_events) {
        return INPUT_ERROR_NO_MEMORY;
    }
    
    /* Wait for events */
    int result = input_wait_events(pid, kernel_events, max_events, timeout_ms);
    
    if (result > 0) {
        /* Copy events to user space */
        if (copy_events_to_user(events, kernel_events, result) != 0) {
            kfree(kernel_events);
            return INPUT_ERROR_INVALID_PARAM;
        }
    }
    
    kfree(kernel_events);
    return result;
}

long sys_input_get_state(input_state_t* state) {
    if (!state) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Validate user-space pointer */
    if (!validate_user_pointer(state, sizeof(input_state_t))) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Get kernel state */
    input_state_t kernel_state;
    int result = input_get_state(&kernel_state);
    
    if (result == INPUT_SUCCESS) {
        /* Copy state to user space */
        if (copy_state_to_user(state, &kernel_state) != 0) {
            return INPUT_ERROR_INVALID_PARAM;
        }
    }
    
    return result;
}

long sys_input_configure(uint32_t device_id, input_device_config_t* config) {
    if (!config || device_id == 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Validate user-space pointer */
    if (!validate_user_pointer(config, sizeof(input_device_config_t))) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Copy config from user space */
    input_device_config_t kernel_config;
    if (copy_config_from_user(&kernel_config, config) != 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Configure device */
    return input_configure_device(device_id, &kernel_config);
}

/* ================================
 * System Call Handlers
 * ================================ */

long sys_input_register_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; /* Unused parameters */
    
    uint32_t subscription_mask = (uint32_t)arg1;
    return sys_input_register(subscription_mask);
}

long sys_input_unregister_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; /* Unused parameters */
    
    return sys_input_unregister();
}

long sys_input_request_focus_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; /* Unused parameters */
    
    return sys_input_request_focus();
}

long sys_input_release_focus_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; /* Unused parameters */
    
    return sys_input_release_focus();
}

long sys_input_poll_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg3; (void)arg4; (void)arg5; /* Unused parameters */
    
    input_event_t* events = (input_event_t*)arg1;
    uint32_t max_events = (uint32_t)arg2;
    
    return sys_input_poll(events, max_events);
}

long sys_input_wait_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg4; (void)arg5; /* Unused parameters */
    
    input_event_t* events = (input_event_t*)arg1;
    uint32_t max_events = (uint32_t)arg2;
    uint32_t timeout_ms = (uint32_t)arg3;
    
    return sys_input_wait(events, max_events, timeout_ms);
}

long sys_input_get_state_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; /* Unused parameters */
    
    input_state_t* state = (input_state_t*)arg1;
    return sys_input_get_state(state);
}

long sys_input_configure_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg3; (void)arg4; (void)arg5; /* Unused parameters */
    
    uint32_t device_id = (uint32_t)arg1;
    input_device_config_t* config = (input_device_config_t*)arg2;
    
    return sys_input_configure(device_id, config);
}

/* ================================
 * Helper Functions
 * ================================ */

static uint32_t get_current_pid(void) {
    /* TODO: Get current process PID from scheduler/process manager */
    /* For now, return a dummy PID */
    return 1;
}

static bool validate_user_pointer(const void* ptr, size_t size) {
    /* TODO: Implement proper user-space pointer validation */
    /* This should check if the pointer is in user-space memory */
    if (!ptr || size == 0) {
        return false;
    }
    
    /* Simple range check - in real implementation this would check
       against the process's virtual memory map */
    uintptr_t addr = (uintptr_t)ptr;
    return (addr >= 0x10000000 && addr < 0x80000000); /* User space range */
}

bool is_user_address_input(const void* ptr, size_t size) {
    return validate_user_pointer(ptr, size);
}

int copy_events_to_user(input_event_t* user_events, const input_event_t* kernel_events, 
                       size_t count) {
    if (!user_events || !kernel_events || count == 0) {
        return -1;
    }
    
    /* TODO: Implement safe copy to user space */
    /* For now, use simple memcpy - real implementation would use
       copy_to_user or similar safe copy function */
    memcpy(user_events, kernel_events, sizeof(input_event_t) * count);
    
    return 0;
}

int copy_events_from_user(input_event_t* kernel_events, const input_event_t* user_events,
                         size_t count) {
    if (!kernel_events || !user_events || count == 0) {
        return -1;
    }
    
    /* TODO: Implement safe copy from user space */
    memcpy(kernel_events, user_events, sizeof(input_event_t) * count);
    
    return 0;
}

int copy_state_to_user(input_state_t* user_state, const input_state_t* kernel_state) {
    if (!user_state || !kernel_state) {
        return -1;
    }
    
    /* TODO: Implement safe copy to user space */
    memcpy(user_state, kernel_state, sizeof(input_state_t));
    
    return 0;
}

int copy_config_from_user(input_device_config_t* kernel_config, 
                         const input_device_config_t* user_config) {
    if (!kernel_config || !user_config) {
        return -1;
    }
    
    /* TODO: Implement safe copy from user space */
    memcpy(kernel_config, user_config, sizeof(input_device_config_t));
    
    return 0;
}
