#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "vm.h"

#include "object.h"


static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
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
    vmInsertObjectIntoHeap((Obj*)string);
    vmAddInternedString(string);
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
  case OBJ_CLOSURE: {
    ObjClosure* closure = AS_CLOSURE(value);
    printFunction(closure->function);
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
    fprintf(stderr, "Should be unreachable: unknown OBJ_TYPE in objectEqual");
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


/* Get a dummy function with mostly-empty data but an initialized chunk */
ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}


ObjClosure* newClosure(ObjFunction* function) {
  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  return closure;
}


void freeObject(Obj* object) {
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
  case OBJ_CLOSURE: {
    ObjClosure* closure = (ObjClosure*) object;
    // The function is not owned! We don't want to free it.
    //
    // Function objects contain the bytecode. They live
    // for the entire vm lifetime.
    //
    // omitted code: FREE(ObjFunction, closure->function);
    FREE(ObjClosure, closure);
    break;
  }
  }
}
