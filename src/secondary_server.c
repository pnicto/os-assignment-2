#include "../include/secondary_server.h"
#include "../include/utils.h"

int main(int argc, char *argv[]) { 
  printf("Initializing secondary server...\n");
  
  int messageQueueID;
  key_t messageQueueKey;

  if ((messageQueueKey = ftok(PATHNAME, PROJ_ID)) == -1)
  {
    perror("Error generating key in ftok");
    exit(1);
  }

  if ((messageQueueID = msgget(messageQueueKey, PERMS)) == -1)
  {
    perror("Error connecting to message queue in msgget. Is the load balancer on?");
    exit(1);
  }

  (void) argc;  // to get rid of unused parameter warning
  int serverID = atoi(argv[1]);
  printf("Secondary Server %d initialized. Listening for requests.\n", serverID);

  pthread_t threads[100];
  int t = 0;

  while (1)
  {
    struct MessageBuffer messageBuffer;
    if (msgrcv(messageQueueID, &messageBuffer,
                sizeof(messageBuffer) - sizeof(messageBuffer.mtype), serverID + 2,
                0) == -1)
    {
      perror("Error receiving message in msgrcv");
      exit(1);
    }

    if (messageBuffer.sequenceNumber == -1){
      // Perform cleanup, join threads etc...
    }

    if (pthread_create(&threads[t], NULL, threadFunc, &messageBuffer)){
      perror("pthread_create");
      exit(1);
    }

    t++;

    
  }



  return 0;
}

void *threadFunc (void *arg){
  struct MessageBuffer *msg = (struct MessageBuffer *) arg;
  int shmid;
  int *shmp;    // Shared memory only stores vertex number
    
  shmid = shmget(msg->sequenceNumber, sizeof(struct shmseg), PERMS);
  if (shmid == -1)
    perror("shmget");
  
  shmp = shmat(shmid, NULL, 0);
  if (shmp == (void *) -1)
    perror("shmat");
  
  if (msg->operationNumber == 3){
    dfs();
  }
  else{
    bfs();
  }

  return (void *) 0;
}

void bfs () {
  // perform and send output through message queue
}

void dfs () {

}
