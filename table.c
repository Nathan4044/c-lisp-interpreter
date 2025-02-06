#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdint.h>
#include <string.h>
#include "table.h"

// Maximum percentage of array slots filled before reallocating to a bigger
// underlying array.
#define TABLE_MAX_LOAD 0.75

// Instantiate a new hash table by setting all components to their zero value.
void initTable(Table *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

// Free all data that has been associated with a hash table.
void freeTable(Table *table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

// Find an entry in a hash table by searching its Entry array for a given key.
// Use the key's hash value as the first slot to check, then iterate through
// the array until reaching a fully empty slot (not a tombstone value) or the
// value with the matching key. Then returning either the matching entry, the
// first tombstone encountered, or the first empty slot.
static Entry* findEntry(Entry* entries, int capacity, Value key, uint32_t hash) {
    // Bitwise and of the hash value and 1 less than the Table's capacity.
    //
    // For capacities that are powers of 2, this is equivalent to:
    // uint32_t index = hash % capacity;
    // But significantly more CPU efficient (modulo is slow).
    uint32_t index = hash & (capacity - 1);

    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];

        if (IS_NULL(entry->key)) {
            if (IS_NULL(entry->value)) {
                // empty entry
                // return tombstone if one found
                return tombstone != NULL ? tombstone : entry;
            } else {
                // tombstone found
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (valuesEqual(entry->key, key)) {
            // entry found
            return entry;
        }

        // Faster than modulo for powers of two. See index declaration.
        index = (index + 1) & (capacity - 1);
    }
}

bool hashOf(Value* value, uint32_t* result) {
#ifdef NAN_BOXING
    if (IS_BOOL(*value)) {
        *result = (uint32_t)AS_BOOL(*value);
    } else if (IS_NUMBER(*value)) {
        *result = (uint32_t)AS_NUMBER(*value);
    } else if (IS_STRING(*value)) {
        *result = AS_STRING(*value)->hash;
    } else {
        return false;
    }

    return true;
#else
    switch (value->type) {
        case VAL_BOOL:
            *result = (uint32_t)value->as.boolean;
            return true;
        case VAL_NUMBER:
            *result = (uint32_t)value->as.number;
            return true;
        case VAL_OBJ: {
            if (!IS_STRING(*value)) return false;

            *result = AS_STRING(*value)->hash;
            return true;
        }
        case VAL_NULL:
            return false;
    }
#endif
}

// Retrieve the value in the table associated with the given key, and place it
// in the provided value address. Return false if the value could not be
// retrieved, otherwise true.
bool tableGet(Table *table, Value key, Value* value) {
    if (table->count == 0) return false;

    uint32_t hash = 0;
    if (!hashOf(&key, &hash)) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key, hash);
    if (IS_NULL(entry->key)) return false;

    *value = entry->value;
    return true;
}

// Reallocate the entries array from the Table to a larger capacity array.
// Transfer all non-tombstone values to the new array.
static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);

    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL_VAL;
        entries[i].value = NULL_VAL;
    }

    // count recalculated since tombstones are not carried over
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (IS_NULL(entry->key)) continue;

        uint32_t hash = 0;
        hashOf(&entry->key, &hash);

        Entry* dest = findEntry(entries, capacity, entry->key, hash);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

// Add the given Value into the Table's entries. If they key already exists
// within the Table, the current value will be replaced at that slot. Return
// true if the key did not already exist in the table.
bool tableSet(Table *table, Value key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    uint32_t hash;
    // TODO: solve return value to make sense
    if (!hashOf(&key, &hash)) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key, hash);
    bool isNewKey = IS_NULL(entry->key);
    // if new key and entry not tombstone
    if (isNewKey && IS_NULL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

// Attempt to delete an entry in the Table that has the given key. Deleted
// entries are replaced with a tombstone (a non-empty slot with no key, so that
// searching for a key isn't broken by removing a key from the chain). Returns
// false if there is no entry to delete.
bool tableDelete(Table* table, Value key) {
    if (table->count == 0) return false;

    uint32_t hash;
    // TODO solve return value.
    if (!hashOf(&key, &hash)) return false;

    // Find entry
    Entry* entry = findEntry(table->entries, table->capacity, key, hash);
    if (IS_NULL(entry->key)) return false;

    // replace with tombstone
    entry->key = NULL_VAL;
    entry->value = BOOL_VAL(true);
    return true;
}

// Add all entries from one table into another.
void tableAddAll(Table *from, Table *to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];

        if (!IS_NULL(entry->key)) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

// Variation on findEntry that is used for the VM's string Table.
// The string Table is used more like a set, to keep track of unique
// string values used in the VM's execution. The idea is to find and return
// the string used as the key in the table rather than the Value.
//
// WARNING: only to be used with string table, since keys are assumed to be
// ObjStrings.
ObjString* tableFindString(Table* table,
        const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    // Bitwise and of the hash value and 1 less than the Table's capacity.
    //
    // For capacities that are powers of 2, this is equivalent to:
    // uint32_t index = hash % table->capacity;
    // But significantly more CPU efficient (modulo is slow).
    uint32_t index = hash & (table->capacity - 1);

    for (;;) {
        Entry* entry = &table->entries[index];

        if (IS_NULL(entry->key)) {
            // stop at tombstone
            if (IS_NULL(entry->value)) return NULL;
        } else {
            uint32_t entryHash = 0;
            hashOf(&entry->key, &entryHash);
            ObjString* entryString = AS_STRING(entry->key);

            if (entryHash == hash
                    && memcmp(entryString->chars, chars, length) == 0) {
                // found
                return entryString;
            }
        }

        // Faster than modulo for powers of two. See index declaration.
        index = (index + 1) & (table->capacity - 1);
    }
}

// Called during garbage collection and used on the Table of interned strings.
// Delete any strings that haven't been marked as reachable.
void tableRemoveWhite(Table *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (!IS_NULL(entry->key) &&
                IS_OBJ(entry->key) &&
                !AS_OBJ(entry->key)->isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

// Mark both the keys and values found in the provided Table.
void markTable(Table *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markValue(entry->key);
        markValue(entry->value);
    }
}
