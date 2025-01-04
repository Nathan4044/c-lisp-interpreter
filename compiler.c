#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
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
    // Used to track how deep into an s-expression the parser is.
    int lParenCount;
} Parser;

static void expression();

// Local is the compiler's representation of a local variable that will exist
// on the stack.
typedef struct {
    Token name; // Contains the string of the variable name.
    int depth; // How deeply nested the variable is.
    bool isCaptured; // If Local variable is captured as upvalue in closure.
} Local;

// Upvalue is a representation of a variable that has been captured from an
// enclosing scope.
typedef struct {
    uint8_t index; // Into the function's list of upvalues.
    bool isLocal; // If captured from immediately surrounding scope.
} Upvalue;

// FunctionType Describes whether we're compiling a function or top level 
// script.
typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} FunctionType;

// A representation of the current state of the compiler.
typedef struct Compiler {
    // When compiling a function, a new compiler is created, and the current
    // compiler becomes the enclosing compiler of the new one. Since function
    // definitions can be arbitrarily nested, this can create a list of
    // compilers.
    struct Compiler* enclosing;
    // Function is the current function being compiled to bytecode. For
    // consistency and simplicity, the top level expressions of the script are
    // counted as a function as well.
    ObjFunction* function;
    // Indicates whether the compiler is compiling the script or an individual
    // function.
    FunctionType type; 

    Local locals[UINT8_COUNT]; // Collection of local variables.
    int localCount;
    Upvalue upvalues[UINT8_COUNT]; // Collection of Upvalues.
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

// Is the next token to be compiled of the given type?
static bool check(TokenType type) {
    return parser.current.type == type;
}

// If the next token is of the provided token type, consume it and return true.
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

// Emit a jump instruction with a dummy value, with all bits set to 1.
// This allows it to be replaced with bitwise and operations.
static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

// Replace the jump instruction at the provided offset with the current 
// location.
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

// Emit a loop instruction that will jump back to the provided offset.
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

// Set the zero value of all the fields in a compiler.
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

// Add one to the scope depth, used to track the nesting depth of local
// variables.
static void beginScope() {
    current->scopeDepth++;
}

// Remove one from the scope depth, remove local variables at the
// removed scope or capture them as upvalues if they're using in a subsequent
// scope.
static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
            current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

// Emit an instruction to retrieve the constant number value and place it on
// the stack.
static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

// Compile the appropriate expressions for each of the arguments passed to a
// function, return the number of arguments compiled.
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

static uint8_t parseVariable(const char* message);
static void defineVariable(uint8_t index);

// Compile a function object from a lambda definition, emit bytes that will
// convert the function to a closure at runtime.
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
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

// Compile an if expression, with an optional else value (defaults to null).
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

// Compile an and expression, which executes expressions until one is falsey,
// at which point it skips the remaining expressions and returns the falsey
// value. If none are falsey then the final expression is returned.
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

// Compile an or expression, which executes expressions until one is thruthy,
// at which point it skips the remaining expressions and returns the truthy
// value. If none are truthy then the final expression is returned.
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

// Compiles a while expression, which always returns a null value.
// Repeatedly executes the provided expressions until the condition expression
// evaluates to a falsey value.
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

// Parse a def expression by finding the associated variable location,
// parsing the expression associated with the variable's value, and emitting a
// define OpCode to put the variable in the correct corresponding location.
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

static void parseExpression();

// Compile a call in an s-expression.
// First compiles the expression that places the function on the stack.
// Next, compile the expressions for each of the arguments so they're placed
// on the stack above the function.
// Finally, emit the call opcode for calling the function, along with the
// argument count.
static void call() {
    // retrieve function
    parseExpression();

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

// Compile an S expression of the form (fn arg1 arg2 arg3...) where the fn is
// compiled based on its associated rule, followed by each arg as an expression.
static void sExpression() {
    parser.lParenCount++;
    advance();

    TokenType operatorType = parser.previous.type;

    switch(operatorType) {
        case TOKEN_AND:
            and_();
            break;
        case TOKEN_DEF:
            def();
            break;
        case TOKEN_IF:
            ifExpr();
            break;
        case TOKEN_LAMBDA:
            lambda();
            break;
        case TOKEN_OR:
            or_();
            break;
        case TOKEN_WHILE:
            while_();
            break;
        default:
            call();
    }

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

// Create a string constant from the previously consumed token, and emit an
// OpCode to load it onto the stack.
static void string() {
    emitConstant(OBJ_VAL(copyString(parser.previous.start+1,
                    parser.previous.length-2)));
}

static void variable();
static void callDict();

// Internal function for parsing all forms of expression.
static void parseExpression() {
    switch(parser.previous.type) {
        case TOKEN_QUOTE:
            if (parser.current.type == TOKEN_LEFT_PAREN) {
                parser.previous.type = TOKEN_LEFT_PAREN;
                parser.current.type = TOKEN_IDENTIFIER;
                parser.current.start = "list";
                parser.current.length = 4;
            } else {
                error("Expect '(' after '''.");
                break;
            }
        case TOKEN_LEFT_PAREN: sExpression(); break;
        case TOKEN_LEFT_BRACE: callDict(); break;
        case TOKEN_IDENTIFIER: variable(); break;
        case TOKEN_STRING: string(); break;
        case TOKEN_NUMBER: number(); break;
        case TOKEN_FALSE: literal(); break;
        case TOKEN_NULL: literal(); break;
        case TOKEN_TRUE: literal(); break;
        default:
            error("Expect expression.");
    }
}

// Add the given identifier token to the constant table as a string value.
// Return the constant index.
static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// Check if 2 identifier tokens are of the same length and contain the same
// characters.
static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// Look backward through the local variables to find the index of the most
// recent match and return its index. Return -1 if no match found.
static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            return i;
        }
    }

    return -1;
}

// Add a new Upvalue to the compiler's list and return the index.
// If the variable is already captured then return its existing index.
static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

// Search for a variable with a name matching the provided token.
// Begin by searching the enclosing scope, if none are found then recursively
// search the scopes around that one. The variable that is captured from further
// scopes is bubbled up as an Upvalue through each scope until the current one
// is reached, so that each Upvalue forms a chain to the current innermost scope.
static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    // Recursive call to enclosing function, allowing Upvalues to bubble up
    // through enclosing scopes.
    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

// Add an OpCode to get the variable of the given token name and add it to the
// stack. Try to find a variable with the matching name from the list of locals,
// if none are found then recursively search enclosing scopes for a matching
// Upvalue to capture. If there is no matching local at any enclosing scope,
// assume the variable is a global.
static void namedVariable(Token name) {
    int arg = resolveLocal(current, &name);
    uint8_t getOp;

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
    }

    emitBytes(getOp, arg);
}

// Fetch a variable with the name given in the last consumed token.
static void variable() {
    namedVariable(parser.previous);
}

// Called in the event an error has occurred. Finds the next likely point that
// the current expression ends, in order to find multiple genuine errors without
// multiple errors being reported for the same problem.
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

// Top level parsing function to compile all expressions.
// Parse the expression and handle any error that occurred in it.
static void expression() {
    advance();
    parseExpression();

    if (parser.panicMode) synchronize();
}

// Add a new local variable to the currently compiling function.
static int addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return -1;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = current->scopeDepth;
    local->isCaptured = false;
    return current->localCount - 1;
}

// If a local variable already exists, return it's index. If not, create a new
// local variable. Return -1 if the variable is being declared at global scope.
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

// Parse an identifier token to return the associated variable's index.
static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    int index = declareVariable();
    if (index == -1) {
        return identifierConstant(&parser.previous);
    } else {
        return index;
    }
}

// Emit a define OpCode based on the current scope depth.
static void defineVariable(uint8_t index) {
    uint8_t setOp = OP_DEFINE_LOCAL;
    if (current->scopeDepth == 0) {
        setOp = OP_DEFINE_GLOBAL;
    }

    emitBytes(setOp, index);
}

static void callDict() {
    Token dict;
    dict.line = parser.previous.line;
    dict.start = "dict";
    dict.length = 4;
    dict.type = TOKEN_IDENTIFIER;

    namedVariable(dict);

    uint16_t argCount = 0;
    while (!match(TOKEN_RIGHT_BRACE)) {
        expression();
        argCount++;

        if (argCount > UINT8_COUNT) {
            error("Too many arguments in dictionary declaration.");
        }
    }

    emitBytes(OP_CALL, (uint8_t)argCount);
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

void markCompilerRoots() {
    Compiler* compiler = current;

    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
