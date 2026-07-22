#include "log.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static FILE *log_file = NULL;
static LogLevel log_level = LOG_INFO;

static const char *level_names[] = {"ERROR", "WARN", "INFO"};

bool log_open(const char *path) {
  char buf[1024];
  if (!path) {
    const char *state = getenv("XDG_STATE_HOME");
    if (!state)
      state = getenv("HOME");
    if (!state)
      return false;
    snprintf(buf, sizeof(buf), "%s/notfm/notfm.log", state);
    path = buf;
  }
  char dir[PATH_MAX];
  strncpy(dir, path, sizeof(dir) - 1);
  char *slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    mkdir(dir, 0755);
  }
  log_file = fopen(path, "a");
  return log_file != NULL;
}

void log_write(LogLevel level, const char *fmt, ...) {
  if (level > log_level)
    return;
  FILE *out = log_file ? log_file : stderr;
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char ts[64];
  strftime(ts, sizeof(ts), "%H:%M:%S", tm);
  fprintf(out, "[%s] %s: ", ts, level_names[level]);
  va_list args;
  va_start(args, fmt);
  vfprintf(out, fmt, args);
  va_end(args);
  fputc('\n', out);
  fflush(out);
}

void log_close(void) {
  if (log_file)
    fclose(log_file);
  log_file = NULL;
}
