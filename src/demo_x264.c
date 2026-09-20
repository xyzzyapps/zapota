/**
 * @file demo_x264.c
 * @brief x264 H.264 Video Encoder Demonstration.
 *
 * Encodes a synthetic luma gradient frame to H.264 NAL units using
 * libx264 in pure C mode (no NASM).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "x264.h"

#define FRAME_WIDTH  320
#define FRAME_HEIGHT 240
#define NUM_FRAMES   10

int main(void) {
    printf("====================================================\n");
    printf("x264 H.264 Video Encoder Demonstration\n");
    printf("====================================================\n");
    printf("x264 build info: %s (build %d)\n\n", X264_POINTVER, X264_BUILD);

    // 1. Configure encoder parameters
    x264_param_t param;
    if (x264_param_default_preset(&param, "ultrafast", "zerolatency") < 0) {
        fprintf(stderr, "Failed to set x264 preset\n");
        return 1;
    }

    param.i_csp = X264_CSP_I420;
    param.i_width = FRAME_WIDTH;
    param.i_height = FRAME_HEIGHT;
    param.b_annexb = 1;           // Annex B byte stream
    param.i_log_level = X264_LOG_ERROR;
    param.rc.i_rc_method = X264_RC_CQP;
    param.rc.i_qp_constant = 28;
    param.b_repeat_headers = 1;
    param.i_fps_num = 30;
    param.i_fps_den = 1;

    if (x264_param_apply_profile(&param, "baseline") < 0) {
        fprintf(stderr, "Failed to apply x264 baseline profile\n");
        return 1;
    }

    // 2. Open encoder
    x264_t *encoder = x264_encoder_open(&param);
    if (!encoder) {
        fprintf(stderr, "Failed to open x264 encoder\n");
        return 1;
    }
    printf("[1] x264 encoder opened: %dx%d @ 30fps, CQP=28, baseline profile\n",
           FRAME_WIDTH, FRAME_HEIGHT);

    // 3. Allocate picture buffer
    x264_picture_t pic_in, pic_out;
    if (x264_picture_alloc(&pic_in, X264_CSP_I420, FRAME_WIDTH, FRAME_HEIGHT) < 0) {
        fprintf(stderr, "Failed to allocate x264 picture\n");
        x264_encoder_close(encoder);
        return 1;
    }

    printf("[2] Encoding %d synthetic gradient frames:\n", NUM_FRAMES);

    int total_nal_bytes = 0;
    for (int frame_idx = 0; frame_idx < NUM_FRAMES; frame_idx++) {
        // Fill with a shifting luma gradient (synthetic content)
        for (int y = 0; y < FRAME_HEIGHT; y++) {
            for (int x = 0; x < FRAME_WIDTH; x++) {
                pic_in.img.plane[0][y * pic_in.img.i_stride[0] + x] =
                    (uint8_t)((x + y + frame_idx * 4) & 0xFF);
            }
        }
        // Fill Cb/Cr planes with neutral grey
        for (int y = 0; y < FRAME_HEIGHT / 2; y++) {
            memset(pic_in.img.plane[1] + y * pic_in.img.i_stride[1], 128, FRAME_WIDTH / 2);
            memset(pic_in.img.plane[2] + y * pic_in.img.i_stride[2], 128, FRAME_WIDTH / 2);
        }

        pic_in.i_pts = frame_idx;

        x264_nal_t *nals;
        int nal_count = 0;
        int frame_size = x264_encoder_encode(encoder, &nals, &nal_count, &pic_in, &pic_out);
        if (frame_size < 0) {
            fprintf(stderr, "  x264 encode error on frame %d\n", frame_idx);
            break;
        }

        total_nal_bytes += frame_size;
        printf("  Frame %2d -> %d NAL units, %d bytes\n", frame_idx + 1, nal_count, frame_size);
    }

    // 4. Flush delayed frames
    x264_nal_t *nals;
    int nal_count = 0;
    while (x264_encoder_delayed_frames(encoder)) {
        int fs = x264_encoder_encode(encoder, &nals, &nal_count, NULL, &pic_out);
        if (fs <= 0) break;
        total_nal_bytes += fs;
        printf("  Flushed delayed frame -> %d bytes\n", fs);
    }

    printf("\n[3] Total H.264 bitstream: %d bytes across %d frames\n",
           total_nal_bytes, NUM_FRAMES);

    x264_picture_clean(&pic_in);
    x264_encoder_close(encoder);
    printf("x264 demonstration completed successfully.\n");
    return 0;
}
