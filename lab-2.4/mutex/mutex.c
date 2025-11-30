#define _GNU_SOURCE
#include "mutex.h"
#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>

static int futex_wait(volatile int *addr, int expected) {
    return syscall(SYS_futex, (int*)addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int futex_wake(volatile int *addr, int n) {
    return syscall(SYS_futex, (int*)addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

void fmutex_init(futex_mutex_t *m) {
    m->state = MUTEX_UNLOCKED;
}

void fmutex_lock(futex_mutex_t *m) {
    if (__sync_bool_compare_and_swap(&m->state, MUTEX_UNLOCKED, MUTEX_LOCKED)) {
        return;
    }

    while (1) {
        int old = m->state;

        if (old == MUTEX_UNLOCKED) {
            if (__sync_bool_compare_and_swap(&m->state, MUTEX_UNLOCKED, MUTEX_LOCKED))
                return;
            continue;
        }

        if (old == MUTEX_LOCKED) {
            if (!__sync_bool_compare_and_swap(&m->state, MUTEX_LOCKED, MUTEX_CONTENDED))
                continue;
            old = MUTEX_CONTENDED;
        }

        futex_wait(&m->state, MUTEX_CONTENDED);
    }
}

void fmutex_unlock(futex_mutex_t *m) {
    int old = __sync_fetch_and_sub(&m->state, 1);

    if (old == MUTEX_LOCKED)
        return;

    m->state = MUTEX_UNLOCKED;
    futex_wake(&m->state, 1);
}

int fmutex_trylock(futex_mutex_t *m) {
    return __sync_bool_compare_and_swap(&m->state, MUTEX_UNLOCKED, MUTEX_LOCKED);
}
