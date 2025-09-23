#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "metronome.h"

#define COMMAND_MAX_LEN 256

struct Coord { int x, y; };

typedef enum {NORMAL_MODE, COMMAND_MODE, PAUSE_MODE} ProgramState;
const char* mode_string(const ProgramState mode) {
    switch(mode) {
        case NORMAL_MODE:   return "-- NORMAL --";
        case PAUSE_MODE:    return "-- PAUSE --";
        case COMMAND_MODE:  return "-- COMMAND --";
        default:            return "-- UNKNOWN --";
    }
}

typedef enum {BEAT_SELECTED, UNIT_SELECTED, MEASURE_NONE_SELECTED} MeaureSelection;

void init_tui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(0);
}

void tui_print(const struct Metronome *m, WINDOW *win, const ProgramState state) {
    int x, y;
    getmaxyx(win, y, x);
    int len = snprintf(NULL, 0, "Metronome at %d BPM", m->bpm);
    int left = (x-len)/2;

    wmove(win, 5, 0);
    wclrtoeol(win);
    mvwprintw(win, 3, left, "Metronome at %d BPM", m->bpm);
    wclrtoeol(win);
    wmove(win, 4, 0);
    wclrtoeol(win);

    MeaureSelection selection = (state == PAUSE_MODE) ? m->track.selection : MEASURE_NONE_SELECTED;

    for(uint8_t i=0; i<=m->track.measure_count; ++i) {
        uint8_t b = m->track.measures[i].beats;
        uint8_t u = m->track.measures[i].unit;
        int offset = snprintf(NULL, 0, "[%d/%d]", b, u);

        wmove(win, 4, left);
        int selected_measure = (m->track.active_measure == i);
        if (selected_measure) { wattron(win, COLOR_PAIR(2)); }
        wprintw(win, "[");

        if(selected_measure && selection == BEAT_SELECTED) { wattron(win, A_UNDERLINE); }
        wprintw(win, "%d", m->track.measures[i].beats);
        if(selected_measure && selection == BEAT_SELECTED) { wattroff(win, A_UNDERLINE); }

        wprintw(win, "/");

        if(selected_measure && selection == UNIT_SELECTED) { wattron(win, A_UNDERLINE); }
        wprintw(win, "%d", m->track.measures[i].unit);
        if(selected_measure && selection == UNIT_SELECTED) { wattroff(win, A_UNDERLINE); }

        wprintw(win, "]");

        if (selected_measure) { wattroff(win, COLOR_PAIR(2)); }
        left+=offset;
    }
    box(win, 0, 0);
    wrefresh(win);
}

void print_practice_info(const struct Metronome *m, WINDOW *win) {
    const struct Practice *p = &m->practice[m->practice_current];
    int x, y;
    getmaxyx(win, y, x);

    const struct Track *t = &m->track;
    const int measures_left = p->interval - p->iteration;

    const char *format = "Adding %d BPM in %d repititions";
    int len = snprintf(
        NULL, 0, 
        format, 
        p->bpm_step, measures_left//, p->bpm_from, p->bpm_to, p->interval
    );

    if (p->interval > 0) {
        mvwprintw(
            win,
            2,
            (x-len)/2, 
            format, 
            p->bpm_step, measures_left//, p->bpm_from, p->bpm_to, p->interval
        );
    }
    wrefresh(win);
}


void update_display(struct Metronome *m, WINDOW *win, const ProgramState mode) {
    wclear(win);

    box(win, 0, 0);
    
    int x, y;
    getmaxyx(win, y, x);
    
    if (m->practice_active) {
        print_practice_info(m, win);
    }

    {
        tui_print(m, win, mode);

        const int margin = 5;
        uint8_t len = getmaxx(win) -2*margin;
        int step = len/(m->track.measures[m->track.active_measure].beats-1);

        for(int i=0; i<m->track.measures[m->track.active_measure].beats; ++i) {
            mvwprintw(
                win,
                y/2 +1, 
                i*step + margin, 
                "%d", 
                i+1
            );
        }
        mvwprintw(
            win,
            y/2 +2, 
            (x-len)/2 + step*(m->tick!=0 ? m->tick-1 : m->tick), 
            "^" 
        );
        if (m->tick == 1) {
            wbkgd(win, COLOR_PAIR(1));
        } else {
            wbkgd(win, 0);
        }
    }

    mvwprintw(stdscr, LINES -2, 0, mode_string(mode));//"-- NORMAL --");
    refresh();
    wrefresh(win);
}

int handle_command_mode(struct Metronome *m) {
    char cmd[COMMAND_MAX_LEN];
    int ch;
    int result = 0;

    ma_device_stop(&m->device);
    echo();
    curs_set(1);
    timeout(20000);

    mvprintw(LINES -2, 0, "-- COMMAND --");
    mvprintw(LINES-1, 0, ":");
    refresh();

    wgetnstr(stdscr, cmd, sizeof(cmd)-1);
    char *token = strtok(cmd, " ");
    if(token) {
        if(strcmp(token, "bpm") == 0) {
            char *value_str = strtok(NULL, " ");
            if(value_str) {
                m->bpm = (double)atoi(value_str);
                m->base_bpm = m->bpm;
                m->tick = 1;
                m->reset = 0x1;
            }
        } else if (strcmp(token, "beats") == 0) {
            char *value = strtok(NULL, " ");
            if (value) {
                metronome_set_beats(m, atoi(value));
            } else {
                mvprintw(LINES-1, 0, ":beats = ");
                refresh();
                {
                    char beats[3];
                    wgetnstr(stdscr, beats, sizeof(beats)-1);
                    metronome_set_beats(m, atoi(beats));
                }
            }
        } else if (strcmp(token, "unit") == 0) {
            char *value = strtok(NULL, " ");
            if (value) {
                metronome_set_unit(m, atoi(value));
            } else {
                mvprintw(LINES-1, 0, ":unit = ");
                refresh();
                {
                    char unit[3];
                    wgetnstr(stdscr, unit, sizeof(unit)-1);
                    metronome_set_unit(m, atoi(unit));
                }
            }
        }

        else if (strcmp(token, "signature") == 0 || strcmp(token, "ts") == 0) {
            char *beats = strtok(NULL, " ");
            char *unit = strtok(NULL, " ");
            if (beats && unit) {
                metronome_set_beats(m, atoi(beats));
                metronome_set_unit(m, atoi(unit));
            } else {
                mvprintw(LINES-1, 0, ":beats = ");
                refresh();
                {
                    char beats[3];
                    wgetnstr(stdscr, beats, sizeof(beats)-1);
                    metronome_set_beats(m, atoi(beats));
                }
                {
                    move(LINES-1, 0);
                    clrtoeol();
                    mvprintw(LINES-1, 0, ":unit = ");
                    refresh();
                    char unit[3];
                    wgetnstr(stdscr, unit, sizeof(unit)-1);
                    metronome_set_unit(m, atoi(unit));
                }
            }
            m->reset = 0x1;
            m->tick = 1;
        } else if(strcmp(token, "practice") == 0) {
            char *value_str = strtok(NULL, " ");
            if(value_str) {
                if(strcmp(value_str, "off") == 0) {
                    m->reset = 0x1;
                    m->practice_active = 0x0;
                    m->tick = 1;
                    m->practice[m->practice_current].interval = 0;
                }
            } else {
                struct Practice p = {.iteration=0};
                {
                    move(LINES-1, 0);
                    clrtoeol();
                    printw(":from bpm = ");
                    refresh();
                    char bpm_step_str[3];
                    wgetnstr(stdscr, bpm_step_str, sizeof(bpm_step_str)-1);
                    p.bpm_from = (float)atoi(bpm_step_str);
                }
                {
                    move(LINES-1, 0);
                    clrtoeol();
                    printw(":to bpm = ");
                    refresh();
                    char bpm_step_str[3];
                    wgetnstr(stdscr, bpm_step_str, sizeof(bpm_step_str)-1);
                    p.bpm_to = (float)atoi(bpm_step_str);
                }
                {
                    move(LINES-1, 0);
                    clrtoeol();
                    printw(":bpm step = ");
                    refresh();
                    char bpm_step_str[3];
                    wgetnstr(stdscr, bpm_step_str, sizeof(bpm_step_str)-1);
                    p.bpm_step = (float)atoi(bpm_step_str);
                }
                {
                    move(LINES-1, 0);
                    clrtoeol();
                    printw(":interval = ");
                    refresh();
                    char interval[3];
                    wgetnstr(stdscr, interval, sizeof(interval)-1);
                    p.interval = atoi(interval);
                }
                m->reset = 0x1;
                m->tick = 1;
                m->practice_current = m->practice_count;
                m->bpm = p.bpm_from;

                memcpy(&m->practice[m->practice_count++], &p, sizeof(p));

                m->track.active_measure = 0;
                m->practice_active = 0x1;
            }
        } else if(strcmp(token, "o") == 0) {
            metronome_add_measure(m);

        } else if(strcmp(token, "reset") == 0) {
            m->bpm = m->base_bpm;
            //m->next_step = m->interval;
            m->reset = 0x1;
            m->tick = 1;
        } else if(strcmp(token, "w") == 0) {
            const char *home = getenv("HOME");
            const char *rel = "/.local/share/metronome.state";
            char path[128];
            sprintf(path, "%s/%s", home, rel);

            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(m, sizeof(uint8_t), 6, f);
                fclose(f);
            }
        }

        else if(strcmp(token, "quit") == 0 || strcmp(token, "q") == 0) {
            result = 1;
        }
    }
    noecho();
    curs_set(0);
    timeout(0);
    move(LINES-1, 0);
    clrtoeol();
    return result;
}

int main(int argc, char **argv) {
    struct Metronome metronome;
    metronome_setup(&metronome);

    ProgramState program_state = NORMAL_MODE;

    init_tui();
    {
        start_color();
        use_default_colors();
        WINDOW *win;
        {
            int ymax, xmax;
            getmaxyx(stdscr, ymax, xmax);
            const int margin = 10;
            init_pair(1, COLOR_YELLOW, -1);
            init_pair(2, COLOR_RED, COLOR_BLACK);
            win = newwin(ymax/2, xmax-2*margin, ymax/4, margin);
            wtimeout(win, 0);
            update_display(&metronome, win, program_state);
            wrefresh(win);
        }

        struct Coord input_pos = {.x=1, .y=getmaxy(stdscr)};

        uint8_t keep_running = 0x1;
        while (keep_running == 0x1) {
            if(program_state == NORMAL_MODE || program_state == PAUSE_MODE) {
                char cmd = wgetch(win);

                switch(cmd) {
                    case 'n': {
                        if(program_state == PAUSE_MODE) {
                            metronome.track.active_measure = 
                                metronome.track.active_measure < metronome.track.measure_count
                                ? metronome.track.active_measure+1
                                : 0;
                            tui_print(&metronome, win, program_state);
                        }
                        break;
                    }
                    case 'p': {
                        if(program_state == PAUSE_MODE) {
                            metronome.track.active_measure = metronome.track.active_measure > 0 
                                ? metronome.track.active_measure-1 
                                : metronome.track.measure_count;
                            tui_print(&metronome, win, program_state);
                        }
                        break;
                    }
                    //case 'J':
                    case 'j': {
                        if(program_state == PAUSE_MODE) {
                            if(metronome.track.selection == 0) {
                                metronome_dec_beats(&metronome);
                            } else {
                                metronome_dec_unit(&metronome);
                            }
                        }
                        //metronome.bpm -= (cmd=='j') ? 1 : 5;
                        tui_print(&metronome, win, program_state);
                        break;
                    } 
                    //case 'K':
                    case 'k': {
                        if(program_state == PAUSE_MODE) {
                            if(metronome.track.selection == 0) {
                                metronome_inc_beats(&metronome);
                            } else {
                                metronome_inc_unit(&metronome);
                            }
                        }
                        //metronome.bpm += (cmd=='k') ? 1 : 5;
                        tui_print(&metronome, win, program_state);
                        break;
                    } 
                    case 'h': {
                        if(program_state == PAUSE_MODE) {
                            metronome.track.selection = metronome.track.selection==1 ? 0 : 1;
                        } else {
                            metronome_dec_beats(&metronome);
                        }
                        tui_print(&metronome, win, program_state);
                        break;
                    }
                    case 'l': {
                        if(program_state == PAUSE_MODE) {
                            metronome.track.selection = metronome.track.selection==0 ? 1 : 0;
                        } else {
                            metronome_inc_beats(&metronome);
                        }
                        tui_print(&metronome, win, program_state);
                        break;
                    }
                    case 'L': {
                        metronome_dec_unit(&metronome);
                        tui_print(&metronome, win, program_state);
                        break;
                    }
                    case 'H': {
                        metronome_inc_unit(&metronome);
                        tui_print(&metronome, win, program_state);
                        break;
                    }
                    case 'o': {
                        metronome_add_measure(&metronome);
                        tui_print(&metronome, win, program_state);
                        break;
                    }
                    case 'd': {
                        metronome_remove_measure(&metronome);
                        tui_print(&metronome, win, program_state);
                        break;
                    }
                    case ':': {
                        program_state = COMMAND_MODE;
                        move(input_pos.y, input_pos.x);
                        printw(":");
                        refresh();
                        char buffer[32];
                        getnstr(buffer, 16);
                        update_display(&metronome, win, program_state);
                        break;
                    }
                    case ' ': {
                        if(program_state == NORMAL_MODE) {
                            program_state = PAUSE_MODE;
                            ma_device_stop(&metronome.device);
                        } else if(program_state == PAUSE_MODE) {
                            program_state = NORMAL_MODE;
                            metronome.reset = 1;
                            metronome.tick = 1;
                            metronome.track.active_measure = 0;
                            ma_device_start(&metronome.device);
                        }
                        update_display(&metronome, win, program_state);

                        break;
                    }
                }
            } 

            if(program_state == COMMAND_MODE) {
                if(handle_command_mode(&metronome) == 1) {
                    keep_running = 0x0;
                }
                program_state = NORMAL_MODE;
                update_display(&metronome, win, program_state);
                ma_device_start(&metronome.device);
            }

            if(metronome.tick > 0) {
                update_display(&metronome, win, program_state);
                metronome.tick = 0;
            } 
            wrefresh(win);
        }
    }
    endwin();
    metronome_shutdown(&metronome);

    return 0;
}
