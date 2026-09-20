/**
 * @file demo_lame.c
 * @brief LAME MP3 Audio Encoder Demonstration.
 *
 * Generates a stereo 440 Hz sine wave PCM buffer and encodes it into
 * an MP3 bitstream using libmp3lame.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "lame.h"

#define SAMPLE_RATE     44100
#define NUM_CHANNELS    2
#define DURATION_SEC    1
#define TOTAL_SAMPLES   (SAMPLE_RATE * DURATION_SEC)
#define MP3_BUFFER_SIZE (1024 * 64)

static void generate_stereo_sine(short *pcm_l, short *pcm_r, int num_samples, float freq_l, float freq_r, float sample_rate) {
    const float pi = 3.14159265358979323846f;
    for (int i = 0; i < num_samples; i++) {
        float sample_time = (float)i / sample_rate;
        pcm_l[i] = (short)(16000.0f * sinf(2.0f * pi * freq_l * sample_time));
        pcm_r[i] = (short)(16000.0f * sinf(2.0f * pi * freq_r * sample_time));
    }
}

int main(void) {
    printf("====================================================\n");
    printf("LAME MP3 Audio Encoder Demonstration\n");
    printf("====================================================\n");

    const char *version = get_lame_version();
    printf("LAME Version: %s\n\n", version ? version : "unknown");

    // 1. Initialize LAME encoder
    lame_t gfp = lame_init();
    if (!gfp) {
        fprintf(stderr, "Error: lame_init() failed\n");
        return 1;
    }

    lame_set_in_samplerate(gfp, SAMPLE_RATE);
    lame_set_num_channels(gfp, NUM_CHANNELS);
    lame_set_out_samplerate(gfp, SAMPLE_RATE);
    lame_set_brate(gfp, 128); // 128 kbps
    lame_set_mode(gfp, STEREO);
    lame_set_quality(gfp, 2); // High quality

    int init_res = lame_init_params(gfp);
    if (init_res < 0) {
        fprintf(stderr, "Error: lame_init_params failed with code %d\n", init_res);
        lame_close(gfp);
        return 1;
    }

    printf("Encoder configured:\n");
    printf("  Input Rate:    %d Hz\n", lame_get_in_samplerate(gfp));
    printf("  Output Rate:   %d Hz\n", lame_get_out_samplerate(gfp));
    printf("  Channels:      %d\n", lame_get_num_channels(gfp));
    printf("  Bitrate:       %d kbps\n", lame_get_brate(gfp));

    // 2. Synthesize PCM data (Left = 440 Hz A4, Right = 880 Hz A5)
    short *pcm_l = (short *)malloc(TOTAL_SAMPLES * sizeof(short));
    short *pcm_r = (short *)malloc(TOTAL_SAMPLES * sizeof(short));
    unsigned char *mp3_buf = (unsigned char *)malloc(MP3_BUFFER_SIZE);

    if (!pcm_l || !pcm_r || !mp3_buf) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        if (pcm_l) free(pcm_l);
        if (pcm_r) free(pcm_r);
        if (mp3_buf) free(mp3_buf);
        lame_close(gfp);
        return 1;
    }

    printf("\nSynthesizing %d stereo samples (1.0 second)...\n", TOTAL_SAMPLES);
    generate_stereo_sine(pcm_l, pcm_r, TOTAL_SAMPLES, 440.0f, 880.0f, (float)SAMPLE_RATE);

    // 3. Encode to MP3
    int bytes_written = lame_encode_buffer(gfp, pcm_l, pcm_r, TOTAL_SAMPLES, mp3_buf, MP3_BUFFER_SIZE);
    if (bytes_written < 0) {
        fprintf(stderr, "Error: lame_encode_buffer returned %d\n", bytes_written);
        free(pcm_l);
        free(pcm_r);
        free(mp3_buf);
        lame_close(gfp);
        return 1;
    }

    int total_bytes = bytes_written;

    // Flush remaining frames
    int flush_bytes = lame_encode_flush(gfp, mp3_buf + total_bytes, MP3_BUFFER_SIZE - total_bytes);
    if (flush_bytes > 0) {
        total_bytes += flush_bytes;
    }

    printf("MP3 Encoding successful:\n");
    printf("  Payload encoded: %d bytes\n", bytes_written);
    printf("  Flushed bytes:   %d bytes\n", flush_bytes);
    printf("  Total MP3 size:  %d bytes\n", total_bytes);

    // Clean up
    free(pcm_l);
    free(pcm_r);
    free(mp3_buf);
    lame_close(gfp);

    printf("\n====================================================\n");
    printf("LAME MP3 demonstration finished successfully.\n");
    printf("====================================================\n");
    return 0;
}
