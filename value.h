#ifndef clisp_value_h
#define clisp_value_h

#include "common.h"

// Forward declarations of objects to prevent cyclical dependencies.
typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjList ObjList;
typedef struct ObjDict ObjDict;

// Enum to represent possible Value types.
typedef enum {
    VAL_BOOL,
    VAL_NULL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

// Value represents a value instance in the VM.
typedef struct {
    ValueType type;

    // union of possible Value types, should be accessed via macros to ensure
    // the correct type is used.
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// Helper macros for confirming the type of a Value.
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

// Helper macros to use a Value as a specific type.
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

// Helper macros to construct a Value of a specific type.
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NULL_VAL ((Value){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})

// ValueArray is a dynamically allocated array of Values.
typedef struct {
    // Maximum slots in the current array.
    int capacity;

    // Current number of Values in the array.
    int count;

    // Pointer to the array of Values.
    Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
char* valueType(Value value);

void printValue(Value value);

#endif
