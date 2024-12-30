#ifndef clisp_object_h
#define clisp_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"
#include <stdint.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// Helper macros for confirming the type of an object.
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

// Helper macros to convert an object to a specific type.
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

// Simple enum for identifying the type of an object.
typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_NATIVE,
    OBJ_UPVALUE,
} ObjType;

// Obj is essentially a header for other object types so that they can be used
// interchangeably, like a form of polymorphism.
struct Obj {
    ObjType type;

    // Whether the object been marked during garbage collection.
    bool isMarked;

    // Instrusive list to keep track of all objects at runtime. Used to find
    // objects to clean up during garbage collection.
    struct Obj* next;
};

// A runtime representation of a function. Can be passed as a value or
// called to execute its contents.
typedef struct {
    Obj obj;

    // Expected number of arguments to the function.
    int arity;

    // Number of values to be captured from enclosed scopes.
    int upvalueCount;

    // Array of bytecode executed by the function.
    Chunk chunk;

    // The name of the function when written.
    ObjString* name;
} ObjFunction;

typedef bool (*NativeFn)(int argCount, Value* args, Value* result);

// A representation of built-in functions that can be accessed at runtime.
typedef struct {
    Obj obj;

    // The C function to be ran when the function is called.
    NativeFn function;
} ObjNative;

// A string object.
struct ObjString {
    Obj obj;
    int length; // Number of chars.
    char* chars; // Start of the string's characters, not null-terminated. 
    // Hash calculated from the string's value, used for equality checks and
    // enables string interning (only one instance of identical strings).
    uint32_t hash;
};

// ObjUpvalue is the runtime representation of a variable that has been lifted
// from its scope in a closure. Most of the time, they will be references to
// earlier points on the stack, before the current function's scope, but
// in the circumstance it is lifted from the scope it is 'closed over', and
// placed in the closed value parameter.
typedef struct ObjUpvalue {
    Obj obj;

    // Points to the location of the value, either on the stack or lifted into
    // the closed parameter.
    Value* location;

    // Contains the value associated with the variable at the time it is lifted.
    Value closed;

    // Used to keep track of all open Upvalues at runtime. An intrusive list
    // held by the VM.
    struct ObjUpvalue* next;
} ObjUpvalue;

// A representation of a closure. Wraps a function object and any values
// captured from the enclosing scopes.
typedef struct {
    Obj obj; 

    // Function that is called when the Closure is called.
    ObjFunction* function;

    // Pointer to the array of pointers that contain the values captured from
    // the enclosing scopes.
    ObjUpvalue** upvalues;

    // Number of values in the upvalues array.
    int upvalueCount;
} ObjClosure;

ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);
ObjUpvalue* newUpvalue(Value* slot);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

// Return true if Value is an Object and has the matching Object type.
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
