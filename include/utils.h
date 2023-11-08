#ifndef UTILS_H
#define UTILS_H

#define TEST_CONST 45

#define PERMS 0644
#define PATHNAME "./src/load_balancer.c"
#define PROJ_ID 'C'
#define BUFFER_SIZE 200

struct MessageBuffer {
  long mtype;
  int clientID;
  int sequenceNumber;
  int operationNumber;
  char graphFileName[BUFFER_SIZE];
};

extern int serverCreationStatus;

#endif
