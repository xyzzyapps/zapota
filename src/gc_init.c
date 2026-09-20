/**
 * @file gc_init.c
 * @brief Constructor that starts Boehm GC before main() in every demo.
 */

#include "gc.h"

#if defined(_MSC_VER)
#  pragma section(".CRT$XCU", read)
static void gc_startup(void);
__declspec(allocate(".CRT$XCU")) void (*gc_startup_p)(void) = gc_startup;
static void gc_startup(void) { GC_INIT(); }
#else
__attribute__((constructor)) static void gc_startup(void) { GC_INIT(); }
#endif
