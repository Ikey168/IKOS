/* IKOS Kernel Main Entry Point
 * Initializes core kernel subsystems including interrupt handling
 */

#include "idt.h"
#include "interrupts.h"
#include "../include/kalloc.h"
#include "../include/syscalls.h"
#include "../include/device_manager.h"
#include "../include/pci.h"
#include "../include/ide_driver.h"
#include "../include/device_driver_test.h"
#include "../include/framebuffer.h"
#include "../include/framebuffer_test.h"
#include "../include/process.h"
#include "../include/user_app_loader.h"
#include "../include/file_explorer.h"
#include "../include/notifications.h"
#include "../include/terminal_gui.h"
#include "../include/network_driver.h"
/* #include "../include/socket_syscalls.h" */ /* Commenting out due to type conflicts */
/* #include "../include/thread_syscalls.h" */ /* Commenting out due to pthread type conflicts */
#include "../include/net/dns.h"
#include "../include/net/tls.h"
/* #include "../include/ext2.h" */ /* Commenting out due to conflicting ext2_alloc_inode definitions */
/* #include "../include/ext2_syscalls.h" */ /* Commenting out due to header conflicts */
#include <stdint.h>

/* Function declarations */
void kernel_init(void);
void kernel_loop(void);
void show_help(void);
void show_statistics(void);
void show_timer_info(void);
void show_device_info(void);
void show_framebuffer_info(void);
/* Temporarily disabled due to header conflicts
void show_app_loader_info(void);
*/
void show_notification_info(void);
void show_terminal_gui_info(void);
void show_network_info(void);
/* Temporarily disabled due to header conflicts
void show_socket_info(void);
void show_threading_info(void);
*/
void kernel_print(const char* format, ...);
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void reboot_system(void);

/* External function declarations */
extern void test_app_loader_basic(void);
extern void file_explorer_test_basic_operations(void);
extern void file_explorer_run_tests(void);
extern void notification_test_basic(void);
extern void terminal_gui_run_tests(void);
extern void terminal_gui_test_basic_integration(void);
extern void network_driver_run_tests(void);
extern void network_driver_test_basic_integration(void);

/* Kernel entry point called from bootloader */
void kernel_main(void) {
    /* Initialize kernel subsystems */
    kernel_init();
    
    /* Enable interrupts and start normal operation */
    enable_interrupts();
    
    /* Main kernel loop */
    kernel_loop();
}

/**
 * Initialize all kernel subsystems
 */
void kernel_init(void) {
    /* Initialize memory management */
    memory_init();
    
    /* Initialize interrupt handling */
    idt_init();
    
    /* Enable timer interrupts for scheduling */
    pic_clear_mask(IRQ_TIMER);
    
    /* Enable keyboard interrupts for input */
    pic_clear_mask(IRQ_KEYBOARD);
    
    /* Initialize Device Driver Framework (Issue #15) */
    kernel_print("Initializing Device Driver Framework...\n");
    device_manager_init();
    pci_init();
    ide_driver_init();
    
    /* Run Device Driver Framework tests */
    kernel_print("Running Device Driver Framework tests...\n");
    test_device_driver_framework();
    
    /* Initialize Framebuffer Driver (Issue #26) */
    kernel_print("Initializing Framebuffer Driver...\n");
    fb_init();
    
    /* Run Framebuffer Driver tests */
    kernel_print("Running Framebuffer Driver tests...\n");
    test_framebuffer_driver();
    
    /* TODO: Initialize other subsystems */
    /* scheduler_init(); */
    
    /* Initialize process management and user-space execution - Issue #17 */
    kernel_print("Initializing process management and user-space execution...\n");
    process_init();
    user_app_loader_init();
    
    /* Initialize File Explorer - Issue #41 */
    kernel_print("Initializing File Explorer...\n");
    if (file_explorer_init(NULL) == FILE_EXPLORER_SUCCESS) {
        kernel_print("File Explorer initialized successfully\n");
        
        /* Register File Explorer as application */
        /* Temporarily disabled due to header conflicts
        if (file_explorer_register_application() == APP_ERROR_SUCCESS) {
            kernel_print("File Explorer registered as application\n");
        }
        */
    } else {
        kernel_print("Failed to initialize File Explorer\n");
    }
    
    /* Initialize Notification System - Issue #42 */
    kernel_print("Initializing Notification System...\n");
    if (notification_system_init(NULL) == NOTIFICATION_SUCCESS) {
        kernel_print("Notification System initialized successfully\n");
    } else {
        kernel_print("Failed to initialize Notification System\n");
    }
    
    /* Initialize Terminal GUI Integration - Issue #43 */
    kernel_print("Initializing Terminal GUI Integration...\n");
    if (terminal_gui_init() == TERMINAL_GUI_SUCCESS) {
        kernel_print("Terminal GUI Integration initialized successfully\n");
    } else {
        kernel_print("Failed to initialize Terminal GUI Integration\n");
    }
    
    /* Initialize Network Interface Driver - Issue #45 */
    kernel_print("Initializing Network Interface Driver...\n");
    if (network_driver_init() == NETWORK_SUCCESS) {
        kernel_print("Network Interface Driver initialized successfully\n");
        
        /* Initialize hardware-specific drivers */
        ethernet_driver_init();
        wifi_driver_init();
        
        /* Detect available network interfaces */
        int eth_count = ethernet_detect_interfaces();
        int wifi_count = wifi_detect_interfaces();
        
        kernel_print("Detected %d Ethernet and %d Wi-Fi interfaces\n", eth_count, wifi_count);
    } else {
        kernel_print("Failed to initialize Network Interface Driver\n");
    }
    
    /* Initialize Socket API - Issue #46 */
    /* Temporarily disabled due to header conflicts
    kernel_print("Initializing Socket API...\n");
    if (socket_syscalls_init() == SOCK_SUCCESS) {
        kernel_print("Socket API initialized successfully\n");
    } else {
        kernel_print("Failed to initialize Socket API\n");
    }
    */
    
    /* Initialize Threading System - Issue #52 */
    /* Temporarily disabled due to header conflicts  
    kernel_print("Initializing Threading System...\n");
    if (thread_system_init() == THREAD_SUCCESS) {
        kernel_print("Threading System initialized successfully\n");
    } else {
        kernel_print("Failed to initialize Threading System\n");
    }
    */
    
    /* Initialize DNS Resolution Service - Issue #47 */
    /* Temporarily disabled due to header conflicts
    kernel_print("Initializing DNS Resolution Service...\n");
    if (dns_kernel_init() == DNS_SUCCESS) {
        kernel_print("DNS Resolution Service initialized successfully\n");
    } else {
        kernel_print("Failed to initialize DNS Resolution Service\n");
    }
    */
    
    /* Initialize TLS/SSL Secure Communication - Issue #48 */
    kernel_print("Initializing TLS/SSL Secure Communication...\n");
    if (tls_init() == TLS_SUCCESS) {
        kernel_print("TLS/SSL Secure Communication initialized successfully\n");
    } else {
        kernel_print("Failed to initialize TLS/SSL Secure Communication\n");
    }
    
    /* Initialize ext2/ext4 Filesystem Support - Issue #49 */
    /* Temporarily disabled due to header conflicts
    kernel_print("Initializing ext2/ext4 Filesystem Support...\n");
    if (ext2_init() == EXT2_SUCCESS) {
        kernel_print("ext2/ext4 Filesystem Support initialized successfully\n");
    } else {
        kernel_print("Failed to initialize ext2/ext4 Filesystem Support\n");
    }
    */
    
    /* vfs_init(); */
    
    kernel_print("IKOS kernel initialized successfully\n");
    
    /* Start init process - Issue #17 */
    kernel_print("Starting init process...\n");
    int32_t init_pid = start_init_process();
    if (init_pid > 0) {
        kernel_print("Init process started successfully (PID %d)\n", init_pid);
    } else {
        kernel_print("Failed to start init process (error %d)\n", init_pid);
    }
}

/**
 * Main kernel execution loop
 */
void kernel_loop(void) {
    kernel_print("IKOS kernel started\n");
    kernel_print("Interrupt handling system active\n");
    
    uint64_t last_ticks = 0;
    char c;
    
    while (1) {
        /* Handle keyboard input */
        if (keyboard_has_data()) {
            c = keyboard_getchar();
            if (c != 0) {
                kernel_print("Key pressed: '%c' (0x%02X)\n", c, c);
                
                /* Simple command processing */
                switch (c) {
                    case 'h':
                        show_help();
                        break;
                    case 's':
                        show_statistics();
                        break;
                    case 'i':
                        show_timer_info();
                        break;
                    case 'd':
                        show_device_info();
                        break;
                    case 'f':
                        show_framebuffer_info();
                        break;
                    /* Temporarily disabled due to header conflicts
                    case 'a':
                        show_app_loader_info();
                        break;
                    */
                    case 'l':
                        test_app_loader_basic();
                        break;
                    case 'e':
                        file_explorer_test_basic_operations();
                        break;
                    case 'g':  // Changed from 'f' to avoid duplicate
                        file_explorer_run_tests();
                        break;
                    case 'o':
                        file_explorer_launch_instance("/");
                        break;
                    case 'n':
                        show_notification_info();
                        break;
                    case 'm':
                        notification_test_basic();
                        break;
                    case 't':
                        show_terminal_gui_info();
                        break;
                    case 'y':  // Changed from 'h' to avoid duplicate
                        terminal_gui_test_basic_integration();
                        break;
                    case 'u':
                        terminal_gui_run_tests();
                        break;
                    case 'w':
                        show_network_info();
                        break;
                    case 'q':
                        network_driver_test_basic_integration();
                        break;
                    case 'z':
                        network_driver_run_tests();
                        break;
                    /* Temporarily disabled due to header conflicts
                    case 'x':
                        show_socket_info();
                        break;
                    case 'j':
                        show_threading_info();
                        break;
                    */
                    case 'r':
                        kernel_print("Rebooting system...\n");
                        reboot_system();
                        break;
                }
            }
        }
        
        /* Show timer updates every second (assuming 100Hz timer) */
        uint64_t current_ticks = get_timer_ticks();
        if (current_ticks - last_ticks >= 100) {
            kernel_print("Timer: %lu ticks\n", current_ticks);
            last_ticks = current_ticks;
        }
        
        /* Yield CPU to prevent busy waiting */
        __asm__ volatile ("hlt");
    }
}

/**
 * Show help information
 */
void show_help(void) {
    kernel_print("\nIKOS Kernel Commands:\n");
    kernel_print("h - Show this help\n");
    kernel_print("s - Show interrupt statistics\n");
    kernel_print("i - Show timer information\n");
    kernel_print("d - Show device driver framework info\n");
    kernel_print("f - Show framebuffer driver info\n");
    kernel_print("a - Show application loader info\n");
    kernel_print("l - Test application loader\n");
    kernel_print("e - Test file explorer basic operations\n");
    kernel_print("x - Run file explorer test suite\n");
    kernel_print("o - Open file explorer window\n");
    kernel_print("n - Show notification system info\n");
    kernel_print("m - Test notification system\n");
    kernel_print("t - Show terminal GUI info\n");
    kernel_print("g - Test terminal GUI integration\n");
    kernel_print("u - Run terminal GUI tests\n");
    kernel_print("w - Show network driver info\n");
    kernel_print("q - Test network driver integration\n");
    kernel_print("z - Run network driver tests\n");
    kernel_print("x - Show socket API info\n");
    kernel_print("j - Show threading system info\n");
    kernel_print("r - Reboot system\n");
    kernel_print("\n");
}

/**
 * Show interrupt statistics
 */
void show_statistics(void) {
    kernel_print("\nInterrupt Statistics:\n");
    kernel_print("Timer interrupts: %lu\n", get_interrupt_count(IRQ_BASE + IRQ_TIMER));
    kernel_print("Keyboard interrupts: %lu\n", get_interrupt_count(IRQ_BASE + IRQ_KEYBOARD));
    kernel_print("Page faults: %lu\n", get_interrupt_count(INT_PAGE_FAULT));
    kernel_print("General protection faults: %lu\n", get_interrupt_count(INT_GENERAL_PROTECTION));
    kernel_print("System calls: %lu\n", get_interrupt_count(INT_SYSCALL));
    kernel_print("\n");
}

/**
 * Show timer information
 */
void show_timer_info(void) {
    uint64_t ticks = get_timer_ticks();
    uint64_t seconds = ticks / 100;  /* Assuming 100Hz timer */
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    
    kernel_print("\nTimer Information:\n");
    kernel_print("Total ticks: %lu\n", ticks);
    kernel_print("Uptime: %lu:%02lu:%02lu\n", hours, minutes % 60, seconds % 60);
    kernel_print("\n");
}

/**
 * Reboot the system
 */
void reboot_system(void) {
    /* Disable interrupts */
    disable_interrupts();
    
    /* Use keyboard controller to reset */
    uint8_t temp;
    do {
        temp = inb(0x64);
        if (temp & 0x01) {
            inb(0x60);
        }
    } while (temp & 0x02);
    
    outb(0x64, 0xFE);  /* Reset command */
    
    /* If that fails, triple fault */
    __asm__ volatile ("int $0x00");
}

/**
 * Show device driver framework information
 */
void show_device_info(void) {
    device_manager_stats_t dev_stats;
    device_manager_get_stats(&dev_stats);
    
    kernel_print("\nDevice Driver Framework Status:\n");
    kernel_print("Total Devices: %u\n", dev_stats.total_devices);
    kernel_print("Active Devices: %u\n", dev_stats.active_devices);
    kernel_print("Total Drivers: %u\n", dev_stats.total_drivers);
    kernel_print("Loaded Drivers: %u\n", dev_stats.loaded_drivers);
    
    pci_stats_t pci_stats;
    pci_get_stats(&pci_stats);
    
    kernel_print("\nPCI Bus Information:\n");
    kernel_print("Total PCI Devices: %u\n", pci_stats.total_devices);
    kernel_print("Buses Scanned: %u\n", pci_stats.buses_scanned);
    kernel_print("Storage Devices: %u\n", pci_stats.storage_devices);
    kernel_print("Network Devices: %u\n", pci_stats.network_devices);
    
    ide_stats_t ide_stats;
    ide_get_stats(&ide_stats);
    
    kernel_print("\nIDE Driver Information:\n");
    kernel_print("Controllers Found: %u\n", ide_stats.controllers_found);
    kernel_print("Drives Found: %u\n", ide_stats.drives_found);
    kernel_print("Total Reads: %u\n", ide_stats.total_reads);
    kernel_print("Total Writes: %u\n", ide_stats.total_writes);
    
    kernel_print("\n");
}

/**
 * Show framebuffer driver information
 */
void show_framebuffer_info(void) {
    fb_info_t* info = fb_get_info();
    
    kernel_print("\nFramebuffer Driver Status:\n");
    if (info && info->initialized) {
        kernel_print("Status: Initialized\n");
        kernel_print("Mode: %d\n", (int)info->mode);
        kernel_print("Resolution: %ux%u\n", info->width, info->height);
        kernel_print("Bits per pixel: %u\n", info->bpp);
        kernel_print("Pitch: %u bytes\n", info->pitch);
        kernel_print("Buffer size: %u bytes\n", info->size);
        kernel_print("Buffer address: 0x%p\n", info->buffer);
        kernel_print("Double buffered: %s\n", info->double_buffered ? "Yes" : "No");
        kernel_print("Color format: %d\n", (int)info->format);
    } else {
        kernel_print("Status: Not initialized\n");
    }
    
    fb_stats_t stats;
    fb_get_stats(&stats);
    
    kernel_print("\nFramebuffer Statistics:\n");
    kernel_print("Pixels drawn: %lu\n", stats.pixels_drawn);
    kernel_print("Lines drawn: %lu\n", stats.lines_drawn);
    kernel_print("Rectangles drawn: %lu\n", stats.rects_drawn);
    kernel_print("Characters drawn: %lu\n", stats.chars_drawn);
    kernel_print("Buffer swaps: %lu\n", stats.buffer_swaps);
    
    kernel_print("\n");
}

/**
 * Simple memory initialization placeholder
 */
void memory_init(void) {
    /* Initialize the new kalloc memory management system */
    extern uint8_t _kernel_end;  /* Linker symbol for end of kernel */
    
    /* Start heap after kernel + some safety margin */
    void* heap_start = (void*)0x400000; /* 4MB - after kernel space */
    size_t heap_size = 0x800000;        /* 8MB heap size */
    
    int result = kalloc_init(heap_start, heap_size);
    if (result == KALLOC_SUCCESS) {
        kernel_print("KALLOC: Memory allocator initialized with %d MB heap\n", 
                    heap_size / (1024 * 1024));
        
        /* Run allocator tests */
        kalloc_run_tests();
        
        kernel_print("KALLOC: All tests completed successfully\n");
    } else {
        kernel_print("KALLOC: Failed to initialize memory allocator (error %d)\n", result);
    }
    
    /* Initialize user-space execution system - Issue #14 */
    init_user_space_execution();
    
    kernel_print("Memory management initialized\n");
}

/*
 * Show app loader information - temporarily disabled due to header conflicts
void show_app_loader_info(void) {
    kernel_print("
=== App Loader Information ===
");
    
    app_loader_stats_t stats;
    if (app_loader_get_stats(&stats) == APP_ERROR_SUCCESS) {
        kernel_print("
App Loader Statistics:
");
        kernel_print("Registry size: %u
", stats.registry_size);
        kernel_print("Apps loaded: %u
", stats.apps_loaded);
        kernel_print("Apps running: %u
", stats.apps_running);
        kernel_print("Apps terminated: %u
", stats.apps_terminated);
        kernel_print("Launch failures: %u
", stats.launch_failures);
        kernel_print("GUI apps active: %u
", stats.gui_apps_active);
        kernel_print("CLI apps active: %u
", stats.cli_apps_active);
        kernel_print("Total memory used: %u bytes
", stats.total_memory_used);
    } else {
        kernel_print("Failed to retrieve app loader statistics
");
    }
    
    kernel_print("
App Loader ready for use
");
    kernel_print("Supported formats: ELF, user-space binaries
");
    kernel_print("Memory management and process isolation included
");
}
*/

/**
 * Show notification system information
 */
void show_notification_info(void) {
    kernel_print("\nNotification System Information:\n");
    
    notification_stats_t stats;
    if (notification_get_stats(&stats) == NOTIFICATION_SUCCESS) {
        kernel_print("Total notifications sent: %lu\n", stats.total_notifications_sent);
        kernel_print("Total notifications shown: %lu\n", stats.total_notifications_shown);
        kernel_print("Total notifications dismissed: %lu\n", stats.total_notifications_dismissed);
        kernel_print("Current active count: %u\n", stats.current_active_count);
        kernel_print("Peak active count: %u\n", stats.peak_active_count);
        kernel_print("System alerts: %lu\n", stats.total_system_alerts);
        kernel_print("Panel visible: %s\n", notification_is_panel_visible() ? "Yes" : "No");
    } else {
        kernel_print("Failed to get notification system statistics\n");
    }
    kernel_print("\n");
}

/**
 * Simple kernel print function
 */
void kernel_print(const char* format, ...) {
    /* TODO: Implement proper formatted printing to console */
    /* For now, this is a placeholder that would output to VGA or serial */
    
    /* Simple character output for demonstration */
    static char* video_memory = (char*)0xB8000;
    static int cursor_pos = 0;
    
    /* Very basic string output - just for testing */
    const char* str = format;
    while (*str && cursor_pos < 80 * 25) {
        if (*str != '%' && *str != '\\') {
            video_memory[cursor_pos * 2] = *str;
            video_memory[cursor_pos * 2 + 1] = 0x07;  /* White on black */
            cursor_pos++;
        }
        str++;
    }
}

/* Port I/O functions */
void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * Show terminal GUI information
 */
void show_terminal_gui_info(void) {
    kernel_print("\nTerminal GUI Integration Information:\n");
    kernel_print("=====================================\n");
    
    // Get focused instance
    terminal_gui_instance_t* focused = terminal_gui_get_focused_instance();
    if (focused) {
        kernel_print("Focused Terminal ID: %u\n", focused->id);
        kernel_print("Focused Terminal Title: %s\n", focused->title);
        kernel_print("Focused Terminal State: %d\n", focused->state);
        kernel_print("Visible Columns: %u\n", focused->visible_cols);
        kernel_print("Visible Rows: %u\n", focused->visible_rows);
    } else {
        kernel_print("No focused terminal instance\n");
    }
    
    // Count active instances
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < TERMINAL_GUI_MAX_INSTANCES; i++) {
        // Note: This would need access to internal manager structure
        // For now, just show general information
    }
    
    kernel_print("Maximum Instances: %d\n", TERMINAL_GUI_MAX_INSTANCES);
    kernel_print("Maximum Tabs per Instance: %d\n", TERMINAL_GUI_MAX_TABS);
    kernel_print("Default Window Size: %dx%d\n", TERMINAL_GUI_DEFAULT_WIDTH, TERMINAL_GUI_DEFAULT_HEIGHT);
    kernel_print("Character Cell Size: %dx%d\n", TERMINAL_GUI_CHAR_WIDTH, TERMINAL_GUI_CHAR_HEIGHT);
    
    kernel_print("\nTerminal GUI Integration ready for use\n");
}

/**
 * Show network interface driver information
 */
void show_network_info(void) {
    kernel_print("\nNetwork Interface Driver Information:\n");
    kernel_print("====================================\n");
    
    // Show interface summary
    kernel_print("Printing all network interfaces:\n");
    network_print_all_interfaces();
    
    // Show default interface
    network_interface_t* default_iface = network_get_default_interface();
    if (default_iface) {
        kernel_print("\nDefault Interface: %s\n", default_iface->name);
        kernel_print("Type: %s\n", 
                    default_iface->type == NETWORK_TYPE_ETHERNET ? "Ethernet" : 
                    default_iface->type == NETWORK_TYPE_WIFI ? "Wi-Fi" : "Unknown");
        kernel_print("State: %s\n", 
                    default_iface->state == NETWORK_STATE_UP ? "UP" : "DOWN");
        kernel_print("MAC: %s\n", network_mac_addr_to_string(&default_iface->mac_address));
        kernel_print("IP: %s\n", network_ip_addr_to_string(&default_iface->ip_address));
        kernel_print("DHCP: %s\n", default_iface->dhcp_enabled ? "Enabled" : "Disabled");
    } else {
        kernel_print("\nNo default interface configured\n");
    }
    
    // Show global statistics
    uint64_t global_tx_packets, global_rx_packets, global_tx_bytes, global_rx_bytes;
    if (network_get_global_stats(&global_tx_packets, &global_rx_packets, 
                                &global_tx_bytes, &global_rx_bytes) == NETWORK_SUCCESS) {
        kernel_print("\nGlobal Network Statistics:\n");
        kernel_print("TX: %lu packets (%lu bytes)\n", global_tx_packets, global_tx_bytes);
        kernel_print("RX: %lu packets (%lu bytes)\n", global_rx_packets, global_rx_bytes);
    }
    
    kernel_print("\nNetwork Interface Driver ready for use\n");
}

/**
 * Show socket API information
 */
/*
 * Show socket API information - temporarily disabled due to header conflicts
void show_socket_info(void) {
    kernel_print("\n=== Socket API Information ===\n");
    
    socket_syscall_stats_t stats;
    if (socket_get_syscall_stats(&stats) == SOCK_SUCCESS) {
        kernel_print("\nSocket System Statistics:\n");
        kernel_print("Active sockets: %u\n", stats.active_sockets);
        kernel_print("Total sockets created: %u\n", stats.total_sockets_created);
        kernel_print("Total sockets closed: %u\n", stats.total_sockets_closed);
        kernel_print("Failed socket operations: %u\n", stats.failed_operations);
        kernel_print("Total bytes sent: %lu\n", stats.total_bytes_sent);
        kernel_print("Total bytes received: %lu\n", stats.total_bytes_received);
        
        kernel_print("\nSocket Type Breakdown:\n");
        kernel_print("TCP sockets: %u\n", stats.tcp_sockets);
        kernel_print("UDP sockets: %u\n", stats.udp_sockets);
        kernel_print("Raw sockets: %u\n", stats.raw_sockets);
        
        kernel_print("\nSocket States:\n");
        kernel_print("Listening sockets: %u\n", stats.listening_sockets);
        kernel_print("Connected sockets: %u\n", stats.connected_sockets);
        kernel_print("Bound sockets: %u\n", stats.bound_sockets);
    } else {
        kernel_print("Failed to retrieve socket statistics\n");
    }
    
    const socket_syscall_config_t* config = socket_get_syscall_config();
    if (config) {
        kernel_print("\nSocket API Configuration:\n");
        kernel_print("Max sockets per process: %u\n", config->max_sockets_per_process);
        kernel_print("Default send buffer size: %u bytes\n", config->default_send_buffer_size);
        kernel_print("Default receive buffer size: %u bytes\n", config->default_recv_buffer_size);
        kernel_print("Default socket timeout: %u ms\n", config->default_timeout_ms);
        kernel_print("Non-blocking operations: %s\n", 
                   config->enable_nonblocking ? "Enabled" : "Disabled");
        kernel_print("Socket reuse: %s\n", 
                   config->enable_reuseaddr ? "Enabled" : "Disabled");
    }
    
    kernel_print("\nSocket API ready for use\n");
    kernel_print("Berkeley-style socket interface available\n");
    kernel_print("Supported operations: socket(), bind(), listen(), accept(), connect(), send(), recv()\n");
    kernel_print("Non-blocking operations and address utilities included\n");
}
*/

/*
 * Show threading system information - temporarily disabled due to header conflicts
void show_threading_info(void) {
    kernel_print("\n=== Threading System Information ===\n");
    
    thread_syscall_stats_t stats;
    if (thread_get_syscall_stats(&stats) == THREAD_SUCCESS) {
        kernel_print("\nThreading System Statistics:\n");
        kernel_print("Active threads: %u\n", stats.active_threads);
        kernel_print("Total threads created: %u\n", stats.total_threads_created);
        kernel_print("Total threads destroyed: %u\n", stats.total_threads_destroyed);
        kernel_print("Context switches: %llu\n", stats.context_switches);
        kernel_print("Failed thread creations: %u\n", stats.failed_thread_creations);
        
        kernel_print("\nSynchronization Objects:\n");
        kernel_print("Active mutexes: %u\n", stats.active_mutexes);
        kernel_print("Active condition variables: %u\n", stats.active_condition_variables);
        kernel_print("Active semaphores: %u\n", stats.active_semaphores);
        kernel_print("Active barriers: %u\n", stats.active_barriers);
        kernel_print("Active spinlocks: %u\n", stats.active_spinlocks);
        kernel_print("Active TLS keys: %u\n", stats.active_tls_keys);
        
        kernel_print("\nSyscall Statistics:\n");
        kernel_print("Thread create calls: %u\n", stats.syscall_thread_create);
        kernel_print("Thread join calls: %u\n", stats.syscall_thread_join);
        kernel_print("Mutex lock calls: %u\n", stats.syscall_mutex_lock);
        kernel_print("Mutex unlock calls: %u\n", stats.syscall_mutex_unlock);
        kernel_print("Condition wait calls: %u\n", stats.syscall_cond_wait);
        kernel_print("Condition signal calls: %u\n", stats.syscall_cond_signal);
    } else {
        kernel_print("Failed to retrieve threading statistics\n");
    }
    
    const thread_syscall_config_t* config = thread_get_syscall_config();
    if (config) {
        kernel_print("\nThreading System Configuration:\n");
        kernel_print("Max threads per process: %u\n", config->max_threads_per_process);
        kernel_print("Default stack size: %u KB\n", config->default_stack_size / 1024);
        kernel_print("Min stack size: %u KB\n", config->min_stack_size / 1024);
        kernel_print("Max stack size: %u KB\n", config->max_stack_size / 1024);
        kernel_print("Thread-local storage slots: %u\n", config->max_tls_keys);
        kernel_print("Preemptive scheduling: %s\n", 
                   config->enable_preemption ? "Enabled" : "Disabled");
        kernel_print("Priority inheritance: %s\n", 
                   config->enable_priority_inheritance ? "Enabled" : "Disabled");
    }
    
    kernel_print("\nThreading API ready for use\n");
    kernel_print("POSIX-style pthread interface available\n");
    kernel_print("Supported operations: pthread_create(), pthread_join(), pthread_detach()\n");
    kernel_print("Synchronization: mutexes, condition variables, semaphores, barriers, spinlocks\n");
    kernel_print("Thread-local storage and comprehensive thread management included\n");
}
*/
