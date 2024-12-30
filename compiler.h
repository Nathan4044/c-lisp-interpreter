#ifndef clisp_compiler_h
#define clisp_compiler_h

#include "chunk.h"
#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source);
void markCompilerRoots();

#endif
