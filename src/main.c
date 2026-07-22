#include "core/app.h"
#include "ui/ui.h"
#include "config/config.h"
#include "pane/thumbnail.h"
#include "utils/log.h"
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static volatile sig_atomic_t sig_caught;

static bool is_gui_ext(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return false;
  ++ext;
  static const char *exts[] = {
    "png",  "jpg",   "jpeg", "gif",  "bmp",  "ppm",   "pgm",  "pbm",
    "webp", "tiff",  "tif",  "svg",  "ico",  "avif",  "heic", "heif",
    "mp4",  "mkv",   "avi",  "mov",  "wmv",  "flv",   "webm", "m4v",
    "mpg",  "mpeg",  "3gp",  "ogv",
    "mp3",  "wav",   "ogg",  "flac", "aac",  "wma",   "m4a",  "opus",
    "pdf",  "epub",  "djvu", "ps",   "chm",
    "zip",  "tar",   "gz",   "bz2",  "xz",   "7z",    "rar",  "zst",
    NULL,
  };
  for (int i = 0; exts[i]; i++)
    if (strcasecmp(ext, exts[i]) == 0)
      return true;
  return false;
}

static void handle_sig(int sig) {
  (void)sig;
  sig_caught = 1;
}

static void handle_sigchld(int sig) {
  (void)sig;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

int main(int argc, char **argv) {
  const char *start_dir = argc > 1 ? argv[1] : ".";

  log_open(NULL);

  Config cfg;
  config_load(&cfg);

  thumb_init(&cfg);

  AppState app;
  if (app_init(&app, start_dir, &cfg) != 0) {
    log_write(LOG_ERROR, "Failed to init app for: %s", start_dir);
    fprintf(stderr, "Failed to initialize app state for: %s\n", start_dir);
    thumb_stop();
    return EXIT_FAILURE;
  }

  Ui ui;
  if (ui_init(&ui) != 0) {
    log_write(LOG_ERROR, "Failed to init notcurses UI");
    fprintf(stderr, "Failed to initialize Notcurses UI\n");
    app_free(&app);
    thumb_stop();
    return EXIT_FAILURE;
  }

  struct sigaction sa = {.sa_handler = handle_sig};
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGPIPE, &sa, NULL);
  struct sigaction sa_chld = {.sa_handler = handle_sigchld,
                              .sa_flags = SA_RESTART | SA_NOCLDSTOP};
  sigaction(SIGCHLD, &sa_chld, NULL);

  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDWR, 0);
  posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_RDWR, 0);
  posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_RDWR, 0);

  log_write(LOG_INFO, "notfm started in: %s", start_dir);

  while (app.running && !sig_caught) {
    if (ui_render(&ui, &app) != 0)
      break;

    if (ui_handle_input(&ui, &app) != 0)
      break;

    if (app.open_file_pending) {
      app.open_file_pending = false;
      pid_t pid;
      char *argv[] = {"xdg-open", app.open_file_path, NULL};
      if (is_gui_ext(app.open_file_path)) {
        posix_spawnp(&pid, "xdg-open", &fa, NULL, argv, environ);
      } else if (posix_spawnp(&pid, "xdg-open", NULL, NULL, argv, environ) == 0 &&
                 pid > 0) {
        struct termios saved_tio = ui.tio;
        ui_suspend(&ui);
        sigset_t chld_block, chld_old;
        sigemptyset(&chld_block);
        sigaddset(&chld_block, SIGCHLD);
        sigprocmask(SIG_BLOCK, &chld_block, &chld_old);
        waitpid(pid, NULL, 0);
        sigprocmask(SIG_SETMASK, &chld_old, NULL);
        if (ui_init(&ui) != 0) {
          ui.tio = saved_tio;
          log_write(LOG_ERROR, "Failed to re-init notcurses");
          app.running = false;
        } else {
          ui_render(&ui, &app);
        }
      }
    }
  }

  posix_spawn_file_actions_destroy(&fa);

  log_write(LOG_INFO, "notfm shutting down");
  app.running = false;
  ui_free_no_restore(&ui);
  app_free(&app);
  thumb_stop();
  log_close();
  return EXIT_SUCCESS;
}
