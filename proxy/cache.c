#include "cache.h"
#include <string.h>
#include <stdlib.h>

static cache_item_t g_cache[CACHE_MAX_ITEMS];
static pthread_mutex_t g_cache_mtx = PTHREAD_MUTEX_INITIALIZER;


static void item_clear_payload(cache_item_t *it) {
    free(it->buffer);
    it->buffer = NULL;
    it->length = 0;
    it->capacity = 0;
}

static void item_reset(cache_item_t *it) {
    it->active = 0;
    it->url[0] = '\0';

    item_clear_payload(it);

    it->done = 0;
    it->error = 0;
    it->users = 0;
    it->last_access = 0;
}

static cache_item_t *lookup_item(const char *url) {
    for (int i = 0; i < CACHE_MAX_ITEMS; ++i) {
        if (g_cache[i].active &&
            strcmp(g_cache[i].url, url) == 0)
            return &g_cache[i];
    }
    return NULL;
}

static cache_item_t *find_free_item(void) {
    for (int i = 0; i < CACHE_MAX_ITEMS; ++i) {
        if (!g_cache[i].active)
            return &g_cache[i];
    }
    return NULL;
}

static cache_item_t *select_lru_victim(void) {
    cache_item_t *victim = NULL;

    for (int i = 0; i < CACHE_MAX_ITEMS; ++i) {
        cache_item_t *it = &g_cache[i];
        if (!it->active || it->users != 0)
            continue;

        if (!victim || it->last_access < victim->last_access)
            victim = it;
    }
    return victim;
}


void cache_system_init(void) {
    pthread_mutex_lock(&g_cache_mtx);
    for (int i = 0; i < CACHE_MAX_ITEMS; ++i) {
        cache_item_t *it = &g_cache[i];
        memset(it, 0, sizeof(*it));
        pthread_mutex_init(&it->mtx, NULL);
        pthread_cond_init(&it->cv, NULL);
    }
    pthread_mutex_unlock(&g_cache_mtx);
}

cache_item_t *cache_acquire(const char *url, int *writer) {
    if (!url || !writer)
        return NULL;

    pthread_mutex_lock(&g_cache_mtx);

    cache_item_t *it = lookup_item(url);
    if (it) {
        it->users++;
        it->last_access = time(NULL);
        *writer = 0;
        pthread_mutex_unlock(&g_cache_mtx);
        return it;
    }

    it = find_free_item();
    if (!it)
        it = select_lru_victim();

    if (!it) {
        pthread_mutex_unlock(&g_cache_mtx);
        return NULL;
    }

    if (it->active) {
        pthread_mutex_lock(&it->mtx);
        item_reset(it);
        pthread_mutex_unlock(&it->mtx);
    }

    it->active = 1;
    strncpy(it->url, url, CACHE_URL_MAXLEN - 1);
    it->url[CACHE_URL_MAXLEN - 1] = '\0';

    it->buffer = NULL;
    it->length = 0;
    it->capacity = 0;

    it->done = 0;
    it->error = 0;

    it->users = 1;
    it->last_access = time(NULL);

    *writer = 1;

    pthread_mutex_unlock(&g_cache_mtx);
    return it;
}

int cache_write(cache_item_t *it, const void *data, size_t size) {
    if (!it || !data || size == 0)
        return CACHE_SUCCESS;

    pthread_mutex_lock(&it->mtx);

    if (it->done || it->error) {
        pthread_mutex_unlock(&it->mtx);
        return CACHE_FAILURE;
    }

    if (it->length + size > it->capacity) {
        size_t new_cap = it->capacity ? it->capacity : CACHE_INIT_CAP;
        while (new_cap < it->length + size)
            new_cap *= 2;

        char *p = realloc(it->buffer, new_cap);
        if (!p) {
            it->error = 1;
            pthread_cond_broadcast(&it->cv);
            pthread_mutex_unlock(&it->mtx);
            return CACHE_FAILURE;
        }
        it->buffer = p;
        it->capacity = new_cap;
    }

    memcpy(it->buffer + it->length, data, size);
    it->length += size;

    pthread_cond_broadcast(&it->cv);
    pthread_mutex_unlock(&it->mtx);
    return CACHE_SUCCESS;
}

void cache_finish_ok(cache_item_t *it) {
    if (!it) return;
    pthread_mutex_lock(&it->mtx);
    it->done = 1;
    pthread_cond_broadcast(&it->cv);
    pthread_mutex_unlock(&it->mtx);
}

void cache_finish_err(cache_item_t *it) {
    if (!it) return;
    pthread_mutex_lock(&it->mtx);
    it->error = 1;
    pthread_cond_broadcast(&it->cv);
    pthread_mutex_unlock(&it->mtx);
}

void cache_release(cache_item_t *it) {
    if (!it) return;

    pthread_mutex_lock(&g_cache_mtx);

    if (it->users > 0)
        it->users--;

    it->last_access = time(NULL);

    if (it->active && it->users == 0 && it->error) {
        pthread_mutex_lock(&it->mtx);
        item_reset(it);
        pthread_mutex_unlock(&it->mtx);
    }

    pthread_mutex_unlock(&g_cache_mtx);
}
