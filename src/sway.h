#include <sys/types.h>
#include <cjson/cJSON.h>

#define SWAY_MAGIC_STR "i3-ipc"
#define SWAY_GET_TREE 4
#define SWAY_RUN_COMMAND 0

struct sway_msg {
  char magic[6];
  int32_t length;
  int32_t type;
} __attribute__((packed));

typedef int sway_session_t;

sway_session_t sway_connect(char *socket_path);
void sway_disconnect(sway_session_t session);

char *sway_get_tree(sway_session_t session);
char *sway_move_focus(sway_session_t session, char *direction);
pid_t sway_get_focused_pid(sway_session_t session);

pid_t find_focused_pid_in_tree(cJSON *root);
