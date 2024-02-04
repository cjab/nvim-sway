// #include <argp.h>
//
// int main(int argc, char **argv) {
// }
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void find_nvim_pid(pid_t parent_pid) {
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
    return;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 &&
        strcmp(entry->d_name, "..") != 0) {

      task_pid = atoi(entry->d_name);
      snprintf(buffer, sizeof(buffer), "/proc/%d/task/%d/children", parent_pid, task_pid);

      f = fopen(buffer, "r");
      if (!fgets(buffer, sizeof(buffer), f)) {
        // This task has no children
        fclose(f);
        continue;
      }
      fclose(f);

      char *child_str = strtok_r(buffer, " ", &save_ptr);
      char cmd_buffer[1024];
      while(1) {
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
        char *cmd = cmd_buffer;
        char *arg = cmd_buffer + strlen(cmd_buffer) + 1;
        if (strstr(cmd, "nvim") && strstr(arg, "--embed")) {
          // Found it!
          printf("%d\n", child_pid);
          return;
        }

        find_nvim_pid(child_pid);
        child_str = strtok_r(NULL, " ", &save_ptr);
      }
    }
  }

  closedir(dir);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <parentPID>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  pid_t parent_pid = atoi(argv[1]);

  find_nvim_pid(parent_pid);

  return 0;
}
