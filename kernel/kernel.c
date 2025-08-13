/* IKOS Kernel Entry Point
 * Main kernel initialization and startup
 */

#include "vmm.h"
#include "memory.h"
#include "scheduler.h"
#include "ipc.h"
#include <stdio.h>

/* Kernel entry point */
void kernel_main(void) {
    printf("IKOS Kernel Starting...\n");
    
    /* Initialize Virtual Memory Manager */
    printf("Initializing Virtual Memory Manager...\n");
    int vmm_result = vmm_init(0x20000000); // 512MB
    if (vmm_result != VMM_SUCCESS) {
        printf("VMM initialization failed: %d\n", vmm_result);
        return;
    }
    printf("VMM initialized successfully\n");
    
    /* Initialize Task Scheduler */
    printf("Initializing Task Scheduler...\n");
    if (scheduler_init() != 0) {
        printf("Scheduler initialization failed\n");
        return;
    }
    printf("Scheduler initialized successfully\n");
    
    /* Initialize IPC System */
    printf("Initializing IPC System...\n");
    if (ipc_init() != 0) {
        printf("IPC initialization failed\n");
        return;
    }
    printf("IPC system initialized successfully\n");
    
    /* Run VMM smoke test */
    printf("Running VMM smoke test...\n");
    vmm_smoke_test();
    
    printf("IKOS Kernel initialization complete!\n");
    printf("All major subsystems operational:\n");
    printf("- Virtual Memory Manager\n");
    printf("- Preemptive Task Scheduler\n");
    printf("- Inter-Process Communication\n");
    
    /* Display system statistics */
    vmm_stats_t* stats = vmm_get_stats();
    printf("\nSystem Statistics:\n");
    printf("- Total Memory Pages: %lu\n", (unsigned long)stats->total_pages);
    printf("- Free Memory Pages: %lu\n", (unsigned long)stats->free_pages);
    printf("- Allocated Pages: %lu\n", (unsigned long)stats->allocated_pages);
    printf("- Page Faults: %lu\n", (unsigned long)stats->page_faults);
    
    /* Kernel main loop */
    printf("\nKernel entering main loop...\n");
    while (1) {
        /* This would typically call the scheduler */
        __asm__ volatile("hlt");
    }
}

/* Kernel entry point from assembly */
void _start(void) {
    kernel_main();
}
