/* IKOS Kernel Memory Allocator (SLAB/SLOB) - Issue #12
 * Provides efficient kernel heap management with object caching
 */

#ifndef KALLOC_H
#define KALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Memory allocation flags */
#define KALLOC_ATOMIC     0x01    /* Atomic allocation (no sleep) */
#define KALLOC_ZERO       0x02    /* Zero the allocated memory */
#define KALLOC_DMA        0x04    /* DMA-compatible memory */
#define KALLOC_HIGH       0x08    /* High memory allocation */

/* Allocation alignment */
#define KALLOC_ALIGN_8    8
#define KALLOC_ALIGN_16   16
#define KALLOC_ALIGN_32   32
#define KALLOC_ALIGN_64   64
#define KALLOC_ALIGN_PAGE 4096

/* SLAB cache sizes (powers of 2 from 8 bytes to 4KB) */
#define KALLOC_MIN_SIZE   8
#define KALLOC_MAX_SIZE   4096
#define KALLOC_NUM_CACHES 10

/* Error codes */
#define KALLOC_SUCCESS    0
#define KALLOC_ERROR_OOM  -1    /* Out of memory */
#define KALLOC_ERROR_INVALID -2 /* Invalid parameters */
#define KALLOC_ERROR_CORRUPT -3 /* Heap corruption detected */

/* Forward declarations */
typedef struct kalloc_cache kalloc_cache_t;
typedef struct kalloc_slab kalloc_slab_t;
typedef struct kalloc_block kalloc_block_t;
typedef struct kalloc_stats kalloc_stats_t;

/* SLAB cache structure */
struct kalloc_cache {
    size_t object_size;           /* Size of objects in this cache */
    size_t align;                 /* Alignment requirement */
    uint32_t objects_per_slab;    /* Number of objects per slab */
    uint32_t slab_size;           /* Size of each slab */
    
    kalloc_slab_t* full_slabs;    /* List of full slabs */
    kalloc_slab_t* partial_slabs; /* List of partially allocated slabs */
    kalloc_slab_t* empty_slabs;   /* List of empty slabs */
    
    uint32_t total_slabs;         /* Total number of slabs */
    uint32_t active_objects;      /* Number of allocated objects */
    uint32_t total_objects;       /* Total objects across all slabs */
    
    char name[32];                /* Cache name for debugging */
};

/* SLAB structure */
struct kalloc_slab {
    void* memory;                 /* Slab memory region */
    uint32_t free_objects;        /* Number of free objects */
    uint32_t first_free;          /* Index of first free object */
    kalloc_slab_t* next;          /* Next slab in list */
    kalloc_cache_t* cache;        /* Parent cache */
    uint32_t magic;               /* Magic number for corruption detection */
};

/* Free block structure for large allocations */
struct kalloc_block {
    size_t size;                  /* Size of this block */
    kalloc_block_t* next;         /* Next free block */
    kalloc_block_t* prev;         /* Previous free block */
    uint32_t magic;               /* Magic number for corruption detection */
};

/* Allocation statistics */
struct kalloc_stats {
    uint64_t total_allocated;     /* Total bytes allocated */
    uint64_t total_freed;         /* Total bytes freed */
    uint64_t current_usage;       /* Current memory usage */
    uint64_t peak_usage;          /* Peak memory usage */
    uint32_t allocation_count;    /* Number of allocations */
    uint32_t free_count;          /* Number of frees */
    uint32_t slab_count;          /* Number of slabs */
    uint32_t cache_hits;          /* SLAB cache hits */
    uint32_t cache_misses;        /* SLAB cache misses */
    uint32_t fragmentation;       /* Fragmentation percentage */
};

/* Core allocation functions */
int kalloc_init(void* heap_start, size_t heap_size);
void kalloc_shutdown(void);

void* kalloc(size_t size);
void* kalloc_aligned(size_t size, size_t align);
void* kalloc_flags(size_t size, uint32_t flags);
void kfree(void* ptr);
void kalloc_kfree(void* ptr);  /* Internal wrapper to avoid naming conflicts */

/* Cache management */
kalloc_cache_t* kalloc_cache_create(const char* name, size_t object_size, 
                                   size_t align);
void kalloc_cache_destroy(kalloc_cache_t* cache);
void* kalloc_cache_alloc(kalloc_cache_t* cache);
void kalloc_cache_free(kalloc_cache_t* cache, void* ptr);

/* Large allocation functions */
void* kalloc_large(size_t size);
void kfree_large(void* ptr, size_t size);

/* Debugging and statistics */
kalloc_stats_t* kalloc_get_stats(void);
void kalloc_print_stats(void);
void kalloc_validate_heap(void);
bool kalloc_check_corruption(void);

/* Memory utilities */
size_t kalloc_usable_size(void* ptr);
void kalloc_trim(size_t pad);
bool kalloc_is_valid_pointer(void* ptr);

/* Testing functions */
void kalloc_run_tests(void);
void kalloc_stress_test(void);

/* Internal functions */
static kalloc_cache_t* find_cache(size_t size);
static kalloc_slab_t* create_slab(kalloc_cache_t* cache);
static void destroy_slab(kalloc_slab_t* slab);
static void* slab_alloc_object(kalloc_slab_t* slab);
static void slab_free_object(kalloc_slab_t* slab, void* ptr);

/* Magic numbers for corruption detection */
#define KALLOC_SLAB_MAGIC    0xDEADBEEF
#define KALLOC_BLOCK_MAGIC   0xCAFEBABE
#define KALLOC_FREE_MAGIC    0xFEEDFACE

/* Size macros */
#define KALLOC_ROUND_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define KALLOC_IS_ALIGNED(ptr, align) (((uintptr_t)(ptr) & ((align) - 1)) == 0)

#endif /* KALLOC_H */
