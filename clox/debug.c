#include <stdio.h>

#include "common.h"

#include "debug.h"
#include "object.h"
#include "value.h"


void disassembleChunk(Chunk* chunk, const char* name) {
  printf("=== %s ===\n", name);
  for (int offset = 0; offset < chunk->count;) {
    // Note: instructions may take more than one slot (instruction width
    // is not fixed). As a result, we can't simply bump offset in the for
    // loop header.
    offset = disassembleInstruction("", chunk, offset);
  }
  printf("=== %s ===\n", name);
}


int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

int constantInstruction(const char* name, Chunk* chunk, int offset) {
  // opcode should be left-justified with 16 columns of space
  // the constant prints as it's index plus (in single quotes) the value.
  uint8_t constant_index = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant_index);
  printValue(chunk->constants.values[constant_index]);
  printf("'\n");
  return offset + 2;
}


int byteInstruction(const char* name, Chunk* chunk, int offset) {
  // opcode should be left-justified with 16 columns of space
  uint8_t stack_index = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, stack_index);
  return offset + 2;
}


int jumpInstruction(const char* name, Chunk* chunk, int offset) {
  // opcode should be left-justified with 16 columns of space
  uint16_t address = (uint16_t)(chunk->code[offset + 1] << 8);
  address |= chunk->code[offset + 2];
  printf("%-16s %4d\n", name, address);
  return offset + 3;
}


int closureInstruction(Chunk* chunk, int offset) {
  // TODO: at the moment this is basically the same as
  // constantInstruction, but it will ge more elaborate by the time we
  // finish with closure.
  uint8_t constant_index = chunk->code[offset + 1];
  printf("%-16s %4d '", "OP_CLOSURE", constant_index);
  Value value = chunk->constants.values[constant_index];
  ObjFunction* function = AS_FUNCTION(value);
  printValue(value);
  printf("'\n");
  int i = 0;
  for (; i < function->upvalueCount; i++) {
    int isLocal = chunk->code[offset + 2 * i + 2];
    int index = chunk->code[offset + 2 * i + 3];
    printf("%04d      |                     %s %d\n",
	   offset - 2, isLocal ? "local" : "not-local", index);
  }
  return offset + 2 * (i + 1);
}


int disassembleInstruction(const char* tag, Chunk* chunk, int offset) {
  printf("%s %04d ", tag, offset);

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
  case OP_DEFINE_GLOBAL:
    return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_GET_GLOBAL:
    return constantInstruction("OP_GET_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL:
    return constantInstruction("OP_SET_GLOBAL", chunk, offset);
  case OP_GET_LOCAL:
    return byteInstruction("OP_GET_LOCAL", chunk, offset);
  case OP_SET_LOCAL:
    return byteInstruction("OP_SET_LOCAL", chunk, offset);
  case OP_GET_UPVALUE:
    return byteInstruction("OP_GET_UPVALUE", chunk, offset);
  case OP_SET_UPVALUE:
    return byteInstruction("OP_SET_UPVALUE", chunk, offset);
  case OP_CLOSE_UPVALUE:
    return simpleInstruction("OP_CLOSE_UPVALUE", offset);
  case OP_NIL:
    return simpleInstruction("OP_NIL", offset);
  case OP_FALSE:
    return simpleInstruction("OP_FALSE", offset);
  case OP_TRUE:
    return simpleInstruction("OP_TRUE", offset);
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
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);
  case OP_NOT:
    return simpleInstruction("OP_NOT", offset);
  case OP_POP:
    return simpleInstruction("OP_POP", offset);
  case OP_PRINT:
    return simpleInstruction("OP_PRINT", offset);
  case OP_LOOP:
    return jumpInstruction("OP_LOOP", chunk, offset);
  case OP_JUMP:
    return jumpInstruction("OP_JUMP", chunk, offset);
  case OP_JUMP_IF_FALSE:
    return jumpInstruction("OP_JUMP_IF_FALSE", chunk, offset);
  case OP_CALL:
    return byteInstruction("OP_CALL", chunk, offset);
  case OP_CLOSURE:
    return closureInstruction(chunk, offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
