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
    uint8_t selected;
    uint8_t size;
};

struct Practice {
    uint8_t bpm_from;
    uint8_t bpm_to;
    uint8_t bpm_step;
    uint8_t interval;
    uint8_t measures_until_next_step;
};

struct Metronome {
    uint8_t bpm;
    uint8_t beats;
    uint8_t unit;
    uint8_t bpm_step; // what does it do? and do I need it for both practice and metronome?

    uint8_t practice_count;
    uint8_t practice_current;
    uint8_t practice_active;

    struct Practice practice[MAX_PRACTICE_SETS];
    struct Track track; 

    // @todo: replace with using "Practice structs"
    //uint8_t interval;
    //uint8_t next_step;
    // @end

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

extern void metronome_add_track(struct Metronome *m);
