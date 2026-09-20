/**
 * @file demo_gc.c
 * @brief Boehm-Demers-Weiser conservative garbage collector.
 */

#include "gc.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("====================================================\n");
    printf("Boehm GC Demonstration\n");
    printf("====================================================\n");

    GC_INIT();
    {
        const unsigned ver = (unsigned)GC_get_version();
        printf("[1] GC version: %u.%u.%u\n",
               (ver >> 16) & 0xff, (ver >> 8) & 0xff, ver & 0xff);
    }
    printf("    heap size:  %lu bytes\n", (unsigned long)GC_get_heap_size());

    char *msg = (char *)GC_MALLOC(32);
    if (!msg) {
        fprintf(stderr, "GC_MALLOC failed\n");
        return 1;
    }
    memcpy(msg, "zapota", 7);
    printf("[2] GC_MALLOC 32 bytes -> \"%s\"\n", msg);

    for (int i = 0; i < 1000; i++) {
        (void)GC_MALLOC(1024);
    }
    const size_t before = (size_t)GC_get_heap_size();
    GC_gcollect();
    const size_t after = (size_t)GC_get_heap_size();
    printf("[3] After 1000 throwaway allocs + GC_gcollect(): heap %zu -> %zu\n",
           before, after);
    printf("[4] Live pointer still valid: \"%s\"\n", msg);
    printf("Boehm GC demonstration completed successfully.\n");
    return 0;
}
