/* IKOS Input Events Header
 * Input event structure definitions and utilities
 */

#ifndef INPUT_EVENTS_H
#define INPUT_EVENTS_H

#include "input.h"
#include <stdint.h>
#include <stdbool.h>

/* Event queue configuration */
#define INPUT_EVENT_QUEUE_DEFAULT_SIZE  128
#define INPUT_EVENT_QUEUE_MAX_SIZE      1024

/* Event priority levels */
typedef enum {
    INPUT_PRIORITY_LOW = 0,
    INPUT_PRIORITY_NORMAL = 1,
    INPUT_PRIORITY_HIGH = 2,
    INPUT_PRIORITY_SYSTEM = 3
} input_event_priority_t;

/* Extended input event for internal use */
typedef struct {
    input_event_t event;
    input_event_priority_t priority;
    uint32_t sequence_number;
    bool consumed;
} input_event_internal_t;

/* Event filter function */
typedef bool (*input_event_filter_t)(const input_event_t* event, void* user_data);

/* Event callback function */
typedef void (*input_event_callback_t)(const input_event_t* event, void* user_data);

/* ================================
 * Event Queue Management
 * ================================ */

/* Create event queue */
input_event_t* input_event_queue_create(size_t size);

/* Destroy event queue */
void input_event_queue_destroy(input_event_t* queue);

/* Add event to queue */
bool input_event_queue_push(input_event_t* queue, size_t queue_size, 
                            size_t* head, size_t* tail, size_t* count,
                            const input_event_t* event);

/* Get event from queue */
bool input_event_queue_pop(input_event_t* queue, size_t queue_size,
                          size_t* head, size_t* tail, size_t* count,
                          input_event_t* event);

/* Peek at next event without removing */
bool input_event_queue_peek(input_event_t* queue, size_t queue_size,
                           size_t head, size_t tail, size_t count,
                           input_event_t* event);

/* Check if queue is empty */
bool input_event_queue_is_empty(size_t count);

/* Check if queue is full */
bool input_event_queue_is_full(size_t count, size_t queue_size);

/* Get queue usage */
size_t input_event_queue_usage(size_t count, size_t queue_size);

/* ================================
 * Event Filtering and Processing
 * ================================ */

/* Filter events by type */
bool input_event_filter_by_type(const input_event_t* event, void* type_mask);

/* Filter events by device */
bool input_event_filter_by_device(const input_event_t* event, void* device_id);

/* Filter keyboard events only */
bool input_event_filter_keyboard_only(const input_event_t* event, void* user_data);

/* Filter mouse events only */
bool input_event_filter_mouse_only(const input_event_t* event, void* user_data);

/* Combine multiple filters */
bool input_event_filter_combine(const input_event_t* event, 
                               input_event_filter_t* filters, 
                               void** filter_data, 
                               size_t filter_count);

/* ================================
 * Event Transformation
 * ================================ */

/* Convert keyboard event to character */
char input_event_key_to_char(const input_event_t* event);

/* Check if event is printable character */
bool input_event_is_printable(const input_event_t* event);

/* Check if event is modifier key */
bool input_event_is_modifier(const input_event_t* event);

/* Check if event is navigation key */
bool input_event_is_navigation(const input_event_t* event);

/* Check if event is function key */
bool input_event_is_function_key(const input_event_t* event);

/* ================================
 * Event Validation
 * ================================ */

/* Validate event structure */
bool input_event_validate(const input_event_t* event);

/* Validate key event data */
bool input_event_validate_key(const input_event_t* event);

/* Validate mouse event data */
bool input_event_validate_mouse(const input_event_t* event);

/* ================================
 * Event Utilities
 * ================================ */

/* Copy event */
void input_event_copy(input_event_t* dest, const input_event_t* src);

/* Compare events */
bool input_event_equal(const input_event_t* a, const input_event_t* b);

/* Get event type name */
const char* input_event_type_name(input_event_type_t type);

/* Get device type name */
const char* input_device_type_name(input_device_type_t type);

/* Format event for debugging */
int input_event_format_debug(const input_event_t* event, char* buffer, size_t buffer_size);

/* ================================
 * Timestamp and Timing
 * ================================ */

/* Get current timestamp */
uint64_t input_get_timestamp(void);

/* Calculate event age */
uint64_t input_event_age(const input_event_t* event);

/* Check if event is stale */
bool input_event_is_stale(const input_event_t* event, uint64_t max_age_ms);

/* ================================
 * Event Batching
 * ================================ */

/* Batch similar events together */
size_t input_event_batch_similar(input_event_t* events, size_t event_count, 
                                input_event_t* batched, size_t max_batched);

/* Check if events can be batched */
bool input_event_can_batch(const input_event_t* a, const input_event_t* b);

/* Merge compatible events */
bool input_event_merge(input_event_t* dest, const input_event_t* src);

#endif /* INPUT_EVENTS_H */
