#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/* Signal handler for SIGALRM: prints Hello World and exits */
void handler(int sig) {
    printf("Hello World!\n");
    fflush(stdout);
    exit(0);
}

int main(void) {
    signal(SIGALRM, handler);  /* register handler for SIGALRM */
    alarm(5);                  /* deliver SIGALRM in 5 seconds */

    while (1) {
        /* busy wait for the signal to arrive */
    }

    return 0;
}
