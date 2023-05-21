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

  // Line numbers are right-justified with 4 spaces. Use a | for
  // opcodes whose line is the same as the previous one.
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
  }
  
  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simpleInstruction("OP_DIVIDE", offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
