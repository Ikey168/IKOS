/* IKOS Scheduler Test Program
 * Demonstrates preemptive task scheduling functionality
 */

#include "scheduler.h"
#include "interrupts.h"

/* Test task functions */
void task1_func(void);
void task2_func(void);
void task3_func(void);

/* Simple console output (placeholder) */
void print_string(const char* str);
void print_char(char c);
void print_number(uint32_t num);

/**
 * Main scheduler test function
 */
int main(void) {
    print_string("IKOS Preemptive Scheduler Test\n");
    print_string("==============================\n\n");
    
    // Initialize scheduler with Round Robin policy
    print_string("Initializing scheduler...\n");
    if (scheduler_init(SCHED_ROUND_ROBIN, 100) != 0) {
        print_string("ERROR: Failed to initialize scheduler\n");
        return -1;
    }
    print_string("Scheduler initialized successfully\n\n");
    
    // Create test tasks
    print_string("Creating test tasks...\n");
    
    task_t* task1 = task_create("Task1", task1_func, PRIORITY_NORMAL, 4096);
    if (!task1) {
        print_string("ERROR: Failed to create Task1\n");
        return -1;
    }
    print_string("Created Task1 (PID: ");
    print_number(task1->pid);
    print_string(")\n");
    
    task_t* task2 = task_create("Task2", task2_func, PRIORITY_HIGH, 4096);
    if (!task2) {
        print_string("ERROR: Failed to create Task2\n");
        return -1;
    }
    print_string("Created Task2 (PID: ");
    print_number(task2->pid);
    print_string(")\n");
    
    task_t* task3 = task_create("Task3", task3_func, PRIORITY_LOW, 4096);
    if (!task3) {
        print_string("ERROR: Failed to create Task3\n");
        return -1;
    }
    print_string("Created Task3 (PID: ");
    print_number(task3->pid);
    print_string(")\n\n");
    
    // Start scheduler
    print_string("Starting scheduler...\n");
    scheduler_start();
    print_string("Scheduler started - preemptive multitasking enabled\n\n");
    
    // Main loop - display statistics periodically
    uint32_t stats_counter = 0;
    while (1) {
        // Yield to other tasks
        sys_yield();
        
        // Display statistics every 1000 iterations
        if (++stats_counter % 1000 == 0) {
            scheduler_stats_t* stats = get_scheduler_stats();
            
            print_string("=== Scheduler Statistics ===\n");
            print_string("Active tasks: ");
            print_number(stats->active_tasks);
            print_string("\nReady tasks: ");
            print_number(stats->ready_tasks);
            print_string("\nTotal context switches: ");
            print_number(stats->total_switches);
            print_string("\nTotal timer interrupts: ");
            print_number(stats->total_interrupts);
            print_string("\nCurrent task: ");
            task_t* current = task_get_current();
            if (current) {
                print_string(current->name);
                print_string(" (PID: ");
                print_number(current->pid);
                print_string(")");
            }
            print_string("\n\n");
        }
    }
    
    return 0;
}

/**
 * Test Task 1 - CPU intensive task
 */
void task1_func(void) {
    uint32_t counter = 0;
    
    while (1) {
        // Simulate CPU work
        for (volatile int i = 0; i < 10000; i++) {
            counter++;
        }
        
        print_string("Task1: Counter = ");
        print_number(counter);
        print_string("\n");
        
        // Voluntarily yield every 10 iterations
        if (counter % 10 == 0) {
            sys_yield();
        }
    }
}

/**
 * Test Task 2 - High priority task
 */
void task2_func(void) {
    uint32_t iterations = 0;
    
    while (1) {
        iterations++;
        
        print_string("Task2 (HIGH PRIORITY): Iteration ");
        print_number(iterations);
        print_string("\n");
        
        // Simulate some work
        for (volatile int i = 0; i < 5000; i++) {
            // Busy wait
        }
        
        // Yield after each iteration
        sys_yield();
    }
}

/**
 * Test Task 3 - Low priority background task
 */
void task3_func(void) {
    uint32_t background_work = 0;
    
    while (1) {
        // Simulate background processing
        for (volatile int i = 0; i < 20000; i++) {
            background_work++;
        }
        
        print_string("Task3 (LOW PRIORITY): Background work = ");
        print_number(background_work);
        print_string("\n");
        
        // Long yield to let other tasks run
        sys_yield();
        
        // Sleep equivalent (yield multiple times)
        for (int i = 0; i < 5; i++) {
            sys_yield();
        }
    }
}

/* Simple console output functions (placeholder implementations) */

/**
 * Print string to console
 */
void print_string(const char* str) {
    while (*str) {
        print_char(*str);
        str++;
    }
}

/**
 * Print character to console
 */
void print_char(char c) {
    // This would interface with the actual console/framebuffer
    // For now, just a placeholder
    static volatile char* video_memory = (char*)0xB8000;
    static int cursor_pos = 0;
    
    if (c == '\n') {
        cursor_pos = (cursor_pos / 160 + 1) * 160;
    } else {
        video_memory[cursor_pos] = c;
        video_memory[cursor_pos + 1] = 0x07; // White on black
        cursor_pos += 2;
    }
    
    // Wrap around if at end of screen
    if (cursor_pos >= 160 * 25 * 2) {
        cursor_pos = 0;
    }
}

/**
 * Print number to console
 */
void print_number(uint32_t num) {
    char buffer[32];
    char* ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';
    
    if (num == 0) {
        *(--ptr) = '0';
    } else {
        while (num > 0) {
            *(--ptr) = '0' + (num % 10);
            num /= 10;
        }
    }
    
    print_string(ptr);
}
