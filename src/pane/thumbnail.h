#pragma once
#include "config/config.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

void thumb_init(Config *cfg);
void thumb_stop(void);
void thumb_enqueue(const char *src);
void thumb_enqueue_dir(const char *dir);
int thumb_get(const char *src, char *out, size_t out_size);
bool thumb_is_cached(const char *src);
bool thumb_is_queued(const char *src);
