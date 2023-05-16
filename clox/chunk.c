#include <stdlib.h>

#include "common.h"
#include "memory.h"

#include "chunk.h"


void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
}


void writeChunk(Chunk* chunk, uint8_t code_byte) {
  if (chunk->capacity < chunk->count + 1) {
    int old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
  }
  chunk->code[chunk->count] = code_byte;
  chunk->count++;
}
