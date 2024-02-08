// #include <argp.h>
//
// int main(int argc, char **argv) {
// }
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cjson/cJSON.h>
#include <msgpack.h>

#define SWAY_MAGIC_STR "i3-ipc"
#define SWAY_GET_TREE 4

extern char** environ;

struct sway_msg {
  char magic[6];
  int32_t length;
  int32_t type;
} __attribute__((packed));
;

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

pid_t find_nvim_pid(pid_t parent_pid) {
  char buffer[1024];
  FILE *f;
  DIR *dir;
  pid_t child_pid;
  pid_t task_pid;
  char *save_ptr;
  struct dirent *entry;

  snprintf(buffer, sizeof(buffer), "/proc/%d/task", parent_pid);
  dir = opendir(buffer);
  if (dir == NULL) {
    return 0;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 &&
        strcmp(entry->d_name, "..") != 0) {

      task_pid = atoi(entry->d_name);
      snprintf(buffer, sizeof(buffer), "/proc/%d/task/%d/children", parent_pid,
               task_pid);

      f = fopen(buffer, "r");
      if (!fgets(buffer, sizeof(buffer), f)) {
        // This task has no children
        fclose(f);
        continue;
      }
      fclose(f);

      char *child_str = strtok_r(buffer, " ", &save_ptr);
      char cmd_buffer[1024];
      char *cmd;
      char *arg;
      while (1) {
        if (!child_str) {
          break;
        }
        child_pid = atoi(child_str);
        if (!child_pid) {
          break;
        }
        snprintf(cmd_buffer, sizeof(cmd_buffer), "/proc/%d/cmdline", child_pid);
        f = fopen(cmd_buffer, "r");
        if (!fgets(cmd_buffer, sizeof(cmd_buffer), f)) {
          // This task has no children
          fclose(f);
          continue;
        }
        fclose(f);
        cmd = cmd_buffer;
        arg = cmd_buffer + strlen(cmd_buffer) + 1;
        if (strstr(cmd, "nvim") && strstr(arg, "--embed")) {
          // Found it!
          closedir(dir);
          return child_pid;
        }

        pid_t nvim_pid = find_nvim_pid(child_pid);
        if (nvim_pid != 0) {
          closedir(dir);
          return nvim_pid;
        }
        child_str = strtok_r(NULL, " ", &save_ptr);
      }
    }
  }
  return 0;
}

int connect_nvim(char *socket_path) {
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

void nvim_command(int sock, char *cmd) {
  static uint32_t msgid = 0;
  uint32_t type = 0;
  char *args[] = {
    "echo",
    "\"Hello, World\"",
  };

  write(sock, &type, sizeof(uint32_t));
  write(sock, &msgid, sizeof(uint32_t));
  //write(sock, &cmd, strlen(cmd) + 1);
  write(sock, "nvim_command", strlen(cmd) + 1);
  msgid += 1;
}

void move_focus(pid_t nvim_pid, char *direction) {
  char run_file_path[2048];
  snprintf(run_file_path, sizeof(run_file_path), "%s/vim-sway-nav.%d.servername",
           getenv("XDG_RUNTIME_DIR"), nvim_pid);
  FILE *run_file = fopen(run_file_path, "r");
  char server_info[1024];
  if (!fgets(server_info, sizeof(server_info), run_file)) {
    // This task has no children
    fclose(run_file);
    return;
  }


  char *save_ptr;
  char *program = strtok_r(server_info, " ", &save_ptr);
  char *server_run_file = strtok_r(NULL, " ", &save_ptr);
  server_run_file[strcspn(server_run_file, "\n")] = 0;
  char cmd_buffer[32];

  int nvimsock = connect_nvim(server_run_file);

  //if (write(nvimsock, &msg, sizeof(struct sway_msg)) == -1) {
  //  fprintf(stderr, "Failed to write to sway socket\n");
  //  exit(EXIT_FAILURE);
  //}
  //if (read(swaysock, &msg, sizeof(struct sway_msg)) == -1) {
  //  fprintf(stderr, "Failed to read to sway socket\n");
  //  exit(EXIT_FAILURE);
  //}


  close(nvimsock);

  //snprintf(cmd_buffer, sizeof(cmd_buffer), "\"VimSwayNav('%s')\"", direction);
  //char *args[] = {
  //  NULL,
  //  "-c",
  //  program,
  //  "--server",
  //  server_name,
  //  "--remote-expr",
  //  cmd_buffer,
  //  NULL
  //};
  //printf("%s --server %s --remote-expr %s", program, server_name, cmd_buffer);
  //fclose(run_file);
  //if (execve("/etc/profiles/per-user/cjab/bin/zsh", args, environ) == -1) {
  //  perror("failed to exec");
  //  exit(EXIT_FAILURE);
  //}
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <left | right | up | down>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  char *direction = argv[1];

  pid_t focused_pid = find_focused_pid();
  pid_t nvim_pid = find_nvim_pid(focused_pid);

  printf("focused_pid: %d\n", focused_pid);

  if (nvim_pid == 0) {
    return 0;
  }

  printf("nvim_pid: %d\n", nvim_pid);

  move_focus(nvim_pid, direction);

  return 0;
}
