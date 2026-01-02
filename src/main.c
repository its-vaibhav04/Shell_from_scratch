#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL); //remove the buffer of stdout i.e printing directly & not storing
  printf("$ ");

  char input[100]; //declaring a char array to store input command of user

  //fgets(where to store the input, maximum capacity of input, from where the input is taken)
  fgets(input,100,stdin);

  // TODO: Uncomment the code below to pass the first stage

  //strcspn(x,y) -> Read string x until any character from y matches (return the index of match)
  //command[index] = '\0' -> Replacing next line char with null terminator
  input[strcspn(input,"\n")] = '\0';
  printf("%s: command not found",input);

  return 0;
}
