#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/* Global flag set by the handler so main() knows the signal arrived */
volatile sig_atomic_t got_signal = 0;

void handler(int sig) {
    printf("Hello World!\n");
    fflush(stdout);
    got_signal = 1;
}

int main(void) {
    signal(SIGALRM, handler);  /* register handler for SIGALRM */
    alarm(5);                  /* deliver SIGALRM in 5 seconds */

    while (!got_signal) {
        /* busy wait for the signal to arrive */
    }

    printf("Turing was right!\n");
    fflush(stdout);

    return 0;
}
