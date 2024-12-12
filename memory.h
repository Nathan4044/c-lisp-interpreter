#ifndef clisp_memory_h
#define clisp_memory_h

#include "common.h"

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

// Simple macro for defining the growth of the capacity of a dynamically
// allocated array.
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// Macro for correctly calling the reallocate function.
// Handles the growing, shrinking, allocating, and freeing of memory for
// dynamically allocated arrays.
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
            sizeof(type) * (newCount))

// Macro for specifically freeing memory from a pointer.
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif
