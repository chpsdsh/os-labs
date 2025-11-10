#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>

typedef struct mythread* mythread_t;

#define MYTHREAD_OK       0
#define MYTHREAD_ERR     -1
#define MYTHREAD_EINVAL  -2   
#define MYTHREAD_ESTATE  -3   
#define MYTHREAD_ESYS    -4   

int mythread_create(mythread_t* thread,
                    void* (*start_routine)(void*),
                    void* arg);

int mythread_join(mythread_t thread, void** retval);

int mythread_detach(mythread_t thread);



#endif 