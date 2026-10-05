#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>

#define ITERATIONS 25

void ParentProcess(volatile int *shm);
void ClientProcess(volatile int *shm);

/* shm[0] = BankAccount, shm[1] = Turn */

int main(void)
{
    int shmID;
    volatile int *ShmPTR;
    pid_t pid;
    int status;

    /* Shared memory for two integers: BankAccount and Turn */
    shmID = shmget(IPC_PRIVATE, 2 * sizeof(int), IPC_CREAT | 0666);
    if (shmID < 0) {
        perror("*** shmget error (server) ***");
        exit(1);
    }

    ShmPTR = (volatile int *) shmat(shmID, NULL, 0);
    if ((void *) ShmPTR == (void *) -1) {
        perror("*** shmat error (server) ***");
        exit(1);
    }

    ShmPTR[0] = 0;   /* BankAccount */
    ShmPTR[1] = 0;   /* Turn */

    pid = fork();
    if (pid < 0) {
        perror("*** fork error (server) ***");
        exit(1);
    } else if (pid == 0) {
        ClientProcess(ShmPTR);
        shmdt((void *) ShmPTR);
        exit(0);
    }

    ParentProcess(ShmPTR);

    wait(&status);
    shmdt((void *) ShmPTR);
    shmctl(shmID, IPC_RMID, NULL);
    return 0;
}

/* Dear Old Dad */
void ParentProcess(volatile int *shm)
{
    int i, account, balance;

    srand((unsigned) time(NULL) ^ (unsigned) getpid());

    for (i = 0; i < ITERATIONS; i++) {
        sleep(rand() % 6);                  /* 0 - 5 seconds */
        account = shm[0];

        while (shm[1] != 0)
            ;                               /* no-op: strict alternation */

        if (account <= 100) {
            balance = rand() % 101;         /* 0 - 100 */
            if (balance % 2 == 0) {
                account += balance;
                printf("Dear old Dad: Deposits $%d / Balance = $%d\n",
                       balance, account);
            } else {
                printf("Dear old Dad: Doesn't have any money to give\n");
            }
            shm[0] = account;
        } else {
            printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n",
                   account);
        }
        fflush(stdout);
        shm[1] = 1;
    }
}

/* Poor Student */
void ClientProcess(volatile int *shm)
{
    int i, account, balance;

    srand((unsigned) time(NULL) ^ ((unsigned) getpid() << 8));

    for (i = 0; i < ITERATIONS; i++) {
        sleep(rand() % 6);                  /* 0 - 5 seconds */
        account = shm[0];

        while (shm[1] != 1)
            ;                               /* no-op: strict alternation */

        balance = rand() % 51;              /* 0 - 50 */
        printf("Poor Student needs $%d\n", balance);

        if (balance <= account) {
            account -= balance;
            printf("Poor Student: Withdraws $%d / Balance = $%d\n",
                   balance, account);
        } else {
            printf("Poor Student: Not Enough Cash ($%d)\n", account);
        }
        fflush(stdout);

        shm[0] = account;
        shm[1] = 0;
    }
}
