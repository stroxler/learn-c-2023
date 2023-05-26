#include <stdlib.h>
#include <stdio.h>

#include "object.h"
#include "value.h"
#include "compiler.h"
#include "vm.h"

#include "memory.h"


// NOTE: I use static data for the markstack, instead of the book's `vm.greyStack`.
//
// This also means my markstack lives until the end of the program (the book frees it
// inside freeVM())
int markstackCapacity = 0;
int markstackCount = 0;
Obj** markstack = NULL;


void* reallocate(void* pointer, size_t old_size, size_t new_size) {
  #ifdef DEBUG_STRESS_GC
  if (new_size > old_size) { collectGarbage(); }
  #endif
  if (new_size == 0) {
    free(pointer);
    return NULL;
  }
  void* result = realloc(pointer, new_size);
  if (result == NULL) {
    fprintf(stderr, "clox: out of memory, exiting now %s:%d", __FILE__, __LINE__);
    exit(1);
  }
  return result;
}


void addToMarkstack(Obj* object) {
  // Make sure the markstack has room
  if (markstackCapacity < markstackCount + 1) {
    markstackCapacity = GROW_CAPACITY(markstackCapacity);
    markstack = (Obj**)realloc(markstack,
			       sizeof(Obj*) * markstackCapacity);
    // Note that we used realloc, not reallocate: we can't trigger
    // another GC run inside of a GC run!
    if (markstack == NULL) {
      fprintf(stderr, "clox: out of memory inside markstack, exiting now %s:%d", __FILE__, __LINE__);
      exit(1);
    }
				 
  }
  // Add the object to the worklist
  markstack[markstackCount++] = object;
}


void markObject(Obj* object) {
  if (object == NULL || object->isMarked) {
    return;
  }
#ifdef DEBUG_LOG_GC
  printf("        %p mark ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  object->isMarked = true;
  // add to the worklist (the book calls this the "grey stack")
  addToMarkstack(object);
}


void markValue(Value value) {
  if (IS_OBJ(value)) {
    markObject(AS_OBJ(value));
  }
}


static void markRoots() {
  markVmRoots();
  GC_LOG("   --- mark vm / mark compiler ---\n");
  markCompilerRoots();
}


void markValueArray(ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}


void traceObjectReferences(Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("        %p trace-object-references ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  switch(object->type) {
  case OBJ_STRING:
    // Strings don't contain references to other objects; they do
    // contain data but it's all in the intern table which is
    // special-cased.
    break;
  case OBJ_UPVALUE: {
    markValue(((ObjUpvalue*)object)->closed);  // (this is just NIL if open)
    break;
  }
  case OBJ_FUNCTION: {
    // Functions contain:
    // - a string for their name
    // - an array of constants (this is most of the compiler-generated data)
    ObjFunction* function = (ObjFunction*)object;
    markObject((Obj*)function->name);
    markValueArray(&function->chunk.constants);
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure* closure = (ObjClosure*)object;
    markObject((Obj*)closure->function);
    for (int i=0; i < closure->upvalueCount; i++) {
      markObject((Obj*)closure->upvalues[i]);
    }
    break;
  }
  }
}


static void traceReferences() {
  while (markstackCount > 0) {
    Obj* object = markstack[--markstackCount];
    traceObjectReferences(object);
  }
}


void collectGarbage() {
  GC_LOG("------ GC BEGIN ------\n");
  markRoots();
  GC_LOG("  ---- mark roots / trace ----\n");
  traceReferences();
  GC_LOG("  ---- trace / sweep ----\n");
  sweepVmObjects();
  GC_LOG("------ GC END ------\n");
}
