#include "../include/primary_server.h"

sem_t *writeSemaphores[20];

int main()
{
    printf("Initializing primary server...\n");

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
            // Cleanup
            for (int i = 0; i < t; i++)
            {
                if (pthread_join(threads[i], NULL))
                {
                    perror("pthread_join");
                    exit(1);
                }
            }
            //is there anything else to do here?
            printf("Terminating Primary Server...\n");
            exit(0);
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
        addGraph(shmSegPtr, threadArgs->messageBuffer,
                 threadArgs->messageQueueID);
    }
    else
    {
        modifyGraph(shmSegPtr, threadArgs->messageBuffer,
                    threadArgs->messageQueueID);
    }

    pthread_exit(NULL);
}

void addGraph(struct ShmSeg *shmp, struct MessageBuffer msg, int messageQueueID)
{
    struct MessageBuffer responseBuffer;
    responseBuffer.mtype = msg.sequenceNumber + 10;
    responseBuffer.sequenceNumber = msg.sequenceNumber;
    responseBuffer.operationNumber = msg.operationNumber;

    writeToFile(msg, shmp);

    sprintf(responseBuffer.response, "File successfully added");

    if (msgsnd(messageQueueID, &responseBuffer,
               sizeof(responseBuffer) - sizeof(responseBuffer.mtype), 0) == -1)
    {
        perror("Error sending message in msgsnd");
        exit(1);
    }
}

void modifyGraph(struct ShmSeg *shmp, struct MessageBuffer msg,
                 int messageQueueID)
{
    struct MessageBuffer responseBuffer;
    responseBuffer.mtype = msg.sequenceNumber + 10;
    responseBuffer.sequenceNumber = msg.sequenceNumber;
    responseBuffer.operationNumber = msg.operationNumber;

    writeToFile(msg, shmp);

    sprintf(responseBuffer.response, "File successfully modified");

    if (msgsnd(messageQueueID, &responseBuffer,
               sizeof(responseBuffer) - sizeof(responseBuffer.mtype), 0) == -1)
    {
        perror("Error sending message in msgsnd");
        exit(1);
    }
}

void writeToFile(struct MessageBuffer msg, struct ShmSeg *shmp)
{
    int n = extractNumber(msg.graphFileName);
    printf("Waiting for file %s to write\n", msg.graphFileName);
    sem_wait(writeSemaphores[n - 1]);

    FILE *fptr = fopen(msg.graphFileName, "w");
    if (fptr == NULL)
    {
        perror("Error opening file");
        exit(1);
    }

    printf("Writing to file %s\n", msg.graphFileName);

    char nodes[3];
    sprintf(nodes, "%d\n", shmp->nodes);
    fputs(nodes, fptr);
    for (int i = 0; i < shmp->nodes; i++)
    {
        fputs(shmp->adjMatrix + (100 * i), fptr);
    }
    fclose(fptr);
    sem_post(writeSemaphores[n - 1]);
}

int extractNumber(char *filename)
{
    int number;
    sscanf(filename, "G%d.txt", &number);
    return number;
}
