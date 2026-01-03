#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

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
void execute_external(char *input)
{
  char *argv[20];
  tokenize(input, argv, 20);

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
  char builtin[3][10] = {"echo", "exit", "type"};
  while (1)
  {

    printf("$ ");

    // fgets(where to store the input, maximum capacity of input, from where the input is taken)
    fgets(input, 100, stdin);
    // Can do scanf() as well

    // strcspn(x,y) -> Read string x until any character from y matches (return the index of match)
    // command[index] = '\0' -> Replacing next line char with null terminator
    input[strcspn(input, "\n")] = '\0';

    // EXIT COMMAND
    if (strcmp(input, "exit") == 0)
      break;

    // ECHO COMMAND
    if (strncmp(input, "echo ", 5) == 0)
    {
      printf("%s\n", input + 5);
    }
    else if (strncmp(input, "type ", 5) == 0)
    { // TYPE COMMAND
      int found = 0;

      for (int i = 0; i < 3; i++)
      { // Checks command for each builtin
        if (strcmp(builtin[i], input + 5) == 0)
        {
          found = 1;
          break;
        }
      }
      if (found)
      {
        printf("%s is a shell builtin\n", input + 5);
      }
      else
      {
        char *exe_path = find_executable(input + 5);
        if (exe_path)
        {
          printf("%s is %s\n", input + 5, exe_path);
          free(exe_path);
        }
        else
        {
          printf("%s: not found\n", input + 5);
        }
      }
    }
    else
    {
      execute_external(input);
    }
  }
  return 0;
}
