/* IKOS User Space Memory Management System - Issue #29
 * Provides comprehensive virtual memory management for user space applications
 * including memory mapping, shared memory, copy-on-write, and memory protection
 */

#ifndef USER_SPACE_MEMORY_H
#define USER_SPACE_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Basic atomic types for user space testing */
typedef struct { volatile int counter; } atomic_t;
typedef struct { volatile long counter; } atomic64_t;

/* Atomic operations */
static inline int atomic_read(const atomic_t *v) { return v->counter; }
static inline void atomic_set(atomic_t *v, int i) { v->counter = i; }
static inline long atomic64_read(const atomic64_t *v) { return v->counter; }
static inline void atomic64_set(atomic64_t *v, long i) { v->counter = i; }

/* System types */
typedef int key_t;
typedef int pid_t;
typedef long off_t;
typedef unsigned int mode_t;
typedef long time_t;

/* IPC structures */
struct shmid_ds {
    key_t shm_perm_key;
    size_t shm_segsz;
    pid_t shm_cpid;
    pid_t shm_lpid;
    unsigned short shm_nattch;
    time_t shm_atime;
    time_t shm_dtime;
    time_t shm_ctime;
};

/* IPC constants */
#define IPC_PRIVATE    0
#define IPC_CREAT      01000
#define IPC_RMID       0

/* File operation constants */
#define O_CREAT        0x40
#define O_RDWR         0x02

/* Resource limit constants */
#define RLIMIT_AS      9
#include <sys/types.h>

/* Forward declarations */
struct file;
struct process;
struct page;

/* ========================== Constants and Flags ========================== */

/* Memory mapping protection flags */
#define PROT_NONE       0x0             /* No access */
#define PROT_READ       0x1             /* Read access */
#define PROT_WRITE      0x2             /* Write access */
#define PROT_EXEC       0x4             /* Execute access */

/* Memory mapping flags */
#define MAP_SHARED      0x01            /* Share changes */
#define MAP_PRIVATE     0x02            /* Private copy-on-write */
#define MAP_FIXED       0x10            /* Interpret addr exactly */
#define MAP_ANONYMOUS   0x20            /* Anonymous mapping */
#define MAP_POPULATE    0x8000          /* Populate (prefault) pagetables */
#define MAP_NONBLOCK    0x10000         /* Do not block on IO */
#define MAP_STACK       0x20000         /* Stack-like segment */
#define MAP_HUGETLB     0x40000         /* Create a huge page mapping */

/* Memory sync flags */
#define MS_ASYNC        0x1             /* Asynchronous sync */
#define MS_SYNC         0x4             /* Synchronous sync */
#define MS_INVALIDATE   0x2             /* Invalidate caches */

/* Memory advice flags */
#define MADV_NORMAL     0x0             /* Default behavior */
#define MADV_RANDOM     0x1             /* Random access pattern */
#define MADV_SEQUENTIAL 0x2             /* Sequential access pattern */
#define MADV_WILLNEED   0x3             /* Will need these pages */
#define MADV_DONTNEED   0x4             /* Don't need these pages */
#define MADV_HUGEPAGE   0xE             /* Use huge pages */
#define MADV_NOHUGEPAGE 0xF             /* Don't use huge pages */

/* VMA flags */
#define VM_READ         0x00000001      /* Can read */
#define VM_WRITE        0x00000002      /* Can write */
#define VM_EXEC         0x00000004      /* Can execute */
#define VM_SHARED       0x00000008      /* Shared between processes */
#define VM_GROWSDOWN    0x00000100      /* Stack-like segment */
#define VM_GROWSUP      0x00000200      /* Heap-like segment */
#define VM_LOCKED       0x00002000      /* Pages are locked in memory */
#define VM_DONTCOPY     0x00020000      /* Don't copy on fork */
#define VM_DONTEXPAND   0x00040000      /* Cannot expand */
#define VM_ACCOUNT      0x00100000      /* Account for memory usage */
#define VM_HUGEPAGE     0x00400000      /* Use huge pages */
#define VM_COW          0x00800000      /* Copy-on-write */

/* Shared memory flags */
#define SHM_RDONLY      0x1000          /* Read-only access */
#define SHM_RND         0x2000          /* Round address to page boundary */
#define SHM_REMAP       0x4000          /* Remap segment */

/* Memory lock flags */
#define MCL_CURRENT     0x1             /* Lock current pages */
#define MCL_FUTURE      0x2             /* Lock future pages */

/* Resource limits */
#define RLIM_NLIMITS    16              /* Number of resource limits */
#define RLIMIT_AS       9               /* Address space limit */
#define RLIMIT_DATA     2               /* Data segment limit */
#define RLIMIT_STACK    3               /* Stack size limit */
#define RLIMIT_RSS      5               /* Resident set size limit */
#define RLIMIT_MEMLOCK  8               /* Locked memory limit */

/* ========================== Data Structures ========================== */

/* Virtual Memory Area structure */
typedef struct vm_area_struct {
    uint64_t               vm_start;             /* Start virtual address */
    uint64_t               vm_end;               /* End virtual address (exclusive) */
    uint32_t               vm_flags;             /* VMA flags (permissions, type) */
    uint32_t               vm_prot;              /* Protection flags */
    
    /* File mapping information */
    struct file*           vm_file;              /* Mapped file (if any) */
    uint64_t               vm_pgoff;             /* File offset in pages */
    
    /* Memory management */
    struct mm_struct*      vm_mm;                /* Associated address space */
    struct vm_area_struct* vm_next;             /* Next VMA in list */
    struct vm_area_struct* vm_prev;             /* Previous VMA in list */
    
    /* Red-black tree for fast lookup */
    struct vm_area_struct* vm_rb_node;          /* RB tree node */
    
    /* Operations */
    struct vm_operations*  vm_ops;               /* VMA operations */
    void*                  vm_private_data;     /* Private data */
    
    /* Reference counting */
    atomic_t               vm_usage;             /* Reference count */
    
    /* COW support */
    atomic_t               vm_shared_count;      /* Shared page count */
} vm_area_struct_t;

/* VMA operations structure */
typedef struct vm_operations {
    void (*open)(vm_area_struct_t* vma);
    void (*close)(vm_area_struct_t* vma);
    int  (*fault)(vm_area_struct_t* vma, uint64_t address);
    int  (*page_mkwrite)(vm_area_struct_t* vma, struct page* page);
    int  (*access)(vm_area_struct_t* vma, uint64_t address, void* buf, int len, int write);
} vm_operations_t;

/* Resource limit structure */
typedef struct rlimit {
    uint64_t               rlim_cur;             /* Current (soft) limit */
    uint64_t               rlim_max;             /* Maximum (hard) limit */
} rlimit_t;

/* Memory management structure */
typedef struct mm_struct {
    /* Virtual memory areas */
    vm_area_struct_t*      mmap;                 /* VMA list head */
    vm_area_struct_t*      mmap_cache;           /* Last accessed VMA */
    uint32_t               map_count;            /* Number of VMAs */
    
    /* Red-black tree for fast VMA lookup */
    void*                  mm_rb;                /* RB tree root */
    
    /* Address space layout */
    uint64_t               task_size;            /* Task virtual address space size */
    uint64_t               start_code;           /* Code segment start */
    uint64_t               end_code;             /* Code segment end */
    uint64_t               start_data;           /* Data segment start */
    uint64_t               end_data;             /* Data segment end */
    uint64_t               start_brk;            /* Heap start */
    uint64_t               brk;                  /* Current heap end */
    uint64_t               start_stack;          /* Stack start */
    uint64_t               mmap_base;            /* Memory mapping base */
    uint64_t               mmap_legacy_base;     /* Legacy mmap base */
    
    /* Page table */
    uint64_t*              pgd;                  /* Page global directory */
    
    /* Memory accounting */
    atomic64_t             total_vm;             /* Total virtual memory (pages) */
    atomic64_t             locked_vm;            /* Locked virtual memory (pages) */
    atomic64_t             pinned_vm;            /* Pinned virtual memory (pages) */
    atomic64_t             data_vm;              /* Data virtual memory (pages) */
    atomic64_t             exec_vm;              /* Executable virtual memory (pages) */
    atomic64_t             stack_vm;             /* Stack virtual memory (pages) */
    atomic64_t             reserved_vm;          /* Reserved virtual memory (pages) */
    atomic64_t             committed_vm;         /* Committed virtual memory (pages) */
    
    /* Physical memory usage */
    atomic64_t             rss_stat[4];          /* RSS statistics */
    atomic64_t             anon_rss;             /* Anonymous RSS */
    atomic64_t             file_rss;             /* File-backed RSS */
    atomic64_t             shmem_rss;            /* Shared memory RSS */
    
    /* Memory limits */
    rlimit_t               rlim[RLIM_NLIMITS];   /* Resource limits */
    
    /* Address space randomization */
    uint64_t               mmap_rnd_bits;        /* mmap randomization bits */
    uint64_t               mmap_rnd_compat_bits; /* 32-bit compat randomization */
    
    /* Reference counting */
    atomic_t               mm_users;             /* Users of this mm */
    atomic_t               mm_count;             /* Reference count */
    
    /* Synchronization */
    volatile int           mmap_lock;            /* Memory map lock */
    volatile int           page_table_lock;     /* Page table lock */
    
    /* Flags and state */
    uint32_t               flags;                /* MM flags */
    uint32_t               def_flags;            /* Default VMA flags */
    
    /* NUMA policy */
    void*                  mempolicy;            /* NUMA memory policy */
    
    /* Context information */
    struct process*        owner;                /* Owning process */
} mm_struct_t;

/* Shared memory segment structure */
typedef struct shm_segment {
    uint32_t               shm_id;               /* Segment ID */
    key_t                  shm_key;              /* Segment key */
    size_t                 shm_size;             /* Segment size */
    uint32_t               shm_perm;             /* Permissions */
    uid_t                  shm_cuid;             /* Creator UID */
    gid_t                  shm_cgid;             /* Creator GID */
    uid_t                  shm_uid;              /* Owner UID */
    gid_t                  shm_gid;              /* Owner GID */
    pid_t                  shm_cpid;             /* Creator PID */
    pid_t                  shm_lpid;             /* Last operation PID */
    time_t                 shm_atime;            /* Last attach time */
    time_t                 shm_dtime;            /* Last detach time */
    time_t                 shm_ctime;            /* Creation time */
    
    /* Physical pages */
    struct page**          shm_pages;            /* Array of pages */
    uint32_t               shm_nattch;           /* Number of attachments */
    
    /* Reference counting */
    atomic_t               shm_refcount;         /* Reference count */
    
    /* List management */
    struct shm_segment*    shm_next;             /* Next segment */
    struct shm_segment*    shm_prev;             /* Previous segment */
    
    /* Synchronization */
    volatile int           shm_lock;             /* Segment lock */
} shm_segment_t;

/* Memory usage statistics */
typedef struct memory_usage {
    uint64_t               vsize;                /* Virtual memory size */
    uint64_t               rss;                  /* Resident set size */
    uint64_t               shared;               /* Shared memory */
    uint64_t               text;                 /* Text (code) size */
    uint64_t               data;                 /* Data size */
    uint64_t               stack;                /* Stack size */
    uint64_t               locked;               /* Locked memory */
    uint64_t               pinned;               /* Pinned memory */
    uint64_t               swap;                 /* Swapped memory */
    uint64_t               anon;                 /* Anonymous memory */
    uint64_t               file;                 /* File-backed memory */
    uint64_t               shmem;                /* Shared memory segments */
} memory_usage_t;

/* Memory pressure information */
typedef struct memory_pressure {
    int                    level;                /* Pressure level (0-100) */
    uint64_t               available;            /* Available memory */
    uint64_t               threshold_low;        /* Low pressure threshold */
    uint64_t               threshold_medium;     /* Medium pressure threshold */
    uint64_t               threshold_high;       /* High pressure threshold */
    uint64_t               reclaim_rate;         /* Memory reclaim rate */
} memory_pressure_t;

/* ========================== Core Memory Management API ========================== */

/* Memory management initialization */
int usmm_init(void);
void usmm_shutdown(void);

/* Address space management */
mm_struct_t* mm_alloc(void);
void mm_free(mm_struct_t* mm);
mm_struct_t* mm_copy(mm_struct_t* oldmm);
int mm_init(mm_struct_t* mm, struct process* task);

/* VMA management */
vm_area_struct_t* find_vma(mm_struct_t* mm, uint64_t addr);
vm_area_struct_t* find_vma_prev(mm_struct_t* mm, uint64_t addr, vm_area_struct_t** prev);
vm_area_struct_t* find_vma_intersection(mm_struct_t* mm, uint64_t start, uint64_t end);

/* VMA operations */
int insert_vm_area(mm_struct_t* mm, vm_area_struct_t* vma);
int remove_vm_area(mm_struct_t* mm, vm_area_struct_t* vma);
int split_vma(mm_struct_t* mm, vm_area_struct_t* vma, uint64_t addr, int new_below);
vm_area_struct_t* merge_vma(mm_struct_t* mm, vm_area_struct_t* prev, 
                           uint64_t addr, uint64_t end, uint32_t vm_flags);

/* ========================== Memory Mapping API ========================== */

/* Memory mapping system calls */
void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int sys_munmap(void* addr, size_t length);
int sys_mprotect(void* addr, size_t length, int prot);
int sys_msync(void* addr, size_t length, int flags);
int sys_madvise(void* addr, size_t length, int advice);

/* Memory locking */
int sys_mlock(const void* addr, size_t length);
int sys_munlock(const void* addr, size_t length);
int sys_mlockall(int flags);
int sys_munlockall(void);

/* Memory information */
int sys_mincore(void* addr, size_t length, unsigned char* vec);

/* Heap management */
void* sys_brk(void* addr);
void* sys_sbrk(intptr_t increment);

/* ========================== Shared Memory API ========================== */

/* POSIX shared memory */
int sys_shm_open(const char* name, int oflag, mode_t mode);
int sys_shm_unlink(const char* name);

/* System V shared memory */
int sys_shmget(key_t key, size_t size, int shmflg);
void* sys_shmat(int shmid, const void* shmaddr, int shmflg);
int sys_shmdt(const void* shmaddr);
int sys_shmctl(int shmid, int cmd, struct shmid_ds* buf);

/* Shared memory management */
shm_segment_t* shm_create(key_t key, size_t size, int shmflg);
shm_segment_t* shm_find(int shmid);
int shm_attach(shm_segment_t* shm, struct process* task, void* addr, int shmflg);
int shm_detach(shm_segment_t* shm, struct process* task, void* addr);
void shm_destroy(shm_segment_t* shm);

/* ========================== Copy-on-Write API ========================== */

/* COW page management */
int cow_page_fault(vm_area_struct_t* vma, uint64_t address);
int setup_cow_mapping(vm_area_struct_t* vma);
void cow_page_dup(struct page* page);
void cow_page_free(struct page* page);

/* Fork support */
int copy_mm(struct process* child, struct process* parent);
void exit_mm(struct process* task);

/* ========================== Memory Protection API ========================== */

/* Memory protection */
int apply_page_protection(uint64_t addr, size_t length, int prot);
int check_page_protection(uint64_t addr, int access_type);

/* Stack protection */
int setup_stack_guard(mm_struct_t* mm);
int check_stack_growth(mm_struct_t* mm, uint64_t addr);

/* Heap protection */
int setup_heap_protection(mm_struct_t* mm);
int check_heap_corruption(mm_struct_t* mm);

/* Address space layout randomization */
uint64_t arch_randomize_brk(mm_struct_t* mm);
uint64_t arch_get_unmapped_area(void* addr, uint64_t len, uint64_t pgoff, uint64_t flags, uint64_t flags2);/* ========================== Memory Accounting API ========================== */

/* Memory usage tracking */
int get_memory_usage(pid_t pid, memory_usage_t* usage);
int update_memory_usage(mm_struct_t* mm, long pages, int type);

/* Memory limits */
int set_memory_limit(pid_t pid, int resource, const rlimit_t* rlim);
int get_memory_limit(pid_t pid, int resource, rlimit_t* rlim);
int check_memory_limit(mm_struct_t* mm, size_t size, int type);

/* Memory pressure */
int register_memory_pressure_callback(void (*callback)(memory_pressure_t* pressure));
int get_memory_pressure(memory_pressure_t* pressure);
void trigger_memory_reclaim(int level);

/* OOM killer */
int oom_kill_select_victim(void);
void oom_kill_process(struct process* victim);
int oom_score_badness(struct process* task);

/* ========================== Page Fault Handling ========================== */

/* Page fault handlers */
int handle_mm_fault(mm_struct_t* mm, vm_area_struct_t* vma, uint64_t address, uint32_t flags);
int handle_page_fault(uint64_t address, uint32_t error_code);

/* Fault types */
#define FAULT_FLAG_WRITE       0x01    /* Write fault */
#define FAULT_FLAG_MKWRITE     0x02    /* Make page writable */
#define FAULT_FLAG_ALLOW_RETRY 0x04    /* Allow retry */
#define FAULT_FLAG_RETRY_NOWAIT 0x08   /* Don't wait for retry */
#define FAULT_FLAG_KILLABLE    0x10    /* Process can be killed */
#define FAULT_FLAG_TRIED       0x20    /* Second try */

/* ========================== Statistics and Monitoring ========================== */

/* Memory statistics */
typedef struct usmm_stats {
    uint64_t               total_mappings;       /* Total memory mappings */
    uint64_t               total_unmappings;     /* Total memory unmappings */
    uint64_t               anonymous_mappings;   /* Anonymous mappings */
    uint64_t               file_mappings;        /* File-backed mappings */
    uint64_t               shared_mappings;      /* Shared mappings */
    uint64_t               cow_pages;            /* COW pages */
    uint64_t               page_faults;          /* Page faults handled */
    uint64_t               major_faults;         /* Major page faults */
    uint64_t               minor_faults;         /* Minor page faults */
    uint64_t               cow_faults;           /* COW page faults */
    uint64_t               oom_kills;            /* OOM kills performed */
    uint64_t               mmap_calls;           /* mmap system calls */
    uint64_t               munmap_calls;         /* munmap system calls */
    uint64_t               mprotect_calls;       /* mprotect system calls */
    uint64_t               shmget_calls;         /* shmget system calls */
    uint64_t               shmat_calls;          /* shmat system calls */
    uint64_t               shmdt_calls;          /* shmdt system calls */
    uint64_t               shmctl_calls;         /* shmctl system calls */
} usmm_stats_t;

/* Statistics collection */
void get_usmm_stats(usmm_stats_t* stats);
void reset_usmm_stats(void);

/* Memory monitoring */
int enable_memory_monitoring(void);
void disable_memory_monitoring(void);
int get_process_memory_info(pid_t pid, memory_usage_t* info);

/* ========================== Debugging and Diagnostics ========================== */

/* Memory debugging */
void dump_vma_list(mm_struct_t* mm);
void dump_memory_layout(mm_struct_t* mm);
int validate_mm_struct(mm_struct_t* mm);
int check_vma_consistency(mm_struct_t* mm);

/* Memory leak detection */
int enable_memory_leak_detection(void);
void disable_memory_leak_detection(void);
void dump_memory_leaks(void);

/* Performance profiling */
int enable_memory_profiling(void);
void disable_memory_profiling(void);
void dump_memory_profile(void);

/* ========================== Utility Functions ========================== */

/* Address space utilities */
bool vma_contains_addr(vm_area_struct_t* vma, uint64_t addr);
bool vma_overlaps_range(vm_area_struct_t* vma, uint64_t start, uint64_t end);
uint64_t vma_size(vm_area_struct_t* vma);

/* Page utilities */
uint64_t addr_to_page(uint64_t addr);
uint64_t page_to_addr(uint64_t page);
uint64_t round_up_to_page(uint64_t addr);
uint64_t round_down_to_page(uint64_t addr);

/* Protection utilities */
uint32_t prot_to_vm_flags(int prot);
int vm_flags_to_prot(uint32_t vm_flags);
bool can_access_vma(vm_area_struct_t* vma, int access_type);

/* COW statistics */
typedef struct {
    uint64_t cow_pages_created;
    uint64_t cow_pages_copied;
    uint64_t cow_faults_handled;
    uint64_t memory_saved;
} cow_stats_t;

void get_cow_stats(cow_stats_t* stats);

/* ========================== Error Codes ========================== */

#define USMM_SUCCESS           0       /* Success */
#define USMM_ENOMEM           -12      /* Out of memory */
#define USMM_EACCES           -13      /* Permission denied */
#define USMM_EFAULT           -14      /* Bad address */
#define USMM_EINVAL           -22      /* Invalid argument */
#define USMM_ENFILE           -23      /* File table overflow */
#define USMM_EMFILE           -24      /* Too many open files */
#define USMM_ENOSYS           -38      /* Function not implemented */
#define USMM_EOVERFLOW        -75      /* Value too large */

#endif /* USER_SPACE_MEMORY_H */
