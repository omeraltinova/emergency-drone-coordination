#include "headers/ai.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "headers/globals.h"

void assign_mission(Drone *drone, Coord target) {
    if (!drone) {
        fprintf(stderr, "assign_mission: Attempted to assign mission to a NULL drone.\n");
        return;
    }
    if (!drone->lock_initialized) { // Linter might complain, but drone.h via ai.h should provide this
        fprintf(stderr, "assign_mission: Drone %d lock not initialized. Cannot assign mission.\n", drone->id);
        return;
    }
    pthread_mutex_lock(&drone->lock);
    drone->target = target;
    drone->status = ON_MISSION;
    // Signal the drone that it has a new mission
    pthread_cond_signal(&drone->mission_cv);
    pthread_mutex_unlock(&drone->lock);
}

Drone *find_closest_idle_drone(Coord target) {
    Drone *closest = NULL;
    int min_distance = INT_MAX;
    
    pthread_mutex_lock(&drones->lock);  // List mutex
    Node *node = drones->head;
    while (node != NULL) {
        Drone* d = *(Drone**)node->data;    // NEW: node->data CONTAINS Drone*, so dereference a Drone**
        
        if (!d) { 
            node = node->next;
            continue;
        }

        if (!d->lock_initialized) { // Linter might complain here too
            node = node->next;
            continue;
        }
        pthread_mutex_lock(&d->lock);
        if (d->status == IDLE) {
            int dist = abs(d->coord.x - target.x) +
                      abs(d->coord.y - target.y);
            if (dist < min_distance) {
                min_distance = dist;
                closest = d; // d is Drone*, which is correct for 'closest'
            }
        }
        pthread_mutex_unlock(&d->lock); // Unlock INDIVIDUAL drone's lock
        node = node->next;
    }
    pthread_mutex_unlock(&drones->lock);
    return closest;
}

void *ai_controller(void *arg) {
    printf("AI controller started\n");
    
    while (running) {
        Survivor *s = NULL;
        void* peek_result = survivors->peek(survivors);

        if (peek_result != NULL) {
            s = *(Survivor**)peek_result;
            
            if (!s) {
                fprintf(stderr, "AI: Peek returned non-NULL but dereferenced Survivor* is NULL.\n");
                sleep(1);
                continue;
            }

            Coord survivor_coord = s->coord;
            printf("AI: Found survivor at (%d,%d)\n", survivor_coord.x, survivor_coord.y);

            Drone *closest = find_closest_idle_drone(survivor_coord);
            
            if (closest != NULL) {
                printf("AI: Found closest drone %d for survivor at (%d,%d)\n", 
                       closest->id, survivor_coord.x, survivor_coord.y);
                
                if (survivors->removedata(survivors, peek_result) == 0) {
                     printf("AI: Successfully removed survivor from waiting list.\n");
                } else {
                    printf("AI: Failed to remove survivor from waiting list (already removed or peek inconsistent?).\n");
                    sleep(1);
                    continue;
                }
                
                assign_mission(closest, survivor_coord);
                printf("AI: Assigned drone %d to survivor at (%d, %d)\n",
                       closest->id, survivor_coord.x, survivor_coord.y);

                s->status = 1;
                s->helped_time = s->discovery_time;

                helpedsurvivors->add(helpedsurvivors, &s);

                printf("AI: Survivor %s being helped by Drone %d\n",
                       s->info, closest->id);
            } else {
                printf("AI: No available drones for survivor at (%d,%d)\n",
                       survivor_coord.x, survivor_coord.y);
            }
        }
        
        if (!running) break;
        sleep(1);
    }
    printf("AI controller exiting.\n");
    fflush(stdout);
    return NULL;
}