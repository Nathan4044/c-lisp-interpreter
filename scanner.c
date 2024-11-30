#include "scanner.h"
#include <string.h>
#include <stdbool.h>

// holds the data required by the scanner for scanning tokens.
typedef struct {
    const char* start; // points to the first character of the token.
    const char* current; // points to the current character to be consumed.
    int line; // the line number of the current token.
} Scanner;

Scanner scanner;

// Initialise the scanner's data.
void initScanner(const char *source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

// is the given character a digit?
static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// Has the scanner reached the end of its input (null terminator).
static bool isAtEnd() {
    return *scanner.current == '\0';
}

// Progress the scanner to the next character.
static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

// Return the next character to be consumed.
static char peek() {
    return *scanner.current;
}

// Return second character to be consumed.
static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

// If the next character to be consumed matches the given character, advance
// and return true. Otherwise, don't advance and return false.
static bool match(char expected) {
    if (isAtEnd()) return false;

    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

// Return a token of the provided type.
// Add the start position of the token literal, the length, and the line number.
static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

// Create a token with the error type, along with the provided error message.
static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

// skip over all the whitespace characters from the current point in source,
// adding to the line count when moving to the next one.
static void skipWhitespace() {
    for (;;) {
        char c = peek();
        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
            default:
                return;
        }
    }
}

// Scan the characters involved in the number token and return a number token.
static Token number() {
    while (isDigit(peek())) advance();

    // look for decimal
    if (peek() == '.' && isDigit(peekNext())) {
        advance();

        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

// Scan the characters involved in the string token and return a string token.
static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    advance();
    return makeToken(TOKEN_STRING);
}

// Scan and return whatever the next token type is.
Token scanToken() {
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();

    if (isDigit(c)) return number();

    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '*': return makeToken(TOKEN_STAR);
        case '/': return makeToken(TOKEN_SLASH);
        case '<': {
            return makeToken(
                    match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        }
        case '>': {
            return makeToken(
                    match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        }
        case '"': return string();
    }

    return errorToken("Unexpected character.");
}
