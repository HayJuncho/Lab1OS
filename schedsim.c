
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#include "process.h"
#include "util.h"

/*
 * Read all processes from the input file.
 * Input format:
 * pid burst_time arrival_time waiting_time turnaround_time priority
 *
 * This implementation loads the input once and dynamically allocates
 * enough space for every process.
 */
static ProcessType *loadProcesses(const char *filename, int *n)
{
    FILE *f = fopen(filename, "r");

    if (f == NULL)
    {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    int capacity = 8;
    int count = 0;

    ProcessType *plist = malloc(capacity * sizeof(ProcessType));

    if (plist == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        ProcessType p;

        int result = fscanf(f, "%d %d %d %d %d %d",
                            &p.pid,
                            &p.bt,
                            &p.art,
                            &p.wt,
                            &p.tat,
                            &p.pri);

        if (result == EOF)
            break;

        if (result != 6)
        {
            fprintf(stderr, "Error: Invalid process data in input file\n");
            free(plist);
            fclose(f);
            exit(EXIT_FAILURE);
        }

        if (p.bt < 0 || p.art < 0)
        {
            fprintf(stderr,
                    "Error: Burst time and arrival time must be nonnegative\n");
            free(plist);
            fclose(f);
            exit(EXIT_FAILURE);
        }

        if (count == capacity)
        {
            if (capacity > INT_MAX / 2 ||
                (size_t)capacity * 2 > SIZE_MAX / sizeof(ProcessType))
            {
                fprintf(stderr, "Error: Too many processes\n");
                free(plist);
                fclose(f);
                exit(EXIT_FAILURE);
            }

            capacity *= 2;

            ProcessType *temp =
                realloc(plist, (size_t)capacity * sizeof(ProcessType));

            if (temp == NULL)
            {
                fprintf(stderr, "Error: Memory allocation failed\n");
                free(plist);
                fclose(f);
                exit(EXIT_FAILURE);
            }

            plist = temp;
        }

        plist[count] = p;
        plist[count].wt = 0;
        plist[count].tat = 0;
        count++;
    }

    fclose(f);

    if (count == 0)
    {
        free(plist);
        fprintf(stderr, "Error: No processes found in input file\n");
        exit(EXIT_FAILURE);
    }

    *n = count;
    return plist;
}

/*
 * FCFS:
 * Select the earliest-arriving process that has not executed.
 * Equal arrival times preserve original input order.
 */
void findWaitingTime(ProcessType plist[], int n)
{
    if (n <= 0)
        return;

    int completed[n];
    memset(completed, 0, sizeof(completed));

    int t = 0;

    for (int k = 0; k < n; k++)
    {
        int next = -1;

        for (int i = 0; i < n; i++)
        {
            if (completed[i])
                continue;

            if (next == -1 ||
                plist[i].art < plist[next].art)
            {
                next = i;
            }
        }

        if (next == -1)
            break;

        if (t < plist[next].art)
            t = plist[next].art;

        plist[next].wt = t - plist[next].art;

        t += plist[next].bt;
        completed[next] = 1;
    }
}

/*
 * SRTF:
 * At each time unit, run the arrived process with the shortest
 * remaining burst time.
 *
 * Ties preserve original input order.
 */
void findWaitingTimeSJF(ProcessType plist[], int n)
{
    if (n <= 0)
        return;

    int remaining[n];
    int completed = 0;
    int t = 0;

    for (int i = 0; i < n; i++)
    {
        remaining[i] = plist[i].bt;
        plist[i].wt = 0;
    }

    while (completed < n)
    {
        int shortest = -1;

        for (int i = 0; i < n; i++)
        {
            if (remaining[i] <= 0 || plist[i].art > t)
                continue;

            if (shortest == -1 ||
                remaining[i] < remaining[shortest])
            {
                shortest = i;
            }
        }

        if (shortest == -1)
        {
            int next_arrival = INT_MAX;

            for (int i = 0; i < n; i++)
            {
                if (remaining[i] > 0 &&
                    plist[i].art < next_arrival)
                {
                    next_arrival = plist[i].art;
                }
            }

            if (next_arrival == INT_MAX)
                break;

            t = next_arrival;
            continue;
        }

        remaining[shortest]--;
        t++;

        if (remaining[shortest] == 0)
        {
            int completion_time = t;

            plist[shortest].wt =
                completion_time -
                plist[shortest].art -
                plist[shortest].bt;

            if (plist[shortest].wt < 0)
                plist[shortest].wt = 0;

            completed++;
        }
    }
}

/*
 * Priority comparator.
 * Larger numeric priority values mean higher priority.
 * Equal priorities are ordered by arrival time, then PID.
 */
int my_comparer(const void *a, const void *b)
{
    const ProcessType *p1 = (const ProcessType *)a;
    const ProcessType *p2 = (const ProcessType *)b;

    if (p1->pri > p2->pri)
        return -1;

    if (p1->pri < p2->pri)
        return 1;

    if (p1->art < p2->art)
        return -1;

    if (p1->art > p2->art)
        return 1;

    if (p1->pid < p2->pid)
        return -1;

    if (p1->pid > p2->pid)
        return 1;

    return 0;
}

/*
 * Priority scheduling:
 * Among processes that have arrived, run the highest-priority process.
 * This is non-preemptive priority scheduling.
 */
void findavgTimePriority(ProcessType plist[], int n)
{
    if (n <= 0)
        return;

    int completed[n];
    memset(completed, 0, sizeof(completed));

    for (int i = 0; i < n; i++)
        plist[i].wt = 0;

    int t = 0;

    for (int k = 0; k < n; k++)
    {
        int next = -1;

        for (int i = 0; i < n; i++)
        {
            if (completed[i] || plist[i].art > t)
                continue;

            if (next == -1 ||
                plist[i].pri > plist[next].pri ||
                (plist[i].pri == plist[next].pri &&
                 plist[i].art < plist[next].art))
            {
                next = i;
            }
        }

        if (next == -1)
        {
            int next_arrival = INT_MAX;

            for (int i = 0; i < n; i++)
            {
                if (!completed[i] &&
                    plist[i].art < next_arrival)
                {
                    next_arrival = plist[i].art;
                }
            }

            if (next_arrival == INT_MAX)
                break;

            t = next_arrival;
            k--;
            continue;
        }

        plist[next].wt = t - plist[next].art;

        t += plist[next].bt;
        completed[next] = 1;
    }

    findTurnAroundTime(plist, n);
    printf("\n*********\nPriority\n");
}

/*
 * Round Robin:
 * A FIFO ready queue holds the processes that have arrived.
 * Newly arrived processes are enqueued during each time slice.
 */
void findWaitingTimeRR(ProcessType plist[], int n, int quantum)
{
    if (n <= 0 || quantum <= 0)
        return;

    int remaining[n];
    int queue[n];
    int head = 0;
    int tail = 0;
    int queued = 0;
    int completed = 0;
    int t = 0;

    for (int i = 0; i < n; i++)
    {
        remaining[i] = plist[i].bt;
        plist[i].wt = 0;
    }

    /*
     * Zero-burst processes finish at arrival time.
     * They do not need CPU time or a queue entry.
     */
    for (int i = 0; i < n; i++)
    {
        if (remaining[i] == 0)
        {
            plist[i].wt = 0;
            completed++;
        }
    }

    /*
     * Enqueue newly arrived processes in original input order.
     * queued[] prevents the same process from being added twice.
     */
    int enqueued[n];
    memset(enqueued, 0, sizeof(enqueued));

    while (completed < n)
    {
        for (int i = 0; i < n; i++)
        {
            if (!enqueued[i] &&
                remaining[i] > 0 &&
                plist[i].art <= t)
            {
                queue[tail] = i;
                tail = (tail + 1) % n;
                queued++;
                enqueued[i] = 1;
            }
        }

        if (queued == 0)
        {
            int next_arrival = INT_MAX;

            for (int i = 0; i < n; i++)
            {
                if (remaining[i] > 0 &&
                    !enqueued[i] &&
                    plist[i].art < next_arrival)
                {
                    next_arrival = plist[i].art;
                }
            }

            if (next_arrival == INT_MAX)
                break;

            t = next_arrival;
            continue;
        }

        int current = queue[head];
        head = (head + 1) % n;
        queued--;

        int slice = remaining[current] < quantum
                        ? remaining[current]
                        : quantum;

        remaining[current] -= slice;
        t += slice;

        /*
         * Add arrivals during the current slice before
         * re-enqueuing the process that just ran.
         */
        for (int i = 0; i < n; i++)
        {
            if (!enqueued[i] &&
                remaining[i] > 0 &&
                plist[i].art <= t)
            {
                queue[tail] = i;
                tail = (tail + 1) % n;
                queued++;
                enqueued[i] = 1;
            }
        }

        if (remaining[current] == 0)
        {
            plist[current].wt =
                t - plist[current].art - plist[current].bt;

            if (plist[current].wt < 0)
                plist[current].wt = 0;

            completed++;
        }
        else
        {
            queue[tail] = current;
            tail = (tail + 1) % n;
            queued++;
        }
    }
}

/*
 * Calculate turnaround time:
 * Turnaround time = waiting time + burst time.
 */
void findTurnAroundTime(ProcessType plist[], int n)
{
    for (int i = 0; i < n; i++)
    {
        plist[i].tat = plist[i].wt + plist[i].bt;
    }
}

/*
 * Print metrics and averages.
 * Results are printed in the original input order.
 */
void printMetrics(ProcessType plist[], int n)
{
    if (n <= 0)
    {
        printf("No processes to display.\n");
        return;
    }

    long long total_wt = 0;
    long long total_tat = 0;

    printf("\tProcesses\tBurst time\tWaiting time\tTurn around time\n");

    for (int i = 0; i < n; i++)
    {
        total_wt += plist[i].wt;
        total_tat += plist[i].tat;

        printf("\t%d\t\t%d\t\t%d\t\t%d\n",
               plist[i].pid,
               plist[i].bt,
               plist[i].wt,
               plist[i].tat);
    }

    printf("\nAverage waiting time = %.2f",
           (double)total_wt / n);

    printf("\nAverage turn around time = %.2f\n",
           (double)total_tat / n);
}

/*
 * FCFS wrapper
 */
void findavgTimeFCFS(ProcessType plist[], int n)
{
    findWaitingTime(plist, n);
    findTurnAroundTime(plist, n);

    printf("\n*********\nFCFS\n");
}

/*
 * SRTF wrapper
 */
void findavgTimeSJF(ProcessType plist[], int n)
{
    findWaitingTimeSJF(plist, n);
    findTurnAroundTime(plist, n);

    printf("\n*********\nSJF\n");
}

/*
 * Round Robin wrapper
 */
void findavgTimeRR(ProcessType plist[], int n, int quantum)
{
    findWaitingTimeRR(plist, n, quantum);
    findTurnAroundTime(plist, n);

    printf("\n*********\nRR Quantum = %d\n", quantum);
}

/*
 * Driver:
 * Load input once and create independent copies for each algorithm.
 */
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: ./schedsim <input-file-path>\n");
        return EXIT_FAILURE;
    }

    int n = 0;
    int quantum = 2;

    ProcessType *original = loadProcesses(argv[1], &n);

    ProcessType *work = malloc((size_t)n * sizeof(ProcessType));

    if (work == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(original);
        return EXIT_FAILURE;
    }

    // FCFS
    memcpy(work, original, (size_t)n * sizeof(ProcessType));
    findavgTimeFCFS(work, n);
    printMetrics(work, n);

    // SJF / SRTF
    memcpy(work, original, (size_t)n * sizeof(ProcessType));
    findavgTimeSJF(work, n);
    printMetrics(work, n);

    // Priority
    memcpy(work, original, (size_t)n * sizeof(ProcessType));
    findavgTimePriority(work, n);
    printMetrics(work, n);

    // Round Robin
    memcpy(work, original, (size_t)n * sizeof(ProcessType));
    findavgTimeRR(work, n, quantum);
    printMetrics(work, n);

    free(work);
    free(original);

    return EXIT_SUCCESS;
}
