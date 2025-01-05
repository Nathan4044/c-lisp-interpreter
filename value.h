#ifndef clisp_value_h
#define clisp_value_h

#include <stdint.h>
#include <string.h>

#include "common.h"

// Forward declarations of objects to prevent cyclical dependencies.
typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjList ObjList;
typedef struct ObjDict ObjDict;

#ifdef NAN_BOXING

// all NAN bits set, quiet NAN bit set (unset can represent error values which
// are defined by the IEEE floating point number specification), plus an extra
// bit set for special values defined by Intel.
#define QNAN     ((uint64_t)0x7ffc000000000000)

// The first bit in the 64bit floating point number representation, which
// represents the sign of a number. Used here when the NAN values are set, to
// determine if the value is a pointer.
#define SIGN_BIT ((uint64_t)0x8000000000000000)

#define TAG_NULL  1 // 01
#define TAG_FALSE 2 // 10
#define TAG_TRUE  3 // 11

typedef uint64_t Value;

#define IS_BOOL(value)   (((value) | 1) == TRUE_VAL)
#define IS_NULL(value)   ((value) == NULL_VAL)
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_OBJ(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)   ((value) == TRUE_VAL)
#define AS_NUMBER(value) valueToNum(value)
#define AS_OBJ(value) \
    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NULL_VAL        ((Value)(uint64_t)(QNAN | TAG_NULL))
#define NUMBER_VAL(num) numToValue(num)
#define OBJ_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double valueToNum(Value value) {
    // memcpy is optimised away at compile time, meaning that the bits are
    // simply seen as the Value type rather than double.
    double num;
    memcpy(&num, &value, sizeof(value));
    return num;
}

static inline Value numToValue(double num) {
    // memcpy is optimised away at compile time, meaning that the bits are
    // simply seen as the Value type rather than double.
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

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

#endif

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
