#include "headers/drone.h"
#include "headers/globals.h"
#include <stdbool.h> // For true/false
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Global drone fleet
Drone *drone_fleet = NULL;
int num_drones = 10; // Default fleet size
//int num_drones = 2; // Default fleet size

int initialize_drones() {
    printf("initialize_drones: START\n");
    printf("Allocating memory for %d drones...\n", num_drones);
    drone_fleet = malloc(sizeof(Drone) * num_drones);
    if (!drone_fleet) {
        fprintf(stderr, "Failed to allocate memory for drone fleet\n");
        return -1;
    }
    srand(time(NULL));

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
        drone_fleet[i].id = i;
        drone_fleet[i].status = IDLE;
        drone_fleet[i].thread_id = 0;
        drone_fleet[i].lock_initialized = false; // Initialize flag
        drone_fleet[i].cv_initialized = false;   // Initialize flag
        
        printf("Initializing drone %d...\n", i);
        
        drone_fleet[i].coord.x = rand() % map.height;
        drone_fleet[i].coord.y = rand() % map.width;
        
        if (drone_fleet[i].coord.x >= map.height || drone_fleet[i].coord.y >= map.width) {
            fprintf(stderr, "Invalid coordinates for drone %d: (%d,%d)\n",
                    i, drone_fleet[i].coord.x, drone_fleet[i].coord.y);
            init_failed = 1;
            break; 
        }
        printf("Drone %d initial position: (%d,%d)\n", 
               i, drone_fleet[i].coord.x, drone_fleet[i].coord.y);
        
        drone_fleet[i].target = drone_fleet[i].coord;
        
        printf("Initializing mutex for drone %d...\n", i);
        if (pthread_mutex_init(&drone_fleet[i].lock, NULL) != 0) {
            fprintf(stderr, "Failed to initialize mutex for drone %d\n", i);
            init_failed = 1;
            break; 
        }
        drone_fleet[i].lock_initialized = true; // Set flag on success
        
        printf("Initializing condition variable for drone %d...\n", i);
        if (pthread_cond_init(&drone_fleet[i].mission_cv, NULL) != 0) {
            fprintf(stderr, "Failed to initialize condition variable for drone %d\n", i);
            if (drone_fleet[i].lock_initialized) { // Cleanup previously initialized mutex
                pthread_mutex_destroy(&drone_fleet[i].lock);
                drone_fleet[i].lock_initialized = false;
            }
            init_failed = 1;
            break;
        }
        drone_fleet[i].cv_initialized = true; // Set flag on success
        
        Drone* drone_ptr = &drone_fleet[i]; 
        printf("Adding drone %d to global list...\n", i);
        Node *added_node = drones->add(drones, &drone_ptr);    // Pass address of drone_ptr
        if (!added_node) {
            fprintf(stderr, "Failed to add drone %d to global list - list might be full\n", i);
            if (drone_fleet[i].cv_initialized) { // Cleanup CV
                pthread_cond_destroy(&drone_fleet[i].mission_cv);
                drone_fleet[i].cv_initialized = false;
            }
            if (drone_fleet[i].lock_initialized) { // Cleanup mutex
                pthread_mutex_destroy(&drone_fleet[i].lock);
                drone_fleet[i].lock_initialized = false;
            }
            init_failed = 1;
            break;
        }
        printf("Successfully added drone %d to global list\n", i);
        
        printf("Creating thread for drone %d...\n", i);
        int thread_result = pthread_create(&drone_fleet[i].thread_id, &attr, drone_behavior, &drone_fleet[i]);
        if (thread_result != 0) {
            fprintf(stderr, "Failed to create thread for drone %d: %s\n", i, strerror(thread_result));
            drones->removedata(drones, &drone_ptr);    // Pass address of drone_ptr for removal comparison
            if (drone_fleet[i].cv_initialized) { // Cleanup CV
                pthread_cond_destroy(&drone_fleet[i].mission_cv);
                drone_fleet[i].cv_initialized = false;
            }
            if (drone_fleet[i].lock_initialized) { // Cleanup mutex
                pthread_mutex_destroy(&drone_fleet[i].lock);
                drone_fleet[i].lock_initialized = false;
            }
            init_failed = 1;
            break;
        }
        printf("Drone %d thread created successfully (thread_id: %lu)\n", i, (unsigned long)drone_fleet[i].thread_id);
        usleep(50000);
    }

    pthread_attr_destroy(&attr);

    if (init_failed) {
        fprintf(stderr, "Drone initialization failed overall. Cleaning up partially initialized drones...\n");
        // The loop already cleans up the drone that caused the failure.
        // We might need to iterate 'i' drones that were fully or partially set up before failure.
        // For simplicity here, the break exits, and controller's cleanup_and_exit will call cleanup_drones.
        // A more robust internal cleanup here would loop from 0 to i-1 (or num_drones if failure was outside loop)
        // and destroy initialized resources. However, cleanup_drones should handle this if called.
        // The main issue is to ensure cleanup_drones correctly uses the flags.
        return -1; 
    }
    
    printf("All drones initialized successfully\n");
    printf("initialize_drones: END (success)\n");
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
    printf("cleanup_drones: START\n");
    if (!drone_fleet) {
        printf("cleanup_drones: drone_fleet is NULL, exiting.\n");
        return;
    }
    
    for(int i = 0; i < num_drones; i++) {
        if (drone_fleet[i].thread_id != 0) {
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
            drone_fleet[i].thread_id = 0; // Mark thread as joined/handled
        } else {
            printf("cleanup_drones: Drone %d has no valid thread_id to cancel/join or was already handled.\n", i);
        }
        
        if (drone_fleet[i].cv_initialized) {
            printf("cleanup_drones: Destroying CV for drone %d\n", i);
            pthread_cond_destroy(&drone_fleet[i].mission_cv);
            drone_fleet[i].cv_initialized = false; 
        }
        if (drone_fleet[i].lock_initialized) {
            printf("cleanup_drones: Destroying mutex for drone %d\n", i);
            pthread_mutex_destroy(&drone_fleet[i].lock);
            drone_fleet[i].lock_initialized = false; 
        }

        Drone* drone_to_remove = &drone_fleet[i];
        if (drones) { // Check if global drones list exists
             // We attempt removal, but the drone might have already been removed
             // or failed to be added. removedata should handle "not found" gracefully.
            if (drones->removedata(drones, &drone_to_remove) == 0) {    // Pass address of drone_to_remove (Drone*)
                printf("cleanup_drones: Drone %d successfully removed from global list.\n", i);
            } else {
                // This could also mean drone was not in the list to begin with if init failed early
                printf("cleanup_drones: Drone %d not found in global list or failed to remove.\n", i);
            }
        }
    }
    
    free(drone_fleet);
    drone_fleet = NULL;
    printf("Drone fleet cleanup complete\n");
    printf("cleanup_drones: END\n");
}