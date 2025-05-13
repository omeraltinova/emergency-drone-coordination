#include "headers/survivor.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "headers/globals.h"
#include "headers/map.h"

Survivor *create_survivor(Coord *coord, char *info,
                          struct tm *discovery_time) {
    Survivor *s = malloc(sizeof(Survivor));
    if (!s) return NULL;

    memset(s, 0, sizeof(Survivor));
    s->coord = *coord;
    memcpy(&s->discovery_time, discovery_time, sizeof(struct tm));
    strncpy(s->info, info, sizeof(s->info) - 1);
    s->info[sizeof(s->info) - 1] = '\0';  // Ensure null-termination
    s->status = 0;  // Initialize status (e.g., 0 for waiting)
    return s;
}

void *survivor_generator(void *args) {
    (void)args;  // Unused parameter
    time_t t;
    struct tm discovery_time;
    
    // Initialize random seed
    srand(time(NULL));
    printf("Survivor generator started\n");

    while (running) {
        // Generate random survivor
        Coord coord = {.x = rand() % map.height,  // Changed from width to height
                      .y = rand() % map.width};   // Changed from height to width

        char info[25];
        snprintf(info, sizeof(info), "SURV-%04d", rand() % 10000);

        time(&t);
        localtime_r(&t, &discovery_time);

        // Create and add to lists
        Survivor *s = create_survivor(&coord, info, &discovery_time);
        if (!s) continue;

        // Add to global survivor list
        Node* added_node_global = survivors->add(survivors, s);
        if (!added_node_global) {
            fprintf(stderr, "Survivor Generator: Failed to add to global survivors list (full?)\n");
            free(s); // Clean up survivor if not added
            sleep(1); // Avoid busy loop if list is full
            continue;
        }

        // Add to map cell's survivor list
        Coord cell_coord = s->coord; // Use a copy for map cell access, though s->coord is fine for now.
        if (cell_coord.x < map.height && cell_coord.y < map.width) {
            Node* added_node_cell = map.cells[cell_coord.x][cell_coord.y].survivors->add(
                map.cells[cell_coord.x][cell_coord.y].survivors, s);
            if (!added_node_cell) {
                 fprintf(stderr, "Survivor Generator: Failed to add to map cell [%d][%d] survivors list (full?)\n", cell_coord.x, cell_coord.y);
                 // If adding to cell list fails, we might want to remove from global list too, or handle inconsistency.
                 // For now, just note the error. The survivor is still in the global list.
            }
        } else {
            fprintf(stderr, "Survivor Generator: Invalid coordinates (%d,%d) for map cell access.\n", cell_coord.x, cell_coord.y);
        }

        printf("New survivor at (%d,%d): %s\n", s->coord.x, s->coord.y, s->info);
        
        // Check running flag before sleep to allow quicker exit
        if (!running) break;
        sleep(rand() % 3 + 2);  // Generate every 2-5 seconds
    }
    printf("Survivor generator exiting.\n");
    fflush(stdout); // Ensure output is flushed before thread truly exits
    return NULL;
}

void survivor_cleanup(Survivor *s) {
    // Remove from map cell
    pthread_mutex_lock(
        &map.cells[s->coord.x][s->coord.y].survivors->lock);
    map.cells[s->coord.x][s->coord.y].survivors->removedata(
        map.cells[s->coord.x][s->coord.y].survivors, s);
    pthread_mutex_unlock(
        &map.cells[s->coord.x][s->coord.y].survivors->lock);

    free(s);
}