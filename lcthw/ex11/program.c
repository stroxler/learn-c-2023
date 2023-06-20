#include <stdio.h>

int main(int argc, char* argv[])
{
  int numbers [4] = { 0 };
  printf ("numbers %d %d %d %d\n", numbers[0], numbers[1], numbers[2], numbers[3]);
  char chars[4] = { 'a' };
  printf ("chars %c %c %c %c\n", chars[0], chars[1], chars[2], chars[3]);

  numbers[0] = 1;
  numbers[1] = 2;
  numbers[2] = 3;
  printf ("numbers %d %d %d %d\n", numbers[0], numbers[1], numbers[2], numbers[3]);

  chars[0] = 'b';
  chars[1] = 'c';
  chars[2] = 'd';
  printf ("chars %c %c %c %c\n", chars[0], chars[1], chars[2], chars[3]);

  chars[3] = '\0';
  printf ("chars %c %c %c %c\n", chars[0], chars[1], chars[2], chars[3]);

  printf("chars as a string: %s\n", chars);

  char* a_string = "str";
  printf("a_string as a string: %s\n", a_string);

  printf("a_string as chars: %c %c %c %c\n", a_string[0], a_string[1], a_string[2], a_string[3]);

  return 0;
}
