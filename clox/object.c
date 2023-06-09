#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "vm.h"

#include "object.h"


const char* typeName(ObjType type) {
  switch(type){
  case OBJ_STRING: return "OBJ_STRING";
  case OBJ_FUNCTION: return "OBJ_FUNCTION";
  case OBJ_CLOSURE: return "OBJ_CLOSURE";
  case OBJ_UPVALUE: return "OBJ_UPVALUE";
  }
}

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = false;
  GC_LOG("%p allocate (size %zu) of type %s\n", (void*)object, size, typeName(type));
  vmInsertObjectIntoHeap(object);
  return object;
}


#define ALLOCATE_OBJ(type, objectType) \
  (type*)allocateObject(sizeof(type), objectType)


Table interned_strings;


/* Implement FNV-1a hash */
uint32_t hashChars(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t) key[i];
    hash *= 16777619;
  }
  return hash;
}


ObjString* allocateString(char* chars, int length) {
  uint32_t hash = hashChars(chars, length);
  ObjString* string = vmFindInternedString(chars, length, hash);
  if (string == NULL) {
    string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->chars = chars;
    string->length = length;
    string->hash = hashChars(chars, length);
    // adding the string to the table could trigger a GC, so guard
    // with a push / pop.
    push(OBJ_VAL(string));
    vmAddInternedString(string);
    pop();
  }
  return string;
}


// Given a segment of a C-string (by a segment, I mean not '\0'
// terminated, just a bunch of characters), create an ObjString
// pointer where the underlying chars are a valid C-string.
//
// The underlying char* data is copied, both to simplify
// lifetime handling and to let us ensure all strings can be passed
// to C library functions expecting C-strings.
ObjString* createString(const char* segment_start, int length) {
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, segment_start, length);
  chars[length] = '\0';
  return allocateString(chars, length);
}


static void printFunction(ObjFunction* function) {
  if (function->name == NULL) {
    printf("<fn top-level>");
  } else {
    printf("<fn %s>", function->name->chars);
  }
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING: {
    printf("\"%s\"", AS_CSTRING(value));
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction* function = AS_FUNCTION(value);
    printFunction(function);
    break;
  }
  case OBJ_UPVALUE: {
    printf("(upvalue)");
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure* closure = AS_CLOSURE(value);
    printf("closure(");
    printFunction(closure->function);
    printf(")");
    break;
  }
  }
}


bool objectEqual(Value value0, Value value1) {
  if (OBJ_TYPE(value0) != OBJ_TYPE(value1)) {
    return false;
  }
  switch (OBJ_TYPE(value0)) {
  case OBJ_STRING: {
    // This works because of string interning - the comparison
    // work is done upfront in tableFindString.
    return AS_STRING(value0) == AS_STRING(value1);
  }
  default:
    // TODO we should probably compare the Obj* pointers here - if the underlying
    // values are actually identical (as pointers) then they are equal.
    fprintf(stderr, "Comparison of non-string objects isn't really supported");
    return false;

  }
}


Value concatenateStrings(Value left, Value right) {
  ObjString* lstr = AS_STRING(left);
  ObjString* rstr = AS_STRING(right);

  int length = lstr->length + rstr->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, lstr->chars, lstr->length);
  memcpy(chars + lstr->length, rstr->chars, rstr->length);
  chars[length] = '\0';

  // Wrap the chars in a string struct, and return a Value pointing to it.
  ObjString* string = allocateString(chars, length);
  Value value = OBJ_VAL(string);
  return value;
}


/* Get a dummy function with mostly-empty data but an initialized chunk.

   Note that this is called in the *compiler*, not the interpreter -
   functions are treated as runtime values, but they are purely static
   (i.e. constant) and all of them are reachable from the top-level
   Chunk. */
ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->name = NULL;
  function->upvalueCount = 0;
  initChunk(&function->chunk);
  return function;
}


ObjUpvalue* newUpvalue(Value* value) {
  ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->location = value;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}


/* Wrap an ObjFunction in a closure. This is a runtime-only operation;
   for the top-level script we make a dummy closure in `interpret`
   whereas other closures are created by the OP_CLOSURE we emit for
   function declarations.

   The purpose of the wrapper is to have a place for upvalues. */
ObjClosure* newClosure(ObjFunction* function) {
  // The upvalue count is known statically, so we can pre-allocate
  // and pre-initialize the pointer slots here.
  //
  // The contents will be filled out as part of the same OP_CLOSURE
  // execution where we construct this, but that has to be done inside
  // of run() so we can access the vm stack.
  //
  // Note that this has to run *before* we allocate the closure,
  // because the ALLOCATE could trigger a GC so we don't want
  // the closure to already exist in the heap.
  //
  // Note that this is an empty array of ObjUpvalue* _pointers_,
  // there are no actual objects here. So therere's no risk of the
  // upvalues being GC'd, they don't exist yet :)
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }
  // Now that there will be no more ALLOCATE calls, we can go ahead
  // and create the closure.
  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}


void freeObject(Obj* object) {
  GC_LOG("%p free type %s\n", (void*)object, typeName(object->type));
  switch (object->type) {
  case OBJ_STRING: {
    ObjString* string = (ObjString*) object;
    FREE_ARRAY(char, string->chars, string->length + 1);
    FREE(ObjString, string);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction* function = (ObjFunction*) object;
    freeChunk(&function->chunk);
    FREE(ObjFunction, function);
    break;
  }
  case OBJ_UPVALUE: {
    ObjUpvalue* upvalue = (ObjUpvalue*) object;
    // Do *not* free the next upvalue: the VM owns the linked
    // list and will handle lifetimes!
    FREE(ObjUpvalue, upvalue);
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure* closure = (ObjClosure*) object;
    // The function is not owned! We don't want to free it.
    //
    // Function objects contain the bytecode. They live
    // for the entire vm lifetime.
    //
    // omitted code: FREE(ObjFunction, closure->function);
    FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
    FREE(ObjClosure, closure);
    break;
  }
  }
}
