#include <stdio.h>
#include "uthread.h"

void *worker(void *arg) {
    int id = (int)(long)arg;
    for (int i = 0; i < 7; ++i) {
        printf("thread %d: step %d\n", id, i);
        uthread_yield(); 
    }
    return (void *)(long)(id * 10);
}

int main(void) {
    uthread_t *t1, *t2;
    void *ret1, *ret2;

    uthread_init();

    uthread_create(&t1, worker, (void *)1);
    uthread_create(&t2, worker, (void *)2);

    uthread_join(t1, &ret1);
    uthread_join(t2, &ret2);

    printf("t1 returned %ld\n", (long)ret1);
    printf("t2 returned %ld\n", (long)ret2);

    return 0;
}
