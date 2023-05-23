#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"


typedef enum {
  OP_ADD,
  OP_CONSTANT,
  OP_DIVIDE,
  OP_DEFINE_GLOBAL,
  OP_EQUAL,
  OP_FALSE,
  OP_GREATER,
  OP_LESS,
  OP_MULTIPLY,
  OP_NEGATE,
  OP_NIL,
  OP_NOT,
  OP_POP,
  OP_PRINT,
  OP_RETURN,
  OP_SUBTRACT,
  OP_TRUE,
} OpCode;


/* Bytecodes can be opcodes, but they can also be other uint8 values, e.g.
   immediate values for arithmetic codes. */
typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  int* lines;
  ValueArray constants;
} Chunk;


void initChunk(Chunk* chunk);

int addConstant(Chunk* chunk, Value value);  // returns the index of the added constant

void writeChunk(Chunk* chunk, uint8_t code_byte, int line);

void freeChunk(Chunk* chunk);

#endif
