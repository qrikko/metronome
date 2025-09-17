#include <stdint.h>
#include <miniaudio.h>

struct Practice {
    uint8_t base_bpm;
    uint8_t step;
    uint8_t interval;
};

struct Metronome {
    uint8_t bpm;
    uint8_t beats;
    uint8_t unit;

    uint8_t bpm_step;
    uint8_t interval;
    uint8_t next_step;

    uint8_t base_bpm;

    uint8_t tick;
    uint8_t reset;
    ma_device device;
};

extern int metronome_setup(struct Metronome *m);
extern void metronome_shutdown(struct Metronome *m);

extern void metronome_set_beats(struct Metronome *m, const int value);
extern void metronome_set_unit(struct Metronome *m, const int value);
extern void metronome_dec_unit(struct Metronome *m);
extern void metronome_inc_unit(struct Metronome *m);
extern void metronome_dec_beats(struct Metronome *m);
extern void metronome_inc_beats(struct Metronome *m);
