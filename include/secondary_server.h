#ifndef SECONDARY_SERVER_H
#define SECONDARY_SERVER_H

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

struct ThreadArgs
{
    int messageQueueID;
    struct MessageBuffer messageBuffer;
};

struct DfsThreadArgs
{
    int vertex, previousVertex, nodeCount;
    int *adjMatrix;
    int *output, *outputLength;
    struct DfsThreadArgs *args;
    pthread_mutex_t *mutex;
};

struct BfsThreadArgs
{
    int vertex, previousVertex, nodeCount;
    int *adjMatrix;
    int *output, *outputLength, *threadCountCurrent, *threadCountNext;
    struct BfsThreadArgs *args;
    pthread_mutex_t *outputMutex, *countStartMutex, *countEndMutex;
    sem_t *startSemaphore, *endSemaphore;
};

static void *threadFunc(void *arg);
void bfs(struct MessageBuffer msg, int *shmp, int messageQueueID);
void dfs(struct MessageBuffer msg, int *shmp, int messageQueueID);

static void *dfsThreadFunction(void *args);
static void *bfsThreadFunction(void *args);

void initBfsArgs(struct BfsThreadArgs *destination,
                 const struct BfsThreadArgs *source, int vertex);
void initDfsArgs(struct DfsThreadArgs *destination,
                 const struct DfsThreadArgs *source, int vertex);

#endif
