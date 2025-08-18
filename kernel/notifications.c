/*
 * IKOS Operating System - Notification System Implementation
 * Issue #42: Notifications & System Alerts
 *
 * Comprehensive notification system providing application notifications,
 * system alerts, and user messaging with GUI integration.
 */

#include "notifications.h"
#include "gui.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"
#include <stddef.h>
#include <stdlib.h>

/* ================================
 * Global State
 * ================================ */

static bool g_notification_system_initialized = false;
static notification_config_t g_config;
static notification_stats_t g_stats;
static uint32_t g_next_notification_id = 1;

/* Notification storage */
static notification_t* g_active_notifications = NULL;
static notification_t* g_notification_history = NULL;
static uint32_t g_active_count = 0;
static uint32_t g_history_count = 0;

/* GUI components */
static gui_window_t* g_notification_panel = NULL;
static gui_widget_t* g_notification_list = NULL;
static bool g_panel_visible = false;

/* Callback management */
static notification_event_callback_t g_event_callbacks[NOTIFICATION_MAX_SUBSCRIBERS];
static void* g_event_callback_data[NOTIFICATION_MAX_SUBSCRIBERS];
static system_alert_callback_t g_alert_callbacks[NOTIFICATION_MAX_SUBSCRIBERS];
static void* g_alert_callback_data[NOTIFICATION_MAX_SUBSCRIBERS];
static uint32_t g_event_callback_count = 0;
static uint32_t g_alert_callback_count = 0;

/* Simple logging macros */
#define NOTIFICATION_LOG_INFO(fmt, ...)  printf("[NOTIFICATION-INFO] " fmt "\n", ##__VA_ARGS__)
#define NOTIFICATION_LOG_ERROR(fmt, ...) printf("[NOTIFICATION-ERROR] " fmt "\n", ##__VA_ARGS__)
#define NOTIFICATION_LOG_DEBUG(fmt, ...) printf("[NOTIFICATION-DEBUG] " fmt "\n", ##__VA_ARGS__)

/* ================================
 * Internal Helper Functions
 * ================================ */

static notification_t* allocate_notification(void) {
    notification_t* notification = (notification_t*)kmalloc(sizeof(notification_t));
    if (notification) {
        memset(notification, 0, sizeof(notification_t));
        notification->id = g_next_notification_id++;
        notification->created_time = get_current_time();
        notification->state = NOTIFICATION_STATE_PENDING;
    }
    return notification;
}

static void free_notification(notification_t* notification) {
    if (!notification) return;
    
    /* Destroy GUI window if exists */
    if (notification->window) {
        gui_destroy_window(notification->window);
        notification->window = NULL;
    }
    
    kfree(notification);
}

static void add_to_active_list(notification_t* notification) {
    if (!notification) return;
    
    notification->next = g_active_notifications;
    if (g_active_notifications) {
        g_active_notifications->prev = notification;
    }
    g_active_notifications = notification;
    g_active_count++;
    
    if (g_active_count > g_stats.peak_active_count) {
        g_stats.peak_active_count = g_active_count;
    }
}

static void remove_from_active_list(notification_t* notification) {
    if (!notification) return;
    
    if (notification->prev) {
        notification->prev->next = notification->next;
    } else {
        g_active_notifications = notification->next;
    }
    
    if (notification->next) {
        notification->next->prev = notification->prev;
    }
    
    notification->next = notification->prev = NULL;
    if (g_active_count > 0) {
        g_active_count--;
    }
}

static void add_to_history(notification_t* notification) {
    if (!notification) return;
    
    notification->next = g_notification_history;
    if (g_notification_history) {
        g_notification_history->prev = notification;
    }
    g_notification_history = notification;
    g_history_count++;
    
    /* Limit history size */
    if (g_history_count > NOTIFICATION_MAX_HISTORY) {
        notification_t* oldest = g_notification_history;
        while (oldest && oldest->next) {
            oldest = oldest->next;
        }
        
        if (oldest && oldest->prev) {
            oldest->prev->next = NULL;
            free_notification(oldest);
            g_history_count--;
        }
    }
}

static void trigger_event_callbacks(notification_t* notification, 
                                  notification_state_t old_state,
                                  notification_state_t new_state) {
    for (uint32_t i = 0; i < g_event_callback_count; i++) {
        if (g_event_callbacks[i]) {
            g_event_callbacks[i](notification, old_state, new_state, g_event_callback_data[i]);
        }
    }
}

static void trigger_alert_callbacks(system_alert_type_t alert_type, const char* message) {
    for (uint32_t i = 0; i < g_alert_callback_count; i++) {
        if (g_alert_callbacks[i]) {
            g_alert_callbacks[i](alert_type, message, g_alert_callback_data[i]);
        }
    }
}

static void set_notification_state(notification_t* notification, notification_state_t new_state) {
    if (!notification || notification->state == new_state) return;
    
    notification_state_t old_state = notification->state;
    notification->state = new_state;
    
    /* Update timestamps */
    time_t current_time = get_current_time();
    switch (new_state) {
        case NOTIFICATION_STATE_VISIBLE:
            notification->shown_time = current_time;
            break;
        case NOTIFICATION_STATE_DISMISSED:
        case NOTIFICATION_STATE_EXPIRED:
        case NOTIFICATION_STATE_CLICKED:
        case NOTIFICATION_STATE_ACTIONED:
            notification->dismissed_time = current_time;
            break;
        default:
            break;
    }
    
    /* Update statistics */
    switch (new_state) {
        case NOTIFICATION_STATE_VISIBLE:
            g_stats.total_notifications_shown++;
            break;
        case NOTIFICATION_STATE_DISMISSED:
            g_stats.total_notifications_dismissed++;
            break;
        case NOTIFICATION_STATE_CLICKED:
            g_stats.total_notifications_clicked++;
            break;
        case NOTIFICATION_STATE_EXPIRED:
            g_stats.total_notifications_expired++;
            break;
        default:
            break;
    }
    
    /* Trigger callbacks */
    trigger_event_callbacks(notification, old_state, new_state);
    
    /* Handle state transitions */
    if (new_state == NOTIFICATION_STATE_DISMISSED ||
        new_state == NOTIFICATION_STATE_EXPIRED ||
        new_state == NOTIFICATION_STATE_CLICKED ||
        new_state == NOTIFICATION_STATE_ACTIONED) {
        
        /* Move to history */
        remove_from_active_list(notification);
        add_to_history(notification);
    }
}

/* ================================
 * GUI Integration
 * ================================ */

static gui_color_t get_notification_color(notification_type_t type) {
    switch (type) {
        case NOTIFICATION_TYPE_SUCCESS:  return 0xFF28A745; /* Green */
        case NOTIFICATION_TYPE_WARNING:  return 0xFFFFC107; /* Yellow */
        case NOTIFICATION_TYPE_ERROR:    return 0xFFDC3545; /* Red */
        case NOTIFICATION_TYPE_CRITICAL: return 0xFF8B0000; /* Dark Red */
        case NOTIFICATION_TYPE_SYSTEM:   return 0xFF007BFF; /* Blue */
        case NOTIFICATION_TYPE_INFO:
        case NOTIFICATION_TYPE_APPLICATION:
        default:                         return 0xFF6C757D; /* Gray */
    }
}

static gui_window_t* create_notification_window(notification_t* notification) {
    if (!notification) return NULL;
    
    /* Calculate window position */
    gui_rect_t bounds = {
        g_config.panel_position.x,
        g_config.panel_position.y + (int32_t)(g_active_count * (NOTIFICATION_ITEM_HEIGHT + NOTIFICATION_ITEM_MARGIN)),
        NOTIFICATION_PANEL_WIDTH,
        NOTIFICATION_ITEM_HEIGHT
    };
    
    /* Create window */
    gui_window_t* window = gui_create_window("", bounds, GUI_WINDOW_POPUP);
    if (!window) return NULL;
    
    /* Set window properties */
    window->resizable = false;
    window->movable = false;
    window->closable = false;
    window->minimizable = false;
    window->maximizable = false;
    
    /* Create background panel with type-specific color */
    gui_color_t bg_color = get_notification_color(notification->type);
    if (window->root_widget) {
        window->root_widget->background_color = bg_color;
    }
    
    /* Create title label */
    gui_rect_t title_bounds = {10, 5, NOTIFICATION_PANEL_WIDTH - 70, 20};
    gui_widget_t* title_label = gui_create_label(title_bounds, notification->title, window->root_widget);
    if (title_label) {
        title_label->foreground_color = GUI_COLOR_WHITE;
    }
    
    /* Create message label */
    gui_rect_t message_bounds = {10, 25, NOTIFICATION_PANEL_WIDTH - 70, 35};
    gui_widget_t* message_label = gui_create_label(message_bounds, notification->message, window->root_widget);
    if (message_label) {
        message_label->foreground_color = GUI_COLOR_WHITE;
    }
    
    /* Create app name label */
    if (strlen(notification->app_name) > 0) {
        gui_rect_t app_bounds = {10, 60, NOTIFICATION_PANEL_WIDTH - 70, 15};
        gui_widget_t* app_label = gui_create_label(app_bounds, notification->app_name, window->root_widget);
        if (app_label) {
            app_label->foreground_color = GUI_COLOR_LIGHT_GRAY;
        }
    }
    
    /* Create close button */
    gui_rect_t close_bounds = {NOTIFICATION_PANEL_WIDTH - 60, 5, 50, 20};
    gui_widget_t* close_button = gui_create_button(close_bounds, "✕", window->root_widget);
    if (close_button) {
        close_button->background_color = GUI_COLOR_DARK_GRAY;
        close_button->foreground_color = GUI_COLOR_WHITE;
    }
    
    /* Create action buttons if any */
    for (uint32_t i = 0; i < notification->action_count && i < 2; i++) {
        gui_rect_t action_bounds = {
            10 + (int32_t)(i * 80),
            NOTIFICATION_ITEM_HEIGHT - 25,
            75,
            20
        };
        
        gui_widget_t* action_button = gui_create_button(action_bounds, 
                                                       notification->actions[i].label, 
                                                       window->root_widget);
        if (action_button) {
            action_button->background_color = notification->actions[i].is_destructive ? 
                                             GUI_COLOR_RED : GUI_COLOR_BLUE;
            action_button->foreground_color = GUI_COLOR_WHITE;
        }
    }
    
    /* Show progress bar if enabled */
    if (notification->show_progress) {
        gui_rect_t progress_bounds = {10, 45, NOTIFICATION_PANEL_WIDTH - 70, 10};
        gui_widget_t* progress_bar = gui_create_widget(GUI_WIDGET_PROGRESSBAR, 
                                                      progress_bounds, window->root_widget);
        if (progress_bar) {
            gui_progressbar_set_value(progress_bar, notification->progress_value);
        }
    }
    
    return window;
}

static void update_notification_panel(void) {
    if (!g_panel_visible || !g_notification_panel) {
        return;
    }
    
    /* Update display for all active notifications */
    notification_t* notification = g_active_notifications;
    while (notification) {
        if (notification->state == NOTIFICATION_STATE_VISIBLE && !notification->window) {
            notification->window = create_notification_window(notification);
            if (notification->window) {
                gui_show_window(notification->window, true);
            }
        }
        notification = notification->next;
    }
}

/* ================================
 * Timeout Handling
 * ================================ */

static void check_notification_timeouts(void) {
    time_t current_time = get_current_time();
    notification_t* notification = g_active_notifications;
    
    while (notification) {
        notification_t* next = notification->next;
        
        if (notification->timeout_ms > 0 && 
            notification->state == NOTIFICATION_STATE_VISIBLE &&
            notification->shown_time > 0) {
            
            uint64_t elapsed_ms = (current_time - notification->shown_time) * 1000;
            if (elapsed_ms >= notification->timeout_ms) {
                set_notification_state(notification, NOTIFICATION_STATE_EXPIRED);
            }
        }
        
        notification = next;
    }
}

/* ================================
 * Event Handlers
 * ================================ */

static void notification_button_clicked(gui_event_t* event, void* user_data) {
    (void)event;
    uint32_t* notification_id = (uint32_t*)user_data;
    if (notification_id) {
        notification_dismiss(*notification_id);
    }
}

static void notification_action_clicked(gui_event_t* event, void* user_data) {
    (void)event;
    notification_action_t* action = (notification_action_t*)user_data;
    if (action && action->callback) {
        /* Find notification by checking user_data context */
        /* This is a simplified implementation */
        action->callback(0, action->name, action->user_data);
        g_stats.total_actions_performed++;
    }
}

/* ================================
 * Core API Implementation
 * ================================ */

int notification_system_init(const notification_config_t* config) {
    if (g_notification_system_initialized) {
        NOTIFICATION_LOG_INFO("Notification system already initialized");
        return NOTIFICATION_SUCCESS;
    }
    
    NOTIFICATION_LOG_INFO("Initializing notification system");
    
    /* Initialize configuration */
    if (config) {
        g_config = *config;
    } else {
        /* Set default configuration */
        memset(&g_config, 0, sizeof(notification_config_t));
        g_config.notifications_enabled = true;
        g_config.sounds_enabled = true;
        g_config.show_on_lock_screen = false;
        g_config.show_previews = true;
        g_config.group_by_app = true;
        g_config.sort_by_priority = true;
        g_config.max_visible_notifications = NOTIFICATION_MAX_VISIBLE;
        g_config.default_timeout_ms = NOTIFICATION_DEFAULT_TIMEOUT;
        g_config.panel_position = (gui_point_t){800 - NOTIFICATION_PANEL_WIDTH - 10, 10};
        g_config.auto_hide_panel = false;
        g_config.min_priority_to_show = NOTIFICATION_PRIORITY_LOW;
        g_config.min_priority_for_sound = NOTIFICATION_PRIORITY_NORMAL;
    }
    
    /* Initialize statistics */
    memset(&g_stats, 0, sizeof(notification_stats_t));
    g_stats.system_start_time = time(NULL);
    
    /* Initialize lists */
    g_active_notifications = NULL;
    g_notification_history = NULL;
    g_active_count = 0;
    g_history_count = 0;
    
    /* Initialize callbacks */
    memset(g_event_callbacks, 0, sizeof(g_event_callbacks));
    memset(g_event_callback_data, 0, sizeof(g_event_callback_data));
    memset(g_alert_callbacks, 0, sizeof(g_alert_callbacks));
    memset(g_alert_callback_data, 0, sizeof(g_alert_callback_data));
    g_event_callback_count = 0;
    g_alert_callback_count = 0;
    
    /* Create notification panel if GUI is available */
    if (gui_get_desktop()) {
        gui_rect_t panel_bounds = {
            g_config.panel_position.x,
            g_config.panel_position.y,
            NOTIFICATION_PANEL_WIDTH,
            NOTIFICATION_MAX_VISIBLE * (NOTIFICATION_ITEM_HEIGHT + NOTIFICATION_ITEM_MARGIN)
        };
        
        g_notification_panel = gui_create_window("Notifications", panel_bounds, GUI_WINDOW_POPUP);
        if (g_notification_panel) {
            g_notification_panel->visible = false;
            g_notification_panel->resizable = false;
            g_notification_panel->movable = false;
        }
    }
    
    g_notification_system_initialized = true;
    NOTIFICATION_LOG_INFO("Notification system initialized successfully");
    
    return NOTIFICATION_SUCCESS;
}

void notification_system_shutdown(void) {
    if (!g_notification_system_initialized) {
        return;
    }
    
    NOTIFICATION_LOG_INFO("Shutting down notification system");
    
    /* Dismiss all active notifications */
    notification_dismiss_all();
    
    /* Clean up history */
    notification_t* notification = g_notification_history;
    while (notification) {
        notification_t* next = notification->next;
        free_notification(notification);
        notification = next;
    }
    g_notification_history = NULL;
    g_history_count = 0;
    
    /* Destroy notification panel */
    if (g_notification_panel) {
        gui_destroy_window(g_notification_panel);
        g_notification_panel = NULL;
    }
    
    /* Clear callbacks */
    g_event_callback_count = 0;
    g_alert_callback_count = 0;
    
    g_notification_system_initialized = false;
}

uint32_t notification_send(const char* title, 
                          const char* message,
                          notification_type_t type) {
    return notification_send_advanced(title, message, "System", NULL, type, 
                                     NOTIFICATION_PRIORITY_NORMAL, 
                                     g_config.default_timeout_ms);
}

uint32_t notification_send_advanced(const char* title,
                                   const char* message,
                                   const char* app_name,
                                   const char* icon_path,
                                   notification_type_t type,
                                   notification_priority_t priority,
                                   uint32_t timeout_ms) {
    if (!g_notification_system_initialized || !title || !message) {
        return 0;
    }
    
    if (!g_config.notifications_enabled) {
        return 0;
    }
    
    if (priority < g_config.min_priority_to_show) {
        return 0;
    }
    
    /* Check if we have room for more notifications */
    if (g_active_count >= NOTIFICATION_MAX_ACTIVE) {
        NOTIFICATION_LOG_ERROR("Maximum active notifications reached");
        return 0;
    }
    
    /* Allocate notification */
    notification_t* notification = allocate_notification();
    if (!notification) {
        NOTIFICATION_LOG_ERROR("Failed to allocate notification");
        return 0;
    }
    
    /* Fill notification data */
    strncpy(notification->title, title, NOTIFICATION_MAX_TITLE_LENGTH - 1);
    strncpy(notification->message, message, NOTIFICATION_MAX_MESSAGE_LENGTH - 1);
    
    if (app_name) {
        strncpy(notification->app_name, app_name, NOTIFICATION_MAX_APP_NAME_LENGTH - 1);
    } else {
        strcpy(notification->app_name, "System");
    }
    
    if (icon_path) {
        strncpy(notification->icon_path, icon_path, NOTIFICATION_MAX_ICON_PATH_LENGTH - 1);
    }
    
    notification->type = type;
    notification->priority = priority;
    notification->timeout_ms = timeout_ms;
    notification->sender_pid = 0; /* TODO: Get current process PID */
    
    /* Add to active list */
    add_to_active_list(notification);
    
    /* Update statistics */
    g_stats.total_notifications_sent++;
    g_stats.last_notification_time = notification->created_time;
    
    /* Show notification immediately if high priority */
    if (priority >= NOTIFICATION_PRIORITY_HIGH) {
        set_notification_state(notification, NOTIFICATION_STATE_VISIBLE);
        
        /* Play sound if enabled */
        if (g_config.sounds_enabled && priority >= g_config.min_priority_for_sound) {
            notification_play_sound(type);
        }
    }
    
    /* Update display */
    update_notification_panel();
    
    NOTIFICATION_LOG_DEBUG("Sent notification ID %u: %s", notification->id, title);
    
    return notification->id;
}

uint32_t notification_send_with_actions(const char* title,
                                       const char* message,
                                       const char* app_name,
                                       notification_type_t type,
                                       const notification_action_t* actions,
                                       uint32_t action_count) {
    uint32_t notification_id = notification_send_advanced(title, message, app_name, NULL,
                                                         type, NOTIFICATION_PRIORITY_NORMAL,
                                                         NOTIFICATION_PERSIST_TIMEOUT);
    
    if (notification_id > 0 && actions && action_count > 0) {
        notification_t* notification = notification_get_by_id(notification_id);
        if (notification) {
            uint32_t count = (action_count > NOTIFICATION_MAX_ACTIONS) ? NOTIFICATION_MAX_ACTIONS : action_count;
            for (uint32_t i = 0; i < count; i++) {
                notification->actions[i] = actions[i];
            }
            notification->action_count = count;
        }
    }
    
    return notification_id;
}

int notification_dismiss(uint32_t notification_id) {
    notification_t* notification = notification_get_by_id(notification_id);
    if (!notification) {
        return NOTIFICATION_ERROR_NOT_FOUND;
    }
    
    set_notification_state(notification, NOTIFICATION_STATE_DISMISSED);
    
    NOTIFICATION_LOG_DEBUG("Dismissed notification ID %u", notification_id);
    
    return NOTIFICATION_SUCCESS;
}

int notification_dismiss_all(void) {
    notification_t* notification = g_active_notifications;
    while (notification) {
        notification_t* next = notification->next;
        set_notification_state(notification, NOTIFICATION_STATE_DISMISSED);
        notification = next;
    }
    
    NOTIFICATION_LOG_DEBUG("Dismissed all active notifications");
    
    return NOTIFICATION_SUCCESS;
}

notification_t* notification_get_by_id(uint32_t notification_id) {
    /* Search active notifications first */
    notification_t* notification = g_active_notifications;
    while (notification) {
        if (notification->id == notification_id) {
            return notification;
        }
        notification = notification->next;
    }
    
    /* Search history */
    notification = g_notification_history;
    while (notification) {
        if (notification->id == notification_id) {
            return notification;
        }
        notification = notification->next;
    }
    
    return NULL;
}

/* ================================
 * System Alert Functions
 * ================================ */

uint32_t notification_send_system_alert(system_alert_type_t alert_type,
                                       const char* title,
                                       const char* message) {
    notification_type_t notification_type;
    notification_priority_t priority;
    
    /* Map alert type to notification properties */
    switch (alert_type) {
        case SYSTEM_ALERT_LOW_BATTERY:
        case SYSTEM_ALERT_DISK_FULL:
        case SYSTEM_ALERT_NETWORK_DOWN:
            notification_type = NOTIFICATION_TYPE_WARNING;
            priority = NOTIFICATION_PRIORITY_HIGH;
            break;
            
        case SYSTEM_ALERT_LOW_MEMORY:
        case SYSTEM_ALERT_HARDWARE_ERROR:
        case SYSTEM_ALERT_SERVICE_FAILED:
            notification_type = NOTIFICATION_TYPE_ERROR;
            priority = NOTIFICATION_PRIORITY_URGENT;
            break;
            
        case SYSTEM_ALERT_SECURITY:
            notification_type = NOTIFICATION_TYPE_CRITICAL;
            priority = NOTIFICATION_PRIORITY_CRITICAL;
            break;
            
        case SYSTEM_ALERT_UPDATE_AVAILABLE:
        case SYSTEM_ALERT_MAINTENANCE:
        case SYSTEM_ALERT_CUSTOM:
        default:
            notification_type = NOTIFICATION_TYPE_SYSTEM;
            priority = NOTIFICATION_PRIORITY_NORMAL;
            break;
    }
    
    uint32_t notification_id = notification_send_advanced(title, message, "System Alert", NULL,
                                                         notification_type, priority,
                                                         NOTIFICATION_PERSIST_TIMEOUT);
    
    if (notification_id > 0) {
        g_stats.total_system_alerts++;
        trigger_alert_callbacks(alert_type, message);
        
        NOTIFICATION_LOG_INFO("System alert: %s - %s", title, message);
    }
    
    return notification_id;
}

/* ================================
 * Pre-defined System Alerts
 * ================================ */

uint32_t notification_alert_low_memory(uint64_t available_bytes, uint64_t total_bytes) {
    char message[256];
    uint32_t percentage = (uint32_t)((available_bytes * 100) / total_bytes);
    
    snprintf(message, sizeof(message), 
             "System memory is running low. Only %u%% (%lu MB) available.",
             percentage, available_bytes / (1024 * 1024));
    
    return notification_send_system_alert(SYSTEM_ALERT_LOW_MEMORY,
                                        "Low Memory Warning",
                                        message);
}

uint32_t notification_alert_low_battery(uint32_t battery_percentage) {
    char message[256];
    
    snprintf(message, sizeof(message),
             "Battery level is critically low: %u%%. Please connect charger.",
             battery_percentage);
    
    return notification_send_system_alert(SYSTEM_ALERT_LOW_BATTERY,
                                        "Low Battery",
                                        message);
}

uint32_t notification_alert_service_failed(const char* service_name, const char* error_message) {
    char message[512];
    
    snprintf(message, sizeof(message),
             "Service '%s' has failed: %s",
             service_name ? service_name : "Unknown",
             error_message ? error_message : "Unknown error");
    
    return notification_send_system_alert(SYSTEM_ALERT_SERVICE_FAILED,
                                        "Service Failure",
                                        message);
}

/* ================================
 * Utility Functions
 * ================================ */

const char* notification_type_to_string(notification_type_t type) {
    switch (type) {
        case NOTIFICATION_TYPE_INFO: return "info";
        case NOTIFICATION_TYPE_SUCCESS: return "success";
        case NOTIFICATION_TYPE_WARNING: return "warning";
        case NOTIFICATION_TYPE_ERROR: return "error";
        case NOTIFICATION_TYPE_CRITICAL: return "critical";
        case NOTIFICATION_TYPE_SYSTEM: return "system";
        case NOTIFICATION_TYPE_APPLICATION: return "application";
        default: return "unknown";
    }
}

const char* notification_priority_to_string(notification_priority_t priority) {
    switch (priority) {
        case NOTIFICATION_PRIORITY_LOW: return "low";
        case NOTIFICATION_PRIORITY_NORMAL: return "normal";
        case NOTIFICATION_PRIORITY_HIGH: return "high";
        case NOTIFICATION_PRIORITY_URGENT: return "urgent";
        case NOTIFICATION_PRIORITY_CRITICAL: return "critical";
        default: return "unknown";
    }
}

const char* notification_state_to_string(notification_state_t state) {
    switch (state) {
        case NOTIFICATION_STATE_PENDING: return "pending";
        case NOTIFICATION_STATE_VISIBLE: return "visible";
        case NOTIFICATION_STATE_DISMISSED: return "dismissed";
        case NOTIFICATION_STATE_EXPIRED: return "expired";
        case NOTIFICATION_STATE_CLICKED: return "clicked";
        case NOTIFICATION_STATE_ACTIONED: return "actioned";
        default: return "unknown";
    }
}

gui_color_t notification_type_to_color(notification_type_t type) {
    return get_notification_color(type);
}

int notification_play_sound(notification_type_t type) {
    /* TODO: Implement sound playing based on notification type */
    (void)type;
    return NOTIFICATION_SUCCESS;
}

/* ================================
 * Public API Functions
 * ================================ */

void notification_show_panel(bool show) {
    g_panel_visible = show;
    if (g_notification_panel) {
        gui_show_window(g_notification_panel, show);
    }
    
    if (show) {
        update_notification_panel();
    }
}

bool notification_is_panel_visible(void) {
    return g_panel_visible;
}

void notification_update_display(void) {
    check_notification_timeouts();
    
    if (g_panel_visible) {
        update_notification_panel();
    }
    
    /* Show pending notifications */
    notification_t* notification = g_active_notifications;
    while (notification) {
        if (notification->state == NOTIFICATION_STATE_PENDING) {
            set_notification_state(notification, NOTIFICATION_STATE_VISIBLE);
        }
        notification = notification->next;
    }
}

int notification_get_stats(notification_stats_t* stats) {
    if (!stats) {
        return NOTIFICATION_ERROR_INVALID_PARAM;
    }
    
    *stats = g_stats;
    stats->current_active_count = g_active_count;
    
    return NOTIFICATION_SUCCESS;
}

/* ================================
 * Simple time implementation
 * ================================ */

static time_t get_current_time(void) {
    /* Simple time implementation - should be replaced with real time function */
    static time_t fake_time = 1000000;
    fake_time++;
    return fake_time;
}
