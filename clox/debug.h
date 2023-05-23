#ifndef clox_debug_h
#define clox_debug_h

#include "value.h"
#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
void printValue(Value value);
int disassembleInstruction(const char* tag, Chunk* chunk, int offset);

#endif
