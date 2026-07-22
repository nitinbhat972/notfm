#pragma once
#include "fs/fs.h"
#include "pane.h"
#include "utils/vec.h"
#include <notcurses/notcurses.h>

typedef enum {
  PREVIEW_NONE,
  PREVIEW_DIR,
  PREVIEW_TEXT,
  PREVIEW_IMAGE,
  PREVIEW_CACHING,
  PREVIEW_ERROR,
} PreviewMode;

typedef struct {
  PreviewMode mode;
  Pane subdir;
  StrVec text;
  char image_path[PATH_MAX];
  char caching_src[PATH_MAX];
  int error;
} Preview;

void preview_init(Preview *p);
void preview_update(Preview *p, const char *parent_path, FsEntry *entry, bool show_hidden);
void preview_check_cached(Preview *p);
void preview_free(Preview *p);
