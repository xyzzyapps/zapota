/**
 * @file demo_miniaudio.c
 * @brief miniaudio: version + sine waveform (no device I/O).
 */

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DEVICE_IO
#include "miniaudio.h"

#include <math.h>
#include <stdio.h>

int main(void) {
    printf("====================================================\n");
    printf("miniaudio Demonstration\n");
    printf("====================================================\n");
    printf("[1] miniaudio %s\n", ma_version_string());

    ma_waveform_config cfg = ma_waveform_config_init(ma_format_f32, 1, 48000, ma_waveform_type_sine, 0.5, 440.0);
    ma_waveform wave;
    if (ma_waveform_init(&cfg, &wave) != MA_SUCCESS) {
        fprintf(stderr, "ma_waveform_init failed\n");
        return 1;
    }

    float frames[480];
    ma_uint64 frames_read = 0;
    if (ma_waveform_read_pcm_frames(&wave, frames, 480, &frames_read) != MA_SUCCESS || frames_read != 480) {
        fprintf(stderr, "ma_waveform_read_pcm_frames failed\n");
        ma_waveform_uninit(&wave);
        return 1;
    }
    ma_waveform_uninit(&wave);

    double acc = 0.0;
    for (int i = 0; i < 480; i++) acc += (double)frames[i] * (double)frames[i];
    printf("[2] 440 Hz sine, 480 frames @ 48 kHz, RMS=%.4f\n", sqrt(acc / 480.0));
    printf("miniaudio demonstration completed successfully.\n");
    return 0;
}
