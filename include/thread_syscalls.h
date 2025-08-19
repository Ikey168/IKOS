/*
 * IKOS Threading System Calls Header
 * Issue #52 - Multi-Threading & Concurrency Support
 * 
 * Kernel-space threading syscall interface and thread management
 */

#ifndef THREAD_SYSCALLS_H
#define THREAD_SYSCALLS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "process.h"

/* Thread system call numbers (extending from existing syscalls) */
#define SYS_THREAD_CREATE       720
#define SYS_THREAD_EXIT         721
#define SYS_THREAD_JOIN         722
#define SYS_THREAD_DETACH       723
#define SYS_THREAD_SELF         724
#define SYS_THREAD_YIELD        725
#define SYS_THREAD_SLEEP        726
#define SYS_THREAD_CANCEL       727
#define SYS_THREAD_KILL         728
#define SYS_THREAD_SETNAME      729

#define SYS_MUTEX_INIT          730
#define SYS_MUTEX_DESTROY       731
#define SYS_MUTEX_LOCK          732
#define SYS_MUTEX_TRYLOCK       733
#define SYS_MUTEX_UNLOCK        734
#define SYS_MUTEX_TIMEDLOCK     735

#define SYS_COND_INIT           740
#define SYS_COND_DESTROY        741
#define SYS_COND_WAIT           742
#define SYS_COND_TIMEDWAIT      743
#define SYS_COND_SIGNAL         744
#define SYS_COND_BROADCAST      745

#define SYS_SEM_INIT            750
#define SYS_SEM_DESTROY         751
#define SYS_SEM_WAIT            752
#define SYS_SEM_TRYWAIT         753
#define SYS_SEM_POST            754
#define SYS_SEM_GETVALUE        755
#define SYS_SEM_TIMEDWAIT       756

#define SYS_RWLOCK_INIT         760
#define SYS_RWLOCK_DESTROY      761
#define SYS_RWLOCK_RDLOCK       762
#define SYS_RWLOCK_WRLOCK       763
#define SYS_RWLOCK_UNLOCK       764
#define SYS_RWLOCK_TRYRDLOCK    765
#define SYS_RWLOCK_TRYWRLOCK    766

#define SYS_BARRIER_INIT        770
#define SYS_BARRIER_DESTROY     771
#define SYS_BARRIER_WAIT        772

#define SYS_SPINLOCK_INIT       780
#define SYS_SPINLOCK_DESTROY    781
#define SYS_SPINLOCK_LOCK       782
#define SYS_SPINLOCK_TRYLOCK    783
#define SYS_SPINLOCK_UNLOCK     784

#define SYS_TLS_CREATE_KEY      790
#define SYS_TLS_DELETE_KEY      791
#define SYS_TLS_GET_VALUE       792
#define SYS_TLS_SET_VALUE       793

#define SYS_THREAD_STATS        800
#define SYS_THREAD_LIST         801
#define SYS_THREAD_INFO         802

/* Thread error codes */
#define THREAD_SUCCESS          0
#define THREAD_ERROR           -1
#define THREAD_EAGAIN          -11
#define THREAD_EINVAL          -22
#define THREAD_EPERM           -1
#define THREAD_ESRCH           -3
#define THREAD_EDEADLK         -35
#define THREAD_ENOMEM          -12
#define THREAD_EBUSY           -16
#define THREAD_ETIMEDOUT       -110
#define THREAD_ENOTSUP         -95

/* Threading constants */
#define MAX_THREADS_PER_PROCESS 256
#define MAX_MUTEXES_PER_PROCESS 1024
#define MAX_SEMAPHORES_PER_PROCESS 256
#define MAX_CONDITION_VARS_PER_PROCESS 256
#define MAX_TLS_KEYS_GLOBAL     256

#define THREAD_NAME_MAX         32
#define THREAD_STACK_MIN        (16 * 1024)
#define THREAD_STACK_DEFAULT    (2 * 1024 * 1024)
#define THREAD_STACK_MAX        (8 * 1024 * 1024)

/* ================================
 * Thread Management Structures
 * ================================ */

/* Thread states */
typedef enum {
    KTHREAD_STATE_NEW = 0,          /* Thread created but not started */
    KTHREAD_STATE_READY,            /* Ready to run */
    KTHREAD_STATE_RUNNING,          /* Currently executing */
    KTHREAD_STATE_BLOCKED,          /* Blocked on synchronization object */
    KTHREAD_STATE_SLEEPING,         /* Sleeping */
    KTHREAD_STATE_TERMINATED,       /* Thread has terminated */
    KTHREAD_STATE_ZOMBIE            /* Terminated but not joined */
} kthread_state_t;

/* Thread Control Block (kernel-space) */
typedef struct kthread {
    uint32_t tid;                   /* Thread ID */
    uint32_t pid;                   /* Parent process ID */
    char name[THREAD_NAME_MAX];     /* Thread name */
    
    kthread_state_t state;          /* Current state */
    int priority;                   /* Thread priority */
    int policy;                     /* Scheduling policy */
    
    /* CPU context */
    process_context_t context;      /* Saved CPU context */
    
    /* Memory management */
    uint64_t stack_base;            /* Stack base address */
    uint64_t stack_size;            /* Stack size */
    uint64_t stack_guard;           /* Stack guard address */
    
    /* Entry point and argument */
    void* (*entry_point)(void*);    /* Thread entry function */
    void* arg;                      /* Thread argument */
    void* return_value;             /* Thread return value */
    
    /* Thread attributes */
    int detach_state;               /* Detached or joinable */
    size_t guard_size;              /* Guard area size */
    
    /* Cancellation */
    int cancel_state;               /* Cancellation state */
    int cancel_type;                /* Cancellation type */
    bool cancel_pending;            /* Cancellation request pending */
    
    /* Thread-local storage */
    void* tls_data[MAX_TLS_KEYS_GLOBAL];  /* TLS data array */
    
    /* Timing and statistics */
    uint64_t creation_time;         /* Creation timestamp */
    uint64_t cpu_time;              /* Total CPU time used */
    uint64_t context_switches;      /* Number of context switches */
    uint64_t last_run_time;         /* Last time thread ran */
    
    /* Synchronization */
    void* blocking_on;              /* Object thread is blocked on */
    uint32_t blocking_type;         /* Type of blocking object */
    struct kthread* blocker_next;   /* Next in blocker queue */
    
    /* Parent process */
    process_t* process;             /* Parent process */
    
    /* Thread list management */
    struct kthread* next;           /* Next thread in process */
    struct kthread* prev;           /* Previous thread in process */
    struct kthread* next_global;    /* Next in global thread list */
    struct kthread* prev_global;    /* Previous in global thread list */
    
    /* Scheduler integration */
    struct kthread* sched_next;     /* Next in scheduler queue */
    struct kthread* sched_prev;     /* Previous in scheduler queue */
    uint32_t time_slice;            /* Remaining time slice */
    uint32_t quantum;               /* Original time quantum */
    
    /* Join support */
    struct kthread* joiner;         /* Thread waiting to join */
    bool joined;                    /* Has been joined */
    
    /* Cleanup handlers */
    struct cleanup_handler* cleanup_stack;  /* Cleanup handler stack */
} kthread_t;

/* Cleanup handler structure */
struct cleanup_handler {
    void (*routine)(void*);         /* Cleanup routine */
    void* arg;                      /* Routine argument */
    struct cleanup_handler* next;   /* Next handler in stack */
};

/* ================================
 * Synchronization Structures
 * ================================ */

/* Kernel mutex structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t type;                  /* Mutex type */
    uint32_t owner_tid;             /* Owner thread ID */
    uint32_t lock_count;            /* Lock count (for recursive mutexes) */
    uint32_t waiters_count;         /* Number of waiting threads */
    kthread_t* wait_queue_head;     /* Head of wait queue */
    kthread_t* wait_queue_tail;     /* Tail of wait queue */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t flags;                 /* Mutex flags */
} kmutex_t;

/* Kernel condition variable structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t waiters_count;         /* Number of waiting threads */
    kthread_t* wait_queue_head;     /* Head of wait queue */
    kthread_t* wait_queue_tail;     /* Tail of wait queue */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t flags;                 /* Condition variable flags */
    uint32_t broadcast_seq;         /* Broadcast sequence number */
} kcond_t;

/* Kernel semaphore structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t value;                 /* Current semaphore value */
    uint32_t max_value;             /* Maximum semaphore value */
    uint32_t waiters_count;         /* Number of waiting threads */
    kthread_t* wait_queue_head;     /* Head of wait queue */
    kthread_t* wait_queue_tail;     /* Tail of wait queue */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t flags;                 /* Semaphore flags */
} ksem_t;

/* Kernel read-write lock structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t readers;               /* Number of active readers */
    uint32_t writers;               /* Number of active writers (0 or 1) */
    uint32_t read_waiters;          /* Number of threads waiting to read */
    uint32_t write_waiters;         /* Number of threads waiting to write */
    uint32_t writer_tid;            /* ID of thread holding write lock */
    kthread_t* read_wait_queue;     /* Queue of threads waiting to read */
    kthread_t* write_wait_queue;    /* Queue of threads waiting to write */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t flags;                 /* Lock flags */
} krwlock_t;

/* Kernel barrier structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t count;                 /* Total number of threads */
    uint32_t waiting;               /* Number of waiting threads */
    uint32_t generation;            /* Current generation number */
    kthread_t* wait_queue_head;     /* Head of wait queue */
    kthread_t* wait_queue_tail;     /* Tail of wait queue */
    uint64_t creation_time;         /* Creation timestamp */
} kbarrier_t;

/* Kernel spinlock structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    volatile uint32_t lock;         /* Lock variable */
    uint32_t owner_tid;             /* Owner thread ID */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t spin_count;            /* Spin count for debugging */
} kspinlock_t;

/* ================================
 * Thread System Call Functions
 * ================================ */

/* Thread management syscalls */
int sys_thread_create(void* (*start_routine)(void*), void* arg, uint32_t* tid,
                      const pthread_attr_t* attr);
void sys_thread_exit(void* retval);
int sys_thread_join(uint32_t tid, void** retval);
int sys_thread_detach(uint32_t tid);
uint32_t sys_thread_self(void);
int sys_thread_yield(void);
int sys_thread_sleep(uint64_t nanoseconds);
int sys_thread_cancel(uint32_t tid);
int sys_thread_kill(uint32_t tid, int sig);
int sys_thread_setname(uint32_t tid, const char* name);

/* Mutex syscalls */
int sys_mutex_init(uint32_t* mutex_id, const pthread_mutexattr_t* attr);
int sys_mutex_destroy(uint32_t mutex_id);
int sys_mutex_lock(uint32_t mutex_id);
int sys_mutex_trylock(uint32_t mutex_id);
int sys_mutex_unlock(uint32_t mutex_id);
int sys_mutex_timedlock(uint32_t mutex_id, const struct timespec* abstime);

/* Condition variable syscalls */
int sys_cond_init(uint32_t* cond_id, const pthread_condattr_t* attr);
int sys_cond_destroy(uint32_t cond_id);
int sys_cond_wait(uint32_t cond_id, uint32_t mutex_id);
int sys_cond_timedwait(uint32_t cond_id, uint32_t mutex_id, const struct timespec* abstime);
int sys_cond_signal(uint32_t cond_id);
int sys_cond_broadcast(uint32_t cond_id);

/* Semaphore syscalls */
int sys_sem_init(uint32_t* sem_id, int pshared, unsigned int value);
int sys_sem_destroy(uint32_t sem_id);
int sys_sem_wait(uint32_t sem_id);
int sys_sem_trywait(uint32_t sem_id);
int sys_sem_post(uint32_t sem_id);
int sys_sem_getvalue(uint32_t sem_id, int* sval);
int sys_sem_timedwait(uint32_t sem_id, const struct timespec* abs_timeout);

/* Read-write lock syscalls */
int sys_rwlock_init(uint32_t* rwlock_id, const pthread_rwlockattr_t* attr);
int sys_rwlock_destroy(uint32_t rwlock_id);
int sys_rwlock_rdlock(uint32_t rwlock_id);
int sys_rwlock_wrlock(uint32_t rwlock_id);
int sys_rwlock_unlock(uint32_t rwlock_id);
int sys_rwlock_tryrdlock(uint32_t rwlock_id);
int sys_rwlock_trywrlock(uint32_t rwlock_id);

/* Barrier syscalls */
int sys_barrier_init(uint32_t* barrier_id, const pthread_barrierattr_t* attr, unsigned int count);
int sys_barrier_destroy(uint32_t barrier_id);
int sys_barrier_wait(uint32_t barrier_id);

/* Spinlock syscalls */
int sys_spinlock_init(uint32_t* lock_id, int pshared);
int sys_spinlock_destroy(uint32_t lock_id);
int sys_spinlock_lock(uint32_t lock_id);
int sys_spinlock_trylock(uint32_t lock_id);
int sys_spinlock_unlock(uint32_t lock_id);

/* Thread-local storage syscalls */
int sys_tls_create_key(uint32_t* key, void (*destructor)(void*));
int sys_tls_delete_key(uint32_t key);
void* sys_tls_get_value(uint32_t key);
int sys_tls_set_value(uint32_t key, const void* value);

/* Statistics and monitoring syscalls */
int sys_thread_stats(pthread_stats_t* stats);
int sys_thread_list(uint32_t* threads, size_t max_count);
int sys_thread_info(uint32_t tid, void* info, size_t info_size);

/* ================================
 * Thread Management Functions
 * ================================ */

/* Core thread functions */
int thread_system_init(void);
kthread_t* thread_create_kernel(process_t* proc, void* (*entry)(void*), void* arg,
                               const pthread_attr_t* attr);
int thread_destroy_kernel(kthread_t* thread);
int thread_schedule_kernel(kthread_t* thread);

/* Thread state management */
int thread_set_state(kthread_t* thread, kthread_state_t state);
kthread_state_t thread_get_state(kthread_t* thread);
kthread_t* thread_get_current(void);
kthread_t* thread_get_by_tid(uint32_t tid);

/* Thread queue management */
int thread_enqueue_wait(kthread_t* thread, void* sync_object, uint32_t sync_type);
int thread_dequeue_wait(kthread_t* thread);
kthread_t* thread_dequeue_first_waiter(void* sync_object);
int thread_wake_all_waiters(void* sync_object);

/* Thread context switching */
void thread_context_switch(kthread_t* prev, kthread_t* next);
void thread_save_context(kthread_t* thread);
void thread_restore_context(kthread_t* thread);

/* Thread memory management */
int thread_setup_stack(kthread_t* thread, size_t stack_size);
int thread_cleanup_stack(kthread_t* thread);
int thread_check_stack_overflow(kthread_t* thread);

/* Thread scheduling integration */
int thread_integrate_with_scheduler(void);
void thread_scheduler_tick(void);
kthread_t* thread_scheduler_pick_next(void);

/* Thread cleanup */
int thread_cleanup_process_threads(process_t* proc);
int thread_cleanup_thread_resources(kthread_t* thread);

/* ================================
 * Synchronization Object Management
 * ================================ */

/* Mutex management */
kmutex_t* mutex_allocate(void);
int mutex_deallocate(kmutex_t* mutex);
int mutex_add_waiter(kmutex_t* mutex, kthread_t* thread);
kthread_t* mutex_remove_waiter(kmutex_t* mutex);

/* Condition variable management */
kcond_t* cond_allocate(void);
int cond_deallocate(kcond_t* cond);
int cond_add_waiter(kcond_t* cond, kthread_t* thread);
kthread_t* cond_remove_waiter(kcond_t* cond);

/* Semaphore management */
ksem_t* sem_allocate(void);
int sem_deallocate(ksem_t* sem);
int sem_add_waiter(ksem_t* sem, kthread_t* thread);
kthread_t* sem_remove_waiter(ksem_t* sem);

/* Read-write lock management */
krwlock_t* rwlock_allocate(void);
int rwlock_deallocate(krwlock_t* rwlock);
int rwlock_add_reader_waiter(krwlock_t* rwlock, kthread_t* thread);
int rwlock_add_writer_waiter(krwlock_t* rwlock, kthread_t* thread);

/* ================================
 * Thread-Local Storage Management
 * ================================ */

typedef struct {
    bool in_use;                    /* Key is allocated */
    void (*destructor)(void*);      /* Destructor function */
    uint64_t creation_time;         /* Creation timestamp */
} tls_key_t;

/* TLS functions */
int tls_system_init(void);
int tls_allocate_key(uint32_t* key, void (*destructor)(void*));
int tls_deallocate_key(uint32_t key);
int tls_set_thread_value(kthread_t* thread, uint32_t key, const void* value);
void* tls_get_thread_value(kthread_t* thread, uint32_t key);
int tls_cleanup_thread(kthread_t* thread);

/* ================================
 * Thread Statistics
 * ================================ */

typedef struct {
    uint64_t threads_created;       /* Total threads created */
    uint64_t threads_destroyed;     /* Total threads destroyed */
    uint64_t context_switches;      /* Total context switches */
    uint64_t mutex_operations;      /* Total mutex operations */
    uint64_t cond_operations;       /* Total condition variable operations */
    uint64_t sem_operations;        /* Total semaphore operations */
    uint64_t rwlock_operations;     /* Total read-write lock operations */
    uint64_t spinlock_operations;   /* Total spinlock operations */
    uint64_t total_cpu_time;        /* Total CPU time used by all threads */
    uint64_t total_wait_time;       /* Total time spent waiting */
    uint32_t active_threads;        /* Currently active threads */
    uint32_t blocked_threads;       /* Currently blocked threads */
} thread_kernel_stats_t;

/* Statistics functions */
int thread_stats_init(void);
int thread_stats_get(thread_kernel_stats_t* stats);
int thread_stats_reset(void);
void thread_stats_update_context_switch(void);
void thread_stats_update_mutex_op(void);
void thread_stats_update_cond_op(void);
void thread_stats_update_sem_op(void);

/* ================================
 * Thread Validation and Debugging
 * ================================ */

/* Validation functions */
bool thread_validate_tid(uint32_t tid);
bool thread_validate_mutex_id(uint32_t mutex_id);
bool thread_validate_cond_id(uint32_t cond_id);
bool thread_validate_sem_id(uint32_t sem_id);

/* Debugging functions */
void thread_debug_print_info(kthread_t* thread);
void thread_debug_print_all_threads(void);
void thread_debug_print_sync_objects(void);

/* Deadlock detection */
int thread_deadlock_detect(void);
int thread_deadlock_resolve(void);

#endif /* THREAD_SYSCALLS_H */
