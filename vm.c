#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

VM vm;

static Value peek(int n);
static void runtimeError(const char* c, ...);

static NativeResult clockNative(int argCount, Value* args) {
    NativeResult result;

    result.value = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    result.success = true;

    return result;
}

static NativeResult add(int argCount, Value* args) {
    NativeResult result;
    result.value = NULL_VAL;
    result.success = false;

    double total = 0;

    for (int i = 0; i < argCount; i++) {
        if (!IS_NUMBER(args[i])) {
            runtimeError("Operand must be a number.");
            return result;
        }
        total += AS_NUMBER(args[i]);
    }

    result.value = NUMBER_VAL(total);
    result.success = true;
    return result;
}

static NativeResult multiply(int argCount, Value* args) {
    NativeResult result;
    result.value = NULL_VAL;
    result.success = false;

    double total = 1;

    for (int i = 0; i < argCount; i++) {
        if (!IS_NUMBER(args[i])) {
            runtimeError("Operand must be a number.");
            return result;
        }

        total *= AS_NUMBER(args[i]);
    }

    result.value = NUMBER_VAL(total);
    result.success = true;
    return result;
}

static NativeResult subtract(int argCount, Value* args) {
    NativeResult result;
    result.value = NULL_VAL;
    result.success = false;

    switch (argCount) {
        case 0:
            runtimeError("Attempted to call '-' with no arguments.");
            break;
        case 1:
            if (!IS_NUMBER(args[0])) {
                runtimeError("Operand must be a number.");
                break;
            }
            result.value = NUMBER_VAL(-(AS_NUMBER(args[0])));
            result.success = true;
        default: {
            double sub = 0;

            for (int i = 1; i < argCount; i++) {
                if (!IS_NUMBER(args[i])) {
                    runtimeError("Operand must be a number.");
                    return result;
                }

                sub += AS_NUMBER(args[i]);
            }

            result.value = NUMBER_VAL(AS_NUMBER(args[0]) - sub);
            result.success = true;
        }
    }

    return result;
}

static NativeResult divide(int argCount, Value* args) {
    NativeResult result;
    result.value = NULL_VAL;
    result.success = false;

    switch (argCount) {
        case 0:
            runtimeError("Attempted to call '/' with no arguments.");
            return result;
        case 1:
            if (!IS_NUMBER(args[0])) {
                runtimeError("Operand must be a number.");
                return result;
            }
            result.value = NUMBER_VAL(-(AS_NUMBER(args[0])));
            result.success = true;
            return result;
        default: {
            if (!IS_NUMBER(args[0])) {
                runtimeError("Operand must be a number.");
                return result;
            }
            double first = AS_NUMBER(args[0]);

            for (int i = 1; i < argCount; i++) {
                if (!IS_NUMBER(args[i])) {
                    runtimeError("Operand must be a number.");
                    return result;
                }

                double div = AS_NUMBER(args[i]);

                if (div == 0) {
                    runtimeError("Attemped divide by zero");
                    return result;
                }

                first /= div;
            }

            result.value = NUMBER_VAL(first);
            result.success = true;
            return result;
        }
    }
}

static NativeResult greater(int argCount, Value* args) {
    NativeResult result;
    result.value = NULL_VAL;
    result.success = false;

    bool isGreater = true;

    if (argCount == 0) {
        runtimeError("Attempted to call '>' with no arguments.");
        return result;
    }

    if (!IS_NUMBER(args[0])) {
        runtimeError("Attempted '>' with non-number");
        return result;
    }

    for (int i = 0; i < argCount - 1; i++) {
        Value first = args[i];
        Value second = args[i+1];

        if (!IS_NUMBER(second)) {
            runtimeError("Attempted '>' with non-number");
            return result;
        }

        double firstNum = AS_NUMBER(first);
        double secondNum = AS_NUMBER(second);

        if (!(firstNum > secondNum)) {
            isGreater = false;
            break;
        }
    }

    result.value = BOOL_VAL(isGreater);
    result.success = true;
    return result;
}

static NativeResult less(int argCount, Value* args) {
    NativeResult result;
    result.value = NULL_VAL;
    result.success = false;

    bool isLess = true;

    if (argCount == 0) {
        runtimeError("Attempted to call '<' with no arguments.");
        return result;
    }

    if (!IS_NUMBER(args[0])) {
        runtimeError("Attempted '<' with non-number");
        return result;
    }

    for (int i = 0; i < argCount - 1; i++) {
        Value first = args[i];
        Value second = args[i+1];

        if (!IS_NUMBER(second)) {
            runtimeError("Attempted '>' with non-number");
            return result;
        }

        double firstNum = AS_NUMBER(first);
        double secondNum = AS_NUMBER(second);

        if (!(firstNum < secondNum)) {
            isLess = false;
            break;
        }
    }

    result.value = BOOL_VAL(isLess);
    result.success = true;
    return result;
}

static NativeResult equal(int count, Value* args) {
    bool areEqual = true;
    for (int i = 0; i < count - 1; i++) {
        if (!valuesEqual(args[i], args[i+1])) {
            areEqual = false;
            break;
        }
    }

    NativeResult result;
    result.success = true;
    result.value = BOOL_VAL(areEqual);
    return result;
}

static NativeResult printVals(int argCount, Value* args) {
    for (int i = 0; i < argCount; i++) {
        printValue(args[i]);

        printf(" ");
    }
    printf("\n");

    NativeResult result;
    result.success = true;
    result.value = NULL_VAL;
    return result;
}

static NativeResult strCat(int argCount, Value* args) {
    int len = 1; // 1 for null terminator
    char* str;

    for (int i = 0; i < argCount; i++) {
        Value v = args[i];
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

    for (int i = 0; i < argCount; i++) {
        Value v = peek(argCount-i-1);
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
    s = takeString(chars, len);

    NativeResult result;
    result.value = OBJ_VAL(s);
    result.success = true;

    return result;
}

// Reset the VM's stack my moving the pointer for the top of the stack to
// the beginning of the stack array.
static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ",
                function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
    int line = frame->closure->function->chunk.lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

static void defineNative(const char* name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

// Set the initial state of the VM.
void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globals);

    defineNative("+", add);
    defineNative("*", multiply);
    defineNative("-", subtract);
    defineNative("/", divide);
    defineNative("<", less);
    defineNative(">", greater);
    defineNative("=", equal);
    defineNative("clock", clockNative);
    defineNative("print", printVals);
    defineNative("str", strCat);
}

void freeVM() {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globals);
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

static bool call(ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.",
                closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAME_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1; // -1 to account for function sat on stack
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (!IS_OBJ(callee)) {
        runtimeError("Can only call functions.");
        return false;
    }

    switch (AS_OBJ(callee)->type) {
        case OBJ_CLOSURE:
            return call(AS_CLOSURE(callee), argCount);
        case OBJ_NATIVE: {
            NativeFn native = AS_NATIVE(callee);
            NativeResult result = native(argCount, vm.stackTop - argCount);
            vm.stackTop -= argCount + 1;
            push(result.value);
            return result.success;
        }
        default:
            runtimeError("Can only call functions.");
            return false;
    }
}

static bool isFalsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
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
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (frame->ip += 2, \
        (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("        ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        
        disassembleInstruction(&frame->closure->function->chunk,
                (int)(frame->ip - frame->closure->function->chunk.code));
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
            case OP_POP:
                pop();
                break;
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;

                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(value);
                break;
            }
            case OP_DEFINE_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                push(peek(0));
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_JUMP_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                push(OBJ_VAL(closure));
                break;
            }
            case OP_RETURN: {
                Value result = pop();
                vm.frameCount--;

                if (vm.frameCount == 0) {
                    pop();
                    printValue(result);
                    printf("\n");

                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

#undef READ_STRING
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_BYTE
}

// The function called to handle the execution of the provided Chunk in the VM.
InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}
