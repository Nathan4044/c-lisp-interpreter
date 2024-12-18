#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// Representation of the current state of the parser.
typedef struct {
    // The most recent token from scanToken.
    Token current;
    // The previously scanned token, the token currently being compiled.
    Token previous;
    bool hadError;
    // Ensures that the user isn't bombarded with error messages
    // for a single issue.
    bool panicMode;
    int lParenCount;
} Parser;

// Definition of the function signature used for parsing different expressions.
typedef void (*ParseFn)();

static void expression();

typedef struct {
    ParseFn expr;
    ParseFn sExpr;
}   ParseRule;

static ParseRule getRule(TokenType type);

// Single instance parser and chunks.
// - replace these as args passed to functions in order to support concurrent
//   parser usage for embedding.
Parser parser;
Chunk* compilingChunk;

// Should be useful later, when compiling separate chunks for each function.
static Chunk* currentChunk() {
    return compilingChunk;
}

// Print a structured error message to stderr that lets the user know what the
// error is and where it took place (line number and offending token).
static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // nothing? what?
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

// Wrapper around errorAt to output an error message for the token currently
// being compiled.
static void error(const char* message) {
    errorAt(&parser.previous, message);
}

// Wrapper around errorAt to output an error message for the next token to be
// compiled.
static void errorAtCurrent(const char* message)  {
    errorAt(&parser.current, message);
}

// Move to the next token generated by the scanner in order to compile it.
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

// Used to conditionally advance to the next token, only if parser.current is of
// the provided token type.
static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

// Write a byte to the current chunk, along with accurate line information.
static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

// Wrapper around emitByte for emitting two bytes at once. Added purely as a
// convenience for implementation.
static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

// Wrapper around emitByte for emitting an OP_RETURN byte.
static void emitReturn() {
    overwriteLast(currentChunk(), OP_RETURN);
}

static void emitPop() {
    emitByte(OP_POP);
}

// Add the provided value as a constant to the current chunk, returning its
// index in the constant pool.
//
// todo: check for identical constants and return its index instead.
static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);

    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

// Wrapper around emitBytes for convenience.
//
// Add the given constant to the chunk's constant pool, and add the bytecode
// needed to push the value onto the VM's stack.
static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

// Currently used for debug purposes and emitting a final return, to be updated
// in the future.
static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
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

        if (operandCount > UINT8_MAX) {
            error("Too many arguments in s-expression.");
            return -1;
        }

        expression();
        operandCount++;
    }

    return operandCount;
}

static void add() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }

    advance();
    switch (operandCount) {
        case 0:
            emitConstant(NUMBER_VAL(0));
            break;
        case 1:
            break;
        default:
            emitBytes(OP_ADD, operandCount);
    }
}

static void multiply() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }
    advance();

    switch (operandCount) {
        case 0:
            emitConstant(NUMBER_VAL(1));
            break;
        case 1:
            break;
        default:
            emitBytes(OP_MULTIPLY, operandCount);
    }
}

static void subtract() {
    int operandCount = compileArgs();

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
}

static void divide() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }
    advance();

    if (operandCount == 0) {
        error("attemped to call / with no arguments");
        return;
    }
    emitBytes(OP_DIVIDE, operandCount);
}

static void not() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }
    advance();

    switch (operandCount) {
        case 0:
            error("attemped to call not with no arguments");
            return;
        case 1:
            emitByte(OP_NOT);
            break;
        default:
            error("attemped to call not with too many arguments");
            return;
    }
}

static void equal() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }
    advance();

    switch (operandCount) {
        case 0:
            emitByte(OP_TRUE);
            break;
        default:
            emitBytes(OP_EQUAL, operandCount);
    }
}

static void greater() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }
    advance();

    switch (operandCount) {
        case 0:
            error("attempted to call > with 0 arguments");
            return;
        case 1:
            emitByte(OP_TRUE);
            break;
        default:
            emitBytes(OP_GREATER, operandCount);
    }
}

static void less() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }
    advance();

    switch (operandCount) {
        case 0:
            error("attempted to call < with 0 arguments");
            return;
        case 1:
            emitByte(OP_TRUE);
            break;
        default:
            emitBytes(OP_LESS, operandCount);
    }
}

static void strcmd() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }
    advance();

    emitBytes(OP_STR, operandCount);
}

static void print() {
    int operandCount = compileArgs();

    if (operandCount < 0) {
        return;
    }
    advance();

    emitBytes(OP_PRINT, operandCount);
}

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static void namedVariable(Token name) {
    uint8_t arg = identifierConstant(&name);
    emitBytes(OP_GET_GLOBAL, arg);
}

static void variable() {
    namedVariable(parser.previous);
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != EOF) {
        if (parser.lParenCount == 0) return;

        switch (parser.current.type) {
            case TOKEN_LEFT_PAREN:
                parser.lParenCount++;
                break;
            case TOKEN_RIGHT_PAREN:
                parser.lParenCount--;
            default:
                ;
        }

        advance();
    }
}

static void sExpression() {
    parser.lParenCount++;
    advance();

    TokenType operatorType = parser.previous.type;

    ParseFn fn = *getRule(operatorType).sExpr;

    if (fn == NULL) {
        error("Invalid first arg in s-expression.");
        return;
    }

    fn();
    parser.lParenCount--;
}

// Compile the given token to the current form of literal value.
static void literal() {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NULL: emitByte(OP_NULL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return; // unreachable
    }
}

static void string() {
    emitConstant(OBJ_VAL(copyString(parser.previous.start+1,
                    parser.previous.length-2)));
}

// Currently only used to wrap parse. Remove in future if nothing changes.
static void expression() {
    advance();
    ParseFn rule = *getRule(parser.previous.type).expr;

    if (rule == NULL) {
        error("Expect expression.");
        return;
    }

    rule();

    if (parser.panicMode) synchronize();
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void def() {
    uint8_t global = parseVariable("Expect variable name.");

    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' at end of def expression.");

    defineVariable(global);
}

static void negate() {
    expression();
    emitByte(OP_NEGATE);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = { sExpression, NULL },
    [TOKEN_RIGHT_PAREN]     = { NULL, NULL },
    [TOKEN_LEFT_BRACE]      = { NULL, NULL },
    [TOKEN_RIGHT_BRACE]     = { NULL, NULL },
    [TOKEN_MINUS]           = { negate, subtract },
    [TOKEN_PLUS]            = { NULL, add },
    [TOKEN_SLASH]           = { NULL, divide },
    [TOKEN_STAR]            = { NULL, multiply },
    [TOKEN_EQUAL]           = { NULL, equal },
    [TOKEN_GREATER]         = { NULL, greater },
    [TOKEN_LESS]            = { NULL, less },
    [TOKEN_IDENTIFIER]      = { variable, NULL }, // user defined function calls will replace NULL here
    [TOKEN_STRING]          = { string, NULL },
    [TOKEN_NUMBER]          = { number, NULL },
    [TOKEN_AND]             = { NULL, NULL },
    [TOKEN_DEF]             = { NULL, def },
    [TOKEN_FALSE]           = { literal, NULL },
    [TOKEN_FOR]             = { NULL, NULL },
    [TOKEN_IF]              = { NULL, NULL },
    [TOKEN_LAMBDA]          = { NULL, NULL },
    [TOKEN_NOT]             = { NULL, not },
    [TOKEN_NULL]            = { literal, NULL },
    [TOKEN_OR]              = { NULL, NULL },
    [TOKEN_PRINT]           = { NULL, print },
    [TOKEN_STR_CMD]         = { NULL, strcmd },
    [TOKEN_TRUE]            = { literal, NULL },
    [TOKEN_WHILE]           = { NULL, NULL },
    [TOKEN_ERROR]           = { NULL, NULL },
    [TOKEN_EOF]             = { NULL, NULL },
};

// Return the correct fule from the rules array.
static ParseRule getRule(TokenType type) {
    return rules[type];
}

// Compile takes a string of source code, scans the tokens, and compiles them
// into a chunk.
bool compile(const char* source, Chunk* chunk) {
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;
    parser.lParenCount = 0;

    advance();
    while (!match(TOKEN_EOF)) {
        expression();
        emitPop(); // Rewritten by endCompiler() to OP_RETURN on last expression.
    }

    endCompiler();
    return !parser.hadError;
}
