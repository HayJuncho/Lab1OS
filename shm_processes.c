/*
 * Poor Student / Dear Old Dad (+ Lovable Mom extra credit)
 *
 * Usage:  ./psdd                 -> 1 parent (Dad), 1 student
 *         ./psdd <parents> <N>   -> parents = 1 (Dad) or 2 (Dad + Mom),
 *                                   N = number of Poor Students
 *
 * BankAccount is a shared int (System V shared memory). A named POSIX
 * semaphore (mutex) guarantees that only one process at a time does the
 * read-modify-write on BankAccount, so no race conditions occur.
 *
 * Stop the program by sending a signal (e.g. kill -INT <pid>) to the
 * parent process; it will kill its children and clean up.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define SEM_NAME "/psdd_bank_mutex"

static int    ShmID = -1;
static int   *BankAccount = NULL;
static sem_t *mutex = SEM_FAILED;
static pid_t *children = NULL;
static int    numChildren = 0;

static void DearOldDad(void);
static void LovableMom(void);
static void PoorStudent(int id, int numStudents);

/* Parent's signal handler: kill all children and release IPC resources. */
static void cleanup(int sig)
{
     (void) sig;
     for (int i = 0; i < numChildren; i++)
          if (children[i] > 0)
               kill(children[i], SIGTERM);
     while (wait(NULL) > 0)
          ;
     if (mutex != SEM_FAILED) {
          sem_close(mutex);
          sem_unlink(SEM_NAME);
     }
     if (BankAccount != NULL)
          shmdt((void *) BankAccount);
     if (ShmID >= 0)
          shmctl(ShmID, IPC_RMID, NULL);
     printf("\nParent: cleaned up shared memory and semaphore. Exiting.\n");
     exit(0);
}

int main(int argc, char *argv[])
{
     int numParents = 1, numStudents = 1;

     if (argc == 3) {
          numParents  = atoi(argv[1]);
          numStudents = atoi(argv[2]);
     } else if (argc != 1) {
          printf("Use: %s [#parents(1|2) #students]\n", argv[0]);
          exit(1);
     }
     if (numParents < 1 || numParents > 2 || numStudents < 1) {
          printf("#parents must be 1 or 2 and #students must be >= 1\n");
          exit(1);
     }

     /* Unbuffered stdout so output from all processes interleaves cleanly
        (also matters when output is piped to a file or autograder). */
     setvbuf(stdout, NULL, _IONBF, 0);

     /* Shared BankAccount */
     ShmID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
     if (ShmID < 0) {
          perror("shmget");
          exit(1);
     }
     BankAccount = (int *) shmat(ShmID, NULL, 0);
     if (BankAccount == (int *) -1) {
          perror("shmat");
          exit(1);
     }
     *BankAccount = 0;

     /* Mutex semaphore protecting BankAccount (remove any stale one first) */
     sem_unlink(SEM_NAME);
     mutex = sem_open(SEM_NAME, O_CREAT, 0644, 1);
     if (mutex == SEM_FAILED) {
          perror("sem_open");
          exit(1);
     }

     children = calloc(numStudents + 1, sizeof(pid_t));
     signal(SIGINT, cleanup);
     signal(SIGTERM, cleanup);

     /* Optional Lovable Mom process */
     if (numParents == 2) {
          pid_t pid = fork();
          if (pid < 0) { perror("fork"); cleanup(0); }
          if (pid == 0) {
               signal(SIGINT, SIG_DFL);
               signal(SIGTERM, SIG_DFL);
               LovableMom();
               exit(0);
          }
          children[numChildren++] = pid;
     }

     /* Poor Student processes */
     for (int i = 0; i < numStudents; i++) {
          pid_t pid = fork();
          if (pid < 0) { perror("fork"); cleanup(0); }
          if (pid == 0) {
               signal(SIGINT, SIG_DFL);
               signal(SIGTERM, SIG_DFL);
               PoorStudent(i + 1, numStudents);
               exit(0);
          }
          children[numChildren++] = pid;
     }

     /* The original process is Dear Old Dad */
     DearOldDad();
     return 0;
}

static void DearOldDad(void)
{
     int localBalance = 0;
     srand(time(NULL) ^ (getpid() << 16));

     while (1) {
          sleep(rand() % 6);                         /* 0 - 5 seconds */
          printf("Dear Old Dad: Attempting to Check Balance\n");

          sem_wait(mutex);                           /* enter critical section */
          if (rand() % 2 == 0) {
               if (localBalance < 100) {
                    /* Deposit Money */
                    localBalance = *BankAccount;
                    int amount = rand() % 101;       /* 0 - 100 */
                    if (amount % 2 == 0) {
                         localBalance += amount;
                         printf("Dear old Dad: Deposits $%d / Balance = $%d\n",
                                amount, localBalance);
                         *BankAccount = localBalance;
                    } else {
                         printf("Dear old Dad: Doesn't have any money to give\n");
                    }
               } else {
                    printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n",
                           localBalance);
               }
          } else {
               printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
          }
          sem_post(mutex);                           /* leave critical section */
     }
}

static void LovableMom(void)
{
     int localBalance = 0;
     srand(time(NULL) ^ (getpid() << 16));

     while (1) {
          sleep(rand() % 11);                        /* 0 - 10 seconds */
          printf("Loveable Mom: Attempting to Check Balance\n");

          sem_wait(mutex);
          if (localBalance <= 100) {
               /* Deposit Money (always) */
               localBalance = *BankAccount;
               int amount = rand() % 126;            /* 0 - 125 */
               localBalance += amount;
               printf("Lovable Mom: Deposits $%d / Balance = $%d\n",
                      amount, localBalance);
               *BankAccount = localBalance;
          }
          sem_post(mutex);
     }
}

static void PoorStudent(int id, int numStudents)
{
     int localBalance = 0;
     char name[32];

     /* Keep the exact original message text when there is only one student */
     if (numStudents > 1)
          snprintf(name, sizeof(name), "Poor Student(%d)", id);
     else
          snprintf(name, sizeof(name), "Poor Student");

     srand(time(NULL) ^ (getpid() << 16));

     while (1) {
          sleep(rand() % 6);                         /* 0 - 5 seconds */
          printf("%s: Attempting to Check Balance\n", name);

          sem_wait(mutex);
          if (rand() % 2 == 0) {
               /* Withdraw Money */
               localBalance = *BankAccount;
               int need = rand() % 51;               /* 0 - 50 */
               printf("%s needs $%d\n", name, need);
               if (need <= localBalance) {
                    localBalance -= need;
                    printf("%s: Withdraws $%d / Balance = $%d\n",
                           name, need, localBalance);
               } else {
                    printf("%s: Not Enough Cash ($%d)\n", name, localBalance);
               }
               *BankAccount = localBalance;
          } else {
               printf("%s: Last Checking Balance = $%d\n", name, localBalance);
          }
          sem_post(mutex);
     }
}
