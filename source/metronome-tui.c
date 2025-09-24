#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "metronome.h"

#define COMMAND_MAX_LEN 256

struct Coord { int x, y; };

typedef enum {NORMAL_MODE, COMMAND_MODE, PAUSE_MODE} ProgramMode;
const char* mode_string(const ProgramMode mode) {
    switch(mode) {
        case NORMAL_MODE:   return "-- NORMAL --";
        case PAUSE_MODE:    return "-- PAUSE --";
        case COMMAND_MODE:  return "-- COMMAND --";
        default:            return "-- UNKNOWN --";
    }
}

typedef enum {BEAT_SELECTED, UNIT_SELECTED, BPM_SELECTED, NONE_SELECTED} SelectionState;

void init_tui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(0);
}

void tui_print(const struct Metronome *m, WINDOW *win, const ProgramMode mode, const SelectionState state) {
    int x, y;
    getmaxyx(win, y, x);
    wmove(win, 5, 0);
    wclrtoeol(win);
    wclrtoeol(win);
    wmove(win, 4, 0);
    wclrtoeol(win);

    SelectionState selection = (mode==PAUSE_MODE) ? state : NONE_SELECTED;

    int len = snprintf(NULL, 0, "Metronome at %d BPM", m->bpm);
    int left = (x-len)/2;

    mvwprintw(win, 3, left, "Metronome at ");
    if(selection == BPM_SELECTED) { wattron(win, COLOR_PAIR(2)); wattron(win, A_UNDERLINE); }
    wprintw(win, "%d", m->bpm);
    if(selection == BPM_SELECTED) { wattroff(win, COLOR_PAIR(2)); wattroff(win, A_UNDERLINE); }
    wprintw(win, " BPM");

    for(uint8_t i=0; i<=m->track.measure_count; ++i) {
        uint8_t b = m->track.measures[i].beats;
        uint8_t u = m->track.measures[i].unit;
        int offset = snprintf(NULL, 0, "[%d/%d]", b, u);

        wmove(win, 4, left);
        int selected_measure = (m->track.active_measure == i);
        if(selected_measure && (state<BPM_SELECTED || mode==NORMAL_MODE)) { wattron(win, COLOR_PAIR(2)); }
        wprintw(win, "[");

        if(selected_measure && selection == BEAT_SELECTED) { wattron(win, A_UNDERLINE); }
        wprintw(win, "%d", m->track.measures[i].beats);
        if(selected_measure && selection == BEAT_SELECTED) { wattroff(win, A_UNDERLINE); }

        wprintw(win, "/");

        if(selected_measure && selection == UNIT_SELECTED) { wattron(win, A_UNDERLINE); }
        wprintw(win, "%d", m->track.measures[i].unit);
        if(selected_measure && selection == UNIT_SELECTED) { wattroff(win, A_UNDERLINE); }

        wprintw(win, "]");

        if (selected_measure && (state<BPM_SELECTED || mode==NORMAL_MODE)) { wattroff(win, COLOR_PAIR(2)); }
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


void update_display(struct Metronome *m, WINDOW *win, const ProgramMode mode) {
    wclear(win);

    box(win, 0, 0);

    int x, y;
    getmaxyx(win, y, x);

    if (m->practice_active) {
        print_practice_info(m, win);
    }

    {
        tui_print(m, win, mode, NONE_SELECTED);

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
                { // From bpm
                    char bpm_step_str[4];
                    uint16_t bpm_step = 0;

                    while(bpm_step<1 || bpm_step>254) {
                        move(LINES-1, 0);
                        clrtoeol();
                        printw(":from bpm = ");
                        refresh();

                        wgetnstr(stdscr, bpm_step_str, sizeof(bpm_step_str)-1);
                        bpm_step = atoi(bpm_step_str);

                        if(bpm_step<1 || bpm_step>254) {
                            move(LINES-1, 0);
                            clrtoeol();
                            printw("[ERROR] bpm must be between 1-254!");
                            refresh();
                            sleep(1);
                        }
                    }
                    metronome_practice_set_from_bpm(&p, bpm_step);
                }
                { // To bpm
                    char bpm_step_str[4];
                    uint16_t bpm_step = 0;

                    while(bpm_step<1 || bpm_step<=p.bpm_from || bpm_step>255) {
                        move(LINES-1, 0);
                        clrtoeol();
                        printw(":to bpm = ");
                        refresh();

                        wgetnstr(stdscr, bpm_step_str, sizeof(bpm_step_str)-1);
                        bpm_step = atoi(bpm_step_str);

                        if(bpm_step<1 || bpm_step <= p.bpm_from || bpm_step>255) {
                            move(LINES-1, 0);
                            clrtoeol();
                            printw("[ERROR] bpm target must be greater than from (%d) and between 1-255!", p.bpm_from);
                            refresh();
                            sleep(1);
                        }
                    }
                    p.bpm_to = bpm_step;
                }
                { // bpm step size
                    char bpm_step_str[4];
                    uint8_t bpm_step = 0;

                    while(bpm_step<1 || bpm_step>10) {
                        move(LINES-1, 0);
                        clrtoeol();
                        printw(":bpm step = ");
                        refresh();
                    
                        wgetnstr(stdscr, bpm_step_str, sizeof(bpm_step_str)-1);
                        bpm_step = atoi(bpm_step_str);

                        if(bpm_step<1 || bpm_step>10) {
                            move(LINES-1, 0);
                            clrtoeol();
                            printw("[ERROR] bpm step must be between 1-10!");
                            refresh();
                            sleep(1);
                        }
                    }
                    p.bpm_step = bpm_step;
                }
                {
                    char interval_str[4];
                    uint8_t interval = 0;

                    while(interval<1 || interval>100) {
                        move(LINES-1, 0);
                        clrtoeol();
                        printw(":interval = ");
                        refresh();

                        wgetnstr(stdscr, interval_str, sizeof(interval_str)-1);
                        interval = atoi(interval_str);

                        if(interval<1 || interval>100) {
                            move(LINES-1, 0);
                            clrtoeol();
                            printw("[ERROR] interval must be between 1-100!");
                            refresh();
                            sleep(1);
                        }
                    }
                    p.interval = interval;
                }
                m->reset = 0x1;
                m->tick = 1;
                m->practice_current = m->practice_count;
                m->bpm = p.bpm_from;

                memcpy(&m->practice[m->practice_count++], &p, sizeof(p));

                m->track.active_measure = 0;
                m->practice_active = 0x1;
            }
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

    ProgramMode program_mode = NORMAL_MODE;
    SelectionState input_selection = NONE_SELECTED;

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
            update_display(&metronome, win, program_mode);
            wrefresh(win);
        }

        struct Coord input_pos = {.x=1, .y=getmaxy(stdscr)};

        uint8_t keep_running = 0x1;
        while (keep_running == 0x1) {
            if(program_mode == NORMAL_MODE || program_mode == PAUSE_MODE) {
                char cmd = wgetch(win);

                switch(cmd) {
                    case 'j': {
                        if(program_mode == PAUSE_MODE) {
                            if(input_selection == BEAT_SELECTED) {
                            metronome_dec_beats(&metronome);
                            } else if(input_selection == UNIT_SELECTED) {
                                metronome_dec_unit(&metronome);
                            } else if(input_selection == BPM_SELECTED) {
                                metronome.bpm--;
                            }
                        }
                        //metronome.bpm -= (cmd=='j') ? 1 : 5;
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    } 
                    case 'k': {
                        if(program_mode == PAUSE_MODE) {
                            if(input_selection == BEAT_SELECTED) {
                                metronome_inc_beats(&metronome);
                            } else if(input_selection == UNIT_SELECTED) {
                                metronome_inc_unit(&metronome);
                            } else if(input_selection == BPM_SELECTED) {
                                metronome.bpm++;
                            }
                        }
                        //metronome.bpm += (cmd=='k') ? 1 : 5;
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    } 
                    case 'h': {
                        if(program_mode == PAUSE_MODE) {
                            if(input_selection==BEAT_SELECTED) {
                                input_selection=UNIT_SELECTED;
                                metronome.track.active_measure = metronome.track.active_measure > 0 
                                    ? metronome.track.active_measure-1 
                                    : metronome.track.measure_count
                                ;
                            } else if(input_selection==UNIT_SELECTED) {
                                input_selection=BEAT_SELECTED;
                            }
                            tui_print(&metronome, win, program_mode, input_selection);
                        }
                        break;
                    }
                    case 'l': {
                        if(program_mode == PAUSE_MODE) {
                            if(input_selection==BEAT_SELECTED) {
                                input_selection=UNIT_SELECTED;
                            } else if(input_selection==UNIT_SELECTED) {
                                input_selection=BEAT_SELECTED;
                                metronome.track.active_measure = 
                                    metronome.track.active_measure < metronome.track.measure_count
                                    ? metronome.track.active_measure+1
                                    : 0
                                ;
                            }

                            tui_print(&metronome, win, program_mode, input_selection);
                        }
                        break;
                    }
                    case 'n': {
                        if(program_mode == PAUSE_MODE) {
                            if(input_selection<BPM_SELECTED) {
                                metronome.track.active_measure = 
                                    (metronome.track.active_measure < metronome.track.measure_count)
                                    ? metronome.track.active_measure+1
                                    : 0
                                ;
                            }
                        }
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case 'p': {
                        if(program_mode == PAUSE_MODE) {
                            if(input_selection<BPM_SELECTED) {
                                metronome.track.active_measure = 
                                    (metronome.track.active_measure > 0)
                                    ? metronome.track.active_measure-1
                                    : metronome.track.measure_count
                                ;
                            }
                            tui_print(&metronome, win, program_mode, input_selection);  
                        }
                        break;
                    }
                    case 'L': {
                        metronome_dec_unit(&metronome);
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case 'H': {
                        metronome_inc_unit(&metronome);
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case 'a': {
                        metronome_insert_measure_after(&metronome);
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case 'A': {
                        metronome_insert_measure_at_end(&metronome);
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case 'i': {
                        metronome_insert_measure_before(&metronome);
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case 'I': {
                        metronome_insert_measure_at_start(&metronome);
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case 'd': {
                        metronome_remove_measure(&metronome);
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case '\t': {
                        input_selection = 
                            (input_selection >= BPM_SELECTED) 
                            ? BEAT_SELECTED : BPM_SELECTED
                        ;
                        tui_print(&metronome, win, program_mode, input_selection);
                        break;
                    }
                    case ':': {
                        program_mode = COMMAND_MODE;
                        move(input_pos.y, input_pos.x);
                        printw(":");
                        refresh();
                        char buffer[32];
                        getnstr(buffer, 16);
                        update_display(&metronome, win, program_mode);
                        break;
                    }
                    case ' ': {
                        if(program_mode == NORMAL_MODE) {
                            program_mode = PAUSE_MODE;
                            input_selection = BEAT_SELECTED;
                            ma_device_stop(&metronome.device);
                            tui_print(&metronome, win, program_mode, input_selection);
                        } else if(program_mode == PAUSE_MODE) {
                            program_mode = NORMAL_MODE;
                            metronome.reset = 1;
                            metronome.tick = 1;
                            metronome.track.active_measure = 0;
                            ma_device_start(&metronome.device);
                        }
                        break;
                    }
                }
            } 

            if(program_mode == COMMAND_MODE) {
                if(handle_command_mode(&metronome) == 1) {
                    keep_running = 0x0;
                }
                program_mode = NORMAL_MODE;
                update_display(&metronome, win, program_mode);
                ma_device_start(&metronome.device);
            }

            if(metronome.tick > 0) {
                update_display(&metronome, win, program_mode);
                metronome.tick = 0;
            } 
            wrefresh(win);
        }
    }
    endwin();
        metronome_shutdown(&metronome);

        return 0;
    }
