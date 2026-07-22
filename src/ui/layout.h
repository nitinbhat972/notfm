#pragma once
#include <notcurses/notcurses.h>

typedef struct {
  unsigned rows, cols;
  unsigned sep1, sep2;
  struct ncplane *parent;
  struct ncplane *current;
  struct ncplane *preview;
  struct ncplane *cmdline;
} Layout;

int  layout_create(struct notcurses *nc, Layout *l);
void layout_destroy(Layout *l);
int  layout_resize(struct notcurses *nc, Layout *l);
