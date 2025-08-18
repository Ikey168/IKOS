/* IKOS Notification System Test Suite - Issue #42 */
#include "notifications.h"
#include <stdio.h>

void notification_test_basic(void) {
    notification_config_t config = {0};
    config.notifications_enabled = true;
    config.sounds_enabled = false;
    config.max_visible_notifications = 3;
    config.default_timeout_ms = 2000;
    config.panel_position.x = 10;
    config.panel_position.y = 10;
    notification_system_init(&config);

    uint32_t id1 = notification_send("Test Info", "This is an info notification.", NOTIFICATION_TYPE_INFO);
    uint32_t id2 = notification_send("Test Success", "Operation completed successfully.", NOTIFICATION_TYPE_SUCCESS);
    uint32_t id3 = notification_send("Test Warning", "This is a warning.", NOTIFICATION_TYPE_WARNING);
    uint32_t id4 = notification_send("Test Error", "An error occurred.", NOTIFICATION_TYPE_ERROR);
    uint32_t id5 = notification_send_system_alert(SYSTEM_ALERT_LOW_BATTERY, "Low Battery", "Battery is below 10%.");

    printf("Notifications sent: %u %u %u %u %u\n", id1, id2, id3, id4, id5);
    notification_show_panel(true);
    notification_update_display();
}

#ifdef NOTIFICATION_TESTING
int main(void) {
    notification_test_basic();
    return 0;
}
#endif
