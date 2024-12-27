#ifndef clisp_chunk_h
#define clisp_chunk_h

#include "common.h"
#include "value.h"
#include <stdint.h>

// Enum representing the individual bytecode instructions for the VM.
typedef enum {
    OP_CONSTANT,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_RETURN,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_DEFINE_LOCAL,
    OP_GET_LOCAL,
    OP_GET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_JUMP_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
} OpCode;

// A chunk is a container for constants and bytecode instructions.
typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void overwriteLast(Chunk* chnk, uint8_t byte);
int addConstant(Chunk* chunk, Value value);
void freeChunk(Chunk* chunk);

#endif
