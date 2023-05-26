#include <stdlib.h>

#include "common.h"
#include "memory.h"
#include "value.h"

// (only needed to push values for gc safety; we could alternatively
//  add a hook in the compiler to register an extra "temp" gc root)
#include "vm.h"

#include "chunk.h"


void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}


#include <stdio.h>


int addConstant(Chunk* chunk, Value value) {
  push(value);
  printf("pushed value "); printValue(value); printf("\n");
  writeValueArray(&chunk->constants, value);
  printf("wrote value "); printValue(value); printf("\n");
  pop();
  return chunk->constants.count - 1;
}


void writeChunk(Chunk* chunk, uint8_t code_byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
    chunk->lines = GROW_ARRAY(int, chunk->lines, old_capacity, chunk->capacity);
  }
  chunk->code[chunk->count] = code_byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}


void freeChunk(Chunk* chunk) {
  freeValueArray(&chunk->constants);
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);
  initChunk(chunk);
}
