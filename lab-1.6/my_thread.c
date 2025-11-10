#include "my_thread.h"

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

    int rc = clone(thread_trampoline, stack_top, flags, t);
    
    if (rc == -1)
    {
        munmap(t->stack, t->stack_size);
        free(t);
        return MYTHREAD_ESYS;
    }

    *thread = t;
    return MYTHREAD_OK;
}