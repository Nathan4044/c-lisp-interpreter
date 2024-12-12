#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
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

Value popMultiple(int count) {
    vm.stackTop -= count;
    return *vm.stackTop;
}

// Return the Value n positions from the top, without removing it.
static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
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
            return INTERPRET_RUNTIME_ERROR;
        }

        first /= div;
    }

    pop();
    push(NUMBER_VAL(first));
    return INTERPRET_OK;
}

static InterpretResult greater(uint8_t count) {
    bool result = true;

    if (!IS_NUMBER(peek(count-1))) {
        runtimeError("Attempted '>' with non-number");
        return INTERPRET_RUNTIME_ERROR;
    }

    for (int i = 0; i < count - 1; i++) {
        Value first = peek(count-i-1);
        Value second = peek(count-i-2);

        if (!IS_NUMBER(second)) {
            runtimeError("Attempted '>' with non-number");
            return INTERPRET_RUNTIME_ERROR;
        }

        double firstNum = AS_NUMBER(first);
        double secondNum = AS_NUMBER(second);

        if (!(firstNum > secondNum)) {
            result = false;
            break;
        }
    }

    popMultiple(count);
    push(BOOL_VAL(result));
    return INTERPRET_OK;
}

static InterpretResult less(uint8_t count) {
    bool result = true;

    if (!IS_NUMBER(peek(count-1))) {
        runtimeError("Attempted '<' with non-number");
        return INTERPRET_RUNTIME_ERROR;
    }

    for (int i = 0; i < count - 1; i++) {
        Value first = peek(count-i-1);
        Value second = peek(count-i-2);

        if (!IS_NUMBER(second)) {
            runtimeError("Attempted '>' with non-number");
            return INTERPRET_RUNTIME_ERROR;
        }

        double firstNum = AS_NUMBER(first);
        double secondNum = AS_NUMBER(second);

        if (!(firstNum < secondNum)) {
            result = false;
            break;
        }
    }

    popMultiple(count);
    push(BOOL_VAL(result));
    return INTERPRET_OK;
}

static void equal(uint8_t count) {
    bool result = true;
    for (int i = 0; i < count - 1; i++) {
        if (!valuesEqual(peek(count-i-1), peek(count-i-2))) {
            result = false;
            break;
        }
    }

    popMultiple(count);
    push(BOOL_VAL(result));
}

static void strCat(uint8_t count) {
    int len = 1; // 1 for null terminator
    char* str;

    for (int i = 0; i < count; i++) {
        Value v = peek(count-i-1);
        switch (v.type) {
            case VAL_BOOL:
                if (AS_BOOL(v)) {
                    len += 4;
                } else {
                    len += 5;
                }
                break;
            case VAL_NULL:
                len += 4;
                break;
            case VAL_NUMBER:
                sprintf(str, "%g", AS_NUMBER(v));
                len += strlen(str);
                break;
            case VAL_OBJ:
                len += AS_STRING(v)->length;
        }
    }

    char* chars = ALLOCATE(char, len);
    int current = 0;
    ObjString* s;

    for (int i = 0; i < count; i++) {
        Value v = peek(count-i-1);
        switch (v.type) {
            case VAL_BOOL:
                if (AS_BOOL(v)) {
                    memcpy(chars+current, "true", 4);
                    current += 4;
                } else {
                    memcpy(chars+current, "false", 5);
                    current += 5;
                }
                break;
            case VAL_NULL:
                memcpy(chars+current, "null", 4);
                current += 4;
                break;
            case VAL_NUMBER:
                sprintf(str, "%g", AS_NUMBER(v));
                int l = strlen(str);
                memcpy(chars+current, str, l);
                current += l;
                break;
            case VAL_OBJ:
                s = AS_STRING(v);
                memcpy(chars+current, s->chars, s->length);
                current += s->length;
                break;
        }
    }
    chars[len-1] = '\0';

    popMultiple(count);
    s = takeString(chars, len);
    push(OBJ_VAL(s));
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
            case OP_EQUAL: equal(READ_BYTE()); break;
            case OP_GREATER:
                if (greater(READ_BYTE()) != INTERPRET_OK) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            case OP_LESS:
                if (less(READ_BYTE()) != INTERPRET_OK) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
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
            case OP_STR:
                strCat(READ_BYTE());
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_RETURN: 
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
        }
    }

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
