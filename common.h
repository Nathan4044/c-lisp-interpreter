#ifndef clisp_common_h
#define clisp_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Enables printing of compiled chunks. Comment out to disable.
#define DEBUG_PRINT_CODE

// Enables execution tracing in the VM. Comment out to disable.
#define DEBUG_TRACE_EXECUTION

// Runs garbage collection as often as possible, to help search for potential
// bugs.
#define DEBUG_STRESS_GC

// Print information to the console when garbage collection is called.
#define DEBUG_LOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)

#endif
