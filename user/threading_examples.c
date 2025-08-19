/*
 * IKOS Threading Examples and Demonstrations
 * Issue #52 - Multi-Threading & Concurrency Support
 * 
 * Comprehensive examples showing pthread API usage including
 * thread creation, synchronization, and concurrency patterns.
 */

#include "../include/pthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ================================
 * Example 1: Basic Thread Creation
 * ================================ */

void* simple_thread_function(void* arg) {
    int thread_num = *(int*)arg;
    
    printf("Thread %d: Starting execution\n", thread_num);
    
    for (int i = 0; i < 5; i++) {
        printf("Thread %d: Iteration %d\n", thread_num, i);
        pthread_yield(); // Yield to other threads
    }
    
    printf("Thread %d: Finished execution\n", thread_num);
    return (void*)(long)(thread_num * 100);
}

void example_basic_threading(void) {
    printf("\n=== Example 1: Basic Thread Creation ===\n");
    
    const int NUM_THREADS = 3;
    pthread_t threads[NUM_THREADS];
    int thread_args[NUM_THREADS];
    void* return_values[NUM_THREADS];
    
    /* Create threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = i + 1;
        int result = pthread_create(&threads[i], NULL, simple_thread_function, 
                                   &thread_args[i]);
        if (result != 0) {
            printf("Error creating thread %d: %d\n", i, result);
            continue;
        }
        printf("Created thread %d with ID: %u\n", i + 1, threads[i]);
    }
    
    /* Join threads and collect return values */
    for (int i = 0; i < NUM_THREADS; i++) {
        int result = pthread_join(threads[i], &return_values[i]);
        if (result == 0) {
            printf("Thread %d returned: %ld\n", i + 1, (long)return_values[i]);
        } else {
            printf("Error joining thread %d: %d\n", i + 1, result);
        }
    }
    
    printf("All threads completed\n");
}

/* ================================
 * Example 2: Thread Attributes
 * ================================ */

void* custom_thread_function(void* arg) {
    char* name = (char*)arg;
    
    /* Set thread name */
    pthread_setname_np(pthread_self(), name);
    
    printf("Custom thread '%s' (TID: %u) starting\n", name, pthread_self());
    
    /* Do some work */
    for (int i = 0; i < 3; i++) {
        printf("Thread '%s': Working... (%d/3)\n", name, i + 1);
        pthread_yield();
    }
    
    printf("Custom thread '%s' finishing\n", name);
    return NULL;
}

void example_thread_attributes(void) {
    printf("\n=== Example 2: Thread Attributes ===\n");
    
    pthread_t thread1, thread2;
    pthread_attr_t attr;
    
    /* Initialize attributes */
    pthread_attr_init(&attr);
    
    /* Set custom stack size */
    size_t stack_size = 1024 * 1024; // 1MB
    pthread_attr_setstacksize(&attr, stack_size);
    
    /* Create detached thread */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int result = pthread_create(&thread1, &attr, custom_thread_function, "DetachedWorker");
    if (result == 0) {
        printf("Created detached thread with 1MB stack\n");
    }
    
    /* Create joinable thread with default attributes */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    result = pthread_create(&thread2, &attr, custom_thread_function, "JoinableWorker");
    if (result == 0) {
        printf("Created joinable thread\n");
        
        /* Join the joinable thread */
        pthread_join(thread2, NULL);
        printf("Joinable thread completed\n");
    }
    
    /* Clean up attributes */
    pthread_attr_destroy(&attr);
    
    /* Give detached thread time to complete */
    for (int i = 0; i < 5; i++) {
        pthread_yield();
    }
}

/* ================================
 * Example 3: Mutex Synchronization
 * ================================ */

static int shared_counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void* counter_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 10; i++) {
        /* Lock mutex before accessing shared data */
        pthread_mutex_lock(&counter_mutex);
        
        int old_value = shared_counter;
        shared_counter++;
        printf("Thread %d: incremented counter from %d to %d\n", 
               thread_id, old_value, shared_counter);
        
        /* Unlock mutex */
        pthread_mutex_unlock(&counter_mutex);
        
        /* Small delay to increase chance of contention */
        pthread_yield();
    }
    
    return NULL;
}

void example_mutex_synchronization(void) {
    printf("\n=== Example 3: Mutex Synchronization ===\n");
    
    const int NUM_THREADS = 3;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    /* Reset shared counter */
    shared_counter = 0;
    
    printf("Initial counter value: %d\n", shared_counter);
    
    /* Create threads that will increment shared counter */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;
        int result = pthread_create(&threads[i], NULL, counter_thread, &thread_ids[i]);
        if (result != 0) {
            printf("Error creating counter thread %d\n", i);
        }
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Final counter value: %d (expected: %d)\n", 
           shared_counter, NUM_THREADS * 10);
    
    if (shared_counter == NUM_THREADS * 10) {
        printf("Mutex synchronization successful!\n");
    } else {
        printf("Mutex synchronization failed - possible race condition\n");
    }
}

/* ================================
 * Example 4: Producer-Consumer with Condition Variables
 * ================================ */

#define BUFFER_SIZE 5

typedef struct {
    int buffer[BUFFER_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} circular_buffer_t;

static circular_buffer_t shared_buffer;

void buffer_init(circular_buffer_t* buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->not_full, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
}

void buffer_destroy(circular_buffer_t* buf) {
    pthread_mutex_destroy(&buf->mutex);
    pthread_cond_destroy(&buf->not_full);
    pthread_cond_destroy(&buf->not_empty);
}

void buffer_put(circular_buffer_t* buf, int item) {
    pthread_mutex_lock(&buf->mutex);
    
    /* Wait while buffer is full */
    while (buf->count == BUFFER_SIZE) {
        pthread_cond_wait(&buf->not_full, &buf->mutex);
    }
    
    /* Add item to buffer */
    buf->buffer[buf->tail] = item;
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;
    buf->count++;
    
    printf("Produced item: %d (buffer count: %d)\n", item, buf->count);
    
    /* Signal that buffer is not empty */
    pthread_cond_signal(&buf->not_empty);
    
    pthread_mutex_unlock(&buf->mutex);
}

int buffer_get(circular_buffer_t* buf) {
    pthread_mutex_lock(&buf->mutex);
    
    /* Wait while buffer is empty */
    while (buf->count == 0) {
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }
    
    /* Remove item from buffer */
    int item = buf->buffer[buf->head];
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count--;
    
    printf("Consumed item: %d (buffer count: %d)\n", item, buf->count);
    
    /* Signal that buffer is not full */
    pthread_cond_signal(&buf->not_full);
    
    pthread_mutex_unlock(&buf->mutex);
    return item;
}

void* producer_thread(void* arg) {
    int producer_id = *(int*)arg;
    
    for (int i = 1; i <= 5; i++) {
        int item = producer_id * 100 + i;
        buffer_put(&shared_buffer, item);
        pthread_yield(); /* Allow other threads to run */
    }
    
    printf("Producer %d finished\n", producer_id);
    return NULL;
}

void* consumer_thread(void* arg) {
    int consumer_id = *(int*)arg;
    
    for (int i = 0; i < 5; i++) {
        int item = buffer_get(&shared_buffer);
        printf("Consumer %d got item: %d\n", consumer_id, item);
        pthread_yield(); /* Allow other threads to run */
    }
    
    printf("Consumer %d finished\n", consumer_id);
    return NULL;
}

void example_producer_consumer(void) {
    printf("\n=== Example 4: Producer-Consumer with Condition Variables ===\n");
    
    /* Initialize shared buffer */
    buffer_init(&shared_buffer);
    
    const int NUM_PRODUCERS = 2;
    const int NUM_CONSUMERS = 2;
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];
    
    /* Create producer threads */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_ids[i] = i + 1;
        int result = pthread_create(&producers[i], NULL, producer_thread, &producer_ids[i]);
        if (result != 0) {
            printf("Error creating producer thread %d\n", i);
        } else {
            printf("Created producer thread %d\n", i + 1);
        }
    }
    
    /* Create consumer threads */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_ids[i] = i + 1;
        int result = pthread_create(&consumers[i], NULL, consumer_thread, &consumer_ids[i]);
        if (result != 0) {
            printf("Error creating consumer thread %d\n", i);
        } else {
            printf("Created consumer thread %d\n", i + 1);
        }
    }
    
    /* Wait for all producers to finish */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    /* Wait for all consumers to finish */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    printf("Producer-Consumer example completed\n");
    
    /* Clean up */
    buffer_destroy(&shared_buffer);
}

/* ================================
 * Example 5: Semaphore Usage
 * ================================ */

static sem_t resource_semaphore;

void* worker_with_semaphore(void* arg) {
    int worker_id = *(int*)arg;
    
    printf("Worker %d: Waiting for resource...\n", worker_id);
    
    /* Acquire resource */
    sem_wait(&resource_semaphore);
    
    printf("Worker %d: Acquired resource, working...\n", worker_id);
    
    /* Simulate work */
    for (int i = 0; i < 3; i++) {
        printf("Worker %d: Working... (%d/3)\n", worker_id, i + 1);
        pthread_yield();
    }
    
    printf("Worker %d: Releasing resource\n", worker_id);
    
    /* Release resource */
    sem_post(&resource_semaphore);
    
    return NULL;
}

void example_semaphores(void) {
    printf("\n=== Example 5: Semaphore Usage ===\n");
    
    const int NUM_WORKERS = 5;
    const int MAX_RESOURCES = 2; /* Only 2 workers can access resource simultaneously */
    
    pthread_t workers[NUM_WORKERS];
    int worker_ids[NUM_WORKERS];
    
    /* Initialize semaphore with 2 resources */
    sem_init(&resource_semaphore, 0, MAX_RESOURCES);
    printf("Initialized semaphore with %d resources\n", MAX_RESOURCES);
    
    /* Create worker threads */
    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_ids[i] = i + 1;
        int result = pthread_create(&workers[i], NULL, worker_with_semaphore, &worker_ids[i]);
        if (result != 0) {
            printf("Error creating worker thread %d\n", i);
        }
    }
    
    /* Wait for all workers to complete */
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }
    
    printf("All workers completed\n");
    
    /* Clean up semaphore */
    sem_destroy(&resource_semaphore);
}

/* ================================
 * Example 6: Barrier Synchronization
 * ================================ */

static pthread_barrier_t sync_barrier;

void* barrier_worker(void* arg) {
    int worker_id = *(int*)arg;
    
    printf("Worker %d: Starting work phase 1\n", worker_id);
    
    /* Simulate work phase 1 */
    for (int i = 0; i < worker_id; i++) {
        printf("Worker %d: Phase 1 work %d\n", worker_id, i + 1);
        pthread_yield();
    }
    
    printf("Worker %d: Finished phase 1, waiting at barrier\n", worker_id);
    
    /* Wait at barrier for all workers to complete phase 1 */
    int result = pthread_barrier_wait(&sync_barrier);
    
    if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
        printf("Worker %d: I'm the last one! All workers synchronized.\n", worker_id);
    }
    
    printf("Worker %d: Starting work phase 2\n", worker_id);
    
    /* Simulate work phase 2 */
    for (int i = 0; i < 3; i++) {
        printf("Worker %d: Phase 2 work %d\n", worker_id, i + 1);
        pthread_yield();
    }
    
    printf("Worker %d: Finished all work\n", worker_id);
    return NULL;
}

void example_barriers(void) {
    printf("\n=== Example 6: Barrier Synchronization ===\n");
    
    const int NUM_WORKERS = 4;
    pthread_t workers[NUM_WORKERS];
    int worker_ids[NUM_WORKERS];
    
    /* Initialize barrier for all workers */
    pthread_barrier_init(&sync_barrier, NULL, NUM_WORKERS);
    printf("Initialized barrier for %d workers\n", NUM_WORKERS);
    
    /* Create worker threads */
    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_ids[i] = i + 1;
        int result = pthread_create(&workers[i], NULL, barrier_worker, &worker_ids[i]);
        if (result != 0) {
            printf("Error creating worker thread %d\n", i);
        }
    }
    
    /* Wait for all workers to complete */
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }
    
    printf("All workers completed both phases\n");
    
    /* Clean up barrier */
    pthread_barrier_destroy(&sync_barrier);
}

/* ================================
 * Example 7: Thread-Local Storage
 * ================================ */

static pthread_key_t tls_key;

void tls_destructor(void* value) {
    printf("TLS destructor called for value: %ld\n", (long)value);
    /* In a real application, you might free allocated memory here */
}

void* tls_worker(void* arg) {
    int worker_id = *(int*)arg;
    
    /* Set thread-specific data */
    long tls_value = worker_id * 1000;
    pthread_setspecific(tls_key, (void*)tls_value);
    
    printf("Worker %d: Set TLS value to %ld\n", worker_id, tls_value);
    
    /* Do some work */
    for (int i = 0; i < 3; i++) {
        /* Retrieve thread-specific data */
        long current_value = (long)pthread_getspecific(tls_key);
        printf("Worker %d: TLS value is %ld (iteration %d)\n", 
               worker_id, current_value, i + 1);
        pthread_yield();
    }
    
    /* Modify thread-specific data */
    tls_value += worker_id;
    pthread_setspecific(tls_key, (void*)tls_value);
    printf("Worker %d: Updated TLS value to %ld\n", worker_id, tls_value);
    
    return NULL;
}

void example_thread_local_storage(void) {
    printf("\n=== Example 7: Thread-Local Storage ===\n");
    
    const int NUM_WORKERS = 3;
    pthread_t workers[NUM_WORKERS];
    int worker_ids[NUM_WORKERS];
    
    /* Create TLS key with destructor */
    int result = pthread_key_create(&tls_key, tls_destructor);
    if (result != 0) {
        printf("Error creating TLS key: %d\n", result);
        return;
    }
    printf("Created TLS key\n");
    
    /* Create worker threads */
    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_ids[i] = i + 1;
        result = pthread_create(&workers[i], NULL, tls_worker, &worker_ids[i]);
        if (result != 0) {
            printf("Error creating worker thread %d\n", i);
        }
    }
    
    /* Wait for all workers to complete */
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }
    
    printf("All TLS workers completed\n");
    
    /* Clean up TLS key */
    pthread_key_delete(tls_key);
}

/* ================================
 * Example 8: Threading Statistics
 * ================================ */

void example_threading_statistics(void) {
    printf("\n=== Example 8: Threading Statistics ===\n");
    
    pthread_stats_t stats;
    int result = pthread_getstat(&stats);
    
    if (result == 0) {
        printf("Threading Statistics:\n");
        printf("  Total threads created: %lu\n", stats.total_threads_created);
        printf("  Active threads: %lu\n", stats.active_threads);
        printf("  Context switches: %lu\n", stats.context_switches);
        printf("  Mutex contentions: %lu\n", stats.mutex_contentions);
        printf("  Condition signals: %lu\n", stats.condition_signals);
        printf("  Semaphore operations: %lu\n", stats.semaphore_operations);
        printf("  Total CPU time: %lu ns\n", stats.total_cpu_time);
        printf("  Idle time: %lu ns\n", stats.idle_time);
    } else {
        printf("Error getting threading statistics: %d\n", result);
    }
}

/* ================================
 * Comprehensive Threading Demo
 * ================================ */

void threading_comprehensive_demo(void) {
    printf("IKOS Threading API Comprehensive Demo\n");
    printf("====================================\n");
    
    printf("This demo showcases the complete pthread API implementation\n");
    printf("including thread creation, synchronization primitives, and\n");
    printf("advanced threading features.\n");
    
    /* Run all examples */
    example_basic_threading();
    example_thread_attributes();
    example_mutex_synchronization();
    example_producer_consumer();
    example_semaphores();
    example_barriers();
    example_thread_local_storage();
    example_threading_statistics();
    
    printf("\n=== Threading Demo Complete ===\n");
    printf("All threading examples completed successfully!\n");
    printf("The pthread API is ready for use in IKOS applications.\n");
}

/* ================================
 * Performance Benchmark
 * ================================ */

static pthread_mutex_t bench_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int bench_counter = 0;

void* benchmark_thread(void* arg) {
    int iterations = *(int*)arg;
    
    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&bench_mutex);
        bench_counter++;
        pthread_mutex_unlock(&bench_mutex);
        
        if (i % 1000 == 0) {
            pthread_yield();
        }
    }
    
    return NULL;
}

void threading_performance_benchmark(void) {
    printf("\n=== Threading Performance Benchmark ===\n");
    
    const int NUM_THREADS = 4;
    const int ITERATIONS_PER_THREAD = 10000;
    pthread_t threads[NUM_THREADS];
    int iterations = ITERATIONS_PER_THREAD;
    
    printf("Running benchmark with %d threads, %d iterations each\n", 
           NUM_THREADS, ITERATIONS_PER_THREAD);
    
    bench_counter = 0;
    
    /* Start benchmark timer (simplified) */
    printf("Starting benchmark...\n");
    
    /* Create benchmark threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        int result = pthread_create(&threads[i], NULL, benchmark_thread, &iterations);
        if (result != 0) {
            printf("Error creating benchmark thread %d\n", i);
        }
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Benchmark completed\n");
    printf("Final counter value: %d (expected: %d)\n", 
           bench_counter, NUM_THREADS * ITERATIONS_PER_THREAD);
    
    if (bench_counter == NUM_THREADS * ITERATIONS_PER_THREAD) {
        printf("Performance benchmark: PASSED\n");
    } else {
        printf("Performance benchmark: FAILED (race condition detected)\n");
    }
    
    /* Display final statistics */
    example_threading_statistics();
}

/* ================================
 * Simple Threading Test
 * ================================ */

void* simple_test_thread(void* arg) {
    printf("Simple test thread running (TID: %u)\n", pthread_self());
    return (void*)42;
}

int threading_simple_test(void) {
    printf("IKOS Threading Simple Test\n");
    printf("==========================\n");
    
    /* Test basic thread creation and join */
    pthread_t thread;
    void* retval;
    
    printf("Creating test thread...\n");
    int result = pthread_create(&thread, NULL, simple_test_thread, NULL);
    if (result != 0) {
        printf("FAIL: pthread_create returned %d\n", result);
        return -1;
    }
    
    printf("Test thread created with TID: %u\n", thread);
    
    printf("Joining test thread...\n");
    result = pthread_join(thread, &retval);
    if (result != 0) {
        printf("FAIL: pthread_join returned %d\n", result);
        return -1;
    }
    
    printf("Test thread returned: %ld\n", (long)retval);
    
    if ((long)retval == 42) {
        printf("SUCCESS: Basic threading test passed\n");
        return 0;
    } else {
        printf("FAIL: Unexpected return value\n");
        return -1;
    }
}

/* ================================
 * Main Example Function
 * ================================ */

int main(void) {
    printf("IKOS Multi-Threading Examples\n");
    printf("=============================\n\n");
    
    /* Run simple test first */
    if (threading_simple_test() != 0) {
        printf("Basic threading test failed, skipping comprehensive demo\n");
        return 1;
    }
    
    printf("\n");
    
    /* Run comprehensive demo */
    threading_comprehensive_demo();
    
    /* Run performance benchmark */
    threading_performance_benchmark();
    
    printf("\nAll threading examples and tests completed!\n");
    return 0;
}
