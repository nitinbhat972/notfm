#pragma once
#include <limits.h>

typedef enum { CLIP_NONE, CLIP_COPY, CLIP_CUT } ClipboardOp;

typedef struct {
  char path[PATH_MAX];
  ClipboardOp op;
} Clipboard;

void clip_copy(Clipboard *cb, const char *path);
void clip_cut(Clipboard *cb, const char *path);
int  clip_paste(Clipboard *cb, const char *dest_dir);
void clip_clear(Clipboard *cb);
