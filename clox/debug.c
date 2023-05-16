#include <stdio.h>

#include "common.h"

#include "debug.h"
#include "value.h"


void disassembleChunk(Chunk* chunk, const char* name) {
  printf("=== %s ===\n", name);

  for (int offset = 0; offset < chunk->count;) {
    // Note: instructions may take more than one slot (instruction width
    // is not fixed). As a result, we can't simply bump offset in the for
    // loop header.
    offset = disassembleInstruction(chunk, offset);
  }
}


void printValue(Value value) {
  // %g is a "smart" formatter that uses scientific notation in some
  // %cases.
  printf("%g", value);
}

int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

int constantInstruction(const char*name, Chunk* chunk, int offset) {
  // opcode should be left-justified with 16 columns of space
  // the constant prints as it's index plus (in single quotes) the value.
  uint8_t constant_index = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant_index);
  printValue(chunk->constants.values[constant_index]);
  printf("'\n");
  return offset + 2;
}


int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
