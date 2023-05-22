#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"


typedef enum {
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_CONSTANT,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_EQUAL,
  OP_LESS,
  OP_GREATER,
  OP_NEGATE,
  OP_NOT,
  OP_RETURN,
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
