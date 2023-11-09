#ifndef SECONDARY_SERVER_H
#define SECONDARY_SERVER_H

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
void bfs();
void dfs();

#endif
