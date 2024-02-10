#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "sway.h"

int connect_sway() {
  char *socket_path = getenv("SWAYSOCK");
  struct sockaddr_un server_addr;
  int sockfd;

  if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  memset(&server_addr, 0, sizeof(struct sockaddr_un));
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

  if (connect(sockfd, (struct sockaddr *)&server_addr,
              sizeof(struct sockaddr_un)) == -1) {
    perror("connect");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  return sockfd;
}

char *get_sway_tree() {
  char *buffer;
  int swaysock = connect_sway();
  struct sway_msg msg = {
      .magic = SWAY_MAGIC_STR, .length = 0, .type = SWAY_GET_TREE};
  if (write(swaysock, &msg, sizeof(struct sway_msg)) == -1) {
    fprintf(stderr, "Failed to write to sway socket\n");
    exit(EXIT_FAILURE);
  }
  if (read(swaysock, &msg, sizeof(struct sway_msg)) == -1) {
    fprintf(stderr, "Failed to read to sway socket\n");
    exit(EXIT_FAILURE);
  }
  buffer = malloc(msg.length);
  if (read(swaysock, buffer, msg.length) == -1) {
    fprintf(stderr, "Failed to read to sway socket\n");
    exit(EXIT_FAILURE);
  }
  close(swaysock);
  return buffer;
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

pid_t find_focused_pid() {
  char *tree = get_sway_tree();
  cJSON *json = cJSON_Parse(tree);

  if (json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    goto end;
  }

  return find_focused_pid_in_tree(json);

end:
  cJSON_Delete(json);
  free(tree);
  return 0;
}
