#include "threadpool.h"
#include <stdlib.h>

static void *worker_thread(void *arg) {
    threadpool_t *pool = (threadpool_t *)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        while (pool->count == 0 && !pool->stop) {
            pthread_cond_wait(&pool->cond_not_empty, &pool->mutex);
        }

        if (pool->stop && pool->count == 0) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        task_t task = *(pool->head);
        pool->head++;
        if (pool->head == pool->queue + pool->queue_size) {
            pool->head = pool->queue; 
        }
        pool->count--;

        pthread_cond_signal(&pool->cond_not_full);
        pthread_mutex_unlock(&pool->mutex);

        if (task.func) {
            task.func(task.arg);
        }
    }

    return NULL;
}

threadpool_t *threadpool_create(int num_threads, int queue_size) {
    threadpool_t *pool = malloc(sizeof(*pool));
    if (!pool) return NULL;

    pool->num_threads = num_threads;
    pool->queue_size  = queue_size;
    pool->queue       = malloc(sizeof(task_t) * queue_size);
    pool->count       = 0;
    pool->stop        = 0;

    if (!pool->queue) {
        free(pool);
        return NULL;
    }

    pool->head = pool->queue;
    pool->tail = pool->queue;

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond_not_empty, NULL);
    pthread_cond_init(&pool->cond_not_full, NULL);

    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        free(pool->queue);
        free(pool);
        return NULL;
    }

    for (int i = 0; i < num_threads; ++i) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }

    return pool;
}

int threadpool_submit(threadpool_t *pool, task_fn_t func, void *arg) {
    pthread_mutex_lock(&pool->mutex);

    while (pool->count == pool->queue_size && !pool->stop) {
        pthread_cond_wait(&pool->cond_not_full, &pool->mutex);
    }
    if (pool->stop) {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    
    pool->tail->func = func;
    pool->tail->arg  = arg;
    pool->tail++;
    if (pool->tail == pool->queue + pool->queue_size) {
        pool->tail = pool->queue; 
    }
    pool->count++;

    pthread_cond_signal(&pool->cond_not_empty);
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

void threadpool_destroy(threadpool_t *pool) {
    if (!pool) return;

    pthread_mutex_lock(&pool->mutex);
    pool->stop = 1;
    pthread_cond_broadcast(&pool->cond_not_empty);
    pthread_cond_broadcast(&pool->cond_not_full);
    pthread_mutex_unlock(&pool->mutex);

    for (int i = 0; i < pool->num_threads; ++i) {
        pthread_join(pool->threads[i], NULL);
    }

    free(pool->threads);
    free(pool->queue);

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond_not_empty);
    pthread_cond_destroy(&pool->cond_not_full);
    free(pool);
}
