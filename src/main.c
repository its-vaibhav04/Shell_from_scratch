#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>

void enable_raw_mode() {
  struct termios raw;
  tcgetattr(STDIN_FILENO, &raw);
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int cmp_strings(const void* a, const void* b) {
  const char* sa = (const char*)a;
  const char* sb = (const char*)b;
  return strcmp(sa, sb);
}


// Tokenizes input into argv array, returns argc
int tokenize(char* input, char* argv[], int max_args)
{
  if (!input)
    return 0;

  int argc = 0;
  char* p = input;
  char* w = input;

  while (*p) {
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '\0')
      break;
    if (argc >= max_args - 1)
      break;

    argv[argc++] = w;

    while (*p && *p != ' ' && *p != '\t') {
      //Single Quotes Handling
      if (*p == '\'') {
        p++;
        while (*p && *p != '\'')
          *w++ = *p++;
        if (*p == '\'')
          p++;
      }

      // Double quotes handling
      else if (*p == '"') {
        p++;
        while (*p && *p != '"') {
          if (*p == '\\') {
            char next = *(p + 1);
            if (next == '"' || next == '\\') {
              p++;
              *w++ = *p++;
            }
            else {
              *w++ = *p++;
            }
          }
          else {
            *w++ = *p++;
          }
        }
        if (*p == '"')
          p++;
      }
      else if (*p == '\\') {
        p++;
        if (*p)
          *w++ = *p++;
      }
      else {
        *w++ = *p++;
      }
    }
    if (*p)
      p++;
    *w++ = '\0';
  }
  argv[argc] = NULL;
  return argc;
}

// Returns full path of executable if found in PATH, else NULL
// Caller must free the returned string
char* find_executable(const char* command)
{
  if (!command || strlen(command) == 0)
    return NULL;

  char* path_env = getenv("PATH");
  if (!path_env)
    return NULL;

  char* path_copy = strdup(path_env);
  if (!path_copy)
    return NULL;

  char* token = strtok(path_copy, ":");
  while (token)
  {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", token, command);

    // Check if file exists and is executable
    if (access(full_path, X_OK) == 0)
    {
      struct stat path_stat;
      if (stat(full_path, &path_stat) == 0 && S_ISREG(path_stat.st_mode))
      {
        free(path_copy);
        return strdup(full_path);
      }
    }
    token = strtok(NULL, ":");
  }

  free(path_copy);
  return NULL;
}

// Executes external commands
void execute_external(char* argv[])
{

  char* exe_path = find_executable(argv[0]);
  if (!exe_path)
  {
    printf("%s: command not found\n", argv[0]);
    return;
  }

  pid_t pid = fork();

  if (pid == 0)
  {
    // child
    execv(exe_path, argv);
    perror("execv"); // only runs if exec fails
    exit(1);
  }
  else if (pid > 0)
  {
    // parent
    wait(NULL);
  }
  else
  {
    perror("fork");
  }
  free(exe_path);
}

// Parses redirection in argv, returns output file if found, else NULL

void parse_redirection(char* argv[], char** out_stdout, char** out_stderr, bool* stdout_append, bool* stderr_append)
{
  *out_stdout = NULL;
  *out_stderr = NULL;
  *stdout_append = false;
  *stderr_append = false;


  for (int i = 0; argv[i]; i++) {
    if ((strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) && argv[i + 1]) {
      *out_stdout = argv[i + 1];
      argv[i] = NULL;
      argv[i + 1] = NULL;
      continue;
    }
    else if (strcmp(argv[i], "2>") == 0 && argv[i + 1]) {
      *out_stderr = argv[i + 1];
      argv[i] = NULL;
      argv[i + 1] = NULL;
      continue;
    }
    else if ((strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "1>>") == 0) && argv[i + 1]) {
      *out_stdout = argv[i + 1];
      argv[i] = NULL;
      argv[i + 1] = NULL;
      *stdout_append = true;
      continue;
    }
    else if (strcmp(argv[i], "2>>") == 0 && argv[i + 1]) {
      *out_stderr = argv[i + 1];
      argv[i] = NULL;
      argv[i + 1] = NULL;
      *stderr_append = true;
      continue;
    }
  }
}

void handle_sigint(int sig) {
  (void)sig;
  write(STDOUT_FILENO, "\n$ ", 3);
}
const char* builtin[] = { "echo", "exit", "type", "pwd", "cd" };

void handle_command(char* buffer) {
  char* argvv[20];
  char input_copy[100];

  strcpy(input_copy, buffer);
  int argc = tokenize(input_copy, argvv, 20);

  if (argc == 0)
    return;

  char* out_stdout, * out_stderr;
  bool stdout_append, stderr_append;
  parse_redirection(argvv, &out_stdout, &out_stderr, &stdout_append, &stderr_append);

  int saved_stdout = -1, saved_stderr = -1;

  if (out_stdout) {
    int flags = O_WRONLY | O_CREAT |
      (stdout_append ? O_APPEND : O_TRUNC);
    saved_stdout = dup(1);
    int fd = open(out_stdout, flags, 0644);
    dup2(fd, 1);
    close(fd);
  }

  if (out_stderr) {
    int flags = O_WRONLY | O_CREAT |
      (stderr_append ? O_APPEND : O_TRUNC);
    saved_stderr = dup(2);
    int fd = open(out_stderr, flags, 0644);
    dup2(fd, 2);
    close(fd);
  }

  // EXIT COMMAND
  if (argc == 1 && strcmp(argvv[0], "exit") == 0)
    exit(0);

  // ECHO COMMAND
  if (strcmp(argvv[0], "echo") == 0)
  {
    for (int i = 1; argvv[i]; i++)
    {
      printf("%s", argvv[i]);
      if (argvv[i + 1])
        printf(" ");
    }
    printf("\n");
  }
  else if (argc >= 2 && strcmp(argvv[0], "type") == 0)
  { // TYPE COMMAND
    const char* cmd = argvv[1];
    int found = 0;
    int builtin_count = sizeof(builtin) / sizeof(builtin[0]);

    for (int i = 0; i < builtin_count; i++)
    { // Checks command for each builtin
      if (strcmp(builtin[i], cmd) == 0)
      {
        found = 1;
        break;
      }
    }
    if (found)
    {
      printf("%s is a shell builtin\n", cmd);
    }
    else
    {
      char* exe_path = find_executable(cmd);
      if (exe_path)
      {
        printf("%s is %s\n", cmd, exe_path);
        free(exe_path);
      }
      else
      {
        printf("%s: not found\n", cmd);
      }
    }
  }
  else if (strcmp(argvv[0], "pwd") == 0)
  {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
      printf("%s\n", cwd);
    }
  }
  else if (strcmp(argvv[0], "cd") == 0)
  {
    const char* path = NULL;

    if (argc == 1)
    {
      path = getenv("HOME");
      if (!path)
      {
        fprintf(stderr, "cd: HOME not set\n");
        goto cleanup;
      }
    }
    else if (argc == 2 && strcmp(argvv[1], "~") == 0)
    {
      path = getenv("HOME");
      if (!path)
      {
        fprintf(stderr, "cd: HOME not set\n");
        goto cleanup;
      }
    }
    else if (argc == 2)
      path = argvv[1];
    else
    {
      fprintf(stderr, "cd: too many arguments\n");
      goto cleanup;
    }

    if (chdir(path) != 0)
      fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
  }
  else
    execute_external(argvv);

cleanup:
  if (out_stdout) {
    dup2(saved_stdout, 1);
    close(saved_stdout);
  }

  if (out_stderr) {
    dup2(saved_stderr, 2);
    close(saved_stderr);
  }
  return;

cleanup_and_exit:
  if (out_stdout) {
    dup2(saved_stdout, 1);
    close(saved_stdout);
  }

  if (out_stderr) {
    dup2(saved_stderr, 2);
    close(saved_stderr);
  }
  return;
}

int is_builtin(const char* cmd) {
  for (int i = 0; builtin[i]; i++) {
    if (strcmp(builtin[i], cmd) == 0)
      return 1;
  }
  return 0;
}

void execute_builtin_in_pipeline(char** argv, int argc, int in_fd, int out_fd) {
  int saved_stdin = -1, saved_stdout = -1;

  if (in_fd != STDIN_FILENO) {
    saved_stdin = dup(STDIN_FILENO);
    dup2(in_fd, STDIN_FILENO);
  }

  if (out_fd != STDOUT_FILENO) {
    saved_stdout = dup(STDOUT_FILENO);
    dup2(out_fd, STDOUT_FILENO);
  }

  if (strcmp(argv[0], "echo") == 0) {
    for (int i = 1; i < argc && argv[i]; i++) {
      printf("%s", argv[i]);
      if (argv[i + 1])
        printf(" ");
    }
    printf("\n");
  }
  else if (strcmp(argv[0], "type") == 0 && argc >= 2) {
    const char* cmd = argv[1];
    int found = 0;
    for (int i = 0; builtin[i]; i++) {
      if (strcmp(builtin[i], cmd) == 0) {
        found = 1;
        break;
      }
    }
    if (found) {
      printf("%s is a shell builtin\n", cmd);
    }
    else {
      char* exe_path = find_executable(cmd);
      if (exe_path) {
        printf("%s is %s\n", cmd, exe_path);
        free(exe_path);
      }
      else {
        printf("%s: not found\n", cmd);
      }
    }
  }
  else if (strcmp(argv[0], "pwd") == 0) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("%s\n", cwd);
    }
  }
  else if (strcmp(argv[0], "cd") == 0) {
    const char* path = argc > 1 ? argv[1] : getenv("HOME");
    if (path && chdir(path) != 0) {
      fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
    }
  }

  fflush(stdout);

  if (saved_stdin != -1) {
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
  }
  if (saved_stdout != -1) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }
}

void execute_pipeline(char* input) {
  char* pipe_pos = strchr(input, '|');
  if (!pipe_pos) return;

  *pipe_pos = '\0';
  char* left_cmd = input;
  char* right_cmd = pipe_pos + 1;

  while (*left_cmd == ' ') left_cmd++;
  while (*right_cmd == ' ') right_cmd++;

  char left_copy[1024], right_copy[1024];
  strncpy(left_copy, left_cmd, sizeof(left_copy) - 1);
  strncpy(right_copy, right_cmd, sizeof(right_copy) - 1);
  left_copy[sizeof(left_copy) - 1] = '\0';
  right_copy[sizeof(right_copy) - 1] = '\0';

  char* left_argv[32], * right_argv[32];
  int left_argc = tokenize(left_copy, left_argv, 32);
  int right_argc = tokenize(right_copy, right_argv, 32);

  if (left_argc == 0 || right_argc == 0) {
    fprintf(stderr, "Invalid pipeline\n");
    return;
  }

  int left_is_builtin = is_builtin(left_argv[0]);
  int right_is_builtin = is_builtin(right_argv[0]);

  int pipefd[2];
  if (pipe(pipefd) < 0) {
    perror("pipe");
    return;
  }

  pid_t left_pid = -1, right_pid = -1;

  if (left_is_builtin) {
    execute_builtin_in_pipeline(left_argv, left_argc, STDIN_FILENO, pipefd[1]);
    close(pipefd[1]);
  }
  else {
    left_pid = fork();
    if (left_pid == 0) {
      close(pipefd[0]);
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[1]);

      char* exec = find_executable(left_argv[0]);
      if (!exec) {
        fprintf(stderr, "%s: command not found\n", left_argv[0]);
        exit(1);
      }
      execv(exec, left_argv);
      perror("execv");
      exit(1);
    }
    close(pipefd[1]);
  }

  if (right_is_builtin) {
    execute_builtin_in_pipeline(right_argv, right_argc, pipefd[0], STDOUT_FILENO);
    close(pipefd[0]);
  }
  else {
    right_pid = fork();
    if (right_pid == 0) {
      close(pipefd[1]);
      dup2(pipefd[0], STDIN_FILENO);
      close(pipefd[0]);

      char* exec = find_executable(right_argv[0]);
      if (!exec) {
        fprintf(stderr, "%s: command not found\n", right_argv[0]);
        exit(1);
      }
      execv(exec, right_argv);
      perror("execv");
      exit(1);
    }
    close(pipefd[0]);
  }

  if (left_pid > 0) waitpid(left_pid, NULL, 0);
  if (right_pid > 0) waitpid(right_pid, NULL, 0);
}

int main(int argc, char* argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL); // remove the buffer of stdout i.e printing directly & not storing

  // REPL - Read Evaluate Print Loop
  // char input[100]; // declaring a char array to store input command of user
  // const char* builtin[] = { "echo", "exit", "type", "pwd", "cd" };
  signal(SIGINT, handle_sigint);
  enable_raw_mode();
  char buffer[1024];
  int len = 0;
  bool last_was_tab = false;
  write(STDOUT_FILENO, "$ ", 2);
  while (1)
  {
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n < 0) {
      if (errno == EINTR) {
        len = 0;
        continue;
      }
      break;
    }
    if (n == 0) {
      if (len == 0) {
        write(STDOUT_FILENO, "\n", 1);
        break;
      }
      continue;
    }
    if (c != '\t') {
      last_was_tab = false;
    }


    if (c == '\n') {
      buffer[len] = '\0';
      write(STDOUT_FILENO, "\n", 1);
      if (len > 0) {
        if (strchr(buffer, '|')) {
          execute_pipeline(buffer);
        }
        else {
          handle_command(buffer);
        }
      }
      len = 0;
      write(STDOUT_FILENO, "$ ", 2);
      continue;
    }

    if (c == '\t') {
      int i = len - 1;
      while (i >= 0 && buffer[i] != ' ')
        i--;

      int start = i + 1;
      int prefix_len = len - start;

      char prefix[256];
      strncpy(prefix, buffer + start, prefix_len);
      prefix[prefix_len] = '\0';

      char all_matches[256][256];
      int total = 0;

      for (int b = 0; builtin[b]; b++) {
        if (strncmp(builtin[b], prefix, prefix_len) == 0) {
          strcpy(all_matches[total++], builtin[b]);
        }
      }

      if (total == 0) {
        char* path_env = getenv("PATH");
        if (path_env) {
          char* path_copy = strdup(path_env);
          char* dir = strtok(path_copy, ":");

          while (dir && total < 256) {
            DIR* dp = opendir(dir);
            if (dp) {
              struct dirent* entry;
              while ((entry = readdir(dp)) && total < 256) {
                if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
                  char full_path[1024];
                  snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

                  if (access(full_path, X_OK) == 0) {
                    struct stat st;
                    if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
                      int is_dup = 0;
                      for (int k = 0; k < total; k++) {
                        if (strcmp(all_matches[k], entry->d_name) == 0) {
                          is_dup = 1;
                          break;
                        }
                      }
                      if (!is_dup) {
                        strcpy(all_matches[total++], entry->d_name);
                      }
                    }
                  }
                }
              }
              closedir(dp);
            }
            dir = strtok(NULL, ":");
          }
          free(path_copy);
        }
      }

      if (total == 0) {
        write(STDOUT_FILENO, "\x07", 1);
        last_was_tab = false;
        continue;
      }

      qsort(all_matches, total, sizeof(all_matches[0]), cmp_strings);

      int lcp_len = strlen(all_matches[0]);
      for (int j = 1; j < total; j++) {
        int k = 0;
        while (k < lcp_len &&
          all_matches[0][k] &&
          all_matches[j][k] &&
          all_matches[0][k] == all_matches[j][k]) {
          k++;
        }
        lcp_len = k;
      }

      if (lcp_len > prefix_len) {
        write(STDOUT_FILENO, "\r\033[K$ ", 6);
        memcpy(buffer + start, all_matches[0], lcp_len);
        len = start + lcp_len;
        buffer[len] = '\0';
        if (total == 1 && lcp_len == strlen(all_matches[0])) {
          buffer[len++] = ' ';
          buffer[len] = '\0';
        }
        write(STDOUT_FILENO, buffer, len);
        last_was_tab = false;
        continue;
      }

      if (total == 1) {
        write(STDOUT_FILENO, "\r\033[K$ ", 6);
        int mlen = strlen(all_matches[0]);
        memcpy(buffer + start, all_matches[0], mlen);
        buffer[start + mlen] = ' ';
        len = start + mlen + 1;
        buffer[len] = '\0';
        write(STDOUT_FILENO, buffer, len);
        last_was_tab = false;
        continue;
      }

      if (!last_was_tab) {
        write(STDOUT_FILENO, "\x07", 1);
        last_was_tab = true;
        continue;
      }

      last_was_tab = false;
      write(STDOUT_FILENO, "\n", 1);
      for (int j = 0; j < total; j++) {
        write(STDOUT_FILENO, all_matches[j], strlen(all_matches[j]));
        if (j < total - 1)
          write(STDOUT_FILENO, "  ", 2);
      }
      write(STDOUT_FILENO, "\n$ ", 3);
      write(STDOUT_FILENO, buffer, len);
      continue;
    }

    if (c == 127) {
      if (len > 0) {
        len--;
        buffer[len] = '\0';

        // Move cursor back, erase char, move back again
        write(STDOUT_FILENO, "\b \b", 3);
      }
      continue;
    }

    buffer[len++] = c;
    write(STDOUT_FILENO, &c, 1);
  }
  return 0;
}
