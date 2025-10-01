#include <stdint.h>
#include <miniaudio.h>

#define MAX_TRACKS              16
#define MAX_MEASURES_PER_TRACK  32
#define MAX_PRACTICE_SETS       32

struct Measure {
    uint8_t beats;
    uint8_t unit;
};

struct Track {
    struct Measure measures[MAX_MEASURES_PER_TRACK];
    uint8_t selection;
    uint8_t active_measure;
    uint8_t measure_count;
};

struct Practice {
    uint8_t bpm_from;
    uint8_t bpm_to;
    uint8_t bpm_step;
    uint8_t interval;
    uint8_t iteration;
};

struct Metronome {
    uint8_t bpm;
    uint8_t bpm_step; // what does it do? and do I need it for both practice and metronome?

    uint8_t practice_count;
    uint8_t practice_current;
    uint8_t practice_active;

    struct Practice practice[MAX_PRACTICE_SETS];
    struct Track track; 

    uint8_t base_bpm;

    uint8_t tick;
    uint8_t reset;
    ma_device device;
};

extern int metronome_setup(struct Metronome *m);
extern void metronome_shutdown(struct Metronome *m);

extern void metronome_save(const struct Metronome *m, const char *path);

extern void metronome_set_beats(struct Metronome *m, const int value);
extern void metronome_set_unit(struct Metronome *m, const int value);
extern void metronome_dec_unit(struct Metronome *m);
extern void metronome_inc_unit(struct Metronome *m);
extern void metronome_dec_beats(struct Metronome *m);
extern void metronome_inc_beats(struct Metronome *m);

extern void metronome_insert_measure_at_start(struct Metronome *m);
extern void metronome_insert_measure_before(struct Metronome *m);
extern void metronome_insert_measure_after(struct Metronome *m);
extern void metronome_insert_measure_at_end(struct Metronome *m);

extern void metronome_remove_measure(struct Metronome *m);

extern void metronome_practice_set_from_bpm(struct Practice *, uint8_t bpm);
