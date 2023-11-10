#include "../include/primary_server.h"

int main()
{
    printf("Initializing primary server...\n");

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

    printf("Primary Server initialized. Listening for requests.\n");

    pthread_t threads[100];
    int t = 0;

    while (1)
    {
        struct MessageBuffer messageBuffer;
        if (msgrcv(messageQueueID, &messageBuffer,
                   sizeof(messageBuffer) - sizeof(messageBuffer.mtype), 2,
                   0) == -1)
        {
            perror("Error receiving message in msgrcv");
            exit(1);
        }

        if (messageBuffer.sequenceNumber == -1)
        {
            // Perform cleanup, join threads etc...
        }
        struct ThreadArgs threadArgs;
        threadArgs.messageQueueID = messageQueueID;
        threadArgs.messageBuffer = messageBuffer;

        if (pthread_create(&threads[t], NULL, threadFunc, &threadArgs))
        {
            perror("pthread_create");
            exit(1);
        }

        t++;
    }

    return 0;
}

void *threadFunc(void *arg)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)arg;
    int shmId;
    struct ShmSeg *shmSegPtr;
    shmId = shmget(threadArgs->messageBuffer.sequenceNumber,
                   sizeof(struct ShmSeg), PERMS);
    if (shmId == -1)
    {
        perror("Error in shmget");
        exit(1);
    }
    shmSegPtr = (struct ShmSeg *)shmat(shmId, NULL, 0);
    if (shmSegPtr == (void *)-1)
    {
        perror("Error in shmat");
        exit(1);
    }

    if (threadArgs->messageBuffer.operationNumber == 1)
    {
        addGraph();
    }
    else
    {
        modifyGraph();
    }

    return (void *)0;
}

void addGraph()
{
    // Add graph and send output through message queue
}

void modifyGraph()
{
}
