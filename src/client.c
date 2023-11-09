#include "../include/client.h"

int main()
{
    int messageQueueID;
    key_t messageQueueKey;

    if ((messageQueueKey = ftok(PATHNAME, PROJ_ID)) == -1)
    {
        perror("Error generating key in ftok");
        exit(1);
    }

    if ((messageQueueID = msgget(messageQueueKey, PERMS)) == -1)
    {
        perror(
            "Error connecting to message queue in msgget. Is the load balancer "
            "on?");
        exit(1);
    }

    while (1)
    {
        printf("\n1. Add a new graph to the database\n2. Modify an existing "
               "graph of "
               "the database\n3. Perform DFS on an existing graph of the "
               "database\n4. "
               "Perform BFS on an existing graph of the database\n");

        int sequenceNumber, operationNumber;
        struct MessageBuffer requestBuffer, responseBuffer;
        requestBuffer.mtype = 1;

        printf("Enter Sequence Number: ");
        scanf("%d", &sequenceNumber);
        requestBuffer.sequenceNumber = sequenceNumber;

        printf("Enter Operation Number: ");
        scanf("%d", &operationNumber);
        requestBuffer.operationNumber = operationNumber;

        printf("Enter Graph File Name: ");
        scanf("%s", requestBuffer.graphFileName);

        int shmId;
        struct ShmSeg *shmSegPtr;
        int *shmIntPtr;

        if (operationNumber <= 2)
        {
            shmId = shmget(sequenceNumber, sizeof(struct ShmSeg),
                           IPC_CREAT | PERMS);
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
        }
        else
        {
            shmId = shmget(sequenceNumber, sizeof(int), IPC_CREAT | PERMS);
            if (shmId == -1)
            {
                perror("Error in shmget");
                exit(1);
            }

            shmIntPtr = (int *)shmat(shmId, NULL, 0);
            if (shmIntPtr == (void *)-1)
            {
                perror("Error in shmat");
                exit(1);
            }
        }

        switch (operationNumber)
        {
        case 1: {
            printf("Enter the number of nodes in the graph: ");
            scanf("%d", &(shmSegPtr->nodes));
            // Do error handling here for nodes.
            printf("Enter adjacency matrix, each row on a separate line and "
                   "elements "
                   "of a single row separated by whitespace characters\n");
            getchar();
            for (int i = 0; i < shmSegPtr->nodes; i++)
            {
                // Figure out length of string for input here.
                fgets(shmSegPtr->adjMatrix + (100 * i), 100, stdin);
            }
        }
        break;
        case 2: {
            printf("Enter the number of nodes in the graph: ");
            scanf("%d", &(shmSegPtr->nodes));
            printf("Enter adjacency matrix, each row on a separate line and "
                   "elements "
                   "of a single row separated by whitespace characters\n");
            getchar();
            for (int i = 0; i < shmSegPtr->nodes; i++)
            {
                // Figure out length of string for input here.
                fgets(shmSegPtr->adjMatrix + (100 * i), 100, stdin);
            }
        }
        break;
        case 3: {
            printf("Enter starting vertex: ");
            scanf("%d", &(shmIntPtr[0]));
        }
        break;
        case 4: {
            printf("Enter starting vertex: ");
            scanf("%d", &(shmIntPtr[0]));
        }
        break;
        default:
            printf("Invalid operation number\n");
            continue;
        }

        if (msgsnd(messageQueueID, &requestBuffer,
                   sizeof(requestBuffer) - sizeof(requestBuffer.mtype),
                   0) == -1)
        {
            perror("Error sending message in msgsnd");
            if (shmctl(shmId, IPC_RMID, 0) == -1)
            {
                perror("shmctl");
                exit(1);
            }
            exit(1);
        }

        if (msgrcv(messageQueueID, &responseBuffer,
                   sizeof(responseBuffer) - sizeof(responseBuffer.mtype),
                   sequenceNumber + 10, 0) == -1)
        {
            perror("Error receiving message in msgrcv");
            if (shmctl(shmId, IPC_RMID, 0) == -1)
            {
                perror("shmctl");
                exit(1);
            }
            exit(1);
        }

        printf("%s\n", responseBuffer.response);

        // Terminate Shared Memory instance.
        if (shmctl(shmId, IPC_RMID, 0) == -1)
        {
            perror("shmctl");
            exit(1);
        }
    }
    return 0;
}
