#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <stddef.h>
#include <time.h>

#define CACHE_URL_MAXLEN   512
#define CACHE_MAX_ITEMS    32
#define CACHE_INIT_CAP     4096

#define CACHE_SUCCESS  0
#define CACHE_FAILURE -1

typedef struct cache_item {
    int    active;
    char   url[CACHE_URL_MAXLEN];

    char  *buffer;
    size_t length;
    size_t capacity;

    int    done;
    int    error;

    int    users;
    time_t last_access;

    pthread_mutex_t mtx;
    pthread_cond_t  cv;
} cache_item_t;

void cache_system_init(void);

cache_item_t *cache_acquire(const char *url, int *writer);

int cache_write(cache_item_t *item, const void *data, size_t size);

void cache_finish_ok(cache_item_t *item);
void cache_finish_err(cache_item_t *item);

void cache_release(cache_item_t *item);

#endif 
