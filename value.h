#ifndef clisp_value_h
#define clisp_value_h

#include "common.h"

// Value represents a value instance in the VM.
typedef double Value;

// ValueArray is a dynamically allocated array of Values.
typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

#endif

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

void printValue(Value value);
