/**
 * @file demo_dr_wav.c
 * @brief dr_wav: write a PCM buffer to memory, then decode it.
 */

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("====================================================\n");
    printf("dr_wav Demonstration\n");
    printf("====================================================\n");

    drwav_int16 pcm[64];
    for (int i = 0; i < 64; i++) {
        pcm[i] = (drwav_int16)((i % 8) * 1000);
    }

    drwav_data_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.channels = 1;
    fmt.sampleRate = 8000;
    fmt.bitsPerSample = 16;

    void *mem = NULL;
    size_t memsz = 0;
    drwav wav;
    if (!drwav_init_memory_write(&wav, &mem, &memsz, &fmt, NULL)) {
        fprintf(stderr, "drwav_init_memory_write failed\n");
        return 1;
    }
    if (drwav_write_pcm_frames(&wav, 64, pcm) != 64) {
        fprintf(stderr, "drwav_write_pcm_frames failed\n");
        drwav_uninit(&wav);
        return 1;
    }
    drwav_uninit(&wav);
    printf("[1] Wrote %zu-byte in-memory WAV (64 mono s16 frames)\n", memsz);

    if (!drwav_init_memory(&wav, mem, memsz, NULL)) {
        fprintf(stderr, "drwav_init_memory failed\n");
        drwav_free(mem, NULL);
        return 1;
    }
    drwav_int16 back[64];
    const drwav_uint64 n = drwav_read_pcm_frames_s16(&wav, 64, back);
    drwav_uninit(&wav);
    drwav_free(mem, NULL);

    if (n != 64 || memcmp(pcm, back, sizeof pcm) != 0) {
        fprintf(stderr, "round-trip mismatch (n=%llu)\n", (unsigned long long)n);
        return 1;
    }
    printf("[2] Round-trip decode matched 64 frames\n");
    printf("dr_wav demonstration completed successfully.\n");
    return 0;
}
