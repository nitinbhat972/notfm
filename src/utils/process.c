#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils/vec.h"

extern char **environ;

int process_read_stdout(char *const argv[], StrVec *out, size_t max_lines) {
  int pipefd[2];
  if (pipe(pipefd) == -1)
    return -1;

  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, pipefd[1], STDOUT_FILENO);
  posix_spawn_file_actions_addclose(&fa, pipefd[0]);

  pid_t pid;
  int ret = posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ);
  posix_spawn_file_actions_destroy(&fa);
  close(pipefd[1]);

  if (ret != 0) {
    close(pipefd[0]);
    return -1;
  }

  FILE *fp = fdopen(pipefd[0], "r");
  if (!fp) {
    close(pipefd[0]);
    return -1;
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t nread;
  size_t count = 0;
  while (count < max_lines && (nread = getline(&line, &len, fp)) != -1) {
    char *dup = strndup(line, nread);
    if (nread > 0 && dup[nread - 1] == '\n')
      dup[nread - 1] = '\0';
    vec_push(out, dup);
    count++;
  }
  free(line);
  fclose(fp);

  int status;
  waitpid(pid, &status, 0);
  return 0;
}
