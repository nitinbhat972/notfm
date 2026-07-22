#include "renderer.h"
#include "core/app.h"
#include "ui/style.h"
#include <errno.h>
#include <string.h>

static void format_size(char *buf, size_t sz, off_t size) {
  if (size < 1024)
    snprintf(buf, sz, "%ldB", (long)size);
  else if (size < 1024 * 1024)
    snprintf(buf, sz, "%.1fK", size / 1024.0);
  else if (size < 1024LL * 1024 * 1024)
    snprintf(buf, sz, "%.1fM", size / (1024.0 * 1024));
  else
    snprintf(buf, sz, "%.1fG", size / (1024.0 * 1024 * 1024));
}

static void draw_entry(struct ncplane *n, int y, bool sel, bool active,
                       const FsEntry *e, const char *parent_path) {
  unsigned cols;
  ncplane_dim_yx(n, NULL, &cols);
  Style s = style_get(e);
  ncplane_set_channels(n, s.channels);
  uint32_t st = s.styles;
  if (sel) {
    ncplane_set_fg_palindex(n, 3);
    if (active)
      st |= NCSTYLE_BOLD;
    else
      st |= NCSTYLE_ITALIC;
  }
  ncplane_set_styles(n, st);

  ncplane_putstr_yx(n, y, 0, sel ? ">" : " ");
  ncplane_printf_yx(n, y, 1, "%-.*s%s", (int)cols - 3, e->basename,
                    e->is_dir ? "/" : "");
  ncplane_set_styles(n, NCSTYLE_NONE);
  ncplane_set_fg_default(n);
  ncplane_set_bg_default(n);
}

void render_pane(struct ncplane *plane, Pane *p, bool active) {
  ncplane_erase(plane);
  if (p->error) {
    ncplane_putstr_yx(plane, 0, 1,
                      p->error == EACCES ? "Permission Denied"
                                         : strerror(p->error));
    return;
  }

  unsigned rows;
  ncplane_dim_yx(plane, &rows, NULL);
  size_t cur = p->nav.cursor;

  if (cur == SIZE_MAX)
    cur = 0;

  if (cur < p->nav.scroll)
    p->nav.scroll = cur;

  else if (cur >= p->nav.scroll + rows)
    p->nav.scroll = cur - rows + 1;

  for (size_t i = 0; i < rows && (p->nav.scroll + i) < p->count; i++)
    draw_entry(plane, (int)i,
               p->nav.cursor != SIZE_MAX &&
                   (p->nav.scroll + i == p->nav.cursor),
               active, &p->entries[p->nav.scroll + i], p->nav.path);
}

static void render_caching_msg(struct ncplane *plane, const Preview *p) {
  const char *prefix = "caching ";
  const char *fname = strrchr(p->caching_src, '/');

  fname = fname ? fname + 1 : p->caching_src;

  ncplane_putstr_yx(plane, 0, 1, prefix);
  ncplane_set_styles(plane, NCSTYLE_BOLD);
  ncplane_putstr(plane, fname);
  ncplane_set_styles(plane, NCSTYLE_NONE);
}

void render_preview(struct notcurses *nc, struct ncplane *plane, Preview *p,
                    bool active, int display_size, int max_rows) {
  unsigned orig_rows, orig_cols;
  ncplane_dim_yx(plane, &orig_rows, &orig_cols);

  unsigned want;
  if (p->mode == PREVIEW_IMAGE && display_size > 0)
    want = (unsigned)display_size < (unsigned)max_rows ? (unsigned)display_size
                                                       : (unsigned)max_rows;
  else
    want = (unsigned)max_rows;

  if (want != orig_rows) {
    unsigned keep = orig_rows < want ? orig_rows : want;
    ncplane_resize(plane, 0, 0, keep, orig_cols, 0, 0, want, orig_cols);
  }

  ncplane_erase(plane);

  if (p->mode == PREVIEW_CACHING) {
    render_caching_msg(plane, p);
    return;
  }

  if (p->mode == PREVIEW_IMAGE && p->image_path[0]) {
    struct ncvisual *ncv = ncvisual_from_file(p->image_path);
    if (ncv) {
      struct ncvisual_options vopts = {
          .n = plane,
          .scaling = NCSCALE_SCALE_HIRES,
          .blitter = NCBLIT_PIXEL,
      };

      if (!ncvisual_blit(nc, ncv, &vopts)) {
        vopts.scaling = NCSCALE_SCALE;
        vopts.blitter = NCBLIT_DEFAULT;
        ncvisual_blit(nc, ncv, &vopts);
      }
      ncvisual_destroy(ncv);
    }
  } else if (p->mode == PREVIEW_DIR) {
    render_pane(plane, &p->subdir, active);
  } else if (p->mode == PREVIEW_TEXT) {
    unsigned rows;
    ncplane_dim_yx(plane, &rows, NULL);

    for (size_t i = 0; i < p->text.len && (int)i < (int)rows; i++) {
      const char *line = p->text.data[i];
      if (!line)
        continue;

      ncplane_cursor_move_yx(plane, (int)i, 1);
      ncplane_add_ansi(plane, line);
    }
  }
}

void render_status(struct ncplane *plane, AppState *app) {
  ncplane_erase(plane);
  unsigned cols;
  ncplane_dim_yx(plane, NULL, &cols);

  int left_len = 0;
  if (app->current.count > 0) {
    FsEntry *e = &app->current.entries[app->current.nav.cursor];
    ncplane_set_styles(plane, NCSTYLE_BOLD);
    left_len = ncplane_putstr(plane, e->basename);
    ncplane_set_styles(plane, NCSTYLE_NONE);
    if (e->is_dir) {
      ncplane_putstr(plane, "/");
      ++left_len;
    }
  }

  char right_buf[256];
  int r = 0;
  if (app->current.count > 0) {
    FsEntry *e = &app->current.entries[app->current.nav.cursor];
    char sz[16];
    format_size(sz, sizeof(sz), e->size);
    r += snprintf(right_buf + r, sizeof(right_buf) - (size_t)r, "%s", sz);

    r += snprintf(right_buf + r, sizeof(right_buf) - (size_t)r, "  [%zu/%zu]",
                  app->current.nav.cursor + 1, app->current.count);
  }

  unsigned right_x = left_len + 2;
  if (right_x + (unsigned)r <= cols)
    ncplane_putstr_yx(plane, 0, cols - (unsigned)r, right_buf);
}

void render_cmdline(struct ncplane *plane, AppState *app) {
  if (!app->confirm_active && !app->prompt_active && !app->find_active)
    return;

  ncplane_erase(plane);

  if (app->confirm_active) {
    ncplane_set_styles(plane, NCSTYLE_BOLD);
    ncplane_putstr_yx(plane, 0, 0, "Delete? [y/n]");
    ncplane_set_styles(plane, NCSTYLE_NONE);
  } else if (app->prompt_active) {
    ncplane_putstr_yx(plane, 0, 0, ":");
    ncplane_set_fg_default(plane);
    if (app->prompt_cursor > 0)
      ncplane_printf(plane, "%.*s", (int)app->prompt_cursor,
                     app->prompt_buffer);

    char c = app->prompt_buffer[app->prompt_cursor];
    char disp = c ? c : ' ';

    ncplane_set_styles(plane, NCSTYLE_BOLD);
    ncplane_set_fg_palindex(plane, 3);
    ncplane_printf(plane, "%c", disp);
    ncplane_set_styles(plane, NCSTYLE_NONE);
    ncplane_set_fg_default(plane);
    ncplane_putstr(plane,
                   app->prompt_buffer + app->prompt_cursor + (c ? 1 : 0));
  } else if (app->find_active) {
    ncplane_printf_yx(plane, 0, 0, "/%s", app->find_buffer);
  }
}
