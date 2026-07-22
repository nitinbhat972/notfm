#pragma once
#include "core/clipboard.h"
#include "pane/pane.h"
#include "pane/preview.h"
#include "utils/vec.h"
#include "config/config.h"
#include <limits.h>
#include <stdbool.h>

typedef struct {
  int score;
  size_t idx;
} Match;

typedef struct {
  Pane parent;
  Pane current;
  Preview preview;
  Vec(NavState) cache;
  Clipboard clipboard;
  Config *config;
  bool running;
  bool prompt_active;
  bool confirm_active;
  char prompt_buffer[1024];
  size_t prompt_cursor;
  bool open_file_pending;
  char open_file_path[PATH_MAX];
  bool pending_y;
  bool pending_d;
  bool find_active;
  char find_buffer[256];
  size_t find_orig_cursor;
  Vec(size_t) find_results;
  size_t find_result_idx;
  char last_find_buffer[256];
  Vec(size_t) last_find_results;
} AppState;

int  app_init(AppState *app, const char *start_dir, Config *cfg);
void app_free(AppState *app);
int  app_refresh(AppState *app);
int  app_enter(AppState *app);
int  app_parent(AppState *app);
int  app_cursor(AppState *app, int offset);
int  app_execute_command(AppState *app, const char *cmd);
int  app_delete_selected(AppState *app);
int  app_find_start(AppState *app);
int  app_find_update(AppState *app);
int  app_find_accept(AppState *app);
int  app_find_cancel(AppState *app);
int  app_find_next(AppState *app);
int  app_find_prev(AppState *app);
