/*
 * IKOS Operating System - Notification System Header
 * Issue #42: Notifications & System Alerts
 *
 * Comprehensive notification system providing application notifications,
 * system alerts, and user messaging capabilities.
 */

#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include "gui.h"
#include "process.h"
#include <stdint.h>
#include <stdbool.h>

/* Define time_t if not available */
#ifndef _TIME_T_DEFINED
typedef long time_t;
#define _TIME_T_DEFINED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ================================
 * Notification Constants
 * ================================ */

#define NOTIFICATION_MAX_TITLE_LENGTH       128
#define NOTIFICATION_MAX_MESSAGE_LENGTH     512
#define NOTIFICATION_MAX_APP_NAME_LENGTH    64
#define NOTIFICATION_MAX_ICON_PATH_LENGTH   256
#define NOTIFICATION_MAX_ACTION_NAME_LENGTH 64
#define NOTIFICATION_MAX_ACTIONS            8
#define NOTIFICATION_MAX_ACTIVE             32
#define NOTIFICATION_MAX_HISTORY            128
#define NOTIFICATION_MAX_SUBSCRIBERS        16

/* Default notification settings */
#define NOTIFICATION_DEFAULT_TIMEOUT        5000    /* 5 seconds */
#define NOTIFICATION_MIN_TIMEOUT            1000    /* 1 second */
#define NOTIFICATION_MAX_TIMEOUT            30000   /* 30 seconds */
#define NOTIFICATION_PERSIST_TIMEOUT        0       /* Never timeout */

/* Notification display settings */
#define NOTIFICATION_PANEL_WIDTH            350
#define NOTIFICATION_ITEM_HEIGHT            80
#define NOTIFICATION_ITEM_MARGIN            5
#define NOTIFICATION_PANEL_MARGIN           10
#define NOTIFICATION_MAX_VISIBLE            5

/* ================================
 * Notification Types and Priorities
 * ================================ */

typedef enum {
    NOTIFICATION_TYPE_INFO = 0,      /* General information */
    NOTIFICATION_TYPE_SUCCESS = 1,   /* Success notification */
    NOTIFICATION_TYPE_WARNING = 2,   /* Warning notification */
    NOTIFICATION_TYPE_ERROR = 3,     /* Error notification */
    NOTIFICATION_TYPE_CRITICAL = 4,  /* Critical system alert */
    NOTIFICATION_TYPE_SYSTEM = 5,    /* System notification */
    NOTIFICATION_TYPE_APPLICATION = 6 /* Application-specific */
} notification_type_t;

typedef enum {
    NOTIFICATION_PRIORITY_LOW = 0,    /* Low priority, can be delayed */
    NOTIFICATION_PRIORITY_NORMAL = 1, /* Normal priority */
    NOTIFICATION_PRIORITY_HIGH = 2,   /* High priority, show immediately */
    NOTIFICATION_PRIORITY_URGENT = 3, /* Urgent, interrupt user */
    NOTIFICATION_PRIORITY_CRITICAL = 4 /* Critical, system-level alert */
} notification_priority_t;

typedef enum {
    NOTIFICATION_STATE_PENDING = 0,   /* Waiting to be displayed */
    NOTIFICATION_STATE_VISIBLE = 1,   /* Currently displayed */
    NOTIFICATION_STATE_DISMISSED = 2, /* User dismissed */
    NOTIFICATION_STATE_EXPIRED = 3,   /* Timed out */
    NOTIFICATION_STATE_CLICKED = 4,   /* User clicked */
    NOTIFICATION_STATE_ACTIONED = 5   /* User performed action */
} notification_state_t;

/* ================================
 * Notification Actions
 * ================================ */

typedef struct notification_action {
    char name[NOTIFICATION_MAX_ACTION_NAME_LENGTH];
    char label[NOTIFICATION_MAX_ACTION_NAME_LENGTH];
    void (*callback)(uint32_t notification_id, const char* action_name, void* user_data);
    void* user_data;
    bool is_default;    /* Default action (triggered on click) */
    bool is_destructive; /* Destructive action (different styling) */
} notification_action_t;

/* ================================
 * Notification Structure
 * ================================ */

typedef struct notification {
    uint32_t id;
    char title[NOTIFICATION_MAX_TITLE_LENGTH];
    char message[NOTIFICATION_MAX_MESSAGE_LENGTH];
    char app_name[NOTIFICATION_MAX_APP_NAME_LENGTH];
    char icon_path[NOTIFICATION_MAX_ICON_PATH_LENGTH];
    
    notification_type_t type;
    notification_priority_t priority;
    notification_state_t state;
    
    time_t created_time;
    time_t shown_time;
    time_t dismissed_time;
    uint32_t timeout_ms;    /* 0 = no timeout */
    
    /* Actions */
    notification_action_t actions[NOTIFICATION_MAX_ACTIONS];
    uint32_t action_count;
    
    /* Display properties */
    bool persistent;        /* Don't auto-dismiss */
    bool show_progress;     /* Show progress bar */
    int32_t progress_value; /* Progress percentage (0-100) */
    bool sound_enabled;     /* Play notification sound */
    
    /* Tracking */
    uint32_t display_count; /* How many times shown */
    pid_t sender_pid;       /* Process that sent notification */
    
    /* Internal state */
    gui_window_t* window;   /* GUI window if displayed */
    struct notification* next;
    struct notification* prev;
} notification_t;

/* ================================
 * Notification Configuration
 * ================================ */

typedef struct notification_config {
    bool notifications_enabled;
    bool sounds_enabled;
    bool show_on_lock_screen;
    bool show_previews;
    bool group_by_app;
    bool sort_by_priority;
    uint32_t max_visible_notifications;
    uint32_t default_timeout_ms;
    gui_point_t panel_position;    /* Top-right corner position */
    bool auto_hide_panel;
    notification_priority_t min_priority_to_show;
    notification_priority_t min_priority_for_sound;
} notification_config_t;

/* ================================
 * System Alert Types
 * ================================ */

typedef enum {
    SYSTEM_ALERT_LOW_MEMORY = 0,     /* Low memory warning */
    SYSTEM_ALERT_LOW_BATTERY = 1,    /* Low battery warning */
    SYSTEM_ALERT_DISK_FULL = 2,      /* Disk space warning */
    SYSTEM_ALERT_NETWORK_DOWN = 3,   /* Network connectivity issue */
    SYSTEM_ALERT_HARDWARE_ERROR = 4, /* Hardware malfunction */
    SYSTEM_ALERT_SERVICE_FAILED = 5, /* System service failure */
    SYSTEM_ALERT_SECURITY = 6,       /* Security alert */
    SYSTEM_ALERT_UPDATE_AVAILABLE = 7, /* System update available */
    SYSTEM_ALERT_MAINTENANCE = 8,    /* Scheduled maintenance */
    SYSTEM_ALERT_CUSTOM = 9          /* Custom system alert */
} system_alert_type_t;

/* ================================
 * Statistics and Monitoring
 * ================================ */

typedef struct notification_stats {
    uint64_t total_notifications_sent;
    uint64_t total_notifications_shown;
    uint64_t total_notifications_dismissed;
    uint64_t total_notifications_clicked;
    uint64_t total_notifications_expired;
    uint64_t total_actions_performed;
    uint64_t total_system_alerts;
    uint32_t current_active_count;
    uint32_t peak_active_count;
    uint32_t registered_applications;
    time_t system_start_time;
    time_t last_notification_time;
} notification_stats_t;

/* ================================
 * Event Callbacks
 * ================================ */

typedef void (*notification_event_callback_t)(notification_t* notification, 
                                             notification_state_t old_state,
                                             notification_state_t new_state,
                                             void* user_data);

typedef void (*system_alert_callback_t)(system_alert_type_t alert_type,
                                       const char* message,
                                       void* user_data);

/* ================================
 * Core Notification API
 * ================================ */

/* System initialization */
int notification_system_init(const notification_config_t* config);
void notification_system_shutdown(void);
int notification_system_get_config(notification_config_t* config);
int notification_system_set_config(const notification_config_t* config);

/* Basic notification operations */
uint32_t notification_send(const char* title, 
                          const char* message,
                          notification_type_t type);

uint32_t notification_send_advanced(const char* title,
                                   const char* message,
                                   const char* app_name,
                                   const char* icon_path,
                                   notification_type_t type,
                                   notification_priority_t priority,
                                   uint32_t timeout_ms);

uint32_t notification_send_with_actions(const char* title,
                                       const char* message,
                                       const char* app_name,
                                       notification_type_t type,
                                       const notification_action_t* actions,
                                       uint32_t action_count);

int notification_update(uint32_t notification_id,
                       const char* title,
                       const char* message);

int notification_update_progress(uint32_t notification_id,
                                int32_t progress_value);

int notification_dismiss(uint32_t notification_id);
int notification_dismiss_all(void);
int notification_dismiss_by_app(const char* app_name);

/* Notification management */
notification_t* notification_get_by_id(uint32_t notification_id);
int notification_get_active_list(notification_t** notifications, uint32_t* count);
int notification_get_history(notification_t** notifications, uint32_t* count, uint32_t max_count);

/* System alerts */
uint32_t notification_send_system_alert(system_alert_type_t alert_type,
                                       const char* title,
                                       const char* message);

int notification_register_system_alert_callback(system_alert_callback_t callback, void* user_data);
int notification_unregister_system_alert_callback(system_alert_callback_t callback);

/* Event handling */
int notification_register_event_callback(notification_event_callback_t callback, void* user_data);
int notification_unregister_event_callback(notification_event_callback_t callback);

/* Application integration */
int notification_register_application(const char* app_name, pid_t pid);
int notification_unregister_application(const char* app_name, pid_t pid);
int notification_set_app_icon(const char* app_name, const char* icon_path);

/* Statistics and monitoring */
int notification_get_stats(notification_stats_t* stats);
void notification_reset_stats(void);

/* Display management */
void notification_show_panel(bool show);
bool notification_is_panel_visible(void);
void notification_update_display(void);
void notification_handle_gui_event(gui_event_t* event);

/* ================================
 * System Alert Helpers
 * ================================ */

/* Pre-defined system alerts */
uint32_t notification_alert_low_memory(uint64_t available_bytes, uint64_t total_bytes);
uint32_t notification_alert_low_battery(uint32_t battery_percentage);
uint32_t notification_alert_disk_full(const char* mount_point, uint64_t available_bytes);
uint32_t notification_alert_network_down(const char* interface_name);
uint32_t notification_alert_hardware_error(const char* device_name, const char* error_message);
uint32_t notification_alert_service_failed(const char* service_name, const char* error_message);
uint32_t notification_alert_security_event(const char* event_description);
uint32_t notification_alert_update_available(const char* update_description);

/* ================================
 * Utility Functions
 * ================================ */

const char* notification_type_to_string(notification_type_t type);
const char* notification_priority_to_string(notification_priority_t priority);
const char* notification_state_to_string(notification_state_t state);
const char* system_alert_type_to_string(system_alert_type_t alert_type);

/* GUI integration helpers */
gui_color_t notification_type_to_color(notification_type_t type);
const char* notification_type_to_icon(notification_type_t type);

/* Sound integration */
int notification_play_sound(notification_type_t type);
int notification_set_sound_enabled(bool enabled);

/* ================================
 * Error Codes
 * ================================ */

#define NOTIFICATION_SUCCESS                0
#define NOTIFICATION_ERROR_INVALID_PARAM    -1
#define NOTIFICATION_ERROR_NO_MEMORY        -2
#define NOTIFICATION_ERROR_NOT_FOUND        -3
#define NOTIFICATION_ERROR_PERMISSION       -4
#define NOTIFICATION_ERROR_SYSTEM_ERROR     -5
#define NOTIFICATION_ERROR_QUEUE_FULL       -6
#define NOTIFICATION_ERROR_NOT_INITIALIZED  -7
#define NOTIFICATION_ERROR_GUI_ERROR        -8
#define NOTIFICATION_ERROR_TIMEOUT          -9
#define NOTIFICATION_ERROR_DUPLICATE        -10

/* ================================
 * Test Functions
 * ================================ */

#ifdef NOTIFICATION_TESTING
void notification_run_tests(void);
void notification_test_basic_operations(void);
void notification_test_system_alerts(void);
void notification_test_gui_integration(void);
void notification_test_event_handling(void);
void notification_test_performance(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NOTIFICATIONS_H */
