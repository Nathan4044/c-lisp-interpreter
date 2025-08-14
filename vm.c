#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "nativeFns.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

VM vm;

// Return the Value n positions from the top, without removing it.
static Value peek(int distance)
{
    return vm.stackTop[-1 - distance];
}

// Reset the VM's stack my moving the pointer for the top of the stack to
// the beginning of the stack array.
static void resetStack(void)
{
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

// Write an error message to stderr from the provided string template and
// passed values. Print a stack trace of the call stack at the time of the error
// and the remove all items from the stack.
void runtimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = (size_t)(frame->ip - function->chunk.code - 1);
        fprintf(stderr, "[line %d] in ",
            function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
    resetStack();
}

// Add a native function to the globals pool with the given identifier.
static void defineNative(const char* name, NativeFn function)
{
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, vm.stack[0], vm.stack[1]);
    pop();
    pop();
}

// Set the initial state of the VM.
// Zero all the VM's fields, and add all relevant native functions.
void initVM(void)
{
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globals);

    vm.greyCount = 0;
    vm.greyCapacity = 0;
    vm.greyStack = NULL;

    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    defineNative("+", add);
    defineNative("*", multiply);
    defineNative("-", subtract);
    defineNative("/", divide);
    defineNative("rem", rem);
    defineNative("<", less);
    defineNative(">", greater);
    defineNative("=", equal);
    defineNative("clock", clockNative);
    defineNative("print", printVals);
    defineNative("str", strCat);
    defineNative("not", not_);

    // List related builtins
    defineNative("list", list);
    defineNative("push", push_);
    defineNative("push!", pushMut);
    defineNative("first", first);
    defineNative("rest", rest);
    defineNative("len", len);

    // Dict related builtins
    defineNative("dict", dict);
    defineNative("set", set);
    defineNative("get", get);
}

// Free all allocated memory associated with the VM.
void freeVM(void)
{
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globals);
}

// Add a new Value to the top of the VM's value stack.
void push(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

// Remove the top Value from the VM's value stack and return it.
Value pop(void)
{
    vm.stackTop--;
    return *vm.stackTop;
}

// Add a new frame to the call stack, designate the slots for parameters that
// have been passed in, and execute the bytecode stored in the function of the
// provided closure.
static bool call(ObjClosure* closure, int argCount)
{
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

static bool callNative(NativeFn native, int argCount, bool popFunc)
{
    Value result = NULL_VAL;
    bool successful = native(argCount, vm.stackTop - argCount, &result);
    vm.stackTop -= argCount + popFunc;
    push(result);
    return successful;
}

// Properly execute the call convention for the given value type.
static bool callValue(Value callee, int argCount)
{
    if (!IS_OBJ(callee)) {
        runtimeError("Can only call functions.");
        return false;
    }

    switch (AS_OBJ(callee)->type) {
    case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
    case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        return callNative(native, argCount, true);
    }
    default:
        runtimeError("Can only call functions.");
        return false;
    }
}

// Create an Upvalue object, insert it into the list of open upvalues held by
// the VM. If the VM already contains a reference to the same variable then
// return the existing Upvalue from the list.
static ObjUpvalue* captureUpvalue(Value* local)
{
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

// Close over all Upvalues until reaching the provided slot.
static void closeUpvalues(Value* last)
{
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

// Return true if the given value is equivelent to false in lisp.
bool isFalsey(Value value)
{
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
static InterpretResult run(void)
{
    static void* dispatchTable[] = {
        &&op_constant,
        &&op_null,
        &&op_true,
        &&op_false,
        &&op_pop,
        &&op_define_global,
        &&op_get_global,
        &&op_define_local,
        &&op_get_local,
        &&op_get_upvalue,
        &&op_close_upvalue,
        &&op_jump_false,
        &&op_jump,
        &&op_loop,
        &&op_call,
        &&op_add,
        &&op_subtract,
        &&op_multiply,
        &&op_divide,
        &&op_closure,
        &&op_return,
    };
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    ObjString* name;
    Value constant;
    uint8_t slot;
    uint16_t offset;
    int argCount;
    ObjFunction* function;
    Value result;

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define DISPATCH() goto* dispatchTable[READ_BYTE()]
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

    DISPATCH();
op_constant:
    constant = READ_CONSTANT();
    push(constant);
    DISPATCH();
op_null:
    push(NULL_VAL);
    DISPATCH();
op_true:
    push(BOOL_VAL(true));
    DISPATCH();
op_false:
    push(BOOL_VAL(false));
    DISPATCH();
op_pop:
    pop();
    DISPATCH();
op_define_global:
    name = READ_STRING();
    tableSet(&vm.globals, OBJ_VAL(name), peek(0));
    DISPATCH();
op_get_global:
    name = READ_STRING();
    Value value;

    if (!tableGet(&vm.globals, OBJ_VAL(name), &value)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
    }

    push(value);
    DISPATCH();
op_define_local:
    slot = READ_BYTE();
    frame->slots[slot] = peek(0);
    push(peek(0));
    DISPATCH();
op_get_local:
    slot = READ_BYTE();
    push(frame->slots[slot]);
    DISPATCH();
op_get_upvalue:
    slot = READ_BYTE();
    push(*frame->closure->upvalues[slot]->location);
    DISPATCH();
op_close_upvalue:
    closeUpvalues(vm.stackTop - 1);
    pop();
    DISPATCH();
op_jump_false:
    offset = READ_SHORT();
    if (isFalsey(peek(0)))
        frame->ip += offset;
    DISPATCH();
op_jump:
    offset = READ_SHORT();
    frame->ip += offset;
    DISPATCH();
op_loop:
    offset = READ_SHORT();
    frame->ip -= offset;
    DISPATCH();
op_call:
    argCount = READ_BYTE();
    if (!callValue(peek(argCount), argCount))
        return INTERPRET_RUNTIME_ERROR;
    frame = &vm.frames[vm.frameCount - 1];

    DISPATCH();
op_add:
    argCount = READ_BYTE();
    if (!callNative(add, argCount, false))
        return INTERPRET_RUNTIME_ERROR;

    DISPATCH();
op_subtract:
    argCount = READ_BYTE();
    if (!callNative(subtract, argCount, false))
        return INTERPRET_RUNTIME_ERROR;

    DISPATCH();
op_multiply:
    argCount = READ_BYTE();
    if (!callNative(multiply, argCount, false))
        return INTERPRET_RUNTIME_ERROR;

    DISPATCH();
op_divide:
    argCount = READ_BYTE();
    push(NULL_VAL);
    if (!callNative(divide, argCount, false))
        return INTERPRET_RUNTIME_ERROR;

    DISPATCH();
op_closure:
    function = AS_FUNCTION(READ_CONSTANT());
    ObjClosure* closure = newClosure(function);
    push(OBJ_VAL(closure));

    for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();

        if (isLocal)
            closure->upvalues[i] = captureUpvalue(frame->slots + index);
        else
            closure->upvalues[i] = frame->closure->upvalues[index];
    }

    DISPATCH();
op_return:
    result = pop();
    closeUpvalues(frame->slots);
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
    DISPATCH();

#undef DISPATCH
#undef READ_STRING
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_BYTE
}

// The given source code is compiled to bytecode and stored in a top-level
// function. If there are no compilation errors, the returned function is then
// executed on the VM.
InterpretResult interpret(const char* source)
{
    ObjFunction* function = compile(source);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}
