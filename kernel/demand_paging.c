/* IKOS Demand Paging Implementation - Issue #27
 * Provides on-demand page loading with swap support and page fault handling
 */

#include "memory_advanced.h"
#include "memory.h"
#include "process.h"
#include "interrupts.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ========================== Constants and Configuration ========================== */

#define SWAP_MAGIC              0xDEADBEEF  /* Swap file magic number */
#define SWAP_MAX_FILES          8           /* Maximum swap files */
#define SWAP_BLOCK_SIZE         4096        /* Swap block size (page size) */
#define SWAP_BLOCKS_PER_FILE    (1024 * 1024) /* 4GB per swap file */

/* Page fault error codes */
#define PF_PROT                 0x01        /* Protection violation */
#define PF_WRITE                0x02        /* Write access */
#define PF_USER                 0x04        /* User mode access */
#define PF_RSVD                 0x08        /* Reserved bit violation */
#define PF_INSTR                0x10        /* Instruction fetch */

/* Swap entry format (64-bit) */
#define SWAP_TYPE_SHIFT         7
#define SWAP_TYPE_MASK          0x1F        /* 5 bits for swap type */
#define SWAP_OFFSET_SHIFT       12
#define SWAP_OFFSET_MASK        0xFFFFFFFFFULL /* 36 bits for offset */

/* Page replacement algorithms */
typedef enum {
    REPLACEMENT_LRU,        /* Least Recently Used */
    REPLACEMENT_CLOCK,      /* Clock/Second chance */
    REPLACEMENT_FIFO,       /* First In First Out */
    REPLACEMENT_RANDOM      /* Random replacement */
} replacement_algorithm_t;

/* ========================== Data Structures ========================== */

/* Swap file descriptor */
typedef struct swap_file {
    int                 fd;                 /* File descriptor */
    char                path[256];          /* File path */
    uint64_t            size;               /* File size in bytes */
    uint32_t            pages;              /* Total pages in swap file */
    uint32_t            free_pages;         /* Free pages available */
    uint8_t*            bitmap;             /* Allocation bitmap */
    uint32_t            priority;           /* Swap priority */
    bool                active;             /* Is swap file active */
    volatile int        lock;               /* Swap file lock */
} swap_file_t;

/* Page frame information for replacement */
typedef struct page_frame {
    struct page*        page;               /* Physical page */
    uintptr_t           virt_addr;          /* Virtual address */
    struct process*     process;            /* Owning process */
    uint64_t            access_time;        /* Last access time */
    uint32_t            access_count;       /* Access counter */
    bool                dirty;              /* Has been modified */
    bool                referenced;         /* Recently referenced */
    struct page_frame*  next;               /* Next in replacement list */
    struct page_frame*  prev;               /* Previous in replacement list */
} page_frame_t;

/* Demand paging statistics */
typedef struct paging_stats {
    uint64_t            page_faults;        /* Total page faults */
    uint64_t            major_faults;       /* Major page faults (disk I/O) */
    uint64_t            minor_faults;       /* Minor page faults (no I/O) */
    uint64_t            swap_ins;           /* Pages swapped in */
    uint64_t            swap_outs;          /* Pages swapped out */
    uint64_t            pages_reclaimed;    /* Pages reclaimed */
    uint64_t            oom_kills;          /* Out of memory kills */
    uint64_t            thrashing_events;   /* Thrashing detection */
} paging_stats_t;

/* ========================== Global State ========================== */

static swap_file_t swap_files[SWAP_MAX_FILES];
static uint32_t active_swap_files = 0;
static replacement_algorithm_t replacement_algo = REPLACEMENT_LRU;
static bool demand_paging_enabled = false;
static volatile int paging_lock = 0;

/* Page replacement lists */
static page_frame_t* active_list_head = NULL;
static page_frame_t* inactive_list_head = NULL;
static uint32_t active_pages = 0;
static uint32_t inactive_pages = 0;

/* Statistics */
static paging_stats_t paging_stats = {0};

/* LRU clock for page replacement */
static uint64_t global_clock = 0;

/* Memory pressure thresholds */
static uint32_t low_memory_threshold = 10;     /* % of free memory */
static uint32_t high_memory_threshold = 5;     /* % of free memory */

/* ========================== Helper Functions ========================== */

static inline void paging_global_lock(void) {
    while (__sync_lock_test_and_set(&paging_lock, 1)) {
        /* Spin wait */
    }
}

static inline void paging_global_unlock(void) {
    __sync_lock_release(&paging_lock);
}

static inline void swap_file_lock(swap_file_t* swap) {
    if (swap) {
        while (__sync_lock_test_and_set(&swap->lock, 1)) {
            /* Spin wait */
        }
    }
}

static inline void swap_file_unlock(swap_file_t* swap) {
    if (swap) {
        __sync_lock_release(&swap->lock);
    }
}

static inline uint64_t get_current_time(void) {
    /* TODO: Implement proper timestamp function */
    return ++global_clock;
}

static void debug_print(const char* format, ...) {
    /* Simple debug printing - would integrate with kernel logging */
    va_list args;
    va_start(args, format);
    /* TODO: Integrate with kernel_log system */
    va_end(args);
}

/* ========================== Swap Management ========================== */

/**
 * Initialize a swap file
 */
static int init_swap_file(const char* path, uint32_t priority) {
    if (!path || active_swap_files >= SWAP_MAX_FILES) {
        return -1;
    }
    
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < SWAP_MAX_FILES; i++) {
        if (!swap_files[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return -1;
    }
    
    swap_file_t* swap = &swap_files[slot];
    
    /* TODO: Open swap file with proper filesystem interface */
    /* For now, simulate file operations */
    swap->fd = slot + 100;  /* Dummy file descriptor */
    strncpy(swap->path, path, sizeof(swap->path) - 1);
    swap->path[sizeof(swap->path) - 1] = '\0';
    
    swap->size = SWAP_BLOCKS_PER_FILE * SWAP_BLOCK_SIZE;
    swap->pages = SWAP_BLOCKS_PER_FILE;
    swap->free_pages = swap->pages;
    swap->priority = priority;
    swap->active = true;
    swap->lock = 0;
    
    /* Allocate bitmap for tracking used blocks */
    uint32_t bitmap_size = (swap->pages + 7) / 8;
    swap->bitmap = (uint8_t*)kmalloc(bitmap_size, GFP_KERNEL);
    if (!swap->bitmap) {
        swap->active = false;
        return -1;
    }
    
    memset(swap->bitmap, 0, bitmap_size);
    
    active_swap_files++;
    
    debug_print("Paging: Initialized swap file %s with %u pages\n", path, swap->pages);
    
    return slot;
}

/**
 * Allocate a swap slot
 */
static uint64_t allocate_swap_slot(void) {
    /* Find swap file with highest priority and free space */
    swap_file_t* best_swap = NULL;
    int best_idx = -1;
    uint32_t highest_priority = 0;
    
    for (int i = 0; i < SWAP_MAX_FILES; i++) {
        swap_file_t* swap = &swap_files[i];
        if (swap->active && swap->free_pages > 0 && swap->priority >= highest_priority) {
            best_swap = swap;
            best_idx = i;
            highest_priority = swap->priority;
        }
    }
    
    if (!best_swap) {
        return 0;  /* No swap space available */
    }
    
    swap_file_lock(best_swap);
    
    /* Find first free bit in bitmap */
    uint32_t bitmap_size = (best_swap->pages + 7) / 8;
    uint32_t page_idx = 0;
    bool found = false;
    
    for (uint32_t byte_idx = 0; byte_idx < bitmap_size && !found; byte_idx++) {
        if (best_swap->bitmap[byte_idx] != 0xFF) {
            for (int bit = 0; bit < 8; bit++) {
                if (!(best_swap->bitmap[byte_idx] & (1 << bit))) {
                    page_idx = byte_idx * 8 + bit;
                    if (page_idx < best_swap->pages) {
                        best_swap->bitmap[byte_idx] |= (1 << bit);
                        found = true;
                        break;
                    }
                }
            }
        }
    }
    
    if (!found) {
        swap_file_unlock(best_swap);
        return 0;
    }
    
    best_swap->free_pages--;
    
    swap_file_unlock(best_swap);
    
    /* Encode swap entry: type (5 bits) | offset (36 bits) */
    uint64_t swap_entry = ((uint64_t)best_idx << SWAP_TYPE_SHIFT) | 
                         ((uint64_t)page_idx << SWAP_OFFSET_SHIFT) | 1;
    
    debug_print("Paging: Allocated swap slot %lu (file %d, page %u)\n", 
                swap_entry, best_idx, page_idx);
    
    return swap_entry;
}

/**
 * Free a swap slot
 */
static void free_swap_slot(uint64_t swap_entry) {
    if (!swap_entry || !(swap_entry & 1)) {
        return;  /* Invalid swap entry */
    }
    
    uint32_t swap_type = (swap_entry >> SWAP_TYPE_SHIFT) & SWAP_TYPE_MASK;
    uint32_t page_idx = (swap_entry >> SWAP_OFFSET_SHIFT) & SWAP_OFFSET_MASK;
    
    if (swap_type >= SWAP_MAX_FILES) {
        return;
    }
    
    swap_file_t* swap = &swap_files[swap_type];
    if (!swap->active || page_idx >= swap->pages) {
        return;
    }
    
    swap_file_lock(swap);
    
    /* Clear bit in bitmap */
    uint32_t byte_idx = page_idx / 8;
    uint32_t bit_idx = page_idx % 8;
    
    if (swap->bitmap[byte_idx] & (1 << bit_idx)) {
        swap->bitmap[byte_idx] &= ~(1 << bit_idx);
        swap->free_pages++;
    }
    
    swap_file_unlock(swap);
    
    debug_print("Paging: Freed swap slot %lu\n", swap_entry);
}

/**
 * Read page from swap
 */
static int swap_in_page(uint64_t swap_entry, struct page* page) {
    if (!swap_entry || !(swap_entry & 1) || !page) {
        return -1;
    }
    
    uint32_t swap_type = (swap_entry >> SWAP_TYPE_SHIFT) & SWAP_TYPE_MASK;
    uint32_t page_idx = (swap_entry >> SWAP_OFFSET_SHIFT) & SWAP_OFFSET_MASK;
    
    if (swap_type >= SWAP_MAX_FILES) {
        return -1;
    }
    
    swap_file_t* swap = &swap_files[swap_type];
    if (!swap->active || page_idx >= swap->pages) {
        return -1;
    }
    
    /* TODO: Implement actual disk I/O */
    /* For now, simulate reading from swap */
    void* page_data = (void*)page;
    memset(page_data, 0, PAGE_SIZE);  /* Zero-fill for simulation */
    
    paging_stats.swap_ins++;
    
    debug_print("Paging: Swapped in page from slot %lu\n", swap_entry);
    
    return 0;
}

/**
 * Write page to swap
 */
static int swap_out_page(struct page* page, uint64_t swap_entry) {
    if (!page || !swap_entry || !(swap_entry & 1)) {
        return -1;
    }
    
    uint32_t swap_type = (swap_entry >> SWAP_TYPE_SHIFT) & SWAP_TYPE_MASK;
    uint32_t page_idx = (swap_entry >> SWAP_OFFSET_SHIFT) & SWAP_OFFSET_MASK;
    
    if (swap_type >= SWAP_MAX_FILES) {
        return -1;
    }
    
    swap_file_t* swap = &swap_files[swap_type];
    if (!swap->active || page_idx >= swap->pages) {
        return -1;
    }
    
    /* TODO: Implement actual disk I/O */
    /* For now, simulate writing to swap */
    
    paging_stats.swap_outs++;
    
    debug_print("Paging: Swapped out page to slot %lu\n", swap_entry);
    
    return 0;
}

/* ========================== Page Replacement ========================== */

/**
 * Add page to replacement list
 */
static void add_to_replacement_list(page_frame_t* frame) {
    if (!frame) {
        return;
    }
    
    paging_global_lock();
    
    /* Add to active list initially */
    frame->next = active_list_head;
    frame->prev = NULL;
    
    if (active_list_head) {
        active_list_head->prev = frame;
    }
    
    active_list_head = frame;
    active_pages++;
    
    frame->access_time = get_current_time();
    frame->referenced = true;
    
    paging_global_unlock();
}

/**
 * Remove page from replacement list
 */
static void remove_from_replacement_list(page_frame_t* frame) {
    if (!frame) {
        return;
    }
    
    paging_global_lock();
    
    /* Remove from current list */
    if (frame->prev) {
        frame->prev->next = frame->next;
    }
    if (frame->next) {
        frame->next->prev = frame->prev;
    }
    
    /* Update head pointers */
    if (active_list_head == frame) {
        active_list_head = frame->next;
        active_pages--;
    } else if (inactive_list_head == frame) {
        inactive_list_head = frame->next;
        inactive_pages--;
    }
    
    frame->next = NULL;
    frame->prev = NULL;
    
    paging_global_unlock();
}

/**
 * Move page from active to inactive list
 */
static void deactivate_page(page_frame_t* frame) {
    if (!frame) {
        return;
    }
    
    paging_global_lock();
    
    /* Remove from active list */
    if (frame->prev) {
        frame->prev->next = frame->next;
    }
    if (frame->next) {
        frame->next->prev = frame->prev;
    }
    
    if (active_list_head == frame) {
        active_list_head = frame->next;
    }
    active_pages--;
    
    /* Add to inactive list */
    frame->next = inactive_list_head;
    frame->prev = NULL;
    
    if (inactive_list_head) {
        inactive_list_head->prev = frame;
    }
    
    inactive_list_head = frame;
    inactive_pages++;
    
    frame->referenced = false;
    
    paging_global_unlock();
}

/**
 * Select page for replacement using LRU algorithm
 */
static page_frame_t* select_lru_victim(void) {
    page_frame_t* victim = NULL;
    uint64_t oldest_time = UINT64_MAX;
    
    paging_global_lock();
    
    /* Check inactive list first */
    page_frame_t* frame = inactive_list_head;
    while (frame) {
        if (frame->access_time < oldest_time) {
            oldest_time = frame->access_time;
            victim = frame;
        }
        frame = frame->next;
    }
    
    /* If no inactive pages, check active list */
    if (!victim) {
        frame = active_list_head;
        while (frame) {
            if (frame->access_time < oldest_time) {
                oldest_time = frame->access_time;
                victim = frame;
            }
            frame = frame->next;
        }
    }
    
    paging_global_unlock();
    
    return victim;
}

/**
 * Select page for replacement using Clock algorithm
 */
static page_frame_t* select_clock_victim(void) {
    static page_frame_t* clock_hand = NULL;
    
    paging_global_lock();
    
    /* Initialize clock hand if needed */
    if (!clock_hand) {
        clock_hand = inactive_list_head ? inactive_list_head : active_list_head;
    }
    
    /* Find unreferenced page */
    page_frame_t* start = clock_hand;
    page_frame_t* victim = NULL;
    
    do {
        if (!clock_hand) {
            clock_hand = inactive_list_head ? inactive_list_head : active_list_head;
            if (!clock_hand) {
                break;
            }
        }
        
        if (!clock_hand->referenced) {
            victim = clock_hand;
            clock_hand = clock_hand->next;
            break;
        }
        
        /* Give second chance */
        clock_hand->referenced = false;
        clock_hand = clock_hand->next;
        
    } while (clock_hand != start);
    
    paging_global_unlock();
    
    return victim;
}

/**
 * Select page for replacement
 */
static page_frame_t* select_replacement_victim(void) {
    switch (replacement_algo) {
        case REPLACEMENT_LRU:
            return select_lru_victim();
        case REPLACEMENT_CLOCK:
            return select_clock_victim();
        case REPLACEMENT_FIFO:
            /* Use oldest page in inactive list */
            return inactive_list_head;
        case REPLACEMENT_RANDOM:
            /* TODO: Implement random selection */
            return select_lru_victim();  /* Fallback to LRU */
        default:
            return select_lru_victim();
    }
}

/**
 * Reclaim a page for reuse
 */
static struct page* reclaim_page(void) {
    page_frame_t* victim = select_replacement_victim();
    if (!victim) {
        return NULL;
    }
    
    /* If page is dirty, write to swap */
    if (victim->dirty) {
        uint64_t swap_entry = allocate_swap_slot();
        if (swap_entry) {
            if (swap_out_page(victim->page, swap_entry) == 0) {
                /* Update page table entry to point to swap */
                /* TODO: Update page table with swap entry */
            } else {
                free_swap_slot(swap_entry);
                return NULL;  /* Failed to swap out */
            }
        } else {
            return NULL;  /* No swap space available */
        }
    }
    
    /* Remove from replacement list */
    remove_from_replacement_list(victim);
    
    /* Free page frame descriptor */
    kfree(victim);
    
    paging_stats.pages_reclaimed++;
    
    debug_print("Paging: Reclaimed page %p\n", victim->page);
    
    return victim->page;
}

/* ========================== Page Fault Handler ========================== */

/**
 * Handle page fault
 */
static int handle_page_fault(uintptr_t fault_addr, uint32_t error_code, struct process* process) {
    if (!process) {
        return -1;
    }
    
    paging_stats.page_faults++;
    
    /* Check if fault address is valid */
    /* TODO: Implement VMA (Virtual Memory Area) checking */
    
    bool is_write = (error_code & PF_WRITE) != 0;
    bool is_user = (error_code & PF_USER) != 0;
    bool is_present = !(error_code & PF_PROT);
    
    debug_print("Paging: Page fault at 0x%lx, error=0x%x, process=%p\n", 
                fault_addr, error_code, process);
    
    /* Page not present - demand loading */
    if (!is_present) {
        /* Check if page is swapped out */
        /* TODO: Check page table for swap entry */
        uint64_t swap_entry = 0;  /* Would get from page table */
        
        if (swap_entry) {
            /* Major fault - load from swap */
            paging_stats.major_faults++;
            
            /* Allocate physical page */
            struct page* page = alloc_pages(GFP_KERNEL, 0);
            if (!page) {
                /* Try to reclaim a page */
                page = reclaim_page();
                if (!page) {
                    /* Out of memory - might need to kill process */
                    paging_stats.oom_kills++;
                    return -1;
                }
            }
            
            /* Load page from swap */
            if (swap_in_page(swap_entry, page) != 0) {
                __free_pages(page, 0);
                return -1;
            }
            
            /* Free swap slot */
            free_swap_slot(swap_entry);
            
            /* Map page in page table */
            /* TODO: Update page table mapping */
            
            /* Add to replacement tracking */
            page_frame_t* frame = (page_frame_t*)kmalloc(sizeof(page_frame_t), GFP_KERNEL);
            if (frame) {
                frame->page = page;
                frame->virt_addr = fault_addr & ~(PAGE_SIZE - 1);
                frame->process = process;
                frame->dirty = false;
                frame->access_count = 1;
                add_to_replacement_list(frame);
            }
            
        } else {
            /* Minor fault - allocate new page */
            paging_stats.minor_faults++;
            
            /* Allocate and zero page */
            struct page* page = alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
            if (!page) {
                page = reclaim_page();
                if (!page) {
                    paging_stats.oom_kills++;
                    return -1;
                }
                memset((void*)page, 0, PAGE_SIZE);
            }
            
            /* Map page in page table */
            /* TODO: Update page table mapping */
            
            /* Add to replacement tracking */
            page_frame_t* frame = (page_frame_t*)kmalloc(sizeof(page_frame_t), GFP_KERNEL);
            if (frame) {
                frame->page = page;
                frame->virt_addr = fault_addr & ~(PAGE_SIZE - 1);
                frame->process = process;
                frame->dirty = false;
                frame->access_count = 1;
                add_to_replacement_list(frame);
            }
        }
        
        return 0;
    }
    
    /* Page is present but access violation */
    if (error_code & PF_PROT) {
        /* Could be copy-on-write or permission violation */
        /* TODO: Handle COW and permissions */
        debug_print("Paging: Protection violation at 0x%lx\n", fault_addr);
        return -1;
    }
    
    return 0;
}

/**
 * Page fault interrupt handler
 */
void page_fault_handler(registers_t* regs) {
    /* Get fault address from CR2 register */
    uintptr_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    uint32_t error_code = regs->err_code;
    struct process* current = get_current_process();
    
    if (handle_page_fault(fault_addr, error_code, current) != 0) {
        /* Unhandled page fault - terminate process or panic */
        debug_print("Paging: Unhandled page fault in process %p\n", current);
        /* TODO: Implement proper error handling */
    }
}

/* ========================== Memory Pressure Management ========================== */

/**
 * Check memory pressure and trigger reclaim if needed
 */
static void check_memory_pressure(void) {
    uint32_t free_pages = get_free_page_count();
    uint32_t total_pages = get_total_page_count();
    uint32_t free_percent = (free_pages * 100) / total_pages;
    
    if (free_percent <= high_memory_threshold) {
        /* High memory pressure - aggressive reclaim */
        debug_print("Paging: High memory pressure (%u%% free), starting aggressive reclaim\n", 
                   free_percent);
        
        /* Reclaim multiple pages */
        for (int i = 0; i < 16 && free_percent <= high_memory_threshold; i++) {
            struct page* reclaimed = reclaim_page();
            if (!reclaimed) {
                break;
            }
            free_pages++;
            free_percent = (free_pages * 100) / total_pages;
        }
        
    } else if (free_percent <= low_memory_threshold) {
        /* Low memory pressure - gentle reclaim */
        debug_print("Paging: Low memory pressure (%u%% free), starting gentle reclaim\n", 
                   free_percent);
        
        /* Reclaim a few pages */
        for (int i = 0; i < 4; i++) {
            struct page* reclaimed = reclaim_page();
            if (!reclaimed) {
                break;
            }
        }
    }
}

/**
 * Background reclaim thread (would be called periodically)
 */
void kswapd_thread(void) {
    while (demand_paging_enabled) {
        check_memory_pressure();
        
        /* Sleep for a while */
        /* TODO: Implement proper thread sleeping */
        for (volatile int i = 0; i < 1000000; i++) {
            /* Busy wait for simulation */
        }
    }
}

/* ========================== Public API ========================== */

/**
 * Initialize demand paging system
 */
int demand_paging_init(void) {
    if (demand_paging_enabled) {
        return 0;
    }
    
    /* Initialize swap files array */
    memset(swap_files, 0, sizeof(swap_files));
    active_swap_files = 0;
    
    /* Initialize replacement lists */
    active_list_head = NULL;
    inactive_list_head = NULL;
    active_pages = 0;
    inactive_pages = 0;
    
    /* Initialize statistics */
    memset(&paging_stats, 0, sizeof(paging_stats));
    
    /* Set up page fault handler */
    /* TODO: Register page fault handler with interrupt system */
    
    global_clock = 0;
    paging_lock = 0;
    
    demand_paging_enabled = true;
    
    debug_print("Paging: Demand paging system initialized\n");
    
    return 0;
}

/**
 * Shutdown demand paging system
 */
void demand_paging_shutdown(void) {
    if (!demand_paging_enabled) {
        return;
    }
    
    demand_paging_enabled = false;
    
    /* Print statistics */
    debug_print("Paging: Shutdown statistics:\n");
    debug_print("  Page faults: %lu (major: %lu, minor: %lu)\n", 
                paging_stats.page_faults, paging_stats.major_faults, paging_stats.minor_faults);
    debug_print("  Swap operations: %lu in, %lu out\n", 
                paging_stats.swap_ins, paging_stats.swap_outs);
    debug_print("  Pages reclaimed: %lu\n", paging_stats.pages_reclaimed);
    debug_print("  OOM kills: %lu\n", paging_stats.oom_kills);
    
    /* Clean up swap files */
    for (int i = 0; i < SWAP_MAX_FILES; i++) {
        swap_file_t* swap = &swap_files[i];
        if (swap->active) {
            if (swap->bitmap) {
                kfree(swap->bitmap);
            }
            /* TODO: Close swap file */
            swap->active = false;
        }
    }
    
    /* Clean up replacement lists */
    page_frame_t* frame = active_list_head;
    while (frame) {
        page_frame_t* next = frame->next;
        kfree(frame);
        frame = next;
    }
    
    frame = inactive_list_head;
    while (frame) {
        page_frame_t* next = frame->next;
        kfree(frame);
        frame = next;
    }
    
    debug_print("Paging: Demand paging system shutdown complete\n");
}

/**
 * Enable swap file
 */
int swapon(const char* path, uint32_t priority) {
    if (!path || !demand_paging_enabled) {
        return -1;
    }
    
    return init_swap_file(path, priority);
}

/**
 * Disable swap file
 */
int swapoff(const char* path) {
    if (!path || !demand_paging_enabled) {
        return -1;
    }
    
    /* Find swap file */
    for (int i = 0; i < SWAP_MAX_FILES; i++) {
        swap_file_t* swap = &swap_files[i];
        if (swap->active && strcmp(swap->path, path) == 0) {
            swap_file_lock(swap);
            
            /* TODO: Move all pages from this swap file back to memory */
            
            if (swap->bitmap) {
                kfree(swap->bitmap);
            }
            
            swap->active = false;
            active_swap_files--;
            
            swap_file_unlock(swap);
            
            debug_print("Paging: Disabled swap file %s\n", path);
            return 0;
        }
    }
    
    return -1;  /* Swap file not found */
}

/**
 * Get paging statistics
 */
void get_paging_stats(struct demand_paging_stats* stats) {
    if (!stats) {
        return;
    }
    
    stats->page_faults = paging_stats.page_faults;
    stats->major_faults = paging_stats.major_faults;
    stats->minor_faults = paging_stats.minor_faults;
    stats->swap_ins = paging_stats.swap_ins;
    stats->swap_outs = paging_stats.swap_outs;
    stats->pages_reclaimed = paging_stats.pages_reclaimed;
    stats->active_pages = active_pages;
    stats->inactive_pages = inactive_pages;
    stats->total_swap_pages = 0;
    stats->free_swap_pages = 0;
    
    /* Calculate swap usage */
    for (int i = 0; i < SWAP_MAX_FILES; i++) {
        if (swap_files[i].active) {
            stats->total_swap_pages += swap_files[i].pages;
            stats->free_swap_pages += swap_files[i].free_pages;
        }
    }
}

/**
 * Set page replacement algorithm
 */
void set_replacement_algorithm(replacement_algorithm_t algo) {
    replacement_algo = algo;
    debug_print("Paging: Set replacement algorithm to %d\n", algo);
}

/* Temporary stub for missing structure */
struct demand_paging_stats {
    uint64_t page_faults;
    uint64_t major_faults;
    uint64_t minor_faults;
    uint64_t swap_ins;
    uint64_t swap_outs;
    uint64_t pages_reclaimed;
    uint32_t active_pages;
    uint32_t inactive_pages;
    uint32_t total_swap_pages;
    uint32_t free_swap_pages;
};
