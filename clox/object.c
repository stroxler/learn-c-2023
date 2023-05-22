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
