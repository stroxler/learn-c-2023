#include <stdio.h>
#include <stdarg.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "table.h"

#include "vm.h"


// The VM is a global static variable!
//
// We could refactor later to make it a regular varable, which
// would allow us to instantiate multiple vms.
VM vm;


void vmInsertObjectIntoHeap(Obj* object) {
  object->next = vm.objects;
  vm.objects = object;
}


bool vmAddInternedString(ObjString* string) {
  return tableSet(&vm.strings, string, NIL_VAL);
}


ObjString* vmFindInternedString(const char* chars, int length, uint32_t hash) {
  return tableFindString(&vm.strings, chars, length, hash);
}
			 


static void resetStack() {
  // no nead to actually clear the stack, just reset pointer. This
  // works because the stack is not a pointer but a plain array, inlined
  // directly in the vm and allocated as part of the struct.
  vm.stack_top = vm.stack;
}

void initVM() {
  resetStack();
  initTable(&vm.strings);
  initTable(&vm.globals);
}

static void runtimeError(const char* format, ...) {
  // Dump the raw message to stderr
  va_list args;
  va_start (args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  // Find the relevant line number and dump that as well
  //
  // Why the extra -1? Remember that one invariant is `ip` always
  // points at the *next* byte we would work with, not the one we are
  // currently processing (the READ_BYTE macro enforces this).
  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];
  fprintf(stderr, "[line %d]\n", line);

  // We're always going to abort execution when we hit a runtimeError
  // (we do it in the caller, boundaries often aren't super clean in
  // idiomatic C code, but by globally analyzing the interpreter you
  // can verify this). So we always want to reset the stack.
  resetStack();
}


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


Value peek(int distance) {
  return vm.stack_top[-1 - distance];
}


/* Macros for `run()`. We unset them after. */


#define READ_BYTE() (*vm.ip++)
// (note: ++ binds tighter than *!)


// read the next 2 bytes of bytecode as a big-endian 16-bit int
#define READ_SHORT() \
  (vm.ip += 2, (uint16_t)(vm.ip[-2] << 8) | vm.ip[-1])
// (recall that the comma operator throws away the LHS of an expression)


#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])


#define READ_STRING() AS_STRING(READ_CONSTANT())


// Expand a C binary op into a stack operation.
//
// Note that the top of the stack is always the RHS of the operation.
//
// This `do { ... } while (false)` pattern is a standard way to
// ensure that multi-line macros can be used when not wrapped in a block.
//
// Note that compile correctness ensures the stack will always have enough
// elements when we run this, a stack underflow can't happen unless our impl
// itself is buggy.
#define C_BINARY_NUMERIC_OP(valueType, op)	\
  do { \
    if (!IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) { \
      runtimeError("Operands must be numbers."); \
      return INTERPRET_RUNTIME_ERROR; \
    } \
    double b = AS_NUMBER(pop()); \
    double a = AS_NUMBER(pop()); \
    push(valueType(a op b)); \
  } while (false)


// (recall static means private, loosely speaking)
static InterpretResult run() {

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
    printf("trace:          stack: { ");
    for(Value* slot = vm.stack; slot < vm.stack_top; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf(" }\n");
    disassembleInstruction("trace:",
			   vm.chunk,
			   (int)(vm.ip - vm.chunk->code));
			   
    #endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    case OP_NIL:
      push(NIL_VAL); break;
    case OP_FALSE:
      push(BOOL_VAL(false)); break;
    case OP_TRUE:
      push(BOOL_VAL(true)); break;
    case OP_ADD: {
      // Unlike most other ops, OP_ADD is polymorphic over numbers and strings
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
	Value right = pop();
	Value left = pop();
	push(concatenateStrings(left, right));
      } else {
	C_BINARY_NUMERIC_OP(NUMBER_VAL, +);
      }
      break;
    }
    case OP_SUBTRACT:
      C_BINARY_NUMERIC_OP(NUMBER_VAL, -); break;
    case OP_MULTIPLY:
      C_BINARY_NUMERIC_OP(NUMBER_VAL, *); break;
    case OP_DIVIDE:
      C_BINARY_NUMERIC_OP(NUMBER_VAL, /); break;
    case OP_EQUAL:
      push(BOOL_VAL(valueEqual(pop(), pop()))); break;
    case OP_LESS:
      C_BINARY_NUMERIC_OP(BOOL_VAL, <); break;
    case OP_GREATER:
      C_BINARY_NUMERIC_OP(BOOL_VAL, >); break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0))) {
	runtimeError("Operand to negation must be a number.");
	return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    case OP_NOT:
      push(BOOL_VAL(valueFalsey(pop()) ? true : false));
      break;
    case OP_PRINT: {
      printValue(pop());
      printf("\n");
      break;
    }
    case OP_POP: {
      pop();
      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjString* name = READ_STRING();
      // Why do we peek and only then pop?
      //
      // Any time a value is ephemeral, i.e. used in the interpreter
      // loop (so any operation that consumes values), we can pop
      // freely. But if the value we are popping will continue to be live
      // in the program we have to be careful in case GC is triggered.
      //
      // As a result, we need to make sure the value is in globals *before*
      // we remove it from the stack since the GC will check both places
      // but it can't check values that are only accessible from raw C code.
      tableSet(&vm.globals, name, peek(0));
      pop();
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString* name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
	runtimeError("Undefined variable '%s'.", name->chars);
	return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }
    case OP_SET_GLOBAL: {
      ObjString* name = READ_STRING();
      if (tableSet(&vm.globals, name, peek(0))) {
	// Oops - we set a variable that wasn't declared!
	tableDelete(&vm.globals, name);
	runtimeError("Undefined variable '%s'.", name->chars);
	return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SET_LOCAL: {
      // One thing that seems weird here is that we never allocate a
      // slot for any opcode. That's because there *is* no opcode for
      // actually defining a local - we just push the value ot the stack!
      //
      // This opcode is only used when setting an already-existant local.
      uint8_t slot = READ_BYTE();
      vm.stack[slot] = peek(0);
      break;
    }
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(vm.stack[slot]);
      break;
    }
    case OP_RETURN: {
      return INTERPRET_OK;
      break;
    }
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      vm.ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      vm.ip -= offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      // NOTE: the compiler is responsible for popping this if
      // necessary; we retain it here because logical operators will
      // want it.
      if (valueFalsey(peek(0))) {
	vm.ip += offset;
      }
      break;
    }
    }
  }
}

/* unset the macros that are for use in `run` */
#undef READ_STRING
#undef READ_CONSTANT
#undef READ_BYTE
#undef READ_SHORT
#undef C_BINARY_NUMERIC_OP


InterpretResult interpretChunk(Chunk* chunk) {
  return run();
}


InterpretResult interpret(const char* source) {
  Chunk chunk;
  initChunk(&chunk);
  if (!compile(&chunk, source)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;
  InterpretResult result = run();

  vm.chunk = NULL;
  freeChunk(&chunk);
  return result;
}


void freeObjects() {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
}

void freeVM() {
  freeObjects();
  freeTable(&vm.globals);
  freeTable(&vm.strings);
}
