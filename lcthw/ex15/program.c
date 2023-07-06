#include <stdio.h>

int main(int argc, char *argv[]) {
  // Array based version of the program

  int ages[] = { 35, 40, 45 };
  char* names[] = { "Susan", "Hongbo", "Jose" };

  int count = sizeof(ages) / sizeof(int);

  for (int i = 0; i < count; i++) {
    printf("%s is %d years old\n", names[i], ages[i]);
  }

  printf("----\n");

  // Pointer-based version of the program
  
  int* agesp = ages;
  char** namesp = names;

  for (int i = 0; i < count; i++) {
    printf("%s is %d years old\n", *(namesp + i), *(agesp + i));
  }
}
