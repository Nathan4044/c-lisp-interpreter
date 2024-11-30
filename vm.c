#include "chunk.h"
#include "common.h"
#include <stdio.h>
#include "vm.h"

VM vm;

void initVM() {
}

void freeVM() {
}

// The run function is the main part of the interpreter.
//
// It consists of a central loop that continually reads through and executes
// the bytecode in the VM's Chunk.
//
// The instruction is fetched (READ_BYTE in the switch statement), then
// decoded (the case statements for each instruction), and executed
// (the actions taken within each case statement).
static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

    for (;;) {
        uint8_t instruction;
        switch(instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                printValue(constant);
                printf("\n");
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }

#undef READ_CONSTANT
#undef READ_BYTE
}

// The function called to handle the execution of the provided Chunk in the VM.
InterpretResult interpret(Chunk *chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}
