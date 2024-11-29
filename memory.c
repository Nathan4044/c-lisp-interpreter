#include <stdlib.h>
#include "memory.h"

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
