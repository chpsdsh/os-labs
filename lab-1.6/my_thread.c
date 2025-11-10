#include "my_thread.h"

static int trampoline_func(void *arg)
{
    mythread *t = (mythread *)arg;

    void *ret = NULL;
    if (t->start_routine)
    {
        ret = t->start_routine(t->arg);
    }

    t->retval = ret;
    atomic_store(&t->futex_word, 1);
    futex_wake(&t->futex_word, 1);

    atomic_fetch_sub(&t->refs, 1);

    syscall(SYS_exit, 0);
    return 0;
}

int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg)
{
    if (!thread || !start_routine)
    {
        return MYTHREAD_EINVAL;
    }

    mythread *t = (mythread *)calloc(1, sizeof(*t));
    if (!t)
        return MYTHREAD_ERR;

    t->start_routine = start_routine;
    t->arg = arg;
    t->retval = NULL;

    atomic_store(&t->joinable, 1);
    atomic_store(&t->refs, 2);
    atomic_store(&t->futex_word, 0);

    t->stack_size = MYTHREAD_STACK_SIZE;

    t->stack = mmap(NULL, t->stack_size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                    -1, 0);

    if (t->stack == MAP_FAILED)
    {
        free(t);
        return MYTHREAD_ESYS;
    }
    void *stack_top = (uint8_t *)t->stack + t->stack_size;

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES |
                CLONE_SIGHAND | CLONE_SYSVSEM | CLONE_THREAD;

    int rc = clone(trampoline_func, stack_top, flags, t);

    if (rc == -1)
    {
        munmap(t->stack, t->stack_size);
        free(t);
        return MYTHREAD_ESYS;
    }

    *thread = t;
    return MYTHREAD_OK;
}

int mythread_join(mythread_t thread, void **retval)
{
    if (!thread)
    {
        return MYTHREAD_EINVAL;
    }

    if (atomic_load(&thread->joinable) == 0)
    {
        return MYTHREAD_ESTATE;
    }

    for (;;)
    {
        int f = atomic_load(&thread->futex_word);
        if (f == 1)
            break;
        futex_wait(&thread->futex_word, 0);
    }

    if (retval)
        *retval = thread->retval;

    int old = atomic_fetch_sub(&thread->refs, 1);
    if (old == 1)
    {
        release_thread(thread);
    }
    return MYTHREAD_OK;
}

int mythread_detach(mythread_t thread) {
    if (!thread) return MYTHREAD_EINVAL;

    int expected = 1;
    if (!atomic_compare_exchange_strong(&thread->joinable, &expected, 0)) {
        return MYTHREAD_ESTATE; 
    }

    while (atomic_load(&thread->futex_word) == 0) {
        futex_wait(&thread->futex_word, 0);
    }

    int old = atomic_fetch_sub(&thread->refs, 1);
    if (old == 1) {
        release_thread(thread);
    }
    return MYTHREAD_OK;
}