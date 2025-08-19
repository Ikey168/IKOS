/*
 * IKOS User-Space pthread Implementation
 * Issue #52 - Multi-Threading & Concurrency Support
 * 
 * User-space implementation of pthread API with syscall wrappers
 */

#include "../include/pthread.h"
#include "../include/syscalls.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* ================================
 * System Call Wrappers
 * ================================ */

/* Generic syscall function */
static inline long syscall(long number, ...) {
    long result;
    /* This is a simplified syscall implementation */
    /* In a real system, this would use the appropriate syscall instruction */
    asm volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number)
        : "memory"
    );
    return result;
}

static inline long syscall1(long number, long arg1) {
    long result;
    asm volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "D" (arg1)
        : "memory"
    );
    return result;
}

static inline long syscall2(long number, long arg1, long arg2) {
    long result;
    asm volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "D" (arg1), "S" (arg2)
        : "memory"
    );
    return result;
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
    long result;
    asm volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "D" (arg1), "S" (arg2), "d" (arg3)
        : "memory"
    );
    return result;
}

static inline long syscall4(long number, long arg1, long arg2, long arg3, long arg4) {
    long result;
    register long r10 asm("r10") = arg4;
    asm volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "D" (arg1), "S" (arg2), "d" (arg3), "r" (r10)
        : "memory"
    );
    return result;
}

/* ================================
 * Thread-Local Storage
 * ================================ */

static __thread bool pthread_lib_initialized = false;
static __thread pthread_t current_thread_id = 0;

/* ================================
 * Library Initialization
 * ================================ */

static int pthread_lib_init(void) {
    if (!pthread_lib_initialized) {
        /* Get current thread ID from kernel */
        current_thread_id = (pthread_t)syscall(SYS_THREAD_SELF);
        pthread_lib_initialized = true;
    }
    return 0;
}

/* ================================
 * Core Threading Functions
 * ================================ */

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg) {
    pthread_lib_init();
    
    uint32_t tid;
    int result = syscall4(SYS_THREAD_CREATE, (long)start_routine, (long)arg, 
                         (long)&tid, (long)attr);
    
    if (result == 0 && thread) {
        *thread = tid;
    }
    
    return result;
}

int pthread_join(pthread_t thread, void** retval) {
    pthread_lib_init();
    return syscall2(SYS_THREAD_JOIN, thread, (long)retval);
}

int pthread_detach(pthread_t thread) {
    pthread_lib_init();
    return syscall1(SYS_THREAD_DETACH, thread);
}

void pthread_exit(void* retval) {
    pthread_lib_init();
    syscall1(SYS_THREAD_EXIT, (long)retval);
    /* This should not return */
    while(1);
}

pthread_t pthread_self(void) {
    pthread_lib_init();
    return (pthread_t)syscall(SYS_THREAD_SELF);
}

int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2;
}

int pthread_cancel(pthread_t thread) {
    pthread_lib_init();
    return syscall1(SYS_THREAD_CANCEL, thread);
}

int pthread_kill(pthread_t thread, int sig) {
    pthread_lib_init();
    return syscall2(SYS_THREAD_KILL, thread, sig);
}

int pthread_yield(void) {
    pthread_lib_init();
    return syscall(SYS_THREAD_YIELD);
}

/* ================================
 * Thread Attributes
 * ================================ */

int pthread_attr_init(pthread_attr_t* attr) {
    if (!attr) return EINVAL;
    
    attr->stack_size = PTHREAD_STACK_DEFAULT;
    attr->stack_addr = NULL;
    attr->detach_state = PTHREAD_CREATE_JOINABLE;
    attr->inherit_sched = PTHREAD_INHERIT_SCHED;
    attr->sched_policy = SCHED_OTHER;
    attr->sched_priority = THREAD_PRIORITY_NORMAL;
    attr->scope = PTHREAD_SCOPE_SYSTEM;
    attr->guard_size = 4096;
    
    return 0;
}

int pthread_attr_destroy(pthread_attr_t* attr) {
    if (!attr) return EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate) {
    if (!attr) return EINVAL;
    if (detachstate != PTHREAD_CREATE_JOINABLE && 
        detachstate != PTHREAD_CREATE_DETACHED) {
        return EINVAL;
    }
    
    attr->detach_state = detachstate;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate) {
    if (!attr || !detachstate) return EINVAL;
    *detachstate = attr->detach_state;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize) {
    if (!attr) return EINVAL;
    if (stacksize < PTHREAD_STACK_MIN) return EINVAL;
    
    attr->stack_size = stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stacksize) {
    if (!attr || !stacksize) return EINVAL;
    *stacksize = attr->stack_size;
    return 0;
}

int pthread_attr_setstack(pthread_attr_t* attr, void* stackaddr, size_t stacksize) {
    if (!attr) return EINVAL;
    if (stacksize < PTHREAD_STACK_MIN) return EINVAL;
    
    attr->stack_addr = stackaddr;
    attr->stack_size = stacksize;
    return 0;
}

int pthread_attr_getstack(const pthread_attr_t* attr, void** stackaddr, size_t* stacksize) {
    if (!attr || !stackaddr || !stacksize) return EINVAL;
    *stackaddr = attr->stack_addr;
    *stacksize = attr->stack_size;
    return 0;
}

int pthread_attr_setguardsize(pthread_attr_t* attr, size_t guardsize) {
    if (!attr) return EINVAL;
    attr->guard_size = guardsize;
    return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t* attr, size_t* guardsize) {
    if (!attr || !guardsize) return EINVAL;
    *guardsize = attr->guard_size;
    return 0;
}

/* ================================
 * Thread Scheduling
 * ================================ */

int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param) {
    /* Not implemented yet */
    return ENOTSUP;
}

int pthread_getschedparam(pthread_t thread, int* policy, struct sched_param* param) {
    /* Not implemented yet */
    return ENOTSUP;
}

int pthread_setschedprio(pthread_t thread, int prio) {
    /* Not implemented yet */
    return ENOTSUP;
}

static int concurrency_level = 0;

int pthread_setconcurrency(int level) {
    if (level < 0) return EINVAL;
    concurrency_level = level;
    return 0;
}

int pthread_getconcurrency(void) {
    return concurrency_level;
}

/* ================================
 * Thread Cancellation
 * ================================ */

static __thread int cancel_state = PTHREAD_CANCEL_ENABLE;
static __thread int cancel_type = PTHREAD_CANCEL_DEFERRED;

int pthread_setcancelstate(int state, int* oldstate) {
    if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE) {
        return EINVAL;
    }
    
    if (oldstate) {
        *oldstate = cancel_state;
    }
    cancel_state = state;
    return 0;
}

int pthread_setcanceltype(int type, int* oldtype) {
    if (type != PTHREAD_CANCEL_DEFERRED && type != PTHREAD_CANCEL_ASYNCHRONOUS) {
        return EINVAL;
    }
    
    if (oldtype) {
        *oldtype = cancel_type;
    }
    cancel_type = type;
    return 0;
}

void pthread_testcancel(void) {
    /* Check for pending cancellation */
    /* This would require kernel support to check cancellation state */
}

/* ================================
 * Mutex Functions
 * ================================ */

static uint32_t get_mutex_kernel_id(pthread_mutex_t* mutex) {
    /* In a real implementation, we would map user-space mutex to kernel ID */
    /* For now, use a simple mapping */
    return (uint32_t)(uintptr_t)mutex;
}

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (!mutex) return EINVAL;
    
    pthread_lib_init();
    
    /* Initialize mutex structure */
    mutex->magic = 0x4D555458; // "MUTX"
    mutex->type = attr ? attr->type : PTHREAD_MUTEX_NORMAL;
    mutex->owner = 0;
    mutex->lock_count = 0;
    mutex->waiters = 0;
    mutex->wait_queue = NULL;
    mutex->creation_time = 0; /* Would get from kernel */
    mutex->flags = 0;
    
    /* Create kernel mutex */
    uint32_t kernel_id;
    int result = syscall2(SYS_MUTEX_INIT, (long)&kernel_id, (long)attr);
    if (result == 0) {
        /* Store kernel ID somehow - for now, just mark as initialized */
        mutex->flags |= 1; /* Initialized flag */
    }
    
    return result;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (!mutex || mutex->magic != 0x4D555458) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_mutex_kernel_id(mutex);
    int result = syscall1(SYS_MUTEX_DESTROY, kernel_id);
    
    if (result == 0) {
        memset(mutex, 0, sizeof(*mutex));
    }
    
    return result;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!mutex || mutex->magic != 0x4D555458) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_mutex_kernel_id(mutex);
    return syscall1(SYS_MUTEX_LOCK, kernel_id);
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (!mutex || mutex->magic != 0x4D555458) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_mutex_kernel_id(mutex);
    int result = syscall1(SYS_MUTEX_TRYLOCK, kernel_id);
    
    /* Convert kernel errors to pthread errors */
    if (result == THREAD_EBUSY) {
        return EBUSY;
    }
    
    return result;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!mutex || mutex->magic != 0x4D555458) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_mutex_kernel_id(mutex);
    return syscall1(SYS_MUTEX_UNLOCK, kernel_id);
}

int pthread_mutex_timedlock(pthread_mutex_t* mutex, const struct timespec* abstime) {
    if (!mutex || mutex->magic != 0x4D555458 || !abstime) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_mutex_kernel_id(mutex);
    return syscall2(SYS_MUTEX_TIMEDLOCK, kernel_id, (long)abstime);
}

/* ================================
 * Mutex Attributes
 * ================================ */

int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    if (!attr) return EINVAL;
    
    attr->type = PTHREAD_MUTEX_NORMAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    attr->protocol = PTHREAD_PRIO_NONE;
    attr->prioceiling = 0;
    
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    if (!attr) return EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (!attr) return EINVAL;
    if (type != PTHREAD_MUTEX_NORMAL && 
        type != PTHREAD_MUTEX_RECURSIVE && 
        type != PTHREAD_MUTEX_ERRORCHECK) {
        return EINVAL;
    }
    
    attr->type = type;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type) {
    if (!attr || !type) return EINVAL;
    *type = attr->type;
    return 0;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared) {
    if (!attr) return EINVAL;
    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED) {
        return EINVAL;
    }
    
    attr->pshared = pshared;
    return 0;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t* attr, int* pshared) {
    if (!attr || !pshared) return EINVAL;
    *pshared = attr->pshared;
    return 0;
}

/* ================================
 * Condition Variable Functions
 * ================================ */

static uint32_t get_cond_kernel_id(pthread_cond_t* cond) {
    return (uint32_t)(uintptr_t)cond;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    if (!cond) return EINVAL;
    
    pthread_lib_init();
    
    cond->magic = 0x434F4E44; // "COND"
    cond->waiters = 0;
    cond->wait_queue = NULL;
    cond->creation_time = 0;
    cond->flags = 0;
    cond->broadcast_seq = 0;
    
    uint32_t kernel_id;
    int result = syscall2(SYS_COND_INIT, (long)&kernel_id, (long)attr);
    if (result == 0) {
        cond->flags |= 1; /* Initialized flag */
    }
    
    return result;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
    if (!cond || cond->magic != 0x434F4E44) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_cond_kernel_id(cond);
    int result = syscall1(SYS_COND_DESTROY, kernel_id);
    
    if (result == 0) {
        memset(cond, 0, sizeof(*cond));
    }
    
    return result;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    if (!cond || cond->magic != 0x434F4E44 ||
        !mutex || mutex->magic != 0x4D555458) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t cond_id = get_cond_kernel_id(cond);
    uint32_t mutex_id = get_mutex_kernel_id(mutex);
    
    return syscall2(SYS_COND_WAIT, cond_id, mutex_id);
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                           const struct timespec* abstime) {
    if (!cond || cond->magic != 0x434F4E44 ||
        !mutex || mutex->magic != 0x4D555458 || !abstime) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t cond_id = get_cond_kernel_id(cond);
    uint32_t mutex_id = get_mutex_kernel_id(mutex);
    
    return syscall3(SYS_COND_TIMEDWAIT, cond_id, mutex_id, (long)abstime);
}

int pthread_cond_signal(pthread_cond_t* cond) {
    if (!cond || cond->magic != 0x434F4E44) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_cond_kernel_id(cond);
    return syscall1(SYS_COND_SIGNAL, kernel_id);
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    if (!cond || cond->magic != 0x434F4E44) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_cond_kernel_id(cond);
    return syscall1(SYS_COND_BROADCAST, kernel_id);
}

/* ================================
 * Condition Variable Attributes
 * ================================ */

int pthread_condattr_init(pthread_condattr_t* attr) {
    if (!attr) return EINVAL;
    
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    attr->clock_id = 0; /* CLOCK_REALTIME */
    
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t* attr) {
    if (!attr) return EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t* attr, int pshared) {
    if (!attr) return EINVAL;
    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED) {
        return EINVAL;
    }
    
    attr->pshared = pshared;
    return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t* attr, int* pshared) {
    if (!attr || !pshared) return EINVAL;
    *pshared = attr->pshared;
    return 0;
}

/* ================================
 * Semaphore Functions
 * ================================ */

static uint32_t get_sem_kernel_id(sem_t* sem) {
    return (uint32_t)(uintptr_t)sem;
}

int sem_init(sem_t* sem, int pshared, unsigned int value) {
    if (!sem || value > SEM_VALUE_MAX) return EINVAL;
    
    pthread_lib_init();
    
    sem->magic = 0x53454D41; // "SEMA"
    sem->value = value;
    sem->max_value = SEM_VALUE_MAX;
    sem->waiters = 0;
    sem->wait_queue = NULL;
    sem->creation_time = 0;
    sem->flags = 0;
    
    uint32_t kernel_id;
    int result = syscall3(SYS_SEM_INIT, (long)&kernel_id, pshared, value);
    if (result == 0) {
        sem->flags |= 1; /* Initialized flag */
    }
    
    return result;
}

int sem_destroy(sem_t* sem) {
    if (!sem || sem->magic != 0x53454D41) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_sem_kernel_id(sem);
    int result = syscall1(SYS_SEM_DESTROY, kernel_id);
    
    if (result == 0) {
        memset(sem, 0, sizeof(*sem));
    }
    
    return result;
}

int sem_wait(sem_t* sem) {
    if (!sem || sem->magic != 0x53454D41) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_sem_kernel_id(sem);
    return syscall1(SYS_SEM_WAIT, kernel_id);
}

int sem_trywait(sem_t* sem) {
    if (!sem || sem->magic != 0x53454D41) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_sem_kernel_id(sem);
    int result = syscall1(SYS_SEM_TRYWAIT, kernel_id);
    
    /* Convert kernel errors to standard errors */
    if (result == THREAD_EAGAIN) {
        return EAGAIN;
    }
    
    return result;
}

int sem_timedwait(sem_t* sem, const struct timespec* abs_timeout) {
    if (!sem || sem->magic != 0x53454D41 || !abs_timeout) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_sem_kernel_id(sem);
    return syscall2(SYS_SEM_TIMEDWAIT, kernel_id, (long)abs_timeout);
}

int sem_post(sem_t* sem) {
    if (!sem || sem->magic != 0x53454D41) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_sem_kernel_id(sem);
    return syscall1(SYS_SEM_POST, kernel_id);
}

int sem_getvalue(sem_t* sem, int* sval) {
    if (!sem || sem->magic != 0x53454D41 || !sval) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_id = get_sem_kernel_id(sem);
    return syscall2(SYS_SEM_GETVALUE, kernel_id, (long)sval);
}

/* Named semaphores - not implemented */
sem_t* sem_open(const char* name, int oflag, ...) {
    return SEM_FAILED;
}

int sem_close(sem_t* sem) {
    return ENOTSUP;
}

int sem_unlink(const char* name) {
    return ENOTSUP;
}

/* ================================
 * Thread-Local Storage
 * ================================ */

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    if (!key) return EINVAL;
    
    pthread_lib_init();
    
    uint32_t kernel_key;
    int result = syscall2(SYS_TLS_CREATE_KEY, (long)&kernel_key, (long)destructor);
    if (result == 0) {
        *key = kernel_key;
    }
    
    return result;
}

int pthread_key_delete(pthread_key_t key) {
    pthread_lib_init();
    return syscall1(SYS_TLS_DELETE_KEY, key);
}

void* pthread_getspecific(pthread_key_t key) {
    pthread_lib_init();
    return (void*)syscall1(SYS_TLS_GET_VALUE, key);
}

int pthread_setspecific(pthread_key_t key, const void* value) {
    pthread_lib_init();
    return syscall2(SYS_TLS_SET_VALUE, key, (long)value);
}

/* ================================
 * IKOS-Specific Extensions
 * ================================ */

int pthread_getstat(pthread_stats_t* stats) {
    if (!stats) return EINVAL;
    
    pthread_lib_init();
    return syscall1(SYS_THREAD_STATS, (long)stats);
}

int pthread_resetstat(void) {
    pthread_lib_init();
    /* Would need a separate syscall for this */
    return ENOTSUP;
}

int pthread_setname_np(pthread_t thread, const char* name) {
    if (!name) return EINVAL;
    
    pthread_lib_init();
    return syscall2(SYS_THREAD_SETNAME, thread, (long)name);
}

int pthread_getname_np(pthread_t thread, char* name, size_t len) {
    /* Not implemented yet */
    return ENOTSUP;
}

int pthread_list_threads(pthread_t* threads, size_t max_count) {
    if (!threads || max_count == 0) return EINVAL;
    
    pthread_lib_init();
    return syscall2(SYS_THREAD_LIST, (long)threads, max_count);
}

/* ================================
 * Simplified implementations for other functions
 * ================================ */

/* Read-write locks - simplified */
int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    if (!rwlock) return EINVAL;
    
    rwlock->magic = 0x52574C4B; // "RWLK"
    uint32_t kernel_id;
    return syscall2(SYS_RWLOCK_INIT, (long)&kernel_id, (long)attr);
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    if (!rwlock || rwlock->magic != 0x52574C4B) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)rwlock;
    return syscall1(SYS_RWLOCK_DESTROY, kernel_id);
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    if (!rwlock || rwlock->magic != 0x52574C4B) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)rwlock;
    return syscall1(SYS_RWLOCK_RDLOCK, kernel_id);
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    if (!rwlock || rwlock->magic != 0x52574C4B) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)rwlock;
    return syscall1(SYS_RWLOCK_WRLOCK, kernel_id);
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    if (!rwlock || rwlock->magic != 0x52574C4B) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)rwlock;
    return syscall1(SYS_RWLOCK_UNLOCK, kernel_id);
}

/* Barriers - simplified */
int pthread_barrier_init(pthread_barrier_t* barrier, const pthread_barrierattr_t* attr,
                         unsigned int count) {
    if (!barrier || count == 0) return EINVAL;
    
    barrier->magic = 0x42415252; // "BARR"
    barrier->count = count;
    uint32_t kernel_id;
    return syscall3(SYS_BARRIER_INIT, (long)&kernel_id, (long)attr, count);
}

int pthread_barrier_destroy(pthread_barrier_t* barrier) {
    if (!barrier || barrier->magic != 0x42415252) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)barrier;
    return syscall1(SYS_BARRIER_DESTROY, kernel_id);
}

int pthread_barrier_wait(pthread_barrier_t* barrier) {
    if (!barrier || barrier->magic != 0x42415252) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)barrier;
    int result = syscall1(SYS_BARRIER_WAIT, kernel_id);
    
    /* Return special value for the last thread */
    if (result == 1) {
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    
    return result;
}

/* Spinlocks - simplified */
int pthread_spin_init(pthread_spinlock_t* lock, int pshared) {
    if (!lock) return EINVAL;
    
    lock->magic = 0x5350494E; // "SPIN"
    lock->lock = 0;
    uint32_t kernel_id;
    return syscall2(SYS_SPINLOCK_INIT, (long)&kernel_id, pshared);
}

int pthread_spin_destroy(pthread_spinlock_t* lock) {
    if (!lock || lock->magic != 0x5350494E) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)lock;
    return syscall1(SYS_SPINLOCK_DESTROY, kernel_id);
}

int pthread_spin_lock(pthread_spinlock_t* lock) {
    if (!lock || lock->magic != 0x5350494E) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)lock;
    return syscall1(SYS_SPINLOCK_LOCK, kernel_id);
}

int pthread_spin_trylock(pthread_spinlock_t* lock) {
    if (!lock || lock->magic != 0x5350494E) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)lock;
    return syscall1(SYS_SPINLOCK_TRYLOCK, kernel_id);
}

int pthread_spin_unlock(pthread_spinlock_t* lock) {
    if (!lock || lock->magic != 0x5350494E) return EINVAL;
    
    uint32_t kernel_id = (uint32_t)(uintptr_t)lock;
    return syscall1(SYS_SPINLOCK_UNLOCK, kernel_id);
}

/* pthread_once - simplified implementation */
int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) return EINVAL;
    
    /* Simple implementation using atomic operations */
    if (__sync_bool_compare_and_swap(&once_control->state, 0, 1)) {
        init_routine();
        once_control->state = 2;
    } else {
        /* Wait for initialization to complete */
        while (once_control->state == 1) {
            pthread_yield();
        }
    }
    
    return 0;
}

/* Cleanup handlers - simplified */
int pthread_cleanup_push(void (*routine)(void*), void* arg) {
    /* Not implemented - would require kernel support for cleanup stacks */
    return ENOTSUP;
}

int pthread_cleanup_pop(int execute) {
    /* Not implemented */
    return ENOTSUP;
}

/* Attribute functions for other sync primitives */
int pthread_rwlockattr_init(pthread_rwlockattr_t* attr) {
    if (!attr) return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr) {
    if (!attr) return EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int pthread_barrierattr_init(pthread_barrierattr_t* attr) {
    if (!attr) return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_barrierattr_destroy(pthread_barrierattr_t* attr) {
    if (!attr) return EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

/* Remaining read-write lock and barrier functions */
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
    if (!rwlock || rwlock->magic != 0x52574C4B) return EINVAL;
    uint32_t kernel_id = (uint32_t)(uintptr_t)rwlock;
    return syscall1(SYS_RWLOCK_TRYRDLOCK, kernel_id);
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
    if (!rwlock || rwlock->magic != 0x52574C4B) return EINVAL;
    uint32_t kernel_id = (uint32_t)(uintptr_t)rwlock;
    return syscall1(SYS_RWLOCK_TRYWRLOCK, kernel_id);
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t* rwlock, const struct timespec* abstime) {
    /* Not implemented */
    return ENOTSUP;
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t* rwlock, const struct timespec* abstime) {
    /* Not implemented */
    return ENOTSUP;
}

int pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr, int pshared) {
    if (!attr) return EINVAL;
    attr->pshared = pshared;
    return 0;
}

int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* attr, int* pshared) {
    if (!attr || !pshared) return EINVAL;
    *pshared = attr->pshared;
    return 0;
}

int pthread_barrierattr_setpshared(pthread_barrierattr_t* attr, int pshared) {
    if (!attr) return EINVAL;
    attr->pshared = pshared;
    return 0;
}

int pthread_barrierattr_getpshared(const pthread_barrierattr_t* attr, int* pshared) {
    if (!attr || !pshared) return EINVAL;
    *pshared = attr->pshared;
    return 0;
}
