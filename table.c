#include "memory.h"
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
static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NULL(entry->value)) {
                // empty entry
                // return tombstone if one found
                return tombstone != NULL ? tombstone : entry;
            } else {
                // tombstone found
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            // entry found
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

// Retrieve the value in the table associated with the given key, and place it
// in the provided value address. Return false if the value could not be 
// retrieved, otherwise true.
bool tableGet(Table *table, ObjString *key, Value *value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

// Reallocate the entries array from the Table to a larger capacity array.
// Transfer all non-tombstone values to the new array.
static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL_VAL;
    }

    // count recalculated since tombstones are not carried over
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
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
bool tableSet(Table *table, ObjString *key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
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
bool tableDelete(Table *table, ObjString *key) {
    if (table->count == 0) return false;

    // Find entry
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // replace with tombstone
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

// Add all entries from one table into another.
void tableAddAll(Table *from, Table *to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];

        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

// Variation on findEntry that is used for the VM's string Table.
// The string Table is used more like a set, to keep track of unique
// string values used in the VM's execution. The idea is to find and return
// the string used as the key in the table rather than the Value.
ObjString* tableFindString(Table* table,
        const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];

        if (entry->key == NULL) {
            // stop at tombstone
            if (IS_NULL(entry->value)) return NULL;
        } else if (entry->key->hash == hash &&
                memcmp(entry->key->chars, chars, length) == 0) {
            // found
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}
