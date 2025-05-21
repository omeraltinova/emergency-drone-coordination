#ifndef SERVER_H
#define SERVER_H
#include <pthread.h>
#include "list.h"
#include "drone.h"
#include "survivor.h"
#include <stdint.h>

#define SERVER_PORT 2100
#define MAX_CLIENTS 32

extern pthread_mutex_t drones_mutex;
extern pthread_mutex_t survivors_mutex;
extern List *drones;
extern List *survivors;

// Server configuration structure
typedef struct {
    int map_width;
    int map_height;
    int max_drones;
    int drone_speed;
    int survivor_spawn_rate;  // in seconds
    int port;
} ServerConfig;

// Function declarations
extern void print_server_banner(void);
extern ServerConfig get_server_config(void);
extern void apply_server_config(ServerConfig config);

void* client_handler(void* arg);

#endif // SERVER_H
