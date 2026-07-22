#pragma once
#include <limits.h>
#include <stdbool.h>

typedef struct {
  bool show_hidden;
  int preview_height;
  int thumbnail_quality;
  char config_path[PATH_MAX];
} Config;

int  config_load(Config *cfg);
void config_default(Config *cfg);
