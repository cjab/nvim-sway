#include <msgpack.h>

#include "common.h"

extern char **environ;

typedef struct nvim_session {
  int sock;
  int next_msgid;
} nvim_session_t;

pid_t find_nvim_pid(pid_t parent_pid);

char *nvim_socket_path(pid_t pid);
void nvim_connect(nvim_session_t *session, char *socket_path);
void nvim_disconnect(nvim_session_t *session);

void nvim_command(nvim_session_t *session, char *cmd);
msgpack_object nvim_eval(nvim_session_t *session, char *cmd);

uint64_t nvim_get_focus(nvim_session_t *session);
uint64_t nvim_get_next_focus(nvim_session_t *session, direction_t direction);
void nvim_move_focus(nvim_session_t *session, direction_t direction, int count);

int nvim_receive(nvim_session_t *session, msgpack_object *result);
