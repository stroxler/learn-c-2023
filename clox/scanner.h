#ifndef clox_scanner_h
#define clox_scanner_h



typedef enum {
  // One character tokens
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON,
  TOKEN_PLUS, TOKEN_MINUS,
  TOKEN_STAR, TOKEN_SLASH,
  // One and two-character paired tokens
  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_LESS, TOKEN_LESS_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,
  // Literals
  TOKEN_NUMBER, TOKEN_STRING, TOKEN_IDENTIFIER,
  // Keywords
  TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
  TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
  TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
  TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
  // Special tokens
  TOKEN_ERROR, TOKEN_EOF
} TokenType;


// Tokens point back into the source (note that this means the source
// lifetime has to be greater than that of tokens).
typedef struct {
  TokenType type;
  const char* start;
  int length;
  int line;
} Token;



void initScanner(const char* source);


Token scanToken();


char* tokenTypeName(TokenType token);

#endif
