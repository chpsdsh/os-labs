#include "proxy.h"

#include "cache.h"
#include "threadpool.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


typedef struct client_task {
    int fd;
} client_task_t;


static void handle_client(void* arg) {
    
}

int proxy_run(int listen_port, int worker_threads) {
    cache_system_init();

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        return 1;
    }

    int yes = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)listen_port);

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sfd);
        return 1;
    }

    if (listen(sfd, 128) < 0) {
        close(sfd);
        return 1;
    }


    threadpool_t* pool = threadpool_create(worker_threads, 1024);
    if (!pool) {
        close(sfd);
        return 1;
    }

    for (;;) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(sfd, (struct sockaddr*)&cli, &cli_len);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            continue;
        }

        client_task_t* task = (client_task_t*)malloc(sizeof(*task));
        if (!task) {
            close(cfd);
            continue;
        }

        task->fd = cfd;
        if (threadpool_submit(pool, handle_client, task) != 0) {
            close(cfd);
            free(task);
        }
    }

    threadpool_destroy(pool);
    close(sfd);
    return 0;
}
