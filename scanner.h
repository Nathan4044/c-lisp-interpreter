#ifndef clisp_scanner_h
#define clisp_scanner_h

typedef enum {
    // single character tokens
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_QUOTE, TOKEN_DASH, TOKEN_PLUS,
    TOKEN_SLASH, TOKEN_STAR,
    // literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // keywords
    TOKEN_AND, TOKEN_DEF, TOKEN_FALSE, TOKEN_FOR,
    TOKEN_IF, TOKEN_LAMBDA, TOKEN_NULL, TOKEN_OR,
    TOKEN_TRUE, TOKEN_WHILE,

    TOKEN_ERROR, TOKEN_EOF
} TokenType;

// Scanned tokens that the scanner recognises as a distinct item.
typedef struct {
    TokenType type;

    // Pointer to where in the source code the Token string starts.
    const char* start;

    // Number of characters in the token.
    int length;

    // Line number of the characters in the token.
    int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif
