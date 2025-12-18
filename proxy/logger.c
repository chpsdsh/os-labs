#include "logger.h"

#include <stdarg.h>
#include <time.h>

static log_level_t g_min_level = LOG_INFO;

static FILE *g_log_file = NULL;

static void logger_vprint(log_level_t msg_level,
                          const char *level_name,
                          const char *format,
                          va_list args)
{
    if (msg_level > g_min_level || !g_log_file)
        return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (tm_info) {
        fprintf(g_log_file,
                "[%02d:%02d:%02d] %-5s | ",
                tm_info->tm_hour,
                tm_info->tm_min,
                tm_info->tm_sec,
                level_name);
    } else {
        fprintf(g_log_file, "[--:--:--] %-5s | ", level_name);
    }

    vfprintf(g_log_file, format, args);
    fprintf(g_log_file, "\n");
    fflush(g_log_file);
}

int logger_init(log_level_t min_level)
{
    g_min_level = min_level;

    g_log_file = fopen("proxy_logs.log", "a");
    if (!g_log_file)
        return -1;

    return 0;
}

void logger_finalize(void)
{
    if (g_log_file) {
        fflush(g_log_file);
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

void log_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logger_vprint(LOG_ERROR, "ERROR", format, args);
    va_end(args);
}

void log_info(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logger_vprint(LOG_INFO, "INFO", format, args);
    va_end(args);
}

void log_debug(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logger_vprint(LOG_DEBUG, "DEBUG", format, args);
    va_end(args);
}
