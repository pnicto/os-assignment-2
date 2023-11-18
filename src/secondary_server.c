#include "../include/secondary_server.h"

sem_t *writeSemaphores[20];
sem_t *readCountSemaphore;
int numReaders;

int main(int argc, char *argv[])
{
    printf("Initializing secondary server...\n");

    // open named semaphores
    char filename[FILE_NAME_SIZE];
    for (int i = 1; i <= 20; i++)
    {
        snprintf(filename, FILE_NAME_SIZE, WRITE_SEMAPHORE_FORMAT, i);
        writeSemaphores[i - 1] = sem_open(filename, O_EXCL, 0644, 1);
        if (writeSemaphores[i - 1] == SEM_FAILED)
        {
            perror("Error initializing write semaphore in sem_open");
            exit(1);
        }
    }

    readCountSemaphore = sem_open(READ_COUNT_SEMAPHORE_NAME, O_EXCL, 0644, 1);
    if (readCountSemaphore == SEM_FAILED)
    {
        perror("Error initializing read count semaphore in sem_open");
        exit(1);
    }

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

    if (argc < 2)
    {
        printf("Server number is needed as a command line argument\n");
        exit(1);
    }

    int serverID = atoi(argv[1]);

    if (serverID < 1 || serverID > 2)
    {
        printf("Server number must be 1 or 2\n");
        exit(1);
    }

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
            // Cleanup
            for (int i = 0; i < t; i++)
            {
                if (pthread_join(threads[i], NULL))
                {
                    perror("Error in pthread_join");
                    exit(1);
                }
            }
            printf("Terminating Secondary Server...\n");
            exit(0);
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

    if (sem_post(readCountSemaphore) == -1)
    {
        perror("Error in sem_post");
        exit(1);
    }
    if (sem_getvalue(readCountSemaphore, &numReaders) == -1)
    {
        perror("Error in sem_getvalue");
        exit(1);
    }
    if (numReaders == 1)
    {
        int n = extractNumber(msg.graphFileName);
        printf("Waiting for file %s to read\n", msg.graphFileName);
        if (sem_wait(writeSemaphores[n - 1]) == -1)
        {
            perror("Error in sem_wait");
            exit(1);
        }
        printf("Acquired file %s\n", msg.graphFileName);
    }

    FILE *fp = fopen(msg.graphFileName, "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        exit(1);
    }

    printf("Reading file %s\n", msg.graphFileName);

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

    if (sem_wait(readCountSemaphore) == -1)
    {
        perror("Error in sem_wait");
        exit(1);
    }
    if (sem_getvalue(readCountSemaphore, &numReaders) == -1)
    {
        perror("Error in sem_getvalue");
        exit(1);
    }
    if (numReaders == 0)
    {
        int n = extractNumber(msg.graphFileName);
        if (sem_post(writeSemaphores[n - 1]) == -1)
        {
            perror("Error in sem_post");
            exit(1);
        }
    }

    /* outputLength is the number of vertices in the output array

    threadCountCurrent is the number of threads at the current depth, equal to
    number of nodes at the current depth

    threadCountNext is the number of threads at the next depth, equal to number
    of nodes at the next depth */
    int outputLength = 0, threadCountCurrent = 1, threadCountNext = 1;
    int output[nodeCount];

    // Initialize mutexes and semaphores
    pthread_mutex_t outputMutex, countStartMutex, countEndMutex;
    if (pthread_mutex_init(&outputMutex, NULL) ||
        pthread_mutex_init(&countStartMutex, NULL) ||
        pthread_mutex_init(&countEndMutex, NULL))
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

    if (sem_wait(&startSemaphore) == -1)
    {
        perror("Error in sem_wait");
        exit(1);
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, bfsThreadFunction,
                       (void *)&args[startingVertex]))
    {
        perror("Error in pthread_create");
        exit(1);
    }

    if (pthread_join(thread, NULL))
    {
        perror("Error in pthread_join");
        exit(1);
    }

    if (pthread_mutex_destroy(&outputMutex) ||
        pthread_mutex_destroy(&countStartMutex) ||
        pthread_mutex_destroy(&countEndMutex))
    {
        perror("Error destroying mutex");
        exit(1);
    }

    if (sem_destroy(&startSemaphore) == -1 || sem_destroy(&endSemaphore) == -1)
    {
        perror("Error destroying semaphore");
        exit(1);
    }

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
}

static void *bfsThreadFunction(void *args)
{
    struct BfsThreadArgs *threadArgs = (struct BfsThreadArgs *)args;

    if (pthread_mutex_lock(threadArgs->countStartMutex))
    {
        perror("Error in pthread_mutex_lock");
        exit(1);
    }
    (*(threadArgs->threadCountNext))--;
    if (*(threadArgs->threadCountNext) == 0)
    {
        // If all threads at current depth have started, lock the end semaphore
        // and unlock the start semaphore
        if (sem_wait(threadArgs->endSemaphore) == -1)
        {
            perror("Error in sem_wait");
            exit(1);
        }
        if (sem_post(threadArgs->startSemaphore) == -1)
        {
            perror("Error in sem_post");
            exit(1);
        }
    }
    if (pthread_mutex_unlock(threadArgs->countStartMutex))
    {
        perror("Error in pthread_mutex_unlock");
        exit(1);
    }

    // Wait for all threads at current depth to start
    if (sem_wait(threadArgs->startSemaphore) == -1)
    {
        perror("Error in sem_wait");
        exit(1);
    }
    if (sem_post(threadArgs->startSemaphore) == -1)
    {
        perror("Error in sem_post");
        exit(1);
    }

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
            if (pthread_mutex_lock(threadArgs->countEndMutex))
            {
                perror("Error in pthread_mutex_lock");
                exit(1);
            }
            (*(threadArgs->threadCountNext))++;
            if (pthread_mutex_unlock(threadArgs->countEndMutex))
            {
                perror("Error in pthread_mutex_unlock");
                exit(1);
            }

            childNodes[childThreadCount] = i;
            childThreadCount++;
        }
    }

    if (pthread_mutex_lock(threadArgs->outputMutex))
    {
        perror("Error in pthread_mutex_lock");
        exit(1);
    }
    threadArgs->output[*(threadArgs->outputLength)] =
        threadArgs->vertex + 1; // +1 because of 0 indexing
    *(threadArgs->outputLength) = *(threadArgs->outputLength) + 1;
    if (pthread_mutex_unlock(threadArgs->outputMutex))
    {
        perror("Error in pthread_mutex_unlock");
        exit(1);
    }

    if (pthread_mutex_lock(threadArgs->countEndMutex))
    {
        perror("Error in pthread_mutex_lock");
        exit(1);
    }
    (*(threadArgs->threadCountCurrent))--;
    if (*(threadArgs->threadCountCurrent) == 0)
    {
        // If all threads at current depth have ended, lock the start semaphore
        // and unlock the end semaphore
        // Also set the thread count for the next depth
        *(threadArgs->threadCountCurrent) = *(threadArgs->threadCountNext);
        if (sem_wait(threadArgs->startSemaphore) == -1)
        {
            perror("Error in sem_wait");
            exit(1);
        }
        if (sem_post(threadArgs->endSemaphore) == -1)
        {
            perror("Error in sem_post");
            exit(1);
        }
    }
    if (pthread_mutex_unlock(threadArgs->countEndMutex))
    {
        perror("Error in pthread_mutex_unlock");
        exit(1);
    }

    if (sem_wait(threadArgs->endSemaphore) == -1)
    {
        perror("Error in sem_wait");
        exit(1);
    }
    if (sem_post(threadArgs->endSemaphore) == -1)
    {
        perror("Error in sem_post");
        exit(1);
    }

    for (int i = 0; i < childThreadCount; i++)
    {
        initBfsArgs(&(threadArgs->args[childNodes[i]]), threadArgs,
                    childNodes[i]);

        if (pthread_create(&threads[i], NULL, bfsThreadFunction,
                           (void *)&threadArgs->args[childNodes[i]]))
        {
            perror("Error in pthread_create");
            exit(1);
        }
    }

    for (int i = 0; i < childThreadCount; i++)
    {
        if (pthread_join(threads[i], NULL))
        {
            perror("Error in pthread_join");
            exit(1);
        }
    }

    pthread_exit(NULL);
}

void dfs(struct MessageBuffer msg, int *shmp, int messageQueueID)
{
    int startingVertex = shmp[0] - 1; // -1 because of 0 indexing

    if (sem_post(readCountSemaphore) == -1)
    {
        perror("Error in sem_post");
        exit(1);
    }
    if (sem_getvalue(readCountSemaphore, &numReaders) == -1)
    {
        perror("Error in sem_getvalue");
        exit(1);
    }
    if (numReaders == 1)
    {
        int n = extractNumber(msg.graphFileName);
        printf("Waiting for file %s to read\n", msg.graphFileName);
        sem_wait(writeSemaphores[n - 1]);
        printf("Acquired file %s\n", msg.graphFileName);
    }

    FILE *fp = fopen(msg.graphFileName, "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        exit(1);
    }

    printf("Reading file %s\n", msg.graphFileName);

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

    if (sem_wait(readCountSemaphore) == -1)
    {
        perror("Error in sem_wait");
        exit(1);
    }
    if (sem_getvalue(readCountSemaphore, &numReaders) == -1)
    {
        perror("Error in sem_getvalue");
        exit(1);
    }
    if (numReaders == 0)
    {
        int n = extractNumber(msg.graphFileName);
        if (sem_post(writeSemaphores[n - 1]) == -1)
        {
            perror("Error in sem_post");
            exit(1);
        }
    }

    int outputLength = 0;
    int output[nodeCount];

    pthread_mutex_t mutex;
    if (pthread_mutex_init(&mutex, NULL))
    {
        perror("Error in pthread_mutex_init");
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
    if (pthread_create(&thread, NULL, dfsThreadFunction,
                       (void *)&args[startingVertex]))
    {
        perror("Error in pthread_create");
        exit(1);
    }

    if (pthread_join(thread, NULL))
    {
        perror("Error in pthread_join");
        exit(1);
    }

    if (pthread_mutex_destroy(&mutex))
    {
        perror("Error in pthread_mutex_destroy");
        exit(1);
    }

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

            if (pthread_create(&threads[childThreadCount], NULL,
                               dfsThreadFunction, (void *)&threadArgs->args[i]))
            {
                perror("Error in pthread_create");
                exit(1);
            }
            childThreadCount++;
        }
    }

    if (isLeaf)
    {
        if (pthread_mutex_lock(threadArgs->mutex))
        {
            perror("Error in pthread_mutex_lock");
            exit(1);
        }
        threadArgs->output[*(threadArgs->outputLength)] =
            threadArgs->vertex + 1; // +1 because of 0 indexing
        *(threadArgs->outputLength) = *(threadArgs->outputLength) + 1;
        if (pthread_mutex_unlock(threadArgs->mutex))
        {
            perror("Error in pthread_mutex_unlock");
            exit(1);
        }
    }
    else
    {
        for (int i = 0; i < childThreadCount; i++)
        {
            if (pthread_join(threads[i], NULL))
            {
                perror("Error in pthread_join");
                exit(1);
            }
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

int extractNumber(char *filename)
{
    int number;
    sscanf(filename, "G%d.txt", &number);
    return number;
}
