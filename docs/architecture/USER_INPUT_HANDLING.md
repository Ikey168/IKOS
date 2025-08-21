# IKOS Issue #38: User Input Handling (Mouse & Keyboard)

## Implementation Status: 🔄 IN PROGRESS

## Overview
Develop a unified input handling system for user-space applications. This system will provide event-driven input handling supporting both keyboard and mouse events, with a comprehensive API for applications to receive user input. This creates a system-wide input management service that enables both GUI and CLI applications to receive input effectively.

## Problem Statement
Currently, IKOS lacks a unified input handling system that can manage both keyboard and mouse events in a coordinated manner. Applications need a standardized way to receive input events, and the system needs to arbitrate between multiple applications that may want input simultaneously.

## Goals

### Primary Objectives
1. **Unified Input System**: Create a centralized input management service
2. **Event-Driven Architecture**: Implement asynchronous event-driven input handling
3. **Mouse Support**: Complete mouse input handling with position, buttons, and wheel
4. **Keyboard Support**: Enhanced keyboard input with modifiers, special keys, and input modes
5. **Application API**: Provide clean API for applications to register for and receive input events
6. **Input Focus Management**: Handle focus switching between applications
7. **Input Device Abstraction**: Abstract different input hardware through unified interface

### Secondary Objectives
1. **Input Buffering**: Efficient event queuing and buffering
2. **Hot-plugging**: Support for dynamic input device addition/removal
3. **Input Filtering**: Allow applications to filter specific event types
4. **Accessibility**: Support for accessibility input modes
5. **Input Recording/Playback**: For testing and automation
6. **Security**: Prevent unauthorized input interception

## Technical Architecture

### Core Components

#### 1. Input Manager (`kernel/input_manager.c`)
- Central input management service
- Device registration and management
- Event routing and distribution
- Focus management between applications
- Input device hotplug support

#### 2. Keyboard Driver Integration (`kernel/input_keyboard.c`)
- Enhanced keyboard driver integration
- Key mapping and translation
- Modifier key handling (Ctrl, Alt, Shift, etc.)
- Repeat rate and delay management
- Special key sequences (function keys, arrows, etc.)

#### 3. Mouse Driver Integration (`kernel/input_mouse.c`)
- Mouse position tracking and reporting
- Button state management (left, right, middle, additional)
- Scroll wheel support (vertical and horizontal)
- Mouse acceleration and sensitivity
- Cursor boundary management

#### 4. Event System (`kernel/input_events.c`)
- Event structure definitions
- Event queue management
- Event filtering and routing
- Priority-based event handling
- Event batching and optimization

#### 5. Application Interface (`kernel/input_api.c`)
- System call interface for applications
- Event subscription and unsubscription
- Focus request and management
- Input mode configuration
- Event polling and blocking reads

#### 6. Input Device Abstraction (`kernel/input_device.c`)
- Generic input device framework
- Device capability detection
- Hardware abstraction layer
- Device-specific driver integration
- Plug-and-play support

### Key Data Structures

#### Input Event Structure
```c
typedef enum {
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON_PRESS,
    INPUT_EVENT_MOUSE_BUTTON_RELEASE,
    INPUT_EVENT_MOUSE_WHEEL,
    INPUT_EVENT_DEVICE_CONNECT,
    INPUT_EVENT_DEVICE_DISCONNECT
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    uint64_t timestamp;
    uint32_t device_id;
    
    union {
        struct {
            uint32_t keycode;
            uint32_t modifiers;
            uint32_t unicode;
        } key;
        
        struct {
            int32_t x, y;
            int32_t delta_x, delta_y;
        } mouse_move;
        
        struct {
            uint32_t button;
            int32_t x, y;
        } mouse_button;
        
        struct {
            int32_t delta_x, delta_y;
            int32_t x, y;
        } mouse_wheel;
    } data;
} input_event_t;
```

#### Input Device Structure
```c
typedef struct input_device {
    uint32_t device_id;
    char name[64];
    input_device_type_t type;
    uint32_t capabilities;
    void* device_data;
    
    // Device operations
    int (*read_event)(struct input_device* dev, input_event_t* event);
    int (*configure)(struct input_device* dev, void* config);
    int (*reset)(struct input_device* dev);
    
    struct input_device* next;
} input_device_t;
```

#### Application Input Context
```c
typedef struct {
    pid_t pid;
    uint32_t subscription_mask;
    input_event_t* event_queue;
    size_t queue_size;
    size_t queue_head, queue_tail;
    bool has_focus;
    wait_queue_head_t wait_queue;
} input_context_t;
```

## Implementation Requirements

### Phase 1: Core Input Framework
1. **Input Manager Implementation**
   - Central input service initialization
   - Device registration framework
   - Basic event routing infrastructure
   - Simple focus management

2. **Event System Foundation**
   - Event structure definitions
   - Basic event queue implementation
   - Event routing between devices and applications
   - Memory management for events

3. **System Call Interface**
   - Input system call definitions
   - Basic application registration
   - Event polling interface
   - Focus request handling

### Phase 2: Keyboard Enhancement
1. **Enhanced Keyboard Driver**
   - Integration with existing keyboard driver
   - Advanced key mapping and translation
   - Modifier key combination handling
   - Repeat rate configuration

2. **Keyboard Event Processing**
   - Raw scancode to keycode translation
   - Unicode character generation
   - Special key sequence handling
   - Input method integration framework

### Phase 3: Mouse Integration
1. **Mouse Driver Integration**
   - PS/2 mouse support integration
   - USB mouse support (future)
   - Serial mouse support (legacy)
   - Mouse configuration and calibration

2. **Mouse Event Processing**
   - Absolute and relative positioning
   - Button state tracking
   - Scroll wheel event handling
   - Mouse acceleration algorithms

### Phase 4: Advanced Features
1. **Focus Management**
   - Window focus integration with GUI system
   - Input focus arbitration
   - Focus stealing prevention
   - Background application input filtering

2. **Input Device Hotplug**
   - Dynamic device detection
   - Runtime device addition/removal
   - Device capability negotiation
   - Hot-plug notification system

## API Specification

### System Calls

#### Input Registration
```c
// Register application for input events
int sys_input_register(uint32_t event_mask);

// Unregister from input events
int sys_input_unregister(void);

// Request input focus
int sys_input_request_focus(void);

// Release input focus
int sys_input_release_focus(void);
```

#### Event Handling
```c
// Poll for input events (non-blocking)
int sys_input_poll(input_event_t* events, size_t max_events);

// Wait for input events (blocking)
int sys_input_wait(input_event_t* events, size_t max_events, int timeout_ms);

// Get current input state
int sys_input_get_state(input_state_t* state);

// Configure input parameters
int sys_input_configure(uint32_t device_id, void* config);
```

### Kernel API

#### Device Registration
```c
// Register input device
int input_register_device(input_device_t* device);

// Unregister input device
int input_unregister_device(uint32_t device_id);

// Send event from device
int input_report_event(uint32_t device_id, input_event_t* event);
```

#### Event Management
```c
// Queue event for application
int input_queue_event(pid_t pid, input_event_t* event);

// Set application focus
int input_set_focus(pid_t pid);

// Get focused application
pid_t input_get_focus(void);
```

## Integration Points

### GUI System Integration
- **Window Manager**: Focus change notifications
- **Event Routing**: Direct integration with GUI event system
- **Cursor Management**: Mouse cursor position and rendering
- **Widget Events**: Mouse clicks, hover, keyboard input for widgets

### Terminal Integration
- **Console Input**: Keyboard input for terminal applications
- **Terminal Focus**: Focus management for multiple terminals
- **Special Keys**: Function keys, arrow keys, terminal control sequences
- **Mouse Support**: Terminal mouse reporting (if supported)

### Process Manager Integration
- **Process Focus**: Link input focus with process management
- **Process Termination**: Clean up input contexts on process exit
- **Permission Management**: Input access control per process
- **Resource Limiting**: Input event queue limits per process

### Scheduler Integration
- **Input Scheduling**: Priority handling for input events
- **Interrupt Handling**: Efficient input interrupt processing
- **Wake-up Events**: Wake processes waiting for input
- **Performance**: Low-latency input event delivery

## Performance Requirements

### Latency Targets
- **Key Press to Application**: < 5ms
- **Mouse Movement**: < 2ms for position updates
- **Event Processing**: < 1ms for event routing
- **Focus Changes**: < 10ms for focus transitions

### Throughput Requirements
- **Event Rate**: Support up to 1000 events/second
- **Multiple Applications**: Handle 20+ concurrent input contexts
- **Device Support**: Support up to 16 simultaneous input devices
- **Memory Usage**: < 2MB total for input system

## Testing Strategy

### Unit Tests
- Input event structure validation
- Event queue operations
- Device registration/unregistration
- Focus management logic
- API function testing

### Integration Tests
- Keyboard driver integration
- Mouse driver integration
- GUI system integration
- Multi-application scenarios
- Focus switching validation

### Performance Tests
- Input latency measurements
- Event throughput testing
- Memory usage validation
- CPU usage optimization
- Stress testing with high event rates

### Hardware Tests
- Physical keyboard testing
- Physical mouse testing
- Different device types
- Hot-plug scenarios
- Legacy device support

## Security Considerations

### Input Isolation
- Prevent unauthorized input interception
- Secure focus management
- Application input permission system
- Input data sanitization

### Access Control
- Per-process input permissions
- Privileged input operations
- System-level input control
- Input device access restrictions

## Implementation Plan

### Phase 1: Foundation (Week 1)
1. **Core Framework** - Input manager and event system basics
2. **System Calls** - Basic API implementation
3. **Testing Framework** - Unit test infrastructure
4. **Documentation** - API documentation and examples

### Phase 2: Keyboard Integration (Week 2)
1. **Keyboard Driver** - Enhanced driver integration
2. **Key Processing** - Advanced key mapping and translation
3. **Modifier Keys** - Complex modifier combination handling
4. **Special Keys** - Function keys and special sequences

### Phase 3: Mouse Integration (Week 3)
1. **Mouse Driver** - Mouse driver integration
2. **Position Tracking** - Absolute and relative positioning
3. **Button Handling** - Mouse button state management
4. **Wheel Support** - Scroll wheel event processing

### Phase 4: Advanced Features (Week 4)
1. **Focus Management** - Advanced focus arbitration
2. **Device Hotplug** - Dynamic device management
3. **Performance Optimization** - Latency and throughput optimization
4. **Integration Testing** - Full system validation

## Success Criteria

### Functional Requirements
- ✅ Complete keyboard input handling with all keys and modifiers
- ✅ Full mouse support including position, buttons, and wheel
- ✅ Event-driven architecture with proper queuing
- ✅ Application API for input registration and event handling
- ✅ Focus management between multiple applications
- ✅ Integration with existing GUI and terminal systems

### Performance Requirements
- ✅ Input latency under 5ms for key events
- ✅ Mouse tracking with <2ms position updates
- ✅ Support for 1000+ events per second
- ✅ Memory usage under 2MB for entire input system
- ✅ CPU overhead less than 5% during normal operation

### Integration Requirements
- ✅ Seamless GUI system integration
- ✅ Terminal emulator input support
- ✅ Process manager integration for focus handling
- ✅ Clean device driver abstraction
- ✅ Robust error handling and recovery

## Deliverables

### Core Implementation Files
- `kernel/input_manager.c` - Central input management
- `kernel/input_keyboard.c` - Keyboard input handling
- `kernel/input_mouse.c` - Mouse input handling
- `kernel/input_events.c` - Event system implementation
- `kernel/input_api.c` - System call interface
- `kernel/input_device.c` - Device abstraction layer

### Header Files
- `include/input.h` - Main input system header
- `include/input_events.h` - Event structure definitions
- `include/input_device.h` - Device interface definitions
- `include/input_syscalls.h` - System call interface

### Testing Files
- `tests/test_input.c` - Comprehensive input system tests
- `tests/test_input_integration.c` - Integration test suite
- `tests/input_test_utils.c` - Testing utilities and helpers

### Documentation
- Updated system call documentation
- Input system programming guide
- Device driver integration guide
- Performance tuning recommendations

## Dependencies

### Prerequisites
- **Issue #34**: Terminal Emulator (for console input integration)
- **Issue #37**: GUI System (for window manager integration)
- **Keyboard Driver**: Existing PS/2 keyboard support
- **Mouse Driver**: Basic mouse hardware support
- **Process Manager**: Process identification and management
- **Memory Manager**: For event queues and data structures

### Integration Dependencies
- **Window Manager**: Focus change notifications
- **Process Manager**: Process termination cleanup
- **Scheduler**: Interrupt handling and priority management
- **Virtual Memory**: Memory mapping for shared input buffers
- **IPC System**: Inter-process communication for input events

## Risk Assessment

### Technical Risks
- **Performance Impact**: Input processing affecting system performance
- **Memory Usage**: Event queues consuming excessive memory
- **Device Compatibility**: Supporting diverse input hardware
- **Focus Conflicts**: Race conditions in focus management

### Mitigation Strategies
- **Performance Monitoring**: Continuous latency and throughput measurement
- **Memory Limits**: Configurable queue sizes and garbage collection
- **Device Testing**: Comprehensive hardware compatibility testing
- **Atomic Operations**: Use of atomic operations for focus management

## Timeline

### Week 1: Foundation
- Days 1-2: Core input manager and event system
- Days 3-4: System call interface implementation
- Days 5-7: Basic testing and validation

### Week 2: Keyboard Integration
- Days 1-3: Enhanced keyboard driver integration
- Days 4-5: Advanced key processing and modifiers
- Days 6-7: Special key handling and testing

### Week 3: Mouse Integration
- Days 1-3: Mouse driver integration and position tracking
- Days 4-5: Button and wheel event handling
- Days 6-7: Mouse configuration and testing

### Week 4: Finalization
- Days 1-2: Focus management implementation
- Days 3-4: Performance optimization and device hotplug
- Days 5-7: Integration testing and documentation

## Expected Outcome

A comprehensive user input handling system that provides:

1. **Unified Input Management**: Single point of control for all input devices
2. **Event-Driven Architecture**: Efficient asynchronous input processing
3. **Complete Hardware Support**: Full keyboard and mouse functionality
4. **Application Integration**: Clean API for GUI and console applications
5. **Focus Management**: Intelligent input focus arbitration
6. **Performance Excellence**: Low-latency, high-throughput input processing
7. **System Integration**: Seamless integration with existing IKOS components

This implementation will enable rich user interaction for both graphical and text-based applications, providing the foundation for advanced user interfaces and application development on the IKOS platform.

## Files to be Created/Modified

### New Files
- `kernel/input_manager.c`
- `kernel/input_keyboard.c`
- `kernel/input_mouse.c`
- `kernel/input_events.c`
- `kernel/input_api.c`
- `kernel/input_device.c`
- `include/input.h`
- `include/input_events.h`
- `include/input_device.h`
- `include/input_syscalls.h`
- `tests/test_input.c`
- `tests/test_input_integration.c`
- `tests/input_test_utils.c`

### Modified Files
- `include/syscalls.h` (add input system calls)
- `kernel/syscalls.c` (add input system call handlers)
- `kernel/Makefile` (add input system compilation)
- `include/process.h` (add input context to process structure)
- `kernel/process_manager.c` (add input cleanup on process exit)

## Labels
- enhancement
- drivers
- gui

## Assignee
- Ikey168

## Milestone
- Milestone 4: User Interaction Layer
