#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

#include "table.h"


#define TABLE_MAX_LOAD 0.75


/* This is very similar to FindEntry, except that:
   - it only deals with keys, not values (we're using our table as a
     hashset here).
   - it takes the components of an ObjString as input rather than
     an actual ObjString because it runs *before* we create the struct

   It could in theory get away with skipping tombstones if we used
   append-only interning, but the clox implementation does garbage-collect
   unused strings so we need to support tombstones.
*/
ObjString* tableFindString(Table* strings, const char*
			   chars, int length, uint32_t hash) {
  if (strings->count == 0) {
    return NULL;
  }
  uint32_t index = hash % strings->capacity;
  for (;;) {
    Entry* entry = &strings->entries[index];
    if (entry->key == NULL) {
      // Skip over tombstones. The first actual empty entry we find indicates
      // that this is a new string.
      if (IS_NIL(entry->value)) {
	return NULL;
      } else {
	; // skip over tombstones
      }
    } else if (entry->key->length == length &&
	       memcmp(entry->key->chars, chars, length) == 0) {
      return entry->key;
    }
    index = (index + 1) % strings->capacity;
  }
}


void initTable(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}


void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}


void markTable(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    // Note that these can be NULL; that's okay b/c markObject handles NULL
    markObject((Obj*)entry->key);
    markValue(entry->value);
  }
}


void tableDeleteUnmarkedKeys(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->obj.isMarked) {
      tableDelete(table, entry->key);
    }
  }
}


/* Find an entry, if there is one.  This function is not safe
   to call unless we know the table has capacity. In general, conditions
   for it to work:
   - we need to make sure the table isn't empty
   - all table additions need to check load and potentially adjust capacity

   Note that the key == key check only works because we intern the keys in
   object.h / object.c.
*/
Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
  uint32_t index = key->hash % capacity;
  Entry* tombstone = NULL;
  for (;;) {
    Entry* entry = &entries[index];
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
	// If we find an emtpy entry, the key is not present. Return the
	// first tombstone we found if any, otherwise that empty entry.
        return tombstone != NULL ? tombstone : entry;
      } else {
	// set the tombstone the first time we find a deleted entry
	if (tombstone == NULL) {
	  tombstone = entry;
	}
      }
    } else if (entry->key == key) {
      return entry;
    }
    index = (index + 1) % capacity;  // linear scan with wraparound on collisions
  }
}


void adjustCapacity(Table* table, int capacity) {
  // create a new array (contents will be undefined!)
  Entry* entries = ALLOCATE(Entry, capacity);
  // initialize the array with empty entries - this ensures no undefined behavior
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }
  // Copy over data.
  //
  // Note that we skip tombstones, and so we want to reset the count.
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* source = &table->entries[i];
    if ((source->key) != NULL) {
      Entry* dest = findEntry(entries, capacity, source->key);
      dest->key = source->key;
      dest->value = source->value;
      table->count++;
    }
  }
  // free the old entries' memory
  FREE_ARRAY(Entry, table->entries, table->capacity);
  // only now overwrite the existing table data
  table->entries = entries;
  table->capacity = capacity;
}


bool tableSet(Table* table, ObjString* key, Value value) {
  // resize if necessary
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }
  // Find the right entry. Note that it may be an empty entry, but
  // we'll always get something because the entries are pre-populated.
  Entry* entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == NULL;
  // increment the count, but only if we didn't re-occupy a tombstone
  if (isNewKey && IS_NIL(entry->value)) {
    table->count++;
  }
  // Set the entry value.
  entry->key = key;
  entry->value = value;
  return isNewKey;
}


bool tableGet(Table* table, ObjString* key, Value* valueInOut) {
  if (table->count == 0) {
    return false;
  }
  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) {
    return false;
  }
  *valueInOut = entry->value;
  return true;
}


bool tableDelete(Table* table, ObjString* key) {
  if (table->count == 0) {
    return false;
  }
  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) {
    return false;
  }
  // Tombstone the entry.
  //
  // Note that `findEntries` depends on these tombstones, which are
  // distinct (boolean true vs nil) from initial, empty entries.
  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
  // Note that we never decreased the count. This is intentional:
  // tombstones aren't really empty from the standpoint of findEntries
  // termination, so we treat them as if they were full from a counting
  // perspective.
}


void tableAddAll(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if ((entry->key) != NULL) {
      tableSet(to, entry->key, entry->value);
    }
  }
}
