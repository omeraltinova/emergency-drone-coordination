// controller.c
#include "headers/globals.h"
#include "headers/map.h"
#include "headers/drone.h"
#include "headers/survivor.h"
#include "headers/ai.h"
#include "headers/list.h"
#include "headers/view.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

List *survivors = NULL, *helpedsurvivors = NULL, *drones = NULL;
volatile sig_atomic_t running = 1;

// Make thread IDs global to controller.c so cleanup_and_exit can access them
pthread_t survivor_thread_id = 0;
pthread_t ai_thread_id = 0;

static bool cleanup_has_run = false; // Guard for cleanup logic

void cleanup_and_exit() {
    if (cleanup_has_run) { // Check if cleanup has already been initiated
        return;
    }
    cleanup_has_run = true; // Set the flag
    
    running = 0; // Signal all threads to stop
    printf("Cleanup: Signaled threads to stop & initiated cleanup sequence.\n"); // Modified message

    // Join AI thread
    if (ai_thread_id != 0) {
        printf("Cleanup: Joining AI thread...\n");
        pthread_join(ai_thread_id, NULL);
        printf("Cleanup: AI thread joined.\n");
    }

    // Join Survivor Generator thread
    if (survivor_thread_id != 0) {
        printf("Cleanup: Joining Survivor Generator thread...\n");
        pthread_join(survivor_thread_id, NULL);
        printf("Cleanup: Survivor Generator thread joined.\n");
    }

    printf("Cleaning up resources...\n");
    if (survivors) survivors->destroy(survivors);
    if (helpedsurvivors) helpedsurvivors->destroy(helpedsurvivors);
    if (drones) drones->destroy(drones);
    freemap();
    quit_all();
}

void signal_handler(int signum) {
    printf("\nReceived signal %d, cleaning up...\n", signum);
    cleanup_and_exit(); // This will now execute fully once
    printf("Exiting due to signal.\n");
    fflush(stdout); // Flush output before _exit
    _exit(1); // Use _exit for signal handlers
}

int main() {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Starting Drone Simulator...\n");
    
    // Initialize global lists with error checking
    survivors = create_list(sizeof(Survivor), 1000);
    if (!survivors) {
        fprintf(stderr, "Failed to create survivors list\n");
        cleanup_and_exit();
        return 1;
    }
    
    helpedsurvivors = create_list(sizeof(Survivor), 1000);
    if (!helpedsurvivors) {
        fprintf(stderr, "Failed to create helped survivors list\n");
        cleanup_and_exit();
        return 1;
    }
    
    drones = create_list(sizeof(Drone*), 100);
    if (!drones) {
        fprintf(stderr, "Failed to create drones list\n");
        cleanup_and_exit();
        return 1;
    }
    
    printf("Lists created successfully\n");

    // Initialize map
    printf("Initializing map...\n");
    init_map(40, 30);
    printf("Map initialization complete\n");

    // Initialize drones (spawn threads)
    printf("Initializing drones...\n");
    if (initialize_drones() != 0) { // initialize_drones itself handles cleanup of drone threads
        fprintf(stderr, "Failed to initialize drones\n");
        cleanup_and_exit(); // This might be problematic if called before threads are stored
        return 1;
    }
    printf("Drone initialization complete\n");

    // Start survivor generator thread
    // pthread_t survivor_thread; // Use global survivor_thread_id
    printf("Starting survivor generator thread...\n");
    int result = pthread_create(&survivor_thread_id, NULL, survivor_generator, NULL);
    if (result != 0) {
        fprintf(stderr, "Failed to create survivor generator thread: %s\n", strerror(result));
        cleanup_and_exit();
        return 1;
    }
    printf("Survivor generator thread started\n");

    // Start AI controller thread
    // pthread_t ai_thread; // Use global ai_thread_id
    printf("Starting AI controller thread...\n");
    result = pthread_create(&ai_thread_id, NULL, ai_controller, NULL);
    if (result != 0) {
        fprintf(stderr, "Failed to create AI controller thread: %s\n", strerror(result));
        cleanup_and_exit();
        return 1;
    }
    printf("AI controller thread started\n");

    // Initialize SDL
    printf("Initializing SDL...\n");
    if (init_sdl_window() != 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        cleanup_and_exit();
        return 1;
    }
    printf("SDL initialized successfully\n");

    // Main loop
    printf("Entering main loop...\n");
    while (running) {
        // Process events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
                break;
            }
        }

        // Update display
        if (draw_map() != 0) {
            fprintf(stderr, "Error drawing map\n");
            break;
        }
        
        SDL_Delay(100); // Cap at ~10 FPS
    }
    
    printf("Main loop exited. Initiating cleanup...\n");
    cleanup_and_exit();
    printf("Program finished cleanly.\n"); // Changed from "Cleanup complete"
    fflush(stdout); // Ensure this is flushed before exiting
    return 0;
}