/**
 * @file demo_sokol.c
 * @brief Sokol time + log headers (no GPU window required).
 */

#define SOKOL_IMPL
#include "sokol_time.h"
#include "sokol_log.h"

#include <stdio.h>

int main(void) {
    printf("====================================================\n");
    printf("Sokol Time / Log Demonstration\n");
    printf("====================================================\n");

    stm_setup();
    const uint64_t t0 = stm_now();

    volatile unsigned acc = 0;
    for (unsigned i = 0; i < 100000u; i++) {
        acc += i;
    }

    const double ms = stm_ms(stm_since(t0));
    printf("[1] sokol_time: 1e5-iter loop took %.3f ms (acc=%u)\n", ms, acc);

    slog_func("zapota", 3, 0, "sokol_log default callback", __LINE__, __FILE__, NULL);
    printf("[2] sokol_log: slog_func invoked (see stderr line above)\n");
    printf("Sokol demonstration completed successfully.\n");
    return 0;
}
