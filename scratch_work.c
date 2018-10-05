#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
  // Load config files, if any.

  // Run command loop.
  printf("hi");

  char *cmd = "ls";
  char *argv[3];
  argv[0] = "ls";
  argv[1] = "-la";
  argv[2] = NULL;

  // Perform any shutdown/cleanup.

  return 0;
}
