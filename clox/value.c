#include <stdio.h>

#include "memory.h"

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