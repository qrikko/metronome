#include "metronome.h"
#include <stdint.h>
#include <stdlib.h>

#define MA_IMPLEMENTATION
#include <miniaudio.h>

#include <cJSON.h>

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
    m->track.measures[m->track.active_measure].beats = clamp(value, MIN_NOMINATOR, MAX_NOMINATOR);
}
void metronome_set_unit(struct Metronome *m, const int value) {
    m->track.measures[m->track.active_measure].unit = clamp(power_of_two(value), MIN_DENOMINATOR, MAX_DENOMINATOR);
}
void metronome_inc_unit(struct Metronome *m) { 
    uint8_t *unit = &m->track.measures[m->track.active_measure].unit;
    *unit = min(*unit << 1, MAX_DENOMINATOR);
}
void metronome_dec_unit(struct Metronome *m) {
    uint8_t *unit = &m->track.measures[m->track.active_measure].unit;
    *unit = max(*unit >> 1, MIN_DENOMINATOR);
}
void metronome_inc_beats(struct Metronome *m) {
    uint8_t *beats = &m->track.measures[m->track.active_measure].beats;
    *beats = min(*beats+1, MAX_NOMINATOR);
}
void metronome_dec_beats(struct Metronome *m) {
    uint8_t *beats = &m->track.measures[m->track.active_measure].beats;
    *beats = max(*beats-1, MIN_NOMINATOR);
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

    const double beat_duration = (60.0/m->bpm) * (4.0/m->track.measures[m->track.active_measure].unit);
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
            beat_counter = (beat_counter +1) % m->track.measures[m->track.active_measure].beats;

            if(beat_counter == 0) {
                m->track.active_measure = m->track.active_measure < m->track.measure_count ? m->track.active_measure+1 : 0;
            }
            
            m->tick = beat_counter+1;

            // @todo: need to only increase iteration IF all the tracks mesures were completed
            if (beat_counter == 0 && m->practice_active) {
                struct Practice *p = &m->practice[m->practice_current];
                if (m->track.active_measure == 0) {
                    p->iteration++;
                }

                if(p->iteration > p->interval-1) {
                    m->bpm += p->bpm_step;
                    p->iteration = 0;
                }
            }
        }
    }
}
void metronome_save(const struct Metronome *m, const char *path) {
    char path_buffer[128];
    if(path==NULL) {
        sprintf(path_buffer, "%s/.local/share/metronome.save", getenv("HOME"));
    } else {
        strcpy(path_buffer, path);
    }

    FILE *f = fopen(path_buffer, "w");
    cJSON *json = cJSON_CreateObject();
    cJSON *j_metronome = cJSON_AddObjectToObject(json, "metronome");
    { // base settings
        cJSON_AddNumberToObject(j_metronome, "base_bpm", m->base_bpm);
        cJSON_AddNumberToObject(j_metronome, "bpm", m->bpm);
    }
    { // Track settings
        cJSON *j_track = cJSON_AddObjectToObject(j_metronome, "track");
        cJSON_AddItemToObject(j_metronome, "track", j_track);

        cJSON *j_measure_obj = cJSON_AddObjectToObject(j_track, "measures");
        cJSON_AddNumberToObject(j_measure_obj, "measure_count", m->track.measure_count);

        cJSON *j_measures = cJSON_AddArrayToObject(j_measure_obj, "data");
        for(size_t i=0; i<=m->track.measure_count; ++i) {
            cJSON* j_measure = cJSON_CreateObject();
            cJSON_AddNumberToObject(j_measure, "beats", m->track.measures[i].beats);
            cJSON_AddNumberToObject(j_measure, "unit", m->track.measures[i].unit);

            cJSON_AddItemToArray(j_measures, j_measure);
        }
    }
    { // Practice settings
        cJSON *j_practice_obj = cJSON_AddObjectToObject(j_metronome, "practice");
        cJSON_AddNumberToObject(j_practice_obj, "count", m->practice_count);
        cJSON* j_practice_array = cJSON_AddArrayToObject(j_practice_obj, "data");

        for(size_t i=0; i<m->practice_count; ++i) {
            cJSON* j_practice = cJSON_CreateObject();
            cJSON_AddNumberToObject(j_practice, "bpm_from", m->practice[i].bpm_from);
            cJSON_AddNumberToObject(j_practice, "bpm_to", m->practice[i].bpm_to);
            cJSON_AddNumberToObject(j_practice, "bpm_step", m->practice[i].bpm_step);
            cJSON_AddNumberToObject(j_practice, "interval", m->practice[i].interval);

            cJSON_AddItemToArray(j_practice_array, j_practice);
        }
    }
    
    char *jsonstr = cJSON_Print(json);
    fprintf(f, "%s", jsonstr);

    fclose(f);
    cJSON_Delete(json);
    free(jsonstr);
}
void metronome_load(struct Metronome *m) {
    const char *home = getenv("HOME");
    const char *rel = "/.local/share/metronome.save";
    char path[128];
    sprintf(path, "%s/%s", home, rel);
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long filesize = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *buffer = (char *)malloc(filesize + 1);
        fread(buffer, 1, filesize, f);
        buffer[filesize] = '\0';

        fclose(f);

        cJSON *json = cJSON_Parse(buffer);
        if(json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if(error_ptr != NULL) {
                fprintf(stderr, "Error before: %s\n", error_ptr);
            }
            cJSON_Delete(json);
            free(buffer);
            return;
        }

        { // read savefile
            cJSON *jm = cJSON_GetObjectItemCaseSensitive(json, "metronome");

            cJSON* bpm = cJSON_GetObjectItemCaseSensitive(jm, "bpm");
            if(cJSON_IsNumber(bpm)) { m->bpm = bpm->valueint; }
            cJSON* base_bpm = cJSON_GetObjectItemCaseSensitive(jm, "base_bpm");
            if(cJSON_IsNumber(base_bpm)) { m->base_bpm = base_bpm->valueint; }

            { // track data
                cJSON *track = cJSON_GetObjectItemCaseSensitive(jm, "track");
                if(cJSON_IsObject(track)) {
                    cJSON *measures = cJSON_GetObjectItemCaseSensitive(track, "measures");
                    if(cJSON_IsObject(measures)) {
                        cJSON* measure_count = cJSON_GetObjectItemCaseSensitive(measures, "measure_count");
                        if(cJSON_IsNumber(measure_count)) {
                            m->track.measure_count = measure_count->valueint;
                        }
                    }
                    cJSON* measure_data = cJSON_GetObjectItemCaseSensitive(measures, "data");
                    if(cJSON_IsArray(measure_data)) {
                        for(size_t i=0; i<=m->track.measure_count; ++i) {
                            cJSON* measure = cJSON_GetArrayItem(measure_data, i);
                            if(cJSON_IsObject(measure)) {
                                cJSON* beats = cJSON_GetObjectItemCaseSensitive(measure, "beats");
                                if(cJSON_IsNumber(beats)) { m->track.measures[i].beats = beats->valueint; }

                                cJSON* unit = cJSON_GetObjectItemCaseSensitive(measure, "unit");
                                if(cJSON_IsNumber(unit)) { m->track.measures[i].unit = unit->valueint; }
                            }
                        }
                    }
                }
            }
            { // practice data
                cJSON* practices = cJSON_GetObjectItemCaseSensitive(jm, "practice");
                if(cJSON_IsObject(practices)) {
                    cJSON* practice_count = cJSON_GetObjectItemCaseSensitive(practices, "count");
                    if(cJSON_IsNumber(practice_count)) { m->practice_count = practice_count->valueint; }
                    if(m->practice_count > 0) { m->practice_active = 1; }

                    cJSON* practice_data = cJSON_GetObjectItemCaseSensitive(practices, "data");
                    for(size_t i=0; i<m->practice_count; ++i) {
                        m->practice[i].iteration = 0;

                        cJSON* practice = cJSON_GetArrayItem(practice_data, i);
                        if(cJSON_IsObject(practice)) {
                            cJSON* bpm_from = cJSON_GetObjectItemCaseSensitive(practice, "bpm_from");
                            if(cJSON_IsNumber(bpm_from)) { m->practice[i].bpm_from = bpm_from->valueint; }

                            cJSON* bpm_to = cJSON_GetObjectItemCaseSensitive(practice, "bpm_to");
                            if(cJSON_IsNumber(bpm_to)) { m->practice[i].bpm_to = bpm_to->valueint; }

                            cJSON* bpm_step = cJSON_GetObjectItemCaseSensitive(practice, "bpm_step");
                            if(cJSON_IsNumber(bpm_step)) { m->practice[i].bpm_step = bpm_step->valueint; }

                            cJSON* interval = cJSON_GetObjectItemCaseSensitive(practice, "interval");
                            if(cJSON_IsNumber(interval)) { m->practice[i].interval = interval->valueint; }
                        }
                    }
                }
            }
        }

        cJSON_Delete(json);
        free(buffer);
    } else {
        m->bpm      = 80.0;
        m->track.measures[0].beats    = 4;
        m->track.measures[0].unit     = 4;
    }
}
int metronome_setup(struct Metronome *m) {
    m->tick = 1;
    m->track.active_measure = 0;
    m->track.measure_count = 0;

    m->bpm = 42;
    m->track.measures[0].beats = 7;
    m->track.measures[0].unit = 8;

    m->practice_count = 0;
    
    metronome_load(m);

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
void metronome_insert_measure_at_start(struct Metronome *m) {
    assert(++m->track.measure_count < 10);
    
    for(int i=m->track.measure_count+1; i>0; --i) {
        m->track.measures[i] = m->track.measures[i-1];
    }

    m->track.active_measure=0;
    m->track.measures[0].beats = 4;
    m->track.measures[0].unit = 4;
}
void metronome_insert_measure_before(struct Metronome *m) {
    assert(++m->track.measure_count < 10);

    for(int i=m->track.measure_count+1; i>m->track.active_measure; --i) {
        m->track.measures[i] = m->track.measures[i-1];
    }

    m->track.measures[m->track.active_measure].beats = 4;
    m->track.measures[m->track.active_measure].unit = 4;
}
void metronome_insert_measure_after(struct Metronome *m) {
    assert(++m->track.measure_count < 10);

    for(int i=m->track.measure_count+1; i>m->track.active_measure+1; --i) {
        m->track.measures[i] = m->track.measures[i-1];
    }

    m->track.active_measure++;
    m->track.measures[m->track.active_measure].beats = 4;
    m->track.measures[m->track.active_measure].unit = 4;
}
void metronome_insert_measure_at_end(struct Metronome *m) {
    assert(++m->track.measure_count < 10);
    m->track.measures[m->track.measure_count].beats = 4;
    m->track.measures[m->track.measure_count].unit = 4;
    m->track.active_measure = m->track.measure_count;
}
void metronome_remove_measure(struct Metronome *m) {
    if (m->track.measure_count < 1) { return; }

    for(int i=m->track.active_measure; i<=m->track.measure_count; ++i) {
        m->track.measures[i] = m->track.measures[i+1];
    }
    m->track.measure_count--;
    m->track.active_measure =
        (m->track.active_measure > m->track.measure_count)
        ? m->track.measure_count
        : m->track.active_measure
    ;
}
void metronome_practice_set_from_bpm(struct Practice *p, uint8_t bpm) {
    p->bpm_from = (bpm>0 && bpm<255) ? bpm : 1;
}
