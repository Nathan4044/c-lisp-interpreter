#ifndef clisp_scanner_h
#define clisp_scanner_h

typedef enum {
    // single character tokens
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    // literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // keywords
    TOKEN_AND, TOKEN_DEF, TOKEN_FALSE,
	TOKEN_FOR, TOKEN_IF, TOKEN_LAMBDA,
	TOKEN_NOT, TOKEN_NULL, TOKEN_OR,
    TOKEN_TRUE, TOKEN_WHILE,

    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif
