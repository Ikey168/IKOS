/* IKOS Slab Allocator Implementation - Issue #27
 * Provides efficient allocation for kernel objects with per-CPU caches
 */

#include "memory_advanced.h"
#include "memory.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ========================== Constants and Configuration ========================== */

#define SLAB_MAGIC          0xCAFEBABE  /* Magic number for validation */
#define MAX_CACHES          128         /* Maximum number of caches */
#define MAX_CPUS            32          /* Maximum number of CPUs */
#define SLAB_MIN_ALIGN      8           /* Minimum object alignment */
#define SLAB_MAX_SIZE       (PAGE_SIZE / 2) /* Maximum object size for slab */

/* Slab states */
#define SLAB_FULL           0x01        /* All objects allocated */
#define SLAB_PARTIAL        0x02        /* Some objects allocated */
#define SLAB_EMPTY          0x04        /* No objects allocated */

/* Cache coloring for better cache performance */
#define COLOUR_ALIGN        64          /* Cache line size */
#define MAX_COLOUR          16          /* Maximum color offset */

/* ========================== Internal Data Structures ========================== */

/* Slab descriptor */
typedef struct slab {
    struct kmem_cache*     cache;                /* Parent cache */
    void*                  s_mem;                /* Memory for objects */
    uint32_t               inuse;                /* Objects in use */
    uint32_t               free;                 /* Free objects count */
    uint32_t               colour_off;           /* Cache coloring offset */
    
    /* Free object tracking */
    void*                  freelist;             /* First free object */
    
    /* List management */
    struct slab*           next;                 /* Next slab in list */
    struct slab*           prev;                 /* Previous slab in list */
    
    /* Debugging */
    uint32_t               magic;                /* Magic number */
    
#ifdef SLAB_DEBUG
    void**                 caller_addrs;         /* Allocation callers */
    uint64_t*              alloc_times;          /* Allocation timestamps */
#endif
} slab_t;

/* Per-CPU cache structure */
typedef struct cpu_cache {
    void**                 avail;                /* Available objects array */
    uint32_t               avail_count;          /* Number of available objects */
    uint32_t               limit;                /* Cache limit */
    uint32_t               batchcount;           /* Transfer batch size */
    
    /* Statistics */
    uint64_t               allocs;               /* Allocations from this cache */
    uint64_t               frees;                /* Frees to this cache */
    uint64_t               transfers_in;         /* Transfers from shared cache */
    uint64_t               transfers_out;        /* Transfers to shared cache */
} cpu_cache_t;

/* ========================== Global State ========================== */

static kmem_cache_t* cache_chain = NULL;       /* List of all caches */
static kmem_cache_t cache_cache;                /* Cache for cache descriptors */
static bool slab_initialized = false;
static uint32_t cache_count = 0;
static volatile int slab_lock = 0;

/* Current cache coloring offset */
static uint32_t cache_colour = 0;

/* Slab statistics */
static struct slab_stats {
    uint64_t total_caches;
    uint64_t total_slabs;
    uint64_t total_objects;
    uint64_t allocated_objects;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t slab_allocations;
    uint64_t slab_frees;
} slab_statistics = {0};

/* ========================== Helper Functions ========================== */

static inline void slab_lock_cache(kmem_cache_t* cache) {
    while (__sync_lock_test_and_set(&cache->lock, 1)) {
        /* Spin wait */
    }
}

static inline void slab_unlock_cache(kmem_cache_t* cache) {
    __sync_lock_release(&cache->lock);
}

static inline void slab_global_lock(void) {
    while (__sync_lock_test_and_set(&slab_lock, 1)) {
        /* Spin wait */
    }
}

static inline void slab_global_unlock(void) {
    __sync_lock_release(&slab_lock);
}

static inline uint32_t get_current_cpu(void) {
    /* TODO: Implement proper CPU identification */
    return 0;
}

static void debug_print(const char* format, ...) {
    /* Simple debug printing - would integrate with kernel logging */
    va_list args;
    va_start(args, format);
    /* TODO: Integrate with kernel_log system */
    va_end(args);
}

/* ========================== Object Management ========================== */

/**
 * Get next free object from slab freelist
 */
static void* get_free_object(slab_t* slab) {
    if (!slab || !slab->freelist) {
        return NULL;
    }
    
    void* obj = slab->freelist;
    
    /* Update freelist to next free object */
    slab->freelist = *(void**)obj;
    slab->inuse++;
    slab->free--;
    
    return obj;
}

/**
 * Return object to slab freelist
 */
static void put_free_object(slab_t* slab, void* obj) {
    if (!slab || !obj) {
        return;
    }
    
    /* Add object to head of freelist */
    *(void**)obj = slab->freelist;
    slab->freelist = obj;
    slab->inuse--;
    slab->free++;
}

/**
 * Initialize objects in a new slab
 */
static void init_slab_objects(slab_t* slab) {
    if (!slab || !slab->cache) {
        return;
    }
    
    kmem_cache_t* cache = slab->cache;
    uint32_t objects_per_slab = (PAGE_SIZE - sizeof(slab_t)) / cache->object_size;
    void* obj_start = (char*)slab->s_mem;
    
    /* Initialize freelist */
    slab->freelist = obj_start;
    slab->free = objects_per_slab;
    slab->inuse = 0;
    
    /* Link all objects in freelist */
    for (uint32_t i = 0; i < objects_per_slab - 1; i++) {
        void* current = (char*)obj_start + i * cache->object_size;
        void* next = (char*)obj_start + (i + 1) * cache->object_size;
        *(void**)current = next;
    }
    
    /* Last object points to NULL */
    void* last = (char*)obj_start + (objects_per_slab - 1) * cache->object_size;
    *(void**)last = NULL;
    
    /* Call constructor for each object if available */
    if (cache->ctor) {
        for (uint32_t i = 0; i < objects_per_slab; i++) {
            void* obj = (char*)obj_start + i * cache->object_size;
            cache->ctor(obj);
        }
    }
    
    debug_print("Slab: Initialized %u objects in slab %p\n", objects_per_slab, slab);
}

/* ========================== Slab Management ========================== */

/**
 * Create a new slab
 */
static slab_t* create_slab(kmem_cache_t* cache, gfp_t gfp_flags) {
    if (!cache) {
        return NULL;
    }
    
    /* Allocate memory for the slab */
    struct page* page = alloc_pages(gfp_flags, 0);
    if (!page) {
        return NULL;
    }
    
    /* Slab descriptor at the end of the page */
    slab_t* slab = (slab_t*)((char*)page + PAGE_SIZE - sizeof(slab_t));
    
    /* Initialize slab descriptor */
    slab->cache = cache;
    slab->s_mem = page;
    slab->colour_off = cache_colour;
    slab->next = NULL;
    slab->prev = NULL;
    slab->magic = SLAB_MAGIC;
    
    /* Update cache coloring */
    cache_colour = (cache_colour + COLOUR_ALIGN) % (MAX_COLOUR * COLOUR_ALIGN);
    
    /* Initialize objects */
    init_slab_objects(slab);
    
    slab_statistics.total_slabs++;
    slab_statistics.total_objects += slab->free;
    
    debug_print("Slab: Created new slab %p for cache %s\n", slab, cache->name);
    
    return slab;
}

/**
 * Destroy a slab
 */
static void destroy_slab(slab_t* slab) {
    if (!slab || slab->magic != SLAB_MAGIC) {
        return;
    }
    
    kmem_cache_t* cache = slab->cache;
    
    /* Call destructor for all objects if available */
    if (cache->dtor) {
        uint32_t objects_per_slab = (PAGE_SIZE - sizeof(slab_t)) / cache->object_size;
        void* obj_start = slab->s_mem;
        
        for (uint32_t i = 0; i < objects_per_slab; i++) {
            void* obj = (char*)obj_start + i * cache->object_size;
            cache->dtor(obj);
        }
    }
    
    /* Free the page */
    __free_pages((struct page*)slab->s_mem, 0);
    
    slab_statistics.total_slabs--;
    slab_statistics.slab_frees++;
    
    debug_print("Slab: Destroyed slab %p from cache %s\n", slab, cache->name);
}

/**
 * Add slab to appropriate list
 */
static void add_slab_to_list(kmem_cache_t* cache, slab_t* slab) {
    if (!cache || !slab) {
        return;
    }
    
    if (slab->free == 0) {
        /* Add to full list */
        slab->next = cache->slabs_full;
        if (cache->slabs_full) {
            cache->slabs_full->prev = slab;
        }
        cache->slabs_full = slab;
    } else if (slab->inuse == 0) {
        /* Add to empty list */
        slab->next = cache->slabs_empty;
        if (cache->slabs_empty) {
            cache->slabs_empty->prev = slab;
        }
        cache->slabs_empty = slab;
    } else {
        /* Add to partial list */
        slab->next = cache->slabs_partial;
        if (cache->slabs_partial) {
            cache->slabs_partial->prev = slab;
        }
        cache->slabs_partial = slab;
    }
}

/**
 * Remove slab from its current list
 */
static void remove_slab_from_list(kmem_cache_t* cache, slab_t* slab) {
    if (!cache || !slab) {
        return;
    }
    
    /* Remove from linked list */
    if (slab->prev) {
        slab->prev->next = slab->next;
    }
    if (slab->next) {
        slab->next->prev = slab->prev;
    }
    
    /* Update head pointers */
    if (cache->slabs_full == slab) {
        cache->slabs_full = slab->next;
    } else if (cache->slabs_partial == slab) {
        cache->slabs_partial = slab->next;
    } else if (cache->slabs_empty == slab) {
        cache->slabs_empty = slab->next;
    }
    
    slab->next = NULL;
    slab->prev = NULL;
}

/* ========================== Per-CPU Cache Management ========================== */

/**
 * Refill per-CPU cache from shared slabs
 */
static int refill_cpu_cache(kmem_cache_t* cache, cpu_cache_t* cpu_cache, gfp_t gfp_flags) {
    if (!cache || !cpu_cache) {
        return -1;
    }
    
    slab_lock_cache(cache);
    
    /* Try to get objects from partial slabs first */
    slab_t* slab = cache->slabs_partial;
    if (!slab) {
        /* Create a new slab if no partial slabs available */
        slab = create_slab(cache, gfp_flags);
        if (!slab) {
            slab_unlock_cache(cache);
            return -1;
        }
        add_slab_to_list(cache, slab);
    }
    
    /* Transfer objects to CPU cache */
    uint32_t transfer_count = 0;
    uint32_t max_transfer = cpu_cache->limit - cpu_cache->avail_count;
    
    while (transfer_count < max_transfer && transfer_count < cpu_cache->batchcount && slab->free > 0) {
        void* obj = get_free_object(slab);
        if (!obj) {
            break;
        }
        
        cpu_cache->avail[cpu_cache->avail_count++] = obj;
        transfer_count++;
    }
    
    /* Move slab to appropriate list if state changed */
    if (slab->free == 0) {
        remove_slab_from_list(cache, slab);
        add_slab_to_list(cache, slab);
    }
    
    cpu_cache->transfers_in += transfer_count;
    
    slab_unlock_cache(cache);
    
    debug_print("Slab: Refilled CPU cache with %u objects\n", transfer_count);
    
    return transfer_count;
}

/**
 * Free objects from per-CPU cache back to shared slabs
 */
static void free_cpu_cache_objects(kmem_cache_t* cache, cpu_cache_t* cpu_cache) {
    if (!cache || !cpu_cache || cpu_cache->avail_count == 0) {
        return;
    }
    
    slab_lock_cache(cache);
    
    /* Free half of the objects to maintain cache efficiency */
    uint32_t free_count = cpu_cache->avail_count / 2;
    
    for (uint32_t i = 0; i < free_count; i++) {
        void* obj = cpu_cache->avail[--cpu_cache->avail_count];
        
        /* Find which slab this object belongs to */
        /* TODO: Implement more efficient slab lookup */
        slab_t* slab = cache->slabs_partial;
        bool found = false;
        
        /* Check partial slabs */
        while (slab && !found) {
            if (obj >= slab->s_mem && obj < (char*)slab->s_mem + PAGE_SIZE) {
                found = true;
                break;
            }
            slab = slab->next;
        }
        
        /* Check full slabs */
        if (!found) {
            slab = cache->slabs_full;
            while (slab && !found) {
                if (obj >= slab->s_mem && obj < (char*)slab->s_mem + PAGE_SIZE) {
                    found = true;
                    break;
                }
                slab = slab->next;
            }
        }
        
        if (found && slab) {
            /* Move slab from full to partial list if needed */
            if (slab->free == 0) {
                remove_slab_from_list(cache, slab);
            }
            
            put_free_object(slab, obj);
            
            /* Add back to appropriate list */
            if (slab->free == 1) {  /* Was full, now partial */
                add_slab_to_list(cache, slab);
            }
        }
    }
    
    cpu_cache->transfers_out += free_count;
    
    slab_unlock_cache(cache);
    
    debug_print("Slab: Freed %u objects from CPU cache\n", free_count);
}

/* ========================== Public API Implementation ========================== */

/**
 * Create a new slab cache
 */
kmem_cache_t* kmem_cache_create(const char* name, size_t size, 
                                size_t align, slab_flags_t flags,
                                void (*ctor)(void*)) {
    if (!name || size == 0 || size > SLAB_MAX_SIZE || !slab_initialized) {
        return NULL;
    }
    
    /* Allocate cache descriptor */
    kmem_cache_t* cache = (kmem_cache_t*)kmem_cache_alloc(&cache_cache, GFP_KERNEL);
    if (!cache) {
        return NULL;
    }
    
    /* Initialize cache */
    strncpy(cache->name, name, sizeof(cache->name) - 1);
    cache->name[sizeof(cache->name) - 1] = '\0';
    
    cache->object_size = size;
    cache->align = align > SLAB_MIN_ALIGN ? align : SLAB_MIN_ALIGN;
    cache->flags = flags;
    cache->ctor = ctor;
    cache->dtor = NULL;  /* Not implemented yet */
    
    /* Initialize slab lists */
    cache->slabs_full = NULL;
    cache->slabs_partial = NULL;
    cache->slabs_empty = NULL;
    
    /* Initialize per-CPU caches */
    for (int i = 0; i < MAX_CPUS; i++) {
        cpu_cache_t* cpu_cache = &cache->percpu_cache[i];
        cpu_cache->limit = 64;  /* Default limit */
        cpu_cache->batchcount = 16;  /* Default batch size */
        cpu_cache->avail_count = 0;
        cpu_cache->avail = (void**)kmalloc(cpu_cache->limit * sizeof(void*), GFP_KERNEL);
        
        if (!cpu_cache->avail) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) {
                kfree(cache->percpu_cache[j].avail);
            }
            kmem_cache_free(&cache_cache, cache);
            return NULL;
        }
        
        memset(&cpu_cache->allocs, 0, sizeof(cpu_cache->allocs));
    }
    
    /* Initialize statistics */
    memset(&cache->stats, 0, sizeof(cache->stats));
    
    cache->lock = 0;
    
    /* Add to global cache list */
    slab_global_lock();
    cache->next = cache_chain;
    cache_chain = cache;
    cache_count++;
    slab_global_unlock();
    
    slab_statistics.total_caches++;
    
    debug_print("Slab: Created cache '%s' with object size %zu\n", name, size);
    
    return cache;
}

/**
 * Destroy a slab cache
 */
void kmem_cache_destroy(kmem_cache_t* cache) {
    if (!cache) {
        return;
    }
    
    slab_lock_cache(cache);
    
    /* Free all slabs */
    slab_t* slab = cache->slabs_full;
    while (slab) {
        slab_t* next = slab->next;
        destroy_slab(slab);
        slab = next;
    }
    
    slab = cache->slabs_partial;
    while (slab) {
        slab_t* next = slab->next;
        destroy_slab(slab);
        slab = next;
    }
    
    slab = cache->slabs_empty;
    while (slab) {
        slab_t* next = slab->next;
        destroy_slab(slab);
        slab = next;
    }
    
    /* Free per-CPU cache arrays */
    for (int i = 0; i < MAX_CPUS; i++) {
        if (cache->percpu_cache[i].avail) {
            kfree(cache->percpu_cache[i].avail);
        }
    }
    
    slab_unlock_cache(cache);
    
    /* Remove from global cache list */
    slab_global_lock();
    if (cache_chain == cache) {
        cache_chain = cache->next;
    } else {
        kmem_cache_t* prev = cache_chain;
        while (prev && prev->next != cache) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = cache->next;
        }
    }
    cache_count--;
    slab_global_unlock();
    
    /* Free cache descriptor */
    kmem_cache_free(&cache_cache, cache);
    
    slab_statistics.total_caches--;
    
    debug_print("Slab: Destroyed cache '%s'\n", cache->name);
}

/**
 * Allocate an object from a cache
 */
void* kmem_cache_alloc(kmem_cache_t* cache, gfp_t flags) {
    if (!cache) {
        return NULL;
    }
    
    uint32_t cpu = get_current_cpu();
    cpu_cache_t* cpu_cache = &cache->percpu_cache[cpu];
    
    /* Try to get object from per-CPU cache first */
    if (cpu_cache->avail_count > 0) {
        void* obj = cpu_cache->avail[--cpu_cache->avail_count];
        cpu_cache->allocs++;
        cache->stats.total_allocations++;
        slab_statistics.cache_hits++;
        return obj;
    }
    
    /* CPU cache is empty, refill it */
    slab_statistics.cache_misses++;
    
    if (refill_cpu_cache(cache, cpu_cache, flags) <= 0) {
        cache->stats.total_allocations++;  /* Count failed attempts */
        return NULL;
    }
    
    /* Now try allocation again */
    if (cpu_cache->avail_count > 0) {
        void* obj = cpu_cache->avail[--cpu_cache->avail_count];
        cpu_cache->allocs++;
        cache->stats.total_allocations++;
        slab_statistics.allocated_objects++;
        return obj;
    }
    
    return NULL;
}

/**
 * Allocate an object from a specific NUMA node
 */
void* kmem_cache_alloc_node(kmem_cache_t* cache, gfp_t flags, int node) {
    /* For now, ignore NUMA preference and use regular allocation */
    /* TODO: Implement NUMA-aware allocation */
    return kmem_cache_alloc(cache, flags);
}

/**
 * Free an object back to its cache
 */
void kmem_cache_free(kmem_cache_t* cache, void* obj) {
    if (!cache || !obj) {
        return;
    }
    
    uint32_t cpu = get_current_cpu();
    cpu_cache_t* cpu_cache = &cache->percpu_cache[cpu];
    
    /* Add object to per-CPU cache if there's space */
    if (cpu_cache->avail_count < cpu_cache->limit) {
        cpu_cache->avail[cpu_cache->avail_count++] = obj;
        cpu_cache->frees++;
        cache->stats.total_frees++;
        slab_statistics.allocated_objects--;
        
        /* Free some objects if cache is getting full */
        if (cpu_cache->avail_count >= cpu_cache->limit * 3 / 4) {
            free_cpu_cache_objects(cache, cpu_cache);
        }
        
        return;
    }
    
    /* CPU cache is full, free some objects first */
    free_cpu_cache_objects(cache, cpu_cache);
    
    /* Now add the object */
    if (cpu_cache->avail_count < cpu_cache->limit) {
        cpu_cache->avail[cpu_cache->avail_count++] = obj;
        cpu_cache->frees++;
        cache->stats.total_frees++;
        slab_statistics.allocated_objects--;
    }
}

/* ========================== Cache Information API ========================== */

/**
 * Shrink a cache by freeing empty slabs
 */
int kmem_cache_shrink(kmem_cache_t* cache) {
    if (!cache) {
        return 0;
    }
    
    slab_lock_cache(cache);
    
    int freed_slabs = 0;
    slab_t* slab = cache->slabs_empty;
    
    while (slab) {
        slab_t* next = slab->next;
        remove_slab_from_list(cache, slab);
        destroy_slab(slab);
        freed_slabs++;
        slab = next;
    }
    
    cache->slabs_empty = NULL;
    
    slab_unlock_cache(cache);
    
    debug_print("Slab: Shrunk cache '%s', freed %d empty slabs\n", cache->name, freed_slabs);
    
    return freed_slabs;
}

/**
 * Get cache name
 */
const char* kmem_cache_name(kmem_cache_t* cache) {
    return cache ? cache->name : NULL;
}

/**
 * Get cache object size
 */
size_t kmem_cache_size(kmem_cache_t* cache) {
    return cache ? cache->object_size : 0;
}

/* ========================== Initialization and Shutdown ========================== */

/**
 * Initialize the slab allocator
 */
int slab_allocator_init(void) {
    if (slab_initialized) {
        return 0;
    }
    
    /* Initialize cache_cache (bootstrap) */
    memset(&cache_cache, 0, sizeof(cache_cache));
    strcpy(cache_cache.name, "kmem_cache");
    cache_cache.object_size = sizeof(kmem_cache_t);
    cache_cache.align = SLAB_MIN_ALIGN;
    cache_cache.flags = 0;
    cache_cache.ctor = NULL;
    cache_cache.dtor = NULL;
    
    /* Initialize per-CPU caches for cache_cache */
    for (int i = 0; i < MAX_CPUS; i++) {
        cpu_cache_t* cpu_cache = &cache_cache.percpu_cache[i];
        cpu_cache->limit = 32;
        cpu_cache->batchcount = 8;
        cpu_cache->avail_count = 0;
        /* Bootstrap allocation - this will be replaced later */
        cpu_cache->avail = (void**)((char*)&cache_cache + sizeof(kmem_cache_t) + i * 32 * sizeof(void*));
        memset(&cpu_cache->allocs, 0, sizeof(cpu_cache->allocs));
    }
    
    cache_cache.lock = 0;
    cache_cache.next = NULL;
    
    /* Initialize global state */
    cache_chain = &cache_cache;
    cache_count = 1;
    cache_colour = 0;
    memset(&slab_statistics, 0, sizeof(slab_statistics));
    slab_statistics.total_caches = 1;
    
    slab_initialized = true;
    
    debug_print("Slab: Allocator initialized\n");
    
    return 0;
}

/**
 * Shutdown the slab allocator
 */
void slab_allocator_shutdown(void) {
    if (!slab_initialized) {
        return;
    }
    
    /* Print statistics */
    debug_print("Slab: Shutdown statistics:\n");
    debug_print("  Total caches: %lu\n", slab_statistics.total_caches);
    debug_print("  Total slabs: %lu\n", slab_statistics.total_slabs);
    debug_print("  Cache hits: %lu\n", slab_statistics.cache_hits);
    debug_print("  Cache misses: %lu\n", slab_statistics.cache_misses);
    debug_print("  Objects allocated: %lu\n", slab_statistics.allocated_objects);
    
    /* Destroy all caches except cache_cache */
    kmem_cache_t* cache = cache_chain;
    while (cache && cache != &cache_cache) {
        kmem_cache_t* next = cache->next;
        kmem_cache_destroy(cache);
        cache = next;
    }
    
    slab_initialized = false;
    
    debug_print("Slab: Allocator shutdown complete\n");
}

/* ========================== Statistics and Debugging ========================== */

/**
 * Get slab allocator statistics
 */
void slab_get_stats(struct slab_allocator_stats* stats) {
    if (!stats) {
        return;
    }
    
    stats->total_caches = slab_statistics.total_caches;
    stats->total_slabs = slab_statistics.total_slabs;
    stats->total_objects = slab_statistics.total_objects;
    stats->allocated_objects = slab_statistics.allocated_objects;
    stats->cache_hits = slab_statistics.cache_hits;
    stats->cache_misses = slab_statistics.cache_misses;
    stats->hit_ratio = slab_statistics.cache_hits + slab_statistics.cache_misses > 0 ?
                       (slab_statistics.cache_hits * 100) / 
                       (slab_statistics.cache_hits + slab_statistics.cache_misses) : 0;
}

/* Temporary stub for missing structure */
struct slab_allocator_stats {
    uint64_t total_caches;
    uint64_t total_slabs;
    uint64_t total_objects;
    uint64_t allocated_objects;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint32_t hit_ratio;
};
