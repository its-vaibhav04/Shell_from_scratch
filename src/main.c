#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL); //remove the buffer of stdout i.e printing directly & not storing

  //REPL - Read Evaluate Print Loop
  char input[100]; //declaring a char array to store input command of user
  char builtin[3][10]={"echo","exit","type"};
  while(1){

    printf("$ ");

    //fgets(where to store the input, maximum capacity of input, from where the input is taken)
    fgets(input,100,stdin);
    //Can do scanf() as well

    //strcspn(x,y) -> Read string x until any character from y matches (return the index of match)
    //command[index] = '\0' -> Replacing next line char with null terminator
    input[strcspn(input,"\n")] = '\0';

    //Exit Clause
    if(strcmp(input,"exit")==0) break;

    if(strncmp(input, "echo ", 5) == 0){
      printf("%s\n",input+5);
    }else if(strncmp(input, "type ", 5) == 0){
          int found = 0;

          for (int i = 0; i < 3; i++) {
              if (strcmp(builtin[i], input+5) == 0) {
                  found = 1;
                  break;
              }
          }

          if (found)
              printf("%s is a shell builtin\n",input+5);
          else
              printf("%s: not found\n",input+5);
    }else{
      printf("%s: command not found\n",input);
    }
  }

  return 0;
}
