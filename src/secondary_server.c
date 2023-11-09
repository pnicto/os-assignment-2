#include "../include/secondary_server.h"

int main(int argc, char *argv[])
{
    printf("Initializing secondary server...\n");

    int messageQueueID;
    key_t messageQueueKey;

    if ((messageQueueKey = ftok(PATHNAME, PROJ_ID)) == -1)
    {
        perror("Error generating key in ftok");
        exit(1);
    }

    if ((messageQueueID = msgget(messageQueueKey, PERMS)) == -1)
    {
        perror("Error connecting to message queue in msgget. Is the load "
               "balancer on?");
        exit(1);
    }

    (void)argc; // to get rid of unused parameter warning
    int serverID = atoi(argv[1]);
    printf("Secondary Server %d initialized. Listening for requests.\n",
           serverID);

    pthread_t threads[100];
    int t = 0;

    while (1)
    {
        struct ThreadArgs threadArgs;
        threadArgs.messageQueueID = messageQueueID;
        if (msgrcv(messageQueueID, &threadArgs.messageBuffer,
                   sizeof(threadArgs.messageBuffer) -
                       sizeof(threadArgs.messageBuffer.mtype),
                   serverID + 2, 0) == -1)
        {
            perror("Error receiving message in msgrcv");
            exit(1);
        }

        if (threadArgs.messageBuffer.sequenceNumber == -1)
        {
            // Perform cleanup, join threads etc...
        }

        if (pthread_create(&threads[t], NULL, threadFunc, &threadArgs))
        {
            perror("Error in pthread_create");
            exit(1);
        }

        t++;
    }

    return 0;
}

static void *threadFunc(void *arg)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)arg;
    int shmid;
    int *shmp; // Shared memory only stores vertex number

    shmid =
        shmget(threadArgs->messageBuffer.sequenceNumber, sizeof(int), PERMS);
    if (shmid == -1)
    {
        perror("Error in shmget");
        exit(1);
    }

    shmp = shmat(shmid, NULL, 0);
    if (shmp == (void *)-1)
    {
        perror("Error in shmat");
        exit(1);
    }

    if (threadArgs->messageBuffer.operationNumber == 3)
    {
        dfs(threadArgs->messageBuffer, shmp, threadArgs->messageQueueID);
    }
    else
    {
        bfs(threadArgs->messageBuffer, shmp, threadArgs->messageQueueID);
    }

    pthread_exit(NULL);
}

void bfs(struct MessageBuffer msg, int *shmp, int messageQueueID)
{
    // perform and send output through message queue

    // TEMP to stop unused parameter warning
    (void)msg;
    (void)shmp;
    (void)messageQueueID;
}

void dfs(struct MessageBuffer msg, int *shmp, int messageQueueID)
{
    int startingVertex = shmp[0] - 1; // -1 because of 0 indexing

    // TODO: Add named semaphore to make sure both read and write don't happen
    // together
    FILE *fp = fopen(msg.graphFileName, "r");

    if (fp == NULL)
    {
        perror("Error opening file");
        exit(1);
    }

    int nodeCount;
    fscanf(fp, "%d", &nodeCount);

    int adjMatrix[nodeCount * nodeCount];

    for (int i = 0; i < nodeCount; i++)
    {
        for (int j = 0; j < nodeCount; j++)
        {
            fscanf(fp, "%d", &adjMatrix[i * nodeCount + j]);
        }
    }

    fclose(fp);
    // TODO: release/increment semaphore

    // TEMP
    printf("Initial matrix:\n");
    for (int i = 0; i < nodeCount; i++)
    {
        for (int j = 0; j < nodeCount; j++)
        {
            printf("%d ", adjMatrix[i * nodeCount + j]);
        }
        printf("\n");
    }

    int outputLength = 0;
    int output[nodeCount];

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    struct DfsThreadArgs args[nodeCount];

    args[startingVertex].vertex = startingVertex;
    args[startingVertex].previousVertex = -1;
    args[startingVertex].nodeCount = nodeCount;
    args[startingVertex].adjMatrix = adjMatrix;
    args[startingVertex].mutex = &mutex;
    args[startingVertex].output = output;
    args[startingVertex].outputLength = &outputLength;
    args[startingVertex].args = args;

    pthread_t thread;
    pthread_create(&thread, NULL, dfsThreadFunction,
                   (void *)&args[startingVertex]);

    pthread_join(thread, NULL);
    pthread_mutex_destroy(&mutex);

    // TEMP
    printf("Final matrix:\n");
    for (int i = 0; i < nodeCount; i++)
    {
        for (int j = 0; j < nodeCount; j++)
        {
            printf("%d ", adjMatrix[i * nodeCount + j]);
        }
        printf("\n");
    }

    char responseString[RESPONSE_SIZE];

    for (int i = 0; i < outputLength; i++)
    {
        char tempString[RESPONSE_SIZE];
        sprintf(tempString, "%d ", output[i]);
        strcat(responseString, tempString);
    }

    struct MessageBuffer responseBuffer;
    responseBuffer.mtype = msg.sequenceNumber + 10;
    responseBuffer.sequenceNumber = msg.sequenceNumber;
    responseBuffer.operationNumber = msg.operationNumber;
    strcpy(responseBuffer.response, responseString);

    if (msgsnd(messageQueueID, &responseBuffer,
               sizeof(responseBuffer) - sizeof(responseBuffer.mtype), 0) == -1)
    {
        perror("Error sending message in msgsnd");
        exit(1);
    }

    pthread_exit(NULL);
}

static void *dfsThreadFunction(void *args)
{
    struct DfsThreadArgs *threadArgs = (struct DfsThreadArgs *)args;

    int isLeaf = 1;
    pthread_t threads[threadArgs->nodeCount];
    int childThreadCount = 0;

    for (int i = 0; i < threadArgs->nodeCount; i++)
    {
        if ((threadArgs->adjMatrix[threadArgs->vertex * threadArgs->nodeCount +
                                   i] == 1) &&
            (i != threadArgs->previousVertex))
        {
            isLeaf = 0;

            threadArgs->args[i].vertex = i;
            threadArgs->args[i].previousVertex = threadArgs->vertex;
            threadArgs->args[i].nodeCount = threadArgs->nodeCount;
            threadArgs->args[i].adjMatrix = threadArgs->adjMatrix;
            threadArgs->args[i].mutex = threadArgs->mutex;
            threadArgs->args[i].output = threadArgs->output;
            threadArgs->args[i].outputLength = threadArgs->outputLength;
            threadArgs->args[i].args = threadArgs->args;

            pthread_create(&threads[childThreadCount], NULL, dfsThreadFunction,
                           (void *)&threadArgs->args[i]);
            childThreadCount++;
        }
    }

    if (isLeaf)
    {
        pthread_mutex_lock(threadArgs->mutex);

        threadArgs->output[*(threadArgs->outputLength)] =
            threadArgs->vertex + 1; // +1 because of 0 indexing
        *(threadArgs->outputLength) = *(threadArgs->outputLength) + 1;
        threadArgs->outputLength++;

        pthread_mutex_unlock(threadArgs->mutex);
    }
    else
    {
        for (int i = 0; i < childThreadCount; i++)
        {
            pthread_join(threads[i], NULL);
        }
    }

    pthread_exit(NULL);
}