#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

// remove unused parameter warning for functions built to match the builtin function signature
#define UNUSED(x) (void)(x)

// Return the amount of seconds since execution began.
bool clockNative(int argCount, Value* args, Value* result) {
    UNUSED(argCount);
    UNUSED(args);
    *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    return true;
}

// Add up all numbers passed to +. Throws error when non-number types are given.
bool add(int argCount, Value* args, Value* result) {
    double total = 0;

    for (int i = 0; i < argCount; i++) {
        if (!IS_NUMBER(args[i])) {
            runtimeError("Operand must be a number.");
            return false;
        }
        total += AS_NUMBER(args[i]);
    }

    *result = NUMBER_VAL(total);
    return true;
}

// Multiply togather all numbers passed to *. Throws error when non-number types
// are given.
bool multiply(int argCount, Value* args, Value* result) {
    double total = 1;

    for (int i = 0; i < argCount; i++) {
        if (!IS_NUMBER(args[i])) {
            runtimeError("Operand must be a number.");
            return false;
        }

        total *= AS_NUMBER(args[i]);
    }

    *result = NUMBER_VAL(total);
    return true;
}

// Functionality changed depending on how many Values are passed in:
//
// If 1 Value is passed in, negate it and return it.
//
// If more than 1 Value is passed in, the first Value is taken as the base, then
// the remaining values are subtracted from it. Once all Values have been
// subtracted, return what remains.
//
// Throws an error if no Values are provided, or if one of the arguments is not
// a number.
bool subtract(int argCount, Value* args, Value* result) {
    switch (argCount) {
        case 0:
            runtimeError("Attempted to call '-' with no arguments.");
            return false;
        case 1:
            if (!IS_NUMBER(args[0])) {
                runtimeError("Operand must be a number.");
                return false;
            }
            *result = NUMBER_VAL(-(AS_NUMBER(args[0])));
            return true;
        default: {
            double sub = 0;

            for (int i = 1; i < argCount; i++) {
                if (!IS_NUMBER(args[i])) {
                    runtimeError("Operand must be a number.");
                    return false;
                }

                sub += AS_NUMBER(args[i]);
            }

            *result = NUMBER_VAL(AS_NUMBER(args[0]) - sub);
            return true;
        }
    }
}

// Functionality changed depending on how many Values are passed in:
//
// If 1 Value is passed in, make a fraction of 1 over the given Value.
// e.g. (/ 4) results in 0.25
//
// If more than 1 Value is passed in, the first Value is taken as the base, then
// the remaining values are used to divide it. Once all Values have been used,
// return what remains.
//
// Throws an error if no Values are provided, or if one of the arguments is not
// a number.
bool divide(int argCount, Value* args, Value* result) {
    switch (argCount) {
        case 0:
            runtimeError("Attempted to call '/' with no arguments.");
            return false;
        case 1:
            if (!IS_NUMBER(args[0])) {
                runtimeError("Operand must be a number.");
                return false;
            }

            if (AS_NUMBER(args[0]) == 0) {
                runtimeError("Cannot divide by zero.");
                return false;
            }

            *result = NUMBER_VAL(1 / AS_NUMBER(args[0]));
            return true;
        default: {
            if (!IS_NUMBER(args[0])) {
                runtimeError("Operand must be a number.");
                return false;
            }
            double first = AS_NUMBER(args[0]);

            for (int i = 1; i < argCount; i++) {
                if (!IS_NUMBER(args[i])) {
                    runtimeError("Operand must be a number.");
                    return false;
                }

                double div = AS_NUMBER(args[i]);

                if (div == 0) {
                    runtimeError("Attemped divide by zero");
                    return false;
                }

                first /= div;
            }

            *result = NUMBER_VAL(first);
            return true;
        }
    }
}

// Equivalent to % operator in lua.
//
// Returns the remainder when the first Value is divided by the second Value.
// The result is signed the same as the second argument.
bool rem(int argCount, Value* args, Value* result) {
    if (argCount != 2) {
        runtimeError("Attempted to call 'rem' with wrong number of arguments.");
        return false;
    }

    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        runtimeError("Attempted to call 'rem' with non-number.");
        return false;
    }

    double answer = remainder(AS_NUMBER(args[0]), AS_NUMBER(args[1]));
    if (answer < 0) answer *= -1;

    if (AS_NUMBER(args[1]) < 0) {
        answer *= -1;
    }

    *result = NUMBER_VAL(answer);
    return true;
}

// Ensure all Values are greater than the following one.
//
// Error thrown if there are no arguments, or if any argument is not a number.
bool greater(int argCount, Value* args, Value* result) {
    bool isGreater = true;

    if (argCount == 0) {
        runtimeError("Attempted to call '>' with no arguments.");
        return false;
    }

    if (!IS_NUMBER(args[0])) {
        runtimeError("Attempted '>' with non-number");
        return false;
    }

    for (int i = 0; i < argCount - 1; i++) {
        Value first = args[i];
        Value second = args[i+1];

        if (!IS_NUMBER(second)) {
            runtimeError("Attempted '>' with non-number");
            return false;
        }

        double firstNum = AS_NUMBER(first);
        double secondNum = AS_NUMBER(second);

        if (!(firstNum > secondNum)) {
            isGreater = false;
            break;
        }
    }

    *result = BOOL_VAL(isGreater);
    return true;
}

// Ensure all Values are less than the following one.
//
// Error thrown if there are no arguments, or if any argument is not a number.
bool less(int argCount, Value* args, Value* result) {
    bool isLess = true;

    if (argCount == 0) {
        runtimeError("Attempted to call '<' with no arguments.");
        return false;
    }

    if (!IS_NUMBER(args[0])) {
        runtimeError("Attempted '<' with non-number");
        return false;
    }

    for (int i = 0; i < argCount - 1; i++) {
        Value first = args[i];
        Value second = args[i+1];

        if (!IS_NUMBER(second)) {
            runtimeError("Attempted '>' with non-number");
            return false;
        }

        double firstNum = AS_NUMBER(first);
        double secondNum = AS_NUMBER(second);

        if (!(firstNum < secondNum)) {
            isLess = false;
            break;
        }
    }

    *result = BOOL_VAL(isLess);
    return true;
}

// Returns true if all Values passed as argument are equivalent.
bool equal(int count, Value* args, Value* result) {
    bool areEqual = true;
    for (int i = 0; i < count - 1; i++) {
        if (!valuesEqual(args[i], args[i+1])) {
            areEqual = false;
            break;
        }
    }

    *result = BOOL_VAL(areEqual);
    return true;
}

// Print all Values given, separated by a space. Returns null.
bool printVals(int argCount, Value* args, Value* result) {
    UNUSED(result);
    for (int i = 0; i < argCount; i++) {
        printValue(args[i]);

        printf(" ");
    }
    printf("\n");

    return true;
}

// Create a string of all the Values passed to the function, separated by a
// space, and returns it.
bool strCat(int argCount, Value* args, Value* result) {
    int len = 1; // 1 for null terminator
    char str[30];

    for (int i = 0; i < argCount; i++) {
        Value v = args[i];
#ifdef NAN_BOXING
        if (IS_BOOL(v)) {
            len += AS_BOOL(v) ? 4 : 5;
        } else if (IS_NULL(v)) {
            len += 4;
        } else if (IS_NUMBER(v)) {
            sprintf(str, "%g", AS_NUMBER(v));
            len += strlen(str);
        } else if (IS_OBJ(v)) {
            Obj* obj = AS_OBJ(v);

            switch (obj->type) {
                case OBJ_STRING:
                    len += AS_STRING(v)->length;
                    break;
                case OBJ_LIST:
                    len += 6;
                    break;
                case OBJ_DICT:
                    len += 6;
                    break;
                case OBJ_FUNCTION:
                case OBJ_CLOSURE:
                case OBJ_NATIVE:
                    len += 6;
                case OBJ_UPVALUE:
                    runtimeError("Should not be able to pass upvalue.");
                    return false;
            }
        }
#else
        switch (v.type) {
            case VAL_BOOL:
                if (AS_BOOL(v)) {
                    len += 4;
                } else {
                    len += 5;
                }
                break;
            case VAL_NULL:
                len += 4;
                break;
            case VAL_NUMBER:
                sprintf(str, "%g", AS_NUMBER(v));
                len += strlen(str);
                break;
            case VAL_OBJ:
                // TODO: broken
                len += AS_STRING(v)->length;
        }
#endif
    }

    char* chars = ALLOCATE(char, len);
    int current = 0;
    ObjString* s;

    for (int i = 0; i < argCount; i++) {
        Value v = args[i];
#ifdef NAN_BOXING
        if (IS_BOOL(v)) {
            if (AS_BOOL(v)) {
                memcpy(chars+current, "true", 4);
                current += 4;
            } else {
                memcpy(chars+current, "false", 5);
                current += 5;
            }
        } else if (IS_NULL(v)) {
            memcpy(chars+current, "null", 4);
            current += 4;
        } else if (IS_NUMBER(v)) {
            sprintf(str, "%g", AS_NUMBER(v));
            int l = (int)strlen(str);
            memcpy(chars+current, str, l);
            current += l;
        } else if (IS_OBJ(v)) {
            Obj* obj = AS_OBJ(v);

            switch (obj->type) {
                case OBJ_STRING:
                    s = AS_STRING(v);
                    memcpy(chars+current, s->chars, s->length);
                    current += s->length;
                    break;
                case OBJ_LIST:
                    memcpy(chars+current, "<list>", 6);
                    current += 6;
                    break;
                case OBJ_DICT:
                    memcpy(chars+current, "<dict>", 6);
                    current += 6;
                    break;
                case OBJ_FUNCTION:
                case OBJ_CLOSURE:
                case OBJ_NATIVE:
                    memcpy(chars+current, "< fn >", 6);
                    current += 6;
                    break;
                case OBJ_UPVALUE:
                    runtimeError("Should not be able to pass upvalue.");
                    return false;
            }
        }
#else
        switch (v.type) {
            case VAL_BOOL:
                if (AS_BOOL(v)) {
                    memcpy(chars+current, "true", 4);
                    current += 4;
                } else {
                    memcpy(chars+current, "false", 5);
                    current += 5;
                }
                break;
            case VAL_NULL:
                memcpy(chars+current, "null", 4);
                current += 4;
                break;
            case VAL_NUMBER:
                sprintf(str, "%g", AS_NUMBER(v));
                int l = strlen(str);
                memcpy(chars+current, str, l);
                current += l;
                break;
            // TODO: broken
            case VAL_OBJ:
                s = AS_STRING(v);
                memcpy(chars+current, s->chars, s->length);
                current += s->length;
                break;
        }
#endif
    }
    chars[len-1] = '\0';
    s = takeString(chars, len);

    *result = OBJ_VAL(s);
    return true;
}

// Return false if Value evaluates to true, return false otherwise.
bool not_(int argCount, Value* args, Value* result) {
    switch (argCount) {
        case 0:
            runtimeError("Attempted to call 'not' with no arguments.");
            return false;
        case 1:
            *result = BOOL_VAL(isFalsey(args[0]));
            return true;
        default:
            runtimeError("Attempted to call 'not' with more than one argument.");
            return false;
    }
}

// Return a List object containing all Values passed in to the function.
bool list(int argCount, Value* args, Value* result) {
    ObjList* list = newList();

    for (int i = 0; i < argCount; i++) {
        writeValueArray(&list->array, args[i]);
    }

    *result = OBJ_VAL(list);
    return true;
}

// Return a new list, containing all the values of the list passed in and with
// the new Value appended to it.
bool push_(int argCount, Value* args, Value* result) {
    if (argCount != 2) {
        runtimeError(
            "Attempted to call 'push' with incorrect number of arguments."
        );
        return false;
    }

    if (!IS_LIST(args[0])) {
        runtimeError("Attempted to call 'push' on non-list object.");
        return false;
    }

    ObjList* oldList = AS_LIST(args[0]);
    ObjList* newlist = newList();

    newlist->array.count = oldList->array.count;
    newlist->array.capacity = oldList->array.capacity;
    newlist->array.values = ALLOCATE(Value, newlist->array.capacity);

    for (int i = 0; i < oldList->array.count; i++) {
        newlist->array.values[i] = oldList->array.values[i];
    }

    writeValueArray(&newlist->array, args[1]);

    *result = OBJ_VAL(newlist);
    return true;
}

// Append the provided Value to the List. Return null.
bool pushMut(int argCount, Value* args, Value* result) {
    UNUSED(result);
    if (argCount != 2) {
        runtimeError(
            "Attempted to call 'push!' with incorrect number of arguments."
        );
        return false;
    }

    if (!IS_LIST(args[0])) {
        runtimeError("Attempted to call 'push!' on non-list object.");
        return false;
    }

    writeValueArray(&AS_LIST(args[0])->array, args[1]);

    return true;
}

// Return the first item of the provided list, null if list is empty.
bool first(int argCount, Value* args, Value* result) {
    if (argCount != 1) {
        runtimeError(
            "Attempted to call 'first' with incorrect number of arguments."
        );
        return false;
    }

    if (!IS_LIST(args[0])) {
        runtimeError("Attempted to call 'first' on non-list object.");
        return false;
    }

    ObjList* list = AS_LIST(args[0]);

    if (list->array.count > 0) {
        *result = list->array.values[0];
    }

    return true;
}

// Return a new list containing all but the first element of the provided list.
bool rest(int argCount, Value* args, Value* result) {
    if (argCount != 1) {
        runtimeError(
            "Attempted to call 'rest' with incorrect number of arguments."
        );
        return false;
    }

    if (!IS_LIST(args[0])) {
        runtimeError("Attempted to call 'rest' on non-list object.");
        return false;
    }

    ObjList* oldList = AS_LIST(args[0]);

    switch (oldList->array.count) {
        case 0:
            return true;
        case 1:
            *result = OBJ_VAL(newList());
            return true;
    }

    ObjList* newlist = newList();
    newlist->array.capacity = oldList->array.capacity;
    newlist->array.count = oldList->array.count - 1;
    newlist->array.values = ALLOCATE(Value, newlist->array.capacity);

    for (int i = 0; i < newlist->array.count; i++) {
        newlist->array.values[i] = oldList->array.values[i+1];
    }

    *result = OBJ_VAL(newlist);
    return true;
}

// Return the length of the provided string or list.
bool len(int argCount, Value* args, Value* result) {
    if (argCount != 1) {
        runtimeError(
            "Attempted to call 'len' with incorrect number of arguments."
        );
        return false;
    }

    if (!IS_OBJ(args[0])) {
        runtimeError("Attempted to call 'len' on incompatible type.");
        return false;
    }

    switch (AS_OBJ(args[0])->type) {
        case OBJ_LIST: {
            *result = NUMBER_VAL(AS_LIST(args[0])->array.count);
            return true;
        }
        case OBJ_STRING: {
            *result = NUMBER_VAL(AS_STRING(args[0])->length);
            return true;
        }
        default:
            runtimeError("Attempted to call 'len' on incompatible type.");
            return false;
    }
}

// Create and return a new Dict object, with each 2 passed in Values used as
// key/value pairs.
bool dict(int argCount, Value* args, Value* result) {
    if (argCount % 2 != 0) {
        runtimeError("Dict definition must have a value for every key.");
        return false;
    }

    ObjDict* dict = newDict();

    for (int i = 0; i < argCount - 1; i += 2) {
        uint32_t hash;
        if (!hashOf(&args[i], &hash)) {
            runtimeError("Invalid Dict key type: %s.", valueType(args[i]));
            return false;
        }

        tableSet(&dict->table, args[i], args[i+1]);
    }

    *result = OBJ_VAL(dict);
    return true;
}

// Return a new Dict, which is a copy of the provided Dict along with a new 
// Entry constructed of the two provided Values as a key/value pair.
//
// If the key already exists in the provided Dict, overwrite its value.
bool set(int argCount, Value* args, Value* result) {
    if (argCount != 3) {
        runtimeError("Attempted to call 'set' with wrong number of arguments.");
        return false;
    }

    if (!IS_DICT(args[0])) {
        runtimeError("Cannot call set on non-dict type.");
        return false;
    }

    uint32_t hash;
    if (!hashOf(&args[1], &hash)) {
        runtimeError("Invalid Dict key type: %s.", valueType(args[1]));
        return false;
    }

    ObjDict* dict = newDict();

    tableAddAll(&AS_DICT(args[0])->table, &dict->table);
    tableSet(&dict->table, args[1], args[2]);

    *result = OBJ_VAL(dict);
    return true;
}

// Return the Value from the given Dict that's associated with the provided key.
// Return null if the key is not found.
bool get(int argCount, Value* args, Value* result) {
    if (argCount != 2) {
        runtimeError("Attempted to call 'get' with wrong number of arguments.");
        return false;
    }

    if (!IS_DICT(args[0])) {
        runtimeError("Cannot call get on non-dict type.");
        return false;
    }

    uint32_t hash;
    if (!hashOf(&args[1], &hash)) {
        runtimeError("Invalid Dict key type: %s.", valueType(args[1]));
        return false;
    }

    Value got;
    if (tableGet(&AS_DICT(args[0])->table, args[1], &got)) {
        *result = got;
    }

    return true;
}
#undef UNUSED
