#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "sway.h"

sway_session_t sway_connect(char *socket_path) {
  struct sockaddr_un server_addr;
  int sock;

  if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  memset(&server_addr, 0, sizeof(struct sockaddr_un));
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

  if (connect(sock, (struct sockaddr *)&server_addr,
              sizeof(struct sockaddr_un)) == -1) {
    perror("connect");
    close(sock);
    exit(EXIT_FAILURE);
  }

  return sock;
}

void sway_disconnect(sway_session_t session) { close(session); }

char *sway_get_tree(sway_session_t session) {
  char *buffer;
  struct sway_msg msg = {
      .magic = SWAY_MAGIC_STR, .length = 0, .type = SWAY_GET_TREE};
  if (write((int)session, &msg, sizeof(struct sway_msg)) == -1) {
    fprintf(stderr, "Failed to write to sway socket\n");
    exit(EXIT_FAILURE);
  }
  if (read(session, &msg, sizeof(struct sway_msg)) == -1) {
    fprintf(stderr, "Failed to read to sway socket\n");
    exit(EXIT_FAILURE);
  }
  buffer = malloc(msg.length);
  if (read(session, buffer, msg.length) == -1) {
    fprintf(stderr, "Failed to read to sway socket\n");
    exit(EXIT_FAILURE);
  }
  return buffer;
}

void sway_move_focus(sway_session_t session, direction_t direction) {
  char *buffer;
  char *cmd;
  switch (direction) {
  case LEFT:
    cmd = "focus left";
    break;
  case RIGHT:
    cmd = "focus right";
    break;
  case UP:
    cmd = "focus up";
    break;
  case DOWN:
    cmd = "focus down";
    break;
  default:
    fprintf(stderr, "Invalid direction\n");
    exit(EXIT_FAILURE);
  }
  struct sway_msg msg = {
      .magic = SWAY_MAGIC_STR, .length = strlen(cmd), .type = SWAY_RUN_COMMAND};
  if (write(session, &msg, sizeof(struct sway_msg)) == -1) {
    fprintf(stderr, "Failed to write to sway socket\n");
    exit(EXIT_FAILURE);
  }
  if (write(session, cmd, strlen(cmd)) == -1) {
    fprintf(stderr, "Failed to write to sway socket\n");
    exit(EXIT_FAILURE);
  }
  if (read(session, &msg, sizeof(struct sway_msg)) == -1) {
    fprintf(stderr, "Failed to read to sway socket\n");
    exit(EXIT_FAILURE);
  }
  buffer = malloc(msg.length);
  if (read(session, buffer, msg.length) == -1) {
    fprintf(stderr, "Failed to read to sway socket\n");
    exit(EXIT_FAILURE);
  }
  free(buffer);
}

pid_t sway_get_focused_pid(sway_session_t session) {
  char *tree = sway_get_tree(session);
  cJSON *json = cJSON_Parse(tree);
  pid_t focused_pid = 0;

  if (json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    goto end;
  }

  focused_pid = find_focused_pid_in_tree(json);

end:
  cJSON_Delete(json);
  free(tree);

  return focused_pid;
}

pid_t find_focused_pid_in_tree(cJSON *root) {
  const cJSON *focused = cJSON_GetObjectItemCaseSensitive(root, "focused");
  if (cJSON_IsTrue(focused)) {
    const cJSON *pid = cJSON_GetObjectItemCaseSensitive(root, "pid");
    return pid->valueint;
  }
  const cJSON *nodes = cJSON_GetObjectItemCaseSensitive(root, "nodes");
  cJSON *curr_node;
  cJSON_ArrayForEach(curr_node, nodes) {
    int pid = find_focused_pid_in_tree(curr_node);
    if (pid != 0) {
      return pid;
    }
  };
  return 0;
}
