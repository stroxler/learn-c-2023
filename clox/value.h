#ifndef clox_value_h
#define clox_value_h

#include "common.h"


typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
  } data;
} Value;


#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define AS_BOOL(value) ((value).data.boolean)
#define AS_NUMBER(value) ((value).data.number)


typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;


void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

void printValue(Value value);

bool valueFalsey(Value value);
bool valueEqual(Value value0, Value value1);
bool valueLess(Value value0, Value value1);
bool valueGreater(Value value0, Value value1);


#endif
