#include <stddef.h>
#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

// reallocate takes a pointer and does one of three actions:
// 1. if the newSize is 0, free the pointer
// 2. if the oldSize is 0, initialise a new block of memory
// 3. resize the memory block at the pointer from the old to the new size
//
// realloc from stdlib takes care of case 2 and 3.
//
// All memory allocation and deallocation goes through this function, to
// assist in tracking for the garbage collector.
void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
    vm.bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#else
        if (vm.bytesAllocated > vm.nextGC) {
            collectGarbage();
        }
#endif
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL)
        exit(1);
    return result;
}

// Mark object as visited during garbage collection.
void markObject(Obj* object)
{
    if (object == NULL)
        return;
    if (object->isMarked)
        return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;

    if (vm.greyCapacity < vm.greyCount + 1) {
        vm.greyCapacity = (int)GROW_CAPACITY(vm.greyCapacity);
        vm.greyStack = (Obj**)realloc(vm.greyStack,
            sizeof(Obj*) * (size_t)vm.greyCapacity);

        if (vm.greyStack == NULL)
            exit(1);
    }

    vm.greyStack[vm.greyCount++] = object;
}

// Mark the object associated with a Value during garbage collection.
void markValue(Value value)
{
    if (IS_OBJ(value))
        markObject(AS_OBJ(value));
}

// Mark every value in a dynamically allocated array during garbage collection.
static void markArray(ValueArray* array)
{
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

// Mark all objects associated with the current object.
static void blackenObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
    case OBJ_CLOSURE: {
        ObjClosure* closure = (ObjClosure*)object;
        markObject((Obj*)closure->function);
        for (int i = 0; i < closure->upvalueCount; i++) {
            markObject((Obj*)closure->upvalues[i]);
        }
        break;
    }
    case OBJ_FUNCTION: {
        ObjFunction* function = (ObjFunction*)object;
        markObject((Obj*)function->name);
        markArray(&function->chunk.constants);
        break;
    }
    case OBJ_UPVALUE:
        markValue(((ObjUpvalue*)object)->closed);
        break;
    case OBJ_LIST: {
        ObjList* list = (ObjList*)object;
        for (int i = 0; i < list->array.count; i++) {
            markValue(list->array.values[i]);
        }
        break;
    }
    case OBJ_DICT: {
        ObjDict* dict = (ObjDict*)object;
        for (int i = 0; i < dict->table.capacity; i++) {
            Entry entry = dict->table.entries[i];
            markValue(entry.key);
            markValue(entry.value);
        }
        break;
    }
    case OBJ_NATIVE:
    case OBJ_STRING:
        break;
    }
}

// Free the memory used by the object at the given address.
static void freeObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

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
    case OBJ_LIST: {
        ObjList* list = (ObjList*)object;
        initValueArray(&list->array);
        FREE(ObjList, list);
        break;
    }
    case OBJ_DICT: {
        ObjDict* dict = (ObjDict*)object;
        initTable(&dict->table);
        FREE(ObjDict, dict);
        break;
    }
    }
}

// Mark all Values directly accessible by the VM.
static void markRoots(void)
{
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL;
        upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    markTable(&vm.globals);
    markCompilerRoots();
}

// For each marked object, remove it from the greyStack and add all associated
// values onto the greyStack.
static void traceReferences(void)
{
    while (vm.greyCount > 0) {
        Obj* object = vm.greyStack[--vm.greyCount];
        blackenObject(object);
    }
}

// Go through all the objects that have been allocated. Free any that have not
// been marked.
static void sweep(void)
{
    Obj* previous = NULL;
    Obj* object = vm.objects;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;

            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
}

// Straightforward mark and sweep garbage collection. Stops code execution when
// running.
// Starts by recursively tracing through all objects reachable from the VM,
// then delete all objects that have not been marked as reachable.
void collectGarbage(void)
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    traceReferences();
    // Extra stage for removing strings that have no references.
    tableRemoveWhite(&vm.strings);
    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("\tcollected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}

// Free all objects that have been allocated in the VM.
void freeObjects(void)
{
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    free(vm.greyStack);
}
