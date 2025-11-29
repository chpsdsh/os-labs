#include "my_thread.h"

static _Atomic int reaper_started = 0;
static _Atomic int reaper_futex = 0;
static _Atomic(mythread *) reaper_list = NULL;

static void reaper_enqueue(mythread *t)
{
    mythread *old_head;
    while (1)
    {
        old_head = atomic_load(&reaper_list);
        t->next_reap = old_head;
        if (atomic_compare_exchange_weak(&reaper_list, &old_head, t))
        {
            break;
        }
    }
}

static int reaper_func(void *arg)
{
    (void)arg;

    for (;;)
    {
        while (atomic_load(&reaper_futex) == 0)
        {
            futex_wait(&reaper_futex, 0);
        }

        atomic_store(&reaper_futex, 0);

        mythread *list = atomic_exchange(&reaper_list, NULL);
        while (list)
        {
            mythread *next = list->next_reap;
            release_thread(list);
            list = next;
        }
    }
    return 0;
}

static void start_reaper_if_needed(void)
{
    int expected = 0;
    if (!atomic_compare_exchange_strong(&reaper_started, &expected, 1))
        return;

    void *stack = mmap(NULL, MYTHREAD_STACK_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                       -1, 0);
    if (stack == MAP_FAILED)
    {
        atomic_store(&reaper_started, 0);
        return;
    }

    void *stack_top = (uint8_t *)stack + MYTHREAD_STACK_SIZE;

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES |
                CLONE_SIGHAND | CLONE_SYSVSEM | CLONE_THREAD;

    int rc = clone(reaper_func, stack_top, flags, NULL);
    if (rc == -1)
    {
        munmap(stack, MYTHREAD_STACK_SIZE);
        atomic_store(&reaper_started, 0);
        return;
    }
}

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

    if (atomic_load(&t->joinable) == 0)
    {
        reaper_enqueue(t);
        atomic_store(&reaper_futex, 1);
        futex_wake(&reaper_futex, 1);
    }

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

    start_reaper_if_needed();

    mythread *t = (mythread *)calloc(1, sizeof(*t));
    if (!t)
        return MYTHREAD_ERR;

    t->start_routine = start_routine;
    t->arg = arg;
    t->retval = NULL;

    atomic_store(&t->joinable, 1);
    atomic_store(&t->futex_word, 0);
    t->next_reap = NULL;

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

    int expected = 1;
    if (!atomic_compare_exchange_strong(&thread->joinable, &expected, -1))
    {
        return MYTHREAD_ESTATE;
    }

    while (atomic_load(&thread->futex_word) == 0)
    {
        futex_wait(&thread->futex_word, 0);
    }

    if (retval)
        *retval = thread->retval;

    release_thread(thread);

    return MYTHREAD_OK;
}

int mythread_detach(mythread_t thread)
{
    if (!thread)
        return MYTHREAD_EINVAL;

    int expected = 1;
    if (!atomic_compare_exchange_strong(&thread->joinable, &expected, 0))
    {
        return MYTHREAD_ESTATE;
    }

    if (atomic_load(&thread->futex_word) == 1)
    {
        reaper_enqueue(thread);
        atomic_store(&reaper_futex, 1);
        futex_wake(&reaper_futex, 1);
    }

    return MYTHREAD_OK;
}
