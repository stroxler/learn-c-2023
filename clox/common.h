#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//#define PRINT_DEBUGGING
#ifdef PRINT_DEBUGGING
#define PRINT_DEBUG(...) printf(__VA_ARGS__)
#else
#define PRINT_DEBUG(...) ;
#endif

#define DEBUG_TRACE_EXECUTION
#define DEBUG_PRINT_CODE

#endif
