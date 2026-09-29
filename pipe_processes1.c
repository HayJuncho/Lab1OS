#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int safe_read(int fd, char *buf, size_t size) {
    ssize_t n = read(fd, buf, size);
    if (n <= 0) {
        perror("read");
        exit(1);
    }
    return (int)n;
}

void safe_write(int fd, const char *buf) {
    if (write(fd, buf, strlen(buf) + 1) == -1) {
        perror("write");
        exit(1);
    }
}

int main() {
    int fd1[2];   // P1 -> P2
    int fd2[2];   // P2 -> P1

    char fixed1[] = "howard.edu";
    char fixed2[] = "gobison.org";

    char input1[100];
    char input2[100];

    if (pipe(fd1) == -1 || pipe(fd2) == -1) {
        perror("pipe");
        exit(1);
    }

    printf("Other string is: howard.edu\n");
    printf("Input : ");
    fflush(stdout);

    if (!fgets(input1, sizeof(input1), stdin)) {
        fprintf(stderr, "Input error\n");
        exit(1);
    }
    input1[strcspn(input1, "\n")] = '\0';

    pid_t p = fork();
    if (p < 0) {
        perror("fork");
        exit(1);
    }

    /* ============================
       PARENT PROCESS (P1)
       ============================ */
    if (p > 0) {

        close(fd1[0]); // close read end of pipe1
        close(fd2[1]); // close write end of pipe2

        safe_write(fd1[1], input1);
        close(fd1[1]);

        wait(NULL);

        char returned_str[300];
        safe_read(fd2[0], returned_str, sizeof(returned_str));
        close(fd2[0]);

        if (strlen(returned_str) + strlen(fixed2) + 1 >= sizeof(returned_str)) {
            fprintf(stderr, "String too long\n");
            exit(1);
        }

        strcat(returned_str, fixed2);

        printf("Output (P1): %s\n", returned_str);
    }

    /* ============================
       CHILD PROCESS (P2)
       ============================ */
    else {

        close(fd1[1]); // close write end of pipe1
        close(fd2[0]); // close read end of pipe2

        char concat_str[300];
        safe_read(fd1[0], concat_str, sizeof(concat_str));
        close(fd1[0]);

        if (strlen(concat_str) + strlen(fixed1) + 1 >= sizeof(concat_str)) {
            fprintf(stderr, "String too long\n");
            exit(1);
        }

        strcat(concat_str, fixed1);
        printf("Output (P2): %s\n", concat_str);

        printf("Input : ");
        fflush(stdout);

        if (!fgets(input2, sizeof(input2), stdin)) {
            fprintf(stderr, "Input error\n");
            exit(1);
        }
        input2[strcspn(input2, "\n")] = '\0';

        if (strlen(concat_str) + strlen(input2) + 1 >= sizeof(concat_str)) {
            fprintf(stderr, "String too long\n");
            exit(1);
        }

        strcat(concat_str, input2);

        safe_write(fd2[1], concat_str);
        close(fd2[1]);

        exit(0);
    }

    return 0;
}
