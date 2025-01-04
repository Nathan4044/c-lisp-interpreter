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

// is the given character a letter or underscore?
static bool isValidIdentChar(char c) {
    switch (c) {
        case '(':
        case ')':
        case '{':
        case '}':
        case ' ':
        case '\'':
        case '\r':
        case '\t':
        case '\n':
            return false;
        case '/':
            if (peekNext() == '/') return false;
        default:
            return true;
    }
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

// Check if the currently selected characters compromise the rest of a keyword
// by checking first that the length of the remaining characters is the same as
// the length of the remaining keyword. If so, then check that the remaining
// characters match the remaining characters of the keyword.
static TokenType checkKeyword(
        int start, int length, const char* rest, TokenType type
        ) {
    if (scanner.current - scanner.start == start + length &&
            memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

// Check if the current identifier is actually a keyword, reserved by the
// language. Return the type of the matched keyword if any, else it is
// an identifier.
static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'd': return checkKeyword(1, 2, "ef", TOKEN_DEF);
        case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                }
            }
            break;
        case 'l': return checkKeyword(1, 5, "ambda", TOKEN_LAMBDA);
        case 'n': return checkKeyword(1, 3, "ull", TOKEN_NULL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 't': return checkKeyword(1, 3, "rue", TOKEN_TRUE);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}

// Read the remaining characters of an identifier, which (after the initial
// letter or underscore) can be any alphanumeric character or an underscore.
static Token identifier() {
    while (isValidIdentChar(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
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
        case '\'': return makeToken(TOKEN_QUOTE);
        case '"': return string();
    }

    if (isValidIdentChar(c)) return identifier();

    return errorToken("Unexpected character.");
}
