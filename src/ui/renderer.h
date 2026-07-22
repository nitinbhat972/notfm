#pragma once
#include "core/app.h"
#include "pane/pane.h"
#include "pane/preview.h"
#include <notcurses/notcurses.h>

void render_pane(struct ncplane *plane, Pane *p, bool active);
void render_preview(struct notcurses *nc, struct ncplane *plane, Preview *p, bool active, int display_size, int max_rows);
void render_status(struct ncplane *plane, AppState *app);
void render_cmdline(struct ncplane *plane, AppState *app);
