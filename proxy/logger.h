#ifndef CACHE_PROXY_LOGGER_H
#define CACHE_PROXY_LOGGER_H

#include <stdio.h>

typedef enum {
    LOG_ERROR = 0,
    LOG_INFO  = 1,
    LOG_DEBUG = 2
} log_level_t;

int  logger_init(log_level_t min_level);

void logger_finalize(void);

void log_error(const char *format, ...);
void log_info (const char *format, ...);
void log_debug(const char *format, ...);

#endif 
