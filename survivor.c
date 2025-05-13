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
        Node* added_node_global = survivors->add(survivors, &s);
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
                map.cells[cell_coord.x][cell_coord.y].survivors, &s);
            if (!added_node_cell) {
                 fprintf(stderr, "Survivor Generator: Failed to add to map cell [%d][%d] survivors list (full?)\n", cell_coord.x, cell_coord.y);
                 // If adding to cell list fails, we might want to remove from global list too, or handle inconsistency.
                 // For now, just note the error. The survivor is still in the global list.
                 // Consider removing from global list if cell add fails and it's critical:
                 // survivors->removedata(survivors, &s); // Potential issue: s might be freed below if global add failed first.
                 // free(s); // s would be freed if global add fails.
                 // A robust solution would be to ensure s is only freed once, or not added globally if cell add is required.
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
    // The list stores Survivor**, so we need to pass the address of the Survivor*
    // However, s itself is a Survivor*. If the list stores Survivor* directly (copied by value/memcpy),
    // then removedata needs to compare with the actual Survivor* value that was stored.
    // Given our new understanding that list stores POINTERS TO Survivor*,
    // and add receives &s (address of Survivor*), removedata should also receive &s.
    // But the parameter 's' to survivor_cleanup IS the Survivor*.
    // This indicates that the list's removedata needs to be passed the actual Survivor*
    // that was stored (which means the list stores Survivor* values, not Survivor** values).
    // Let's re-verify how removedata and add work with datasize = sizeof(Survivor*).
    // add(list, &survivor_ptr) -> memcpy(node->data, &survivor_ptr, sizeof(Survivor*)) -> node->data now holds survivor_ptr's value.
    // removedata(list, &survivor_ptr_to_match) -> memcmp(temp->data, &survivor_ptr_to_match, sizeof(Survivor*)). This is correct.
    // So, if map cell list stores Survivor*, and s is Survivor*, we pass &s.

    // If list stores Survivor* (meaning datasize is sizeof(Survivor*) and add got &s)
    // then map.cells[...]->removedata needs the address of the Survivor* to match.
    // 's' is the Survivor*. The list contains the *value* of pointers like 's'.
    // So we need to find where this 's' value is stored in the list.
    // removedata(list, data_to_compare_against_stored_pointer_values)
    // removedata will compare data_to_compare_against_stored_pointer_values with each node->data.
    // If node->data contains s (the pointer value), then memcmp(node->data, &s, sizeof(Survivor*)) is what we need.
    // So the call should be map.cells[...]->removedata(..., &s) if s is the Survivor* to remove.
    
    // This part is tricky because of the nested mutexes and who owns what.
    // The original code locked the list, then called removedata (which also locks).
    // Let's assume removedata handles its own locking.

    // pthread_mutex_lock( // This was removed in previous diffs as removedata should be self-locking
    //     &map.cells[s->coord.x][s->coord.y].survivors->lock);
    map.cells[s->coord.x][s->coord.y].survivors->removedata(
        map.cells[s->coord.x][s->coord.y].survivors, &s); // Pass address of s for comparison
    // pthread_mutex_unlock(
    //     &map.cells[s->coord.x][s->coord.y].survivors->lock);

    free(s); // Finally, free the survivor instance itself.
}