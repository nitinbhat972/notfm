#include "config.h"
#include "ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int config_ini_handler(void *user, const char *section, const char *name,
                              const char *value) {
  Config *cfg = (Config *)user;

  if (strcmp(name, "show_hidden") == 0)
    cfg->show_hidden = strcmp(value, "true") == 0 || strcmp(value, "1") == 0;
  else if (strcmp(name, "preview_height") == 0 || strcmp(name, "thumbnail_size") == 0)
    cfg->preview_height = atoi(value);
  else if (strcmp(name, "thumbnail_quality") == 0)
    cfg->thumbnail_quality = atoi(value);

  return 1;
}

void config_default(Config *cfg) { *cfg = (Config){0}; }

static void resolve_path(Config *cfg) {
  const char *xdg = getenv("XDG_CONFIG_HOME");
  if (xdg && xdg[0]) {
    snprintf(cfg->config_path, sizeof(cfg->config_path), "%s/notfm/config.ini",
             xdg);
    return;
  }
  const char *home = getenv("HOME");
  if (home)
    snprintf(cfg->config_path, sizeof(cfg->config_path),
             "%s/.config/notfm/config.ini", home);
}

int config_load(Config *cfg) {
  config_default(cfg);
  resolve_path(cfg);
  if (cfg->config_path[0])
    ini_parse(cfg->config_path, config_ini_handler, cfg);
  return 0;
}
