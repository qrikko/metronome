#include "metronome.h"
#include <stdint.h>

#define MA_IMPLEMENTATION
#include <miniaudio.h>

#include <math.h>
#include <stdio.h>

#define min(a, b) ({ \
     __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define max(a, b) ({ \
     __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define clamp(a, b, c) min(max(a, b), c)

#define SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (512)
#define CLICK_ONE_FREQUENCY (1880.0)
#define CLICK_FREQUENCY (880.0)

#define MIN_DENOMINATOR (2)
#define MAX_DENOMINATOR (16)
#define MIN_NOMINATOR (2)
#define MAX_NOMINATOR (32)

//static struct Metronome _metronome;

//struct Metronome *metronome_state() { return &_metronome; }

unsigned int power_of_two(unsigned int value) {
    if(value == 0) return 1;
    if(value == 1) return 1;

    unsigned int x = value -1;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    unsigned int upper = x+1;
    unsigned int lower = upper >> 1;

    if (value-lower <= upper-value) {
        return lower;
    } 
    return upper;
}

void metronome_set_beats(struct Metronome *m, const int value) {
    m->beats = clamp(value, MIN_NOMINATOR, MAX_NOMINATOR);
}
void metronome_set_unit(struct Metronome *m, const int value) {
    m->unit = clamp(power_of_two(value), MIN_DENOMINATOR, MAX_DENOMINATOR);
}
void metronome_dec_unit(struct Metronome *m) {
    m->unit = min(m->unit << 1, MAX_DENOMINATOR);
}
void metronome_inc_unit(struct Metronome *m) { 
    m->unit = max(m->unit >> 1, MIN_DENOMINATOR);
}
void metronome_dec_beats(struct Metronome *m) {
    m->beats = max(m->beats-1, MIN_NOMINATOR);
}
void metronome_inc_beats(struct Metronome *m) {
    m->beats = min(m->beats+1, MAX_NOMINATOR);
}

void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    float *out = (float*)output;
    (void)input;
    (void)device;
    struct Metronome *m = device->pUserData;

    static double phase = 0.0;
    static unsigned int beat_sample_counter = 0;
    static unsigned int beat_counter = 0;
    static const unsigned int click_samples = (unsigned int)(0.02 * SAMPLE_RATE); // 20ms click

    if (m->reset == 0x1) {
        phase = 0.0;
        beat_sample_counter = 0;
        beat_counter = 0;
        m->reset = 0x0;
    }

    const double beat_duration = (60.0/m->bpm) * (4.0/m->unit);
    const unsigned int beat_samples = (unsigned int)(beat_duration * SAMPLE_RATE);
                                                                                  
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
            beat_counter = (beat_counter +1) % m->beats;

            m->tick = beat_counter+1;

            if (beat_counter == 0 && m->interval > 0) {
                if (--m->next_step == 0) {
                    m->next_step = m->interval;
                    m->bpm += m->bpm_step;
                }
            }
        }
    }
}

int metronome_setup(struct Metronome *m) {
    m->tick = 1;
    {
        uint8_t data[6];
        const char *home = getenv("HOME");
        const char *rel = "/.local/share/metronome.state";
        char path[128];
        sprintf(path, "%s/%s", home, rel);
        FILE *f = fopen(path, "rb");
        if (f) {
            fread(data, sizeof(uint8_t), 6, f);

            m->bpm          = data[0]; 
            m->beats        = data[1];
            m->unit         = data[2];

            m->bpm_step     = data[3];
            m->interval     = data[4];
            m->next_step    = data[5];

            fclose(f);
        } else {
            m->bpm      = 80.0;
            m->beats    = 4;
            m->unit     = 4;
        }
    }

    ma_result result;
    ma_device_config device_config;

    device_config                   = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format   = ma_format_f32;
    device_config.playback.channels = 2;
    device_config.sampleRate        = SAMPLE_RATE;
    device_config.pUserData         = m;
    device_config.dataCallback      = data_callback;

    result = ma_device_init(NULL, &device_config, &m->device);
    if(result != MA_SUCCESS) {
        printf("FAILED to OPEN playback device!\n");
        return -1;
    }

    // Start device
    result = ma_device_start(&m->device);
    if(result != MA_SUCCESS) {
        printf("FAILED to START playback device\n");
        return -1;
    }
    return 0;
}

void metronome_shutdown(struct Metronome *m) {
    ma_device_uninit(&m->device);
}

