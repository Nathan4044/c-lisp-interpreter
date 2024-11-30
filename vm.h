#ifndef clisp_vm_h
#define clisp_vm_h

#include "chunk.h"
#include <stdint.h>

// A container for the state of the VM.
typedef struct {
    Chunk* chunk; // The chunk being executed.
    uint8_t* ip; // The instruction pointer, pointing to the next byte to read.
} VM;

// A representation of the different return states of running the VM.
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();

InterpretResult interpret(Chunk* chunk);

#endif
