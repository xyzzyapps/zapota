/**
 * @file demo_nuklear.c
 * @brief Nuklear Immediate-Mode GUI Engine Demonstration.
 */

#include <stdio.h>
#include <string.h>

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_IO
#define NK_IMPLEMENTATION
#include "nuklear.h"

static float font_get_text_width(nk_handle handle, float height, const char *text, int len) {
    (void)handle;
    (void)height;
    return (float)len * 8.0f; // estimated 8px monospace width
}

int main(void) {
    printf("====================================================\n");
    printf("Nuklear Immediate-Mode GUI Engine Demonstration\n");
    printf("====================================================\n");

    struct nk_context ctx;
    struct nk_user_font font;
    memset(&font, 0, sizeof(font));
    font.height = 14.0f;
    font.width = font_get_text_width;

    if (!nk_init_default(&ctx, &font)) {
        fprintf(stderr, "Failed to initialize Nuklear context\n");
        return 1;
    }

    printf("[1] Initialized Nuklear immediate-mode context in memory.\n");

    // Simulate 3 UI frames
    for (int frame = 1; frame <= 3; ++frame) {
        nk_input_begin(&ctx);
        nk_input_end(&ctx);

        if (nk_begin(&ctx, "ZigCrossPlatformDemo", nk_rect(50, 50, 240, 200),
                     NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE)) {

            nk_layout_row_dynamic(&ctx, 30, 1);
            nk_label(&ctx, "Welcome to Nuklear GUI!", NK_TEXT_LEFT);

            nk_layout_row_dynamic(&ctx, 30, 2);
            if (nk_button_label(&ctx, "Button 1")) {
                printf("  -> Button 1 pressed!\n");
            }
            if (nk_button_label(&ctx, "Button 2")) {
                printf("  -> Button 2 pressed!\n");
            }

            nk_layout_row_dynamic(&ctx, 25, 1);
            static int property_val = 42;
            nk_property_int(&ctx, "Score:", 0, &property_val, 100, 1, 1);
        }
        nk_end(&ctx);

        printf("  Frame %d: Processed window layout and controls successfully.\n", frame);
        nk_clear(&ctx);
    }

    nk_free(&ctx);
    printf("\nNuklear demonstration completed successfully.\n");
    return 0;
}
