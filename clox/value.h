#ifndef clox_value_h
#define clox_value_h

#include "common.h"

/*
  In Lox, values themselves are "raw" structs which will
  generally be stored either in stack- or static-lifetime
  variables.

  This allows us to efficiently access fixed-size primitives:
  booleans, numbers, and 

  All variable-size objects are represented by values that hold an
  Obj* pointer inside them. The Obj struct itself will be a tagged
  enum for the different kinds of dynamically-sized data we might
  have.
  
  The Obj struct is defined in object.h.
  - The dependency structure is a bit weird because there's a cycle;
    value and object depend on one another.
  - As a result, we need forward declarations here.
 */

// Forward declaration - see object.h for actual definition.
//
// Obj is a base struct and the others inherit from it (via embedding)
typedef struct Obj Obj;
typedef struct ObjString ObjString;


typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* object;
  } data;
} Value;


#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(value) ((Value){VAL_OBJ, {.object = (Obj*)value}})

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_NUMBER)

#define AS_BOOL(value) ((value).data.boolean)
#define AS_NUMBER(value) ((value).data.number)
#define AS_OBJ(value) ((value).data.object)


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
