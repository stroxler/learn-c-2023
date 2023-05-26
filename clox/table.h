#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"


typedef struct {
  ObjString* key;
  Value value;
} Entry;


typedef struct {
  int count;
  int capacity;
  Entry* entries;
} Table;


ObjString* tableFindString(Table* table, const char* chars, int
			   length, uint32_t hash);

void initTable(Table* table);


void freeTable(Table* table);


void markTable(Table* table);

void tableDeleteUnmarkedKeys(Table* table);


bool tableSet(Table* table, ObjString* key, Value value);


bool tableGet(Table* table, ObjString* key, Value* valueInOut);


bool tableDelete(Table* table, ObjString* key);


void tableAddAll(Table* from, Table* to);


#endif
