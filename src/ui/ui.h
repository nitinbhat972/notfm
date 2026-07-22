#pragma once
#include "ui/layout.h"
#include "core/app.h"
#include <notcurses/notcurses.h>
#include <termios.h>

typedef struct {
  struct notcurses *nc;
  Layout layout;
  struct termios tio;
} Ui;

int  ui_init(Ui *ui);
void ui_suspend(Ui *ui);
void ui_free_no_restore(Ui *ui);
int  ui_render(Ui *ui, AppState *app);
int  ui_handle_input(Ui *ui, AppState *app);
