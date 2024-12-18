#include <stdint.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

// initChunk initialises all values of a Chunk to their correct zero value.
void initChunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

// writeChunk writes a byte to the given Chunk.
void writeChunk(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity,
                chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity,
                chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void overwriteLast(Chunk *chunk, uint8_t byte) {
    chunk->code[chunk->count-1] = byte;
}

// freeChunk frees any allocated memory associated with a Chunk.
void freeChunk(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

// addConstant adds another constant to the provided Chunk.
int addConstant(Chunk *chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}
