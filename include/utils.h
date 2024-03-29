#ifndef UTILS_H
#define UTILS_H

#define TEST_CONST 45

#define PERMS 0644
#define PATHNAME "./src/load_balancer.c"
#define PROJ_ID 'C'
#define FILE_NAME_SIZE 20
#define MATRIX_SIZE 2000
#define RESPONSE_SIZE 100
#define READ_COUNT_SEMAPHORE_NAME "/RCOUNT_%d"
#define WRITE_SEMAPHORE_FORMAT "/W_G%d"

struct MessageBuffer
{
    long mtype;
    int sequenceNumber;
    int operationNumber;
    char graphFileName[FILE_NAME_SIZE];
    char response[RESPONSE_SIZE];
};

struct ShmSeg
{
    int cnt; // Number of bytes written to adjMatrix
    int nodes;
    char adjMatrix[MATRIX_SIZE];
};

struct ThreadArgs
{
    int messageQueueID;
    struct MessageBuffer messageBuffer;
};

#endif
