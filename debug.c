#include "debug.h"
#include "chunk.h"
#include <stdint.h>
#include <stdio.h>

// disassembleChunk prints out a human-readable representation of a chunk of
// bytecode.
void disassembleChunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

// Prints a representation of a single byte instruction.
static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

// Prints a representation of an instruction that references a constant.
static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}

// Prints a representation of an instruction that references a constant.
static int rangeInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, constant);

    return offset + 2;
}

// disassembleInstruction prints the instruction at the provided offset.
// It dispatches to the correct printing function depending on the instruction.
int disassembleInstruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);

    if (offset > 0 &&
            chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];

    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NULL:
            return simpleInstruction("OP_NULL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_ADD:
			return rangeInstruction("OP_ADD", chunk, offset);
        case OP_SUBTRACT:
			return rangeInstruction("OP_SUBTRACT", chunk, offset);
        case OP_MULTIPLY:
			return rangeInstruction("OP_MULTIPLY", chunk, offset);
        case OP_DIVIDE:
			return rangeInstruction("OP_DIVIDE", chunk, offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_EQUAL:
            return rangeInstruction("OP_EQUAL", chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
