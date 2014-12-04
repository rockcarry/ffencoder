/* 包含头文件 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

// 内部常量定义
#define LOG_MODE_DISABLE  0
#define LOG_MODE_FILE     1
#define LOG_MODE_DEBUGER  2
#define LOG_MODE_STDOUT   3
#define LOG_MODE_STDERR   4

// 内部全部变量定义
static FILE *s_log_fp   = NULL;
static DWORD s_log_mode = LOG_MODE_DISABLE;

/* 函数实现 */
static void log_init(char *file)
{
    if (!s_log_fp) {
        if (strcmp(file, "DEBUGER") == 0) {
            s_log_mode = LOG_MODE_DEBUGER;
        }
        else if (strcmp(file, "STDOUT") == 0) {
            s_log_mode = LOG_MODE_STDOUT;
            s_log_fp   = stdout;
        }
        else if (strcmp(file, "STDERR") == 0) {
            s_log_mode = LOG_MODE_STDERR;
            s_log_fp   = stderr;
        }
        else {
            s_log_fp = fopen(file, "w");
            if (s_log_fp) {
                s_log_mode = LOG_MODE_FILE;
            }
        }
    }
}

static void log_done(void)
{
    if (s_log_mode == LOG_MODE_FILE)
    {
        fflush(s_log_fp);
        fclose(s_log_fp);
        s_log_fp = NULL;
    }
    s_log_mode = LOG_MODE_DISABLE;
}

#define MAX_LOG_BUF 1024
static void log_printf(char *format, ...)
{
    char buf[MAX_LOG_BUF];
    va_list valist;

    // if debug log is not enable, directly return
    if (s_log_mode == LOG_MODE_DISABLE) return;

    va_start(valist, format);
    vsnprintf(buf, MAX_LOG_BUF, format, valist);
    va_end(valist);

    if (s_log_mode == LOG_MODE_DEBUGER)
    {
        OutputDebugString(buf);
    }
    else
    {
        fputs(buf, s_log_fp);
        fflush(s_log_fp);
    }
}
