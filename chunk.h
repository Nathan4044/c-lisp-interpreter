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
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
} OpCode;

// A chunk is a container for constants and bytecode instructions.
typedef struct {
    // Number of bytes in code.
    int count;

    // Capacity of code array.
    int capacity;

    // Contains the bytecode that the VM will run.
    uint8_t* code;

    // 1-1 mapped values to code, line number of corresponding byte.
    int* lines;

    // Array of constant values in source code.
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void overwriteLast(Chunk* chnk, uint8_t byte);
int addConstant(Chunk* chunk, Value value);
void freeChunk(Chunk* chunk);

#endif
