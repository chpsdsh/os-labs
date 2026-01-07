#include "proxy.h"

#include "cache.h"
#include "logger.h"
#include "threadpool.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static void* memfind(const void* hay, size_t hay_len, const void* needle, size_t n_len) {
    if (!hay || !needle || n_len == 0 || n_len > hay_len)
        return NULL;
    const unsigned char* h = (const unsigned char*)hay;
    const unsigned char* n = (const unsigned char*)needle;

    for (size_t i = 0; i + n_len <= hay_len; ++i) {
        if (h[i] == n[0] && memcmp(h + i, n, n_len) == 0) {
            return (void*)(h + i);
        }
    }
    return NULL;
}

static int strncasecmp_simple(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca == '\0' || cb == '\0')
            return (int)ca - (int)cb;
        ca = (unsigned char)tolower(ca);
        cb = (unsigned char)tolower(cb);
        if (ca != cb)
            return (int)ca - (int)cb;
    }
    return 0;
}

static int send_all(int fd, const void* buf, size_t len) {
    const char* p = (const char*)buf;
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, p + off, len - off, 0);
        if (n <= 0)
            return -1;
        off += (size_t)n;
    }
    return 0;
}

static int read_request_headers(int fd, char* buf, int cap) {
    int used = 0;
    while (used < cap) {
        ssize_t n = recv(fd, buf + used, cap - used, 0);
        if (n <= 0)
            return -1;
        used += (int)n;
        if (used >= 4 && memfind(buf, (size_t)used, "\r\n\r\n", 4)) {
            return used;
        }
    }
    return -1;
}

static int connect_host_port(const char* host, int port) {
    struct hostent* he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        log_error("gethostbyname(%s) failed", host);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        log_error("socket() for origin failed, errno=%d", errno);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_error("connect(%s:%d) failed, errno=%d", host, port, errno);
        close(fd);
        return -1;
    }

    return fd;
}

static int parse_http_get(const char* req, int req_len,
                          char* host, int host_cap,
                          char* path, int path_cap) {
    const char* line_end = memfind(req, (size_t)req_len, "\r\n", 2);
    if (!line_end)
        return -1;

    const char* p = req;
    if (strncmp(p, "GET ", 4) != 0)
        return -1;
    p += 4;

    const char* sp = memfind(p, (size_t)(line_end - p), " ", 1);
    if (!sp)
        return -1;

    int url_len = (int)(sp - p);
    if (url_len <= 0 || url_len >= 1024)
        return -1;

    char url[1024];
    memcpy(url, p, (size_t)url_len);
    url[url_len] = '\0';

    if (strncmp(url, "http://", 7) == 0) {
        const char* h = url + 7;
        const char* slash = strchr(h, '/');
        if (!slash)
            return -1;

        int hlen = (int)(slash - h);
        if (hlen <= 0 || hlen >= host_cap)
            return -1;
        memcpy(host, h, (size_t)hlen);
        host[hlen] = '\0';

        int plen = (int)strlen(slash);
        if (plen <= 0 || plen >= path_cap)
            return -1;
        memcpy(path, slash, (size_t)plen + 1);
        return 0;
    }

    if (url_len >= path_cap)
        return -1;
    memcpy(path, url, (size_t)url_len);
    path[url_len] = '\0';

    const char* hdrs = line_end + 2;
    const char* end = req + req_len;


    const char* cur = hdrs;
    while (cur < end) {
        const char* eol = memfind(cur, (size_t)(end - cur), "\r\n", 2);
        if (!eol)
            break;

        int len = (int)(eol - cur);
        if (len == 0)
            break;

        if (len >= 5 && strncasecmp_simple(cur, "Host:", 5) == 0) {
            const char* v = cur + 5;
            while (v < eol && (*v == ' ' || *v == '\t'))
                v++;
            int hlen = (int)(eol - v);
            if (hlen <= 0 || hlen >= host_cap)
                return -1;
            memcpy(host, v, (size_t)hlen);
            host[hlen] = '\0';
            return 0;
        }

        cur = eol + 2;
    }

    return -1;
}

typedef struct client_task {
    int fd;
} client_task_t;

static void send_simple(int fd, const char* status, const char* body) {
    char msg[512];
    int n = snprintf(msg, sizeof(msg),
                     "HTTP/1.0 %s\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "%s",
                     status, strlen(body), body);
    if (n > 0)
        (void)send_all(fd, msg, (size_t)n);
}

static void stream_cached_to_client(int cfd, cache_item_t* it) {
    size_t sent = 0;

    pthread_mutex_lock(&it->mtx);
    for (;;) {
        while (sent == it->length && !it->done && !it->error) {
            pthread_cond_wait(&it->cv, &it->mtx);
        }

        if (sent < it->length) {
            size_t chunk = it->length - sent;
            const char* ptr = it->buffer + sent;

            pthread_mutex_unlock(&it->mtx);
            ssize_t n = send(cfd, ptr, chunk, 0);
            if (n <= 0) {
                return;
            }
            sent += (size_t)n;
            pthread_mutex_lock(&it->mtx);
            continue;
        }

        if (it->done || it->error)
            break;
    }
    pthread_mutex_unlock(&it->mtx);
}

static void handle_client(void* arg) {
    client_task_t* t = (client_task_t*)arg;
    int cfd = t->fd;
    free(t);

    char req[8192];
    int req_len = read_request_headers(cfd, req, (int)sizeof(req));
    if (req_len <= 0) {
        close(cfd);
        return;
    }

    char host[256];
    char path[1024];
    if (parse_http_get(req, req_len, host, (int)sizeof(host), path, (int)sizeof(path)) != 0) {
        send_simple(cfd, "400 Bad Request", "Bad Request\n");
        close(cfd);
        return;
    }

    char key[CACHE_URL_MAXLEN];
    int key_len = snprintf(key, sizeof(key), "%s%s", host, path);
    if (key_len <= 0 || key_len >= (int)sizeof(key)) {
        send_simple(cfd, "414 Request-URI Too Long", "URL too long\n");
        close(cfd);
        return;
    }

    int am_writer = 0;
    cache_item_t* it = cache_acquire(key, &am_writer);
    if (!it) {
        send_simple(cfd, "503 Service Unavailable", "Cache full\n");
        close(cfd);
        return;
    }

    if (am_writer) {
        log_info("CACHE MISS (writer): %s", key);

        int sfd = connect_host_port(host, 80);
        if (sfd < 0) {
            cache_finish_err(it);
            send_simple(cfd, "502 Bad Gateway", "Bad Gateway\n");
            close(cfd);
            cache_release(it);
            return;
        }

        char oreq[2048];
        int oreq_len = snprintf(oreq, sizeof(oreq),
                                "GET %s HTTP/1.0\r\n"
                                "Host: %s\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                                path, host);

        if (oreq_len <= 0 || oreq_len >= (int)sizeof(oreq) ||
            send_all(sfd, oreq, (size_t)oreq_len) != 0) {
            close(sfd);
            cache_finish_err(it);
            send_simple(cfd, "502 Bad Gateway", "Bad Gateway\n");
            close(cfd);
            cache_release(it);
            return;
        }


        char buf[4096];
        for (;;) {
            ssize_t n = recv(sfd, buf, sizeof(buf), 0);
            if (n == 0)
                break;
            if (n < 0) {
                close(sfd);
                cache_finish_err(it);
                if (cfd >= 0)
                    close(cfd);

                cache_release(it);
                return;
            }

            if (cache_write(it, buf, (size_t)n) != CACHE_SUCCESS) {
                close(sfd);
                cache_finish_err(it);
                if (cfd >= 0)
                    close(cfd);
                cache_release(it);
                return;
            }

            if (cfd >= 0) {
                if (send_all(cfd, buf, (size_t)n) != 0) {
                    close(cfd);
                    cfd = -1;
                }
            }
        }

        close(sfd);
        cache_finish_ok(it);

        if (cfd >= 0)
            close(cfd);
        cache_release(it);
        return;
    } else {
        log_info("CACHE HIT (reader): %s", key);

        stream_cached_to_client(cfd, it);

        if (cfd >= 0)
            close(cfd);
        cache_release(it);
        return;
    }
}

int proxy_run(int listen_port, int worker_threads) {
    cache_system_init();

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        log_error("socket() failed errno=%d", errno);
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
        log_error("bind() failed port=%d errno=%d", listen_port, errno);
        close(sfd);
        return 1;
    }

    if (listen(sfd, 128) < 0) {
        log_error("listen() failed errno=%d", errno);
        close(sfd);
        return 1;
    }

    log_info("proxy_run listening port=%d workers=%d", listen_port, worker_threads);

    threadpool_t* pool = threadpool_create(worker_threads, 1024);
    if (!pool) {
        log_error("threadpool_create failed");
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
            log_error("accept() failed errno=%d", errno);
            continue;
        }

        client_task_t* task = (client_task_t*)malloc(sizeof(*task));
        if (!task) {
            send_simple(cfd, "503 Service Unavailable", "No memory\n");
            close(cfd);
            continue;
        }

        task->fd = cfd;
        if (threadpool_submit(pool, handle_client, task) != 0) {
            send_simple(cfd, "503 Service Unavailable", "Busy\n");
            close(cfd);
            free(task);
        }
    }

    threadpool_destroy(pool);
    close(sfd);
    return 0;
}
