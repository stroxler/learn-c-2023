#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "scanner.h"
#include "chunk.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include "compiler.h"


// Viewing scanner output (debugging only) -----------

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


// Parsing utility logic --------------------------------


typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool hadErrorSinceSynchronize;
} Parser;


Parser parser;


static void errorAt(Token* token, const char* message) {
  // Do not report > 1 syntax error per synchronize block.
  if (parser.hadErrorSinceSynchronize) {
    return;
  }
  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing: the message will contain the lexing error in this case.
  } else {
    // The %.*s syntax lets us use a length + char*
    // (as opposed to %s which requires a \0-terminated C-string)
    fprintf(stderr, " at %.*s", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
  parser.hadErrorSinceSynchronize = true;
}

  
static void errorAtPrevious(const char* message) {
  errorAt(&parser.previous, message);
}


static void errorAtCurrent(const char* message) {
  errorAt(&parser.previous, message);
}

static void advance() {
  parser.previous = parser.current;

  parser.current = scanToken();
  while (parser.current.type == TOKEN_ERROR) {
    // (recall that error lexemes contain a static-lifetime C-string
    // rather than a pointer into the source)
    const char* lexing_message = parser.current.start;
    errorAtCurrent(lexing_message);
    parser.current = scanToken();
  }
  // End invariant is that:
  // - parser.previous will point to previous valid token, if any
  // - parser.current will at next valid token
  //
  // In practice, by the end of parsing anything (i.e. at the point
  // where we're ready to compile it), parser.current is going to be
  // pointing at the next token that *isn't* a part of the parsed code,
  // and parser.previous will point at the last token that *is*. We rely
  // on this to get the line mapping right in bytecode.
  //
  // Similarly, in the middle of parsing (e.g. a binary expression),
  // typically parser.previous will point to the last token we processed,
  // (e.g. the operator) whereas parser.current will point to the next
  // unprocessed token (e.g. the first token in the RHS).
}


static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
  } else {
    errorAtCurrent(message);
  }
}

// Compiling utility code ---------------------------------------


// There's just one active chunk per compile for the moment.
// This will change when we move beyond only supporting expressions.
Chunk* compilingChunk;


static Chunk* currentChunk() {
  return compilingChunk;
}


static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line); 
}


static void emit2Bytes(uint8_t byte0, uint8_t byte1) {
  emitByte(byte0);
  emitByte(byte1);
}


static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {
    // Recall that constants is a dynamic array, so there's no
    // problem with memory safety here; the reason we have to error
    // is that we broke our limit on byte size, not that we ran out
    // of space for constants.
    errorAtPrevious("Too many constants in one chunk");
    return 0;
  }
  return (uint8_t)constant;
}


static void emitConstant(Value value) {
  emit2Bytes(OP_CONSTANT, makeConstant(value));
}


// Parse precedence and Pratt tables ----------------------------


// Precedence, low to high
typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR, // or
  PREC_AND,  // and
  PREC_EQUALITY,  // ==, !=
  PREC_COMPARISON,  // < <= > >=
  PREC_TERM,  // + -
  PREC_FACTOR, // *, /
  PREC_UNARY, // !, unary -
  PREC_CALL, // . ()
  PREC_PRIMARY
} Precedence;


// pointer to a func with a `void parse_fn_name();` signature.
typedef void (*ParseFn)();


typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;


// Forward declarations of parsing functions
static void grouping();
static void expression();
static void binary();
static void unary();
static void number();


// Array of ParseRules to handle binary infix parsing.
//
// The `[ENUM_VALUE] =` syntax is a way of inlining
// enums as in indices to arrays (this is new in C99)
ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,  binary,   PREC_TERM},
  [TOKEN_PLUS]          = {NULL,   binary,   PREC_TERM},
  [TOKEN_STAR]          = {NULL,   binary,   PREC_FACTOR},
  [TOKEN_SLASH]         = {NULL,   binary,   PREC_FACTOR},
  [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
};


static ParseRule* getRule(TokenType type) {
  return &rules[type];
}


/* Pratt parsing. This is only ever called when the `current` token
   should be at the start of a valid expression.

   In particular, it's only ever called from `expression`, `unary`,
   an the RHS of `binary` - in all three cases we know that if the
   code is valid, the next token is the start of a valid expression.

   The `precedence` controls what kind of expression this could be... in
   particular we need to know which binary operations we're willing to
   eagerly expand, versus stopping and letting control flow bubble back
   up.

   For example, if we're parsing a TERM then we want to eagerly consume
   all FACTORS, but if we see another TERM or a boolean operator we want
   to stop and let control flow bubble up.
*/
static void parsePrecedence(Precedence precedence) {
  advance();

  // This should never be NULL because of the restriction that we
  // only call the function when `current` is at the start of a valid
  // expression.
  ParseFn prefix_rule = getRule(parser.previous.type)->prefix;
  if (prefix_rule == NULL) {
    errorAtPrevious("Expect expression.");
    return;
  }

  prefix_rule();

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule();
  }
}

// Parse + compile core -----------------------------------------


/* Parse a number, and add a constant to the bytecode */
static void number() {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

/* Parse an expression, consume ending ) */
static void grouping() {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void binary() {
  TokenType operator_type = parser.previous.type;
  ParseRule* rule = getRule(operator_type);

  /* By the time we get here, we will have already parsed (and
     compiled bytecode to push onto the stack) the LHS. The binary
     operation opcode is going to operate on a 2-argument stack.

     So, we need to parse the RHS. We do this by bumping the
     precedence (operators at the same precendence are
     left-associative!), parsing starting at `current`, and only then
     emitting the binary operation opcode. */
  Precedence operand_min_precedence = ((Precedence)(rule->precedence + 1));
  parsePrecedence(operand_min_precedence);
  // ^ At this point, we've output bytecode to make the stack have
  // [..., LHS, RHS] at the point where the next instruction would be
  // whatever we emit next.

  switch (operator_type) {
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  default:
    fprintf(stderr, "Should be unreachable - unknown binary op!");
    return;
  }
}


/* Append an OP_NEGATE to the expression on the right side */
static void unary() {
  TokenType operator_type = parser.previous.type;

  parsePrecedence(PREC_UNARY);

  switch(operator_type) {
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  default:
    fprintf(stderr, "Should be unreachable - unknown unary op!");
    return;
  }
}


static void expression() {
  parsePrecedence(PREC_ASSIGNMENT);
}


// Api to compile a chunk ---------------------------------------


static void finalizeChunk() {
  // For the moment, inject a return after we compile an expression.
  // The line number will match the end of the expression.
  emitByte(OP_RETURN);
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif
}



bool compile(Chunk* chunk, const char* source) {
  initScanner(source);
  // // debug hook:
  // showTokens();
  // return false;

  compilingChunk = chunk;

  advance();
  expression();

  consume(TOKEN_EOF, "Expect end of expression.");
  finalizeChunk();
  

  return !parser.hadError;
}
