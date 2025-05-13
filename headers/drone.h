#ifndef DRONE_H
#define DRONE_H

#include <stdbool.h> // For bool, true, false
#include "coord.h"
#include <time.h>
#include <pthread.h>
#include "list.h"

typedef enum {
    IDLE,
    ON_MISSION,
    DISCONNECTED
} DroneStatus;


typedef struct drone {
    int id;
    pthread_t thread_id;
    int status;             // IDLE, ON_MISSION, DISCONNECTED
    Coord coord;
    Coord target;
    struct tm last_update;
    pthread_mutex_t lock;   // Per-drone mutex
    pthread_cond_t mission_cv;  // Condition variable for new missions
    volatile bool lock_initialized;    // Flag to track if mutex is initialized
    volatile bool cv_initialized;      // Flag to track if condition variable is initialized
} Drone;

// Global drone list (extern)
extern List *drones;
extern Drone *drone_fleet; // Array of drones
extern int num_drones;    // Number of drones in the fleet
// Functions
int initialize_drones();  // Returns 0 on success, -1 on failure
void cleanup_drones();    // Cleanup function for drone fleet
void* drone_behavior(void *arg);

#endif