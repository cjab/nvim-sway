#include <dirent.h>
#include <sys/un.h>
#include <unistd.h>

#include "nvim.h"

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

void nvim_move_focus(pid_t nvim_pid, char *direction) {
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
  // Move past program token in file
  strtok_r(server_info, " ", &save_ptr);
  char *server_run_file = strtok_r(NULL, " ", &save_ptr);
  server_run_file[strcspn(server_run_file, "\n")] = 0;
  char cmd_buffer[40];

  printf("runfile: %s", server_run_file);

  int nvimsock = connect_nvim(server_run_file);

  static uint32_t msgid = 0;
  uint32_t type = 0;
  char *method = "nvim_command";
  msgpack_sbuffer sbuf;
  msgpack_packer pk;

  snprintf(cmd_buffer, sizeof(cmd_buffer), "exec VimSwayNav('%s')", direction);

  printf("=> %s", cmd_buffer);

  msgpack_sbuffer_init(&sbuf);
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_array(&pk, 4);
  msgpack_pack_int(&pk, type); // Type
  msgpack_pack_uint32(&pk, msgid); // Msgid
  msgpack_pack_str(&pk, strlen(method));
  msgpack_pack_str_body(&pk, method, strlen(method)); // Method
  msgpack_pack_array(&pk, 1);
  msgpack_pack_str(&pk, strlen(cmd_buffer));
  msgpack_pack_str_body(&pk, cmd_buffer, strlen(cmd_buffer)); // Args

  if(write(nvimsock, sbuf.data, sbuf.size) == -1) {
    fprintf(stderr, "Failed to write\n");
    exit(EXIT_FAILURE);
  }

  char recvd[5];
  if (read(nvimsock, &recvd, 5) == -1) {
    fprintf(stderr, "Failed to read\n");
    exit(EXIT_FAILURE);
  }

  msgid += 1;

  close(nvimsock);
}
