#ifndef MYTHREAD_H
#define MYTHREAD_H

#define _GNU_SOURCE
#include <stddef.h>
#include <stdatomic.h>
#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdint.h>

typedef struct mythread *mythread_t;

#define MYTHREAD_OK 0
#define MYTHREAD_ERR -1
#define MYTHREAD_EINVAL -2
#define MYTHREAD_ESTATE -3
#define MYTHREAD_ESYS -4

#define MYTHREAD_STACK_SIZE (1u << 20)

int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg);

int mythread_join(mythread_t thread, void **retval);

int mythread_detach(mythread_t thread);

typedef struct mythread
{
    void *(*start_routine)(void *);
    void *arg;
    void *retval;

    _Atomic int joinable;
    _Atomic int refs;
    _Atomic int futex_word;

    void *stack;
    size_t stack_size;
} mythread;

static inline int futex_wait(_Atomic int *addr, int expect)
{
    return (int)syscall(SYS_futex, addr, FUTEX_WAIT, expect, NULL, NULL, 0);
}

static inline int futex_wake(_Atomic int *addr, int n)
{
    return (int)syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

static inline void release_thread(mythread *t)
{
    if (!t)
        return;
    if (t->stack)
    {
        munmap(t->stack, t->stack_size);
    }
    free(t);
}

#endif
