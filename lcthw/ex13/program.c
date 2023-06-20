#include <stdio.h>

int main(int argc, char* argv[])
{
  if (argc != 2) {
    printf("ERROR: please specify an argument string.\n");
    return 1;
  }

  int i = 0;
  for (i = 0; argv[1][i] != '\0'; i++) {
    char letter = argv[1][i];

    switch(letter) {
      case 'A':
        printf("%d: %d = 'A'\n", i, letter);
        break;
      case 'a':
        printf("%d: %d = 'a'\n", i, letter);
        break;
      case 'E':
        printf("%d: %d = 'E'\n", i, letter);
        break;
      case 'e':
        printf("%d: %d = 'e'\n", i, letter);
        break;
      case 'I':
        printf("%d: %d = 'I'\n", i, letter);
        break;
      case 'i':
        printf("%d: %d = 'i'\n", i, letter);
        break;
      case 'O':
        printf("%d: %d = 'O'\n", i, letter);
        break;
      case 'o':
        printf("%d: %d = 'o'\n", i, letter);
        break;
      case 'U':
        printf("%d: %d = 'U'\n", i, letter);
        break;
      case 'u':
        printf("%d: %d = 'u'\n", i, letter);
        break;
      default:
        printf("%d: %d (not a vowel) = %c'\n", i, letter, letter);
    }
  }
  return 0;
}
