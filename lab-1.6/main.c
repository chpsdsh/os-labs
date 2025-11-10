#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "my_thread.h"

static void* worker(void* p) {
    uintptr_t id = (uintptr_t)p;
    usleep(1000000);                
    return (void*)(id * 2);
}

void demo_join_all(void) {
    const int N = 8;
    mythread_t th[N];

    for (int i = 0; i < N; ++i) {
        mythread_create(&th[i], worker, (void*)(uintptr_t)i);
    }

    for (int i = 0; i < N; ++i) {
        void* ret = NULL;
        mythread_join(th[i], &ret);
        printf("thread %d -> retval=%lu\n", i, (unsigned long)(uintptr_t)ret);
    }
}


void demo_detach_one(void) {
    mythread_t t;
    mythread_create(&t, worker, (void*)(uintptr_t)23);
    mythread_detach(t);           
    usleep(20000);                
    printf("detached thread finished silently\n");
}

int main(void) {
    demo_join_all();
    demo_detach_one();
    return 0;
}