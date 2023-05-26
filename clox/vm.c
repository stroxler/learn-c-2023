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
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
  // Dump the raw message to stderr
  va_list args;
  va_start (args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // For each call frame (starting with the innermost, which is
  // at index vm.frameCount - 1)...
  for (int frameIndex = vm.frameCount - 1; frameIndex >= 0; frameIndex --) {
    // ...Find the relevant line number and dump that as well
    //
    // Why the extra -1? Remember that one invariant is `ip` always
    // points at the *next* byte we would work with, not the one we are
    // now. If we hit an error, it happened on the previously-used byte.
    CallFrame* frame = &vm.frames[frameIndex];
    size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
    int line = frame->closure->function->chunk.lines[instruction];
    const char* function_name = frame->closure->function->name == NULL ?
      "top-level" :
      frame->closure->function->name->chars;
    fprintf(stderr, "[line %d] in %s\n", line, function_name);
  }

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


/* Helpers for run() */

static bool call(ObjClosure* closure, uint8_t arg_count) {
  // Check that we can grab a call frame, then grab it
  if ((vm.frameCount >= FRAMES_MAX)) {
    runtimeError("Stack overflow (too many call frames).");
    return false;
  }
  if ((closure->function->arity != arg_count)) {
    runtimeError("Mismatch in argument count.");
    return false;
  }
  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stack_top - arg_count - 1;
  return true;
}


static bool callValue(Value callee, uint8_t arg_count) {
  if (IS_OBJ(callee) && IS_CLOSURE(callee)) {
    return call(AS_CLOSURE(callee), arg_count);
  } else {
    runtimeError("Can only call functions.");
    return false;
  }
}


static ObjUpvalue* captureUpvalue(Value* local) {
  // Search for an upvalue that already exists on this local.
  //
  // We can find it by just comparing the value pointer (which is a
  // stack slot). We keep the list reverse-ordered by stack position
  // so that searches are generally cheap.
  ObjUpvalue* upvalue = vm.openUpvalues;
  ObjUpvalue** insert_pointer = &vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    insert_pointer = &upvalue->next;
    upvalue = upvalue->next;
  }
  // At this point we either found the upvalue or we are ready to
  // insert a new one in front of `upvalue`.
  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  } else {
    ObjUpvalue* created = newUpvalue(local);
    *insert_pointer = created;  // (-> binds tighter than *)
    created->next = upvalue;
    return created;
  }
}


/* Close all open upvalues associated with this local or anything
   deeper in the stack ("deeper in the stack" matters when we end
   a function and reset the frame all at once with no OP_POPs).
   
   Note that all of them will live at the list head. */
static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL
	 && vm.openUpvalues->location > last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    // Copy the Value out of the stack and into `closed`, *moving*
    // any associated *Obj pointer (in the rust ownership sense).
    //
    // The VM heap is essentially composed of closed upvalues.
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = vm.openUpvalues->next;
  }
}



/* Macros for `run()`. We unset them after. */


#define READ_BYTE() (*frame->ip++)
// (note: ++ binds tighter than *!)


// read the next 2 bytes of bytecode as a big-endian 16-bit int
#define READ_SHORT() \
  (frame->ip += 2, (uint16_t)(frame->ip[-2] << 8) | frame->ip[-1])
// (recall that the comma operator throws away the LHS of an expression)


#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])


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

  // Grab the top frame.
  //
  // All locals are looked up relative to its frame offset,
  // and ip is now tracked per-frame.
  CallFrame* frame = &vm.frames[vm.frameCount - 1];

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
			   &frame->closure->function->chunk,
			   (int)(frame->ip - frame->closure->function->chunk.code));
			   
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
      frame->slots[slot] = peek(0);
      break;
    }
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t upvalue_slot = READ_BYTE();
      Value* location = frame->closure->upvalues[upvalue_slot]->location;
      *location = peek(0);
      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t upvalue_slot = READ_BYTE();
      Value* location = frame->closure->upvalues[upvalue_slot]->location;
      push(*location);
      break;
    }
    case OP_CLOSE_UPVALUE: {
      closeUpvalues(vm.stack_top - 1);
      pop();
      break;
    }
    case OP_RETURN: {
      // The result is on top of the stack. Get it, then reset frame.
      // Note that we need to be careful of the gc here.
      Value result = pop();
      // Close all the open upvalues pointing into the current stack
      // frame, then go ahead and remove the stack frame (exiting if
      // we're already at the top level).
      closeUpvalues(frame->slots);
      vm.frameCount--;
      if (vm.frameCount == 0) {
        return INTERPRET_OK;
      }
      // reset the stack top: next free slot should be
      // where the function was before. Push the return value there.
      vm.stack_top = frame->slots;
      push(result);
      // reset the current frame in run()
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      frame->ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      frame->ip -= offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      // NOTE: the compiler is responsible for popping this if
      // necessary; we retain it here because logical operators will
      // want it.
      if (valueFalsey(peek(0))) {
	frame->ip += offset;
      }
      break;
    }
    case OP_CLOSURE: {
      // this stores only the static data (bytecode + constants + name)
      ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
      ObjClosure* closure = newClosure(function);
      for (int i = 0; i < closure->upvalueCount; i++) {
	uint8_t isLocal = READ_BYTE();
	uint8_t index = READ_BYTE();
	// If isLocal, the capture is one layer up (i.e. the current frame)
	// and we may actually need to create an upvalue.
	//
	// Otherwise it's somewhere further up the call stack, and we want
	// to get the upvalue pointer from the current frame (they will be
	// tracked all the way to whatever scope the upvalue lives in,
	// across all intermediate frames).
	if (isLocal) {
	  closure->upvalues[i] = captureUpvalue(frame->slots + index);
	} else {
	  closure->upvalues[i] = frame->closure->upvalues[index];
	}
      }
      push(OBJ_VAL(closure));
      break;
    }
    case OP_CALL: {
      uint8_t arg_count = READ_BYTE();
      // Set up the call. This can fail (e.g. if the function is not
      // callable, if arg counts are mismatched). If it *is* successful,
      // it will append a frame to vm.frames and we then need to
      // bump the local frame in the `run()` loop.
      if (!callValue(peek(arg_count), arg_count)) {
	return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
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


InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  ObjClosure* top_level = newClosure(function);
  push(OBJ_VAL(top_level));
  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = top_level;
  frame->ip = function->chunk.code;
  frame->slots = vm.stack;

  return run();
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
