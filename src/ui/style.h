#pragma once
#include "fs/fs.h"
#include <notcurses/notcurses.h>

typedef struct {
  uint64_t channels;
  uint32_t styles;
} Style;

void style_init(void);
Style style_get(const FsEntry *e);
int ncplane_add_ansi(struct ncplane *n, const char *str);
