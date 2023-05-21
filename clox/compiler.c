#include <stdio.h>

#include "common.h"
#include "scanner.h"
#include "chunk.h"

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


bool compile(Chunk* chunk, const char* source) {
  initScanner(source);
  // // debug hook:
  showTokens();
  return false;
}
