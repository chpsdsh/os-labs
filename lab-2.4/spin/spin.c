#include "spin.h"

void spin_init(spinlock_t *lock) {
    lock->flag = 0;
}

void spin_lock(spinlock_t *lock) {
    while (1) {
        if (__sync_bool_compare_and_swap(&lock->flag, 0, 1)) {
            return;
        }
     
        while (lock->flag) {}
    }
}

void spin_unlock(spinlock_t *lock) {
    __sync_bool_compare_and_swap(&lock->flag, 1, 0);
}

int spin_trylock(spinlock_t *lock) {
    return __sync_bool_compare_and_swap(&lock->flag, 0, 1);
}
