#include <stdio.h>

int main(int argc, char* argv[])
{
  if (argc == 1) {
    printf("You have one argument, '%s'.\n", argv[0]);
  } else if (argc > 1 && argc < 4) {
    printf("Here are your arguments:\n");
    for (int i = 0; i < argc; i++) {
      printf("'%s' ", argv[i]);
    }
  } else {
    printf("Too many arguments, please pass at most 2");
  }
  return 0;
}
