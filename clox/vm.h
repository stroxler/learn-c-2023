#ifndef clox_vm_h
#define clox_vm_h

#include "value.h"
#include "chunk.h"


#define STACK_MAX 256


typedef struct {
  // bytecode + instruction pointer
  Chunk* chunk;
  uint8_t* ip;
  // value stack
  Value stack[STACK_MAX];
  Value* stack_top;
} VM;


typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void initVM();

InterpretResult interpret(const char* source);

void push(Value value);

Value pop();

void freeVM();

#endif
