#include <stdio.h>

#include "common.h"

#include "debug.h"


void disassembleChunk(Chunk* chunk, const char* name) {
  printf("=== %s ===\n", name);

  for (int offset = 0; offset < chunk->count;) {
    // Note: instructions may take more than one slot (instruction width
    // is not fixed). As a result, we can't simply bump offset in the for
    // loop header.
    offset = disassembleInstruction(chunk, offset);
  }
}


int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}


int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
