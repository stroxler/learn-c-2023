#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "memory.h"
#include "value.h"
#include "chunk.h"


typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
} ObjType;


const char* typeName(ObjType type);


// Obj is a base struct.
//
// We'll use struct inheritance for subtypes: the first
// field of each concrete type is an Obj, which means that
// we can always get to the type, and we can also:
//
// - upcast any child, which just accesses the first part
// - downcast any base after checkign the type tag
//
// This is one way of implementing a wide-open tagged enum
// where the different enum values have very different data,
// without introducing extra layers of pointers.


// Note the non-typedef form here - the typedef was a forward declaration in value.h
struct Obj {
  ObjType type;
  bool isMarked;
  Obj* next;  // objects form a linked list so the vm can keep track of all heap data
};



// Note the non-typedef form here - the typedef was a forward declaration in value.h
struct ObjString {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash;
};


// This one, on the other hand is not forward-declared
//
// Note that the chunk is built-in, not a pointer (the underlying
// bytes are themselves a pointer, chunk is just the metadata)
typedef struct {
  Obj obj;
  int arity;
  Chunk chunk;
  ObjString* name;
  int upvalueCount;
} ObjFunction;


typedef struct ObjUpvalue {
  Obj obj;
  Value* location;
  // This is only populated for closed upvalues - it starts out nil.
  Value closed;
  // Hook for the vm to create a linked list of open upvalues that
  // haven't yet been moved to the heap.
  struct ObjUpvalue* next;
} ObjUpvalue;


typedef struct {
  Obj obj;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;


ObjString* createString(const char* segment_start, int length);


/* Macros for working with objects.

   All of these take Value (not Obj or Obj*) as an input!
 */


#define OBJ_TYPE(value) (AS_OBJ(value)->type)


// This can't be a macro because `value` is used more than once.
static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}


#define IS_STRING(value) (isObjType(value, OBJ_STRING))
#define IS_FUNCTION(value) (isObjType(value, OBJ_FUNCTION))
#define IS_UPVALUE(value) (isObjType(value, OBJ_CLOSURE))
#define IS_CLOSURE(value) (isObjType(value, OBJ_CLOSURE))


#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_UPVALUE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))


/* Helper functions for objects. Again, these take a Value as input */

void printObject(Value value);
bool objectEqual(Value value0, Value value1);
Value concatenateStrings(Value left, Value right);

ObjFunction* newFunction();
ObjUpvalue* newUpvalue(Value* value);
ObjClosure* newClosure(ObjFunction* function);

/* Helper function for the VM to garbage collect objects.

   Note that this does *not* take a Value, because it is a heap-only
   action and only the Obj part of an object value is on the heap */
void freeObject(Obj* object);

#endif
