#ifndef PRIMARY_SERVER_H
#define PRIMARY_SERVER_H

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils.h"

static void *threadFunc(void *arg);
void addGraph(struct ShmSeg *shmp, struct MessageBuffer msg,
              int messageQueueID);
void modifyGraph(struct ShmSeg *shmp, struct MessageBuffer msg,
                 int messageQueueID);
void writeToFile(struct MessageBuffer msg, struct ShmSeg *shmp);
int extractNumber(char *filename);

#endif
