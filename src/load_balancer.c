#include "../include/load_balancer.h"

int main()
{
    printf("Initializing load balancer...\n");

    int messageQueueID;
    key_t messageQueueKey;

    if ((messageQueueKey = ftok(PATHNAME, PROJ_ID)) == -1)
    {
        perror("Error generating key in ftok");
        exit(1);
    }

    if ((messageQueueID = msgget(messageQueueKey, PERMS | IPC_CREAT)) == -1)
    {
        perror("Error creating message queue in msgget");
        exit(1);
    }

    // create named semaphores
    sem_t *readSemaphores[20];
    sem_t *writeSemaphores[20];
    char filename[FILE_NAME_SIZE];

    for (int i = 1; i <= 20; i++)
    {
        snprintf(filename, FILE_NAME_SIZE, READ_SEMAPHORE_FORMAT, i);
        readSemaphores[i - 1] = sem_open(filename, O_CREAT | O_EXCL, 0644, 1);
        if (readSemaphores[i - 1] == SEM_FAILED)
        {
            perror("Error initializing read semaphore in sem_open");
            exit(1);
        }
        if (sem_close(readSemaphores[i - 1]) == -1)
        {
            perror("Error closing read semaphore in sem_close");
            exit(1);
        }

        snprintf(filename, FILE_NAME_SIZE, WRITE_SEMAPHORE_FORMAT, i);
        writeSemaphores[i - 1] = sem_open(filename, O_CREAT | O_EXCL, 0644, 1);
        if (writeSemaphores[i - 1] == SEM_FAILED)
        {
            perror("Error initializing write semaphore in sem_open");
            exit(1);
        }
        if (sem_close(writeSemaphores[i - 1]) == -1)
        {
            perror("Error closing write semaphore in sem_close");
            exit(1);
        }
    }

    printf("Load balancer initialized. Listening for requests.\n");

    while (1)
    {
        struct MessageBuffer messageBuffer;
        if (msgrcv(messageQueueID, &messageBuffer,
                   sizeof(messageBuffer) - sizeof(messageBuffer.mtype), 1,
                   0) == -1)
        {
            perror("Error receiving message in msgrcv");

            exit(1);
        }

        if (messageBuffer.sequenceNumber == -2)
        {
            // cleanup
            struct MessageBuffer requestBuffer;
            requestBuffer.mtype = 2;
            requestBuffer.sequenceNumber = -1;

            if (msgsnd(messageQueueID, &requestBuffer,
                        sizeof(requestBuffer) - sizeof(requestBuffer.mtype), 0) == -1) {
                perror("Error sending message in msgsnd");
                exit(1);
            }
            requestBuffer.mtype = 3;
            requestBuffer.sequenceNumber = -1;

            if (msgsnd(messageQueueID, &requestBuffer,
                        sizeof(requestBuffer) - sizeof(requestBuffer.mtype), 0) == -1) {
                perror("Error sending message in msgsnd");
                exit(1);
            }
            requestBuffer.mtype = 4;
            requestBuffer.sequenceNumber = -1;

            if (msgsnd(messageQueueID, &requestBuffer,
                        sizeof(requestBuffer) - sizeof(requestBuffer.mtype), 0) == -1) {
                perror("Error sending message in msgsnd");
                exit(1);
            }

            sleep(5);
            printf("Removing message queue...\n");
            if (msgctl(messageQueueID, IPC_RMID, NULL) == -1) {
                perror("Removing queue failed");
                exit(1);
            }

            printf("Load Balancer exiting...\n");
            break;

            // named semaphore cleanup
            char filename[FILE_NAME_SIZE];
            for (int i = 1; i <= 20; i++)
            {
                snprintf(filename, FILE_NAME_SIZE, READ_SEMAPHORE_FORMAT, i);
                sem_unlink(filename);
                snprintf(filename, FILE_NAME_SIZE, WRITE_SEMAPHORE_FORMAT, i);
                sem_unlink(filename);
            }
        }

        if (messageBuffer.operationNumber <= 2)
        {
            messageBuffer.mtype = 2;
        }
        else
        {
            if (messageBuffer.sequenceNumber % 2)
                messageBuffer.mtype = 3;
            else
                messageBuffer.mtype = 4;
        }

        if (msgsnd(messageQueueID, &messageBuffer,
                   sizeof(messageBuffer) - sizeof(messageBuffer.mtype),
                   0) == -1)
            perror("Error in sending the message.");
    }
    return 0;
}
