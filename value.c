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
        int newCapacity = (int)GROW_CAPACITY((size_t)oldCapacity);

        array->values = GROW_ARRAY(Value, array->values, (size_t)oldCapacity, (size_t)newCapacity);
        array->capacity = newCapacity;
    }

    array->values[array->count] = value;
    array->count++;
}

// freeValueArray frees any allocated memory associated with a ValueArray.
void freeValueArray(ValueArray *array) {
    FREE_ARRAY(Value, array->values, (size_t)array->capacity);
    initValueArray(array);
}

// printValue prints a human-readable representation of a Value.
void printValue(Value value) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        printf(AS_BOOL(value) ? "true" : "false");
    } else if (IS_NULL(value)) {
        printf("null");
    } else if (IS_NUMBER(value)) {
        printf("%g", AS_NUMBER(value));
    } else if (IS_OBJ(value)) {
        printObject(value);
    }
#else
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NULL: printf("null"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: printObject(value); break;
    }
#endif
}

// Returns the Value's type as a string.
char* valueType(Value value) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) return "bool";
    if (IS_NULL(value)) return "null";
    if (IS_NUMBER(value)) return "number";
    if (IS_OBJ(value)) {
        Obj* obj = AS_OBJ(value);
        switch (obj->type) {
            case OBJ_DICT: return "dict";
            case OBJ_STRING: return "string";
            case OBJ_FUNCTION: return "function";
            case OBJ_CLOSURE: return "closure";
            case OBJ_LIST: return "list";
            case OBJ_UPVALUE: return "upvalue";
            case OBJ_NATIVE: return "native fn";
        }
    }
    return "unreachable";
#else
    switch (value.type) {
        case VAL_BOOL: return "bool";
        case VAL_NULL: return "null";
        case VAL_NUMBER: return "number";
        case VAL_OBJ: {
            Obj* obj = AS_OBJ(value);
            switch (obj->type) {
                case OBJ_DICT: return "dict";
                case OBJ_STRING: return "string";
                case OBJ_FUNCTION: return "function";
                case OBJ_CLOSURE: return "closure";
                case OBJ_LIST: return "list";
                case OBJ_UPVALUE: return "upvalue";
                case OBJ_NATIVE: return "native fn";
            }
        }
    }
#endif
}

// Checks is two given values are the same, based on their types.
bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
    // Ensures that non-quiet NAN values are compared to each other as doubles.
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    return a == b;
#else
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL: return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
        default: return false;
    }
#endif
}
