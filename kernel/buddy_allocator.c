/* IKOS Buddy Allocator Implementation - Issue #27
 * Provides efficient physical page allocation with fragmentation reduction
 */

#include "memory_advanced.h"
#include "memory.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ========================== Constants and Configuration ========================== */

#define MAX_ORDER           10          /* Maximum allocation order (1024 pages) */
#define PAGE_SIZE           4096        /* Standard page size */
#define BUDDY_MAGIC         0xBEEF1234  /* Magic number for validation */

/* ========================== Global State ========================== */

static memory_zone_t memory_zones[MAX_NR_ZONES];
static int num_zones = 0;
static bool buddy_initialized = false;

/* Statistics */
static struct buddy_stats {
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t allocation_failures;
    uint64_t coalescing_operations;
    uint64_t fragmentation_events;
    uint64_t zone_fallbacks;
} buddy_statistics = {0};

/* Zone preferences for different allocation types */
static zone_type_t zone_preferences[4][MAX_NR_ZONES] = {
    [GFP_KERNEL] = {ZONE_NORMAL, ZONE_DMA, ZONE_HIGHMEM, ZONE_MOVABLE},
    [GFP_DMA] = {ZONE_DMA, ZONE_NORMAL, ZONE_HIGHMEM, ZONE_MOVABLE},
    [GFP_USER] = {ZONE_HIGHMEM, ZONE_NORMAL, ZONE_MOVABLE, ZONE_DMA},
    [GFP_ATOMIC] = {ZONE_NORMAL, ZONE_DMA, ZONE_HIGHMEM, ZONE_MOVABLE}
};

/* ========================== Helper Functions ========================== */

static inline void buddy_lock_zone(memory_zone_t* zone) {
    while (__sync_lock_test_and_set(&zone->lock, 1)) {
        /* Spin wait */
    }
}

static inline void buddy_unlock_zone(memory_zone_t* zone) {
    __sync_lock_release(&zone->lock);
}

static inline unsigned long page_to_pfn(struct page* page) {
    /* Convert page structure to page frame number */
    return ((unsigned long)page - (unsigned long)memory_zones[0].start_pfn) / sizeof(struct page);
}

static inline struct page* pfn_to_page(unsigned long pfn) {
    /* Convert page frame number to page structure */
    return (struct page*)(memory_zones[0].start_pfn + pfn * sizeof(struct page));
}

static inline unsigned long buddy_index(unsigned long page_idx, unsigned int order) {
    return page_idx ^ (1UL << order);
}

static inline bool is_buddy_free(struct page* page, unsigned int order) {
    /* Check if the buddy page is free and has the same order */
    return page && (page->flags & (1 << order)) && !(page->flags & 0x1);
}

static void debug_print(const char* format, ...) {
    /* Simple debug printing - would integrate with kernel logging */
    va_list args;
    va_start(args, format);
    /* TODO: Integrate with kernel_log system */
    va_end(args);
}

/* ========================== Free Area Management ========================== */

/**
 * Add a page to the free list of specified order
 */
static void add_to_free_list(memory_zone_t* zone, struct page* page, unsigned int order) {
    if (!zone || !page || order > MAX_ORDER) {
        return;
    }
    
    /* Set page as free with specified order */
    page->flags = (1 << order);
    page->private = 0;
    
    /* Add to head of free list */
    page->next = zone->free_area[order].free_list;
    if (zone->free_area[order].free_list) {
        zone->free_area[order].free_list->prev = page;
    }
    page->prev = NULL;
    zone->free_area[order].free_list = page;
    
    /* Update counters */
    zone->free_area[order].nr_free++;
    zone->free_pages += (1UL << order);
    
    debug_print("Buddy: Added page %p to order %d free list\n", page, order);
}

/**
 * Remove a page from the free list
 */
static void remove_from_free_list(memory_zone_t* zone, struct page* page, unsigned int order) {
    if (!zone || !page || order > MAX_ORDER) {
        return;
    }
    
    /* Remove from linked list */
    if (page->prev) {
        page->prev->next = page->next;
    } else {
        zone->free_area[order].free_list = page->next;
    }
    
    if (page->next) {
        page->next->prev = page->prev;
    }
    
    /* Clear page fields */
    page->next = NULL;
    page->prev = NULL;
    page->flags = 0;
    
    /* Update counters */
    zone->free_area[order].nr_free--;
    zone->free_pages -= (1UL << order);
    
    debug_print("Buddy: Removed page %p from order %d free list\n", page, order);
}

/**
 * Find and remove a free page of specified order
 */
static struct page* get_free_page(memory_zone_t* zone, unsigned int order) {
    if (!zone || order > MAX_ORDER) {
        return NULL;
    }
    
    /* Look for a free page of the requested order */
    if (zone->free_area[order].nr_free > 0) {
        struct page* page = zone->free_area[order].free_list;
        if (page) {
            remove_from_free_list(zone, page, order);
            return page;
        }
    }
    
    return NULL;
}

/* ========================== Buddy Coalescing ========================== */

/**
 * Attempt to coalesce a page with its buddy
 */
static struct page* coalesce_buddies(memory_zone_t* zone, struct page* page, unsigned int order) {
    unsigned long page_idx = page_to_pfn(page);
    unsigned long buddy_idx;
    struct page* buddy;
    
    while (order < MAX_ORDER) {
        buddy_idx = buddy_index(page_idx, order);
        buddy = pfn_to_page(buddy_idx);
        
        /* Check if buddy is valid and free */
        if (!buddy || !is_buddy_free(buddy, order)) {
            break;
        }
        
        /* Remove buddy from free list */
        remove_from_free_list(zone, buddy, order);
        
        /* Merge with buddy - keep the lower address page */
        if (buddy_idx < page_idx) {
            page = buddy;
            page_idx = buddy_idx;
        }
        
        order++;
        buddy_statistics.coalescing_operations++;
        
        debug_print("Buddy: Coalesced pages at order %d\n", order - 1);
    }
    
    return page;
}

/**
 * Free a page and attempt coalescing
 */
static void free_page_with_coalescing(memory_zone_t* zone, struct page* page, unsigned int order) {
    if (!zone || !page) {
        return;
    }
    
    buddy_lock_zone(zone);
    
    /* Attempt to coalesce with buddies */
    page = coalesce_buddies(zone, page, order);
    
    /* Add the (possibly coalesced) page to free list */
    add_to_free_list(zone, page, order);
    
    buddy_unlock_zone(zone);
    
    buddy_statistics.total_frees++;
}

/* ========================== Page Splitting ========================== */

/**
 * Split a higher-order page into smaller pages
 */
static struct page* split_page(memory_zone_t* zone, struct page* page, 
                               unsigned int high_order, unsigned int low_order) {
    if (!zone || !page || high_order <= low_order || high_order > MAX_ORDER) {
        return NULL;
    }
    
    unsigned int current_order = high_order;
    
    while (current_order > low_order) {
        current_order--;
        
        /* Calculate buddy page */
        unsigned long page_idx = page_to_pfn(page);
        unsigned long buddy_idx = page_idx + (1UL << current_order);
        struct page* buddy = pfn_to_page(buddy_idx);
        
        if (buddy) {
            /* Add buddy to free list */
            add_to_free_list(zone, buddy, current_order);
        }
        
        debug_print("Buddy: Split page at order %d to %d\n", current_order + 1, current_order);
    }
    
    return page;
}

/* ========================== Zone Management ========================== */

/**
 * Get the appropriate zone for allocation flags
 */
static memory_zone_t* get_allocation_zone(gfp_t gfp_flags) {
    zone_type_t* preferences;
    
    /* Determine zone preferences based on GFP flags */
    if (gfp_flags & GFP_DMA) {
        preferences = zone_preferences[GFP_DMA];
    } else if (gfp_flags & GFP_USER) {
        preferences = zone_preferences[GFP_USER];
    } else if (gfp_flags & GFP_ATOMIC) {
        preferences = zone_preferences[GFP_ATOMIC];
    } else {
        preferences = zone_preferences[GFP_KERNEL];
    }
    
    /* Find the first available zone */
    for (int i = 0; i < MAX_NR_ZONES; i++) {
        for (int j = 0; j < num_zones; j++) {
            if (memory_zones[j].type == preferences[i]) {
                return &memory_zones[j];
            }
        }
    }
    
    /* Fallback to first available zone */
    return num_zones > 0 ? &memory_zones[0] : NULL;
}

/**
 * Check if zone has enough free memory
 */
static bool zone_watermark_ok(memory_zone_t* zone, unsigned int order, gfp_t gfp_flags) {
    if (!zone) {
        return false;
    }
    
    unsigned long required_pages = 1UL << order;
    unsigned long free_pages = zone->free_pages;
    unsigned long watermark;
    
    /* Select appropriate watermark */
    if (gfp_flags & GFP_ATOMIC) {
        watermark = zone->watermark_min;
    } else if (gfp_flags & GFP_NOWAIT) {
        watermark = zone->watermark_low;
    } else {
        watermark = zone->watermark_high;
    }
    
    return free_pages >= (watermark + required_pages);
}

/* ========================== Core Allocation Functions ========================== */

/**
 * Allocate pages from a specific zone
 */
static struct page* __alloc_pages_zone(memory_zone_t* zone, gfp_t gfp_flags, unsigned int order) {
    if (!zone || order > MAX_ORDER) {
        return NULL;
    }
    
    buddy_lock_zone(zone);
    
    /* Check watermarks */
    if (!zone_watermark_ok(zone, order, gfp_flags)) {
        buddy_unlock_zone(zone);
        zone->stats.failures++;
        return NULL;
    }
    
    struct page* page = NULL;
    
    /* Try to find a free page of the requested order */
    page = get_free_page(zone, order);
    
    if (!page) {
        /* Look for higher-order pages to split */
        for (unsigned int current_order = order + 1; current_order <= MAX_ORDER; current_order++) {
            page = get_free_page(zone, current_order);
            if (page) {
                /* Split the page down to the requested order */
                page = split_page(zone, page, current_order, order);
                break;
            }
        }
    }
    
    if (page) {
        /* Mark page as allocated */
        page->flags = 0x1;  /* Allocated flag */
        page->private = order;
        zone->stats.allocations++;
        buddy_statistics.total_allocations++;
        
        debug_print("Buddy: Allocated %lu pages at order %d from zone %d\n", 
                   1UL << order, order, zone->type);
    } else {
        zone->stats.failures++;
        buddy_statistics.allocation_failures++;
    }
    
    buddy_unlock_zone(zone);
    
    return page;
}

/**
 * Main page allocation function
 */
struct page* alloc_pages(gfp_t gfp_flags, unsigned int order) {
    if (!buddy_initialized || order > MAX_ORDER) {
        return NULL;
    }
    
    /* Get the preferred zone for this allocation */
    memory_zone_t* zone = get_allocation_zone(gfp_flags);
    if (!zone) {
        return NULL;
    }
    
    struct page* page = __alloc_pages_zone(zone, gfp_flags, order);
    
    /* Try fallback zones if allocation failed */
    if (!page && !(gfp_flags & GFP_NOWAIT)) {
        for (int i = 0; i < num_zones; i++) {
            if (&memory_zones[i] != zone) {
                page = __alloc_pages_zone(&memory_zones[i], gfp_flags, order);
                if (page) {
                    buddy_statistics.zone_fallbacks++;
                    break;
                }
            }
        }
    }
    
    return page;
}

/**
 * Allocate pages from a specific NUMA node
 */
struct page* alloc_pages_node(int nid, gfp_t gfp_flags, unsigned int order) {
    /* For now, ignore NUMA node preference and use regular allocation */
    /* TODO: Implement NUMA-aware allocation when NUMA support is added */
    return alloc_pages(gfp_flags, order);
}

/**
 * Free allocated pages
 */
void __free_pages(struct page* page, unsigned int order) {
    if (!page || !buddy_initialized || order > MAX_ORDER) {
        return;
    }
    
    /* Find the zone this page belongs to */
    memory_zone_t* zone = NULL;
    unsigned long pfn = page_to_pfn(page);
    
    for (int i = 0; i < num_zones; i++) {
        if (pfn >= memory_zones[i].start_pfn && pfn <= memory_zones[i].end_pfn) {
            zone = &memory_zones[i];
            break;
        }
    }
    
    if (!zone) {
        debug_print("Buddy: Error - Cannot find zone for page %p\n", page);
        return;
    }
    
    /* Free the page with coalescing */
    free_page_with_coalescing(zone, page, order);
    
    debug_print("Buddy: Freed %lu pages at order %d\n", 1UL << order, order);
}

/* ========================== Convenience Functions ========================== */

/**
 * Allocate a single page
 */
unsigned long __get_free_page(gfp_t gfp_flags) {
    struct page* page = alloc_pages(gfp_flags, 0);
    return page ? (unsigned long)page : 0;
}

/**
 * Allocate multiple pages (power of 2)
 */
unsigned long __get_free_pages(gfp_t gfp_flags, unsigned int order) {
    struct page* page = alloc_pages(gfp_flags, order);
    return page ? (unsigned long)page : 0;
}

/**
 * Free a single page
 */
void free_page(unsigned long addr) {
    if (addr) {
        __free_pages((struct page*)addr, 0);
    }
}

/**
 * Free multiple pages
 */
void free_pages(unsigned long addr, unsigned int order) {
    if (addr) {
        __free_pages((struct page*)addr, order);
    }
}

/* ========================== Zone Management API ========================== */

/**
 * Add a memory zone to the buddy allocator
 */
int buddy_add_zone(uint64_t start_pfn, uint64_t end_pfn, zone_type_t type) {
    if (num_zones >= MAX_NR_ZONES || start_pfn >= end_pfn) {
        return -1;
    }
    
    memory_zone_t* zone = &memory_zones[num_zones];
    
    /* Initialize zone structure */
    zone->start_pfn = start_pfn;
    zone->end_pfn = end_pfn;
    zone->type = type;
    zone->total_pages = end_pfn - start_pfn;
    zone->free_pages = 0;
    zone->numa_node = 0;  /* Default to node 0 for now */
    zone->lock = 0;
    
    /* Initialize free areas */
    for (int i = 0; i <= MAX_ORDER; i++) {
        zone->free_area[i].free_list = NULL;
        zone->free_area[i].nr_free = 0;
    }
    
    /* Set watermarks (5%, 10%, 15% of total pages) */
    zone->watermark_min = zone->total_pages / 20;     /* 5% */
    zone->watermark_low = zone->total_pages / 10;     /* 10% */
    zone->watermark_high = zone->total_pages * 3 / 20; /* 15% */
    
    /* Initialize statistics */
    memset(&zone->stats, 0, sizeof(zone->stats));
    
    num_zones++;
    
    debug_print("Buddy: Added zone %d (type %d) with %lu pages\n", 
               num_zones - 1, type, zone->total_pages);
    
    return 0;
}

/**
 * Initialize the buddy allocator
 */
int buddy_allocator_init(void) {
    if (buddy_initialized) {
        return 0;
    }
    
    /* Clear global state */
    memset(memory_zones, 0, sizeof(memory_zones));
    memset(&buddy_statistics, 0, sizeof(buddy_statistics));
    num_zones = 0;
    
    /* Add default zones - these would typically be detected from hardware */
    /* DMA zone: 0-16MB */
    buddy_add_zone(0, 0x1000, ZONE_DMA);
    
    /* Normal zone: 16MB-896MB */
    buddy_add_zone(0x1000, 0x38000, ZONE_NORMAL);
    
    /* High memory zone: 896MB+ */
    buddy_add_zone(0x38000, 0x100000, ZONE_HIGHMEM);
    
    buddy_initialized = true;
    
    debug_print("Buddy: Allocator initialized with %d zones\n", num_zones);
    
    return 0;
}

/**
 * Shutdown the buddy allocator
 */
void buddy_allocator_shutdown(void) {
    if (!buddy_initialized) {
        return;
    }
    
    /* Print final statistics */
    debug_print("Buddy: Shutdown statistics:\n");
    debug_print("  Total allocations: %lu\n", buddy_statistics.total_allocations);
    debug_print("  Total frees: %lu\n", buddy_statistics.total_frees);
    debug_print("  Allocation failures: %lu\n", buddy_statistics.allocation_failures);
    debug_print("  Coalescing operations: %lu\n", buddy_statistics.coalescing_operations);
    debug_print("  Zone fallbacks: %lu\n", buddy_statistics.zone_fallbacks);
    
    buddy_initialized = false;
}

/* ========================== Statistics and Debugging ========================== */

/**
 * Get buddy allocator statistics
 */
void buddy_get_stats(struct buddy_allocator_stats* stats) {
    if (!stats) {
        return;
    }
    
    stats->total_allocations = buddy_statistics.total_allocations;
    stats->total_frees = buddy_statistics.total_frees;
    stats->allocation_failures = buddy_statistics.allocation_failures;
    stats->coalescing_operations = buddy_statistics.coalescing_operations;
    stats->fragmentation_events = buddy_statistics.fragmentation_events;
    stats->zone_fallbacks = buddy_statistics.zone_fallbacks;
    stats->zones_active = num_zones;
    
    /* Calculate total and free memory */
    stats->total_memory = 0;
    stats->free_memory = 0;
    
    for (int i = 0; i < num_zones; i++) {
        stats->total_memory += memory_zones[i].total_pages * PAGE_SIZE;
        stats->free_memory += memory_zones[i].free_pages * PAGE_SIZE;
    }
}

/**
 * Get zone-specific statistics
 */
void buddy_get_zone_stats(int zone_id, zone_stats_t* stats) {
    if (!stats || zone_id < 0 || zone_id >= num_zones) {
        return;
    }
    
    memory_zone_t* zone = &memory_zones[zone_id];
    
    stats->zone_type = zone->type;
    stats->total_pages = zone->total_pages;
    stats->free_pages = zone->free_pages;
    stats->used_pages = zone->total_pages - zone->free_pages;
    stats->watermark_min = zone->watermark_min;
    stats->watermark_low = zone->watermark_low;
    stats->watermark_high = zone->watermark_high;
    stats->allocations = zone->stats.allocations;
    stats->failures = zone->stats.failures;
    stats->reclaim_attempts = zone->stats.reclaim_attempts;
    stats->reclaimed_pages = zone->stats.reclaimed_pages;
}

/**
 * Print buddy allocator debug information
 */
void buddy_debug_print(void) {
    debug_print("=== Buddy Allocator Debug Information ===\n");
    debug_print("Initialized: %s\n", buddy_initialized ? "Yes" : "No");
    debug_print("Number of zones: %d\n", num_zones);
    
    for (int i = 0; i < num_zones; i++) {
        memory_zone_t* zone = &memory_zones[i];
        debug_print("\nZone %d (type %d):\n", i, zone->type);
        debug_print("  PFN range: %lu - %lu\n", zone->start_pfn, zone->end_pfn);
        debug_print("  Total pages: %lu\n", zone->total_pages);
        debug_print("  Free pages: %lu\n", zone->free_pages);
        debug_print("  Watermarks: min=%lu, low=%lu, high=%lu\n",
                   zone->watermark_min, zone->watermark_low, zone->watermark_high);
        
        debug_print("  Free areas:\n");
        for (int order = 0; order <= MAX_ORDER; order++) {
            if (zone->free_area[order].nr_free > 0) {
                debug_print("    Order %d: %lu free blocks\n", 
                           order, zone->free_area[order].nr_free);
            }
        }
    }
    
    debug_print("\nStatistics:\n");
    debug_print("  Allocations: %lu\n", buddy_statistics.total_allocations);
    debug_print("  Frees: %lu\n", buddy_statistics.total_frees);
    debug_print("  Failures: %lu\n", buddy_statistics.allocation_failures);
    debug_print("  Coalescing ops: %lu\n", buddy_statistics.coalescing_operations);
}

/* ========================== Stub Implementations ========================== */

/* Temporary stub implementations for missing types */
struct page {
    struct page* next;
    struct page* prev;
    uint32_t flags;
    uint32_t private;
};

struct buddy_allocator_stats {
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t allocation_failures;
    uint64_t coalescing_operations;
    uint64_t fragmentation_events;
    uint64_t zone_fallbacks;
    uint32_t zones_active;
    uint64_t total_memory;
    uint64_t free_memory;
};
