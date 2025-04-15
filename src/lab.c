#include "lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

// Define the queue struct
struct queue {
    int capacity;
    int size;
    int front;
    int back;
    void **data;
    bool shutdown;

    // Synchronization primitives
    sem_t full;       // Counts number of full slots
    sem_t empty;      // Counts number of empty slots
    pthread_mutex_t lock; // Mutex to protect the critical section
};

typedef struct queue* queue_t;

/**
 * @brief Initialize a new queue
 *
 * @param capacity the maximum capacity of the queue
 * @return A fully initialized queue
 */
queue_t queue_init(int capacity) {
    queue_t q = malloc(sizeof(struct queue));
    if (!q) return NULL;

    q->capacity = capacity;
    q->size = 0;
    q->front = 0;
    q->back = 0;
    q->shutdown = false;

    q->data = malloc(sizeof(void *) * capacity);
    if (!q->data) {
        free(q);
        return NULL;
    }

    sem_init(&q->empty, 0, capacity);   // Start with all slots empty
    sem_init(&q->full, 0, 0);           // Start with no full slots
    pthread_mutex_init(&q->lock, NULL); // Initialize the mutex

    return q;
}

/**
 * @brief Frees all memory and related data; signals all waiting threads.
 *
 * @param q a queue to free
 */
void queue_destroy(queue_t q) {
    if (q) {
        sem_destroy(&q->empty);
        sem_destroy(&q->full);
        pthread_mutex_destroy(&q->lock);
        free(q->data);
        free(q);
    }
}

/**
 * @brief Adds an element to the back of the queue
 *
 * @param q the queue
 * @param data the data to add
 */
void enqueue(queue_t q, void *data) {
    if (!q) return;

    sem_wait(&q->empty); // Wait for an empty slot

    pthread_mutex_lock(&q->lock);
    if (q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        sem_post(&q->empty); // Allow other waiting threads to proceed
        return;
    }

    q->data[q->back] = data;
    q->back = (q->back + 1) % q->capacity;
    q->size++;
    pthread_mutex_unlock(&q->lock);

    sem_post(&q->full); // Signal that a full slot is available
}

/**
 * @brief Removes the first element in the queue.
 *
 * @param q the queue
 * @return the dequeued data
 */
void *dequeue(queue_t q) {
    if (!q) return NULL;

    sem_wait(&q->full); // Wait for a full slot

    pthread_mutex_lock(&q->lock);
    if (q->shutdown && q->size == 0) {
        pthread_mutex_unlock(&q->lock);
        sem_post(&q->full); // Let other waiting threads wake up and exit
        return NULL;
    }

    void *data = q->data[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    pthread_mutex_unlock(&q->lock);

    sem_post(&q->empty); // Signal that an empty slot is available

    return data;
}

// Set the shutdown flag in the queue so all threads can complete and exit properly
void queue_shutdown(queue_t q) {
    if (!q) return;

    pthread_mutex_lock(&q->lock);
    q->shutdown = true;
    pthread_mutex_unlock(&q->lock);

    // Unblock any waiting threads
    for (int i = 0; i < q->capacity; i++) {
        sem_post(&q->full);
        sem_post(&q->empty);
    }
}

// Returns true if the queue is empty
bool is_empty(queue_t q) {
    if (!q) return true;
    return q->size == 0;
}

// Returns true if the queue is full
bool is_full(queue_t q) {
    if (!q) return false;
    return q->size == q->capacity;
}

// Returns true if the queue is shutdown
bool is_shutdown(queue_t q) {
    if (!q) return true;
    return q->shutdown;
}
