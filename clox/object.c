#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"

#include "object.h"

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  return object;
}

#define ALLOCATE_OBJ(type, objectType) \
  (type*)allocateObject(sizeof(type), objectType)
  

ObjString* allocateString(char* chars, int length) {
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->chars = chars;
  string->length = length;
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


void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("\"%s\"", AS_CSTRING(value));
    break;
  }
}


bool objectEqual(Value value0, Value value1) {
  if (OBJ_TYPE(value0) != OBJ_TYPE(value1)) {
    return false;
  }
  switch (OBJ_TYPE(value0)) {
  case OBJ_STRING: {
    ObjString* string0 = AS_STRING(value0);
    ObjString* string1 = AS_STRING(value1);
    return (string0->length == string1->length &&
	    memcmp(string0->chars, string1->chars, string0->length) == 0);
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
