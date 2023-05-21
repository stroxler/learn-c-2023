#include <stdio.h>
#include <string.h>

#include "common.h"

#include "scanner.h"



typedef struct {
  const char* previous;
  const char* current;
  int line;
} Scanner;


Scanner scanner;


void initScanner(const char* source) {
  scanner.previous = source;
  scanner.current = source;
  scanner.line = 1;
}


static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.previous;
  token.length = (int)(scanner.current - scanner.previous);
  token.line = scanner.line;
  return token;
}


static Token errorToken(const char* message) {
  // (The message lifetime will be until the end of the program; generally
  //  it is actually static).
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}




static bool isAtEnd() {
  return *scanner.current == '\0';
}


static char advance() {
  scanner.current++;
  return scanner.current[-1];
}


static char peek() {
  return *scanner.current;
}


static void skipWhitespaceLoop() {
  for (;;) {
    char c = peek();
    switch(c) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    case '\n':
      advance();
      scanner.line++;
      break;
    default:
      return;
    }
  }
}


static void skipWhitespace() {
  skipWhitespaceLoop();
  scanner.previous = scanner.current;
}


static bool isDigit(char c ) {
  return c >= '0' && c <= '9';
}


static Token number() {
  while (isDigit(peek())) {
    advance();
  }

  if (peek() == '.') {
    advance();
    // Note: I'm allowing numbers of the form 5., which
    // the book does not allow but e.g. Python does :)
    while (isDigit(peek())) {
      advance();
    }
  }
  return makeToken(TOKEN_NUMBER);
}


static Token string() {
  for (;;) {
    if (peek() == '"') {
      return makeToken(TOKEN_STRING);
    }
    advance();
  }
}


static bool isAlphaUnderscore(char c) {
  return ((c == '_') ||
	  (c >= 'a' && c <= 'z') || 
	  (c >= 'A' && c <= 'Z'));
}


static bool isAlphaNumericUnderscore(char c) {
  return (isDigit(c) || isAlphaUnderscore(c));
}


static TokenType maybeKeyword(int start,
		              int length,
			      const char* rest,
			      TokenType type) {
  if (scanner.current - scanner.previous == start + length
      && memcmp(scanner.previous + start, rest, length) == 0) {
    return type;
  } else {
    return TOKEN_IDENTIFIER;
  }
}
				    


static TokenType identifierOrKeywordType() {
  switch (scanner.previous[0]) {
  case 'a':
    return maybeKeyword(1, 2, "nd", TOKEN_AND);
  case 'c':
    return maybeKeyword(1, 3, "ass", TOKEN_CLASS);
  case 'e':
    return maybeKeyword(1, 3, "lse", TOKEN_ELSE);
  case 'f':
    switch (scanner.previous[1]) {
    case 'a':
      return maybeKeyword(2, 3, "lse", TOKEN_FOR);
    case 'o':
      return maybeKeyword(2, 1, "r", TOKEN_FALSE);
    case 'u':
      return maybeKeyword(2, 1, "n", TOKEN_FUN);
    default:
      return TOKEN_IDENTIFIER;
    }
  case 'i':
    return maybeKeyword(1, 1, "f", TOKEN_IF);
  case 'n':
    return maybeKeyword(1, 2, "il", TOKEN_NIL);
  case 'o':
    return maybeKeyword(1, 1, "r", TOKEN_OR);
  case 'p':
    return maybeKeyword(1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return maybeKeyword(1, 5, "eturn", TOKEN_RETURN);
  case 's':
    return maybeKeyword(1, 4, "uper", TOKEN_SUPER);
  case 't': {
    switch (scanner.previous[1]) {
    case 'h':
      return maybeKeyword(2, 2, "is", TOKEN_THIS);
    case 'r':
      return maybeKeyword(2, 2, "ue", TOKEN_TRUE);
    default:
      return TOKEN_IDENTIFIER;
    }
  }
  case 'v':
    return maybeKeyword(1, 2, "ar", TOKEN_VAR);
  case 'w':
    return maybeKeyword(1, 4, "hile", TOKEN_WHILE);
  default:
    return TOKEN_IDENTIFIER;
  }
}


static Token identifierOrKeyword() {
  while (isAlphaNumericUnderscore(peek())) {
    advance();
  }
  TokenType token_type = identifierOrKeywordType();
  return makeToken(token_type);
}


Token scanToken() {
  // Invariants:
  // - at the start of the function:
  //   - scanner.current points at the next character not
  //     part of the previous token
  //   - scanner.previous is irrelevant!
  // - at the end of skipWhitespace():
  //   - scanner.previous and scanner.current point at the same char
  //   - that character is the next for the upcoming token
  //
  // Any time we advance, the result will be at scanner.current - 1
  // Any time we peek, the sesult will be at scanner.current
  //
  // The algorithm is mainly centered on making sure that we handle
  // various tokens that nest inside of other tokens correctly.
  skipWhitespace();

  if (isAtEnd()) {
    return makeToken(TOKEN_EOF);
  }

  char c = advance();

  // More complex rules are all identifiable based on the first
  // character.
  if (isDigit(c)) {
    return number();
  }
  if (c == '"') {
    return string();
  }
  if (isAlphaUnderscore(c)) {
    return identifierOrKeyword();
  }

  // The symbol rules are all either one or two characters;
  // all of the two-character ones have a nested one-character
  // possibility so we have to peek.
  switch (c) {
  case '(': return makeToken(TOKEN_LEFT_PAREN);
  case ')': return makeToken(TOKEN_RIGHT_PAREN);
  case '{': return makeToken(TOKEN_LEFT_BRACE);
  case '}': return makeToken(TOKEN_RIGHT_BRACE);
  case ',': return makeToken(TOKEN_COMMA);
  case '.': return makeToken(TOKEN_DOT);
  case ';': return makeToken(TOKEN_SEMICOLON);
  case '+': return makeToken(TOKEN_PLUS);
  case '-': return makeToken(TOKEN_MINUS);
  case '*': return makeToken(TOKEN_STAR);
  case '/': return makeToken(TOKEN_SLASH);
    // Pairs of nested single and double-character tokens
  case '!': {
    char c = peek();
    if (c == '=') {
      advance();
      makeToken(TOKEN_BANG_EQUAL);
    } else {
      makeToken(TOKEN_BANG);
    }
  }
  case '=': {
    char c = peek();
    if (c == '=') {
      advance();
      makeToken(TOKEN_EQUAL_EQUAL);
    } else {
      makeToken(TOKEN_EQUAL);
    }
  }
  case '<': {
    char c = peek();
    if (c == '=') {
      advance();
      makeToken(TOKEN_LESS_EQUAL);
    } else {
      makeToken(TOKEN_LESS);
    }
  }
  case '>': {
    char c = peek();
    if (c == '=') {
      advance();
      makeToken(TOKEN_GREATER_EQUAL);
    } else {
      makeToken(TOKEN_GREATER);
    }
  }
    // Identifiers and keywords
  default:
    return errorToken("NotImplementedYet");
  }
  
  // FIXME!!
  return makeToken(TOKEN_EOF);
}
