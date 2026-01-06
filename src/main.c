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

void parse_redirection(char* argv[], char** out_stdout, char** out_stderr)
{
  *out_stdout = NULL;
  *out_stderr = NULL;

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
  }
}


int main(int argc, char* argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL); // remove the buffer of stdout i.e printing directly & not storing

  // REPL - Read Evaluate Print Loop
  char input[100]; // declaring a char array to store input command of user
  const char* builtin[] = { "echo", "exit", "type", "pwd", "cd" };
  while (1)
  {

    printf("$ ");

    // fgets(where to store the input, maximum capacity of input, from where the input is taken)
    // Can do scanf() as well
    if (!fgets(input, sizeof(input), stdin))
      goto cleanup_and_exit;

    // strcspn(x,y) -> Read string x until any character from y matches (return the index of match)
    // command[index] = '\0' -> Replacing next line char with null terminator
    input[strcspn(input, "\n")] = '\0';

    char* argvv[20];
    char input_copy[100];

    strcpy(input_copy, input);
    int argc = tokenize(input_copy, argvv, 20);

    if (argc == 0)
      continue;

    char* out_stdout, * out_stderr;
    parse_redirection(argvv, &out_stdout, &out_stderr);

    int saved_stdout = -1, saved_stderr = -1;

    if (out_stdout) {
      saved_stdout = dup(1);
      int fd = open(out_stdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      dup2(fd, 1);
      close(fd);
    }

    if (out_stderr) {
      saved_stderr = dup(2);
      int fd = open(out_stderr, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      dup2(fd, 2);
      close(fd);
    }

    // EXIT COMMAND
    if (argc == 1 && strcmp(argvv[0], "exit") == 0)
      goto cleanup_and_exit;

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
    continue;

  cleanup_and_exit:
    if (out_stdout) {
      dup2(saved_stdout, 1);
      close(saved_stdout);
    }

    if (out_stderr) {
      dup2(saved_stderr, 2);
      close(saved_stderr);
    }
    break;

  }
  return 0;
}
