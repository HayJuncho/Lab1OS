#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

/*
 * main3.c
 *
 * Creates two child processes. Each child loops for a random number
 * of iterations (no more than 30), sleeping for a random amount of
 * time (no more than 10 seconds) on each iteration. The parent waits
 * for both children to terminate and reports as each one finishes.
 */

void run_child(void) {
    pid_t pid = getpid();
    pid_t ppid = getppid();

    int iterations = (random() % 30) + 1; /* 1 - 30 iterations */

    for (int i = 0; i < iterations; i++) {
        printf("Child Pid: %d is going to sleep!\n", pid);
        fflush(stdout);

        unsigned int sleep_time = (random() % 10) + 1; /* 1 - 10 secs */
        sleep(sleep_time);

        printf("Child Pid: %d is awake!\nWhere is my Parent: %d?\n", pid, ppid);
        fflush(stdout);
    }

    exit(0);
}

int main(void) {
    /* Seed the random number generator once, in the parent, before forking */
    srandom(time(NULL) ^ getpid());

    pid_t child1, child2;

    child1 = fork();
    if (child1 < 0) {
        perror("fork failed for child1");
        exit(1);
    } else if (child1 == 0) {
        /* Child 1 process */
        run_child();
    }

    child2 = fork();
    if (child2 < 0) {
        perror("fork failed for child2");
        exit(1);
    } else if (child2 == 0) {
        /* Child 2 process */
        run_child();
    }

    /* Parent process: wait for both children to terminate */
    int status;
    pid_t finished_pid;

    for (int i = 0; i < 2; i++) {
        finished_pid = wait(&status);
        if (finished_pid > 0) {
            printf("Child Pid: %d has completed\n", finished_pid);
            fflush(stdout);
        }
    }

    return 0;
}
