#define MA_IMPLEMENTATION
#include <miniaudio.h>

#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (512)
#define BEAT_DURATION_S (60.0/120.0)
#define CLICK_ONE_FREQUENCY (1880.0)
#define CLICK_FREQUENCY (880.0)

#define BEATS_PER_MEASURE 4

void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    float *out = (float*)output;
    (void)input;
    (void)device;

    static double phase = 0.0;
    static unsigned int beat_sample_counter = 0;
    static unsigned int beat_counter = 0;
    static const unsigned int beat_samples = (unsigned int)(BEAT_DURATION_S * SAMPLE_RATE);
    static const unsigned int click_samples = (unsigned int)(0.02 * SAMPLE_RATE); // 20ms click
                                                                                  
    for(ma_uint32 i=0; i<frame_count; ++i) {
        if(beat_sample_counter < click_samples) {
            float amplitude = sin(phase) * .5f;
            *out++ = amplitude;
            *out++ = amplitude; // for sterio output
            phase += 2.0 * M_PI * (beat_counter==0 ? CLICK_ONE_FREQUENCY : CLICK_FREQUENCY) / SAMPLE_RATE;
        } else {
            *out++ = 0.f;
            *out++ = 0.f;
        }
        beat_sample_counter++;

        if(beat_sample_counter >= beat_samples) {
            beat_sample_counter = 0;
            phase = 0.0;
            beat_counter = (beat_counter +1) % BEATS_PER_MEASURE;
        }
    }
}

int main (int argc, char **argv) {
    ma_result result;
    ma_device_config device_config;
    ma_device device;

    // configure device
    device_config                   = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format   = ma_format_f32;
    device_config.playback.channels = 2;
    device_config.sampleRate        = SAMPLE_RATE;
    device_config.dataCallback      = data_callback;

    // Initialize device
    result = ma_device_init(NULL, &device_config, &device);
    if(result != MA_SUCCESS) {
        printf("FAILED to OPEN playback device!\n");
        return -1;
    }

    // Start device
    result = ma_device_start(&device);
    if(result != MA_SUCCESS) {
        printf("FAILED to START playback device\n");
        return -1;
    }

    printf("Metronome running at 120 BPM. Press enter to exit\n");
    getchar();

    ma_device_uninit(&device);

    return 0;
}
