#ifndef clisp_table_h
#define clisp_table_h

#include "common.h"
#include "value.h"
#include <stdint.h>

// Key-Value pair that has been added to the Table.
typedef struct {
    // All keys are stored as strings because they're the only type that has
    // hashing implemented for it.
    Value key;

    // Value retrieved by the key when getting/setting in the Table.
    Value value;
} Entry;

// Data of Hash Table implementation, operated by associated functions.
typedef struct {
    // Number of entries in Table.
    int count;

    // Maximum spaces available in Table.
    int capacity;

    // Pointer to first Entry slot in Array.
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, Value key, Value* value);
bool tableSet(Table* table, Value key, Value value);
bool tableDelete(Table* table, Value key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars,
    int length, uint32_t hash);
void markTable(Table* table);
void tableRemoveWhite(Table* table);
bool hashOf(Value* value, uint32_t* result);

#endif
