#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sway.h"
#include "nvim.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <left | right | up | down>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  char *direction = argv[1];

  pid_t focused_pid = find_focused_pid();
  pid_t nvim_pid = find_nvim_pid(focused_pid);

  printf("focused_pid: %d\n", focused_pid);

  if (nvim_pid == 0) {
    return 0;
  }

  printf("nvim_pid: %d\n", nvim_pid);

  move_focus(nvim_pid, direction);

  return 0;
}
