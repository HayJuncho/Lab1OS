#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

/* Global state shared with the handlers */
volatile sig_atomic_t got_signal = 0;
volatile sig_atomic_t alarm_count = 0;
time_t start_time;

void alarm_handler(int sig) {
    printf("Hello World!\n");
    fflush(stdout);
    alarm_count++;
    got_signal = 1;
    alarm(1);   /* re-arm for the next second */
}

void sigint_handler(int sig) {
    time_t end_time = time(NULL);
    double elapsed = difftime(end_time, start_time);

    printf("\nTotal alarms delivered: %d\n", alarm_count);
    printf("Total time executing: %.0f seconds\n", elapsed);
    fflush(stdout);

    exit(0);
}

int main(void) {
    start_time = time(NULL);

    signal(SIGALRM, alarm_handler);  /* register handler for SIGALRM */
    signal(SIGINT, sigint_handler);  /* register handler for CTRL-C */
    alarm(1);                        /* first SIGALRM in 1 second */

    while (1) {
        while (!got_signal) {
            /* busy wait for the signal to arrive */
        }
        printf("Turing was right!\n");
        fflush(stdout);
        got_signal = 0;
    }

    return 0;
}
