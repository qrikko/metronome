#include <stdint.h>
#include <miniaudio.h>

struct Metronome {
    double bpm;
    double base_bpm;
    float bpm_step;
    uint16_t interval;

    uint16_t next_step;
    uint8_t tick;

    uint8_t beats;
    uint8_t unit;

    uint8_t reset;

    ma_device device;
};

extern struct Metronome *metronome_state();
extern int metronome_setup();
extern void metronome_shutdown();

extern void metronome_set_beats(const int value);
extern void metronome_set_unit(const int value);
extern void metronome_dec_unit();
extern void metronome_inc_unit();
extern void metronome_dec_beats();
extern void metronome_inc_beats();
