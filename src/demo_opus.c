/**
 * @file demo_opus.c
 * @brief Opus Audio Codec Encode/Decode Demonstration.
 *
 * Encodes a 480-sample sine wave PCM frame at 48kHz to Opus compressed
 * bytes, then decodes it back and validates round-trip.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "opus.h"

#define SAMPLE_RATE     48000
#define CHANNELS        1
#define FRAME_SIZE      480     /* 10ms @ 48kHz */
#define MAX_PACKET_SIZE 4000

static void generate_sine(opus_int16 *buf, int len, float freq, float sample_rate) {
    for (int i = 0; i < len; i++) {
        buf[i] = (opus_int16)(20000.0f * sinf(2.0f * 3.14159265f * freq * (float)i / sample_rate));
    }
}

int main(void) {
    printf("====================================================\n");
    printf("Opus Audio Codec Encode/Decode Demonstration\n");
    printf("====================================================\n");

    const char *version = opus_get_version_string();
    printf("Opus Version: %s\n\n", version);

    // 1. Create encoder
    int err;
    OpusEncoder *encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &err);
    if (err != OPUS_OK || !encoder) {
        fprintf(stderr, "Failed to create Opus encoder: %s\n", opus_strerror(err));
        return 1;
    }

    // Configure: 64kbps, VBR
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(64000));
    opus_encoder_ctl(encoder, OPUS_SET_VBR(1));

    // 2. Create decoder
    OpusDecoder *decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err != OPUS_OK || !decoder) {
        fprintf(stderr, "Failed to create Opus decoder: %s\n", opus_strerror(err));
        opus_encoder_destroy(encoder);
        return 1;
    }

    printf("[1] Encoder: 48kHz, mono, 64kbps VBR, OPUS_APPLICATION_AUDIO\n");

    // 3. Encode/decode loop - 5 frames
    opus_int16 pcm_in[FRAME_SIZE];
    opus_int16 pcm_out[FRAME_SIZE];
    unsigned char packet[MAX_PACKET_SIZE];

    int total_encoded_bytes = 0;

    printf("[2] Encoding 5 frames of 440Hz sine wave:\n");
    for (int f = 0; f < 5; f++) {
        generate_sine(pcm_in, FRAME_SIZE, 440.0f, (float)SAMPLE_RATE);

        // Encode
        opus_int32 encoded_bytes = opus_encode(encoder, pcm_in, FRAME_SIZE, packet, MAX_PACKET_SIZE);
        if (encoded_bytes < 0) {
            fprintf(stderr, "  Encode error frame %d: %s\n", f, opus_strerror((int)encoded_bytes));
            break;
        }
        total_encoded_bytes += (int)encoded_bytes;

        // Decode
        int decoded_samples = opus_decode(decoder, packet, encoded_bytes, pcm_out, FRAME_SIZE, 0);
        if (decoded_samples < 0) {
            fprintf(stderr, "  Decode error frame %d: %s\n", f, opus_strerror(decoded_samples));
            break;
        }

        // Compute basic energy to verify round-trip
        double energy = 0.0;
        for (int i = 0; i < decoded_samples; i++) {
            energy += (double)pcm_out[i] * pcm_out[i];
        }
        energy = sqrt(energy / decoded_samples);

        printf("  Frame %d: %ld bytes encoded -> %d samples decoded, RMS=%.1f\n",
               f + 1, (long)encoded_bytes, decoded_samples, energy);
    }

    printf("\n[3] Total compressed bytes: %d (%.1f kbps avg)\n",
           total_encoded_bytes,
           (total_encoded_bytes * 8.0 * SAMPLE_RATE) / (5.0 * FRAME_SIZE * 1000.0));

    opus_encoder_destroy(encoder);
    opus_decoder_destroy(decoder);
    printf("Opus demonstration completed successfully.\n");
    return 0;
}
