#include "app.h"
#include "fs/fs.h"
#include "pane/thumbnail.h"
#include "utils/fuzzy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_match_desc(const void *a, const void *b) {
  const Match *ma = a, *mb = b;

  if (ma->score != mb->score)
    return mb->score - ma->score;

  return (int)(ma->idx - mb->idx);
}

int app_init(AppState *app, const char *start_dir, Config *cfg) {
  *app = (AppState){0};
  app->running = true;
  app->config = cfg;

  char abs_path[PATH_MAX];

  if (fs_get_abs(start_dir, abs_path, sizeof(abs_path)) == -1)
    return -1;

  strncpy(app->current.nav.path, abs_path, sizeof(app->current.nav.path) - 1);
  vec_init(&app->cache);
  vec_init(&app->find_results);
  vec_init(&app->last_find_results);

  return app_refresh(app);
}

static void save_nav(AppState *app, NavState nav) {
  size_t i = 0;
  NavState cur;

  vec_foreach(&app->cache, cur) {
    if (strcmp(cur.path, nav.path) == 0) {
      app->cache.data[i] = nav;
      return;
    }
    i++;
  }
  vec_push(&app->cache, nav);
}

static void load_nav(AppState *app, const char *path, NavState *out) {
  NavState cur;

  vec_foreach(&app->cache, cur) {
    if (strcmp(cur.path, path) == 0) {
      *out = cur;
      return;
    }
  }

  *out = (NavState){0};
  strncpy(out->path, path, sizeof(out->path) - 1);
}

int app_refresh(AppState *app) {
  bool show = app->config->show_hidden;
  char parent_path[PATH_MAX];

  fs_parent(app->current.nav.path, parent_path, sizeof(parent_path));
  pane_update(&app->parent, parent_path, show);
  pane_update(&app->current, app->current.nav.path, show);

  const char *cur_name = strrchr(app->current.nav.path, '/');

  if (cur_name++) {
    for (size_t i = 0; i < app->parent.count; i++)
      if (strcmp(app->parent.entries[i].basename, cur_name) == 0) {
        app->parent.nav.cursor = i;
        break;
      }
  }

  FsEntry *sel = app->current.count > 0
                     ? &app->current.entries[app->current.nav.cursor]
                     : NULL;

  preview_update(&app->preview, app->current.nav.path, sel, show);
  thumb_enqueue_dir(app->current.nav.path);

  return 0;
}

int app_cursor(AppState *app, int offset) {
  if (app->current.count == 0)
    return 0;

  int next = (int)app->current.nav.cursor + offset;

  if (next < 0)
    next = (int)app->current.count - 1;

  if (next >= (int)app->current.count)
    next = 0;

  app->current.nav.cursor = (size_t)next;

  return app_refresh(app);
}

int app_enter(AppState *app) {
  if (app->current.count == 0)
    return 0;

  FsEntry *sel = &app->current.entries[app->current.nav.cursor];

  if (!sel->is_dir)
    return 0;

  save_nav(app, app->current.nav);

  char next[PATH_MAX];
  const char *ps[] = {app->current.nav.path, sel->basename, NULL};
  fs_join(ps, next, sizeof(next));
  load_nav(app, next, &app->current.nav);

  return app_refresh(app);
}

int app_parent(AppState *app) {
  if (strcmp(app->current.nav.path, "/") == 0)
    return 0;

  NavState old = app->current.nav;
  save_nav(app, old);

  char p[PATH_MAX];
  fs_parent(app->current.nav.path, p, sizeof(p));
  load_nav(app, p, &app->current.nav);
  app_refresh(app);

  if (app->current.nav.cursor == 0) {
    const char *child = strrchr(old.path, '/');
    if (child++) {
      for (size_t i = 0; i < app->current.count; i++)
        if (strcmp(app->current.entries[i].basename, child) == 0) {
          app->current.nav.cursor = i;
          break;
        }
    }
  }

  return 0;
}

int app_execute_command(AppState *app, const char *cmd) {
  char buf[1024];
  strncpy(buf, cmd, sizeof(buf) - 1);
  char *command = strtok(buf, " ");
  char *arg = strtok(NULL, "");

  if (!command)
    return -1;

  if (strcmp(command, "q") == 0) {
    app->running = false;
  } else if (strcmp(command, "add") == 0 && arg) {
    char p[PATH_MAX];
    const char *ps[] = {app->current.nav.path, arg, NULL};
    fs_join(ps, p, sizeof(p));

    size_t len = strlen(arg);

    if (len > 0 && arg[len - 1] == '/')
      fs_mkdir(p);

    else
      fs_create_file(p);

    app_refresh(app);
  } else if (strcmp(command, "rename") == 0 && arg && app->current.count > 0) {
    char old[PATH_MAX], next[PATH_MAX];
    FsEntry *sel = &app->current.entries[app->current.nav.cursor];

    const char *p1[] = {app->current.nav.path, sel->basename, NULL};
    const char *p2[] = {app->current.nav.path, arg, NULL};

    fs_join(p1, old, sizeof(old));
    fs_join(p2, next, sizeof(next));

    fs_rename(old, next);
    app_refresh(app);
  } else if (strcmp(command, "delete") == 0) {
    app->confirm_active = true;
  } else if (strcmp(command, "find") == 0 && arg) {
    app_find_start(app);
    strncpy(app->find_buffer, arg, sizeof(app->find_buffer) - 1);

    app_find_update(app);
    app_find_accept(app);
  } else if (strcmp(command, "paste") == 0) {
    clip_paste(&app->clipboard, app->current.nav.path);
    app_refresh(app);
  }

  return 0;
}

int app_delete_selected(AppState *app) {
  if (app->current.count == 0)
    return -1;

  char p[PATH_MAX];
  const char *ps[] = {app->current.nav.path,
                      app->current.entries[app->current.nav.cursor].basename,
                      NULL};

  fs_join(ps, p, sizeof(p));
  fs_remove(p);

  return app_refresh(app);
}

void app_free(AppState *app) {
  if (!app)
    return;

  pane_reset(&app->parent);
  pane_reset(&app->current);
  preview_free(&app->preview);

  vec_free(&app->cache);
  vec_free(&app->find_results);
  vec_free(&app->last_find_results);
}

int app_find_start(AppState *app) {
  app->find_orig_cursor = app->current.nav.cursor;
  app->find_buffer[0] = '\0';

  vec_clear(&app->find_results);

  app->find_result_idx = 0;
  app->find_active = true;

  return 0;
}

int app_find_update(AppState *app) {
  vec_clear(&app->find_results);
  app->find_result_idx = 0;

  if (!app->find_buffer[0] || app->current.count == 0)
    return 0;

  Match *matches = malloc(app->current.count * sizeof(Match));
  if (!matches)
    return -1;

  size_t n = 0;
  for (size_t i = 0; i < app->current.count; i++) {
    int s = fuzzy_match(app->find_buffer, app->current.entries[i].basename);
    if (s > 0) {
      matches[n].score = s;
      matches[n].idx = i;
      n++;
    }
  }

  if (n > 0) {
    qsort(matches, n, sizeof(Match), cmp_match_desc);
    for (size_t i = 0; i < n; i++)
      vec_push(&app->find_results, matches[i].idx);
    app->current.nav.cursor = app->find_results.data[0];
    app->find_result_idx = 0;
  }

  free(matches);
  return 0;
}

int app_find_accept(AppState *app) {
  app->find_active = false;
  strncpy(app->last_find_buffer, app->find_buffer,
          sizeof(app->last_find_buffer) - 1);

  vec_clear(&app->last_find_results);

  size_t val;
  vec_foreach(&app->find_results, val) vec_push(&app->last_find_results, val);

  return 0;
}

int app_find_cancel(AppState *app) {
  app->find_active = false;
  app->current.nav.cursor = app->find_orig_cursor;
  app->find_buffer[0] = '\0';

  vec_clear(&app->find_results);
  app->find_result_idx = 0;

  return 0;
}

int app_find_next(AppState *app) {
  if (app->last_find_results.len == 0)
    return 0;

  app->find_result_idx =
      (app->find_result_idx + 1) % app->last_find_results.len;
  app->current.nav.cursor = app->last_find_results.data[app->find_result_idx];

  return 0;
}

int app_find_prev(AppState *app) {
  if (app->last_find_results.len == 0)
    return 0;

  if (app->find_result_idx == 0)
    app->find_result_idx = app->last_find_results.len - 1;
  else
    app->find_result_idx--;

  app->current.nav.cursor = app->last_find_results.data[app->find_result_idx];

  return 0;
}
