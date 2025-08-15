/* IKOS Kernel Memory Allocator Implementation - Issue #12
 * SLAB/SLOB-based allocator with object caching and large block support
 */

#include "../include/kalloc.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/stdio.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Global allocator state */
static bool kalloc_initialized = false;
static void* heap_base = NULL;
static size_t heap_size = 0;
static kalloc_stats_t allocator_stats = {0};

/* SLAB caches for common allocation sizes */
static kalloc_cache_t size_caches[KALLOC_NUM_CACHES];
static const size_t cache_sizes[KALLOC_NUM_CACHES] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
};

/* Large block free list */
static kalloc_block_t* large_free_list = NULL;

/* Heap lock (will need to implement spinlock for SMP) */
static volatile uint32_t heap_lock = 0;

/* Forward declarations for internal functions */
static void* heap_alloc_pages(size_t pages);
static void heap_free_pages(void* ptr, size_t pages);
static void merge_free_blocks(void);
static kalloc_block_t* split_block(kalloc_block_t* block, size_t size);

/* Initialize the kernel allocator */
int kalloc_init(void* heap_start, size_t heap_sz) {
    if (kalloc_initialized) {
        return KALLOC_ERROR_INVALID;
    }
    
    /* Align heap start to page boundary */
    heap_base = (void*)KALLOC_ROUND_UP((uintptr_t)heap_start, KALLOC_ALIGN_PAGE);
    heap_size = heap_sz - ((uintptr_t)heap_base - (uintptr_t)heap_start);
    
    /* Initialize statistics */
    memset(&allocator_stats, 0, sizeof(kalloc_stats_t));
    
    /* Initialize SLAB caches */
    for (int i = 0; i < KALLOC_NUM_CACHES; i++) {
        kalloc_cache_t* cache = &size_caches[i];
        
        cache->object_size = cache_sizes[i];
        cache->align = (cache_sizes[i] >= 64) ? 64 : cache_sizes[i];
        cache->slab_size = KALLOC_ALIGN_PAGE;
        cache->objects_per_slab = cache->slab_size / cache->object_size;
        
        cache->full_slabs = NULL;
        cache->partial_slabs = NULL;
        cache->empty_slabs = NULL;
        
        cache->total_slabs = 0;
        cache->active_objects = 0;
        cache->total_objects = 0;
        
        /* Simple string formatting for cache name */
        const char* prefix = "size-";
        char* name_ptr = cache->name;
        const char* p = prefix;
        while (*p) *name_ptr++ = *p++;
        
        /* Simple integer to string conversion */
        size_t size = cache_sizes[i];
        if (size >= 1000) {
            *name_ptr++ = '0' + (size / 1000);
            size %= 1000;
        }
        if (size >= 100) {
            *name_ptr++ = '0' + (size / 100);
            size %= 100;
        }
        if (size >= 10) {
            *name_ptr++ = '0' + (size / 10);
            size %= 10;
        }
        *name_ptr++ = '0' + size;
        *name_ptr = '\0';
    }
    
    /* Initialize large block free list with entire heap */
    large_free_list = (kalloc_block_t*)heap_base;
    large_free_list->size = heap_size - sizeof(kalloc_block_t);
    large_free_list->next = NULL;
    large_free_list->prev = NULL;
    large_free_list->magic = KALLOC_BLOCK_MAGIC;
    
    kalloc_initialized = true;
    
    printf("KALLOC: Initialized with %zu KB heap at %p\n", 
           heap_size / 1024, heap_base);
    
    return KALLOC_SUCCESS;
}

/* Shutdown the allocator */
void kalloc_shutdown(void) {
    if (!kalloc_initialized) return;
    
    kalloc_print_stats();
    kalloc_initialized = false;
}

/* Main allocation function */
void* kalloc(size_t size) {
    return kalloc_flags(size, 0);
}

/* Aligned allocation */
void* kalloc_aligned(size_t size, size_t align) {
    if (!kalloc_initialized || size == 0 || 
        (align & (align - 1)) != 0) { /* Check if align is power of 2 */
        return NULL;
    }
    
    /* For small aligned allocations, use cache if alignment matches */
    if (size <= KALLOC_MAX_SIZE && align <= 64) {
        kalloc_cache_t* cache = find_cache(size);
        if (cache && cache->align >= align) {
            return kalloc_cache_alloc(cache);
        }
    }
    
    /* Large aligned allocation */
    size_t total_size = size + align - 1 + sizeof(kalloc_block_t);
    void* raw_ptr = kalloc_large(total_size);
    if (!raw_ptr) return NULL;
    
    uintptr_t aligned_addr = KALLOC_ROUND_UP((uintptr_t)raw_ptr + sizeof(kalloc_block_t), align);
    return (void*)aligned_addr;
}

/* Allocation with flags */
void* kalloc_flags(size_t size, uint32_t flags) {
    if (!kalloc_initialized || size == 0) {
        return NULL;
    }
    
    void* ptr = NULL;
    
    /* Acquire heap lock - simple spinlock */
    while (heap_lock) {
        /* Spin wait */
    }
    heap_lock = 1;
    
    /* Use SLAB cache for small allocations */
    if (size <= KALLOC_MAX_SIZE) {
        kalloc_cache_t* cache = find_cache(size);
        if (cache) {
            ptr = kalloc_cache_alloc(cache);
            allocator_stats.cache_hits++;
        } else {
            allocator_stats.cache_misses++;
        }
    }
    
    /* Use large block allocator for big allocations */
    if (!ptr) {
        ptr = kalloc_large(size);
    }
    
    if (ptr) {
        /* Zero memory if requested */
        if (flags & KALLOC_ZERO) {
            memset(ptr, 0, size);
        }
        
        /* Update statistics */
        allocator_stats.total_allocated += size;
        allocator_stats.current_usage += size;
        allocator_stats.allocation_count++;
        
        if (allocator_stats.current_usage > allocator_stats.peak_usage) {
            allocator_stats.peak_usage = allocator_stats.current_usage;
        }
    }
    
    /* Release heap lock */
    heap_lock = 0;
    
    return ptr;
}

/* Free allocated memory - internal kalloc version */
void kalloc_kfree(void* ptr) {
    kfree(ptr);  /* Call our internal kfree */
}

/* Free allocated memory - public interface */
void kfree(void* ptr) {
    if (!ptr || !kalloc_initialized) {
        return;
    }
    
    /* Acquire heap lock - simple spinlock */
    while (heap_lock) {
        /* Spin wait */
    }
    heap_lock = 1;
    
    /* Determine if this is a SLAB or large allocation */
    bool freed = false;
    
    /* Try SLAB caches first */
    for (int i = 0; i < KALLOC_NUM_CACHES; i++) {
        kalloc_cache_t* cache = &size_caches[i];
        
        /* Check all slabs in this cache */
        for (kalloc_slab_t* slab = cache->full_slabs; slab; slab = slab->next) {
            if (ptr >= slab->memory && 
                ptr < (void*)((char*)slab->memory + cache->slab_size)) {
                slab_free_object(slab, ptr);
                freed = true;
                break;
            }
        }
        
        if (!freed) {
            for (kalloc_slab_t* slab = cache->partial_slabs; slab; slab = slab->next) {
                if (ptr >= slab->memory && 
                    ptr < (void*)((char*)slab->memory + cache->slab_size)) {
                    slab_free_object(slab, ptr);
                    freed = true;
                    break;
                }
            }
        }
        
        if (freed) break;
    }
    
    /* If not found in SLAB caches, treat as large allocation */
    if (!freed) {
        /* Find the block header */
        kalloc_block_t* block = (kalloc_block_t*)((char*)ptr - sizeof(kalloc_block_t));
        
        if (block->magic == KALLOC_BLOCK_MAGIC) {
            kfree_large(ptr, 0); /* Size will be determined from block header */
        }
    }
    
    /* Update statistics */
    allocator_stats.free_count++;
    
    /* Release heap lock */
    heap_lock = 0;
}

/* Create a new SLAB cache */
kalloc_cache_t* kalloc_cache_create(const char* name, size_t object_size, size_t align) {
    if (!kalloc_initialized || object_size == 0 || 
        (align & (align - 1)) != 0) {
        return NULL;
    }
    
    /* For now, return one of the existing caches if size matches */
    for (int i = 0; i < KALLOC_NUM_CACHES; i++) {
        if (size_caches[i].object_size >= object_size && 
            size_caches[i].align >= align) {
            return &size_caches[i];
        }
    }
    
    return NULL;
}

/* Destroy a SLAB cache */
void kalloc_cache_destroy(kalloc_cache_t* cache) {
    if (!cache) return;
    
    /* Free all slabs in this cache */
    kalloc_slab_t* slab = cache->full_slabs;
    while (slab) {
        kalloc_slab_t* next = slab->next;
        destroy_slab(slab);
        slab = next;
    }
    
    slab = cache->partial_slabs;
    while (slab) {
        kalloc_slab_t* next = slab->next;
        destroy_slab(slab);
        slab = next;
    }
    
    slab = cache->empty_slabs;
    while (slab) {
        kalloc_slab_t* next = slab->next;
        destroy_slab(slab);
        slab = next;
    }
}

/* Allocate object from SLAB cache */
void* kalloc_cache_alloc(kalloc_cache_t* cache) {
    if (!cache) return NULL;
    
    kalloc_slab_t* slab = NULL;
    
    /* Try to find a slab with free objects */
    if (cache->partial_slabs) {
        slab = cache->partial_slabs;
    } else if (cache->empty_slabs) {
        slab = cache->empty_slabs;
        /* Move from empty to partial list */
        cache->empty_slabs = slab->next;
        slab->next = cache->partial_slabs;
        cache->partial_slabs = slab;
    } else {
        /* Create a new slab */
        slab = create_slab(cache);
        if (!slab) return NULL;
        
        /* Add to partial list */
        slab->next = cache->partial_slabs;
        cache->partial_slabs = slab;
    }
    
    /* Allocate object from slab */
    void* ptr = slab_alloc_object(slab);
    
    /* Move slab to appropriate list if it became full */
    if (slab->free_objects == 0) {
        /* Remove from partial list */
        if (cache->partial_slabs == slab) {
            cache->partial_slabs = slab->next;
        } else {
            kalloc_slab_t* prev = cache->partial_slabs;
            while (prev && prev->next != slab) {
                prev = prev->next;
            }
            if (prev) {
                prev->next = slab->next;
            }
        }
        
        /* Add to full list */
        slab->next = cache->full_slabs;
        cache->full_slabs = slab;
    }
    
    return ptr;
}

/* Free object back to SLAB cache */
void kalloc_cache_free(kalloc_cache_t* cache, void* ptr) {
    /* This function is called from kfree() */
    /* Implementation is handled there */
}

/* Large allocation using free block list */
void* kalloc_large(size_t size) {
    size_t total_size = size + sizeof(kalloc_block_t);
    total_size = KALLOC_ROUND_UP(total_size, KALLOC_ALIGN_8);
    
    kalloc_block_t* best_block = NULL;
    kalloc_block_t* current = large_free_list;
    
    /* Find best fit block */
    while (current) {
        if (current->magic != KALLOC_BLOCK_MAGIC) {
            printf("KALLOC: Corruption detected in free block at %p\n", current);
            return NULL;
        }
        
        if (current->size >= total_size) {
            if (!best_block || current->size < best_block->size) {
                best_block = current;
            }
        }
        current = current->next;
    }
    
    if (!best_block) {
        return NULL; /* Out of memory */
    }
    
    /* Split the block if it's significantly larger than needed */
    if (best_block->size >= total_size + sizeof(kalloc_block_t) + 64) {
        split_block(best_block, total_size);
    }
    
    /* Remove from free list */
    if (best_block->prev) {
        best_block->prev->next = best_block->next;
    } else {
        large_free_list = best_block->next;
    }
    
    if (best_block->next) {
        best_block->next->prev = best_block->prev;
    }
    
    /* Mark as allocated */
    best_block->magic = KALLOC_FREE_MAGIC;
    
    return (char*)best_block + sizeof(kalloc_block_t);
}

/* Free large allocation */
void kfree_large(void* ptr, size_t size) {
    if (!ptr) return;
    
    kalloc_block_t* block = (kalloc_block_t*)((char*)ptr - sizeof(kalloc_block_t));
    
    if (block->magic != KALLOC_FREE_MAGIC) {
        printf("KALLOC: Invalid magic in block at %p\n", block);
        return;
    }
    
    /* Mark as free */
    block->magic = KALLOC_BLOCK_MAGIC;
    
    /* Add back to free list */
    block->next = large_free_list;
    block->prev = NULL;
    
    if (large_free_list) {
        large_free_list->prev = block;
    }
    large_free_list = block;
    
    /* Merge adjacent free blocks */
    merge_free_blocks();
    
    /* Update statistics */
    allocator_stats.total_freed += block->size;
    allocator_stats.current_usage -= block->size;
}

/* Find appropriate cache for size */
static kalloc_cache_t* find_cache(size_t size) {
    for (int i = 0; i < KALLOC_NUM_CACHES; i++) {
        if (cache_sizes[i] >= size) {
            return &size_caches[i];
        }
    }
    return NULL;
}

/* Create a new slab */
static kalloc_slab_t* create_slab(kalloc_cache_t* cache) {
    /* Allocate memory for slab descriptor + slab data */
    size_t slab_total = sizeof(kalloc_slab_t) + cache->slab_size;
    kalloc_slab_t* slab = (kalloc_slab_t*)kalloc_large(slab_total);
    
    if (!slab) return NULL;
    
    slab->memory = (char*)slab + sizeof(kalloc_slab_t);
    slab->free_objects = cache->objects_per_slab;
    slab->first_free = 0;
    slab->next = NULL;
    slab->cache = cache;
    slab->magic = KALLOC_SLAB_MAGIC;
    
    /* Initialize free object list */
    uint32_t* free_list = (uint32_t*)slab->memory;
    for (uint32_t i = 0; i < cache->objects_per_slab - 1; i++) {
        free_list[i * cache->object_size / sizeof(uint32_t)] = i + 1;
    }
    free_list[(cache->objects_per_slab - 1) * cache->object_size / sizeof(uint32_t)] = 0xFFFFFFFF;
    
    cache->total_slabs++;
    cache->total_objects += cache->objects_per_slab;
    allocator_stats.slab_count++;
    
    return slab;
}

/* Destroy a slab */
static void destroy_slab(kalloc_slab_t* slab) {
    if (!slab) return;
    
    kfree_large(slab, 0);
    allocator_stats.slab_count--;
}

/* Allocate object from slab */
static void* slab_alloc_object(kalloc_slab_t* slab) {
    if (!slab || slab->free_objects == 0) {
        return NULL;
    }
    
    uint32_t obj_index = slab->first_free;
    uint32_t* free_list = (uint32_t*)slab->memory;
    
    slab->first_free = free_list[obj_index * slab->cache->object_size / sizeof(uint32_t)];
    slab->free_objects--;
    slab->cache->active_objects++;
    
    return (char*)slab->memory + (obj_index * slab->cache->object_size);
}

/* Free object back to slab */
static void slab_free_object(kalloc_slab_t* slab, void* ptr) {
    if (!slab || !ptr) return;
    
    uintptr_t obj_offset = (char*)ptr - (char*)slab->memory;
    uint32_t obj_index = obj_offset / slab->cache->object_size;
    
    uint32_t* free_list = (uint32_t*)slab->memory;
    free_list[obj_index * slab->cache->object_size / sizeof(uint32_t)] = slab->first_free;
    slab->first_free = obj_index;
    
    slab->free_objects++;
    slab->cache->active_objects--;
}

/* Split a large block */
static kalloc_block_t* split_block(kalloc_block_t* block, size_t size) {
    if (block->size <= size + sizeof(kalloc_block_t)) {
        return NULL;
    }
    
    kalloc_block_t* new_block = (kalloc_block_t*)((char*)block + size);
    new_block->size = block->size - size;
    new_block->magic = KALLOC_BLOCK_MAGIC;
    
    /* Insert into free list */
    new_block->next = block->next;
    new_block->prev = block;
    
    if (block->next) {
        block->next->prev = new_block;
    }
    block->next = new_block;
    
    block->size = size;
    
    return new_block;
}

/* Merge adjacent free blocks */
static void merge_free_blocks(void) {
    kalloc_block_t* current = large_free_list;
    
    while (current && current->next) {
        kalloc_block_t* next_block = (kalloc_block_t*)((char*)current + current->size + sizeof(kalloc_block_t));
        
        if (next_block == current->next) {
            /* Adjacent blocks - merge them */
            current->size += current->next->size + sizeof(kalloc_block_t);
            kalloc_block_t* old_next = current->next;
            current->next = old_next->next;
            
            if (current->next) {
                current->next->prev = current;
            }
        } else {
            current = current->next;
        }
    }
}

/* Get allocation statistics */
kalloc_stats_t* kalloc_get_stats(void) {
    /* Calculate fragmentation */
    if (allocator_stats.total_allocated > 0) {
        allocator_stats.fragmentation = 
            (uint32_t)((allocator_stats.current_usage * 100) / 
                      allocator_stats.total_allocated);
    }
    
    return &allocator_stats;
}

/* Print allocation statistics */
void kalloc_print_stats(void) {
    kalloc_stats_t* stats = kalloc_get_stats();
    
    printf("\n=== KALLOC Statistics ===\n");
    printf("Total allocated: %llu bytes\n", stats->total_allocated);
    printf("Total freed: %llu bytes\n", stats->total_freed);
    printf("Current usage: %llu bytes\n", stats->current_usage);
    printf("Peak usage: %llu bytes\n", stats->peak_usage);
    printf("Allocations: %u\n", stats->allocation_count);
    printf("Frees: %u\n", stats->free_count);
    printf("SLAB count: %u\n", stats->slab_count);
    printf("Cache hits: %u\n", stats->cache_hits);
    printf("Cache misses: %u\n", stats->cache_misses);
    printf("Fragmentation: %u%%\n", stats->fragmentation);
    
    printf("\n=== SLAB Cache Info ===\n");
    for (int i = 0; i < KALLOC_NUM_CACHES; i++) {
        kalloc_cache_t* cache = &size_caches[i];
        printf("%s: %u/%u objects, %u slabs\n", 
               cache->name, cache->active_objects, 
               cache->total_objects, cache->total_slabs);
    }
}

/* Validate heap integrity */
void kalloc_validate_heap(void) {
    printf("KALLOC: Validating heap integrity...\n");
    
    uint32_t block_count = 0;
    kalloc_block_t* current = large_free_list;
    
    while (current) {
        if (current->magic != KALLOC_BLOCK_MAGIC) {
            printf("KALLOC: ERROR - Invalid magic in block %u at %p\n", 
                   block_count, current);
            return;
        }
        
        block_count++;
        current = current->next;
    }
    
    printf("KALLOC: Heap validation passed (%u free blocks)\n", block_count);
}

/* Check for heap corruption */
bool kalloc_check_corruption(void) {
    kalloc_block_t* current = large_free_list;
    
    while (current) {
        if (current->magic != KALLOC_BLOCK_MAGIC) {
            return true;
        }
        current = current->next;
    }
    
    return false;
}

/* Get usable size of allocation */
size_t kalloc_usable_size(void* ptr) {
    if (!ptr) return 0;
    
    /* Check if it's a SLAB allocation */
    for (int i = 0; i < KALLOC_NUM_CACHES; i++) {
        kalloc_cache_t* cache = &size_caches[i];
        
        for (kalloc_slab_t* slab = cache->full_slabs; slab; slab = slab->next) {
            if (ptr >= slab->memory && 
                ptr < (void*)((char*)slab->memory + cache->slab_size)) {
                return cache->object_size;
            }
        }
        
        for (kalloc_slab_t* slab = cache->partial_slabs; slab; slab = slab->next) {
            if (ptr >= slab->memory && 
                ptr < (void*)((char*)slab->memory + cache->slab_size)) {
                return cache->object_size;
            }
        }
    }
    
    /* Large allocation */
    kalloc_block_t* block = (kalloc_block_t*)((char*)ptr - sizeof(kalloc_block_t));
    if (block->magic == KALLOC_FREE_MAGIC) {
        return block->size - sizeof(kalloc_block_t);
    }
    
    return 0;
}

/* Check if pointer is valid */
bool kalloc_is_valid_pointer(void* ptr) {
    if (!ptr || ptr < heap_base || 
        ptr >= (void*)((char*)heap_base + heap_size)) {
        return false;
    }
    
    return kalloc_usable_size(ptr) > 0;
}
