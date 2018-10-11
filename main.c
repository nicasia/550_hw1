
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int PIPE_COUNT = 0;

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define LSH_PIPE_DELIM "|\n"

//Method for tokenizing by pipe char
char **split_by_pipe(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_PIPE_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_PIPE_DELIM);
  }
  tokens[position] = NULL;
  printf("%d\n", position);
  PIPE_COUNT = position;
  return tokens;
}

//Method for tokenizing by pipe char
char **split_by_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}


//MAIN LOOP
int main(int argc, char **argv)
{
  
  char **args;
  char **pipe_args;

  int status = 1;

  do {
    
    printf("> ");


    char *line = NULL;
    size_t bufsize = 0; 

    getline(&line, &bufsize, stdin);

    printf("LINE: %s", line);

    pipe_args = split_by_pipe(line);
    printf("PIPE COUNT: %d\n", PIPE_COUNT);
    
    pid_t pids[PIPE_COUNT];
    int fd[PIPE_COUNT][2]; //Create buffers for all processes


    //Fork creation process is modified from https://www.daniweb.com/programming/software-development/threads/261514/linux-shell-multi-piping

    for (int i = 0; i < PIPE_COUNT ; i++) {

      args = split_by_line(pipe_args[i]);

      pipe(fd[i]);


      //Create processes for each pipe
      if ((pids[i] = fork()) < 0) {

        printf("FORK ERROR");
        perror("fork");
        abort();
      } 

      //IN CHILD PROCESS
      else if ( pids[i] == 0 ) {
          
        if(i == 0){ //if the first process, no need to read input from buffer
            close(fd[0][0]);
         }else{ //else, read the input from previous process's buffer
            dup2(fd[i-1][0], 0);
         }

         if( i == PIPE_COUNT - 1){ //if the last process, no need to write the output to buffer
            close(fd[i][1]);
         }else{ // else, write the output to buffer to pass to the next process
            dup2(fd[i][1], 1); //
         }
        
         for (int t = 0; t <= i ; t++) { //close all buffers from current and all previous process buffers
            close(fd[t][0]);
            close(fd[t][1]);
          }

        execvp(args[0], args); //execute the method

        free(args);
        exit(0); //Make sure to exit to ensure child processes do not go through the loop
        
      }
      
    }

    int n = PIPE_COUNT;
    int child_status;
    pid_t pid;
    
    //Close any other left out pipe buffer
    for (int t = 0; t < PIPE_COUNT ; t++) {
      close(fd[t][0]);
      close(fd[t][1]);
    }

    //Wait for all processes to finish
    while (n > 0) {
      pid = wait(&child_status);
      printf("Child with PID %ld exited with status %d \n", (long)pid, child_status);
       --n; 
    }

    //Prevent memory leaks
    free(line);
    free(pipe_args);
    //free(pids);
    //free(fd);

  } while (status); 


  return EXIT_SUCCESS;
}


