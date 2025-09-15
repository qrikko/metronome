#include <stdint.h>
#include <miniaudio.h>

struct Metronome {
    double bpm;
    double base_bpm;
    float bpm_step;
    uint16_t interval;

    uint16_t next_step;
    uint8_t tick;

    uint8_t nominator;
    uint8_t denominator;

    uint8_t reset;

    ma_device device;
};

extern struct Metronome *metronome_state();
extern int metronome_setup();
extern void metronome_shutdown();
extern void metronome_dec_note_length();
extern void metronome_inc_note_length();
extern void metronome_dec_beats_per_measure();
extern void metronome_inc_beats_per_measure();
