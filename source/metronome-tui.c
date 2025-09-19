#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "metronome.h"

#define COMMAND_MAX_LEN 256

struct Coord { int x, y; };
//static struct Coord tick_pos;

typedef enum {NORMAL_MODE, COMMAND_MODE, PAUSE_MODE} program_state_t;
const char* mode_string(const program_state_t mode) {
    switch(mode) {
        case NORMAL_MODE:   return "-- NORMAL --";
        case PAUSE_MODE:    return "-- PAUSE --";
        case COMMAND_MODE:  return "-- COMMAND --";
        default:            return "-- UNKNOWN --";
    }
}


void init_tui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(0);
}

void tui_print(const struct Metronome *metronome, WINDOW *win) {
    int x, y;
    getmaxyx(win, y, x);
    int len = snprintf(NULL, 0, "Metronome at %d BPM", metronome->bpm);
    int left = (x-len)/2;

    wmove(win, 5, 0);
    wclrtoeol(win);
    mvwprintw(win, 3, left, "Metronome at %d BPM", metronome->bpm);
    wclrtoeol(win);
    for(uint8_t i=0; i<=metronome->track.size; ++i) {
        uint8_t b = metronome->track.measures[i].beats;
        uint8_t u = metronome->track.measures[i].unit;
        int offset = snprintf(NULL, 0, "[%d/%d]", b, u);

        wmove(win, 4, left);
        int selected = (metronome->track.selected == i);
        if (selected) {
            wattron(win, COLOR_PAIR(2));
            //wattron(win, A_UNDERLINE);
        }
        wprintw(
            win, 
            "[%d/%d]",
            metronome->track.measures[i].beats, 
            metronome->track.measures[i].unit
        );
        if (selected) {
            wattroff(win, COLOR_PAIR(2));
        }
        left+=offset;
    }
    box(win, 0, 0);
    wrefresh(win);
}
void update_display(struct Metronome *metronome, WINDOW *win, const char *mode) {
    wclear(win);

    box(win, 0, 0);
    
    int x, y;
    getmaxyx(win, y, x);
    
    {
        int len = snprintf(
            NULL, 0, 
            "Increase BPM by %d in %d measures", 
            metronome->bpm_step, metronome->next_step
        );

        if (metronome->interval > 0) {
            mvwprintw(
                win,
                2,
                (x-len)/2, 
                "Increase BPM by %d in %d measures", 
                metronome->bpm_step, metronome->next_step
            );
        }
    }
    {
        tui_print(metronome, win);

        const int margin = 5;
        uint8_t len = getmaxx(win) -2*margin;
        int step = len/(metronome->track.measures[metronome->track.selected].beats-1);

        for(int i=0; i<metronome->track.measures[metronome->track.selected].beats; ++i) {
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
            (x-len)/2 + step*(metronome->tick!=0 ? metronome->tick-1 : metronome->tick), 
            "^" 
        );
        if (metronome->tick == 1) {
            wbkgd(win, COLOR_PAIR(1));
        } else {
            wbkgd(win, 0);
            //wbkgd(win, COLOR_PAIR(2));
        }
        //wrefresh(win);
    }

    mvwprintw(stdscr, LINES -2, 0, mode);//"-- NORMAL --");
    refresh();
    wrefresh(win);
}

int handle_command_mode(struct Metronome *metronome) {
    char cmd[COMMAND_MAX_LEN];
    int ch;
    int result = 0;

    ma_device_stop(&metronome->device);
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
                metronome->bpm = (double)atoi(value_str);
                metronome->base_bpm = metronome->bpm;
                metronome->tick = 1;
                metronome->next_step = metronome->interval;
                metronome->reset = 0x1;
            }
        } else if (strcmp(token, "beats") == 0) {
            char *value = strtok(NULL, " ");
            if (value) {
                metronome_set_beats(metronome, atoi(value));
            } else {
                mvprintw(LINES-1, 0, ":beats = ");
                refresh();
                {
                    char beats[3];
                    wgetnstr(stdscr, beats, sizeof(beats)-1);
                    metronome_set_beats(metronome, atoi(beats));
                }
            }
        } else if (strcmp(token, "unit") == 0) {
            char *value = strtok(NULL, " ");
            if (value) {
                metronome_set_unit(metronome, atoi(value));
            } else {
                mvprintw(LINES-1, 0, ":unit = ");
                refresh();
                {
                    char unit[3];
                    wgetnstr(stdscr, unit, sizeof(unit)-1);
                    metronome_set_unit(metronome, atoi(unit));
                }
            }
        }

        else if (strcmp(token, "signature") == 0 || strcmp(token, "ts") == 0) {
            char *beats = strtok(NULL, " ");
            char *unit = strtok(NULL, " ");
            if (beats && unit) {
                metronome_set_beats(metronome, atoi(beats));
                metronome_set_unit(metronome, atoi(unit));
            } else {
                mvprintw(LINES-1, 0, ":beats = ");
                refresh();
                {
                    char beats[3];
                    wgetnstr(stdscr, beats, sizeof(beats)-1);
                    metronome_set_beats(metronome, atoi(beats));
                }
                {
                    move(LINES-1, 0);
                    clrtoeol();
                    mvprintw(LINES-1, 0, ":unit = ");
                    refresh();
                    char unit[3];
                    wgetnstr(stdscr, unit, sizeof(unit)-1);
                    metronome_set_unit(metronome, atoi(unit));
                }
            }
            metronome->next_step = metronome->interval;
            metronome->reset = 0x1;
            metronome->tick = 1;
        } else if(strcmp(token, "practice") == 0) {
            char *value_str = strtok(NULL, " ");
            if(value_str) {
                if(strcmp(value_str, "off") == 0) {
                    metronome->reset = 0x1;
                    metronome->interval = 0;
                    metronome->next_step = 0;
                    metronome->tick = 1;
                }
            } else {
                mvprintw(LINES-1, 0, ":bpm step = ");
                refresh();
                {
                    char bpm_step[3];
                    wgetnstr(stdscr, bpm_step, sizeof(bpm_step)-1);
                    metronome->bpm_step = (float)atoi(bpm_step);
                }
                {
                    move(LINES-1, 0);
                    clrtoeol();
                    mvprintw(LINES-1, 0, ":interval = ");
                    refresh();
                    char interval[3];
                    wgetnstr(stdscr, interval, sizeof(interval)-1);
                    metronome->interval = atoi(interval);
                }
                metronome->reset = 0x1;
                metronome->next_step = metronome->interval;
                metronome->tick = 1;
            }
        } else if(strcmp(token, "o") == 0) {
            metronome_add_track(metronome);

        } else if(strcmp(token, "reset") == 0) {
            metronome->bpm = metronome->base_bpm;
            metronome->next_step = metronome->interval;
            metronome->reset = 0x1;
            metronome->tick = 1;
        } else if(strcmp(token, "w") == 0) {
            const char *home = getenv("HOME");
            const char *rel = "/.local/share/metronome.state";
            char path[128];
            sprintf(path, "%s/%s", home, rel);

            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(metronome, sizeof(uint8_t), 6, f);
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

    program_state_t program_state = NORMAL_MODE;

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
            update_display(&metronome, win, mode_string(program_state));
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
                            metronome.track.selected = metronome.track.selected < metronome.track.size
                                ? metronome.track.selected+1
                                : 0;
                            tui_print(&metronome, win);
                        }
                        break;
                    }
                    case 'p': {
                        if(program_state == PAUSE_MODE) {
                            metronome.track.selected = metronome.track.selected > 0 
                                ? metronome.track.selected-1 
                                : metronome.track.size;
                            tui_print(&metronome, win);
                        }
                        break;
                    }
                    case 'j':
                    case 'J': {
                        metronome.bpm -= (cmd=='j') ? 1 : 5;
                        tui_print(&metronome, win);
                    } break;
                    case 'k':
                    case 'K': {
                        metronome.bpm += (cmd=='k') ? 1 : 5;
                        tui_print(&metronome, win);
                    } break;
                    case 'l': {
                        metronome_inc_beats(&metronome);
                        tui_print(&metronome, win);
                        break;
                    }
                    case 'h': {
                        metronome_dec_beats(&metronome);
                        tui_print(&metronome, win);
                        break;
                    }
                    case 'L': {
                        metronome_dec_unit(&metronome);
                        tui_print(&metronome, win);
                        break;
                    }
                    case 'H': {
                        metronome_inc_unit(&metronome);
                        tui_print(&metronome, win);
                        break;
                    }
                    case 'o': {
                        metronome_add_track(&metronome);
                        tui_print(&metronome, win);
                        break;
                    }
                    case ':': {
                        program_state = COMMAND_MODE;
                        move(input_pos.y, input_pos.x);
                        printw(":");
                        refresh();
                        char buffer[32];
                        getnstr(buffer, 16);
                        update_display(&metronome, win, mode_string(program_state));
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
                            metronome.track.selected = 0;
                            ma_device_start(&metronome.device);
                        }
                        update_display(&metronome, win, mode_string(program_state));

                        break;
                    }
                }
            } 

            if(program_state == COMMAND_MODE) {
                if(handle_command_mode(&metronome) == 1) {
                    keep_running = 0x0;
                }
                program_state = NORMAL_MODE;
                update_display(&metronome, win, mode_string(program_state));
                ma_device_start(&metronome.device);
            }

            if(metronome.tick > 0) {
                //wbkgd(win, COLOR_PAIR(1));
                //wrefresh(win);
                update_display(&metronome, win, mode_string(program_state));
                //wbkgd(win, COLOR_PAIR(2));
                //wrefresh(win);
                metronome.tick = 0;
            } 
            wrefresh(win);
        }
    }
    endwin();
    metronome_shutdown(&metronome);

    return 0;
}
