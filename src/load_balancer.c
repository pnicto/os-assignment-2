#include "../include/load_balancer.h"
#include "../include/utils.h"

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

    printf("Load balancer initialized. Listening for requests.\n");
    
   // int serverCreationStatus = 0;

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

        if (messageBuffer.sequenceNumber == -1){
            // cleanup
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

        msgsnd(messageQueueID, &messageBuffer,
               sizeof(messageBuffer) - sizeof(messageBuffer.mtype), 0)  ;
    }
    return 0;
}
