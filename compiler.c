#include "chunk.h"
#include "compiler.h"
#include "scanner.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_EXPRESSION,
	PREC_UNARY,
	PREC_CALL,
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    Precedence precedence;
} ParseRule;

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

Parser parser;
Chunk* compilingChunk;

static Chunk* currentChunk() {
    return compilingChunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // nothing?
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message)  {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);

    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {
    emitReturn();
}

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    prefixRule();
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(value);
}

static int compileArgs() {
    int operandCount = 0;
    for (;;) {
        if (parser.current.type == TOKEN_RIGHT_PAREN) {
            break;
        }

        if (parser.current.type == TOKEN_EOF) {
            error("Unexpected end of file");
            return -1;
        }

        expression();
        operandCount++;
    }

    return operandCount;
}

// is this needed?
static void sExpression() {
    advance();

    TokenType operatorType = parser.previous.type;
    int operandCount = 0;

    switch (operatorType) {
        case TOKEN_PLUS: {
            operandCount = compileArgs();

            if (operandCount < 0) {
                return;
            }
            advance();

            switch (operandCount) {
                case 0:
                    emitConstant(0);
                    break;
                case 1:
                    break;
                default:
                    emitBytes(OP_ADD, operandCount);
            }
            break;
        }
        case TOKEN_STAR: {
            operandCount = compileArgs();

            if (operandCount < 0) {
                return;
            }
            advance();

            switch (operandCount) {
                case 0:
                    emitConstant(1);
                    break;
                case 1:
                    break;
                default:
                    emitBytes(OP_MULTIPLY, operandCount);
            }
            break;
        }
        case TOKEN_MINUS: {
            operandCount = compileArgs();

            if (operandCount < 0) {
                return;
            }
            advance();

            switch (operandCount) {
                case 0:
                    error("attemped to call - with no arguments");
                    return;
                case 1:
                    emitByte(OP_NEGATE);
                    break;
                default:
                    emitBytes(OP_SUBTRACT, operandCount);
            }
            break;
        }
        case TOKEN_SLASH: {
            operandCount = compileArgs();

            if (operandCount < 0) {
                return;
            }
            advance();

            if (operandCount == 0) {
                error("attemped to call / with no arguments");
                return;
            }
            emitBytes(OP_DIVIDE, operandCount);
            break;
        }
        // TODO: function calls and builtins
        default: return; // unreachable
    }
}

static void unary() {
    TokenType operatorType = parser.previous.type;

    switch (operatorType) {
        case TOKEN_MINUS: {
                              // compile the operand
                              parsePrecedence(PREC_UNARY);
                              emitByte(OP_NEGATE); 
                              break;
                          }
        case TOKEN_LEFT_PAREN: sExpression(); break;
        default: return; // unreachable
    }
}

static void expression() {
    parsePrecedence(PREC_EXPRESSION);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = {sExpression, PREC_NONE},
    [TOKEN_RIGHT_PAREN]     = {NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL, PREC_NONE},
    [TOKEN_MINUS]           = {unary, PREC_NONE},
    [TOKEN_PLUS]            = {NULL, PREC_NONE},
    [TOKEN_SLASH]           = {NULL, PREC_NONE},
    [TOKEN_STAR]            = {NULL, PREC_NONE},
    [TOKEN_EQUAL]           = {NULL, PREC_NONE},
    [TOKEN_GREATER]         = {NULL, PREC_NONE},
    [TOKEN_GREATER_EQUAL]   = {NULL, PREC_NONE},
    [TOKEN_LESS]            = {NULL, PREC_NONE},
    [TOKEN_LESS_EQUAL]      = {NULL, PREC_NONE},
    [TOKEN_IDENTIFIER]      = {NULL, PREC_NONE},
    [TOKEN_STRING]          = {NULL, PREC_NONE},
    [TOKEN_NUMBER]          = {number, PREC_NONE},
    [TOKEN_AND]             = {NULL, PREC_NONE},
    [TOKEN_FALSE]           = {NULL, PREC_NONE},
    [TOKEN_FOR]             = {NULL, PREC_NONE},
    [TOKEN_DEF]             = {NULL, PREC_NONE},
    [TOKEN_IF]              = {NULL, PREC_NONE},
    [TOKEN_NULL]            = {NULL, PREC_NONE},
    [TOKEN_OR]              = {NULL, PREC_NONE},
    [TOKEN_PRINT]           = {NULL, PREC_NONE},
    [TOKEN_TRUE]            = {NULL, PREC_NONE},
    [TOKEN_WHILE]           = {NULL, PREC_NONE},
    [TOKEN_ERROR]           = {NULL, PREC_NONE},
    [TOKEN_EOF]             = {NULL, PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

// *unfinished* compile takes a string of source code, scans the tokens, and prints them.
bool compile(const char* source, Chunk* chunk) {
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");

    endCompiler();
    return !parser.hadError;
}
