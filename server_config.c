#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers/server.h"
#include "headers/globals.h"
#include "headers/server_config.h"

#define DEFAULT_MAP_WIDTH 20
#define DEFAULT_MAP_HEIGHT 20
#define DEFAULT_MAX_DRONES 64
#define DEFAULT_DRONE_SPEED 1
#define DEFAULT_SURVIVOR_SPAWN_RATE 5
#define DEFAULT_PORT 2100

void print_server_banner(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║           Emergency Drone Coordination System              ║\n");
    printf("║                     Server - Phase 2                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
}

void print_config_menu() {
    printf("\n\033[1;33mServer Configuration Menu:\033[0m\n");
    printf("1. Map Width           [current: %d]\n", DEFAULT_MAP_WIDTH);
    printf("2. Map Height          [current: %d]\n", DEFAULT_MAP_HEIGHT);
    printf("3. Maximum Drones      [current: %d]\n", DEFAULT_MAX_DRONES);
    printf("4. Drone Speed         [current: %d]\n", DEFAULT_DRONE_SPEED);
    printf("5. Survivor Spawn Rate [current: %d seconds]\n", DEFAULT_SURVIVOR_SPAWN_RATE);
    printf("6. Server Port         [current: %d]\n", DEFAULT_PORT);
    printf("7. Start Server with these settings\n");
    printf("\nEnter your choice (1-7): ");
}

int get_integer_input(const char* prompt, int min, int max, int default_value) {
    char input[32];
    int value;

    printf("%s [%d-%d, default=%d]: ", prompt, min, max, default_value);
    if (fgets(input, sizeof(input), stdin) == NULL || input[0] == '\n') {
        return default_value;
    }

    value = atoi(input);
    if (value < min || value > max) {
        printf("\033[1;31mInvalid input. Using default value: %d\033[0m\n", default_value);
        return default_value;
    }

    return value;
}

ServerConfig get_server_config(void) {
    ServerConfig config = {
        .port = DEFAULT_PORT,              // Default port
        .max_drones = DEFAULT_MAX_DRONES,  // Maximum number of drones
        .map_width = DEFAULT_MAP_WIDTH,    // Default map width
        .map_height = DEFAULT_MAP_HEIGHT,  // Default map height
        .survivor_spawn_rate = DEFAULT_SURVIVOR_SPAWN_RATE,  // Spawn rate
        .drone_speed = DEFAULT_DRONE_SPEED // Default drone speed
    };
    return config;
}

void apply_server_config(ServerConfig config) {
    printf("[SERVER] Configuration applied:\n");
    printf("  - Port: %d\n", config.port);
    printf("  - Max Drones: %d\n", config.max_drones);
    printf("  - Map Size: %dx%d\n", config.map_width, config.map_height);
    printf("  - Survivor Spawn Rate: %d seconds\n", config.survivor_spawn_rate);
    printf("  - Drone Speed: %d\n", config.drone_speed);
} 