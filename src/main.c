#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

// Tokenizes input into argv array, returns argc
int tokenize(char *input, char *argv[], int max_args)
{
  int argc = 0;
  char *token = strtok(input, " ");

  while (token && argc < max_args - 1)
  {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }
  argv[argc] = NULL;
  return argc;
}

// Returns full path of executable if found in PATH, else NULL
// Caller must free the returned string
char *find_executable(const char *command)
{
  if (!command || strlen(command) == 0)
    return NULL;

  char *path_env = getenv("PATH");
  if (!path_env)
    return NULL;

  char *path_copy = strdup(path_env);
  if (!path_copy)
    return NULL;

  char *token = strtok(path_copy, ":");
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
void execute_external(char *argv[])
{

  char *exe_path = find_executable(argv[0]);
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

int main(int argc, char *argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL); // remove the buffer of stdout i.e printing directly & not storing

  // REPL - Read Evaluate Print Loop
  char input[100]; // declaring a char array to store input command of user
  char builtin[4][10] = {"echo", "exit", "type", "pwd"};
  while (1)
  {

    printf("$ ");

    // fgets(where to store the input, maximum capacity of input, from where the input is taken)
    fgets(input, 100, stdin);
    // Can do scanf() as well

    // strcspn(x,y) -> Read string x until any character from y matches (return the index of match)
    // command[index] = '\0' -> Replacing next line char with null terminator
    input[strcspn(input, "\n")] = '\0';

    char *argvv[20];
    char input_copy[100];

    strcpy(input_copy, input);
    int argc = tokenize(input_copy, argvv, 20);

    if (argc == 0)
      continue;

    // EXIT COMMAND
    if (argc == 1 && strcmp(argvv[0], "exit") == 0)
      break;

    // ECHO COMMAND
    if (strcmp(argvv[0], "echo") == 0)
    {
      for (int i = 1; i < argc; i++)
      {
        printf("%s", argvv[i]);
        if (i < argc - 1)
          printf(" ");
      }
      printf("\n");
    }
    else if (argc >= 2 && strcmp(argvv[0], "type") == 0)
    { // TYPE COMMAND
      const char *cmd = argvv[1];
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
        char *exe_path = find_executable(cmd);
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
      if (chdir(argvv[1]) != 0)
      {
        fprintf(stderr, "cd: %s: %s\n", argvv[1], strerror(errno));
      }
    }
    else
    {
      execute_external(argvv);
    }
  }
  return 0;
}
