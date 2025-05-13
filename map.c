#include "headers/map.h"
#include "headers/list.h"
#include <stdlib.h>
#include <stdio.h>

// Global map instance (defined here, declared extern in map.h)
Map map;

void init_map(int height, int width) {
    printf("Initializing map with dimensions %dx%d...\n", height, width);
    map.height = height;
    map.width = width;

    // Allocate rows (height)
    map.cells = (MapCell**)malloc(sizeof(MapCell*) * height);
    if (!map.cells) {
        perror("Failed to allocate map rows");
        exit(EXIT_FAILURE);
    }

    // Initialize all pointers to NULL for proper cleanup on failure
    for (int i = 0; i < height; i++) {
        map.cells[i] = NULL;
    }

    // Allocate columns (width) for each row
    for (int i = 0; i < height; i++) {
        map.cells[i] = (MapCell*)malloc(sizeof(MapCell) * width);
        if (!map.cells[i]) {
            perror("Failed to allocate map columns");
            freemap();  // This will handle partially allocated memory
            exit(EXIT_FAILURE);
        }

        // Initialize each cell
        for (int j = 0; j < width; j++) {
            map.cells[i][j].coord.x = i;
            map.cells[i][j].coord.y = j;
            // Create a survivor list for this cell (capacity 10)
            map.cells[i][j].survivors = create_list(sizeof(Survivor*), 10);
            if (!map.cells[i][j].survivors) {
                fprintf(stderr, "Failed to create survivor list for cell [%d][%d]\n", i, j);
                freemap();
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("Map initialized successfully: %dx%d\n", height, width);
}

void freemap() {
    if (!map.cells) {
        return;
    }

    for (int i = 0; i < map.height; i++) {
        if (map.cells[i]) {
            for (int j = 0; j < map.width; j++) {
                if (map.cells[i][j].survivors) {
                    map.cells[i][j].survivors->destroy(map.cells[i][j].survivors);
                }
            }
            free(map.cells[i]);
        }
    }
    free(map.cells);
    map.cells = NULL;
    printf("Map destroyed successfully\n");
}