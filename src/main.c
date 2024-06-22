#include <argp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "nvim.h"
#include "sway.h"

const char *argp_program_version = "nvim-sway 0.1.2";
const char *argp_program_bug_address = "chad@jablonski.xyz";
static char doc[] =
    "nvim-sway -- Unified focus of nvim splits and sway windows.";
static char args_doc[] = "<left|right|up|down>";

static struct argp_option options[] = {{0}};

struct arguments {
  enum direction direction;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;

  switch (key) {
  case ARGP_KEY_ARG:
    if (state->arg_num == 0) {
      if (strcmp(arg, "left") == 0) {
        arguments->direction = LEFT;
      } else if (strcmp(arg, "right") == 0) {
        arguments->direction = RIGHT;
      } else if (strcmp(arg, "up") == 0) {
        arguments->direction = UP;
      } else if (strcmp(arg, "down") == 0) {
        arguments->direction = DOWN;
      } else {
        argp_usage(state);
      }
    } else {
      argp_usage(state);
    }
    break;
  case ARGP_KEY_END:
    if (state->arg_num < 1) {
      argp_usage(state);
    }
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

int focus(direction_t direction);
void move_window_focus(sway_session_t sway, direction_t direction);

int main(int argc, char *argv[]) {
  struct arguments arguments;

  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  return focus(arguments.direction);
}

int focus(direction_t direction) {

  char *sway_socket_path = getenv("SWAYSOCK");
  sway_session_t sway = sway_connect(sway_socket_path);

  pid_t focused_pid = sway_get_focused_pid(sway);
  if (focused_pid == 0) {
    // In this case no window was focused.
    // We still want to move the sway focus in the direction specified.
    move_window_focus(sway, direction);
    goto end;
  }

  pid_t nvim_pid = find_nvim_pid(focused_pid);
  if (nvim_pid == 0) {
    // The currently focused window does not contain a neovim instance.
    // Move the sway focus in the direction specified.
    move_window_focus(sway, direction);
    goto end;
  }

  // An instance of neovim was found in the currently focused window.
  nvim_session_t nvim;
  char *path = nvim_socket_path(nvim_pid);
  nvim_connect(&nvim, path);
  free(path);

  if (nvim_get_focus(&nvim) == nvim_get_next_focus(&nvim, direction)) {
    // There are no more nvim splits in this direction.
    // Move the sway window focus.
    move_window_focus(sway, direction);
  } else {
    // There _is_ a split in this direction.
    // Move the nvim split focus.
    nvim_move_focus(&nvim, direction, 1);
  }
  nvim_disconnect(&nvim);

end:
  sway_disconnect(sway);
  return 0;
}

void move_output_focus(sway_session_t sway, direction_t direction) {
  sway_move_output_focus(sway, direction);
}

void move_window_focus(sway_session_t sway, direction_t direction) {
  sway_move_focus(sway, direction);

  pid_t next_focused_pid = sway_get_focused_pid(sway);
  if (next_focused_pid == 0) {
    // After the move no window is focused
    return;
  }

  pid_t next_nvim_pid = find_nvim_pid(next_focused_pid);
  if (next_nvim_pid == 0) {
    // The window we just moved focus to does _not_
    // contain an nvim instance.
    return;
  }

  // The newly focused window contains an nvim instance.
  // This means the split closest to the window from
  // which we're moving should be focused.
  nvim_session_t nvim;
  char *path = nvim_socket_path(next_nvim_pid);
  nvim_connect(&nvim, path);
  free(path);
  switch (direction) {
  case LEFT:
    nvim_move_focus(&nvim, RIGHT, 999);
    break;
  case RIGHT:
    nvim_move_focus(&nvim, LEFT, 999);
    break;
  case UP:
    nvim_move_focus(&nvim, DOWN, 999);
    break;
  case DOWN:
    nvim_move_focus(&nvim, UP, 999);
    break;
  }
  nvim_disconnect(&nvim);
}
