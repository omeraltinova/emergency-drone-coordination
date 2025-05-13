#include "headers/drone.h"
#include "headers/globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Global drone fleet
Drone *drone_fleet = NULL;
int num_drones = 10; // Default fleet size

int initialize_drones() {
    printf("initialize_drones: START\n"); // DEBUG
    printf("Allocating memory for %d drones...\n", num_drones);
    drone_fleet = malloc(sizeof(Drone) * num_drones);
    if (!drone_fleet) {
        fprintf(stderr, "Failed to allocate memory for drone fleet\n");
        return -1;
    }
    srand(time(NULL));

    // Verify global drones list exists
    if (!drones) {
        fprintf(stderr, "Global drones list not initialized\n");
        free(drone_fleet);
        return -1;
    }

    int init_failed = 0;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        fprintf(stderr, "Failed to initialize thread attributes\n");
        free(drone_fleet);
        return -1;
    }
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    for(int i = 0; i < num_drones; i++) {
        printf("Initializing drone %d...\n", i);
        
        drone_fleet[i].id = i;
        drone_fleet[i].status = IDLE;
        drone_fleet[i].thread_id = 0;  // Initialize thread ID to 0
        
        // Generate valid coordinates within map bounds
        drone_fleet[i].coord.x = rand() % map.height;
        drone_fleet[i].coord.y = rand() % map.width;
        
        // Verify the coordinates are valid
        if (drone_fleet[i].coord.x >= map.height || drone_fleet[i].coord.y >= map.width) {
            fprintf(stderr, "Invalid coordinates for drone %d: (%d,%d)\n",
                    i, drone_fleet[i].coord.x, drone_fleet[i].coord.y);
            init_failed = 1;
            break;
        }
        
        printf("Drone %d initial position: (%d,%d)\n", 
               i, drone_fleet[i].coord.x, drone_fleet[i].coord.y);
        
        drone_fleet[i].target = drone_fleet[i].coord;
        
        // Initialize synchronization primitives with error checking
        printf("Initializing mutex for drone %d...\n", i);
        if (pthread_mutex_init(&drone_fleet[i].lock, NULL) != 0) {
            fprintf(stderr, "Failed to initialize mutex for drone %d\n", i);
            init_failed = 1;
            break;
        }
        
        printf("Initializing condition variable for drone %d...\n", i);
        if (pthread_cond_init(&drone_fleet[i].mission_cv, NULL) != 0) {
            fprintf(stderr, "Failed to initialize condition variable for drone %d\n", i);
            pthread_mutex_destroy(&drone_fleet[i].lock);
            init_failed = 1;
            break;
        }
        
        // Add to global drone list with proper locking
        printf("Adding drone %d to global list...\n", i);
        Drone* drone_ptr = &drone_fleet[i]; // drone_ptr IS ALREADY Drone*
        Node *added_node = drones->add(drones, &drone_ptr); // Pass address of drone_ptr for list to copy the pointer value
        if (!added_node) {
            fprintf(stderr, "Failed to add drone %d to global list - list might be full\n", i);
            init_failed = 1;
            break;
        }
        printf("Successfully added drone %d to global list\n", i);
        
        printf("Creating thread for drone %d...\n", i);
        
        // Create thread with error checking
        printf("Calling pthread_create for drone %d...\n", i);
        int thread_result = pthread_create(&drone_fleet[i].thread_id, &attr, drone_behavior, &drone_fleet[i]);
        printf("pthread_create returned %d for drone %d\n", thread_result, i);
        
        if (thread_result != 0) {
            fprintf(stderr, "Failed to create thread for drone %d: %s\n", i, strerror(thread_result));
            // Remove from global list since thread creation failed
            drones->removedata(drones, &drone_fleet[i]);
            init_failed = 1;
            break;
        }
        
        printf("Drone %d thread created successfully (thread_id: %lu)\n", i, (unsigned long)drone_fleet[i].thread_id);
        
        // Small delay between thread creations
        usleep(50000);  // 50ms delay
    }

    pthread_attr_destroy(&attr);

    if (init_failed) {
        fprintf(stderr, "Drone initialization failed, cleaning up...\n");
        cleanup_drones();
        printf("initialize_drones: END (init_failed)\n"); // DEBUG
        return -1;
    }
    
    printf("All drones initialized successfully\n");
    printf("initialize_drones: END (success)\n"); // DEBUG
    return 0;
}

void* drone_behavior(void *arg) {
    Drone *d = (Drone*)arg;
    
    if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0) {
        fprintf(stderr, "Drone %d: Failed to set cancel state\n", d->id);
        return NULL;
    }
    if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL) != 0) {
        fprintf(stderr, "Drone %d: Failed to set cancel type\n", d->id);
        return NULL;
    }
    
    pthread_cleanup_push((void (*)(void*))pthread_mutex_unlock, (void *)&d->lock); // Pass address of mutex
    
    printf("Drone %d: Thread started (thread_id: %lu), entering main loop\n", 
           d->id, (unsigned long)pthread_self());
    
    while(running) {
        int lock_result = pthread_mutex_lock(&d->lock);
        if (lock_result != 0) {
            fprintf(stderr, "Drone %d: Failed to acquire lock: %s\n", d->id, strerror(lock_result));
            break; // Exit while(running) loop
        }
        
        // Inner loop for waiting for a mission when IDLE
        while(d->status == IDLE && running) { 
            printf("Drone %d: Waiting for mission...\n", d->id);
            int wait_result = pthread_cond_wait(&d->mission_cv, &d->lock);
            if (wait_result != 0) {
                fprintf(stderr, "Drone %d: Condition wait failed: %s\n", d->id, strerror(wait_result));
                // Mutex is still held here. cleanup_pop will unlock it.
                goto cleanup_exit_thread; // Use goto for cleanup before exit
            }
            printf("Drone %d: Woke up from wait. Current status: %s\n", 
                   d->id, (d->status == IDLE ? "IDLE" : (d->status == ON_MISSION ? "ON_MISSION" : "DISCONNECTED")));
            // Loop back to check d->status == IDLE and running again
        }

        if (!running) { // If program is stopping, release lock and break outer loop
            pthread_mutex_unlock(&d->lock);
            break;
        }
        
        // If not IDLE (i.e., ON_MISSION after waking or was already ON_MISSION)
        if(d->status == ON_MISSION) {
            // Mutex is currently held
            printf("Drone %d: Moving toward target (%d,%d) from (%d,%d)\n",
                   d->id, d->target.x, d->target.y, d->coord.x, d->coord.y);
            
            if(d->coord.x < d->target.x) d->coord.x++;
            else if(d->coord.x > d->target.x) d->coord.x--;
            
            if(d->coord.y < d->target.y) d->coord.y++;
            else if(d->coord.y > d->target.y) d->coord.y--;

            if(d->coord.x == d->target.x && d->coord.y == d->target.y) {
                d->status = IDLE;
                printf("Drone %d: Mission completed at (%d,%d)!\n", 
                       d->id, d->coord.x, d->coord.y);
                // Status is now IDLE, will loop back to the while(d->status == IDLE && running) check
            }
            pthread_mutex_unlock(&d->lock); // Unlock after processing ON_MISSION state for this step
            
            // If still ON_MISSION (i.e. not yet reached target) and running, then sleep
            if (d->status == ON_MISSION && running) { 
                usleep(500000); 
            }
        } else if (d->status != IDLE) { // Should ideally not happen if only IDLE and ON_MISSION are used by drone itself
             pthread_mutex_unlock(&d->lock); // Ensure lock is released if not IDLE and not ON_MISSION
        }
        // If status became IDLE (either by completing mission or was already IDLE and running is false),
        // the outer loop will re-evaluate. If it was IDLE and running is true, inner wait loop is entered.
        // If lock was not released in ON_MISSION block (e.g. if status became IDLE), it needs to be released.
        // However, the logic above tries to release it within the ON_MISSION block after processing.
        // The case d->status == IDLE is handled at the start of the loop, it waits with lock, then continues.
        // The main point is that the lock should be released ONCE per outer loop iteration if not waiting.

    } // End of while(running)

cleanup_exit_thread:
    pthread_cleanup_pop(1); // Execute cleanup handler (unlocks mutex if held)
    printf("Drone %d: Thread exiting\n", d->id);
    fflush(stdout); // Ensure output is flushed before thread truly exits
    return NULL;
}

void cleanup_drones() {
    printf("cleanup_drones: START\n"); // DEBUG
    if (!drone_fleet) {
        printf("cleanup_drones: drone_fleet is NULL, exiting.\n"); // DEBUG
        return;
    }
    
    for(int i = 0; i < num_drones; i++) {
        // Cancel thread if it exists
        if (drone_fleet[i].thread_id != 0) { // Check if thread_id is valid
            printf("cleanup_drones: Canceling thread for drone %d (tid: %lu)...\n", i, (unsigned long)drone_fleet[i].thread_id);
            pthread_cancel(drone_fleet[i].thread_id);
            void* join_status;
            printf("cleanup_drones: Joining thread for drone %d...\n", i);
            int join_res = pthread_join(drone_fleet[i].thread_id, &join_status);
            if (join_res == 0) {
                if (join_status == PTHREAD_CANCELED) {
                    printf("cleanup_drones: Drone %d thread successfully canceled and joined.\n", i);
                } else {
                    printf("cleanup_drones: Drone %d thread successfully joined (exited normally).\n", i);
                }
            } else {
                fprintf(stderr, "cleanup_drones: Error joining drone %d thread: %s\n", i, strerror(join_res));
            }
        } else {
            printf("cleanup_drones: Drone %d has no valid thread_id to cancel/join.\n", i);
        }
        
        // Destroy synchronization primitives
        pthread_mutex_destroy(&drone_fleet[i].lock);
        pthread_cond_destroy(&drone_fleet[i].mission_cv);
    }
    
    // Free the fleet array
    free(drone_fleet);
    drone_fleet = NULL;
    printf("Drone fleet cleanup complete\n");
    printf("cleanup_drones: END\n"); // DEBUG
}