#include "clipboard.h"
#include "fs/fs.h"
#include <string.h>

void clip_copy(Clipboard *cb, const char *path) {
  strncpy(cb->path, path, sizeof(cb->path) - 1);
  cb->path[sizeof(cb->path) - 1] = '\0';
  cb->op = CLIP_COPY;
}

void clip_cut(Clipboard *cb, const char *path) {
  strncpy(cb->path, path, sizeof(cb->path) - 1);
  cb->path[sizeof(cb->path) - 1] = '\0';
  cb->op = CLIP_CUT;
}

int clip_paste(Clipboard *cb, const char *dest_dir) {
  if (cb->op == CLIP_NONE)
    return -1;
  const char *base = strrchr(cb->path, '/');
  if (!base)
    base = cb->path;
  else
    base++;
  char dst[PATH_MAX];
  const char *ps[] = {dest_dir, base, NULL};
  fs_join(ps, dst, sizeof(dst));
  if (cb->op == CLIP_COPY)
    return fs_copy(cb->path, dst);
  if (fs_rename(cb->path, dst) != 0) {
    if (fs_copy(cb->path, dst) == 0)
      fs_remove(cb->path);
  }
  clip_clear(cb);
  return 0;
}

void clip_clear(Clipboard *cb) {
  cb->path[0] = '\0';
  cb->op = CLIP_NONE;
}
