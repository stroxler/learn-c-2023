#include <stdio.h>

#include "common.h"
#include "debug.h"

#include "vm.h"


// The VM is a global static variable!
//
// We could refactor later to make it a regular varable, which
// would allow us to instantiate multiple vms.
VM vm;


static void resetStack() {
  // no nead to actually clear the stack, just reset pointer. This
  // works because the stack is not a pointer but a plain array, inlined
  // directly in the vm and allocated as part of the struct.
  vm.stack_top = vm.stack;
}

void initVM() {
  resetStack();
}

/* Macros for `run()`. We unset them after. */


#define READ_BYTE() (*vm.ip++)


#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])


// Expand a C binary op into a stack operation.
//
// Note that the top of the stack is always the RHS of the operation.
//
// This `do { ... } while (false)` pattern is a standard way to
// ensure that multi-line macros can be used when not wrapped in a block.
#define C_BINARY_OP(op) \
  do { \
    double b = pop(); \
    double a = pop(); \
    push(a op b); \
  } while (false)


// (recall static means private, loosely speaking)
static InterpretResult run() {

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
    printf("          stack: { ");
    for(Value* slot = vm.stack; slot < vm.stack_top; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf(" }\n");
    disassembleInstruction(vm.chunk,
			   (int)(vm.ip - vm.chunk->code));
			   
    #endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    case OP_ADD:
      C_BINARY_OP(+); break;
    case OP_SUBTRACT:
      C_BINARY_OP(-); break;
    case OP_MULTIPLY:
      C_BINARY_OP(*); break;
    case OP_DIVIDE:
      C_BINARY_OP(/); break;
    case OP_NEGATE:
      push(-pop()); break;
    case OP_RETURN: {
      printf("\nRETURN: ");
      printValue(pop());
      printf("\n");
      return INTERPRET_OK;
    }
    }
  }

}

/* unset the macros that are for use in `run` */
#undef READ_CONSTANT
#undef READ_BYTE
#undef C_BINARY_OP


void push(Value value) {
  // Note that if we were defensive, we'd be checking for stack
  // overflow and underflow in push / pop. That would catch errors in
  // our own implementation; the user stack is something separate
  // and I'm not talking about a user-facing recursion error.
  *vm.stack_top = value;
  // Note that C will adjust pointer arithmetic depending on the type
  // (with a void pointer this won't work out of the box, but here it does)
  vm.stack_top++;
}


Value pop() {
  // Recall that stack_top always points to the next *available* slot.
  // So we can decrement first then dereference
  vm.stack_top--;
  return *vm.stack_top;
}


InterpretResult interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
}


void freeVM() {
}
