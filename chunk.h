#ifndef clisp_chunk_h
#define clisp_chunk_h

#include "common.h"
#include "value.h"
#include <stdint.h>

// Enum representing the individual bytecode instructions for the VM.
typedef enum {
    OP_CONSTANT,
    OP_RETURN
} OpCode;

// A chunk is a container for constants and bytecode instructions.
typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte);
int addConstant(Chunk* chunk, Value value);
void freeChunk(Chunk* chunk);

#endif
