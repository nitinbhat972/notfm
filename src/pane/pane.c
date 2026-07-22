#include "pane.h"
#include <errno.h>
#include <string.h>

void pane_reset(Pane *p) {
  if (p->entries) {
    fs_free(p->entries);
    p->entries = NULL;
  }
  p->count = 0;
  p->error = 0;
}

int pane_update(Pane *p, const char *path, bool show_hidden) {
  pane_reset(p);
  strncpy(p->nav.path, path, sizeof(p->nav.path) - 1);
  p->nav.path[sizeof(p->nav.path) - 1] = '\0';
  int res = fs_list(path, &p->entries, &p->count, show_hidden);
  if (res == -1)
    p->error = errno;
  return res;
}
