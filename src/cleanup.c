#include "../include/cleanup.h"

int main()
{
    char input = 'N';
    while (input != 'Y' && input != 'y') {
            if (input != 'N' && input != 'n') {
            printf("Invalid input\n");
        }
        printf(
            "Do you want the server to terminate? Press Y for Yes and N for No.\n");
        scanf(" %c", &input);
    }

    int messageQueueID;
    key_t messageQueueKey;

    if ((messageQueueKey = ftok(PATHNAME, PROJ_ID)) == -1) {
        perror("Error generating key in ftok");
        exit(1);
    }

    if ((messageQueueID = msgget(messageQueueKey, PERMS)) == -1) {
        perror("Error connecting to message queue in msgget. Is the server on?");
        exit(1);
    }

    struct MessageBuffer requestBuffer;

    requestBuffer.mtype = 1;
    requestBuffer.sequenceNumber = -2;

    if (msgsnd(messageQueueID, &requestBuffer,
                sizeof(requestBuffer) - sizeof(requestBuffer.mtype), 0) == -1) {
        perror("Error sending message in msgsnd");
        exit(1);
    }
    printf("Clean up service exiting...\n");

    return 0;
}
