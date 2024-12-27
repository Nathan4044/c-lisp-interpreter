#include <stdlib.h>
#include "memory.h"
#include "chunk.h"
#include "object.h"
#include "vm.h"

// reallocate takes a pointer and does one of three actions:
// 1. if the newSize is 0, free the pointer
// 2. if the oldSize is 0, initialise a new block of memory
// 3. resize the memory block at the pointer from the old to the new size
//
// realloc from stdlib takes care of case 2 and 3.
//
// All memory allocation and deallocation goes through this function, to
// assist in tracking for the garbage collector.
void* reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

// Free the memory used by the object at the given address.
static void freeObject(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, function);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
    }
}

// Free all objects that have been allocated in the VM.
void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}
