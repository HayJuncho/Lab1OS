#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <grep-argument>\n", argv[0]);
        exit(1);
    }

    char *grep_arg = argv[1];

    int pipe1[2];   // cat -> grep
    int pipe2[2];   // grep -> sort

    if (pipe(pipe1) == -1) {
        perror("pipe1");
        exit(1);
    }
    if (pipe(pipe2) == -1) {
        perror("pipe2");
        exit(1);
    }

    pid_t p1 = fork();

    if (p1 < 0) {
        perror("fork");
        exit(1);
    }

    /* ============================
       CHILD PROCESS P2 (grep)
       ============================ */
    if (p1 == 0) {

        pid_t p2 = fork();

        if (p2 < 0) {
            perror("fork");
            exit(1);
        }

        /* ============================
           CHILD'S CHILD PROCESS P3 (sort)
           ============================ */
        if (p2 == 0) {

            // sort reads from pipe2
            dup2(pipe2[0], STDIN_FILENO);

            close(pipe1[0]);
            close(pipe1[1]);
            close(pipe2[1]);
            close(pipe2[0]);

            execlp("sort", "sort", NULL);
            perror("execlp sort");
            exit(1);
        }

        /* ============================
           CHILD PROCESS P2 (grep)
           ============================ */

        // grep reads from pipe1
        dup2(pipe1[0], STDIN_FILENO);

        // grep writes to pipe2
        dup2(pipe2[1], STDOUT_FILENO);

        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe1[0]);
        close(pipe2[1]);

        execlp("grep", "grep", grep_arg, NULL);
        perror("execlp grep");
        exit(1);
    }

    /* ============================
       PARENT PROCESS P1 (cat)
       ============================ */

    // cat writes to pipe1
    dup2(pipe1[1], STDOUT_FILENO);

    close(pipe1[0]);
    close(pipe2[0]);
    close(pipe2[1]);
    close(pipe1[1]);

    execlp("cat", "cat", "scores", NULL);
    perror("execlp cat");
    exit(1);
}
