#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define LSH_RL_BUFSIZE 1024
/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *lsh_read_line(void)
{
  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Read a character
    c = getchar();

    // If we hit EOF, replace it with a null character and return.
    if (c == EOF || c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize) {
      bufsize += LSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}


int main(int argc, char **argv)
{
  // Load config files, if any.

  // Run command loop.
  printf("hi");

  // char *cmd = "ls";
  // char *thing[3];
  // thing[0] = "ls";
  // thing[1] = "-la";
  // thing[2] = NULL;
  // execvp(cmd, thing);

  char *cmd = "ls";
  char *thing[3];
  thing[0] = "ls";
  thing[1] = "-la";
  thing[2] = NULL;
  execvp(cmd, thing);

  lsh_read_line();



  // Perform any shutdown/cleanup.

  return 0;
}
