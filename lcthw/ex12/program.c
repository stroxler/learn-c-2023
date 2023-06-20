#include <stdio.h>

int main(int argc, char* argv[])
{
  int areas[] = { 10, 12, 13, 14, 20 };
  char name [] = { 'S', 't', 'e', 'v', 'e', 'n', '\0'};
  char full_name[] = "Steven Troxler";

  printf("The size of an int: %ld\n", sizeof(int));
  printf("The size of the areas array: %ld\n", sizeof(areas));
  printf("The count of the areas array: %ld\n", sizeof(areas) / sizeof(int));

  printf("The size of a char: %ld\n", sizeof(char));
  printf("The size of name: %ld\n", sizeof(name));
  printf("The length of name, including its terminator: %ld\n", sizeof(name) / sizeof(char));

  printf("The size of full_name: %ld\n", sizeof(full_name));
  printf("The length of full_name, including its terminator: %ld\n", sizeof(full_name) / sizeof(char));
}
