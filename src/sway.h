#include <sys/types.h>
#include <cjson/cJSON.h>

#define SWAY_MAGIC_STR "i3-ipc"
#define SWAY_GET_TREE 4

struct sway_msg {
  char magic[6];
  int32_t length;
  int32_t type;
} __attribute__((packed));

int connect_sway();
char *get_sway_tree();
pid_t find_focused_pid_in_tree(cJSON *root);
pid_t find_focused_pid();
