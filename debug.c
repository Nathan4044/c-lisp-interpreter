#include "debug.h"
#include "chunk.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>

// disassembleChunk prints out a human-readable representation of a chunk of
// bytecode.
void disassembleChunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }

    printf("\n");
}

// Prints a representation of a single byte instruction.
static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

// Prints an instruction that has a byte argument.
static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset+1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
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

// Prints a jump instruction, along with the destination index.
static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];

    printf("%-16s %4d\n", name, offset + 3 + sign * jump);

    return offset + 3;
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
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_LOCAL:
            return byteInstruction("OP_DEFINE_LOCAL", chunk, offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_JUMP_FALSE:
            return jumpInstruction("OP_JUMP_FALSE", 1, chunk, offset);
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byteInstruction("OP_CALL", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            printValue(chunk->constants.values[constant]);
            printf("\n");
            return offset;
        }
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
