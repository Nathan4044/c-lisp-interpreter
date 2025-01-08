#ifndef clisp_common_h
#define clisp_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Change Value representation to be contained within a double type.
// When not defined, uses a tagged union to represent data.
#define NAN_BOXING

// Enables printing of compiled chunks. Comment out to disable.
// #define DEBUG_PRINT_CODE

// Enables execution tracing in the VM. Comment out to disable.
// #define DEBUG_TRACE_EXECUTION

// Runs garbage collection as often as possible, to help search for potential
// bugs.
// #define DEBUG_STRESS_GC

// Print information to the console when garbage collection is called.
// #define DEBUG_LOG_GC

// Maximum number of Values that can be represented in an array of size UINT8_MAX.
#define UINT8_COUNT (UINT8_MAX + 1)

#endif
