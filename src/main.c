#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <argp.h>

#include "sway.h"
#include "nvim.h"

const char *argp_program_version = "nvim-sway 0.1.1";
const char *argp_program_bug_address = "chad@jablonski.xyz";
static char doc[] =
  "nvim-sway -- Unified focus of nvim splits and sway windows.";
static char args_doc[] = "<left|right|up|down>";

static struct argp_option options[] = {
  { 0 }
};

struct arguments {
  char *direction;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;

  switch(key) {
    case ARGP_KEY_ARG:
      if (state->arg_num == 0) {
        if (strcmp(arg, "left") == 0 || strcmp(arg, "right") == 0 ||
            strcmp(arg, "up") == 0 || strcmp(arg, "down") == 0) {
          arguments->direction = arg;
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

static struct argp argp = {
  options,
  parse_opt,
  args_doc,
  doc
};

int focus(char *direction);

int main(int argc, char *argv[]) {
  struct arguments arguments;

  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  return focus(arguments.direction);
}

int focus(char *direction) {
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
  char *path = nvim_socket_path(nvim_pid);
  nvim_connect(&nvim, path);
  free(path);

  if (nvim_get_focus(&nvim) == nvim_get_next_focus(&nvim, direction)) {
    sway_move_focus(sway, direction);
  } else {
    nvim_move_focus(&nvim, direction);
  }

  nvim_disconnect(&nvim);
  sway_disconnect(sway);
  return 0;
}
