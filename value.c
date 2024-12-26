#include "value.h"
#include "memory.h"
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// initValueArray initialises all values of a ValueArray to their correct zero value.
void initValueArray(ValueArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

// writeValueArray writes a byte to the given ValueArray.
void writeValueArray(ValueArray *array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        int newCapacity = GROW_CAPACITY(oldCapacity);

        array->values = GROW_ARRAY(Value, array->values, oldCapacity, newCapacity);
        array->capacity = newCapacity;
    }

    array->values[array->count] = value;
    array->count++;
}

// freeValueArray frees any allocated memory associated with a ValueArray.
void freeValueArray(ValueArray *array) {
    free(array->values);
    initValueArray(array);
}

// printValue prints a human-readable representation of a Value.
void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NULL: printf("null"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: printObject(value); break;
    }
}

// Checks is two given values are the same, based on their types.
bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL: return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
        default: return false;
    }
}
