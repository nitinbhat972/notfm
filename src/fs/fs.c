#include "fs.h"
#include "utils/vec.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int compare_entries(const void *a, const void *b) {
  const FsEntry *ea = a;
  const FsEntry *eb = b;

  bool a_hidden = ea->basename[0] == '.';
  bool b_hidden = eb->basename[0] == '.';

  int a_rank = (!ea->is_dir << 1) | !a_hidden;
  int b_rank = (!eb->is_dir << 1) | !b_hidden;

  if (a_rank != b_rank)
    return a_rank - b_rank;
  return strcasecmp(ea->basename, eb->basename);
}

int fs_list(const char *path, FsEntry **entries, size_t *count,
            bool show_hidden) {
  DIR *dirp = opendir(path);
  if (dirp == NULL)
    return -1;

  Vec(FsEntry) list;
  vec_init(&list);

  struct dirent *entry;
  while ((entry = readdir(dirp)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    if (!show_hidden && entry->d_name[0] == '.')
      continue;

    char full_path[PATH_MAX];
    const char *parts[] = {path, entry->d_name, NULL};
    fs_join(parts, full_path, sizeof(full_path));

    struct stat st;
    if (lstat(full_path, &st) == -1)
      continue;

    FsEntry fs_entry;
    strncpy(fs_entry.basename, entry->d_name, sizeof(fs_entry.basename));
    fs_entry.basename[sizeof(fs_entry.basename) - 1] = '\0';

    fs_entry.is_link = S_ISLNK(st.st_mode);
    if (fs_entry.is_link) {
      struct stat st_target;
      if (stat(full_path, &st_target) == 0) {
        fs_entry.is_dir = S_ISDIR(st_target.st_mode);
        fs_entry.is_exec = !fs_entry.is_dir && (st_target.st_mode & S_IXUSR);
        fs_entry.size = st_target.st_size;
      } else {
        fs_entry.is_dir = false;
        fs_entry.is_exec = false;
        fs_entry.size = st.st_size;
      }
    } else {
      fs_entry.is_dir = S_ISDIR(st.st_mode);
      fs_entry.is_exec = !fs_entry.is_dir && (st.st_mode & S_IXUSR);
      fs_entry.size = st.st_size;
    }

    vec_push(&list, fs_entry);
  }

  closedir(dirp);
  qsort(list.data, list.len, sizeof(FsEntry), compare_entries);

  *entries = list.data;
  *count = list.len;
  return 0;
}

void fs_free(FsEntry *entries) { free(entries); }

int fs_join(const char *path_arr[], char *out, size_t size) {
  out[0] = '\0';
  for (int i = 0; path_arr[i] != NULL; i++) {
    size_t len = strlen(out);
    if (i > 0 && len > 0 && out[len - 1] != '/') {
      strncat(out, "/", size - len - 1);
    }
    strncat(out, path_arr[i], size - strlen(out) - 1);
  }
  return 0;
}

int fs_parent(const char *path, char *out, size_t size) {
  if (strcmp(path, "/") == 0) {
    strncpy(out, "/", size);
    return 0;
  }
  strncpy(out, path, size);
  char *last_slash = strrchr(out, '/');
  if (last_slash == out) {
    *(last_slash + 1) = '\0';
  } else if (last_slash) {
    *last_slash = '\0';
  }
  return 0;
}

int fs_get_abs(const char *path, char *out, size_t size) {
  if (realpath(path, out) == NULL)
    return -1;
  return 0;
}

int fs_read_lines(const char *path, StrVec *out, size_t max_lines) {
  FILE *f = fopen(path, "r");
  if (!f)
    return -1;

  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  size_t count = 0;

  while (count < max_lines && (read = getline(&line, &len, f)) != -1) {
    if (memchr(line, '\0', read)) {
      vec_clear_str(out);
      vec_push(out, strdup("BINARY FILE"));
      break;
    }
    char *dup = strndup(line, read);
    if (dup[read - 1] == '\n')
      dup[read - 1] = '\0';
    vec_push(out, dup);
    count++;
  }

  free(line);
  fclose(f);
  return 0;
}

int fs_mkdir(const char *path) { return mkdir(path, 0755) == 0 ? 0 : -1; }

int fs_mkdir_p(const char *path) {
  char tmp[PATH_MAX];
  strncpy(tmp, path, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  for (char *p = tmp + 1; *p; p++) {
    if (*p != '/')
      continue;
    *p = '\0';
    mkdir(tmp, 0755);
    *p = '/';
  }
  return mkdir(tmp, 0755) == 0 ? 0 : -1;
}

int fs_remove(const char *path) {
  if (path == NULL)
    return -1;

  struct stat st;
  if (lstat(path, &st) == -1)
    return -1;

  if (S_ISDIR(st.st_mode)) {
    DIR *d = opendir(path);
    if (!d)
      return -1;
    struct dirent *de;
    while ((de = readdir(d))) {
      if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
        continue;
      char sub[PATH_MAX];
      const char *parts[] = {path, de->d_name, NULL};
      fs_join(parts, sub, sizeof(sub));
      fs_remove(sub);
    }
    closedir(d);
  }

  return remove(path) == 0 ? 0 : -1;
}

int fs_rename(const char *old_path, const char *new_path) {
  if (old_path == NULL || new_path == NULL)
    return -1;

  return rename(old_path, new_path) == 0 ? 0 : -1;
}

int fs_copy(const char *src, const char *dst) {
  int in_fd = open(src, O_RDONLY);
  if (in_fd == -1)
    return -1;

  struct stat st;
  if (fstat(in_fd, &st) == -1) {
    close(in_fd);
    return -1;
  }

  int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
  if (out_fd == -1) {
    close(in_fd);
    return -1;
  }

  loff_t offset = 0;
  while (offset < st.st_size) {
    ssize_t ret = syscall(SYS_copy_file_range, in_fd, &offset, out_fd, NULL,
                                  st.st_size - offset, 0);
    if (ret == -1) {
      close(in_fd);
      close(out_fd);
      return -1;
    }
  }

  close(in_fd);
  close(out_fd);
  return 0;
}

int fs_create_file(const char *path) {
  if (path == NULL)
    return -1;
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd == -1)
    return -1;
  close(fd);
  return 0;
}
