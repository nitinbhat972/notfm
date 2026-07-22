#pragma once
#include "fs/fs.h"
#include <stddef.h>

typedef struct {
  char path[PATH_MAX];
  size_t cursor;
  size_t scroll;
} NavState;

typedef struct {
  NavState nav;
  FsEntry *entries;
  size_t count;
  int error;
} Pane;

void pane_reset(Pane *p);
int  pane_update(Pane *p, const char *path, bool show_hidden);
