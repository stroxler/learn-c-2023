#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
  Chunk chunk;
  initChunk(&chunk);

  // Write a constant (once we have a vm, this would push it to the stack)
  //
  // Note that this isn't really type-safe: we're casting an int to uint8_t here, if
  // we have too many constants we will overflow.
  int constant_index = addConstant(&chunk, 42.0);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant_index, 123);

  // write a return op
  writeChunk(&chunk, OP_RETURN, 123);

  disassembleChunk(&chunk, "Initial example chunk");

  freeChunk(&chunk);
  return 0;
}
