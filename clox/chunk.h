#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"


typedef enum {
  OP_RETURN
} OpCode;


/* Bytecodes can be opcodes, but they can also be other uint8 values, e.g.
   immediate values for arithmetic codes. */
typedef struct {
  int count;
  int capacity;
  uint8_t* code;
} Chunk;


void initChunk(Chunk* chunk);

void writeChunk(Chunk* chunk, uint8_t code_byte);

#endif
