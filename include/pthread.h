/*
 * IKOS User-Space Threading API Header
 * Issue #52 - Multi-Threading & Concurrency Support
 * 
 * Provides pthread-like threading API for user applications
 * with mutexes, semaphores, and condition variables.
 */

#ifndef PTHREAD_H
#define PTHREAD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Thread identification */
typedef uint32_t pthread_t;
typedef uint32_t pthread_key_t;

/* Thread states */
typedef enum {
    THREAD_STATE_NEW = 0,           /* Thread created but not started */
    THREAD_STATE_READY,             /* Ready to run */
    THREAD_STATE_RUNNING,           /* Currently executing */
    THREAD_STATE_BLOCKED,           /* Blocked on synchronization object */
    THREAD_STATE_SLEEPING,          /* Sleeping */
    THREAD_STATE_TERMINATED,        /* Thread has terminated */
    THREAD_STATE_ZOMBIE             /* Terminated but not joined */
} thread_state_t;

/* Thread priorities */
typedef enum {
    THREAD_PRIORITY_IDLE = 0,       /* Lowest priority */
    THREAD_PRIORITY_LOW = 1,        /* Low priority */
    THREAD_PRIORITY_NORMAL = 2,     /* Normal priority */
    THREAD_PRIORITY_HIGH = 3,       /* High priority */
    THREAD_PRIORITY_REALTIME = 4    /* Highest priority */
} thread_priority_t;

/* Thread scheduling policies */
typedef enum {
    SCHED_OTHER = 0,                /* Default time-sharing policy */
    SCHED_FIFO,                     /* First-in-first-out real-time policy */
    SCHED_RR                        /* Round-robin real-time policy */
} sched_policy_t;

/* Thread attributes */
typedef struct {
    size_t stack_size;              /* Stack size */
    void* stack_addr;               /* Stack address (NULL for automatic) */
    int detach_state;               /* Detached state */
    int inherit_sched;              /* Inherit scheduling from parent */
    int sched_policy;               /* Scheduling policy */
    int sched_priority;             /* Scheduling priority */
    int scope;                      /* Contention scope */
    size_t guard_size;              /* Guard area size */
} pthread_attr_t;

/* Detach states */
#define PTHREAD_CREATE_JOINABLE     0
#define PTHREAD_CREATE_DETACHED     1

/* Scheduling inheritance */
#define PTHREAD_INHERIT_SCHED       0
#define PTHREAD_EXPLICIT_SCHED      1

/* Contention scope */
#define PTHREAD_SCOPE_SYSTEM        0
#define PTHREAD_SCOPE_PROCESS       1

/* Default values */
#define PTHREAD_STACK_MIN           (16 * 1024)     /* 16KB minimum stack */
#define PTHREAD_STACK_DEFAULT       (2 * 1024 * 1024)  /* 2MB default stack */

/* Thread-local storage */
#define PTHREAD_KEYS_MAX            256
#define PTHREAD_DESTRUCTOR_ITERATIONS 4

/* ================================
 * Mutex Support
 * ================================ */

/* Mutex types */
typedef enum {
    PTHREAD_MUTEX_NORMAL = 0,       /* Normal mutex */
    PTHREAD_MUTEX_RECURSIVE,        /* Recursive mutex */
    PTHREAD_MUTEX_ERRORCHECK        /* Error-checking mutex */
} mutex_type_t;

/* Mutex attributes */
typedef struct {
    int type;                       /* Mutex type */
    int pshared;                    /* Process-shared */
    int protocol;                   /* Priority inheritance protocol */
    int prioceiling;                /* Priority ceiling */
} pthread_mutexattr_t;

/* Mutex structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t type;                  /* Mutex type */
    uint32_t owner;                 /* Owner thread ID */
    uint32_t lock_count;            /* Lock count (for recursive mutexes) */
    uint32_t waiters;               /* Number of waiting threads */
    void* wait_queue;               /* Queue of waiting threads */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t flags;                 /* Mutex flags */
} pthread_mutex_t;

/* Mutex constants */
#define PTHREAD_MUTEX_INITIALIZER   {0x4D555458, PTHREAD_MUTEX_NORMAL, 0, 0, 0, NULL, 0, 0}
#define PTHREAD_PROCESS_PRIVATE     0
#define PTHREAD_PROCESS_SHARED      1
#define PTHREAD_PRIO_NONE           0
#define PTHREAD_PRIO_INHERIT        1
#define PTHREAD_PRIO_PROTECT        2

/* ================================
 * Condition Variable Support
 * ================================ */

/* Condition variable attributes */
typedef struct {
    int pshared;                    /* Process-shared */
    int clock_id;                   /* Clock ID for timed waits */
} pthread_condattr_t;

/* Condition variable structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t waiters;               /* Number of waiting threads */
    void* wait_queue;               /* Queue of waiting threads */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t flags;                 /* Condition variable flags */
    uint32_t broadcast_seq;         /* Broadcast sequence number */
} pthread_cond_t;

/* Condition variable constants */
#define PTHREAD_COND_INITIALIZER    {0x434F4E44, 0, NULL, 0, 0, 0}

/* ================================
 * Semaphore Support
 * ================================ */

/* Semaphore structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t value;                 /* Current semaphore value */
    uint32_t max_value;             /* Maximum semaphore value */
    uint32_t waiters;               /* Number of waiting threads */
    void* wait_queue;               /* Queue of waiting threads */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t flags;                 /* Semaphore flags */
} sem_t;

/* Semaphore constants */
#define SEM_VALUE_MAX               32767
#define SEM_FAILED                  ((sem_t*)-1)

/* ================================
 * Read-Write Lock Support
 * ================================ */

/* Read-write lock attributes */
typedef struct {
    int pshared;                    /* Process-shared */
} pthread_rwlockattr_t;

/* Read-write lock structure */
typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t readers;               /* Number of active readers */
    uint32_t writers;               /* Number of active writers (0 or 1) */
    uint32_t read_waiters;          /* Number of threads waiting to read */
    uint32_t write_waiters;         /* Number of threads waiting to write */
    uint32_t writer_id;             /* ID of thread holding write lock */
    void* read_wait_queue;          /* Queue of threads waiting to read */
    void* write_wait_queue;         /* Queue of threads waiting to write */
    uint64_t creation_time;         /* Creation timestamp */
    uint32_t flags;                 /* Lock flags */
} pthread_rwlock_t;

/* Read-write lock constants */
#define PTHREAD_RWLOCK_INITIALIZER  {0x52574C4B, 0, 0, 0, 0, 0, NULL, NULL, 0, 0}

/* ================================
 * Thread Cancellation
 * ================================ */

/* Cancellation states */
#define PTHREAD_CANCEL_ENABLE       0
#define PTHREAD_CANCEL_DISABLE      1

/* Cancellation types */
#define PTHREAD_CANCEL_DEFERRED     0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1

/* Special return value */
#define PTHREAD_CANCELED            ((void*)-1)

/* ================================
 * Error Codes
 * ================================ */

#define EAGAIN          11          /* Resource temporarily unavailable */
#define EINVAL          22          /* Invalid argument */
#define EPERM           1           /* Operation not permitted */
#define ESRCH           3           /* No such process */
#define EDEADLK         35          /* Resource deadlock avoided */
#define ENOMEM          12          /* Cannot allocate memory */
#define EBUSY           16          /* Device or resource busy */
#define ETIMEDOUT       110         /* Connection timed out */
#define ENOTSUP         95          /* Operation not supported */

/* ================================
 * Core Threading Functions
 * ================================ */

/* Thread management */
int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg);
int pthread_join(pthread_t thread, void** retval);
int pthread_detach(pthread_t thread);
void pthread_exit(void* retval);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);
int pthread_cancel(pthread_t thread);
int pthread_kill(pthread_t thread, int sig);

/* Thread attributes */
int pthread_attr_init(pthread_attr_t* attr);
int pthread_attr_destroy(pthread_attr_t* attr);
int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate);
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stacksize);
int pthread_attr_setstack(pthread_attr_t* attr, void* stackaddr, size_t stacksize);
int pthread_attr_getstack(const pthread_attr_t* attr, void** stackaddr, size_t* stacksize);
int pthread_attr_setguardsize(pthread_attr_t* attr, size_t guardsize);
int pthread_attr_getguardsize(const pthread_attr_t* attr, size_t* guardsize);

/* Thread scheduling */
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param);
int pthread_getschedparam(pthread_t thread, int* policy, struct sched_param* param);
int pthread_setschedprio(pthread_t thread, int prio);
int pthread_setconcurrency(int level);
int pthread_getconcurrency(void);

/* Thread cancellation */
int pthread_setcancelstate(int state, int* oldstate);
int pthread_setcanceltype(int type, int* oldtype);
void pthread_testcancel(void);
int pthread_cleanup_push(void (*routine)(void*), void* arg);
int pthread_cleanup_pop(int execute);

/* ================================
 * Mutex Functions
 * ================================ */

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutex_timedlock(pthread_mutex_t* mutex, const struct timespec* abstime);

/* Mutex attributes */
int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type);
int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t* attr, int* pshared);

/* ================================
 * Condition Variable Functions
 * ================================ */

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                           const struct timespec* abstime);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);

/* Condition variable attributes */
int pthread_condattr_init(pthread_condattr_t* attr);
int pthread_condattr_destroy(pthread_condattr_t* attr);
int pthread_condattr_setpshared(pthread_condattr_t* attr, int pshared);
int pthread_condattr_getpshared(const pthread_condattr_t* attr, int* pshared);

/* ================================
 * Semaphore Functions
 * ================================ */

int sem_init(sem_t* sem, int pshared, unsigned int value);
int sem_destroy(sem_t* sem);
int sem_wait(sem_t* sem);
int sem_trywait(sem_t* sem);
int sem_timedwait(sem_t* sem, const struct timespec* abs_timeout);
int sem_post(sem_t* sem);
int sem_getvalue(sem_t* sem, int* sval);

/* Named semaphores */
sem_t* sem_open(const char* name, int oflag, ...);
int sem_close(sem_t* sem);
int sem_unlink(const char* name);

/* ================================
 * Read-Write Lock Functions
 * ================================ */

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_timedrdlock(pthread_rwlock_t* rwlock, const struct timespec* abstime);
int pthread_rwlock_timedwrlock(pthread_rwlock_t* rwlock, const struct timespec* abstime);

/* Read-write lock attributes */
int pthread_rwlockattr_init(pthread_rwlockattr_t* attr);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr, int pshared);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* attr, int* pshared);

/* ================================
 * Thread-Local Storage
 * ================================ */

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);

/* ================================
 * Thread Barriers
 * ================================ */

typedef struct {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t count;                 /* Total number of threads */
    uint32_t waiting;               /* Number of waiting threads */
    uint32_t generation;            /* Current generation number */
    void* wait_queue;               /* Queue of waiting threads */
    uint64_t creation_time;         /* Creation timestamp */
} pthread_barrier_t;

typedef struct {
    int pshared;                    /* Process-shared */
} pthread_barrierattr_t;

#define PTHREAD_BARRIER_SERIAL_THREAD  1

int pthread_barrier_init(pthread_barrier_t* barrier, const pthread_barrierattr_t* attr,
                         unsigned int count);
int pthread_barrier_destroy(pthread_barrier_t* barrier);
int pthread_barrier_wait(pthread_barrier_t* barrier);

int pthread_barrierattr_init(pthread_barrierattr_t* attr);
int pthread_barrierattr_destroy(pthread_barrierattr_t* attr);
int pthread_barrierattr_setpshared(pthread_barrierattr_t* attr, int pshared);
int pthread_barrierattr_getpshared(const pthread_barrierattr_t* attr, int* pshared);

/* ================================
 * Thread-Once Initialization
 * ================================ */

typedef struct {
    uint32_t magic;                 /* Magic number */
    uint32_t state;                 /* Initialization state */
    pthread_mutex_t mutex;          /* Mutex for synchronization */
} pthread_once_t;

#define PTHREAD_ONCE_INIT           {0x4F4E4345, 0, PTHREAD_MUTEX_INITIALIZER}

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));

/* ================================
 * Spin Locks
 * ================================ */

typedef struct {
    uint32_t magic;                 /* Magic number */
    volatile uint32_t lock;         /* Lock variable */
    uint32_t owner;                 /* Owner thread ID */
    uint64_t creation_time;         /* Creation timestamp */
} pthread_spinlock_t;

int pthread_spin_init(pthread_spinlock_t* lock, int pshared);
int pthread_spin_destroy(pthread_spinlock_t* lock);
int pthread_spin_lock(pthread_spinlock_t* lock);
int pthread_spin_trylock(pthread_spinlock_t* lock);
int pthread_spin_unlock(pthread_spinlock_t* lock);

/* ================================
 * Time Support Structures
 * ================================ */

#ifndef _TIMESPEC_DEFINED
struct timespec {
    long tv_sec;                    /* Seconds */
    long tv_nsec;                   /* Nanoseconds */
};
#define _TIMESPEC_DEFINED
#endif

#ifndef _SCHED_PARAM_DEFINED
struct sched_param {
    int sched_priority;             /* Scheduling priority */
};
#define _SCHED_PARAM_DEFINED
#endif

/* ================================
 * IKOS-Specific Extensions
 * ================================ */

/* Thread statistics */
typedef struct {
    uint64_t total_threads_created;    /* Total threads created */
    uint64_t active_threads;           /* Currently active threads */
    uint64_t context_switches;         /* Total context switches */
    uint64_t mutex_contentions;        /* Mutex contention events */
    uint64_t condition_signals;        /* Condition variable signals */
    uint64_t semaphore_operations;     /* Semaphore operations */
    uint64_t total_cpu_time;           /* Total CPU time used by all threads */
    uint64_t idle_time;                /* Time spent idle */
} pthread_stats_t;

/* IKOS-specific functions */
int pthread_getstat(pthread_stats_t* stats);
int pthread_resetstat(void);
int pthread_setname_np(pthread_t thread, const char* name);
int pthread_getname_np(pthread_t thread, char* name, size_t len);
int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const void* cpuset);
int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, void* cpuset);

/* Thread debugging and monitoring */
int pthread_list_threads(pthread_t* threads, size_t max_count);
int pthread_get_thread_info(pthread_t thread, void* info, size_t info_size);

/* Performance optimization hints */
int pthread_yield(void);
int pthread_setconcurrency(int level);
int pthread_getconcurrency(void);

#endif /* PTHREAD_H */
