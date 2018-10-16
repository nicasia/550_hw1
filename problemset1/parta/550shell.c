/*
 * CSE 550 HW 1 part a
 * Date: October 16 2018
 * Authors: Alex Okeson, Nicasia Bebe-Wang, Ayse Dincer
 *
 * Implements a shell that is able to pipe command line commands
 *
 * Borrowed code: the basic shell structure is modified from https://brennan.io/2015/01/16/write-a-shell-in-c/
 * Fork creation process is modified from https://www.daniweb.com/programming/software-development/threads/261514/linux-shell-multi-piping
 */

#include <sys/wait.h>
#include <sys/types.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int PIPE_COUNT = 0;
#define LSH_TOK_BUFSIZE 256
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
        token = strtok(NULL, LSH_PIPE_DELIM);
    }
    
  tokens[position] = NULL;
    PIPE_COUNT = position;
    return tokens;
}

//Method for tokenizing by space char
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

        //Read line from user
        getline(&line, &bufsize, stdin);
    //printf("LINE: %s", line);

    //Separate into pipe arguments
        pipe_args = split_by_pipe(line);
        //printf("PIPE COUNT: %d\n", PIPE_COUNT);
    
    //Create buffers for all processes
        pid_t pids[PIPE_COUNT];
        int fd[PIPE_COUNT][2]; 


        //Loop through all pipes
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
          else if( pids[i] == 0 ){
         
      //first process takes the input from STDOUT, no need for reading fd
      if(i == 0){ 
          close(fd[0][0]);
       }
      //other processes take arguments from fd of previous process
      else{
          dup2(fd[i-1][0], 0);
       }
       //last process writes directly to STD_OUT, no need for writing to fd
       if( i == PIPE_COUNT - 1){
          close(fd[i][1]);
       }
      //other processes write to buffer to pass their output to the next process
      else{ // else, write the output to buffer to pass to the next process
          dup2(fd[i][1], 1); 
       }
    
       //Close all buffers from all processes upto current one
       for (int t = 0; t <= i ; t++) { 
          close(fd[t][0]);
          close(fd[t][1]);
        }

      //execute the command
      execvp(args[0], args);

      exit(0);
    
    }
          free(args);
      
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
      //printf("Child with PID %ld exited with status %d \n", (long)pid, child_status);
       --n; 
    }

    //Prevent memory leaks
    free(line);
    free(pipe_args);

  } while (status); 


  return EXIT_SUCCESS;
}
