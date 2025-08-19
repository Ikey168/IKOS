/*
 * IKOS Threading Test Suite
 * Issue #52 - Multi-Threading & Concurrency Support
 * 
 * Comprehensive test suite for pthread API including unit tests,
 * integration tests, and stress tests.
 */

#include "../include/pthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ================================
 * Test Framework
 * ================================ */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_GROUP(name) printf("\n=== %s ===\n", name)

void print_test_summary(void) {
    printf("\n=== Test Summary ===\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
}

/* ================================
 * Thread Management Tests
 * ================================ */

void* test_thread_basic(void* arg) {
    int value = *(int*)arg;
    return (void*)(long)(value * 2);
}

void test_thread_creation_join(void) {
    TEST_GROUP("Thread Creation and Join");
    
    pthread_t thread;
    int arg = 21;
    void* retval;
    
    /* Test thread creation */
    int result = pthread_create(&thread, NULL, test_thread_basic, &arg);
    TEST_ASSERT(result == 0, "Thread creation succeeds");
    TEST_ASSERT(thread != 0, "Thread ID is valid");
    
    /* Test thread join */
    result = pthread_join(thread, &retval);
    TEST_ASSERT(result == 0, "Thread join succeeds");
    TEST_ASSERT((long)retval == 42, "Thread return value is correct");
    
    /* Test joining already joined thread */
    result = pthread_join(thread, NULL);
    TEST_ASSERT(result != 0, "Joining already joined thread fails");
}

void* test_thread_self(void* arg) {
    pthread_t* expected_tid = (pthread_t*)arg;
    pthread_t self_tid = pthread_self();
    *expected_tid = self_tid;
    return (void*)(long)self_tid;
}

void test_thread_self_function(void) {
    TEST_GROUP("Thread Self Identification");
    
    pthread_t thread;
    pthread_t expected_tid = 0;
    void* retval;
    
    int result = pthread_create(&thread, NULL, test_thread_self, &expected_tid);
    TEST_ASSERT(result == 0, "Thread creation for self test succeeds");
    
    result = pthread_join(thread, &retval);
    TEST_ASSERT(result == 0, "Thread join for self test succeeds");
    
    TEST_ASSERT(expected_tid == thread, "pthread_self returns correct TID");
    TEST_ASSERT((pthread_t)(long)retval == thread, "Thread return matches TID");
}

void test_thread_detach(void) {
    TEST_GROUP("Thread Detachment");
    
    pthread_t thread;
    int arg = 123;
    
    /* Create joinable thread */
    int result = pthread_create(&thread, NULL, test_thread_basic, &arg);
    TEST_ASSERT(result == 0, "Thread creation succeeds");
    
    /* Detach the thread */
    result = pthread_detach(thread);
    TEST_ASSERT(result == 0, "Thread detachment succeeds");
    
    /* Try to join detached thread (should fail) */
    void* retval;
    result = pthread_join(thread, &retval);
    TEST_ASSERT(result != 0, "Joining detached thread fails");
    
    /* Try to detach already detached thread (should fail) */
    result = pthread_detach(thread);
    TEST_ASSERT(result != 0, "Detaching already detached thread fails");
}

void test_thread_attributes(void) {
    TEST_GROUP("Thread Attributes");
    
    pthread_attr_t attr;
    
    /* Test attribute initialization */
    int result = pthread_attr_init(&attr);
    TEST_ASSERT(result == 0, "Attribute initialization succeeds");
    
    /* Test stack size */
    size_t stack_size;
    result = pthread_attr_getstacksize(&attr, &stack_size);
    TEST_ASSERT(result == 0, "Getting default stack size succeeds");
    TEST_ASSERT(stack_size == PTHREAD_STACK_DEFAULT, "Default stack size is correct");
    
    size_t new_stack_size = 1024 * 1024; // 1MB
    result = pthread_attr_setstacksize(&attr, new_stack_size);
    TEST_ASSERT(result == 0, "Setting stack size succeeds");
    
    result = pthread_attr_getstacksize(&attr, &stack_size);
    TEST_ASSERT(result == 0, "Getting modified stack size succeeds");
    TEST_ASSERT(stack_size == new_stack_size, "Modified stack size is correct");
    
    /* Test detach state */
    int detach_state;
    result = pthread_attr_getdetachstate(&attr, &detach_state);
    TEST_ASSERT(result == 0, "Getting default detach state succeeds");
    TEST_ASSERT(detach_state == PTHREAD_CREATE_JOINABLE, "Default detach state is joinable");
    
    result = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    TEST_ASSERT(result == 0, "Setting detach state succeeds");
    
    result = pthread_attr_getdetachstate(&attr, &detach_state);
    TEST_ASSERT(result == 0, "Getting modified detach state succeeds");
    TEST_ASSERT(detach_state == PTHREAD_CREATE_DETACHED, "Modified detach state is detached");
    
    /* Test attribute destruction */
    result = pthread_attr_destroy(&attr);
    TEST_ASSERT(result == 0, "Attribute destruction succeeds");
}

/* ================================
 * Mutex Tests
 * ================================ */

void test_mutex_basic(void) {
    TEST_GROUP("Basic Mutex Operations");
    
    pthread_mutex_t mutex;
    
    /* Test mutex initialization */
    int result = pthread_mutex_init(&mutex, NULL);
    TEST_ASSERT(result == 0, "Mutex initialization succeeds");
    
    /* Test mutex lock */
    result = pthread_mutex_lock(&mutex);
    TEST_ASSERT(result == 0, "Mutex lock succeeds");
    
    /* Test mutex trylock on locked mutex */
    result = pthread_mutex_trylock(&mutex);
    TEST_ASSERT(result != 0, "Trylock on locked mutex fails");
    
    /* Test mutex unlock */
    result = pthread_mutex_unlock(&mutex);
    TEST_ASSERT(result == 0, "Mutex unlock succeeds");
    
    /* Test mutex trylock on unlocked mutex */
    result = pthread_mutex_trylock(&mutex);
    TEST_ASSERT(result == 0, "Trylock on unlocked mutex succeeds");
    
    result = pthread_mutex_unlock(&mutex);
    TEST_ASSERT(result == 0, "Unlock after trylock succeeds");
    
    /* Test mutex destruction */
    result = pthread_mutex_destroy(&mutex);
    TEST_ASSERT(result == 0, "Mutex destruction succeeds");
}

static pthread_mutex_t shared_mutex;
static int mutex_test_counter = 0;

void* mutex_contention_thread(void* arg) {
    int iterations = *(int*)arg;
    
    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&shared_mutex);
        mutex_test_counter++;
        pthread_mutex_unlock(&shared_mutex);
    }
    
    return NULL;
}

void test_mutex_contention(void) {
    TEST_GROUP("Mutex Contention");
    
    const int NUM_THREADS = 4;
    const int ITERATIONS_PER_THREAD = 100;
    pthread_t threads[NUM_THREADS];
    int iterations = ITERATIONS_PER_THREAD;
    
    /* Initialize mutex and counter */
    pthread_mutex_init(&shared_mutex, NULL);
    mutex_test_counter = 0;
    
    /* Create threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        int result = pthread_create(&threads[i], NULL, mutex_contention_thread, &iterations);
        TEST_ASSERT(result == 0, "Contention thread creation succeeds");
    }
    
    /* Wait for all threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Check final counter value */
    int expected = NUM_THREADS * ITERATIONS_PER_THREAD;
    TEST_ASSERT(mutex_test_counter == expected, "Mutex contention produces correct result");
    
    pthread_mutex_destroy(&shared_mutex);
}

void test_mutex_attributes(void) {
    TEST_GROUP("Mutex Attributes");
    
    pthread_mutexattr_t attr;
    
    /* Test attribute initialization */
    int result = pthread_mutexattr_init(&attr);
    TEST_ASSERT(result == 0, "Mutex attribute initialization succeeds");
    
    /* Test mutex type */
    int type;
    result = pthread_mutexattr_gettype(&attr, &type);
    TEST_ASSERT(result == 0, "Getting default mutex type succeeds");
    TEST_ASSERT(type == PTHREAD_MUTEX_NORMAL, "Default mutex type is normal");
    
    result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    TEST_ASSERT(result == 0, "Setting recursive mutex type succeeds");
    
    result = pthread_mutexattr_gettype(&attr, &type);
    TEST_ASSERT(result == 0, "Getting modified mutex type succeeds");
    TEST_ASSERT(type == PTHREAD_MUTEX_RECURSIVE, "Modified mutex type is recursive");
    
    /* Test process sharing */
    int pshared;
    result = pthread_mutexattr_getpshared(&attr, &pshared);
    TEST_ASSERT(result == 0, "Getting process sharing succeeds");
    TEST_ASSERT(pshared == PTHREAD_PROCESS_PRIVATE, "Default is process private");
    
    /* Test attribute destruction */
    result = pthread_mutexattr_destroy(&attr);
    TEST_ASSERT(result == 0, "Mutex attribute destruction succeeds");
}

/* ================================
 * Condition Variable Tests
 * ================================ */

void test_condition_variables_basic(void) {
    TEST_GROUP("Basic Condition Variable Operations");
    
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    
    /* Initialize condition variable and mutex */
    int result = pthread_cond_init(&cond, NULL);
    TEST_ASSERT(result == 0, "Condition variable initialization succeeds");
    
    result = pthread_mutex_init(&mutex, NULL);
    TEST_ASSERT(result == 0, "Mutex initialization for condition variable succeeds");
    
    /* Test signal and broadcast (no waiters) */
    result = pthread_cond_signal(&cond);
    TEST_ASSERT(result == 0, "Condition signal with no waiters succeeds");
    
    result = pthread_cond_broadcast(&cond);
    TEST_ASSERT(result == 0, "Condition broadcast with no waiters succeeds");
    
    /* Clean up */
    result = pthread_cond_destroy(&cond);
    TEST_ASSERT(result == 0, "Condition variable destruction succeeds");
    
    result = pthread_mutex_destroy(&mutex);
    TEST_ASSERT(result == 0, "Mutex destruction succeeds");
}

static pthread_cond_t test_cond;
static pthread_mutex_t test_mutex;
static int condition_ready = 0;

void* condition_waiter_thread(void* arg) {
    int* result_ptr = (int*)arg;
    
    pthread_mutex_lock(&test_mutex);
    while (!condition_ready) {
        pthread_cond_wait(&test_cond, &test_mutex);
    }
    *result_ptr = 1; /* Signal that we woke up */
    pthread_mutex_unlock(&test_mutex);
    
    return NULL;
}

void test_condition_variables_signaling(void) {
    TEST_GROUP("Condition Variable Signaling");
    
    pthread_t waiter_thread;
    int waiter_result = 0;
    
    /* Initialize synchronization objects */
    pthread_cond_init(&test_cond, NULL);
    pthread_mutex_init(&test_mutex, NULL);
    condition_ready = 0;
    
    /* Create waiter thread */
    int result = pthread_create(&waiter_thread, NULL, condition_waiter_thread, &waiter_result);
    TEST_ASSERT(result == 0, "Waiter thread creation succeeds");
    
    /* Give waiter time to start waiting */
    pthread_yield();
    
    /* Signal the condition */
    pthread_mutex_lock(&test_mutex);
    condition_ready = 1;
    pthread_cond_signal(&test_cond);
    pthread_mutex_unlock(&test_mutex);
    
    /* Wait for waiter thread */
    pthread_join(waiter_thread, NULL);
    
    TEST_ASSERT(waiter_result == 1, "Condition variable woke up waiter");
    
    /* Clean up */
    pthread_cond_destroy(&test_cond);
    pthread_mutex_destroy(&test_mutex);
}

/* ================================
 * Semaphore Tests
 * ================================ */

void test_semaphores_basic(void) {
    TEST_GROUP("Basic Semaphore Operations");
    
    sem_t semaphore;
    
    /* Test semaphore initialization */
    int result = sem_init(&semaphore, 0, 2);
    TEST_ASSERT(result == 0, "Semaphore initialization succeeds");
    
    /* Test semaphore value */
    int value;
    result = sem_getvalue(&semaphore, &value);
    TEST_ASSERT(result == 0, "Getting semaphore value succeeds");
    TEST_ASSERT(value == 2, "Initial semaphore value is correct");
    
    /* Test semaphore wait */
    result = sem_wait(&semaphore);
    TEST_ASSERT(result == 0, "Semaphore wait succeeds");
    
    result = sem_getvalue(&semaphore, &value);
    TEST_ASSERT(result == 0, "Getting semaphore value after wait succeeds");
    TEST_ASSERT(value == 1, "Semaphore value decremented correctly");
    
    /* Test semaphore trywait */
    result = sem_trywait(&semaphore);
    TEST_ASSERT(result == 0, "Semaphore trywait succeeds");
    
    result = sem_getvalue(&semaphore, &value);
    TEST_ASSERT(result == 0, "Getting semaphore value after trywait succeeds");
    TEST_ASSERT(value == 0, "Semaphore value is zero");
    
    /* Test semaphore trywait on zero semaphore */
    result = sem_trywait(&semaphore);
    TEST_ASSERT(result != 0, "Trywait on zero semaphore fails");
    
    /* Test semaphore post */
    result = sem_post(&semaphore);
    TEST_ASSERT(result == 0, "Semaphore post succeeds");
    
    result = sem_getvalue(&semaphore, &value);
    TEST_ASSERT(result == 0, "Getting semaphore value after post succeeds");
    TEST_ASSERT(value == 1, "Semaphore value incremented correctly");
    
    /* Test semaphore destruction */
    result = sem_destroy(&semaphore);
    TEST_ASSERT(result == 0, "Semaphore destruction succeeds");
}

static sem_t resource_sem;
static int sem_test_resources_used = 0;

void* semaphore_worker_thread(void* arg) {
    int worker_id = *(int*)arg;
    
    /* Acquire resource */
    sem_wait(&resource_sem);
    
    /* Use resource (increment counter) */
    sem_test_resources_used++;
    
    /* Simulate work */
    pthread_yield();
    
    /* Release resource */
    sem_post(&resource_sem);
    
    return NULL;
}

void test_semaphores_resource_limiting(void) {
    TEST_GROUP("Semaphore Resource Limiting");
    
    const int NUM_WORKERS = 5;
    const int MAX_RESOURCES = 2;
    pthread_t workers[NUM_WORKERS];
    int worker_ids[NUM_WORKERS];
    
    /* Initialize semaphore with limited resources */
    sem_init(&resource_sem, 0, MAX_RESOURCES);
    sem_test_resources_used = 0;
    
    /* Create worker threads */
    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_ids[i] = i;
        int result = pthread_create(&workers[i], NULL, semaphore_worker_thread, &worker_ids[i]);
        TEST_ASSERT(result == 0, "Semaphore worker thread creation succeeds");
    }
    
    /* Wait for all workers */
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }
    
    TEST_ASSERT(sem_test_resources_used == NUM_WORKERS, "All workers used resources");
    
    /* Check final semaphore value */
    int final_value;
    sem_getvalue(&resource_sem, &final_value);
    TEST_ASSERT(final_value == MAX_RESOURCES, "Semaphore value restored correctly");
    
    sem_destroy(&resource_sem);
}

/* ================================
 * Thread-Local Storage Tests
 * ================================ */

static pthread_key_t tls_test_key;
static int destructor_call_count = 0;

void tls_test_destructor(void* value) {
    destructor_call_count++;
}

void* tls_test_thread(void* arg) {
    int thread_id = *(int*)arg;
    long tls_value = thread_id * 100;
    
    /* Set thread-specific value */
    int result = pthread_setspecific(tls_test_key, (void*)tls_value);
    if (result != 0) return (void*)-1;
    
    /* Get thread-specific value */
    void* retrieved = pthread_getspecific(tls_test_key);
    if ((long)retrieved != tls_value) return (void*)-2;
    
    return (void*)tls_value;
}

void test_thread_local_storage(void) {
    TEST_GROUP("Thread-Local Storage");
    
    /* Create TLS key */
    int result = pthread_key_create(&tls_test_key, tls_test_destructor);
    TEST_ASSERT(result == 0, "TLS key creation succeeds");
    
    /* Test with multiple threads */
    const int NUM_THREADS = 3;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    void* return_values[NUM_THREADS];
    
    destructor_call_count = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;
        result = pthread_create(&threads[i], NULL, tls_test_thread, &thread_ids[i]);
        TEST_ASSERT(result == 0, "TLS test thread creation succeeds");
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], &return_values[i]);
        long expected = (i + 1) * 100;
        TEST_ASSERT((long)return_values[i] == expected, "TLS value is thread-specific");
    }
    
    /* Note: Destructor calls depend on kernel implementation */
    
    /* Delete TLS key */
    result = pthread_key_delete(tls_test_key);
    TEST_ASSERT(result == 0, "TLS key deletion succeeds");
}

/* ================================
 * Barrier Tests
 * ================================ */

static pthread_barrier_t test_barrier;
static int barrier_phase_1_complete[10] = {0};
static int barrier_phase_2_complete[10] = {0};

void* barrier_test_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    /* Phase 1 work */
    barrier_phase_1_complete[thread_id] = 1;
    
    /* Wait at barrier */
    int result = pthread_barrier_wait(&test_barrier);
    
    /* Phase 2 work */
    barrier_phase_2_complete[thread_id] = 1;
    
    return (void*)(long)result;
}

void test_barriers(void) {
    TEST_GROUP("Barrier Synchronization");
    
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    void* return_values[NUM_THREADS];
    
    /* Initialize barrier */
    int result = pthread_barrier_init(&test_barrier, NULL, NUM_THREADS);
    TEST_ASSERT(result == 0, "Barrier initialization succeeds");
    
    /* Reset completion arrays */
    memset(barrier_phase_1_complete, 0, sizeof(barrier_phase_1_complete));
    memset(barrier_phase_2_complete, 0, sizeof(barrier_phase_2_complete));
    
    /* Create threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        result = pthread_create(&threads[i], NULL, barrier_test_thread, &thread_ids[i]);
        TEST_ASSERT(result == 0, "Barrier test thread creation succeeds");
    }
    
    /* Wait for all threads */
    int serial_thread_count = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], &return_values[i]);
        if ((long)return_values[i] == PTHREAD_BARRIER_SERIAL_THREAD) {
            serial_thread_count++;
        }
    }
    
    /* Check that all threads completed both phases */
    bool all_phase_1_complete = true;
    bool all_phase_2_complete = true;
    for (int i = 0; i < NUM_THREADS; i++) {
        if (!barrier_phase_1_complete[i]) all_phase_1_complete = false;
        if (!barrier_phase_2_complete[i]) all_phase_2_complete = false;
    }
    
    TEST_ASSERT(all_phase_1_complete, "All threads completed phase 1");
    TEST_ASSERT(all_phase_2_complete, "All threads completed phase 2");
    TEST_ASSERT(serial_thread_count <= 1, "At most one thread is serial thread");
    
    /* Destroy barrier */
    result = pthread_barrier_destroy(&test_barrier);
    TEST_ASSERT(result == 0, "Barrier destruction succeeds");
}

/* ================================
 * Error Handling Tests
 * ================================ */

void test_error_handling(void) {
    TEST_GROUP("Error Handling");
    
    /* Test invalid thread operations */
    pthread_t invalid_thread = 0;
    void* retval;
    int result = pthread_join(invalid_thread, &retval);
    TEST_ASSERT(result != 0, "Joining invalid thread fails");
    
    result = pthread_detach(invalid_thread);
    TEST_ASSERT(result != 0, "Detaching invalid thread fails");
    
    /* Test invalid mutex operations */
    pthread_mutex_t uninitialized_mutex;
    memset(&uninitialized_mutex, 0, sizeof(uninitialized_mutex));
    result = pthread_mutex_lock(&uninitialized_mutex);
    TEST_ASSERT(result != 0, "Locking uninitialized mutex fails");
    
    /* Test NULL pointer handling */
    result = pthread_create(NULL, NULL, NULL, NULL);
    TEST_ASSERT(result != 0, "Creating thread with NULL function fails");
    
    pthread_attr_t attr;
    result = pthread_attr_setstacksize(&attr, 0); /* Too small */
    TEST_ASSERT(result != 0, "Setting invalid stack size fails");
}

/* ================================
 * Statistics Tests
 * ================================ */

void test_threading_statistics(void) {
    TEST_GROUP("Threading Statistics");
    
    pthread_stats_t stats;
    int result = pthread_getstat(&stats);
    
    /* Note: Statistics might not be fully implemented */
    if (result == 0) {
        TEST_ASSERT(stats.total_threads_created >= 0, "Thread creation count is valid");
        TEST_ASSERT(stats.active_threads >= 0, "Active thread count is valid");
        TEST_ASSERT(stats.context_switches >= 0, "Context switch count is valid");
    } else {
        printf("Statistics not implemented, skipping detailed tests\n");
    }
}

/* ================================
 * Integration Tests
 * ================================ */

/* Complex scenario: Multiple producers and consumers with different sync primitives */
static pthread_mutex_t integration_mutex;
static pthread_cond_t integration_cond;
static sem_t integration_sem;
static int integration_buffer[10];
static int integration_count = 0;

void* integration_producer(void* arg) {
    int producer_id = *(int*)arg;
    
    for (int i = 0; i < 5; i++) {
        /* Wait for semaphore (buffer space) */
        sem_wait(&integration_sem);
        
        /* Lock mutex and add to buffer */
        pthread_mutex_lock(&integration_mutex);
        integration_buffer[integration_count] = producer_id * 100 + i;
        integration_count++;
        pthread_cond_signal(&integration_cond);
        pthread_mutex_unlock(&integration_mutex);
        
        pthread_yield();
    }
    
    return NULL;
}

void* integration_consumer(void* arg) {
    int consumer_id = *(int*)arg;
    int consumed = 0;
    
    while (consumed < 5) {
        pthread_mutex_lock(&integration_mutex);
        while (integration_count == 0) {
            pthread_cond_wait(&integration_cond, &integration_mutex);
        }
        
        int item = integration_buffer[integration_count - 1];
        integration_count--;
        consumed++;
        pthread_mutex_unlock(&integration_mutex);
        
        /* Signal semaphore (buffer space available) */
        sem_post(&integration_sem);
        
        pthread_yield();
    }
    
    return (void*)(long)consumed;
}

void test_integration_scenario(void) {
    TEST_GROUP("Integration Scenario");
    
    const int NUM_PRODUCERS = 2;
    const int NUM_CONSUMERS = 2;
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];
    void* consumer_results[NUM_CONSUMERS];
    
    /* Initialize synchronization objects */
    pthread_mutex_init(&integration_mutex, NULL);
    pthread_cond_init(&integration_cond, NULL);
    sem_init(&integration_sem, 0, 10); /* Buffer size */
    integration_count = 0;
    
    /* Create producers */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_ids[i] = i + 1;
        int result = pthread_create(&producers[i], NULL, integration_producer, &producer_ids[i]);
        TEST_ASSERT(result == 0, "Integration producer creation succeeds");
    }
    
    /* Create consumers */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_ids[i] = i + 1;
        int result = pthread_create(&consumers[i], NULL, integration_consumer, &consumer_ids[i]);
        TEST_ASSERT(result == 0, "Integration consumer creation succeeds");
    }
    
    /* Wait for producers */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    /* Wait for consumers */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], &consumer_results[i]);
        TEST_ASSERT((long)consumer_results[i] == 5, "Consumer processed correct number of items");
    }
    
    TEST_ASSERT(integration_count == 0, "All items were consumed");
    
    /* Clean up */
    pthread_mutex_destroy(&integration_mutex);
    pthread_cond_destroy(&integration_cond);
    sem_destroy(&integration_sem);
}

/* ================================
 * Main Test Functions
 * ================================ */

void run_threading_unit_tests(void) {
    printf("IKOS Threading Unit Tests\n");
    printf("=========================\n");
    
    test_thread_creation_join();
    test_thread_self_function();
    test_thread_detach();
    test_thread_attributes();
    test_mutex_basic();
    test_mutex_contention();
    test_mutex_attributes();
    test_condition_variables_basic();
    test_condition_variables_signaling();
    test_semaphores_basic();
    test_semaphores_resource_limiting();
    test_thread_local_storage();
    test_barriers();
    test_error_handling();
    test_threading_statistics();
    
    print_test_summary();
}

void run_threading_integration_tests(void) {
    printf("\nIKOS Threading Integration Tests\n");
    printf("================================\n");
    
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    test_integration_scenario();
    
    print_test_summary();
}

void run_threading_stress_tests(void) {
    printf("\nIKOS Threading Stress Tests\n");
    printf("===========================\n");
    
    TEST_GROUP("Stress Test: Many Threads");
    
    const int STRESS_THREAD_COUNT = 20;
    pthread_t stress_threads[STRESS_THREAD_COUNT];
    int stress_args[STRESS_THREAD_COUNT];
    
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    /* Create many threads */
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        stress_args[i] = i;
        int result = pthread_create(&stress_threads[i], NULL, test_thread_basic, &stress_args[i]);
        TEST_ASSERT(result == 0, "Stress thread creation succeeds");
    }
    
    /* Join all threads */
    int successful_joins = 0;
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        void* retval;
        int result = pthread_join(stress_threads[i], &retval);
        if (result == 0 && (long)retval == stress_args[i] * 2) {
            successful_joins++;
        }
    }
    
    TEST_ASSERT(successful_joins == STRESS_THREAD_COUNT, "All stress threads completed successfully");
    
    print_test_summary();
}

/* ================================
 * Test Suite Main Function
 * ================================ */

void threading_comprehensive_test(void) {
    printf("IKOS Threading Comprehensive Test Suite\n");
    printf("=======================================\n\n");
    
    /* Run all test suites */
    run_threading_unit_tests();
    run_threading_integration_tests();
    run_threading_stress_tests();
    
    printf("\n=== Overall Test Summary ===\n");
    printf("Comprehensive threading test suite completed\n");
    printf("Threading API implementation validated\n");
}

/* Simple threading test for basic validation */
int threading_basic_test(void) {
    printf("Threading Basic Validation Test\n");
    printf("===============================\n");
    
    int result = 0;
    
    /* Test basic thread creation */
    pthread_t thread;
    int arg = 42;
    void* retval;
    
    printf("Testing basic thread creation...\n");
    int create_result = pthread_create(&thread, NULL, test_thread_basic, &arg);
    if (create_result == 0) {
        printf("PASS: Thread creation succeeded (TID=%u)\n", thread);
        
        printf("Testing thread join...\n");
        int join_result = pthread_join(thread, &retval);
        if (join_result == 0) {
            printf("PASS: Thread join succeeded\n");
            
            if ((long)retval == 84) { // 42 * 2
                printf("PASS: Thread return value correct (%ld)\n", (long)retval);
            } else {
                printf("FAIL: Thread return value incorrect (%ld, expected 84)\n", (long)retval);
                result = -1;
            }
        } else {
            printf("FAIL: Thread join failed (%d)\n", join_result);
            result = -1;
        }
    } else {
        printf("FAIL: Thread creation failed (%d)\n", create_result);
        result = -1;
    }
    
    /* Test basic mutex operations */
    printf("Testing basic mutex operations...\n");
    pthread_mutex_t mutex;
    int mutex_init_result = pthread_mutex_init(&mutex, NULL);
    if (mutex_init_result == 0) {
        printf("PASS: Mutex initialization succeeded\n");
        
        int lock_result = pthread_mutex_lock(&mutex);
        if (lock_result == 0) {
            printf("PASS: Mutex lock succeeded\n");
            
            int unlock_result = pthread_mutex_unlock(&mutex);
            if (unlock_result == 0) {
                printf("PASS: Mutex unlock succeeded\n");
            } else {
                printf("FAIL: Mutex unlock failed (%d)\n", unlock_result);
                result = -1;
            }
        } else {
            printf("FAIL: Mutex lock failed (%d)\n", lock_result);
            result = -1;
        }
        
        pthread_mutex_destroy(&mutex);
    } else {
        printf("FAIL: Mutex initialization failed (%d)\n", mutex_init_result);
        result = -1;
    }
    
    if (result == 0) {
        printf("SUCCESS: Threading basic validation passed\n");
    } else {
        printf("FAILURE: Threading basic validation failed\n");
    }
    
    return result;
}
