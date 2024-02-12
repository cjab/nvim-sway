#include <msgpack.h>

extern char** environ;

pid_t find_nvim_pid(pid_t parent_pid);
int connect_nvim(char *socket_path);
void nvim_command(int sock);
void nvim_move_focus(pid_t nvim_pid, char *direction);
