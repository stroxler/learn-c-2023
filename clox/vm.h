#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "value.h"
#include "chunk.h"
#include "table.h"


#define FRAMES_MAX 64
// We get UINT8_COUNT locals *per frame*, because
// locals are looked up using a uint8_t offset against
// each frame pointer.
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)
// technically we don't actually have robust checking against
// too many temporary variables, it is *possible* to stack overflow
// (see the note in Section 24.3 about temporaries overflowing)

typedef struct {
  ObjClosure* closure;
  uint8_t* ip;
  Value* slots;  // frame pointer into vm.stack
} CallFrame;


typedef struct {
  // frame stack
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  // value stack
  Value stack[STACK_MAX];
  Value* stack_top;
  // open upvalues: captures currently pointing at the stack
  ObjUpvalue* openUpvalues;
  // heap data
  Obj* objects;
  Table strings;
  Table globals;
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

// GC hooks (driven by memory.h code)
void markVmRoots();
void sweepVmObjects();

InterpretResult interpret(const char* source);

void push(Value value);

Value peek(int distance);

Value pop();

void freeVM();

#endif
