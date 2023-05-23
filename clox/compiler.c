#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "scanner.h"
#include "object.h"
#include "value.h"
#include "chunk.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include "compiler.h"

// The compiler is a global, defined per compile unit ------


typedef struct{
  Token name;
  int depth;
} Local;


typedef struct {
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;


Compiler* currentCompiler = NULL;


// Viewing scanner output (debugging only) -----------------


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


// Parsing utility logic ------------------------------------


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


static bool check(TokenType type) {
  return parser.current.type == type;
}


static bool match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  } else {
    return false;
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
  PREC_NEGATION, // !, unary -
  PREC_CALL, // . ()
  PREC_PRIMARY
} Precedence;


char* precedenceNames[] = {
  [PREC_NONE] = "PREC_NONE",
  [PREC_ASSIGNMENT] = "PREC_ASSIGNMENT",
  [PREC_OR] = "PREC_OR",
  [PREC_AND] = "PREC_AND",
  [PREC_EQUALITY] = "PREC_EQUALITY",
  [PREC_COMPARISON] = "PREC_COMPARISON",
  [PREC_TERM] = "PREC_TERM",
  [PREC_FACTOR] = "PREC_FACTOR",
  [PREC_NEGATION] = "PREC_NEGATION",
  [PREC_CALL] = "PREC_CALL",
  [PREC_PRIMARY] = "PREC_PRIMARY",
};


char* precedenceName(Precedence precedence) {
  return precedenceNames[precedence];
}


// pointer to a func with a `void parse_fn_name();` signature.
typedef void (*ParseFn)(bool canAssign);


typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;


// Forward declaration of expression, which ties most of the parsing
// recursion together.
static void expression();

// Parsing functions that aren't yet in our pratt parsing table but will be
static void declaration();
static void statement();

// Forward declarations of parsing functions
static void grouping(bool canAssign);
static void binary(bool canAssign);
static void unary(bool canAssign);
static void number(bool canAssign);
static void string(bool canAssign);
static void literal(bool canAssign);
static void variable(bool canAssign);


// Array of ParseRules to handle binary infix parsing.
//
// The `[ENUM_VALUE] =` syntax is a way of inlining
// enums as in indices to arrays (this is new in C99)
//
// I was a bit surprised but bang does indeed have ultra-low
// precedence.
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
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,   binary,   PREC_EQUALITY},
  [TOKEN_EQUAL_EQUAL]   = {NULL,   binary,   PREC_EQUALITY},
  [TOKEN_LESS]          = {NULL,   binary,   PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,   binary,   PREC_COMPARISON},
  [TOKEN_GREATER]       = {NULL,   binary,   PREC_COMPARISON},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
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

   Note that by the time we actually enter a rule triggered here, the
   token triggering the rule in the Pratt table is *already* consumed,
   so you need to access it using `parser.previous` if the rule has
   variable logic (see for example `unary`).
*/
static void parsePrecedence(Precedence precedence) {
  advance();

  // Hang on to the start token - useful for debugging!
  Token start_token = parser.previous;

  PRINT_DEBUG("Start of parsePrecedence, parser.previous is %s (%d) / %s\n",
              tokenTypeName(start_token.type),
              start_token.line,
	      precedenceName(precedence));

  // This should never be NULL because of the restriction that we
  // only call the function when `current` is at the start of a valid
  // expression.
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  ParseFn prefix_rule = getRule(start_token.type)->prefix;
  if (prefix_rule == NULL) {
    errorAtPrevious("Expect expression.");
    return;
  }

  prefix_rule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    PRINT_DEBUG("Infix of parsePrecedence [start = %s (%d) / %s], parser.current is %s (%d) / %s\n",
                tokenTypeName(start_token.type),
                start_token.line,
	        precedenceName(precedence),
		tokenTypeName(parser.current.type),
		parser.current.line,
		precedenceName(getRule(parser.current.type)->precedence));
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  // If we hit this line, then a recursive call to parsePrecedence set
  // canAssign to false, which means the overall thing we just parsed
  // is an *invalid* assignment target.
  //
  // If we don't catch that here, then the outer loop will silently
  // drop the `=` (because it maps to a null. We want a loud parse
  // error instead.
  //
  // FWIW I tried putting this logic inside of NamedVariable and I got
  // a syntax error on the right line, but it was the wrong error. I still
  // haven't wrapped my head around Pratt enough to figure out why yet.
  if (canAssign && match(TOKEN_EQUAL)) {
    errorAtPrevious("Invalid assignment target.");
  }

  PRINT_DEBUG("End of parsePrecedence [start = %s (%d) / %s], parser.current is %s (%d) / %s\n",
              tokenTypeName(start_token.type),
              start_token.line,
	      precedenceName(precedence),
	      tokenTypeName(parser.current.type),
	      parser.current.line,
	      precedenceName(getRule(parser.current.type)->precedence));
}


static void parseRhsForOperator(TokenType operator_type) {
  Precedence rhs_precedence = (Precedence)(getRule(operator_type)->precedence + 1);
  parsePrecedence(rhs_precedence);
}



// Parse + compile core -----------------------------------------


static bool identifiersEqual(Token* a, Token* b) {
  return (a->length == b->length &&
	  memcmp(a->start, b->start, a->length) == 0);
}


static uint8_t identifierConstant(Token* name) {
  return makeConstant(OBJ_VAL(createString(name->start, name->length)));
}


static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}


static void string(bool canAssign) {
  const char* segment_start = parser.previous.start + 1;
  int segment_length = parser.previous.length - 2;
  emitConstant(OBJ_VAL(createString(segment_start, segment_length)));
}


/* Parse an expression, consume ending ) */
static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void binary(bool canAssign) {
  TokenType operator_type = parser.previous.type;
  ParseRule* rule = getRule(operator_type);

  /* By the time we get here, we will have already parsed (and
     compiled bytecode to push onto the stack) the LHS. The binary
     operation opcode is going to operate on a 2-argument stack.

     So, we need to parse the RHS, which we do by parsing an expression
     with a precedence limit of one more than our current precedence; this
     will emit the bytecode so that the stack looks like [..., LHS, RHS]
     when our binary op bytecode runs. */
  parseRhsForOperator(operator_type);

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
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_BANG_EQUAL:
    emit2Bytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_LESS_EQUAL:
    emit2Bytes(OP_GREATER, OP_NOT);
    break;
  case TOKEN_GREATER_EQUAL:
    emit2Bytes(OP_LESS, OP_NOT);
    break;
  default:
    fprintf(stderr, "should be unreachable - unknown binary op!\n");
    return;
  }
}


static void literal(bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  default:
    fprintf(stderr, "should be unreachable - unknown literal!\n");
    return;
  }
}


static int resolveLocal(Compiler* compiler, Token* name) {
  // Scan backward (across all lexical scopes except global)
  // for any local var with this name, taking first hit.
  for (int i = compiler->localCount - 1; i >=0; i--) {
    Local* local = &compiler->locals[i];
    // NOTE: I diverge from clox here in that I allow a var to use
    // a shadowed var in its own initializer (why? Ocaml does!)
    if (local->depth != -1 && identifiersEqual(name, &local->name)) {
      return i;
    }
  }
  // If there's no hit, return -1 as a sentinel telling the
  // compiler to try a global lookup instead.
  return -1;
}


/* `canAssign` is a precedence-checking hack, specifically needed
   because we can't treat `=` as a Pratt infix operator...

   Why not? Two reasons:
   - Not all expressions are valid assignment targets. If we used
     Pratt parsing, it would be either tricky or impossible depending
     on precedence rules to guarantee that we don't parse invalid code.
   - We cannot eagerly compile the LHS - we need to peek ahead via a
     `match(TOKEN_EQUAL)` to know whether we are getting or setting a
     value *before* we decide whether to emit bytecode to evaluate
     the expression and add it to the stack.
*/
static void namedVariable(Token* name, bool canAssign) {
  int local_index = resolveLocal(currentCompiler, name);

  // Determine whether to use a local or global op. The arg
  // is an int either way, but the meaning is different: it's an
  // index into constants for globals, vs a stack index for locals.
  uint8_t getOp, setOp, arg;
  if (local_index == -1) {
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
    arg = identifierConstant(name);
  } else {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
    arg = (uint8_t)local_index;
  }

  // For bare variables, we can decide get vs set with a simple match
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();  // evaluate the assignment RHS, put it on the stack
    emit2Bytes(setOp, arg);  // (it will stay on the stack)
  } else {
    emit2Bytes(getOp, arg);
  }
}

static void variable(bool canAssign) {
  namedVariable(&parser.previous, canAssign); // note: the book passes by value here
}


static void unary(bool canAssign) {
  TokenType operator_type = parser.previous.type;

  parseRhsForOperator(operator_type);

  switch(operator_type) {
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  case TOKEN_BANG:
    emitByte(OP_NOT);
    break;
  default:
    fprintf(stderr,
	    "Should be unreachable - unknown unary op %s\n",
	    tokenTypeName(operator_type));
    return;
  }
}


static void expression() {
  parsePrecedence(PREC_ASSIGNMENT);
}


static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression");
  emitByte(OP_POP);
}


static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value in print statement");
  emitByte(OP_PRINT);
}


static void beginScope() {
  // Bump scope depth. Nothing else needs doing.
  currentCompiler->scopeDepth++;
}


static void endScope() {
  // Decrement sope depth.
  currentCompiler->scopeDepth--;
  // But also clear out locals, at both:
  // - static analysis type (by decrementing the localCount)
  // - runtime (by emitting bytecode that pops all locals going out of
  //   scope)
  int outer_scope_depth = currentCompiler->scopeDepth;
  while(currentCompiler->localCount > 0 &&
	(currentCompiler->locals[currentCompiler->localCount - 1].depth
	 > outer_scope_depth)) {
    currentCompiler->localCount--;
    emitByte(OP_POP);
  }
}


static void block() {
  beginScope();
  while (!(check(TOKEN_RIGHT_BRACE) || check(TOKEN_EOF))) {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
  endScope();
}


static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    block();
  } else {
    expressionStatement();
  }
}


static void addLocal(Token name) {
  if (currentCompiler->localCount == UINT8_COUNT) {
    errorAtPrevious("Too many local variables in function.");
    return;
  }
  // Get the next free local slot
  Local* local = &currentCompiler->locals[currentCompiler->localCount++];
  local->name = name;
  local->depth = -1; // we'll soon set it to `currentCompiler->scopeDepth`
}


void addLocalToScope() { // NOTE: the book calls this `declareVariable`
  Token* name = &parser.previous;

  // Check that we don't try to define the same variable twice
  // in the same scope
  for (int i = currentCompiler->localCount - 1; i >= 0; i--) {
    Local* local = &currentCompiler->locals[i];
    // The -1 here is a sentinel value that causes us to skip a newly declared
    // but not-yet defined local shadowing some parent scope. This is important
    // when we want to use the shadowed variable inside of the initializing
    // expression, e.g. `var x = 1; { var x = x + 1; }`
    if (local->depth != -1 && local->depth < currentCompiler->scopeDepth) {
      // (stop checking - we've backtracked to some parent scope and
      // shadowing is okay)
      //
      // I think the -1 is for parameters or captures or both,
      // although it's as-yet unexplained.
      break;
    }
    if (identifiersEqual(name, &local->name)) {
      errorAtPrevious("A variable of this name is already defined in the same scope");
    }
  }

  addLocal(*name);
}


uint8_t parseVariableInDeclaration(const char* error_message) {
  consume(TOKEN_IDENTIFIER, error_message);
  // For globals and locals we do different things:
  //
  // - When we hit a global declaration, we emit code to push the name
  //   so we can use it in OP_GLOBAL
  // - When we hit a local declaration, we don't have to emit anything
  //   but we do need to track the variable in our scope (the "resolver"
  //   logic in jlox terms)
  //
  // We return 0 as a placeholder for the constant to push in the local
  // case; the downstream code in defineVariable will ignore this.
  if (currentCompiler->scopeDepth == 0) {
    return identifierConstant(&parser.previous);
  } else {
    addLocalToScope();
    return 0;
  }
}


void defineVariable(uint8_t maybe_global) {
  if (currentCompiler->scopeDepth == 0) {
    emit2Bytes(OP_DEFINE_GLOBAL, maybe_global);
  } else {
    // For locals, we don't need to emit any opcode here. We already
    // emitted the byetocde for the RHS inside varDeclaration, and
    // we *didn't* emit anything in parseVariable, so our new local
    // is already on top of the stack.
    //
    // But we do need to mark the local as reachable only *after*
    // resolving the bytecode for any initializing instruction.
    Local* local = &currentCompiler->locals[currentCompiler->localCount - 1];
    local->depth = currentCompiler->scopeDepth;
  }
}
  

static void varDeclaration() {
  // Push the global name onto the stack (which, in the process, adds
  // the value to chunk.constants and the underlying ObjString* to
  // vm.strings).
  uint8_t maybe_global = parseVariableInDeclaration("Expect variable name.");
  // Push the initial value on the stack - nil if no assignment
  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  // Consume the statement end and emit the bytecode to load the value.
  consume(TOKEN_SEMICOLON, "Expect ';' after var declaration.");
  defineVariable(maybe_global);
}


/* Attempt to recover at likely statement boundaries boundaries */
static void synchronize() {
  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) {
      // If the next token is a valid declaration start, reset state to keep going
      switch(parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN: {
	parser.hadErrorSinceSynchronize = false;
	return;
      }
      default:
	;
      }
    }
    advance();
  }
}


static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }

  if (parser.hadErrorSinceSynchronize) {
    synchronize();
  }
}


// Api to compile a chunk ---------------------------------------


void initParser(Chunk* chunk, const char* source) {
  initScanner(source);
  parser.hadError = false;
  parser.hadErrorSinceSynchronize = false;
  compilingChunk = chunk;
}


void initCompiler(Compiler* compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  currentCompiler = compiler;
}


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
  initParser(chunk, source);
  Compiler compiler;  // (note this is stack allocated, it goes out of scope at func end)
  initCompiler(&compiler);

  // // debug hook:
  // showTokens();
  // return false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }
  
  finalizeChunk();
  

  return !parser.hadError;
}
