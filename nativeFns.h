#include <stdbool.h>
#include "value.h"

bool add(int argCount, Value* args, Value* result);
bool clockNative(int argCount, Value* args, Value* result);
bool divide(int argCount, Value* args, Value* result);
bool equal(int count, Value* args, Value* result);
bool greater(int argCount, Value* args, Value* result);
bool less(int argCount, Value* args, Value* result);
bool multiply(int argCount, Value* args, Value* result);
bool printVals(int argCount, Value* args, Value* result);
bool strCat(int argCount, Value* args, Value* result);
bool subtract(int argCount, Value* args, Value* result);
bool not_(int argCount, Value* args, Value* result);

// List related builtins
bool list(int argCount, Value* args, Value* result);
bool push_(int argCount, Value* args, Value* result);
bool first(int argCount, Value* args, Value* result);
bool rest(int argCount, Value* args, Value* result);
bool len(int argCount, Value* args, Value* result);

// Dict related builtins
bool dict(int argCount, Value* args, Value* result);
bool set(int argCount, Value* args, Value* result);
bool get(int argCount, Value* args, Value* result);
