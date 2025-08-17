/* IKOS Advanced Memory Management Integration - Issue #27
 * Integrates buddy allocator, slab allocator, demand paging, and memory compression
 */

#include "memory_advanced.h"
#include "memory.h"
#include "process.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================== Global State ========================== */

static bool advanced_memory_initialized = false;
static struct advanced_memory_config config = {0};

/* Global statistics */
static struct advanced_memory_stats global_stats = {0};

/* Memory policy settings */
static struct memory_policy {
    /* Allocation policies */
    bool prefer_buddy_for_large;       /* Use buddy for large allocations */
    bool enable_slab_merging;           /* Merge similar-sized slabs */
    bool enable_compression;            /* Enable memory compression */
    bool enable_swap;                   /* Enable demand paging/swap */
    
    /* Thresholds */
    size_t large_allocation_threshold;  /* Switch to buddy allocator */
    size_t compression_threshold;       /* Minimum size for compression */
    uint32_t memory_pressure_threshold; /* Start reclaim percentage */
    uint32_t swap_threshold;            /* Start swapping percentage */
    
    /* Performance tuning */
    bool per_cpu_caches;                /* Enable per-CPU slab caches */
    bool numa_awareness;                /* Enable NUMA-aware allocation */
    replacement_algorithm_t replacement_alg; /* Page replacement algorithm */
} memory_policy = {
    .prefer_buddy_for_large = true,
    .enable_slab_merging = true,
    .enable_compression = true,
    .enable_swap = true,
    .large_allocation_threshold = 4096,
    .compression_threshold = 1024,
    .memory_pressure_threshold = 80,
    .swap_threshold = 90,
    .per_cpu_caches = true,
    .numa_awareness = false,
    .replacement_alg = REPLACEMENT_LRU
};

/* ========================== Forward Declarations ========================== */

static void debug_print(const char* format, ...);
static int initialize_memory_zones(void);
static int setup_default_caches(void);
static void register_oom_handler(void);
static void memory_reclaim_worker(void);

/* ========================== Initialization Functions ========================== */

/**
 * Initialize advanced memory management system
 */
int advanced_memory_init(struct advanced_memory_config* cfg) {
    if (advanced_memory_initialized) {
        return 0;
    }
    
    debug_print("Advanced Memory: Initializing comprehensive memory management...\n");
    
    /* Copy configuration if provided */
    if (cfg) {
        memcpy(&config, cfg, sizeof(config));
    } else {
        /* Use default configuration */
        config.enable_buddy = true;
        config.enable_slab = true;
        config.enable_demand_paging = true;
        config.enable_compression = true;
        config.memory_size = 512 * 1024 * 1024;  /* 512MB default */
        config.numa_nodes = 1;
        config.cpu_count = 1;
        strcpy(config.swap_file_path, "/var/swap/swapfile");
        config.swap_size = 1024 * 1024 * 1024;  /* 1GB swap */
    }
    
    /* Initialize buddy allocator */
    if (config.enable_buddy) {
        debug_print("Advanced Memory: Initializing buddy allocator...\n");
        int result = buddy_allocator_init();
        if (result != 0) {
            debug_print("Advanced Memory: Buddy allocator initialization failed\n");
            return result;
        }
        
        /* Setup memory zones */
        result = initialize_memory_zones();
        if (result != 0) {
            debug_print("Advanced Memory: Memory zone setup failed\n");
            return result;
        }
    }
    
    /* Initialize slab allocator */
    if (config.enable_slab) {
        debug_print("Advanced Memory: Initializing slab allocator...\n");
        int result = slab_allocator_init();
        if (result != 0) {
            debug_print("Advanced Memory: Slab allocator initialization failed\n");
            return result;
        }
        
        /* Setup default caches */
        result = setup_default_caches();
        if (result != 0) {
            debug_print("Advanced Memory: Default cache setup failed\n");
            return result;
        }
    }
    
    /* Initialize demand paging */
    if (config.enable_demand_paging) {
        debug_print("Advanced Memory: Initializing demand paging...\n");
        int result = demand_paging_init();
        if (result != 0) {
            debug_print("Advanced Memory: Demand paging initialization failed\n");
            return result;
        }
        
        /* Add swap file if configured */
        if (config.swap_size > 0) {
            result = add_swap_file(config.swap_file_path, config.swap_size);
            if (result != 0) {
                debug_print("Advanced Memory: Warning - swap file setup failed\n");
            }
        }
    }
    
    /* Initialize memory compression */
    if (config.enable_compression) {
        debug_print("Advanced Memory: Initializing memory compression...\n");
        int result = memory_compression_init();
        if (result != 0) {
            debug_print("Advanced Memory: Memory compression initialization failed\n");
            return result;
        }
    }
    
    /* Register out-of-memory handler */
    register_oom_handler();
    
    /* Initialize statistics */
    memset(&global_stats, 0, sizeof(global_stats));
    global_stats.initialization_time = get_system_time();
    
    advanced_memory_initialized = true;
    debug_print("Advanced Memory: Initialization complete\n");
    
    return 0;
}

/**
 * Initialize memory zones for buddy allocator
 */
static int initialize_memory_zones(void) {
    debug_print("Advanced Memory: Setting up memory zones...\n");
    
    /* Calculate zone boundaries based on total memory */
    uint64_t total_pages = config.memory_size / 4096;
    uint64_t dma_pages = (total_pages > 4096) ? 4096 : total_pages / 4;
    uint64_t normal_pages = total_pages - dma_pages;
    
    /* Add DMA zone (first 16MB typically) */
    int result = buddy_add_zone(0, dma_pages, ZONE_DMA, 0);
    if (result != 0) {
        debug_print("Advanced Memory: Failed to add DMA zone\n");
        return result;
    }
    
    /* Add normal zone */
    if (normal_pages > 0) {
        result = buddy_add_zone(dma_pages, dma_pages + normal_pages, ZONE_NORMAL, 0);
        if (result != 0) {
            debug_print("Advanced Memory: Failed to add normal zone\n");
            return result;
        }
    }
    
    debug_print("Advanced Memory: Memory zones configured - DMA: %lu pages, Normal: %lu pages\n",
               dma_pages, normal_pages);
    
    return 0;
}

/**
 * Setup default slab caches for common kernel objects
 */
static int setup_default_caches(void) {
    debug_print("Advanced Memory: Setting up default slab caches...\n");
    
    /* Common allocation sizes */
    size_t cache_sizes[] = {32, 64, 128, 256, 512, 1024, 2048};
    char cache_names[][32] = {
        "kmalloc-32", "kmalloc-64", "kmalloc-128", "kmalloc-256",
        "kmalloc-512", "kmalloc-1024", "kmalloc-2048"
    };
    
    for (int i = 0; i < 7; i++) {
        kmem_cache_t* cache = kmem_cache_create(cache_names[i], cache_sizes[i], 8, 
                                               SLAB_CACHE_POISON, NULL, NULL);
        if (!cache) {
            debug_print("Advanced Memory: Failed to create cache %s\n", cache_names[i]);
            return -ENOMEM;
        }
    }
    
    /* Process-specific caches */
    kmem_cache_t* task_cache = kmem_cache_create("task_struct", 
                                                sizeof(struct process), 8, 
                                                SLAB_CACHE_POISON, NULL, NULL);
    if (!task_cache) {
        debug_print("Advanced Memory: Failed to create task cache\n");
        return -ENOMEM;
    }
    
    /* Memory management caches */
    kmem_cache_t* vma_cache = kmem_cache_create("vm_area_struct", 
                                               sizeof(struct vm_area_struct), 8,
                                               SLAB_CACHE_POISON, NULL, NULL);
    if (!vma_cache) {
        debug_print("Advanced Memory: Failed to create VMA cache\n");
        return -ENOMEM;
    }
    
    debug_print("Advanced Memory: Default caches created successfully\n");
    return 0;
}

/* ========================== Memory Allocation Interface ========================== */

/**
 * Intelligent memory allocation that chooses the best allocator
 */
void* advanced_kmalloc(size_t size, gfp_t flags) {
    if (!advanced_memory_initialized) {
        return NULL;
    }
    
    global_stats.total_allocations++;
    
    /* Choose allocation strategy based on size and policy */
    if (size >= memory_policy.large_allocation_threshold || !config.enable_slab) {
        /* Use buddy allocator for large allocations */
        unsigned int order = 0;
        while ((1UL << order) * 4096 < size) {
            order++;
        }
        
        struct page* page = buddy_alloc_pages(flags, order);
        if (page) {
            global_stats.buddy_allocations++;
            global_stats.bytes_allocated += (1UL << order) * 4096;
            return (void*)page;
        } else {
            global_stats.allocation_failures++;
            return NULL;
        }
    } else {
        /* Use slab allocator for small allocations */
        
        /* Find appropriate cache */
        kmem_cache_t* cache = find_cache_for_size(size);
        if (!cache) {
            global_stats.allocation_failures++;
            return NULL;
        }
        
        void* ptr = kmem_cache_alloc(cache, flags);
        if (ptr) {
            global_stats.slab_allocations++;
            global_stats.bytes_allocated += size;
            return ptr;
        } else {
            global_stats.allocation_failures++;
            return NULL;
        }
    }
}

/**
 * Free memory allocated by advanced_kmalloc
 */
void advanced_kfree(void* ptr, size_t size) {
    if (!ptr || !advanced_memory_initialized) {
        return;
    }
    
    global_stats.total_frees++;
    
    /* Determine allocation type and free appropriately */
    if (size >= memory_policy.large_allocation_threshold) {
        /* Free through buddy allocator */
        unsigned int order = 0;
        while ((1UL << order) * 4096 < size) {
            order++;
        }
        
        buddy_free_pages((struct page*)ptr, order);
        global_stats.buddy_frees++;
        global_stats.bytes_freed += (1UL << order) * 4096;
    } else {
        /* Free through slab allocator */
        kmem_cache_t* cache = find_cache_for_size(size);
        if (cache) {
            kmem_cache_free(cache, ptr);
            global_stats.slab_frees++;
            global_stats.bytes_freed += size;
        }
    }
}

/* ========================== Memory Pressure Management ========================== */

/**
 * Handle memory pressure by reclaiming pages
 */
static void handle_memory_pressure(void) {
    debug_print("Advanced Memory: Handling memory pressure...\n");
    
    global_stats.memory_pressure_events++;
    
    /* Get current memory usage */
    struct buddy_allocator_stats buddy_stats;
    buddy_get_stats(&buddy_stats);
    
    uint64_t total_memory = config.memory_size;
    uint64_t used_memory = buddy_stats.current_usage * 4096;
    uint64_t usage_percentage = (used_memory * 100) / total_memory;
    
    debug_print("Advanced Memory: Memory usage at %lu%%\n", usage_percentage);
    
    /* Start compression if enabled and threshold reached */
    if (config.enable_compression && 
        usage_percentage >= memory_policy.compression_threshold) {
        
        debug_print("Advanced Memory: Starting memory compression...\n");
        int compressed_pages = compress_inactive_pages();
        if (compressed_pages > 0) {
            global_stats.compression_events++;
            debug_print("Advanced Memory: Compressed %d pages\n", compressed_pages);
        }
    }
    
    /* Start swapping if enabled and threshold reached */
    if (config.enable_demand_paging && 
        usage_percentage >= memory_policy.swap_threshold) {
        
        debug_print("Advanced Memory: Starting page swapping...\n");
        int swapped_pages = swap_out_inactive_pages();
        if (swapped_pages > 0) {
            global_stats.swap_events++;
            debug_print("Advanced Memory: Swapped out %d pages\n", swapped_pages);
        }
    }
    
    /* Reclaim slab caches if needed */
    if (config.enable_slab && usage_percentage >= memory_policy.memory_pressure_threshold) {
        debug_print("Advanced Memory: Reclaiming slab caches...\n");
        int reclaimed = reclaim_slab_caches();
        if (reclaimed > 0) {
            global_stats.cache_reclaim_events++;
            debug_print("Advanced Memory: Reclaimed %d cache objects\n", reclaimed);
        }
    }
}

/**
 * Out-of-memory handler
 */
static void oom_handler(void) {
    debug_print("Advanced Memory: Out of memory condition detected!\n");
    
    global_stats.oom_events++;
    
    /* Emergency memory reclaim */
    handle_memory_pressure();
    
    /* TODO: Implement OOM killer for user processes */
    debug_print("Advanced Memory: Emergency reclaim completed\n");
}

/**
 * Register OOM handler with the system
 */
static void register_oom_handler(void) {
    /* TODO: Integrate with kernel's memory management subsystem */
    debug_print("Advanced Memory: OOM handler registered\n");
}

/* ========================== Statistics and Monitoring ========================== */

/**
 * Get comprehensive memory management statistics
 */
void advanced_memory_get_stats(struct advanced_memory_stats* stats) {
    if (!stats || !advanced_memory_initialized) {
        return;
    }
    
    /* Copy global statistics */
    memcpy(stats, &global_stats, sizeof(global_stats));
    
    /* Get component-specific statistics */
    if (config.enable_buddy) {
        buddy_get_stats(&stats->buddy_stats);
    }
    
    if (config.enable_slab) {
        slab_get_stats(&stats->slab_stats);
    }
    
    if (config.enable_compression) {
        memory_compression_get_stats(&stats->compression_stats);
    }
    
    if (config.enable_demand_paging) {
        demand_paging_get_stats(&stats->paging_stats);
    }
    
    /* Calculate derived statistics */
    if (stats->total_allocations > 0) {
        stats->allocation_success_rate = 
            ((stats->total_allocations - stats->allocation_failures) * 100) / 
            stats->total_allocations;
    }
    
    if (stats->bytes_allocated > 0) {
        stats->average_allocation_size = stats->bytes_allocated / stats->total_allocations;
    }
    
    stats->current_memory_usage = config.memory_size - (stats->buddy_stats.current_usage * 4096);
    stats->memory_utilization = (stats->current_memory_usage * 100) / config.memory_size;
}

/**
 * Print comprehensive memory management report
 */
void advanced_memory_dump_state(void) {
    debug_print("=== Advanced Memory Management State Report ===\n");
    debug_print("Initialized: %s\n", advanced_memory_initialized ? "Yes" : "No");
    
    /* Configuration */
    debug_print("\nConfiguration:\n");
    debug_print("  Buddy allocator: %s\n", config.enable_buddy ? "Enabled" : "Disabled");
    debug_print("  Slab allocator: %s\n", config.enable_slab ? "Enabled" : "Disabled");
    debug_print("  Demand paging: %s\n", config.enable_demand_paging ? "Enabled" : "Disabled");
    debug_print("  Memory compression: %s\n", config.enable_compression ? "Enabled" : "Disabled");
    debug_print("  Total memory: %lu MB\n", config.memory_size / (1024 * 1024));
    debug_print("  Swap size: %lu MB\n", config.swap_size / (1024 * 1024));
    
    /* Global statistics */
    debug_print("\nGlobal Statistics:\n");
    debug_print("  Total allocations: %lu\n", global_stats.total_allocations);
    debug_print("  Total frees: %lu\n", global_stats.total_frees);
    debug_print("  Allocation failures: %lu\n", global_stats.allocation_failures);
    debug_print("  Bytes allocated: %lu\n", global_stats.bytes_allocated);
    debug_print("  Bytes freed: %lu\n", global_stats.bytes_freed);
    debug_print("  Memory pressure events: %lu\n", global_stats.memory_pressure_events);
    debug_print("  OOM events: %lu\n", global_stats.oom_events);
    
    /* Component state dumps */
    if (config.enable_buddy) {
        debug_print("\n--- Buddy Allocator State ---\n");
        buddy_dump_state();
    }
    
    if (config.enable_slab) {
        debug_print("\n--- Slab Allocator State ---\n");
        slab_dump_state();
    }
    
    if (config.enable_compression) {
        debug_print("\n--- Memory Compression State ---\n");
        memory_compression_dump_state();
    }
    
    if (config.enable_demand_paging) {
        debug_print("\n--- Demand Paging State ---\n");
        demand_paging_dump_state();
    }
}

/**
 * Monitor memory usage and trigger reclaim if needed
 */
void advanced_memory_monitor(void) {
    if (!advanced_memory_initialized) {
        return;
    }
    
    /* Check memory pressure */
    struct buddy_allocator_stats buddy_stats;
    buddy_get_stats(&buddy_stats);
    
    uint64_t total_memory = config.memory_size;
    uint64_t used_memory = buddy_stats.current_usage * 4096;
    uint64_t usage_percentage = (used_memory * 100) / total_memory;
    
    if (usage_percentage >= memory_policy.memory_pressure_threshold) {
        handle_memory_pressure();
    }
}

/* ========================== Utility Functions ========================== */

/**
 * Find appropriate cache for allocation size
 */
static kmem_cache_t* find_cache_for_size(size_t size) {
    /* TODO: Implement cache lookup by size */
    /* For now, return NULL to indicate no suitable cache found */
    (void)size;
    return NULL;
}

/**
 * Compress inactive pages
 */
static int compress_inactive_pages(void) {
    /* TODO: Implement page compression logic */
    return 0;
}

/**
 * Swap out inactive pages
 */
static int swap_out_inactive_pages(void) {
    /* TODO: Implement page swapping logic */
    return 0;
}

/**
 * Reclaim slab caches
 */
static int reclaim_slab_caches(void) {
    /* TODO: Implement slab cache reclaim logic */
    return 0;
}

/**
 * Get system time
 */
static uint64_t get_system_time(void) {
    static uint64_t time_counter = 0;
    return ++time_counter;
}

/**
 * Debug print function
 */
static void debug_print(const char* format, ...) {
    /* TODO: Integrate with kernel logging system */
    (void)format;
}

/* ========================== Public API Functions ========================== */

/**
 * Set memory management policy
 */
void advanced_memory_set_policy(struct memory_policy* policy) {
    if (policy) {
        memcpy(&memory_policy, policy, sizeof(memory_policy));
        debug_print("Advanced Memory: Policy updated\n");
    }
}

/**
 * Get current memory management policy
 */
void advanced_memory_get_policy(struct memory_policy* policy) {
    if (policy) {
        memcpy(policy, &memory_policy, sizeof(memory_policy));
    }
}

/**
 * Cleanup advanced memory management
 */
void advanced_memory_cleanup(void) {
    if (!advanced_memory_initialized) {
        return;
    }
    
    debug_print("Advanced Memory: Cleaning up...\n");
    
    /* TODO: Cleanup individual components */
    
    advanced_memory_initialized = false;
    debug_print("Advanced Memory: Cleanup complete\n");
}
