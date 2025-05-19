#ifndef SERVER_H
#define SERVER_H
#include <pthread.h>
#include "list.h"
#include "drone.h"
#include "survivor.h"

#define SERVER_PORT 5000
#define MAX_CLIENTS 32

extern pthread_mutex_t drones_mutex;
extern pthread_mutex_t survivors_mutex;
extern List *drones;
extern List *survivors;

void* client_handler(void* arg);

#endif // SERVER_H
