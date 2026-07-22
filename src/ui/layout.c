#include "layout.h"

int layout_create(struct notcurses *nc, Layout *l) {
  *l = (Layout){0};
  struct ncplane *std = notcurses_stdplane(nc);
  ncplane_dim_yx(std, &l->rows, &l->cols);

  l->sep1 = (unsigned)(l->cols * 0.2);
  l->sep2 = (unsigned)(l->cols * 0.5);

  unsigned h = l->rows - 1;
  struct ncplane_options pane_opts = {.rows = h};

  pane_opts.y = 0; pane_opts.x = 0; pane_opts.cols = l->sep1;
  l->parent = ncplane_create(std, &pane_opts);
  if (!l->parent) return -1;

  pane_opts.x = l->sep1 + 1; pane_opts.cols = l->sep2 - l->sep1 - 1;
  l->current = ncplane_create(std, &pane_opts);
  if (!l->current) return -1;

  pane_opts.x = l->sep2 + 1; pane_opts.cols = l->cols - l->sep2 - 1;
  l->preview = ncplane_create(std, &pane_opts);
  if (!l->preview) return -1;

  pane_opts.y = l->rows - 1; pane_opts.x = 0; pane_opts.rows = 1; pane_opts.cols = l->cols;
  l->cmdline = ncplane_create(std, &pane_opts);
  if (!l->cmdline) return -1;

  return 0;
}

void layout_destroy(Layout *l) {
  if (!l) return;
  if (l->parent)   { ncplane_destroy(l->parent);   l->parent = NULL; }
  if (l->current)  { ncplane_destroy(l->current);  l->current = NULL; }
  if (l->preview)  { ncplane_destroy(l->preview);  l->preview = NULL; }
  if (l->cmdline)  { ncplane_destroy(l->cmdline);  l->cmdline = NULL; }
}

int layout_resize(struct notcurses *nc, Layout *l) {
  layout_destroy(l);
  return layout_create(nc, l);
}
