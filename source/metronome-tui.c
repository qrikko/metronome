#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "metronome.h"

#define COMMAND_MAX_LEN 256

struct Coord { int x, y; };
//static struct Coord tick_pos;

typedef enum {NORMAL_MODE, COMMAND_MODE, PAUSE_MODE} program_state_t;

void init_tui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(0);
}

void update_display(WINDOW *win) {
    struct Metronome *const metronome = metronome_state();
    wclear(win);

    box(win, 0, 0);
    int x, y;
    getmaxyx(win, y, x);
    
    {
        int len = snprintf(
            NULL, 0, 
            "Increase BPM by %.0f in %d measures", 
            metronome->bpm_step, metronome->next_step
        );

        if (metronome->interval > 0) {
            mvwprintw(
                win,
                y/2 -1, 
                (x-len)/2, 
                "Increase BPM by %.0f in %d measures", 
                metronome->bpm_step, metronome->next_step
            );
        }
    }
    {
        int len = snprintf(
            NULL, 0, 
            "Metronome at %.0f BPM [%d/%d]", 
            metronome->bpm, metronome->nominator, metronome->denominator
        );
        mvwprintw(
            win,
            y/2, 
            (x-len)/2, 
            "Metronome at %.0f BPM [%d/%d]", 
            metronome->bpm, metronome->nominator, metronome->denominator
        );

        int step = len/(metronome->nominator-1);

        for(int i=0; i<metronome->nominator; ++i) {
            mvwprintw(
                win,
                y/2 +1, 
                (x-len)/2 + step*i, 
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
    }

    mvwprintw(stdscr, LINES -2, 0, "-- NORMAL --");
    refresh();
    wrefresh(win);
}

int handle_command_mode() {
    struct Metronome *const metronome = metronome_state();
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
        } else if (strcmp(token, "signature") == 0 || strcmp(token, "ts") == 0) {
            char *nominator = strtok(NULL, " ");
            char *denominator = strtok(NULL, " ");
            if (nominator && denominator) {
                metronome->nominator = atoi(nominator);
                metronome->denominator = atoi(denominator);
            } else {
                mvprintw(LINES-1, 0, ":nominator = ");
                refresh();
                {
                    char nominator[3];
                    wgetnstr(stdscr, nominator, sizeof(nominator)-1);
                    metronome->nominator = atoi(nominator);
                }
                {
                    move(LINES-1, 0);
                    clrtoeol();
                    mvprintw(LINES-1, 0, ":denominator = ");
                    refresh();
                    char denominator[3];
                    wgetnstr(stdscr, denominator, sizeof(denominator)-1);
                    metronome->denominator = atoi(denominator);
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
        } else if(strcmp(token, "reset") == 0) {
            metronome->bpm = metronome->base_bpm;
            metronome->next_step = metronome->interval;
            metronome->reset = 0x1;
            metronome->tick = 1;
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
    struct Metronome *const metronome = metronome_state();
    metronome->bpm=120.0; 
    metronome->base_bpm=120.0; 
    metronome->bpm_step=0;
    metronome->interval=0;
    metronome->next_step = 0;

    //tick_pos.x = 10;
    //tick_pos.y = 2;

    program_state_t program_state = NORMAL_MODE;

    metronome_setup();
    //metronome->denominator = 8;
    //metronome->nominator = 7;
    init_tui();
    {
        int ymax, xmax;
        getmaxyx(stdscr, ymax, xmax);
        WINDOW *win = newwin(ymax/2, xmax/2, ymax/4, xmax/4);
        wtimeout(win, 0);
        update_display(win);

        struct Coord bpm_pos = {};
        getyx(win, bpm_pos.y, bpm_pos.x);

        struct Coord input_pos = {.x=1, .y=getmaxy(stdscr)};

        uint8_t keep_running = 0x1;
        while (keep_running == 0x1) {
            if(program_state == NORMAL_MODE) {
                char cmd = wgetch(win);
            //    char cmd = getch();

                switch(cmd) {
                    case 'j':
                    case 'J': {
                        metronome->bpm -= (cmd=='j') ? 1 : 5;
                        int x, y;
                        getmaxyx(win, y, x);
                        int len = snprintf(
                            NULL, 0, 
                            "Metronome at %.0f BPM [%d/%d]", 
                            metronome->bpm, metronome->nominator, metronome->denominator
                        );
                        mvwprintw(
                            win,
                            y/2, 
                            (x-len)/2, 
                            "Metronome at %.0f BPM [%d/%d]", 
                            metronome->bpm, metronome->nominator, metronome->denominator
                        );
                        wrefresh(win);
                    } break;
                    case 'k':
                    case 'K': {
                        metronome->bpm += (cmd=='k') ? 1 : 5;
                        int x, y;
                        getmaxyx(win, y, x);
                        int len = snprintf(
                            NULL, 0, 
                            "Metronome at %.0f BPM [%d/%d]", 
                            metronome->bpm, metronome->nominator, metronome->denominator
                        );
                        mvwprintw(
                            win,
                            y/2, 
                            (x-len)/2, 
                            "Metronome at %.0f BPM [%d/%d]", 
                            metronome->bpm, metronome->nominator, metronome->denominator
                        );
                        wrefresh(win);
                    } break;
                    case 'l':
                        metronome_inc_beats_per_measure();
                        break;
                    case 'h':
                        metronome_dec_beats_per_measure();
                        break;
                    case 'L':
                        metronome_dec_note_length();
                        break;
                    case 'H':
                        metronome_inc_note_length();
                        break;
                    case ' ':
                        program_state = PAUSE_MODE;
                        break;
                    case ':':
                        program_state = COMMAND_MODE;
                        move(input_pos.y, input_pos.x);
                        printw(":");
                        refresh();
                        char buffer[32];
                        getnstr(buffer, 16);
                        break;
                }
            } 

            if (program_state == PAUSE_MODE) {
                ma_device_stop(&metronome->device);
                timeout(20000);
                mvprintw(LINES -2, 0, "-- PAUSE --");
                while(getch() != ' ') {}
                program_state = NORMAL_MODE;
                timeout(0);
                //update_display(win);
                ma_device_start(&metronome->device);
            }

            if(program_state == COMMAND_MODE) {
                if(handle_command_mode() == 1) {
                    keep_running = 0x0;
                }
                program_state = NORMAL_MODE;
                update_display(win);
                ma_device_start(&metronome->device);
            }

            if(metronome->tick > 0) {
                update_display(win);
                metronome->tick = 0;
            } 
            wrefresh(win);
            //usleep(1000);
        }
    }
    endwin();
    metronome_shutdown();

    return 0;
}
