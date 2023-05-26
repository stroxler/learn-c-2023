#ifndef clox_compiler_h
#define clox_compiler_h


#include "object.h"
#include "chunk.h"

void markCompilerRoots();

ObjFunction* compile(const char* source);


#endif
