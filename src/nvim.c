#include <dirent.h>
#include <errno.h>
#include <sys/un.h>
#include <unistd.h>

#include "nvim.h"

#define UNPACKED_BUFFER_SIZE 2048
#define MAX_PID_LEN 7

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

char *nvim_socket_path(pid_t pid) {
  char *buffer;
  char *xdg_dir = getenv("XDG_RUNTIME_DIR");
  if (xdg_dir) {
    int len = strlen(xdg_dir) + MAX_PID_LEN + 9; // 9 = Template len + null char
    buffer = malloc(len);
    snprintf(buffer, len, "%s/nvim.%d.0", xdg_dir, pid);
  } else {
    char *tmp_dir = getenv("TMPDIR");
    char *user = getenv("USER");
    int len = strlen(tmp_dir) + strlen(user) + MAX_PID_LEN +
              14; // 14 = Template len + null char
    buffer = malloc(len);
    snprintf(buffer, 2048, "%snvim.%s/nvim.%d.0", tmp_dir, user, pid);
  }
  return buffer;
}

void nvim_connect(nvim_session_t *session, char *socket_path) {
  struct sockaddr_un server_addr;
  struct timeval timeout;
  int sock;

  if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Set receive timeout to 250ms
  timeout.tv_sec = 0;
  timeout.tv_usec = 250000;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
    perror("setsockopt");
    close(sock);
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

  session->sock = sock;
  session->next_msgid = 0;
}

void nvim_disconnect(nvim_session_t *session) { close(session->sock); }

void nvim_command(nvim_session_t *session, char *cmd) {
  uint32_t type = 0;
  char *method = "nvim_command";

  msgpack_sbuffer sbuf;
  msgpack_packer pk;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_array(&pk, 4);
  msgpack_pack_int(&pk, type);                   // Type
  msgpack_pack_uint32(&pk, session->next_msgid); // Msgid
  msgpack_pack_str(&pk, strlen(method));
  msgpack_pack_str_body(&pk, method, strlen(method)); // Method
  msgpack_pack_array(&pk, 1);
  msgpack_pack_str(&pk, strlen(cmd));
  msgpack_pack_str_body(&pk, cmd, strlen(cmd)); // Args

  if (write(session->sock, sbuf.data, sbuf.size) == -1) {
    fprintf(stderr, "Failed to write\n");
    exit(EXIT_FAILURE);
  }

  // We're not interested in the response of the command
  // but we still need to move past it.
  msgpack_object dummy;
  if (nvim_receive(session, &dummy) == -1) {
    // Timeout occurred, just return without error
    return;
  }

  session->next_msgid += 1;
}

msgpack_object nvim_eval(nvim_session_t *session, char *expression) {
  uint32_t type = 0;
  char *method = "nvim_eval";

  msgpack_sbuffer sbuf;
  msgpack_packer pk;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_array(&pk, 4);
  msgpack_pack_int(&pk, type);                   // Type
  msgpack_pack_uint32(&pk, session->next_msgid); // Msgid
  msgpack_pack_str(&pk, strlen(method));
  msgpack_pack_str_body(&pk, method, strlen(method)); // Method
  msgpack_pack_array(&pk, 1);
  msgpack_pack_str(&pk, strlen(expression));
  msgpack_pack_str_body(&pk, expression, strlen(expression)); // Args

  if (write(session->sock, sbuf.data, sbuf.size) == -1) {
    fprintf(stderr, "Failed to write\n");
    exit(EXIT_FAILURE);
  }

  msgpack_object res;
  if (nvim_receive(session, &res) == -1) {
    // Timeout occurred, return a nil object
    msgpack_object nil_obj = {.type = MSGPACK_OBJECT_NIL};
    session->next_msgid += 1;
    return nil_obj;
  }
  session->next_msgid += 1;

  return res;
}

int nvim_receive(nvim_session_t *session, msgpack_object *result) {
  char buffer[UNPACKED_BUFFER_SIZE];

  int len = read(session->sock, &buffer, sizeof(buffer));
  if (len == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // Timeout occurred
      return -1;
    }
    fprintf(stderr, "Failed to read\n");
    exit(EXIT_FAILURE);
  }

  msgpack_unpacked unpacked;
  msgpack_unpacked_init(&unpacked);

  size_t off = 0;
  msgpack_unpack_return ret;
  ret = msgpack_unpack_next(&unpacked, buffer, len, &off);

  if (ret == MSGPACK_UNPACK_PARSE_ERROR) {
    fprintf(stderr, "Failed to parse received nvim message.\n");
    exit(EXIT_FAILURE);
  } else if (ret == MSGPACK_UNPACK_EXTRA_BYTES) {
    fprintf(stderr, "Parsed nvim message but extra bytes existed.\n");
    exit(EXIT_FAILURE);
  } else if (ret == MSGPACK_UNPACK_CONTINUE) {
    fprintf(stderr, "Did not have enough bytes to parse nvim message.\n");
    exit(EXIT_FAILURE);
  } else if (ret == MSGPACK_UNPACK_NOMEM_ERROR) {
    fprintf(stderr, "Ran out of memory while parsing nvim message.\n");
    exit(EXIT_FAILURE);
  }

  // MSGPACK_UNPACK_SUCCESS
  msgpack_object obj = unpacked.data;

  if (obj.type != MSGPACK_OBJECT_ARRAY) {
    fprintf(stderr, "Nvim response was not an array.\n");
    exit(EXIT_FAILURE);
  }

  msgpack_object_array array = obj.via.array;
  if (array.size != 4) {
    fprintf(stderr, "Nvim response array had %d elements (requires 4).\n",
            array.size);
    exit(EXIT_FAILURE);
  }

  if (array.ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
    fprintf(stderr, "First element of the nvim response was not an integer.\n");
    exit(EXIT_FAILURE);
  }
  uint64_t type = array.ptr[0].via.u64;
  if (type != 1) {
    fprintf(stderr, "Nvim response type is incorrect: %ld.\n", type);
    exit(EXIT_FAILURE);
  }

  if (array.ptr[1].type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
    fprintf(stderr,
            "Second element of the nvim response was not an integer.\n");
    exit(EXIT_FAILURE);
  }
  uint64_t msgid = array.ptr[1].via.u64;
  if (msgid != session->next_msgid) {
    fprintf(stderr, "Received nvim response out of order\n");
    exit(EXIT_FAILURE);
  }

  if (array.ptr[2].type != MSGPACK_OBJECT_NIL) {
    fprintf(stderr, "Third element of the nvim contained an error.\n");
    exit(EXIT_FAILURE);
  }

  *result = array.ptr[3];
  msgpack_unpacked_destroy(&unpacked);

  return 0;
}

uint64_t nvim_get_focus(nvim_session_t *session) {
  msgpack_object res = nvim_eval(session, "winnr()");

  if (res.type == MSGPACK_OBJECT_NIL) {
    // Timeout occurred, return 0 to indicate failure
    return 0;
  }
  if (res.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
    fprintf(stderr, "Invalid nvim_get_focus response\n");
    exit(EXIT_FAILURE);
  }
  return res.via.u64;
}

char dir_to_key(direction_t direction) {
  char dir;
  switch (direction) {
  case LEFT:
    dir = 'h';
    break;
  case RIGHT:
    dir = 'l';
    break;
  case UP:
    dir = 'k';
    break;
  case DOWN:
    dir = 'j';
    break;
  default:
    fprintf(stderr, "Invalid direction\n");
    exit(EXIT_FAILURE);
  }
  return dir;
}

uint64_t nvim_get_next_focus(nvim_session_t *session, direction_t direction) {
  char key = dir_to_key(direction);
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "winnr('%c')", key);
  msgpack_object res = nvim_eval(session, buffer);
  if (res.type == MSGPACK_OBJECT_NIL) {
    // Timeout occurred, return 0 to indicate failure
    return 0;
  }
  if (res.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
    fprintf(stderr, "Invalid nvim_get_focus response\n");
    exit(EXIT_FAILURE);
  }

  return res.via.u64;
}

void nvim_move_focus(nvim_session_t *session, direction_t direction,
                     int count) {
  char key = dir_to_key(direction);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "wincmd %d %c", count, key);
  nvim_command(session, buffer);
}
