/**
 * @file demo_linenoise.c
 * @brief linenoise line-editing library (history API; no TTY prompt).
 */

#include "linenoise.h"
#include <stdio.h>

int main(void) {
    printf("====================================================\n");
    printf("linenoise Demonstration\n");
    printf("====================================================\n");

    linenoiseSetMultiLine(1);
    if (linenoiseHistoryAdd("zig build run-linenoise") == 0) {
        fprintf(stderr, "linenoiseHistoryAdd failed\n");
        return 1;
    }
    linenoiseHistoryAdd("help");
    linenoiseHistorySetMaxLen(32);

    printf("[1] multiline mode on\n");
    printf("[2] history: 2 entries, max 32 (prompt skipped; stdin is not a TTY)\n");
    printf("linenoise demonstration completed successfully.\n");
    return 0;
}
