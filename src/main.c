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
  char *sway_socket_path = getenv("SWAYSOCK");

  sway_session_t sway = sway_connect(sway_socket_path);
  pid_t focused_pid = sway_get_focused_pid(sway);
  pid_t nvim_pid = find_nvim_pid(focused_pid);

  if (nvim_pid == 0) {
    sway_move_focus(sway, direction);
    sway_disconnect(sway);
    return 0;
  }

  nvim_session_t nvim;
  nvim_connect(&nvim, nvim_pid);
  if (nvim_get_focus(&nvim) == nvim_get_next_focus(&nvim, direction)) {
    sway_move_focus(sway, direction);
  } else {
    nvim_move_focus(&nvim, direction);
  }

  nvim_disconnect(&nvim);
  sway_disconnect(sway);

  return 0;
}
