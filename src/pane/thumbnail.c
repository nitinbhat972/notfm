#include "thumbnail.h"
#include "config/config.h"
#include "fs/fs.h"
#include "utils/log.h"
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vips/vips.h>

extern char **environ;

#define QUEUE_CAP 4096
#define THUMB_SIZE 400

static int thumb_quality = 85;

typedef struct {
  char src[PATH_MAX];
} ThumbTask;

typedef struct {
  ThumbTask tasks[QUEUE_CAP];
  size_t head, count;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool running;
} ThumbQueue;
static ThumbQueue q;

static pthread_t *workers;
static int num_workers;

static uint64_t fnv1a(const uint8_t *data, size_t len) {
  uint64_t h = 14695981039346656037ULL;
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 1099511628211ULL;
  }

  return h;
}

static int cache_dir(char *buf, size_t sz) {
  const char *xdg = getenv("XDG_CACHE_HOME");
  if (xdg && xdg[0]) {
    snprintf(buf, sz, "%s/notfm/thumbnails", xdg);
    return 0;
  }

  const char *home = getenv("HOME");
  if (!home)
    return -1;

  snprintf(buf, sz, "%s/.cache/notfm/thumbnails", home);

  return 0;
}

static bool is_media_ext(const char *name) {
  const char *ext = strrchr(name, '.');
  if (!ext)
    return false;
  ++ext;
  static const char *exts[] = {"png", "jpg", "jpeg", "gif", "bmp",  "ppm",
                               "pgm", "pbm", "tiff", "tif", "webp", "mp4",
                               "mkv", "avi", "mov",  "wmv", "flv",  "webm",
                               "m4v", "mpg", "mpeg", "3gp", "ogv",  NULL};

  for (int i = 0; exts[i]; i++)
    if (strcasecmp(ext, exts[i]) == 0)
      return true;

  return false;
}

static bool is_video_ext(const char *name) {
  const char *ext = strrchr(name, '.');
  if (!ext)
    return false;
  ++ext;
  static const char *vids[] = {"mp4", "mkv",  "avi", "mov", "wmv",
                               "flv", "webm", "m4v", "mpg", "mpeg",
                               "3gp", "ogv",  NULL};
  for (int i = 0; vids[i]; i++)
    if (strcasecmp(ext, vids[i]) == 0)
      return true;

  return false;
}

static void *worker(void *arg) {
  (void)arg;

  char d[PATH_MAX];
  if (cache_dir(d, sizeof(d)) == 0)
    fs_mkdir_p(d);

  while (true) {
    pthread_mutex_lock(&q.mutex);
    while (q.count == 0 && q.running)
      pthread_cond_wait(&q.cond, &q.mutex);

    if (q.count == 0) {
      pthread_mutex_unlock(&q.mutex);
      break;
    }

    ThumbTask task = q.tasks[q.head];
    q.head = (q.head + 1) % QUEUE_CAP;
    --q.count;
    pthread_mutex_unlock(&q.mutex);

    char thumb[PATH_MAX];
    if (thumb_get(task.src, thumb, sizeof(thumb)) == 0)
      continue;

    char *sep = strrchr(thumb, '/');
    if (sep) {
      *sep = '\0';
      fs_mkdir_p(thumb);
      *sep = '/';
    }

    if (is_video_ext(task.src)) {
      char s_arg[16], q_arg[16];
      snprintf(s_arg, sizeof(s_arg), "%d", THUMB_SIZE);
      snprintf(q_arg, sizeof(q_arg), "%d", 5);
      char *argv[] = {"ffmpegthumbnailer",
                      "-i",
                      task.src,
                      "-o",
                      thumb,
                      "-s",
                      s_arg,
                      "-q",
                      q_arg,
                      NULL};
      posix_spawn_file_actions_t fa;
      posix_spawn_file_actions_init(&fa);
      posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null",
                                       O_WRONLY, 0);
      pid_t pid;
      int ret =
          posix_spawnp(&pid, "ffmpegthumbnailer", &fa, NULL, argv, environ);
      posix_spawn_file_actions_destroy(&fa);
      if (ret == 0) {
        waitpid(pid, NULL, 0);
        struct stat st;
        if (stat(thumb, &st) == 0 && st.st_size > 0) {
          log_write(LOG_INFO, "thumb: generated %s", thumb);
        } else
          log_write(LOG_WARN, "thumb: ffmpegthumbnailer failed for %s",
                    task.src);
      } else {
        log_write(LOG_WARN, "thumb: spawn failed (%d) for %s", ret, task.src);
      }
      continue;
    }

    VipsImage *out = NULL;
    if (vips_thumbnail(task.src, &out, THUMB_SIZE, "height", (gint)THUMB_SIZE,
                       "size", VIPS_SIZE_BOTH, NULL)) {
      log_write(LOG_WARN, "thumb: vips_thumbnail failed for %s: %s", task.src,
                vips_error_buffer());
      vips_error_clear();
      continue;
    }

    if (vips_image_write_to_file(out, thumb, "Q", thumb_quality, NULL)) {
      log_write(LOG_WARN, "thumb: write failed for %s -> %s: %s", task.src,
                thumb, vips_error_buffer());
      vips_error_clear();
    } else {
      log_write(LOG_INFO, "thumb: generated %s", thumb);
    }

    g_object_unref(out);
  }

  vips_thread_shutdown();
  return NULL;
}

void thumb_init(Config *cfg) {
  thumb_quality = cfg->thumbnail_quality ? cfg->thumbnail_quality : 85;
  q = (ThumbQueue){0};
  pthread_mutex_init(&q.mutex, NULL);
  pthread_cond_init(&q.cond, NULL);
  q.running = true;

  setenv("VIPS_MODULE_PATH", "/nonexistent", 1);
  if (vips_init("notfm"))
    log_write(LOG_WARN, "thumb: vips_init failed");
  vips_cache_set_max_mem(0);

  char nd[PATH_MAX];
  if (cache_dir(nd, sizeof(nd)) == 0)
    log_write(LOG_INFO, "thumb: cache dir = %s", nd);

  long n = sysconf(_SC_NPROCESSORS_ONLN);
  num_workers = n > 0 ? (int)n : 2;
  if (num_workers > 16)
    num_workers = 16;

  workers = calloc((size_t)num_workers, sizeof(pthread_t));
  for (int i = 0; i < num_workers; i++)
    pthread_create(&workers[i], NULL, worker, NULL);
}

void thumb_stop(void) {
  pthread_mutex_lock(&q.mutex);
  q.running = false;
  pthread_cond_broadcast(&q.cond);
  pthread_mutex_unlock(&q.mutex);

  for (int i = 0; i < num_workers; i++)
    pthread_join(workers[i], NULL);

  free(workers);
  workers = NULL;
  num_workers = 0;

  pthread_mutex_destroy(&q.mutex);
  pthread_cond_destroy(&q.cond);
  vips_shutdown();
}

void thumb_enqueue(const char *src) {
  pthread_mutex_lock(&q.mutex);
  if (q.running && q.count < QUEUE_CAP) {
    size_t tail = (q.head + q.count) % QUEUE_CAP;
    strncpy(q.tasks[tail].src, src, sizeof(q.tasks[tail].src) - 1);
    q.tasks[tail].src[sizeof(q.tasks[tail].src) - 1] = '\0';
    ++q.count;
    pthread_cond_broadcast(&q.cond);
  }

  pthread_mutex_unlock(&q.mutex);
}

void thumb_enqueue_dir(const char *dir) {
  DIR *d = opendir(dir);
  if (!d)
    return;

  size_t count = 0;
  struct dirent *entry;
  while ((entry = readdir(d)) != NULL) {
    if (entry->d_name[0] == '.')
      continue;

    if (!is_media_ext(entry->d_name))
      continue;

    char path[PATH_MAX];
    const char *parts[] = {dir, entry->d_name, NULL};
    fs_join(parts, path, sizeof(path));
    thumb_enqueue(path);
    ++count;
  }

  closedir(d);
  if (count > 0)
    log_write(LOG_INFO, "thumb: queued %zu images from %s", count, dir);
}

int thumb_get(const char *src, char *out, size_t out_size) {
  char dir[PATH_MAX];
  if (cache_dir(dir, sizeof(dir)) != 0)
    return -1;

  struct stat st;
  if (stat(src, &st) != 0)
    return -1;

  char buf[PATH_MAX + 32];
  int n = snprintf(buf, sizeof(buf), "%s:%ld", src, (long)st.st_mtime);
  uint64_t hash = fnv1a((const uint8_t *)buf, (size_t)n);
  snprintf(out, out_size, "%s/%016lx.jpg", dir, hash);

  return stat(out, &st) == 0 ? 0 : -1;
}

bool thumb_is_queued(const char *src) {
  pthread_mutex_lock(&q.mutex);
  for (size_t i = 0; i < q.count; i++) {
    size_t idx = (q.head + i) % QUEUE_CAP;
    if (strcmp(q.tasks[idx].src, src) == 0) {
      pthread_mutex_unlock(&q.mutex);
      return true;
    }
  }

  pthread_mutex_unlock(&q.mutex);
  return false;
}

bool thumb_is_cached(const char *src) {
  char thumb[PATH_MAX];
  return thumb_get(src, thumb, sizeof(thumb)) == 0;
}
