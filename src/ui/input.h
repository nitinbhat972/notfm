#pragma once
#include "ui/layout.h"
#include "core/app.h"
#include <notcurses/notcurses.h>

int input_handle(struct notcurses *nc, Layout *layout, AppState *app);
