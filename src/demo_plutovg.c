/**
 * @file demo_plutovg.c
 * @brief PlutoVG 2D Vector Graphics Demonstration.
 */

#include <stdio.h>
#include "plutovg.h"

int main(void) {
    printf("====================================================\n");
    printf("PlutoVG 2D Software Vector Rasterizer Demonstration\n");
    printf("====================================================\n");

    const int width = 400;
    const int height = 300;

    plutovg_surface_t *surface = plutovg_surface_create(width, height);
    if (!surface) {
        fprintf(stderr, "Failed to allocate PlutoVG surface\n");
        return 1;
    }

    plutovg_canvas_t *canvas = plutovg_canvas_create(surface);
    if (!canvas) {
        fprintf(stderr, "Failed to create PlutoVG canvas\n");
        plutovg_surface_destroy(surface);
        return 1;
    }

    // 1. Draw dark background
    plutovg_canvas_set_rgb(canvas, 0.06f, 0.09f, 0.16f); // #0f172a
    plutovg_canvas_rect(canvas, 0, 0, width, height);
    plutovg_canvas_fill(canvas);

    // 2. Draw glowing cyan rectangle card
    plutovg_canvas_set_rgb(canvas, 0.22f, 0.74f, 0.97f); // #38bdf8
    plutovg_canvas_rect(canvas, 50, 50, 300, 200);
    plutovg_canvas_stroke(canvas);

    // 3. Draw golden circular badge
    plutovg_canvas_set_rgb(canvas, 1.0f, 0.8f, 0.0f); // #ffcc00
    plutovg_canvas_circle(canvas, 200, 150, 45);
    plutovg_canvas_fill(canvas);

    printf("[1] Created software surface: %dx%d (%d bytes/row)\n",
           plutovg_surface_get_width(surface),
           plutovg_surface_get_height(surface),
           plutovg_surface_get_stride(surface));
    printf("[2] Rendered vector primitives (rectangles, strokes, circles) in memory.\n");

    plutovg_canvas_destroy(canvas);
    plutovg_surface_destroy(surface);

    printf("PlutoVG demonstration completed successfully.\n");
    return 0;
}
