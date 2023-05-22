#include <stdio.h>

#include "memory.h"

#include "object.h"
#include "value.h"


void initValueArray(ValueArray* array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}


void writeValueArray(ValueArray* array, Value value) {
  if (array->capacity < array->count + 1) {
    int old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
  }
  array->values[array->count] = value;
  array->count++;
}


void freeValueArray(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->count);
  initValueArray(array);
}


void printValue(Value value) {
  switch (value.type) {
  case VAL_BOOL:
    printf(AS_BOOL(value) ? "true" : "false");
    break;
  case VAL_NIL:
    printf("nil");
    break;
  case VAL_NUMBER:
    // %g is a "smart" formatter that uses scientific notation in some
    // %cases.
    printf("%g", AS_NUMBER(value));
    break;
  case VAL_OBJ:
    printObject(value);
    break;
  }
}


bool valueFalsey(Value value) {
  return (IS_NIL(value) ||
	  (IS_BOOL(value) && !AS_BOOL(value)));
}


bool valueEqual(Value value0, Value value1) {
  // Note: the author suggests that memcmp seems like a suitable option
  // here at least when we only have doubles and bools (that wouldn't
  // have occurred to me!), but points out that the content of unused
  // bits in a union is undefined - as a result, when values are
  // actually unequal it would work fine but when they are equal we'd
  // get nondeterministic behavior depending on a combination of the
  // types and the random content of undefined memory locations.
  //
  // In addition, memcmp wouldn't work for types that hold pointers,
  // which we will eventually need to handle
  if (value0.type != value1.type) {
    return false;
  }
  switch (value0.type) {
  case VAL_NIL:
    return true;
  case VAL_BOOL:
    return AS_BOOL(value0) == AS_BOOL(value1);
  case VAL_NUMBER:
    return AS_NUMBER(value0) == AS_NUMBER(value1);
  case VAL_OBJ:
    return objectEqual(value0, value1);
  default:
    fprintf(stderr, "Should be unreachable equality comparison!\n");
    return false;
  }
}
