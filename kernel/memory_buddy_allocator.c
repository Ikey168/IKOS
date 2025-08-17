/* IKOS Buddy Allocator Implementation - Issue #27
 * Physical page allocation with fragmentation reduction
 */

#include "memory_advanced.h"
#include "process.h"
#include <string.h>
#include <stdint.h>

/* Forward declarations */
static void debug_print(const char* format, ...);

/* ========================== Buddy Allocator Constants ========================== */

#define MAX_ORDER               10              /* Maximum allocation order (1024 pages) */
#define BUDDY_MAGIC             0xBUDDY123      /* Magic number for validation */
#define PAGES_PER_SECTION       1024            /* Pages per memory section */

/* Buddy allocator state */
static memory_zone_t* memory_zones[MAX_NR_ZONES];
static uint32_t zone_count = 0;
static bool buddy_initialized = false;

/* Global buddy statistics */
static struct buddy_stats {
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t failed_allocations;
    uint64_t coalescing_operations;
    uint64_t fragmentation_events;
    uint64_t peak_usage;
    uint64_t current_usage;
} buddy_stats = {0};

/* ========================== Page Structure Extensions ========================== */

/* Extended page structure for buddy allocation */
struct page {
    uint32_t flags;                     /* Page flags */
    uint32_t order;                     /* Allocation order */
    uint64_t buddy_magic;               /* Magic number for validation */
    
    /* Buddy allocator management */
    struct page* next_free;             /* Next free page */
    struct page* prev_free;             /* Previous free page */
    
    /* Reference counting */
    atomic_t ref_count;                 /* Reference count */
    
    /* Memory zone */
    memory_zone_t* zone;                /* Containing zone */
    
    /* Physical address */
    uint64_t physical_addr;             /* Physical address */
    
    /* Allocation tracking */
    uint64_t alloc_time;                /* Allocation timestamp */
    void* alloc_caller;                 /* Allocation caller address */
} __attribute__((packed));

/* ========================== Helper Functions ========================== */

/**
 * Get buddy page for a given page and order
 */
static struct page* get_buddy_page(struct page* page, unsigned int order) {
    uint64_t page_idx = page - (struct page*)0;  /* Simplified page index */
    uint64_t buddy_idx = page_idx ^ (1UL << order);
    return (struct page*)buddy_idx;
}

/**
 * Check if two pages are buddies
 */
static bool is_buddy_page(struct page* page, struct page* buddy, unsigned int order) {
    if (!page || !buddy) return false;
    
    /* Both pages must be free and of the same order */
    if (!(page->flags & (1 << 0)) || !(buddy->flags & (1 << 0))) return false;
    if (page->order != order || buddy->order != order) return false;
    
    /* Check buddy relationship */
    uint64_t page_idx = page - (struct page*)0;
    uint64_t buddy_idx = buddy - (struct page*)0;
    
    return (page_idx ^ buddy_idx) == (1UL << order);
}

/**
 * Mark page as allocated
 */
static void mark_page_allocated(struct page* page, unsigned int order) {
    page->flags &= ~(1 << 0);           /* Clear free flag */
    page->order = order;
    page->buddy_magic = BUDDY_MAGIC;
    page->alloc_time = get_system_time();
    page->ref_count.counter = 1;
}

/**
 * Mark page as free
 */
static void mark_page_free(struct page* page, unsigned int order) {
    page->flags |= (1 << 0);            /* Set free flag */
    page->order = order;
    page->buddy_magic = BUDDY_MAGIC;
    page->ref_count.counter = 0;
}

/**
 * Add page to free list
 */
static void add_to_free_list(memory_zone_t* zone, struct page* page, unsigned int order) {
    if (!zone || !page || order > MAX_ORDER) return;
    
    /* Add to head of free list */
    page->next_free = zone->free_area[order].free_list;
    page->prev_free = NULL;
    
    if (zone->free_area[order].free_list) {
        zone->free_area[order].free_list->prev_free = page;
    }
    
    zone->free_area[order].free_list = page;
    zone->free_area[order].nr_free++;
    zone->free_pages++;
    
    mark_page_free(page, order);
}

/**
 * Remove page from free list
 */
static void remove_from_free_list(memory_zone_t* zone, struct page* page, unsigned int order) {
    if (!zone || !page || order > MAX_ORDER) return;
    
    /* Remove from linked list */
    if (page->prev_free) {
        page->prev_free->next_free = page->next_free;
    } else {
        zone->free_area[order].free_list = page->next_free;
    }
    
    if (page->next_free) {
        page->next_free->prev_free = page->prev_free;
    }
    
    zone->free_area[order].nr_free--;
    zone->free_pages--;
    
    page->next_free = NULL;
    page->prev_free = NULL;
}

/**
 * Split a higher order page into smaller pages
 */
static struct page* split_page(memory_zone_t* zone, struct page* page, 
                              unsigned int high_order, unsigned int low_order) {
    if (!zone || !page || high_order <= low_order) return NULL;
    
    unsigned int current_order = high_order;
    
    while (current_order > low_order) {
        current_order--;
        
        /* Get buddy of the current page */
        struct page* buddy = get_buddy_page(page, current_order);
        
        /* Add buddy to free list at current order */
        add_to_free_list(zone, buddy, current_order);
        
        debug_print("Buddy: Split page order %u -> %u, buddy at %p\n", 
                   current_order + 1, current_order, buddy);
    }
    
    return page;
}

/**
 * Try to coalesce free buddy pages
 */
static struct page* coalesce_buddies(memory_zone_t* zone, struct page* page, 
                                    unsigned int order) {
    if (!zone || !page || order >= MAX_ORDER) return page;
    
    buddy_stats.coalescing_operations++;
    
    while (order < MAX_ORDER) {
        struct page* buddy = get_buddy_page(page, order);
        
        /* Check if buddy is free and can be coalesced */
        if (!is_buddy_page(page, buddy, order)) break;
        
        /* Remove buddy from free list */
        remove_from_free_list(zone, buddy, order);
        
        /* Determine which page becomes the new head */
        if (buddy < page) {
            page = buddy;
        }
        
        order++;
        
        debug_print("Buddy: Coalesced pages at order %u\n", order - 1);
    }
    
    return page;
}

/**
 * Find zone for allocation based on flags
 */
static memory_zone_t* select_zone(gfp_t gfp_flags) {
    if (gfp_flags & GFP_DMA) {
        return memory_zones[ZONE_DMA];
    } else if (gfp_flags & GFP_HIGHMEM) {
        return memory_zones[ZONE_HIGHMEM];
    } else {
        return memory_zones[ZONE_NORMAL];
    }
}

/**
 * Check if allocation can be satisfied
 */
static bool can_allocate(memory_zone_t* zone, unsigned int order, gfp_t gfp_flags) {
    if (!zone) return false;
    
    /* Check if we have free pages of the requested order or higher */
    for (unsigned int i = order; i <= MAX_ORDER; i++) {
        if (zone->free_area[i].nr_free > 0) {
            return true;
        }
    }
    
    /* Check watermarks */
    if (zone->free_pages < zone->watermark_min) {
        if (!(gfp_flags & GFP_ATOMIC)) {
            return false;  /* Would need memory reclaim */
        }
    }
    
    return false;
}

/* ========================== Public Buddy Allocator API ========================== */

/**
 * Initialize buddy allocator
 */
int buddy_allocator_init(void) {
    if (buddy_initialized) {
        return 0;
    }
    
    /* Initialize memory zones */
    for (int i = 0; i < MAX_NR_ZONES; i++) {
        memory_zones[i] = NULL;
    }
    
    /* Reset statistics */
    memset(&buddy_stats, 0, sizeof(buddy_stats));
    
    buddy_initialized = true;
    debug_print("Buddy: Allocator initialized\n");
    
    return 0;
}

/**
 * Add memory zone to buddy allocator
 */
int buddy_add_zone(uint64_t start_pfn, uint64_t end_pfn, zone_type_t type, int numa_node) {
    if (!buddy_initialized || type >= MAX_NR_ZONES) {
        return -EINVAL;
    }
    
    /* Allocate zone structure */
    memory_zone_t* zone = (memory_zone_t*)kmalloc(sizeof(memory_zone_t), GFP_KERNEL);
    if (!zone) {
        return -ENOMEM;
    }
    
    /* Initialize zone */
    memset(zone, 0, sizeof(memory_zone_t));
    zone->start_pfn = start_pfn;
    zone->end_pfn = end_pfn;
    zone->type = type;
    zone->numa_node = numa_node;
    zone->total_pages = end_pfn - start_pfn;
    zone->free_pages = 0;
    
    /* Set watermarks (simplified) */
    zone->watermark_min = zone->total_pages / 256;   /* 0.4% */
    zone->watermark_low = zone->total_pages / 128;   /* 0.8% */
    zone->watermark_high = zone->total_pages / 64;   /* 1.6% */
    
    /* Initialize free areas */
    for (int i = 0; i <= MAX_ORDER; i++) {
        zone->free_area[i].free_list = NULL;
        zone->free_area[i].nr_free = 0;
    }
    
    memory_zones[type] = zone;
    zone_count++;
    
    debug_print("Buddy: Added zone type %d, PFN %lu-%lu, NUMA node %d\n",
               type, start_pfn, end_pfn, numa_node);
    
    return 0;
}

/**
 * Allocate pages using buddy allocator
 */
struct page* buddy_alloc_pages(gfp_t gfp_mask, unsigned int order) {
    if (!buddy_initialized || order > MAX_ORDER) {
        buddy_stats.failed_allocations++;
        return NULL;
    }
    
    buddy_stats.total_allocations++;
    
    /* Select appropriate zone */
    memory_zone_t* zone = select_zone(gfp_mask);
    if (!zone) {
        buddy_stats.failed_allocations++;
        return NULL;
    }
    
    /* Check if allocation is possible */
    if (!can_allocate(zone, order, gfp_mask)) {
        buddy_stats.failed_allocations++;
        return NULL;
    }
    
    /* Find the smallest available order that can satisfy the request */
    struct page* page = NULL;
    unsigned int current_order = order;
    
    while (current_order <= MAX_ORDER) {
        if (zone->free_area[current_order].nr_free > 0) {
            page = zone->free_area[current_order].free_list;
            break;
        }
        current_order++;
    }
    
    if (!page) {
        buddy_stats.failed_allocations++;
        return NULL;
    }
    
    /* Remove page from free list */
    remove_from_free_list(zone, page, current_order);
    
    /* Split page if necessary */
    if (current_order > order) {
        page = split_page(zone, page, current_order, order);
    }
    
    /* Mark page as allocated */
    mark_page_allocated(page, order);
    
    /* Update statistics */
    uint64_t allocated_pages = 1UL << order;
    buddy_stats.current_usage += allocated_pages;
    if (buddy_stats.current_usage > buddy_stats.peak_usage) {
        buddy_stats.peak_usage = buddy_stats.current_usage;
    }
    
    zone->stats.allocations++;
    
    debug_print("Buddy: Allocated %lu pages at order %u, page %p\n",
               allocated_pages, order, page);
    
    return page;
}

/**
 * Free pages using buddy allocator
 */
void buddy_free_pages(struct page* page, unsigned int order) {
    if (!buddy_initialized || !page || order > MAX_ORDER) {
        return;
    }
    
    if (page->buddy_magic != BUDDY_MAGIC) {
        debug_print("Buddy: Invalid magic number in page %p\n", page);
        return;
    }
    
    buddy_stats.total_frees++;
    
    memory_zone_t* zone = page->zone;
    if (!zone) {
        debug_print("Buddy: Page %p has no zone\n", page);
        return;
    }
    
    /* Coalesce with buddies */
    page = coalesce_buddies(zone, page, order);
    
    /* Add to free list */
    add_to_free_list(zone, page, page->order);
    
    /* Update statistics */
    uint64_t freed_pages = 1UL << order;
    buddy_stats.current_usage -= freed_pages;
    
    debug_print("Buddy: Freed %lu pages at order %u, page %p\n",
               freed_pages, order, page);
}

/**
 * Get buddy allocator statistics
 */
void buddy_get_stats(struct buddy_allocator_stats* stats) {
    if (!stats) return;
    
    stats->total_allocations = buddy_stats.total_allocations;
    stats->total_frees = buddy_stats.total_frees;
    stats->failed_allocations = buddy_stats.failed_allocations;
    stats->coalescing_operations = buddy_stats.coalescing_operations;
    stats->fragmentation_events = buddy_stats.fragmentation_events;
    stats->peak_usage = buddy_stats.peak_usage;
    stats->current_usage = buddy_stats.current_usage;
    
    /* Calculate fragmentation percentage */
    uint64_t total_free = 0;
    uint64_t largest_free = 0;
    
    for (int z = 0; z < MAX_NR_ZONES; z++) {
        memory_zone_t* zone = memory_zones[z];
        if (!zone) continue;
        
        total_free += zone->free_pages;
        
        for (int o = MAX_ORDER; o >= 0; o--) {
            if (zone->free_area[o].nr_free > 0) {
                largest_free = (1UL << o);
                break;
            }
        }
    }
    
    if (total_free > 0) {
        stats->fragmentation_percentage = 
            ((total_free - largest_free) * 100) / total_free;
    } else {
        stats->fragmentation_percentage = 0;
    }
}

/**
 * Dump buddy allocator state for debugging
 */
void buddy_dump_state(void) {
    debug_print("=== Buddy Allocator State ===\n");
    debug_print("Initialized: %s\n", buddy_initialized ? "Yes" : "No");
    debug_print("Zone count: %u\n", zone_count);
    
    for (int z = 0; z < MAX_NR_ZONES; z++) {
        memory_zone_t* zone = memory_zones[z];
        if (!zone) continue;
        
        debug_print("Zone %d (type %d, NUMA %d):\n", z, zone->type, zone->numa_node);
        debug_print("  PFN range: %lu - %lu\n", zone->start_pfn, zone->end_pfn);
        debug_print("  Total pages: %lu\n", zone->total_pages);
        debug_print("  Free pages: %lu\n", zone->free_pages);
        debug_print("  Watermarks: min=%lu, low=%lu, high=%lu\n",
                   zone->watermark_min, zone->watermark_low, zone->watermark_high);
        
        debug_print("  Free areas:\n");
        for (int o = 0; o <= MAX_ORDER; o++) {
            if (zone->free_area[o].nr_free > 0) {
                debug_print("    Order %d: %lu blocks (%lu pages)\n",
                           o, zone->free_area[o].nr_free,
                           zone->free_area[o].nr_free << o);
            }
        }
    }
    
    debug_print("Statistics:\n");
    debug_print("  Total allocations: %lu\n", buddy_stats.total_allocations);
    debug_print("  Total frees: %lu\n", buddy_stats.total_frees);
    debug_print("  Failed allocations: %lu\n", buddy_stats.failed_allocations);
    debug_print("  Coalescing operations: %lu\n", buddy_stats.coalescing_operations);
    debug_print("  Current usage: %lu pages\n", buddy_stats.current_usage);
    debug_print("  Peak usage: %lu pages\n", buddy_stats.peak_usage);
}

/* ========================== Integration Functions ========================== */

/**
 * Get system time (placeholder implementation)
 */
uint64_t get_system_time(void) {
    /* TODO: Integrate with actual timer system */
    static uint64_t fake_time = 0;
    return ++fake_time;
}

/**
 * Debug print function (placeholder implementation)
 */
static void debug_print(const char* format, ...) {
    /* TODO: Integrate with actual debug/logging system */
    (void)format;  /* Suppress unused parameter warning */
}

/**
 * Placeholder kmalloc for zone allocation
 */
void* kmalloc(size_t size, gfp_t flags) {
    /* TODO: Implement proper kmalloc or use simpler allocator for bootstrap */
    static uint8_t bootstrap_memory[4096];
    static size_t bootstrap_used = 0;
    
    if (bootstrap_used + size > sizeof(bootstrap_memory)) {
        return NULL;
    }
    
    void* ptr = &bootstrap_memory[bootstrap_used];
    bootstrap_used += (size + 7) & ~7;  /* 8-byte alignment */
    return ptr;
}

/**
 * Atomic operations (simplified implementation)
 */
typedef struct {
    volatile int counter;
} atomic_t;

/**
 * Buddy allocator statistics structure
 */
struct buddy_allocator_stats {
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t failed_allocations;
    uint64_t coalescing_operations;
    uint64_t fragmentation_events;
    uint64_t peak_usage;
    uint64_t current_usage;
    uint64_t fragmentation_percentage;
};
