#include "preview.h"
#include "pane/thumbnail.h"
#include "utils/process.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

void preview_init(Preview *p) {
  *p = (Preview){0};
  vec_init(&p->text);
}

static bool is_media_ext(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return false;
  ext++;
  static const char *exts[] = {"png",  "jpg", "jpeg", "gif",  "bmp",
                                "ppm",  "pgm", "pbm",  "tiff", "tif",
                                "webp",
                                "mp4",  "mkv", "avi",  "mov",  "wmv",
                                "flv",  "webm","m4v",  "mpg",  "mpeg",
                                "3gp",  "ogv", NULL};
  for (int i = 0; exts[i]; i++)
    if (strcasecmp(ext, exts[i]) == 0)
      return true;
  return false;
}

void preview_update(Preview *p, const char *parent_path, FsEntry *entry, bool show_hidden) {
  preview_free(p);
  preview_init(p);
  if (!entry)
    return;

  char path[PATH_MAX];
  const char *parts[] = {parent_path, entry->basename, NULL};
  fs_join(parts, path, sizeof(path));

  if (entry->is_dir) {
    p->mode = PREVIEW_DIR;
    pane_update(&p->subdir, path, show_hidden);
    p->subdir.nav.cursor = SIZE_MAX;
    return;
  }

  if (is_media_ext(path)) {
    char thumb[PATH_MAX];
    if (thumb_get(path, thumb, sizeof(thumb)) == 0) {
      p->mode = PREVIEW_IMAGE;
      strncpy(p->image_path, thumb, sizeof(p->image_path) - 1);
    } else {
      p->mode = PREVIEW_CACHING;
      strncpy(p->caching_src, path, sizeof(p->caching_src) - 1);
      if (!thumb_is_queued(path))
        thumb_enqueue(path);
    }
    return;
  }

  p->mode = PREVIEW_TEXT;
  char *bat_argv[] = {"bat", "--color=always", "--style=plain",
                      "--wrap=never", path, NULL};
  if (process_read_stdout(bat_argv, &p->text, 200) != 0 || p->text.len == 0) {
    vec_clear_str(&p->text);
    char *hl_argv[] = {"highlight", "-O", "truecolor",
                       "--out-format=truecolor", path, NULL};
    if (process_read_stdout(hl_argv, &p->text, 200) != 0 || p->text.len == 0) {
      vec_clear_str(&p->text);
      if (fs_read_lines(path, &p->text, 200) == -1) {
        p->error = errno;
        p->mode = PREVIEW_ERROR;
      }
    }
  }
}

void preview_check_cached(Preview *p) {
  if (p->mode != PREVIEW_CACHING)
    return;
  char thumb[PATH_MAX];
  if (thumb_get(p->caching_src, thumb, sizeof(thumb)) == 0) {
    p->mode = PREVIEW_IMAGE;
    strncpy(p->image_path, thumb, sizeof(p->image_path) - 1);
  }
}

void preview_free(Preview *p) {
  pane_reset(&p->subdir);
  vec_clear_str(&p->text);
  p->image_path[0] = '\0';
  p->caching_src[0] = '\0';
  p->mode = PREVIEW_NONE;
  p->error = 0;
}
