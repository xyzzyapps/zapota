/**
 * @file demo_libuv.c
 * @brief libuv event loop: one-shot timer.
 */

#include <uv.h>
#include <stdio.h>

static void on_timer(uv_timer_t *handle) {
    printf("[2] timer fired\n");
    uv_stop(handle->loop);
}

int main(void) {
    printf("====================================================\n");
    printf("libuv Demonstration\n");
    printf("====================================================\n");
    printf("[1] libuv %s\n", uv_version_string());

    uv_loop_t loop;
    if (uv_loop_init(&loop) != 0) {
        fprintf(stderr, "uv_loop_init failed\n");
        return 1;
    }

    uv_timer_t timer;
    uv_timer_init(&loop, &timer);
    uv_timer_start(&timer, on_timer, 1, 0);
    uv_run(&loop, UV_RUN_DEFAULT);
    uv_close((uv_handle_t *)&timer, NULL);
    uv_run(&loop, UV_RUN_DEFAULT);
    uv_loop_close(&loop);

    printf("libuv demonstration completed successfully.\n");
    return 0;
}
