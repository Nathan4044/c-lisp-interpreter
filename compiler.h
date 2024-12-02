#ifndef clisp_compiler_h
#define clisp_compiler_h

#include "chunk.h"
#include "vm.h"

bool compile(const char* source, Chunk* chunk);

#endif
