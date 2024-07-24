/**
 * @file computesum.c
 * @author Yelisetty Karthikeya S M (21CS30060)
 * @brief Compute the sum of a tree using threads
 * 
 * This file contains the implementation of the main function to compute the sum of a tree using threads
 * The program reads the tree from a file (tree.txt) and computes the sum of the tree using threads
 * The program uses the foothread library, a user-level thread library
 * 
 * @date 2021-03-31
 * @copyright MIT License
*/
#include <foothread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

foothread_mutex_t mutex;
foothread_t *threads;
foothread_barrier_t *barriers;
int thread_count;
int *P, *ChildCount, *sums;

// Compute the sum of the tree
int computesum(void *arg)
{
    uintptr_t idx = (uintptr_t)arg;
    if (ChildCount[idx] == 0)
    {
        foothread_mutex_lock(&mutex);
        int val;
        printf("Leaf Node %2d :: Enter a positive integer: ", (int)idx);
        scanf("%d", &val);
        sums[P[idx]] += val;
        foothread_mutex_unlock(&mutex);
        foothread_barrier_wait(&barriers[P[idx]]);
    }
    else
    {
        foothread_barrier_wait(&barriers[idx]);
        foothread_mutex_lock(&mutex);
        printf("Internal Node %2d gets the partial sum %2d from its children\n", (int)idx, sums[idx]);
        if (P[idx] != idx) sums[P[idx]] += sums[idx];
        foothread_mutex_unlock(&mutex);
        foothread_barrier_wait(&barriers[P[idx]]);
    }
    foothread_exit();
    return 0; 
}

void exit_handler(int sig)
{
    free(threads);
    free(barriers);
    free(P);
    free(ChildCount);
    free(sums);

    for (int i = 0; i < thread_count; i++)
    {
        foothread_barrier_destroy(&barriers[i]);
    }

    if (sig) printf("Exiting Gracefully\n");
    exit(0);
}

int main()
{
    signal(SIGTERM, exit_handler);
    signal(SIGINT, exit_handler);
    int n;
    FILE *fp;
    fp = fopen("tree.txt", "r");
    fscanf(fp, "%d", &n);
    
    P = (int *)malloc(n * sizeof(int));
    memset(P, 0, n * sizeof(int));
    for (int i = 0; i < n; i++)
    {
        int idx;
        fscanf(fp, "%d", &idx);
        fscanf(fp, "%d", &P[idx]);
    }
    fclose(fp);

    // calculate child count
    ChildCount = (int *)malloc(n * sizeof(int));
    memset(ChildCount, 0, n * sizeof(int));
    for (int i = 0; i < n; i++)
    {
        if (P[i] != i)
        {
            ChildCount[P[i]]++;
        }
    }

    // Initialize global variables
    threads = (foothread_t *)malloc(n * sizeof(foothread_t));
    barriers = (foothread_barrier_t *)malloc(n * sizeof(foothread_barrier_t));
    thread_count = n;
    sums = (int *)malloc(n * sizeof(int)); // partial sums
    memset(sums, 0, n * sizeof(int));
    
    foothread_attr_t attr = FOOTHREAD_ATTR_INITIALIZER;
    foothread_attr_setjointype(&attr, FOOTHREAD_JOINABLE);
    foothread_mutex_init(&mutex);
    for (int idx = 0; idx < n; idx++)
        foothread_barrier_init(&barriers[idx], ChildCount[idx] + 1);

    for (uintptr_t i = 0; i < n; i++)
    {
        foothread_create(&threads[i], &attr, computesum, (void *)i);
    }

    foothread_exit();
    for (int i = 0; i < n; i++)
    {
        if (P[i] == i)
        {
            printf("Sum at root (node %d) = %d\n", i, sums[i]);
        }
    }

    // destroy mutex and barriers
    foothread_mutex_destroy(&mutex);
    for (int i = 0; i < n; i++)
    {
        foothread_barrier_destroy(&barriers[i]);
    }

    exit_handler(0);
    return 0;
}
