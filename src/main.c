#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

  if (nvim_pid == 0) {
    sway_move_focus(direction);
    return 0;
  }

  nvim_session_t *nvim = nvim_connect(nvim_pid);

  if (nvim_get_focus(nvim) == nvim_get_next_focus(nvim, direction)) {
    sway_move_focus(direction);
  } else {
    nvim_move_focus(nvim, direction);
  }

  nvim_disconnect(nvim);

  return 0;
}
