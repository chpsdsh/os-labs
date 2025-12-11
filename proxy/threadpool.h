#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

typedef void (*task_fn_t)(void *arg);

typedef struct {
    task_fn_t func;
    void     *arg;
} task_t;

typedef struct threadpool {
    pthread_t *threads;
    int        num_threads;

    task_t    *queue;      
    int        queue_size; 

    task_t    *head;       
    task_t    *tail;       
    int        count;      

    pthread_mutex_t mutex;
    pthread_cond_t  cond_not_empty;
    pthread_cond_t  cond_not_full;

    int        stop;
} threadpool_t;

threadpool_t *threadpool_create(int num_threads, int queue_size);
int           threadpool_submit(threadpool_t *pool, task_fn_t func, void *arg);
void          threadpool_destroy(threadpool_t *pool);

#endif // THREADPOOL_H
