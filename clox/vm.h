#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "value.h"
#include "chunk.h"
#include "table.h"


#define STACK_MAX 256


typedef struct {
  // bytecode + instruction pointer
  Chunk* chunk;
  uint8_t* ip;
  // value stack
  Value stack[STACK_MAX];
  Value* stack_top;
  // heap data
  Obj* objects;
  Table strings;
} VM;


// This is exposed so that object.c can use it.
void vmInsertObjectIntoHeap(Obj *object);
bool vmAddInternedString(ObjString* string);
ObjString* vmFindInternedString(const char* chars,
   			        int length, uint32_t hash);



typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void initVM();

InterpretResult interpret(const char* source);

void push(Value value);

Value peek(int distance);

Value pop();

void freeVM();

#endif
