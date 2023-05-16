#include <stdio.h>

#include "common.h"
#include "debug.h"

#include "vm.h"


// The VM is a global static variable!
//
// We could refactor later to make it a regular varable, which
// would allow us to instantiate multiple vms.
VM vm;


void initVM() {
}


// (recall static means private, loosely speaking)
static InterpretResult run() {
  // local macros to read data out of the chunk
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
    disassembleInstruction(vm.chunk,
			   (int)(vm.ip - vm.chunk->code));
			   
    #endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      printValue(constant);
      printf("\n");
      break;
    }
    case OP_RETURN: {
      return INTERPRET_OK;
    }
    }
  }

  // unset the local macro
#undef READ_CONSTANT
#undef READ_BYTE
}


InterpretResult interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
}


void freeVM() {
}
