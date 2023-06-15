#include <stdio.h>

int main(int argc, char* argv[])
{
  char* example_string = "python is easier than c";

  for (char* c = example_string; *c != '\0'; c++) {
    switch (*c) {
      case 'a':
      case 'e':
      case 'i':
      case 'o':
      case 'u':
        printf("'%c' is a vowel\n", *c);
        break;
      default:
        printf("'%c' is not a vowel\n", *c);
        break;
    }
  }

  return 0;
}
