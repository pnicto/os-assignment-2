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
            printf("Cleaning up secondary server...\n");

            while (1) {
                pid_t childPid = wait(NULL);
                if (childPid > 0) {
                printf("Waited for child process with pid %d\n", childPid);
                } else if (childPid == -1) {
                    if (errno == ECHILD) {
                        printf("No more children to wait for\n");
                        break;
                    } else {
                        perror("Error waiting for child process");
                        exit(1);
                    }
                }
            }
            break;
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

    // Read the graph file into adjMatrix
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

    /* outputLength is the number of vertices in the output array

    threadCountCurrent is the number of threads at the current depth, equal to
    number of nodes at the current depth

    threadCountNext is the number of threads at the next depth, equal to number
    of nodes at the next depth */
    int outputLength = 0, threadCountCurrent = 1, threadCountNext = 1;
    int output[nodeCount];

    // Initialize mutexes and semaphores
    pthread_mutex_t outputMutex, countStartMutex, countEndMutex;
    if (pthread_mutex_init(&outputMutex, NULL) == -1 ||
        pthread_mutex_init(&countStartMutex, NULL) == -1 ||
        pthread_mutex_init(&countEndMutex, NULL) == -1)
    {
        perror("Error initializing mutex");
        exit(1);
    }

    sem_t startSemaphore, endSemaphore;
    if (sem_init(&startSemaphore, 0, 1) == -1 ||
        sem_init(&endSemaphore, 0, 1) == -1)
    {
        perror("Error initializing semaphore");
        exit(1);
    }

    struct BfsThreadArgs args[nodeCount];

    // Initialize the arguments for the starting vertex
    args[startingVertex].vertex = startingVertex;
    args[startingVertex].previousVertex = -1;
    args[startingVertex].nodeCount = nodeCount;
    args[startingVertex].adjMatrix = adjMatrix;
    args[startingVertex].output = output;
    args[startingVertex].outputLength = &outputLength;
    args[startingVertex].threadCountCurrent = &threadCountCurrent;
    args[startingVertex].threadCountNext = &threadCountNext;
    args[startingVertex].args = args;
    args[startingVertex].outputMutex = &outputMutex;
    args[startingVertex].countStartMutex = &countStartMutex;
    args[startingVertex].countEndMutex = &countEndMutex;
    args[startingVertex].startSemaphore = &startSemaphore;
    args[startingVertex].endSemaphore = &endSemaphore;

    sem_wait(&startSemaphore);

    pthread_t thread;
    pthread_create(&thread, NULL, bfsThreadFunction,
                   (void *)&args[startingVertex]);
    pthread_join(thread, NULL);

    pthread_mutex_destroy(&outputMutex);
    pthread_mutex_destroy(&countStartMutex);
    pthread_mutex_destroy(&countEndMutex);
    sem_destroy(&startSemaphore);
    sem_destroy(&endSemaphore);

    char responseString[RESPONSE_SIZE] = {0};

    for (int i = 0; i < outputLength; i++)
    {
        char tempString[RESPONSE_SIZE] = {0};
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

static void *bfsThreadFunction(void *args)
{
    struct BfsThreadArgs *threadArgs = (struct BfsThreadArgs *)args;

    pthread_mutex_lock(threadArgs->countStartMutex);
    (*(threadArgs->threadCountNext))--;
    if (*(threadArgs->threadCountNext) == 0)
    {
        // If all threads at current depth have started, lock the end semaphore
        // and unlock the start semaphore
        sem_wait(threadArgs->endSemaphore);
        sem_post(threadArgs->startSemaphore);
    }
    pthread_mutex_unlock(threadArgs->countStartMutex);

    // Wait for all threads at current depth to start
    sem_wait(threadArgs->startSemaphore);
    sem_post(threadArgs->startSemaphore);

    pthread_t threads[threadArgs->nodeCount];
    int childNodes[threadArgs->nodeCount];
    int childThreadCount = 0;

    for (int i = 0; i < threadArgs->nodeCount; i++)
    {
        // Check for connections other than parent
        if ((threadArgs->adjMatrix[threadArgs->vertex * threadArgs->nodeCount +
                                   i] == 1) &&
            (i != threadArgs->previousVertex))
        {
            pthread_mutex_lock(threadArgs->countEndMutex);
            (*(threadArgs->threadCountNext))++;
            pthread_mutex_unlock(threadArgs->countEndMutex);

            childNodes[childThreadCount] = i;
            childThreadCount++;
        }
    }

    pthread_mutex_lock(threadArgs->outputMutex);
    threadArgs->output[*(threadArgs->outputLength)] =
        threadArgs->vertex + 1; // +1 because of 0 indexing
    *(threadArgs->outputLength) = *(threadArgs->outputLength) + 1;
    pthread_mutex_unlock(threadArgs->outputMutex);

    pthread_mutex_lock(threadArgs->countEndMutex);
    (*(threadArgs->threadCountCurrent))--;
    if (*(threadArgs->threadCountCurrent) == 0)
    {
        // If all threads at current depth have ended, lock the start semaphore
        // and unlock the end semaphore
        // Also set the thread count for the next depth
        *(threadArgs->threadCountCurrent) = *(threadArgs->threadCountNext);
        sem_wait(threadArgs->startSemaphore);
        sem_post(threadArgs->endSemaphore);
    }
    pthread_mutex_unlock(threadArgs->countEndMutex);

    sem_wait(threadArgs->endSemaphore);
    sem_post(threadArgs->endSemaphore);

    for (int i = 0; i < childThreadCount; i++)
    {
        initBfsArgs(&(threadArgs->args[childNodes[i]]), threadArgs,
                    childNodes[i]);

        pthread_create(&threads[i], NULL, bfsThreadFunction,
                       (void *)&threadArgs->args[childNodes[i]]);
    }

    for (int i = 0; i < childThreadCount; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_exit(NULL);
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

    int outputLength = 0;
    int output[nodeCount];

    pthread_mutex_t mutex;
    if (pthread_mutex_init(&mutex, NULL) == -1)
    {
        perror("Error initializing mutex");
        exit(1);
    }

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

    char responseString[RESPONSE_SIZE] = {0};

    for (int i = 0; i < outputLength; i++)
    {
        char tempString[RESPONSE_SIZE] = {0};
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
        // Check for connections other than parent
        if ((threadArgs->adjMatrix[threadArgs->vertex * threadArgs->nodeCount +
                                   i] == 1) &&
            (i != threadArgs->previousVertex))
        {
            isLeaf = 0;

            initDfsArgs(&(threadArgs->args[i]), threadArgs, i);

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

void initBfsArgs(struct BfsThreadArgs *destination,
                 const struct BfsThreadArgs *source, int vertex)
{
    destination->vertex = vertex;
    destination->previousVertex = source->vertex;
    destination->nodeCount = source->nodeCount;
    destination->adjMatrix = source->adjMatrix;
    destination->output = source->output;
    destination->outputLength = source->outputLength;
    destination->threadCountCurrent = source->threadCountCurrent;
    destination->threadCountNext = source->threadCountNext;
    destination->args = source->args;
    destination->outputMutex = source->outputMutex;
    destination->countStartMutex = source->countStartMutex;
    destination->countEndMutex = source->countEndMutex;
    destination->startSemaphore = source->startSemaphore;
    destination->endSemaphore = source->endSemaphore;
}

void initDfsArgs(struct DfsThreadArgs *destination,
                 const struct DfsThreadArgs *source, int vertex)
{
    destination->vertex = vertex;
    destination->previousVertex = source->vertex;
    destination->nodeCount = source->nodeCount;
    destination->adjMatrix = source->adjMatrix;
    destination->mutex = source->mutex;
    destination->output = source->output;
    destination->outputLength = source->outputLength;
    destination->args = source->args;
}