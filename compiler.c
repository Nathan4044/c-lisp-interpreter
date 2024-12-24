#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
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

typedef struct {
    Token name;
    int depth;
} Local;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

// Single instance parser and chunks.
// - replace these as args passed to functions in order to support concurrent
//   parser usage for embedding.
Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

// Should be useful later, when compiling separate chunks for each function.
static Chunk* currentChunk() {
    return &current->function->chunk;
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

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void patchJump(int offset) {
    // -2 to get before the jump offset operand
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    // bit manipulation to replace the two 8 bit numbers with a 16 bit number.
    // 8 bits at a time, only keep the bytes that are set to 1.
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset+1] = jump & 0xff;
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;

    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
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

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copyString("lambda", 6);
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

// Currently used for debug purposes and emitting a final return, to be updated
// in the future.
static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL ?
                function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
            current->locals[current->localCount - 1].depth > current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    }
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static int compileArgs() {
    int operandCount = 0;
    while (parser.current.type != TOKEN_RIGHT_PAREN) {
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

static uint8_t parseVariable(const char* message);
static void defineVariable(uint8_t index);

static void lambda() {
    Compiler compiler;
    initCompiler(&compiler, TYPE_FUNCTION);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after lambda keyword\n");

    while (!match(TOKEN_RIGHT_PAREN)) {
        current->function->arity++;
        if (current->function->arity > 255) {
            errorAtCurrent("Can't have more than 255 parameters\n");
        }

        uint8_t constant = parseVariable("Expect parameter name\n");
    }

    while (!match(TOKEN_RIGHT_PAREN)) {
        expression();
        emitByte(OP_POP);
    }

    ObjFunction* function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
}

static void ifExpr() {
    expression();

    int thenJump = emitJump(OP_JUMP_FALSE);

    emitByte(OP_POP);
    expression();

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);

    emitByte(OP_POP);
    if (match(TOKEN_RIGHT_PAREN)) {
        emitByte(OP_NULL);
    } else {
        expression();
        consume(TOKEN_RIGHT_PAREN, "Expect ')' at end of if expression.");
    }

    patchJump(elseJump);
}

static void and_() {
    int operandCount = 0;
    int jumps[UINT8_MAX];

    while (parser.current.type != TOKEN_RIGHT_PAREN) {
        if (parser.current.type == TOKEN_EOF) {
            error("Unexpected end of file");
            return;
        }

        if (operandCount > UINT8_MAX) {
            error("Too many arguments in s-expression.");
            return;
        }

        expression();

        int jump = emitJump(OP_JUMP_FALSE);
        jumps[operandCount++] = jump;
        emitByte(OP_POP);
    }

    currentChunk()->count--;

    for (int i = 0; i < operandCount; i++) {
        patchJump(jumps[i]);
    }

    if (operandCount == 0) {
        emitByte(OP_TRUE);
    }
    advance();

    return;
}

static void or_() {
    int operandCount = 0;
    int jumps[UINT8_MAX];

    while (parser.current.type != TOKEN_RIGHT_PAREN) {
        if (parser.current.type == TOKEN_EOF) {
            error("Unexpected end of file");
            return;
        }

        if (operandCount > UINT8_MAX) {
            error("Too many arguments in s-expression.");
            return;
        }

        expression();

        int jumpFalse = emitJump(OP_JUMP_FALSE);
        int jump = emitJump(OP_JUMP);
        jumps[operandCount++] = jump;

        patchJump(jumpFalse);
        emitByte(OP_POP);
    }

    currentChunk()->count--;

    for (int i = 0; i < operandCount; i++) {
        patchJump(jumps[i]);
    }

    if (operandCount == 0) {
        emitByte(OP_FALSE);
    }
    advance();

    return;
}

static void while_() {
    int loopStart = currentChunk()->count;
    expression();

    int endJump = emitJump(OP_JUMP_FALSE);
    emitByte(OP_POP);

    while (parser.current.type != TOKEN_RIGHT_PAREN) {
        expression();
        emitByte(OP_POP);
    }

    emitLoop(loopStart);
    patchJump(endJump);
    emitByte(OP_POP);
    emitByte(OP_NULL);
    advance();
}

static void call() {
    // retrieve function
    ParseFn rule = *getRule(parser.previous.type).expr;

    if (rule == NULL) {
        error("Expect expression.");
        return;
    }

    rule();

    uint8_t argCount = 0;
    while (!match(TOKEN_RIGHT_PAREN)) {
        if (check(TOKEN_EOF)) {
            error("Unexpected end of file.");
            return;
        }

        expression();

        if (argCount == 255) {
            error("Can't have more than 255 arguments.");
        }

        argCount++;
    }
    
    emitBytes(OP_CALL, argCount);
}

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            return i;
        }
    }

    return -1;
}

static void namedVariable(Token name) {
    uint8_t getOp = OP_GET_LOCAL;
    int arg = resolveLocal(current, &name);

    if (arg == -1) {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
    }

    emitBytes(getOp, arg);
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
        call();
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

static int addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return -1;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = current->scopeDepth;
    return current->localCount - 1;
}

static int declareVariable() {
    if (current->scopeDepth == 0) return -1;

    Token* name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];

        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            return i;
        }
    }

    return addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    int index = declareVariable();
    if (index == -1) {
        return identifierConstant(&parser.previous);
    } else {
        return index;
    }
}

static void defineVariable(uint8_t index) {
    uint8_t setOp = OP_DEFINE_LOCAL;
    if (current->scopeDepth == 0) {
        setOp = OP_DEFINE_GLOBAL;
    }

    emitBytes(setOp, index);
}

static void def() {
    uint8_t index = parseVariable("Expect variable name.");

    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' at end of def expression.");

    Value* val = &currentChunk()->constants.values[currentChunk()->constants.count - 1];
    if (IS_FUNCTION(*val)) {
        ObjString* name = AS_STRING(currentChunk()->constants.values[index]);
        AS_FUNCTION(*val)->name = name;
    }

    defineVariable(index);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = { sExpression, NULL },
    [TOKEN_RIGHT_PAREN]     = { NULL, NULL },
    [TOKEN_LEFT_BRACE]      = { NULL, NULL },
    [TOKEN_RIGHT_BRACE]     = { NULL, NULL },
    [TOKEN_IDENTIFIER]      = { variable, NULL },
    [TOKEN_STRING]          = { string, NULL },
    [TOKEN_NUMBER]          = { number, NULL },
    [TOKEN_AND]             = { NULL, and_ },
    [TOKEN_DEF]             = { NULL, def },
    [TOKEN_FALSE]           = { literal, NULL },
    [TOKEN_FOR]             = { NULL, NULL },
    [TOKEN_IF]              = { NULL, ifExpr },
    [TOKEN_LAMBDA]          = { NULL, lambda },
    [TOKEN_NOT]             = { NULL, not },
    [TOKEN_NULL]            = { literal, NULL },
    [TOKEN_OR]              = { NULL, or_ },
    [TOKEN_TRUE]            = { literal, NULL },
    [TOKEN_WHILE]           = { NULL, while_ },
    [TOKEN_ERROR]           = { NULL, NULL },
    [TOKEN_EOF]             = { NULL, NULL },
};

// Return the correct fule from the rules array.
static ParseRule getRule(TokenType type) {
    return rules[type];
}

// Compile takes a string of source code, scans the tokens, and compiles them
// into a chunk.
ObjFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;
    parser.lParenCount = 0;

    advance();
    while (!match(TOKEN_EOF)) {
        expression();
        emitByte(OP_POP); // Rewritten by endCompiler() to OP_RETURN on last expression.
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}
