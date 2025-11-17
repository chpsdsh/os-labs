#define _XOPEN_SOURCE 700
#include "uthread.h"

#include <stdlib.h>
#include <stdio.h>

#define UTHREAD_STACK_SIZE (64 * 1024)

struct uthread {
    ucontext_t ctx;
    void *stack;

    void *(*start_routine)(void *);
    void *arg;
    void *retval;

    uthread_state_t state;

    struct uthread *joiner;   
    struct uthread *next;     
};

static uthread_t *current = NULL;       
static uthread_t *main_thread = NULL;   

static uthread_t *ready_head = NULL;
static uthread_t *ready_tail = NULL;

static void enqueue_ready(uthread_t *t);
static uthread_t *dequeue_ready(void);
static void schedule(void);
static void thread_trampoline(void);

void uthread_init(void) {
    if (main_thread != NULL) {
        return; 
    }

    main_thread = (uthread_t *)calloc(1, sizeof(uthread_t));
    if (!main_thread) {
        perror("calloc");
        exit(1);
    }

    if (getcontext(&main_thread->ctx) == -1) {
        perror("getcontext");
        exit(1);
    }

    main_thread->stack = NULL; 
    main_thread->start_routine = NULL;
    main_thread->arg = NULL;
    main_thread->retval = NULL;
    main_thread->state = UT_RUNNING;
    main_thread->joiner = NULL;
    main_thread->next = NULL;

    current = main_thread;
}

static void enqueue_ready(uthread_t *t) {
    t->next = NULL;
    if (!ready_tail) {
        ready_head = ready_tail = t;
    } else {
        ready_tail->next = t;
        ready_tail = t;
    }
}

static uthread_t *dequeue_ready(void) {
    if (!ready_head) return NULL;
    uthread_t *t = ready_head;
    ready_head = t->next;
    if (!ready_head)
        ready_tail = NULL;
    t->next = NULL;
    return t;
}

static void schedule(void) {
    uthread_t *prev = current;
    uthread_t *next = dequeue_ready();

    if (!next) {
        if (prev->state == UT_FINISHED && prev == main_thread) {
            exit(0);
        }
        return;
    }

    current = next;
    current->state = UT_RUNNING;

    if (prev->state == UT_RUNNING) {
        prev->state = UT_READY;
        enqueue_ready(prev);
    }

    if (swapcontext(&prev->ctx, &next->ctx) == -1) {
        perror("swapcontext");
        exit(1);
    }
}

static void thread_trampoline(void) {
    uthread_t *self = current;
    void *ret = self->start_routine(self->arg);

    self->retval = ret;
    self->state = UT_FINISHED;

    if (self->joiner) {
        if (self->joiner->state == UT_BLOCKED) {
            self->joiner->state = UT_READY;
            enqueue_ready(self->joiner);
        }
        self->joiner = NULL;
    }

    schedule();

    fprintf(stderr, "thread_trampoline: unreachable reached\n");
    exit(1);
}


int uthread_create(uthread_t **thread,
                   void *(*start_routine)(void *),
                   void *arg) {
    if (!main_thread) {
        uthread_init();
    }

    uthread_t *t = (uthread_t *)calloc(1, sizeof(uthread_t));
    if (!t) {
        return -1;
    }

    if (getcontext(&t->ctx) == -1) {
        free(t);
        return -1;
    }

    t->stack = malloc(UTHREAD_STACK_SIZE);
    if (!t->stack) {
        free(t);
        return -1;
    }

    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = UTHREAD_STACK_SIZE;
    t->ctx.uc_stack.ss_flags = 0;
    t->ctx.uc_link = NULL; 

    t->start_routine = start_routine;
    t->arg = arg;
    t->retval = NULL;
    t->state = UT_READY;
    t->joiner = NULL;
    t->next = NULL;

    makecontext(&t->ctx, thread_trampoline, 0);

    enqueue_ready(t);

    if (thread) {
        *thread = t;
    }

    return 0;
}

int uthread_join(uthread_t *thread, void **retval) {
    if (!thread) return -1;
    if (thread == current) {
        return -1;
    }

    while (thread->state != UT_FINISHED) {
        current->state = UT_BLOCKED;
        thread->joiner = current;
        schedule();
    }

    if (retval) {
        *retval = thread->retval;
    }

    if (thread->stack) {
        free(thread->stack);
    }
    free(thread);

    return 0;
}

void uthread_yield(void) {
    schedule();
}