/* IKOS NUMA-Aware Memory Allocator - Issue #27
 * Non-Uniform Memory Access optimization for multi-processor systems
 */

#include "memory_advanced.h"
#include "buddy_allocator.h"
#include "slab_allocator.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
static void debug_print(const char* format, ...);
static uint64_t get_rdtsc(void);
static int get_current_cpu(void);

/* ========================== NUMA Configuration ========================== */

#define NUMA_MAX_NODES          8           /* Maximum NUMA nodes */
#define NUMA_NODE_SHIFT         20          /* Node ID bit shift */
#define NUMA_DISTANCE_THRESHOLD 20          /* Distance threshold for local allocation */
#define NUMA_CACHE_SIZE         64          /* Per-node cache entries */
#define NUMA_STATS_WINDOW       1000        /* Statistics collection window */

/* NUMA allocation policies */
typedef enum {
    NUMA_POLICY_DEFAULT = 0,                /* System default policy */
    NUMA_POLICY_PREFERRED,                  /* Prefer specific node */
    NUMA_POLICY_BIND,                       /* Bind to specific nodes */
    NUMA_POLICY_INTERLEAVE,                 /* Interleave across nodes */
    NUMA_POLICY_LOCAL                       /* Allocate on local node only */
} numa_policy_t;

/* ========================== NUMA Data Structures ========================== */

/* NUMA memory statistics per node */
typedef struct numa_stats {
    /* Allocation counters */
    uint64_t                local_allocs;       /* Local allocations */
    uint64_t                remote_allocs;      /* Remote allocations */
    uint64_t                migration_count;    /* Page migrations */
    uint64_t                cache_hits;         /* Local cache hits */
    uint64_t                cache_misses;       /* Local cache misses */
    
    /* Memory usage */
    uint64_t                total_pages;        /* Total pages in node */
    uint64_t                free_pages;         /* Free pages in node */
    uint64_t                active_pages;       /* Active pages */
    uint64_t                inactive_pages;     /* Inactive pages */
    
    /* Performance metrics */
    uint64_t                avg_latency;        /* Average access latency */
    uint64_t                bandwidth_used;     /* Bandwidth utilization */
    
    /* Load balancing */
    uint32_t                load_factor;        /* Current load factor */
    uint32_t                pressure_level;     /* Memory pressure level */
} numa_stats_t;

/* Per-CPU NUMA cache */
typedef struct numa_cpu_cache {
    /* Local allocation cache */
    void**                  local_cache;        /* Local memory cache */
    uint32_t                local_count;        /* Objects in local cache */
    uint32_t                local_limit;        /* Local cache limit */
    
    /* Remote allocation tracking */
    void**                  remote_cache;       /* Remote memory cache */
    uint32_t                remote_count;       /* Objects in remote cache */
    uint32_t                remote_limit;       /* Remote cache limit */
    
    /* Preferred node tracking */
    int                     preferred_node;     /* Preferred NUMA node */
    uint64_t                last_access_time;   /* Last access timestamp */
    
    /* Statistics */
    uint64_t                hit_count;          /* Cache hit count */
    uint64_t                miss_count;         /* Cache miss count */
} numa_cpu_cache_t;

/* NUMA node descriptor */
typedef struct numa_node {
    /* Node identification */
    int                     node_id;            /* NUMA node ID */
    bool                    online;             /* Node is online */
    uint64_t                start_pfn;          /* Starting page frame */
    uint64_t                end_pfn;            /* Ending page frame */
    
    /* CPU topology */
    uint32_t                cpu_mask;           /* CPUs in this node */
    int                     num_cpus;           /* Number of CPUs */
    
    /* Memory management */
    buddy_allocator_t*      buddy_allocator;   /* Node-local buddy allocator */
    kmem_cache_t**          local_caches;       /* Node-local slab caches */
    uint32_t                num_caches;         /* Number of local caches */
    
    /* Distance matrix */
    uint8_t                 distances[NUMA_MAX_NODES]; /* Distance to other nodes */
    
    /* Memory zones */
    memory_zone_t*          zones[MAX_NR_ZONES]; /* Memory zones in node */
    
    /* Statistics and monitoring */
    numa_stats_t            stats;              /* Node statistics */
    volatile int            lock;               /* Node lock */
    
    /* Migration support */
    struct {
        uint64_t            migration_threshold; /* Migration threshold */
        uint32_t            scan_period;        /* Page scan period */
        bool                auto_migrate;       /* Auto-migration enabled */
    } migration;
} numa_node_t;

/* Global NUMA allocator state */
typedef struct numa_allocator {
    /* Node management */
    numa_node_t             nodes[NUMA_MAX_NODES]; /* NUMA nodes */
    int                     num_nodes;          /* Number of active nodes */
    int                     max_node_id;        /* Highest node ID */
    
    /* Per-CPU caches */
    numa_cpu_cache_t        cpu_caches[MAX_CPUS]; /* Per-CPU NUMA caches */
    
    /* Global policies */
    numa_policy_t           default_policy;     /* Default allocation policy */
    int                     interleave_node;    /* Current interleave node */
    
    /* Statistics and monitoring */
    struct {
        uint64_t            total_allocations;  /* Total allocations */
        uint64_t            local_allocations;  /* Local allocations */
        uint64_t            remote_allocations; /* Remote allocations */
        uint64_t            migrations;         /* Page migrations */
        uint64_t            fallback_allocs;    /* Fallback allocations */
    } global_stats;
    
    /* Configuration */
    bool                    enabled;            /* NUMA allocator enabled */
    bool                    initialized;        /* Allocator initialized */
    bool                    debug_mode;         /* Debug mode enabled */
    
    /* Synchronization */
    volatile int            global_lock;        /* Global allocator lock */
} numa_allocator_t;

/* Global NUMA allocator instance */
static numa_allocator_t g_numa_allocator = {0};

/* Lock operations */
#define NUMA_LOCK(lock)         while (__sync_lock_test_and_set(&(lock), 1)) { /* spin */ }
#define NUMA_UNLOCK(lock)       __sync_lock_release(&(lock))

/* ========================== NUMA Detection and Initialization ========================== */

/**
 * Detect NUMA topology from hardware
 */
static int detect_numa_topology(void) {
    /* TODO: Implement ACPI SRAT table parsing */
    /* For now, simulate a simple 2-node system */
    
    debug_print("NUMA: Detecting system topology\n");
    
    /* Initialize first node (node 0) */
    numa_node_t* node0 = &g_numa_allocator.nodes[0];
    node0->node_id = 0;
    node0->online = true;
    node0->start_pfn = 0;
    node0->end_pfn = 0x100000;  /* 4GB */
    node0->cpu_mask = 0x01;     /* CPU 0 */
    node0->num_cpus = 1;
    
    /* Distance matrix for node 0 */
    node0->distances[0] = 10;   /* Local distance */
    node0->distances[1] = 20;   /* Remote distance */
    
    /* Initialize second node (node 1) if available */
    if (g_numa_allocator.num_nodes > 1) {
        numa_node_t* node1 = &g_numa_allocator.nodes[1];
        node1->node_id = 1;
        node1->online = true;
        node1->start_pfn = 0x100000;
        node1->end_pfn = 0x200000;  /* 4GB-8GB */
        node1->cpu_mask = 0x02;     /* CPU 1 */
        node1->num_cpus = 1;
        
        /* Distance matrix for node 1 */
        node1->distances[0] = 20;   /* Remote distance */
        node1->distances[1] = 10;   /* Local distance */
    }
    
    debug_print("NUMA: Detected %d nodes\n", g_numa_allocator.num_nodes);
    return 0;
}

/**
 * Initialize per-node allocators
 */
static int init_node_allocators(void) {
    for (int node = 0; node < g_numa_allocator.num_nodes; node++) {
        numa_node_t* numa_node = &g_numa_allocator.nodes[node];
        
        if (!numa_node->online) {
            continue;
        }
        
        debug_print("NUMA: Initializing allocators for node %d\n", node);
        
        /* Initialize node-local buddy allocator */
        /* TODO: Create node-specific buddy allocator instance */
        numa_node->buddy_allocator = NULL;  /* Use global for now */
        
        /* Initialize per-CPU caches for this node */
        for (int cpu = 0; cpu < MAX_CPUS; cpu++) {
            numa_cpu_cache_t* cache = &g_numa_allocator.cpu_caches[cpu];
            
            /* Allocate cache arrays */
            cache->local_cache = (void**)kmalloc(sizeof(void*) * NUMA_CACHE_SIZE);
            cache->remote_cache = (void**)kmalloc(sizeof(void*) * NUMA_CACHE_SIZE);
            
            if (!cache->local_cache || !cache->remote_cache) {
                debug_print("NUMA: Failed to allocate CPU cache for CPU %d\n", cpu);
                continue;
            }
            
            /* Initialize cache parameters */
            cache->local_limit = NUMA_CACHE_SIZE;
            cache->remote_limit = NUMA_CACHE_SIZE / 2;
            cache->local_count = 0;
            cache->remote_count = 0;
            cache->preferred_node = node;  /* Default to first available node */
            cache->last_access_time = 0;
            cache->hit_count = 0;
            cache->miss_count = 0;
        }
        
        /* Initialize node statistics */
        memset(&numa_node->stats, 0, sizeof(numa_stats_t));
        numa_node->stats.total_pages = numa_node->end_pfn - numa_node->start_pfn;
        numa_node->stats.free_pages = numa_node->stats.total_pages;
        
        /* Initialize migration parameters */
        numa_node->migration.migration_threshold = 100;  /* Migrate after 100 remote accesses */
        numa_node->migration.scan_period = 1000;         /* Scan every 1000ms */
        numa_node->migration.auto_migrate = true;
        
        numa_node->lock = 0;
    }
    
    return 0;
}

/**
 * Get the NUMA node for a given CPU
 */
static int cpu_to_node(int cpu) {
    if (cpu >= MAX_CPUS) {
        return -1;
    }
    
    /* Simple mapping for now */
    for (int node = 0; node < g_numa_allocator.num_nodes; node++) {
        numa_node_t* numa_node = &g_numa_allocator.nodes[node];
        if (numa_node->cpu_mask & (1 << cpu)) {
            return node;
        }
    }
    
    return 0;  /* Default to node 0 */
}

/**
 * Get the preferred NUMA node for current context
 */
static int get_preferred_node(void) {
    int cpu = get_current_cpu();
    numa_cpu_cache_t* cache = &g_numa_allocator.cpu_caches[cpu];
    
    /* Use cached preferred node if available */
    if (cache->preferred_node >= 0 && cache->preferred_node < g_numa_allocator.num_nodes) {
        return cache->preferred_node;
    }
    
    /* Fallback to CPU-to-node mapping */
    return cpu_to_node(cpu);
}

/* ========================== NUMA-Aware Allocation ========================== */

/**
 * Allocate pages from specific NUMA node
 */
static page_frame_t* numa_alloc_pages_node(int node, unsigned int order, gfp_t flags) {
    if (node < 0 || node >= g_numa_allocator.num_nodes) {
        return NULL;
    }
    
    numa_node_t* numa_node = &g_numa_allocator.nodes[node];
    if (!numa_node->online) {
        return NULL;
    }
    
    NUMA_LOCK(numa_node->lock);
    
    /* TODO: Use node-specific buddy allocator */
    /* For now, use global buddy allocator with node preference */
    page_frame_t* pages = buddy_alloc_pages(order, flags);
    
    if (pages) {
        /* Update node statistics */
        numa_node->stats.local_allocs++;
        numa_node->stats.free_pages -= (1 << order);
        g_numa_allocator.global_stats.local_allocations++;
    }
    
    NUMA_UNLOCK(numa_node->lock);
    
    debug_print("NUMA: Allocated %u pages from node %d\n", 1 << order, node);
    return pages;
}

/**
 * Allocate pages with NUMA awareness
 */
page_frame_t* numa_alloc_pages(unsigned int order, gfp_t flags, numa_policy_t policy) {
    if (!g_numa_allocator.enabled) {
        /* Fall back to regular buddy allocator */
        return buddy_alloc_pages(order, flags);
    }
    
    g_numa_allocator.global_stats.total_allocations++;
    
    page_frame_t* pages = NULL;
    int preferred_node = get_preferred_node();
    
    switch (policy) {
        case NUMA_POLICY_LOCAL:
            /* Allocate only from local node */
            pages = numa_alloc_pages_node(preferred_node, order, flags);
            break;
            
        case NUMA_POLICY_PREFERRED:
            /* Try preferred node first, then fallback */
            pages = numa_alloc_pages_node(preferred_node, order, flags);
            if (!pages) {
                /* Try other nodes in distance order */
                numa_node_t* node = &g_numa_allocator.nodes[preferred_node];
                for (int i = 0; i < g_numa_allocator.num_nodes; i++) {
                    if (i != preferred_node && node->distances[i] < NUMA_DISTANCE_THRESHOLD) {
                        pages = numa_alloc_pages_node(i, order, flags);
                        if (pages) {
                            g_numa_allocator.global_stats.remote_allocations++;
                            break;
                        }
                    }
                }
            }
            break;
            
        case NUMA_POLICY_INTERLEAVE:
            /* Round-robin allocation across nodes */
            {
                int start_node = __sync_fetch_and_add(&g_numa_allocator.interleave_node, 1) % g_numa_allocator.num_nodes;
                for (int i = 0; i < g_numa_allocator.num_nodes; i++) {
                    int node = (start_node + i) % g_numa_allocator.num_nodes;
                    pages = numa_alloc_pages_node(node, order, flags);
                    if (pages) {
                        if (node != preferred_node) {
                            g_numa_allocator.global_stats.remote_allocations++;
                        }
                        break;
                    }
                }
            }
            break;
            
        case NUMA_POLICY_DEFAULT:
        default:
            /* Use system default policy (preferred with fallback) */
            pages = numa_alloc_pages(order, flags, NUMA_POLICY_PREFERRED);
            break;
    }
    
    /* Ultimate fallback to any available node */
    if (!pages) {
        for (int node = 0; node < g_numa_allocator.num_nodes; node++) {
            pages = numa_alloc_pages_node(node, order, flags);
            if (pages) {
                g_numa_allocator.global_stats.fallback_allocs++;
                break;
            }
        }
    }
    
    return pages;
}

/**
 * Free pages with NUMA tracking
 */
void numa_free_pages(page_frame_t* pages, unsigned int order) {
    if (!pages) {
        return;
    }
    
    /* TODO: Determine which node the pages belong to */
    /* For now, just use regular buddy allocator */
    buddy_free_pages(pages, order);
    
    /* Update statistics */
    /* TODO: Update correct node statistics */
}

/* ========================== NUMA-Aware Slab Allocation ========================== */

/**
 * Allocate from NUMA-aware cache
 */
void* numa_cache_alloc(kmem_cache_t* cache, gfp_t flags, int node) {
    if (!cache || !g_numa_allocator.enabled) {
        return kmem_cache_alloc(cache, flags);
    }
    
    int cpu = get_current_cpu();
    numa_cpu_cache_t* numa_cache = &g_numa_allocator.cpu_caches[cpu];
    
    /* Try local cache first */
    if (numa_cache->local_count > 0 && (node < 0 || node == get_preferred_node())) {
        numa_cache->local_count--;
        numa_cache->hit_count++;
        numa_cache->last_access_time = get_rdtsc();
        return numa_cache->local_cache[numa_cache->local_count];
    }
    
    /* Try remote cache */
    if (numa_cache->remote_count > 0) {
        numa_cache->remote_count--;
        numa_cache->hit_count++;
        numa_cache->last_access_time = get_rdtsc();
        return numa_cache->remote_cache[numa_cache->remote_count];
    }
    
    /* Cache miss - allocate new object */
    numa_cache->miss_count++;
    
    /* Use regular slab allocator for now */
    return kmem_cache_alloc(cache, flags);
}

/**
 * Free to NUMA-aware cache
 */
void numa_cache_free(kmem_cache_t* cache, void* obj) {
    if (!cache || !obj || !g_numa_allocator.enabled) {
        kmem_cache_free(cache, obj);
        return;
    }
    
    int cpu = get_current_cpu();
    numa_cpu_cache_t* numa_cache = &g_numa_allocator.cpu_caches[cpu];
    
    /* TODO: Determine if object is local or remote */
    bool is_local = true;  /* Simplified for now */
    
    /* Try to cache the object */
    if (is_local && numa_cache->local_count < numa_cache->local_limit) {
        numa_cache->local_cache[numa_cache->local_count] = obj;
        numa_cache->local_count++;
        return;
    } else if (!is_local && numa_cache->remote_count < numa_cache->remote_limit) {
        numa_cache->remote_cache[numa_cache->remote_count] = obj;
        numa_cache->remote_count++;
        return;
    }
    
    /* Cache full - free to slab allocator */
    kmem_cache_free(cache, obj);
}

/* ========================== Page Migration Support ========================== */

/**
 * Check if page should be migrated
 */
static bool should_migrate_page(page_frame_t* page, int target_node) {
    /* TODO: Implement migration heuristics */
    /* Consider access patterns, node distance, pressure */
    (void)page;
    (void)target_node;
    return false;  /* Disabled for now */
}

/**
 * Migrate page to target NUMA node
 */
static int migrate_page_to_node(page_frame_t* page, int target_node) {
    if (!page || target_node < 0 || target_node >= g_numa_allocator.num_nodes) {
        return -1;
    }
    
    /* TODO: Implement actual page migration */
    /* This involves:
     * 1. Allocating new page on target node
     * 2. Copying page contents
     * 3. Updating page tables
     * 4. Freeing old page
     */
    
    debug_print("NUMA: Migrating page to node %d\n", target_node);
    
    /* Update migration statistics */
    g_numa_allocator.nodes[target_node].stats.migration_count++;
    g_numa_allocator.global_stats.migrations++;
    
    return 0;
}

/**
 * Background page migration thread
 */
static void numa_migration_worker(void) {
    if (!g_numa_allocator.enabled) {
        return;
    }
    
    /* TODO: Implement background migration logic */
    /* Scan pages, check access patterns, migrate if beneficial */
    
    debug_print("NUMA: Migration worker running\n");
}

/* ========================== Statistics and Monitoring ========================== */

/**
 * Update NUMA statistics
 */
static void update_numa_stats(int node, bool is_local_alloc) {
    if (node < 0 || node >= g_numa_allocator.num_nodes) {
        return;
    }
    
    numa_node_t* numa_node = &g_numa_allocator.nodes[node];
    
    if (is_local_alloc) {
        numa_node->stats.local_allocs++;
    } else {
        numa_node->stats.remote_allocs++;
    }
    
    /* Calculate load factor */
    uint64_t total_pages = numa_node->stats.total_pages;
    uint64_t used_pages = total_pages - numa_node->stats.free_pages;
    numa_node->stats.load_factor = (used_pages * 100) / total_pages;
    
    /* Update pressure level */
    if (numa_node->stats.load_factor > 90) {
        numa_node->stats.pressure_level = 3;  /* High pressure */
    } else if (numa_node->stats.load_factor > 70) {
        numa_node->stats.pressure_level = 2;  /* Medium pressure */
    } else if (numa_node->stats.load_factor > 50) {
        numa_node->stats.pressure_level = 1;  /* Low pressure */
    } else {
        numa_node->stats.pressure_level = 0;  /* No pressure */
    }
}

/**
 * Print NUMA statistics
 */
void numa_print_stats(void) {
    if (!g_numa_allocator.enabled) {
        debug_print("NUMA: Allocator disabled\n");
        return;
    }
    
    debug_print("NUMA Allocator Statistics:\n");
    debug_print("  Nodes: %d\n", g_numa_allocator.num_nodes);
    debug_print("  Total allocations: %lu\n", g_numa_allocator.global_stats.total_allocations);
    debug_print("  Local allocations: %lu\n", g_numa_allocator.global_stats.local_allocations);
    debug_print("  Remote allocations: %lu\n", g_numa_allocator.global_stats.remote_allocations);
    debug_print("  Migrations: %lu\n", g_numa_allocator.global_stats.migrations);
    debug_print("  Fallback allocations: %lu\n", g_numa_allocator.global_stats.fallback_allocs);
    
    for (int node = 0; node < g_numa_allocator.num_nodes; node++) {
        numa_node_t* numa_node = &g_numa_allocator.nodes[node];
        if (!numa_node->online) {
            continue;
        }
        
        debug_print("  Node %d:\n", node);
        debug_print("    Local allocs: %lu\n", numa_node->stats.local_allocs);
        debug_print("    Remote allocs: %lu\n", numa_node->stats.remote_allocs);
        debug_print("    Migrations: %lu\n", numa_node->stats.migration_count);
        debug_print("    Load factor: %u%%\n", numa_node->stats.load_factor);
        debug_print("    Pressure level: %u\n", numa_node->stats.pressure_level);
        debug_print("    Free pages: %lu/%lu\n", numa_node->stats.free_pages, numa_node->stats.total_pages);
    }
}

/* ========================== Public API Implementation ========================== */

/**
 * Initialize the NUMA allocator
 */
int numa_allocator_init(void) {
    if (g_numa_allocator.initialized) {
        return 0;
    }
    
    debug_print("NUMA Allocator: Initializing\n");
    
    /* Initialize global state */
    memset(&g_numa_allocator, 0, sizeof(g_numa_allocator));
    
    /* Detect NUMA topology */
    g_numa_allocator.num_nodes = 1;  /* Default to 1 node */
    g_numa_allocator.max_node_id = 0;
    
    int ret = detect_numa_topology();
    if (ret != 0) {
        debug_print("NUMA: Failed to detect topology, using single node\n");
        g_numa_allocator.num_nodes = 1;
    }
    
    /* Initialize node allocators */
    ret = init_node_allocators();
    if (ret != 0) {
        debug_print("NUMA: Failed to initialize node allocators\n");
        return ret;
    }
    
    /* Set default policy */
    g_numa_allocator.default_policy = NUMA_POLICY_PREFERRED;
    g_numa_allocator.interleave_node = 0;
    
    /* Enable NUMA if we have multiple nodes */
    g_numa_allocator.enabled = (g_numa_allocator.num_nodes > 1);
    g_numa_allocator.initialized = true;
    g_numa_allocator.debug_mode = false;
    
    debug_print("NUMA Allocator: Initialized with %d nodes (enabled: %s)\n",
                g_numa_allocator.num_nodes, g_numa_allocator.enabled ? "yes" : "no");
    
    return 0;
}

/**
 * Shutdown the NUMA allocator
 */
void numa_allocator_shutdown(void) {
    if (!g_numa_allocator.initialized) {
        return;
    }
    
    debug_print("NUMA Allocator: Shutting down\n");
    
    /* Print final statistics */
    numa_print_stats();
    
    /* Free per-CPU caches */
    for (int cpu = 0; cpu < MAX_CPUS; cpu++) {
        numa_cpu_cache_t* cache = &g_numa_allocator.cpu_caches[cpu];
        if (cache->local_cache) {
            kfree(cache->local_cache);
            cache->local_cache = NULL;
        }
        if (cache->remote_cache) {
            kfree(cache->remote_cache);
            cache->remote_cache = NULL;
        }
    }
    
    /* TODO: Cleanup node-specific allocators */
    
    g_numa_allocator.initialized = false;
    g_numa_allocator.enabled = false;
}

/**
 * Set NUMA policy for current process
 */
int numa_set_policy(numa_policy_t policy) {
    if (!g_numa_allocator.enabled) {
        return -1;  /* NUMA not available */
    }
    
    /* TODO: Implement per-process policy storage */
    g_numa_allocator.default_policy = policy;
    
    debug_print("NUMA: Set policy to %d\n", policy);
    return 0;
}

/**
 * Get current NUMA policy
 */
numa_policy_t numa_get_policy(void) {
    return g_numa_allocator.default_policy;
}

/* ========================== Placeholder Functions ========================== */

/**
 * Placeholder debug print function
 */
static void debug_print(const char* format, ...) {
    /* TODO: Implement actual debug printing */
    (void)format;
}

/**
 * Placeholder RDTSC function
 */
static uint64_t get_rdtsc(void) {
    /* TODO: Implement actual timestamp counter reading */
    return 0;
}

/**
 * Placeholder CPU detection function
 */
static int get_current_cpu(void) {
    /* TODO: Implement actual CPU detection */
    return 0;
}
