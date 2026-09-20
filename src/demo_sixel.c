/**
 * demo_sixel.c - libsixel terminal graphics demo
 *
 * Generates a synthetic 64x64 RGB gradient palette image, encodes it
 * to Sixel character stream using the libsixel high-level API, and
 * writes the result to stdout (visible on Sixel-capable terminals).
 *
 * Also prints the encoded byte count for non-Sixel terminals.
 */

#include "sixel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMG_W 64
#define IMG_H 64

static int sixel_write_callback(char *data, int size, void *priv) {
    return (int)fwrite(data, 1, (size_t)size, (FILE *)priv);
}

int main(void) {
    fprintf(stderr, "=== libsixel Demo ===\n");

    /* Allocate 3-byte-per-pixel RGB image */
    unsigned char *pixels = (unsigned char *)malloc(IMG_W * IMG_H * 3);
    if (!pixels) {
        fprintf(stderr, "OOM\n");
        return 1;
    }

    /* Generate a simple RGB gradient */
    for (int y = 0; y < IMG_H; y++) {
        for (int x = 0; x < IMG_W; x++) {
            int idx = (y * IMG_W + x) * 3;
            pixels[idx + 0] = (unsigned char)(x * 255 / (IMG_W - 1));         /* R */
            pixels[idx + 1] = (unsigned char)(y * 255 / (IMG_H - 1));         /* G */
            pixels[idx + 2] = (unsigned char)((x + y) * 255 / (IMG_W + IMG_H - 2)); /* B */
        }
    }

    /* Create output object writing to stdout */
    sixel_output_t *output = NULL;
    SIXELSTATUS status = sixel_output_new(&output, sixel_write_callback, stdout, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_output_new failed: 0x%x\n", status);
        free(pixels);
        return 1;
    }

    /* Create dithering context: 256-color palette, FS dithering */
    sixel_dither_t *dither = NULL;
    status = sixel_dither_new(&dither, 256, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_dither_new failed: 0x%x\n", status);
        sixel_output_unref(output);
        free(pixels);
        return 1;
    }

    /* Sample palette from the image */
    status = sixel_dither_initialize(dither, pixels, IMG_W, IMG_H,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     SIXEL_LARGE_NORM,
                                     SIXEL_REP_CENTER_BOX,
                                     SIXEL_QUALITY_LOW);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_dither_initialize failed: 0x%x\n", status);
        sixel_dither_unref(dither);
        sixel_output_unref(output);
        free(pixels);
        return 1;
    }

    /* Encode and write Sixel stream to stdout */
    fprintf(stderr, "Encoding %dx%d gradient to Sixel...\n", IMG_W, IMG_H);
    status = sixel_encode(pixels, IMG_W, IMG_H, 3, dither, output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_encode failed: 0x%x\n", status);
        sixel_dither_unref(dither);
        sixel_output_unref(output);
        free(pixels);
        return 1;
    }

    sixel_dither_unref(dither);
    sixel_output_unref(output);
    free(pixels);

    fprintf(stderr, "libsixel demo complete. Sixel stream written to stdout.\n");
    return 0;
}
