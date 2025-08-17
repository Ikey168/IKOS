/* IKOS Advanced Memory Management System - Issue #27
 * Provides sophisticated memory allocation, demand paging, compression, and NUMA support
 */

#ifndef MEMORY_ADVANCED_H
#define MEMORY_ADVANCED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Basic type definitions */
typedef int pid_t;
typedef unsigned long gfp_t;
typedef unsigned int slab_flags_t;

/* Forward declarations */
struct page;
struct vm_area_struct;
struct process;

/* ========================== Memory Allocator Types ========================== */

/* GFP (Get Free Pages) flags for allocation behavior */
#define GFP_KERNEL          0x0001      /* Standard kernel allocation */
#define GFP_ATOMIC          0x0002      /* Atomic allocation (no sleep) */
#define GFP_USER            0x0004      /* User-space allocation */
#define GFP_DMA             0x0008      /* DMA-capable memory */
#define GFP_HIGHMEM         0x0010      /* High memory allowed */
#define GFP_ZERO            0x0020      /* Zero-initialized memory */
#define GFP_NOWAIT          0x0040      /* No waiting for memory */
#define GFP_NORETRY         0x0080      /* Don't retry on failure */
#define GFP_NOFAIL          0x0100      /* Never fail allocation */

/* Memory zone types */
typedef enum {
    ZONE_DMA        = 0,                /* DMA-capable memory (< 16MB) */
    ZONE_NORMAL     = 1,                /* Normal kernel memory */
    ZONE_HIGHMEM    = 2,                /* High memory (> 896MB) */
    ZONE_MOVABLE    = 3,                /* Movable/reclaimable memory */
    MAX_NR_ZONES    = 4
} zone_type_t;

/* Slab allocator flags */
#define SLAB_HWCACHE_ALIGN  0x0001      /* Align to hardware cache lines */
#define SLAB_POISON         0x0002      /* Poison objects for debugging */
#define SLAB_RED_ZONE       0x0004      /* Add red zones for overflow detection */
#define SLAB_STORE_USER     0x0008      /* Store allocation caller info */
#define SLAB_PANIC          0x0010      /* Panic on allocation failure */
#define SLAB_DESTROY_BY_RCU 0x0020      /* Defer destruction until RCU */

typedef uint32_t slab_flags_t;

/* ========================== Memory Compression ========================== */

/* Compression algorithms */
typedef enum {
    COMPRESSION_NONE    = 0,            /* No compression */
    COMPRESSION_LZ4     = 1,            /* LZ4 compression (fast) */
    COMPRESSION_ZSTD    = 2,            /* ZSTD compression (balanced) */
    COMPRESSION_LZO     = 3,            /* LZO compression (very fast) */
    COMPRESSION_GZIP    = 4,            /* GZIP compression (high ratio) */
    COMPRESSION_AUTO    = 5             /* Automatic selection */
} compression_type_t;

/* Memory protection flags */
#define MEM_PROT_READ       0x0001      /* Read protection */
#define MEM_PROT_WRITE      0x0002      /* Write protection */
#define MEM_PROT_EXEC       0x0004      /* Execute protection */
#define MEM_PROT_GUARD      0x0008      /* Guard page */
#define MEM_PROT_STACK      0x0010      /* Stack protection */
#define MEM_PROT_HEAP       0x0020      /* Heap protection */

typedef uint32_t protection_flags_t;

/* ========================== NUMA Support ========================== */

#define MAX_NUMA_NODES      64          /* Maximum NUMA nodes */

/* NUMA allocation policies */
typedef enum {
    NUMA_POLICY_DEFAULT = 0,           /* System default policy */
    NUMA_POLICY_BIND    = 1,           /* Bind to specific nodes */
    NUMA_POLICY_PREFERRED = 2,         /* Prefer specific nodes */
    NUMA_POLICY_INTERLEAVE = 3,        /* Interleave across nodes */
    NUMA_POLICY_LOCAL   = 4            /* Local node preference */
} numa_policy_t;

typedef uint64_t cpu_mask_t;           /* CPU affinity mask */

/* ========================== Core Data Structures ========================== */

/* Memory zone structure */
typedef struct memory_zone {
    uint64_t               start_pfn;            /* Start page frame number */
    uint64_t               end_pfn;              /* End page frame number */
    zone_type_t            type;                 /* Zone type */
    
    /* Free page tracking */
    struct free_area {
        struct page*       free_list;            /* Free page list */
        uint64_t           nr_free;              /* Number of free pages */
    } free_area[11];                            /* Orders 0-10 (1-1024 pages) */
    
    uint64_t               free_pages;           /* Total free pages */
    uint64_t               total_pages;          /* Total pages in zone */
    
    /* Watermarks for memory reclaim */
    uint64_t               watermark_min;        /* Minimum free pages */
    uint64_t               watermark_low;        /* Low watermark */
    uint64_t               watermark_high;       /* High watermark */
    
    /* NUMA node */
    int                    numa_node;            /* NUMA node ID */
    
    /* Statistics */
    struct {
        uint64_t           allocations;          /* Total allocations */
        uint64_t           failures;             /* Allocation failures */
        uint64_t           reclaim_attempts;     /* Memory reclaim attempts */
        uint64_t           reclaimed_pages;      /* Pages reclaimed */
    } stats;
    
    /* Synchronization */
    volatile int           lock;                 /* Zone lock */
} memory_zone_t;

/* Slab cache structure */
typedef struct kmem_cache {
    char                   name[64];             /* Cache name */
    size_t                 object_size;          /* Object size */
    size_t                 align;                /* Object alignment */
    slab_flags_t           flags;                /* Cache flags */
    
    void                   (*ctor)(void*);       /* Object constructor */
    void                   (*dtor)(void*);       /* Object destructor */
    
    /* Per-CPU caches */
    struct {
        void**             freelist;             /* Free object list */
        uint32_t           avail;                /* Available objects */
        uint32_t           limit;                /* Cache limit */
    } percpu_cache[32];                         /* Max 32 CPUs */
    
    /* Slab management */
    struct slab {
        void*              objects;              /* Object storage */
        uint32_t           inuse;                /* Objects in use */
        uint32_t           free;                 /* Free objects */
        struct slab*       next;                 /* Next slab */
    } *slabs_full, *slabs_partial, *slabs_empty;
    
    /* Statistics */
    struct {
        uint64_t           total_allocations;    /* Total allocations */
        uint64_t           total_frees;          /* Total frees */
        uint64_t           active_objects;       /* Currently active objects */
        uint64_t           peak_usage;           /* Peak object usage */
    } stats;
    
    /* List management */
    struct kmem_cache*     next;                 /* Next cache in list */
    
    /* Synchronization */
    volatile int           lock;                 /* Cache lock */
} kmem_cache_t;

/* Memory pool structure */
typedef struct mempool {
    int                    min_nr;               /* Minimum elements */
    int                    curr_nr;              /* Current elements */
    void**                 elements;             /* Element array */
    
    /* Allocation functions */
    void*                  (*alloc_fn)(gfp_t gfp_mask, void* pool_data);
    void                   (*free_fn)(void* element, void* pool_data);
    void*                  pool_data;            /* Pool-specific data */
    
    /* Waiters */
    struct {
        struct process*    process;              /* Waiting process */
        struct waiter*     next;                 /* Next waiter */
    } *waiters;
    
    /* Synchronization */
    volatile int           lock;                 /* Pool lock */
} mempool_t;

/* Compressed page structure */
typedef struct compressed_page {
    uint64_t               original_address;     /* Original virtual address */
    uint32_t               compressed_size;      /* Size after compression */
    uint32_t               original_size;        /* Original page size (4KB) */
    compression_type_t     compression_type;     /* Algorithm used */
    
    /* Compressed data storage */
    union {
        uint8_t*           data_ptr;             /* Pointer to compressed data */
        uint8_t            inline_data[64];      /* Small inline storage */
    };
    
    /* Metadata */
    uint64_t               access_time;          /* Last access time */
    uint32_t               access_count;         /* Access frequency */
    uint32_t               compression_ratio;    /* Ratio * 100 (e.g., 250 = 2.5x) */
    
    /* List management */
    struct compressed_page* next;               /* Next in list */
    struct compressed_page* prev;               /* Previous in list */
} compressed_page_t;

/* NUMA node structure */
typedef struct numa_node {
    int                    node_id;              /* NUMA node identifier */
    uint64_t               start_pfn;            /* Start page frame number */
    uint64_t               end_pfn;              /* End page frame number */
    
    /* Memory statistics */
    uint64_t               total_memory;         /* Total memory in bytes */
    uint64_t               free_memory;          /* Free memory in bytes */
    uint64_t               active_memory;        /* Active memory in bytes */
    uint64_t               inactive_memory;      /* Inactive memory in bytes */
    
    /* CPU affinity */
    cpu_mask_t             cpu_mask;             /* CPUs in this node */
    
    /* Distance matrix (cost to access other nodes) */
    uint32_t               distance[MAX_NUMA_NODES];
    
    /* Allocation policy */
    numa_policy_t          policy;               /* Default allocation policy */
    
    /* Statistics */
    struct {
        uint64_t           local_allocations;    /* Local allocations */
        uint64_t           remote_allocations;   /* Remote allocations */
        uint64_t           migrations_in;        /* Pages migrated in */
        uint64_t           migrations_out;       /* Pages migrated out */
    } stats;
} numa_node_t;

/* ========================== Statistics and Monitoring ========================== */

/* Memory statistics structure */
typedef struct memory_stats {
    /* General memory information */
    uint64_t               total_memory;         /* Total system memory */
    uint64_t               free_memory;          /* Free memory */
    uint64_t               used_memory;          /* Used memory */
    uint64_t               cached_memory;        /* Cached memory */
    uint64_t               buffered_memory;      /* Buffered memory */
    uint64_t               shared_memory;        /* Shared memory */
    
    /* Allocation statistics */
    uint64_t               total_allocations;    /* Total allocation requests */
    uint64_t               failed_allocations;   /* Failed allocations */
    uint64_t               allocation_size_total; /* Total allocated bytes */
    uint64_t               allocation_size_peak; /* Peak allocation size */
    
    /* Page fault statistics */
    uint64_t               page_faults_total;    /* Total page faults */
    uint64_t               page_faults_major;    /* Major page faults */
    uint64_t               page_faults_minor;    /* Minor page faults */
    uint64_t               page_faults_cow;      /* Copy-on-write faults */
    
    /* Swap statistics */
    uint64_t               swap_total;           /* Total swap space */
    uint64_t               swap_free;            /* Free swap space */
    uint64_t               pages_swapped_in;     /* Pages swapped in */
    uint64_t               pages_swapped_out;    /* Pages swapped out */
    
    /* Compression statistics */
    uint64_t               pages_compressed;     /* Compressed pages */
    uint64_t               pages_decompressed;   /* Decompressed pages */
    uint64_t               compression_ratio_avg; /* Average compression ratio */
    uint64_t               compression_time_total; /* Total compression time (us) */
    uint64_t               decompression_time_total; /* Total decompression time (us) */
    
    /* NUMA statistics */
    uint64_t               numa_local_allocations; /* Local NUMA allocations */
    uint64_t               numa_remote_allocations; /* Remote NUMA allocations */
    uint64_t               numa_migrations;      /* Page migrations */
    
    /* Fragmentation statistics */
    uint64_t               external_fragmentation; /* External fragmentation % */
    uint64_t               internal_fragmentation; /* Internal fragmentation % */
    
    /* Performance metrics */
    uint64_t               avg_allocation_time;  /* Average allocation time (ns) */
    uint64_t               avg_free_time;        /* Average free time (ns) */
} memory_stats_t;

/* Zone-specific statistics */
typedef struct zone_stats {
    zone_type_t            zone_type;            /* Zone type */
    uint64_t               total_pages;          /* Total pages */
    uint64_t               free_pages;           /* Free pages */
    uint64_t               used_pages;           /* Used pages */
    uint64_t               watermark_min;        /* Minimum watermark */
    uint64_t               watermark_low;        /* Low watermark */
    uint64_t               watermark_high;       /* High watermark */
    uint64_t               allocations;          /* Total allocations */
    uint64_t               failures;             /* Allocation failures */
    uint64_t               reclaim_attempts;     /* Reclaim attempts */
    uint64_t               reclaimed_pages;      /* Pages reclaimed */
} zone_stats_t;

/* NUMA node statistics */
typedef struct numa_stats {
    int                    node_id;              /* Node ID */
    uint64_t               total_memory;         /* Total memory */
    uint64_t               free_memory;          /* Free memory */
    uint64_t               local_allocations;    /* Local allocations */
    uint64_t               remote_allocations;   /* Remote allocations */
    uint64_t               migrations_in;        /* Migrations in */
    uint64_t               migrations_out;       /* Migrations out */
    cpu_mask_t             cpu_mask;             /* CPU affinity */
    uint32_t               avg_distance;         /* Average distance to other nodes */
} numa_stats_t;

/* ========================== Configuration ========================== */

/* Memory compression configuration */
typedef struct compression_config {
    bool                   enabled;              /* Compression enabled */
    compression_type_t     default_algorithm;    /* Default algorithm */
    uint32_t               compression_threshold; /* Min pages to compress */
    uint32_t               min_compression_ratio; /* Min ratio (%) to keep compressed */
    uint32_t               max_compression_time;  /* Max compression time (us) */
    bool                   async_compression;    /* Asynchronous compression */
} compression_config_t;

/* Memory monitoring configuration */
typedef struct monitor_config {
    bool                   enabled;              /* Monitoring enabled */
    uint32_t               sampling_interval;    /* Sampling interval (ms) */
    bool                   detailed_stats;       /* Collect detailed statistics */
    bool                   per_process_stats;    /* Per-process statistics */
    uint32_t               history_size;         /* History buffer size */
} monitor_config_t;

/* Buddy allocator statistics */
typedef struct buddy_allocator_stats {
    uint64_t               total_free_pages;     /* Total free pages */
    uint64_t               total_allocated_pages; /* Total allocated pages */
    uint64_t               allocations;          /* Number of allocations */
    uint64_t               deallocations;        /* Number of deallocations */
    uint64_t               merge_operations;     /* Buddy merge operations */
    uint64_t               split_operations;     /* Page split operations */
    uint64_t               external_fragmentation; /* External fragmentation % */
} buddy_allocator_stats_t;

/* Slab allocator statistics */
typedef struct slab_allocator_stats {
    uint64_t               active_caches;        /* Number of active caches */
    uint64_t               total_objects;        /* Total objects allocated */
    uint64_t               active_objects;       /* Currently active objects */
    uint64_t               allocations;          /* Number of allocations */
    uint64_t               deallocations;        /* Number of deallocations */
    uint64_t               cache_hits;           /* Cache hits */
    uint64_t               cache_misses;         /* Cache misses */
    uint64_t               memory_usage;         /* Total memory usage */
} slab_allocator_stats_t;

/* Swap entry type */
typedef struct swap_entry {
    unsigned long          val;                  /* Swap entry value */
} swap_entry_t;

/* ========================== Core Memory Management API ========================== */

/* Memory manager initialization */
int memory_manager_init(void);
void memory_manager_shutdown(void);

/* Basic memory allocation */
void* kmalloc_new(size_t size, gfp_t flags);
void* kmalloc_node(size_t size, gfp_t flags, int node);
void* kmalloc_zeroed(size_t size, gfp_t flags);
void* kmalloc_aligned(size_t size, size_t alignment, gfp_t flags);
void  kfree_new(const void* ptr);
void  kfree_sized(const void* ptr, size_t size);

/* Page allocation */
struct page* alloc_pages(gfp_t gfp_mask, unsigned int order);
struct page* alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order);
void __free_pages(struct page* page, unsigned int order);
unsigned long __get_free_page(gfp_t gfp_mask);
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
void free_page(unsigned long addr);
void free_pages(unsigned long addr, unsigned int order);

/* ========================== Slab Allocator API ========================== */

/* Cache management */
kmem_cache_t* kmem_cache_create(const char* name, size_t size, 
                                size_t align, slab_flags_t flags,
                                void (*ctor)(void*));
void kmem_cache_destroy(kmem_cache_t* cache);

/* Object allocation */
void* kmem_cache_alloc(kmem_cache_t* cache, gfp_t flags);
void* kmem_cache_alloc_node(kmem_cache_t* cache, gfp_t flags, int node);
void  kmem_cache_free(kmem_cache_t* cache, void* obj);

/* Cache information */
int   kmem_cache_shrink(kmem_cache_t* cache);
const char* kmem_cache_name(kmem_cache_t* cache);
size_t kmem_cache_size(kmem_cache_t* cache);

/* ========================== Memory Pool API ========================== */

/* Pool allocation functions */
typedef void* (*mempool_alloc_t)(gfp_t gfp_mask, void* pool_data);
typedef void  (*mempool_free_t)(void* element, void* pool_data);

/* Pool management */
mempool_t* mempool_create(int min_nr, mempool_alloc_t* alloc_fn,
                          mempool_free_t* free_fn, void* pool_data);
void mempool_destroy(mempool_t* pool);
int  mempool_resize(mempool_t* pool, int new_min_nr);

/* Pool allocation */
void* mempool_alloc(mempool_t* pool, gfp_t gfp_mask);
void  mempool_free(void* element, mempool_t* pool);

/* ========================== Memory Compression API ========================== */

/* Compression management */
int  enable_memory_compression(compression_config_t* config);
void disable_memory_compression(void);
bool is_memory_compression_enabled(void);

/* Page compression */
int  compress_page(struct page* page, compression_type_t type);
int  decompress_page(struct page* page);
bool is_page_compressed(struct page* page);

/* Compression statistics */
/* Compression statistics structure */
typedef struct compression_stats {
    uint64_t pages_compressed;
    uint64_t pages_decompressed;
    uint64_t compression_failures;
    uint64_t bytes_saved;
    uint64_t compression_ratio_percent;
} compression_stats_t;

/* Compression statistics */
void get_compression_stats(compression_stats_t* stats);

/* ========================== NUMA Support API ========================== */

/* NUMA topology */
int  get_numa_node_count(void);
int  get_current_numa_node(void);
int  cpu_to_node(int cpu);
bool node_online(int node);

/* NUMA policy */
void set_numa_policy(numa_policy_t policy);
numa_policy_t get_numa_policy(void);
int  set_mempolicy(numa_policy_t policy, const unsigned long* nodemask,
                   unsigned long maxnode);

/* NUMA allocation */
void* kmalloc_numa(size_t size, gfp_t flags, int preferred_node);
struct page* alloc_pages_numa(gfp_t gfp_mask, unsigned int order,
                              int preferred_node);

/* Page migration */
int migrate_pages(pid_t pid, unsigned long maxnode,
                  const unsigned long* old_nodes,
                  const unsigned long* new_nodes);
int move_pages(pid_t pid, unsigned long count, void** pages,
               const int* nodes, int* status, int flags);

/* ========================== Memory Protection API ========================== */

/* Memory protection */
int set_memory_protection(void* addr, size_t size, protection_flags_t flags);
int clear_memory_protection(void* addr, size_t size, protection_flags_t flags);

/* Stack protection */
int enable_stack_protection(pid_t pid);
int disable_stack_protection(pid_t pid);
int set_stack_guard_size(pid_t pid, size_t size);

/* Heap protection */
int enable_heap_protection(pid_t pid);
int disable_heap_protection(pid_t pid);
int check_heap_integrity(pid_t pid);

/* Memory debugging */
void enable_memory_debugging(void);
void disable_memory_debugging(void);
int  check_memory_leaks(void);

/* ========================== Statistics and Monitoring API ========================== */

/* Memory statistics */
void get_memory_stats(memory_stats_t* stats);
void get_zone_stats(int zone_id, zone_stats_t* stats);
void get_numa_stats(int node_id, numa_stats_t* stats);

/* Memory monitoring */
int  enable_memory_monitoring(monitor_config_t* config);
void disable_memory_monitoring(void);
int  get_memory_pressure_level(void);  /* Returns 0-100 */

/* Memory information */
/* Memory information structure */
typedef struct memory_info {
    uint64_t total_ram;
    uint64_t free_ram;
    uint64_t shared_ram;
    uint64_t buffer_ram;
    uint64_t cached_ram;
    uint64_t total_swap;
    uint64_t free_swap;
    uint64_t compressed_swap;
} memory_info_t;

/* Memory information */
int  get_memory_info(memory_info_t* info);

/* ========================== Internal Helper Functions ========================== */

/* Zone management */
memory_zone_t* get_zone(zone_type_t type);
int add_memory_zone(uint64_t start_pfn, uint64_t end_pfn, zone_type_t type);
void remove_memory_zone(memory_zone_t* zone);

/* Page management */
struct page* pfn_to_page(unsigned long pfn);
unsigned long page_to_pfn(struct page* page);
bool page_is_free(struct page* page);

/* Memory reclaim */
int try_to_free_pages(gfp_t gfp_mask, unsigned int order, int node);
void wakeup_kswapd(void);

/* Error codes */
#define ENOMEM_ADVANCED     (-1000)     /* Advanced memory manager error base */
#define EINVAL_ZONE         (-1001)     /* Invalid memory zone */
#define EINVAL_NODE         (-1002)     /* Invalid NUMA node */
#define ECOMPRESS           (-1003)     /* Compression failure */
#define EDECOMPRESS         (-1004)     /* Decompression failure */
#define EMIGRATE            (-1005)     /* Page migration failure */

#endif /* MEMORY_ADVANCED_H */
