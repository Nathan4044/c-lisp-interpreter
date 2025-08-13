#ifndef clisp_vm_h
#define clisp_vm_h

#include <stdint.h>

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAME_MAX 64
#define STACK_MAX (FRAME_MAX * UINT8_COUNT)

// Representation of an execution frame on the frame stack.
typedef struct {
    // Closure being executed in this Frame.
    ObjClosure* closure;

    // Pointer to the current byte of the function bytecode that is being
    // executed.
    uint8_t* ip;

    // Pointer to the place on the VM stack where local values begin, used as
    // the lowest point of the CallFrame's stack.
    Value* slots;
} CallFrame;

// A container for the state of the VM.
typedef struct {
    // Frame stack of the VM, to track depth of execution of functions
    // during runtime.
    CallFrame frames[FRAME_MAX];

    // The current number of frames actively used on the stack.
    int frameCount;

    // The stack of Values being actively used by the execution of the VM.
    Value stack[STACK_MAX];

    // Pointer to above the last Value placed on the stack.
    Value* stackTop;

    // Table of top-scope Values that are accessed via a call to the hash table.
    Table globals;

    // Table of unique strings used by the VM. The Table here is used more like
    // a set, with the key being the part of the entry that matters.
    Table strings;

    // An intrusive list of all objects that have been created, used to find
    // otherwise unreachable objects during garbage collection.
    Obj* objects;

    // An intrusive list of Values that are both captured from an enclosing
    // scope, and not yet 'closed over' (taken off the stack and stored in
    // the closure itself).
    ObjUpvalue* openUpvalues;

    // Current number of objects in the greyStack.
    int greyCount;

    // Max capacity of the currently allocated array of grey values.
    int greyCapacity;

    // Objects marked as reachable during garbage collection, but have not yet
    // been checked for linked objects that can be reached through it.
    Obj** greyStack;

    // Current number of bytes allocated to objects on the heap by the VM.
    size_t bytesAllocated;

    // The threshold for the next garbage collection. When bytesAllocated
    // becomes higher than nextGC, the garbage collector is triggered, and
    // nextGC will be updated to a new threshold value for the next collection.
    size_t nextGC;
} VM;

// A representation of the different return states of running the VM.
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM(void);
void freeVM(void);

InterpretResult interpret(const char* source);
void push(Value value);
Value pop(void);

void runtimeError(const char* format, ...);
bool isFalsey(Value value);

#endif
