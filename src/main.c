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
const char* builtin[] = { "echo", "exit", "type", "pwd", "cd", "history", "mkdir", "rmdir", "rm", "touch", "cp", "mv", NULL };
char history_commands[50][1024];
int history_count = 0;
int last_appended_index = 0;

void load_history_from_file(const char* filepath) {
  if (!filepath) return;

  FILE* fp = fopen(filepath, "r");
  if (!fp) return;

  char line[1024];
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = '\0';
    if (strlen(line) == 0) continue;

    if (history_count < 50) {
      strncpy(history_commands[history_count], line, sizeof(history_commands[0]) - 1);
      history_commands[history_count][sizeof(history_commands[0]) - 1] = '\0';
      history_count++;
    }
    else {
      for (int i = 0; i < 49; i++) {
        strcpy(history_commands[i], history_commands[i + 1]);
      }
      strncpy(history_commands[49], line, sizeof(history_commands[0]) - 1);
      history_commands[49][sizeof(history_commands[0]) - 1] = '\0';
    }
  }

  fclose(fp);
  last_appended_index = history_count;
}

void save_history_on_exit(const char* filepath) {
  if (!filepath) return;

  FILE* fp_check = fopen(filepath, "r");
  bool file_exists = (fp_check != NULL);
  if (fp_check) fclose(fp_check);

  FILE* fp;
  if (file_exists) {
    fp = fopen(filepath, "a");
  }
  else {
    fp = fopen(filepath, "w");
  }

  if (!fp) return;

  int start_index = file_exists ? last_appended_index : 0;
  for (int i = start_index; i < history_count; i++) {
    fprintf(fp, "%s\n", history_commands[i]);
  }

  fclose(fp);
}

int mkdir_recursive(const char* path, mode_t mode) {
  char tmp[1024];
  char* p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        return -1;
      }
      *p = '/';
    }
  }
  if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

int rmdir_recursive(const char* path) {
  DIR* d = opendir(path);
  size_t path_len = strlen(path);
  int r = -1;

  if (d) {
    struct dirent* p;
    r = 0;

    while (!r && (p = readdir(d))) {
      int r2 = -1;
      char* buf;
      size_t len;

      if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
        continue;

      len = path_len + strlen(p->d_name) + 2;
      buf = malloc(len);

      if (buf) {
        struct stat statbuf;
        snprintf(buf, len, "%s/%s", path, p->d_name);

        if (!stat(buf, &statbuf)) {
          if (S_ISDIR(statbuf.st_mode))
            r2 = rmdir_recursive(buf);
          else
            r2 = unlink(buf);
        }
        free(buf);
      }
      r = r2;
    }
    closedir(d);
  }

  if (!r)
    r = rmdir(path);

  return r;
}

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
  if (argc == 1 && strcmp(argvv[0], "exit") == 0) {
    char* histfile = getenv("HISTFILE");
    if (histfile) {
      save_history_on_exit(histfile);
    }
    exit(0);
  }

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

    for (int i = 0;builtin[i] != NULL; i++)
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
  else if (strcmp(argvv[0], "mkdir") == 0) {
    if (argc < 2) {
      fprintf(stderr, "mkdir: missing operand\n");
      goto cleanup;
    }

    bool parents = false;
    int start_idx = 1;

    if (strcmp(argvv[1], "-p") == 0) {
      parents = true;
      start_idx = 2;
      if (argc < 3) {
        fprintf(stderr, "mkdir: missing operand\n");
        goto cleanup;
      }
    }

    for (int i = start_idx; argvv[i]; i++) {
      int result;
      if (parents) {
        result = mkdir_recursive(argvv[i], 0755);
      }
      else {
        result = mkdir(argvv[i], 0755);
      }

      if (result != 0) {
        fprintf(stderr, "mkdir: cannot create directory '%s': %s\n",
          argvv[i], strerror(errno));
      }
    }
  }
  else if (strcmp(argvv[0], "rmdir") == 0) {
    if (argc < 2) {
      fprintf(stderr, "rmdir: missing operand\n");
      goto cleanup;
    }

    for (int i = 1; argvv[i]; i++) {
      if (rmdir(argvv[i]) != 0) {
        fprintf(stderr, "rmdir: failed to remove '%s': %s\n",
          argvv[i], strerror(errno));
      }
    }
  }
  else if (strcmp(argvv[0], "rm") == 0) {
    if (argc < 2) {
      fprintf(stderr, "rm: missing operand\n");
      goto cleanup;
    }

    bool recursive = false;
    bool force = false;
    int start_idx = 1;

    // Parse flags
    for (int i = 1; argvv[i]; i++) {
      if (argvv[i][0] == '-') {
        for (int j = 1; argvv[i][j]; j++) {
          if (argvv[i][j] == 'r' || argvv[i][j] == 'R') {
            recursive = true;
          }
          else if (argvv[i][j] == 'f') {
            force = true;
          }
        }
        start_idx = i + 1;
      }
      else {
        break;
      }
    }

    if (!argvv[start_idx]) {
      fprintf(stderr, "rm: missing operand\n");
      goto cleanup;
    }

    for (int i = start_idx; argvv[i]; i++) {
      struct stat st;
      if (stat(argvv[i], &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
          if (recursive) {
            if (rmdir_recursive(argvv[i]) != 0 && !force) {
              fprintf(stderr, "rm: cannot remove '%s': %s\n",
                argvv[i], strerror(errno));
            }
          }
          else {
            fprintf(stderr, "rm: cannot remove '%s': Is a directory\n", argvv[i]);
          }
        }
        else {
          if (unlink(argvv[i]) != 0 && !force) {
            fprintf(stderr, "rm: cannot remove '%s': %s\n",
              argvv[i], strerror(errno));
          }
        }
      }
      else if (!force) {
        fprintf(stderr, "rm: cannot remove '%s': %s\n",
          argvv[i], strerror(errno));
      }
    }
  }

  else if (strcmp(argvv[0], "touch") == 0) {
    if (argc < 2) {
      fprintf(stderr, "touch: missing operand\n");
      goto cleanup;
    }

    for (int i = 1; argvv[i]; i++) {
      int fd = open(argvv[i], O_WRONLY | O_CREAT, 0644);
      if (fd < 0) {
        fprintf(stderr, "touch: cannot touch '%s': %s\n",
          argvv[i], strerror(errno));
      }
      else {
        close(fd);
      }
    }
  }

  else if (strcmp(argvv[0], "cp") == 0) {
    if (argc < 3) {
      fprintf(stderr, "cp: missing operand\n");
      goto cleanup;
    }

    const char* src = argvv[1];
    const char* dst = argvv[2];

    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
      fprintf(stderr, "cp: cannot open '%s': %s\n", src, strerror(errno));
      goto cleanup;
    }

    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
      fprintf(stderr, "cp: cannot create '%s': %s\n", dst, strerror(errno));
      close(src_fd);
      goto cleanup;
    }

    char buf[4096];
    ssize_t n;
    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
      if (write(dst_fd, buf, n) != n) {
        fprintf(stderr, "cp: write error: %s\n", strerror(errno));
        break;
      }
    }

    close(src_fd);
    close(dst_fd);
  }

  else if (strcmp(argvv[0], "mv") == 0) {
    if (argc < 3) {
      fprintf(stderr, "mv: missing operand\n");
      goto cleanup;
    }

    const char* src = argvv[1];
    const char* dst = argvv[2];

    if (rename(src, dst) != 0) {
      fprintf(stderr, "mv: cannot move '%s' to '%s': %s\n",
        src, dst, strerror(errno));
    }
  }
  else if (strcmp(argvv[0], "history") == 0) {
    if (argc >= 3 && strcmp(argvv[1], "-r") == 0) {
      const char* filepath = argvv[2];
      FILE* fp = fopen(filepath, "r");

      if (!fp) {
        fprintf(stderr, "history: %s: %s\n", filepath, strerror(errno));
        goto cleanup;
      }

      char line[1024];
      while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        if (history_count < 50) {
          strncpy(history_commands[history_count], line, sizeof(history_commands[0]) - 1);
          history_commands[history_count][sizeof(history_commands[0]) - 1] = '\0';
          history_count++;
        }
        else {
          for (int i = 0; i < 49; i++) {
            strcpy(history_commands[i], history_commands[i + 1]);
          }
          strncpy(history_commands[49], line, sizeof(history_commands[0]) - 1);
          history_commands[49][sizeof(history_commands[0]) - 1] = '\0';
        }
      }

      fclose(fp);
    }
    else if (argc >= 3 && strcmp(argvv[1], "-w") == 0) {
      const char* filepath = argvv[2];
      FILE* fp = fopen(filepath, "w");

      if (!fp) {
        fprintf(stderr, "history: %s: %s\n", filepath, strerror(errno));
        goto cleanup;
      }

      for (int i = 0; i < history_count; i++) {
        fprintf(fp, "%s\n", history_commands[i]);
      }

      fclose(fp);
    }
    else if (argc >= 3 && strcmp(argvv[1], "-a") == 0) {
      const char* filepath = argvv[2];
      FILE* fp = fopen(filepath, "a");

      if (!fp) {
        fprintf(stderr, "history: %s: %s\n", filepath, strerror(errno));
        goto cleanup;
      }

      for (int i = last_appended_index; i < history_count; i++) {
        fprintf(fp, "%s\n", history_commands[i]);
      }

      fclose(fp);
      last_appended_index = history_count;
    }
    else {
      int n = history_count;

      if (argc == 2) {
        n = atoi(argvv[1]);
        if (n < 0) n = 0;
      }

      int start = history_count - n;
      if (start < 0) start = 0;

      for (int i = start; i < history_count; i++) {
        printf("%5d  %s\n", i + 1, history_commands[i]);
      }
    }
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
  else if (strcmp(argv[0], "history") == 0) {
    if (argc >= 3 && strcmp(argv[1], "-r") == 0) {
      const char* filepath = argv[2];
      FILE* fp = fopen(filepath, "r");

      if (!fp) {
        fprintf(stderr, "history: %s: %s\n", filepath, strerror(errno));
        return;
      }

      char line[1024];
      while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;
        if (history_count < 50) {
          strncpy(history_commands[history_count], line, sizeof(history_commands[0]) - 1);
          history_commands[history_count][sizeof(history_commands[0]) - 1] = '\0';
          history_count++;
        }
        else {
          for (int i = 0; i < 49; i++) {
            strcpy(history_commands[i], history_commands[i + 1]);
          }
          strncpy(history_commands[49], line, sizeof(history_commands[0]) - 1);
          history_commands[49][sizeof(history_commands[0]) - 1] = '\0';
        }
      }

      fclose(fp);
    }
    else if (argc >= 3 && strcmp(argv[1], "-w") == 0) {
      const char* filepath = argv[2];
      FILE* fp = fopen(filepath, "w");

      if (!fp) {
        fprintf(stderr, "history: %s: %s\n", filepath, strerror(errno));
        return;
      }

      for (int i = 0; i < history_count; i++) {
        fprintf(fp, "%s\n", history_commands[i]);
      }

      fclose(fp);
    }
    else if (argc >= 3 && strcmp(argv[1], "-a") == 0) {
      const char* filepath = argv[2];
      FILE* fp = fopen(filepath, "a");

      if (!fp) {
        fprintf(stderr, "history: %s: %s\n", filepath, strerror(errno));
        return;
      }

      for (int i = last_appended_index; i < history_count; i++) {
        fprintf(fp, "%s\n", history_commands[i]);
      }

      fclose(fp);
      last_appended_index = history_count;
    }
    else {
      int n = history_count;

      if (argc == 2) {
        n = atoi(argv[1]);
        if (n < 0) n = 0;
      }

      int start = history_count - n;
      if (start < 0) start = 0;

      for (int i = start; i < history_count; i++) {
        printf("%5d  %s\n", i + 1, history_commands[i]);
      }
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
  int pipe_count = 0;
  for (char* p = input; *p; p++) {
    if (*p == '|') pipe_count++;
  }

  if (pipe_count == 0) return;

  int num_commands = pipe_count + 1;

  char* commands[32];
  int cmd_idx = 0;
  char* start = input;

  for (char* p = input; *p; p++) {
    if (*p == '|') {
      *p = '\0';
      commands[cmd_idx++] = start;
      start = p + 1;
    }
  }
  commands[cmd_idx++] = start;

  for (int i = 0; i < num_commands; i++) {
    while (*commands[i] == ' ') commands[i]++;
  }

  char* argv[32][32];
  int argc[32];
  char cmd_copies[32][1024];
  int is_builtin_cmd[32];

  for (int i = 0; i < num_commands; i++) {
    strncpy(cmd_copies[i], commands[i], sizeof(cmd_copies[i]) - 1);
    cmd_copies[i][sizeof(cmd_copies[i]) - 1] = '\0';
    argc[i] = tokenize(cmd_copies[i], argv[i], 32);

    if (argc[i] == 0) {
      fprintf(stderr, "Invalid pipeline\n");
      return;
    }

    is_builtin_cmd[i] = is_builtin(argv[i][0]);
  }

  int pipes[32][2];
  for (int i = 0; i < num_commands - 1; i++) {
    if (pipe(pipes[i]) < 0) {
      perror("pipe");
      for (int j = 0; j < i; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }
      return;
    }
  }

  pid_t pids[32];

  for (int i = 0; i < num_commands; i++) {
    int input_fd = (i == 0) ? STDIN_FILENO : pipes[i - 1][0];
    int output_fd = (i == num_commands - 1) ? STDOUT_FILENO : pipes[i][1];

    if (is_builtin_cmd[i]) {
      execute_builtin_in_pipeline(argv[i], argc[i], input_fd, output_fd);
      pids[i] = -1;

      if (i > 0) {
        close(pipes[i - 1][0]);
      }
      if (i < num_commands - 1) {
        close(pipes[i][1]);
      }
    }
    else {
      pids[i] = fork();

      if (pids[i] < 0) {
        perror("fork");
        for (int j = 0; j < i; j++) {
          if (pids[j] > 0) kill(pids[j], SIGTERM);
        }
        for (int j = 0; j < num_commands - 1; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }
        return;
      }

      if (pids[i] == 0) {
        if (i > 0) {
          dup2(pipes[i - 1][0], STDIN_FILENO);
        }

        if (i < num_commands - 1) {
          dup2(pipes[i][1], STDOUT_FILENO);
        }

        for (int j = 0; j < num_commands - 1; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }

        char* exec = find_executable(argv[i][0]);
        if (!exec) {
          fprintf(stderr, "%s: command not found\n", argv[i][0]);
          exit(1);
        }
        execv(exec, argv[i]);
        perror("execv");
        exit(1);
      }

      if (i > 0) {
        close(pipes[i - 1][0]);
      }
      if (i < num_commands - 1) {
        close(pipes[i][1]);
      }
    }
  }

  for (int i = 0; i < num_commands - 1; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

  for (int i = 0; i < num_commands; i++) {
    if (pids[i] > 0) {
      waitpid(pids[i], NULL, 0);
    }
  }
}

int main(int argc, char* argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL); // remove the buffer of stdout i.e printing directly & not storing

  // REPL - Read Evaluate Print Loop
  // char input[100]; // declaring a char array to store input command of user
  // const char* builtin[] = { "echo", "exit", "type", "pwd", "cd" };
  signal(SIGINT, handle_sigint);

  char* histfile = getenv("HISTFILE");
  if (histfile) {
    load_history_from_file(histfile);
  }

  enable_raw_mode();
  char buffer[1024];
  int len = 0;
  bool last_was_tab = false;
  int history_index = -1;
  char current_input[1024] = { 0 };
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

    if (c == 27) {
      char seq[2];
      if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
      if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

      if (seq[0] == '[') {
        if (seq[1] == 'A') {
          if (history_count == 0) continue;
          if (history_index == -1) {
            strncpy(current_input, buffer, len);
            current_input[len] = '\0';
          }

          if (history_index == -1) {
            history_index = history_count - 1;
          }
          else if (history_index > 0) {
            history_index--;
          }

          write(STDOUT_FILENO, "\r\033[K$ ", 6);
          strcpy(buffer, history_commands[history_index]);
          len = strlen(buffer);
          write(STDOUT_FILENO, buffer, len);
          continue;
        }
        else if (seq[1] == 'B') {
          if (history_index == -1) continue;
          history_index++;

          if (history_index >= history_count) {
            history_index = -1;
            strcpy(buffer, current_input);
            len = strlen(buffer);
          }
          else {
            strcpy(buffer, history_commands[history_index]);
            len = strlen(buffer);
          }

          write(STDOUT_FILENO, "\r\033[K$ ", 6);
          write(STDOUT_FILENO, buffer, len);
          continue;
        }
      }
    }

    if (c == '\n') {
      buffer[len] = '\0';
      write(STDOUT_FILENO, "\n", 1);
      history_index = -1;
      if (len > 0) {
        if (history_count < 50) {
          strncpy(history_commands[history_count], buffer, sizeof(history_commands[0]) - 1);
          history_commands[history_count][sizeof(history_commands[0]) - 1] = '\0';
          history_count++;
        }
        else {
          for (int i = 0; i < 49; i++) {
            strcpy(history_commands[i], history_commands[i + 1]);
          }
          strncpy(history_commands[49], buffer, sizeof(history_commands[0]) - 1);
          history_commands[49][sizeof(history_commands[0]) - 1] = '\0';
        }

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
        history_index = -1;


        // Move cursor back, erase char, move back again
        write(STDOUT_FILENO, "\b \b", 3);
      }
      continue;
    }

    buffer[len++] = c;
    write(STDOUT_FILENO, &c, 1);
    history_index = -1;
  }
  return 0;
}