#pragma once
#include <stdbool.h>

typedef enum { LOG_ERROR, LOG_WARN, LOG_INFO } LogLevel;

bool log_open(const char *path);
void log_write(LogLevel level, const char *fmt, ...);
void log_close(void);
