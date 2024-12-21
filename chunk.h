#ifndef clisp_chunk_h
#define clisp_chunk_h

#include "common.h"
#include "value.h"
#include <stdint.h>

// Enum representing the individual bytecode instructions for the VM.
typedef enum {
    OP_CONSTANT,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_NOT,
    OP_RETURN,
    OP_POP,
    OP_STR,
    OP_PRINT,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_DEFINE_LOCAL,
    OP_GET_LOCAL,
    OP_JUMP_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
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
