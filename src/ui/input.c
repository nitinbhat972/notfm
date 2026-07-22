#include "input.h"
#include "core/clipboard.h"
#include "fs/fs.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int on_confirm(AppState *app, struct ncinput *ni) {
  if (ni->id == 'y' || ni->id == 'Y')
    app_delete_selected(app);
  app->confirm_active = false;
  return 0;
}

static int on_command(AppState *app, struct ncinput *ni) {
  size_t len = strlen(app->prompt_buffer);

  if (ni->id == NCKEY_ENTER) {
    app_execute_command(app, app->prompt_buffer);
    app->prompt_active = false;
    app->prompt_buffer[0] = '\0';
    app->prompt_cursor = 0;
  } else if (ni->id == NCKEY_ESC) {
    app->prompt_active = false;
    app->prompt_buffer[0] = '\0';
    app->prompt_cursor = 0;
  } else if (ni->id == NCKEY_LEFT) {
    if (app->prompt_cursor > 0)
      app->prompt_cursor--;
  } else if (ni->id == NCKEY_RIGHT) {
    if (app->prompt_cursor < len)
      app->prompt_cursor++;
  } else if (ni->id == NCKEY_BACKSPACE) {
    if (app->prompt_cursor > 0) {
      memmove(&app->prompt_buffer[app->prompt_cursor - 1],
              &app->prompt_buffer[app->prompt_cursor],
              len - app->prompt_cursor + 1);
      app->prompt_cursor--;
    }
  } else if (ni->id == NCKEY_DEL) {
    if (app->prompt_cursor < len)
      memmove(&app->prompt_buffer[app->prompt_cursor],
              &app->prompt_buffer[app->prompt_cursor + 1],
              len - app->prompt_cursor);
  } else if (ni->id < 128 && isprint((int)ni->id)) {
    if (len < sizeof(app->prompt_buffer) - 1) {
      memmove(&app->prompt_buffer[app->prompt_cursor + 1],
              &app->prompt_buffer[app->prompt_cursor],
              len - app->prompt_cursor + 1);
      app->prompt_buffer[app->prompt_cursor] = (char)ni->id;
      app->prompt_cursor++;
    }
  }
  return 0;
}

static int on_find(AppState *app, struct ncinput *ni) {
  if (ni->id == NCKEY_ENTER) {
    app_find_accept(app);
  } else if (ni->id == NCKEY_ESC) {
    app_find_cancel(app);
  } else if (ni->id == NCKEY_BACKSPACE || ni->id == NCKEY_DEL) {
    size_t len = strlen(app->find_buffer);
    if (len > 0) {
      app->find_buffer[len - 1] = '\0';
      app_find_update(app);
    }
  } else if (ni->id < 128 && isprint((int)ni->id)) {
    size_t len = strlen(app->find_buffer);
    if (len < sizeof(app->find_buffer) - 1) {
      app->find_buffer[len] = (char)ni->id;
      app->find_buffer[len + 1] = '\0';
      app_find_update(app);
    }
  }
  return 0;
}

static uint32_t normalize_key(uint32_t id, unsigned mods) {
  if (!(mods & NCKEY_MOD_SHIFT))
    return id;
  if (id < 32 || id > 126)
    return id;
  if (id >= 'a' && id <= 'z')
    return (uint32_t)(id - 32);
  switch ((char)id) {
  case '1':
    return '!';
  case '2':
    return '@';
  case '3':
    return '#';
  case '4':
    return '$';
  case '5':
    return '%';
  case '6':
    return '^';
  case '7':
    return '&';
  case '8':
    return '*';
  case '9':
    return '(';
  case '0':
    return ')';
  case '-':
    return '_';
  case '=':
    return '+';
  case '[':
    return '{';
  case ']':
    return '}';
  case '\\':
    return '|';
  case ';':
    return ':';
  case '\'':
    return '"';
  case ',':
    return '<';
  case '.':
    return '>';
  case '/':
    return '?';
  case '`':
    return '~';
  default:
    return id;
  }
}

static int on_normal(struct notcurses *nc, Layout *layout, AppState *app,
                     struct ncinput *ni) {
  (void)nc;
  uint32_t id = ni->id;

  bool py = app->pending_y, pd = app->pending_d;
  app->pending_y = app->pending_d = false;

  switch (id) {
  case '/':
    app_find_start(app);
    break;
  case 'n':
    app_find_next(app);
    break;
  case 'N':
    app_find_prev(app);
    break;
  case ':':
    app->prompt_active = true;
    app->prompt_buffer[0] = '\0';
    app->prompt_cursor = 0;
    break;
  case 'a':
    app->prompt_active = true;
    snprintf(app->prompt_buffer, sizeof(app->prompt_buffer), "add ");
    app->prompt_cursor = strlen(app->prompt_buffer);
    break;
  case 'r':
    if (app->current.count > 0) {
      app->prompt_active = true;
      snprintf(app->prompt_buffer, sizeof(app->prompt_buffer), "rename %s ",
               app->current.entries[app->current.nav.cursor].basename);
      app->prompt_cursor = strlen(app->prompt_buffer);
    }
    break;
  case 'y':
    if (py) {
      if (app->current.count) {
        char path[PATH_MAX];
        FsEntry *e = &app->current.entries[app->current.nav.cursor];
        const char *ps[] = {app->current.nav.path, e->basename, NULL};
        fs_join(ps, path, sizeof(path));
        clip_copy(&app->clipboard, path);
      }
    } else
      app->pending_y = true;
    break;
  case 'd':
    if (pd)
      app->confirm_active = true;
    else
      app->pending_d = true;
    break;
  case 'p':
    clip_paste(&app->clipboard, app->current.nav.path);
    app_refresh(app);
    break;
  case 'c':
  case 'x':
    if (app->current.count) {
      char path[PATH_MAX];
      FsEntry *e = &app->current.entries[app->current.nav.cursor];
      const char *ps[] = {app->current.nav.path, e->basename, NULL};
      fs_join(ps, path, sizeof(path));
      if (id == 'c')
        clip_copy(&app->clipboard, path);
      else
        clip_cut(&app->clipboard, path);
    }
    break;
  case 'z':
    app->config->show_hidden = !app->config->show_hidden;
    app_refresh(app);
    break;
  case 'q':
    app->running = false;
    break;
  case 'j':
  case NCKEY_DOWN:
    app_cursor(app, 1);
    break;
  case 'k':
  case NCKEY_UP:
    app_cursor(app, -1);
    break;
  case 'h':
  case NCKEY_LEFT:
  case NCKEY_BACKSPACE:
    app_parent(app);
    break;
  case 'l':
  case NCKEY_RIGHT:
  case NCKEY_ENTER:
    if (app->current.count > 0) {
      FsEntry *sel = &app->current.entries[app->current.nav.cursor];
      if (!sel->is_dir) {
        const char *parts[] = {app->current.nav.path, sel->basename, NULL};
        fs_join(parts, app->open_file_path, sizeof(app->open_file_path));
        app->open_file_pending = true;
        break;
      }
    }
    app_enter(app);
    break;
  case NCKEY_RESIZE:
    notcurses_refresh(nc, NULL, NULL);
    layout_resize(nc, layout);
    break;
  }
  return 0;
}

int input_handle(struct notcurses *nc, Layout *layout, AppState *app) {
  struct ncinput ni;
  uint32_t id;

  if (app->preview.mode == PREVIEW_CACHING) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += 50000000;
    if (ts.tv_nsec >= 1000000000) {
      ts.tv_sec++;
      ts.tv_nsec -= 1000000000;
    }
    id = notcurses_get(nc, &ts, &ni);
    if (id == 0)
      return 0;
  } else {
    id = notcurses_get(nc, NULL, &ni);
  }

  if (id == (uint32_t)-1) {
    if (errno == EINTR)
      return 0;
    return -1;
  }
  if (ni.evtype == NCTYPE_RELEASE)
    return 0;

  ni.id = normalize_key(ni.id, ni.modifiers);

  if (app->confirm_active)
    return on_confirm(app, &ni);
  if (app->prompt_active)
    return on_command(app, &ni);
  if (app->find_active)
    return on_find(app, &ni);
  return on_normal(nc, layout, app, &ni);
}
