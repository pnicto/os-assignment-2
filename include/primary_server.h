#ifndef PRIMARY_SERVER_H
#define PRIMARY_SERVER_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

static void *threadFunc(void *arg);
void addGraph(struct ShmSeg *shmp, struct MessageBuffer msg,
              int messageQueueID);
void modifyGraph();

#endif
