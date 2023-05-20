#include <stdio.h>

#include "common.h"
#include "scanner.h"

#include "compiler.h"


void showTokens() {
  int line = -1;
  for (;;) {
    Token token = scanToken();
    if (token.line != line) {
      printf("%4d: ", token.line);
      line = token.line;
    } else {
      printf ("   |  ");
    }
    printf("%2d '%.*s'\n", token.type, token.length, token.start);
    if (token.type == TOKEN_EOF) {
      break;
    }
  }
}

void compile(const char* source) {
  initScanner(source);
  showTokens();
}
