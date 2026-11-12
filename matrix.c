#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define MAX_SIZE 100     // largest matrix size we allocate storage for
#define NUM_THREADS 10   // threads used per matrix operation

int MAX = 20;            // actual matrix size (set from the command line)

int matA[MAX_SIZE][MAX_SIZE];
int matB[MAX_SIZE][MAX_SIZE];

int matSumResult[MAX_SIZE][MAX_SIZE];
int matDiffResult[MAX_SIZE][MAX_SIZE];
int matProductResult[MAX_SIZE][MAX_SIZE];

void fillMatrix(int matrix[MAX_SIZE][MAX_SIZE]) {
    for(int i = 0; i<MAX; i++) {
        for(int j = 0; j<MAX; j++) {
            matrix[i][j] = rand()%10+1;
        }
    }
}

void printMatrix(int matrix[MAX_SIZE][MAX_SIZE]) {
    for(int i = 0; i<MAX; i++) {
        for(int j = 0; j<MAX; j++) {
            printf("%5d", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// Works out which rows thread number `id` is responsible for.
// The MAX rows are split as evenly as possible across NUM_THREADS threads
// (for a 20x20 matrix and 10 threads, each thread gets 2 rows).
static void getRowRange(int id, int *start, int *end) {
    int base = MAX / NUM_THREADS;
    int extra = MAX % NUM_THREADS;   // first `extra` threads take one more row
    *start = id * base + (id < extra ? id : extra);
    *end = *start + base + (id < extra ? 1 : 0);
}

// Fetches the appropriate coordinates from the argument, and sets
// the cell of matSumResult at the coordinates to the sum of the
// values at the coordinates of matA and matB.
void* computeSum(void* args) { // pass in the number of the ith thread
    int id = *(int*)args;
    free(args);

    int start, end;
    getRowRange(id, &start, &end);
    for(int i = start; i < end; i++) {
        for(int j = 0; j < MAX; j++) {
            matSumResult[i][j] = matA[i][j] + matB[i][j];
        }
    }
    return NULL;
}

// Fetches the appropriate coordinates from the argument, and sets
// the cell of matDiffResult at the coordinates to the difference of the
// values at the coordinates of matA and matB.
void* computeDiff(void* args) { // pass in the number of the ith thread
    int id = *(int*)args;
    free(args);

    int start, end;
    getRowRange(id, &start, &end);
    for(int i = start; i < end; i++) {
        for(int j = 0; j < MAX; j++) {
            matDiffResult[i][j] = matA[i][j] - matB[i][j];
        }
    }
    return NULL;
}

// Fetches the appropriate coordinates from the argument, and sets
// the cell of matProductResult at the coordinates to the inner product
// of matA and matB.
void* computeProduct(void* args) { // pass in the number of the ith thread
    int id = *(int*)args;
    free(args);

    int start, end;
    getRowRange(id, &start, &end);
    for(int i = start; i < end; i++) {
        for(int j = 0; j < MAX; j++) {
            int sum = 0;
            for(int k = 0; k < MAX; k++) {
                sum += matA[i][k] * matB[k][j];
            }
            matProductResult[i][j] = sum;
        }
    }
    return NULL;
}

// Runs one matrix operation using NUM_THREADS threads and waits for them.
static void runOperation(void* (*op)(void*)) {
    pthread_t threads[NUM_THREADS];

    for(int i = 0; i < NUM_THREADS; i++) {
        int *id = malloc(sizeof(int));   // freed by the thread
        if(id == NULL) {
            perror("malloc");
            exit(1);
        }
        *id = i;
        if(pthread_create(&threads[i], NULL, op, id) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    for(int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}

// Spawn threads to fill each result matrix.
// Each operation is divided among NUM_THREADS (10) threads.
int main(int argc, char *argv[]) {
    srand(time(0));  // Do Not Remove. Just ignore and continue below.

    // 0. Get the matrix size from the command line and assign it to MAX
    if(argc > 1) {
        MAX = atoi(argv[1]);
    }
    if(MAX < 1 || MAX > MAX_SIZE) {
        fprintf(stderr, "Matrix size must be between 1 and %d\n", MAX_SIZE);
        return 1;
    }

    // 1. Fill the matrices (matA and matB) with random values.
    fillMatrix(matA);
    fillMatrix(matB);

    // 2. Print the initial matrices.
    printf("Matrix A:\n");
    printMatrix(matA);
    printf("Matrix B:\n");
    printMatrix(matB);

    // 3-5. Create the threads for each operation (each thread receives a
    // malloc'd thread number, uses it to find its rows, then frees it),
    // and wait for all of them to finish.
    runOperation(computeSum);
    runOperation(computeDiff);
    runOperation(computeProduct);

    // 6. Print the results.
    printf("Results:\n");
    printf("Sum:\n");
    printMatrix(matSumResult);
    printf("Difference:\n");
    printMatrix(matDiffResult);
    printf("Product:\n");
    printMatrix(matProductResult);
    return 0;
}
