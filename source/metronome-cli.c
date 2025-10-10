#include "metronome.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

static struct termios original_termios;

void enable_non_canonical_mode() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &original_termios);
    new_termios = original_termios;

    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

void disable_non_canonical_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

int main (int argc, char **argv) {
    struct Metronome metronome;
    metronome_setup(&metronome);

    metronome.bpm=120; 
    metronome.base_bpm=120.0; 

    if(argc > 1) {
        metronome.bpm = atoi(argv[1]);
    }

    enable_non_canonical_mode();


    printf("Metronome running at %d BPM.\n", metronome.bpm);

    char input_char;
    char keep_running = 0x1;
    while(keep_running == 0x1) {
        if(read(STDIN_FILENO, &input_char, 1) == 1) {
            switch(input_char) {
                case '+':
                    metronome.bpm++;
                    break;
                case '-':
                    metronome.bpm--;
                    break;
                case ':':
                    printf(":");
                    disable_non_canonical_mode();
                    char cmd[128];
                    fscanf(stdin, "%s", cmd);
                    if(strcmp(cmd, "bpm") == 0) {
                        printf("new bpm: ");
                        char value[128];
                        fscanf(stdin, "%s", value);
                        metronome.bpm = atoi(value);
                        metronome.base_bpm = metronome.bpm;
                        //metronome.next_step = metronome.interval;
                    } else if (strcmp(cmd, "interval") == 0) {
                        {
                            printf("bpm step: ");
                            char bpm_step[3];
                            fscanf(stdin, "%s", bpm_step);
                            //metronome.bpm_step = atof(bpm_step);
                        }
                        {
                            printf("measures interval: ");
                            char interval[3];
                            fscanf(stdin, "%s", interval);
                            //metronome.interval = atoi(interval);
                        }
                        //metronome.next_step = metronome.interval;
                    }  else if (strcmp(cmd, "reset") == 0) {
                        metronome.bpm = metronome.base_bpm;
                        //metronome.next_step = metronome.interval;
                    }
                    else if(strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
                        keep_running = 0x0;
                    }
                    enable_non_canonical_mode();
                    break;
            }
            system("clear");
            printf("Metronome running at %d BPM.\n", metronome.bpm);
        }
        usleep(1000);
    }

    metronome_shutdown(&metronome);

    return 0;
}
