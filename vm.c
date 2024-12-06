#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

VM vm;

// Reset the VM's stack my moving the pointer for the top of the stack to
// the beginning of the stack array.
static void resetStack() {
    vm.stackTop = vm.stack;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

// Set the initial state of the VM.
void initVM() {
    resetStack();
}

void freeVM() {
}

// Add a new Value to the top of the VM's value stack.
void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

// Remove the top Value from the VM's value stack and return it.
Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

// Return the Value n positions from the top, without removing it.
static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static Value readBack(uint8_t count) {
    return *(vm.stackTop - count);
}

static InterpretResult add(uint8_t count) {
    double result = 0;

    for (int i = 0; i < count; i++) {
        if (!IS_NUMBER(peek(0))) {
            runtimeError("Operand must be a number.");
            return INTERPRET_RUNTIME_ERROR;
        }
        result += AS_NUMBER(pop());
    }

    push(NUMBER_VAL(result));
    return INTERPRET_OK;
}

static InterpretResult multiply(uint8_t count) {
    double result = 1;

    for (int i = 0; i < count; i++) {
        if (!IS_NUMBER(peek(0))) {
            runtimeError("Operand must be a number.");
            return INTERPRET_RUNTIME_ERROR;
        }

        result *= AS_NUMBER(pop());
    }

    push(NUMBER_VAL(result));
    return INTERPRET_OK;
}

static InterpretResult subtract(uint8_t count) {
    double sub = 0;

    for (int i = 1; i < count; i++) {
        if (!IS_NUMBER(peek(0))) {
            runtimeError("Operand must be a number.");
            return INTERPRET_RUNTIME_ERROR;
        }

        sub += AS_NUMBER(pop());
    }

    push(NUMBER_VAL(AS_NUMBER(pop()) - sub));
    return INTERPRET_OK;
}

static InterpretResult divide(uint8_t count) {
    if (!IS_NUMBER(peek(count))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
    }
    double first = AS_NUMBER(readBack(count));

    for (int i = 1; i < count; i++) {
        if (!IS_NUMBER(peek(0))) {
            runtimeError("Operand must be a number.");
            return INTERPRET_RUNTIME_ERROR;
        }

        double div = AS_NUMBER(pop());

        if (div == 0) {
            runtimeError("Attemped divide by zero");
            exit(3);
        }

        first /= div;
    }

    pop();
    push(NUMBER_VAL(first));
    return INTERPRET_OK;
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
#define BINARY_OP(op) \
    do { \
        double b = pop(); \
        double a = pop(); \
        push(a op b); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("        ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk,
                (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction;
        switch(instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NULL: push(NULL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_ADD:
                if (add(READ_BYTE()) != INTERPRET_OK) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            case OP_SUBTRACT:   
                if (subtract(READ_BYTE()) != INTERPRET_OK) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            case OP_MULTIPLY:   
                if (multiply(READ_BYTE()) != INTERPRET_OK) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            case OP_DIVIDE:     
                if (divide(READ_BYTE()) != INTERPRET_OK) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_RETURN: 
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
        }
    }

#undef BINARY_OP
#undef READ_CONSTANT
#undef READ_BYTE
}

// The function called to handle the execution of the provided Chunk in the VM.
InterpretResult interpret(const char* source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}
