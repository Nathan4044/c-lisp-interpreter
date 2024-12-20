#ifndef clisp_vm_h
#define clisp_vm_h

#include <stdint.h>

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256

// A container for the state of the VM.
typedef struct {
    Chunk* chunk; // The chunk being executed.
    uint8_t* ip; // The instruction pointer, pointing to the next byte to read.
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;
    Obj* objects;
} VM;

// A representation of the different return states of running the VM.
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();

InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif
