#include "style.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *pattern;
  Style style;
} ColorRule;

static ColorRule *rules = NULL;
static size_t rules_count = 0;
static Style def_style, dir_style, link_style, exec_style;

static void parse_ansi_to_style(const char *code, Style *s) {
  ncchannels_set_fg_default(&s->channels);
  ncchannels_set_bg_default(&s->channels);
  s->styles = NCSTYLE_NONE;

  char *copy = strdup(code), *token = strtok(copy, ";");
  while (token) {
    int val = atoi(token);
    if (val == 0) {
      s->styles = NCSTYLE_NONE;
      ncchannels_set_fg_default(&s->channels);
      ncchannels_set_bg_default(&s->channels);
    } else if (val == 1)
      s->styles |= NCSTYLE_BOLD;
    else if (val == 4)
      s->styles |= NCSTYLE_UNDERLINE;
    else if (val >= 30 && val <= 37)
      ncchannels_set_fg_palindex(&s->channels, val - 30);
    else if (val >= 40 && val <= 47)
      ncchannels_set_bg_palindex(&s->channels, val - 40);
    else if (val >= 90 && val <= 97)
      ncchannels_set_fg_palindex(&s->channels, val - 90 + 8);
    else if (val >= 100 && val <= 107)
      ncchannels_set_bg_palindex(&s->channels, val - 100 + 8);
    else if (val == 38 || val == 48) {
      bool bg = (val == 48);
      token = strtok(NULL, ";");
      if (token) {
        int type = atoi(token);
        if (type == 5) {
          token = strtok(NULL, ";");
          if (token) {
            if (bg)
              ncchannels_set_bg_palindex(&s->channels, atoi(token));
            else
              ncchannels_set_fg_palindex(&s->channels, atoi(token));
          }
        } else if (type == 2) {
          int r = 0, g = 0, b = 0;
          token = strtok(NULL, ";");
          if (token)
            r = atoi(token);
          token = strtok(NULL, ";");
          if (token)
            g = atoi(token);
          token = strtok(NULL, ";");
          if (token)
            b = atoi(token);
          if (bg)
            ncchannels_set_bg_rgb8(&s->channels, r, g, b);
          else
            ncchannels_set_fg_rgb8(&s->channels, r, g, b);
        }
      }
    }
    token = strtok(NULL, ";");
  }
  free(copy);
}

void style_init(void) {
  ncchannels_set_fg_default(&def_style.channels);
  ncchannels_set_bg_default(&def_style.channels);
  def_style.styles = NCSTYLE_NONE;

  dir_style = def_style;
  ncchannels_set_fg_palindex(&dir_style.channels, 4);
  dir_style.styles = NCSTYLE_BOLD;
  link_style = def_style;
  ncchannels_set_fg_palindex(&link_style.channels, 6);
  exec_style = def_style;
  ncchannels_set_fg_palindex(&exec_style.channels, 2);

  const char *ls = getenv("LS_COLORS");
  if (!ls)
    return;

  char *copy = strdup(ls), *entry = strtok(copy, ":");
  while (entry) {
    char *eq = strchr(entry, '=');
    if (eq) {
      *eq = '\0';
      char *key = entry, *val = eq + 1;
      if (strcmp(key, "di") == 0)
        parse_ansi_to_style(val, &dir_style);
      else if (strcmp(key, "ln") == 0)
        parse_ansi_to_style(val, &link_style);
      else if (strcmp(key, "ex") == 0)
        parse_ansi_to_style(val, &exec_style);
      else if (strcmp(key, "fi") == 0)
        parse_ansi_to_style(val, &def_style);
      else if (key[0] == '*') {
        rules = realloc(rules, sizeof(ColorRule) * (rules_count + 1));
        rules[rules_count].pattern = strdup(key + 1);
        parse_ansi_to_style(val, &rules[rules_count].style);
        rules_count++;
      }
    }
    entry = strtok(NULL, ":");
  }
  free(copy);
}

Style style_get(const FsEntry *e) {
  if (e->is_dir)
    return dir_style;
  if (e->is_link)
    return link_style;
  if (e->is_exec)
    return exec_style;
  const char *ext = strrchr(e->basename, '.');
  if (ext) {
    for (size_t i = 0; i < rules_count; i++)
      if (strcasecmp(ext, rules[i].pattern) == 0)
        return rules[i].style;
  }
  return def_style;
}

int ncplane_add_ansi(struct ncplane *n, const char *str) {
  const char *p = str;
  int written = 0;
  while (*p) {
    if (*p == '\x1b' && *(p + 1) == '[') {
      p += 2;
      char *end;
      while (1) {
        long val = strtol(p, &end, 10);
        if (p == end)
          val = 0;
        if (val == 0) {
          ncplane_set_styles(n, NCSTYLE_NONE);
          ncplane_set_fg_default(n);
          ncplane_set_bg_default(n);
        } else if (val == 1)
          ncplane_on_styles(n, NCSTYLE_BOLD);
        else if (val == 3)
          ncplane_on_styles(n, NCSTYLE_ITALIC);
        else if (val == 4)
          ncplane_on_styles(n, NCSTYLE_UNDERLINE);
        else if (val >= 30 && val <= 37)
          ncplane_set_fg_palindex(n, val - 30);
        else if (val >= 40 && val <= 47)
          ncplane_set_bg_palindex(n, val - 40);
        else if (val >= 90 && val <= 97)
          ncplane_set_fg_palindex(n, val - 90 + 8);
        else if (val >= 100 && val <= 107)
          ncplane_set_bg_palindex(n, val - 100 + 8);
        else if (val == 38 || val == 48) {
          bool bg = (val == 48);
          if (*end == ';') {
            p = end + 1;
            long type = strtol(p, &end, 10);
            if (type == 5 && *end == ';') {
              p = end + 1;
              long c = strtol(p, &end, 10);
              if (bg)
                ncplane_set_bg_palindex(n, c);
              else
                ncplane_set_fg_palindex(n, c);
            } else if (type == 2 && *end == ';') {
              p = end + 1;
              long r = strtol(p, &end, 10);
              if (*end == ';') {
                p = end + 1;
                long g = strtol(p, &end, 10);
                if (*end == ';') {
                  p = end + 1;
                  long b = strtol(p, &end, 10);
                  if (bg)
                    ncplane_set_bg_rgb8(n, r, g, b);
                  else
                    ncplane_set_fg_rgb8(n, r, g, b);
                }
              }
            }
          }
        }
        if (*end == ';')
          p = end + 1;
        else {
          p = (*end == 'm') ? end + 1 : end;
          break;
        }
      }
    } else {
      size_t wcs;
      int cols = ncplane_putegc(n, p, &wcs);
      if (cols < 0)
        return written;
      p += wcs;
      written += cols;
    }
  }
  return written;
}
