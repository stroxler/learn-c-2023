#include <stdlib.h>
#include <stdio.h>

#include "memory.h"


void* reallocate(void* pointer, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    free(pointer);
    return NULL;
  }
  void* result = realloc(pointer, new_size);
  if (result == NULL) {
    fprintf(stderr, "clox: out of memory, exiting now %s:%d", __FILE__, __LINE__);
    exit(1);
  }
  return result;
}
