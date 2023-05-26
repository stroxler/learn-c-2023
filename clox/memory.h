#ifndef clox_memory_h
#define clox_memory_h

#include "object.h"
#include "value.h"
#include "common.h"


#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)


#define GROW_ARRAY(type, pointer, old_count, new_count) \
  (type*)reallocate(pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))


#define FREE_ARRAY(type, pointer, old_count) \
  reallocate(pointer, sizeof(type*) * (old_count), 0)


void* reallocate(void* pointer, size_t old_size, size_t new_size);

// why are these exposed? Because we rely on inlined mark helpers
// for table.c and compiler.c that need access to them.

void markObject(Obj* object);
void markValue(Value value);

void collectGarbage();

#define ALLOCATE(type, size) \
  reallocate(NULL, 0, sizeof(type) * size)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#endif
