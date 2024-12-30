#ifndef clisp_table_h
#define clisp_table_h

#include "common.h"
#include "value.h"
#include <stdint.h>

// Key-Value pair that has been added to the Table.
typedef struct {
    // All keys are stored as strings because they're the only type that has
    // hashing implemented for it.
    ObjString* key;

    // Value retrieved by the key when getting/setting in the Table.
    Value value;
} Entry;

// Data of Hash Table implementation, operated by associated functions.
typedef struct {
    int count; // Number of entries in Table.
    int capacity; // Maximum spaces available in Table.
    Entry* entries; // Pointer to first Entry slot in Array.
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, 
        int length, uint32_t hash);

#endif
