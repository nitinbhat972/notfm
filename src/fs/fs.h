#pragma once

#include "utils/vec.h"
#include <limits.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct {
  char basename[NAME_MAX];
  bool is_dir;
  bool is_link;
  bool is_exec;
  off_t size;
} FsEntry;

int fs_list(const char *path, FsEntry **entries, size_t *count, bool show_hidden);
void fs_free(FsEntry *entries);

int fs_join(const char *path_arr[], char *out, size_t size);
int fs_parent(const char *path, char *out, size_t size);
int fs_get_abs(const char *path, char *out, size_t size);

int fs_read_lines(const char *path, StrVec *out, size_t max_lines);

int fs_mkdir(const char *path);
int fs_mkdir_p(const char *path);
int fs_remove(const char *path);
int fs_rename(const char *old_path, const char *new_path);
int fs_copy(const char *src, const char *dst);
int fs_create_file(const char *path);
