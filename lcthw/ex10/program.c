#include <stdio.h>

int main(int argc, char* argv[])
{
  char* states [] = {
    "California",
    "Oregon",
    "Washington"
  };
  
  int num_states = sizeof(states) / sizeof(char*);

  for (int i = 0; i < num_states; i++) {
    printf("State %d = %s\n", i, states[i]);
  }

  return 0;
}
