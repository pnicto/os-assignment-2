#ifndef SECONDARY_SERVER_H
#define SECONDARY_SERVER_H

#include <pthread.h>
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
    int vertex;
    int previousVertex;
    int nodeCount;
    int *adjMatrix;
    pthread_mutex_t *mutex;
    int *output;
    int *outputLength;
    struct DfsThreadArgs *args;
};

static void *threadFunc(void *arg);
void bfs(struct MessageBuffer msg, int *shmp, int messageQueueID);
void dfs(struct MessageBuffer msg, int *shmp, int messageQueueID);

static void *dfsThreadFunction(void *args);

#endif
