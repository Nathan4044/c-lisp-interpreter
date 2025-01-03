#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

// Return the amount of seconds since execution began.
bool clockNative(int argCount, Value* args, Value* result) {
    *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    return true;
}

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
            *result = NUMBER_VAL(-(AS_NUMBER(args[0])));
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

bool printVals(int argCount, Value* args, Value* result) {
    for (int i = 0; i < argCount; i++) {
        printValue(args[i]);

        printf(" ");
    }
    printf("\n");

    return true;
}

bool strCat(int argCount, Value* args, Value* result) {
    int len = 1; // 1 for null terminator
    char str[30];

    for (int i = 0; i < argCount; i++) {
        Value v = args[i];
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
                len += AS_STRING(v)->length;
        }
    }

    char* chars = ALLOCATE(char, len);
    int current = 0;
    ObjString* s;

    for (int i = 0; i < argCount; i++) {
        Value v = args[i];
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
            case VAL_OBJ:
                s = AS_STRING(v);
                memcpy(chars+current, s->chars, s->length);
                current += s->length;
                break;
        }
    }
    chars[len-1] = '\0';
    s = takeString(chars, len);

    *result = OBJ_VAL(s);
    return true;
}

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

bool list(int argCount, Value* args, Value* result) {
    ObjList* list = newList();

    for (int i = 0; i < argCount; i++) {
        writeValueArray(&list->array, args[i]);
    }

    *result = OBJ_VAL(list);
    return true;
}

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
