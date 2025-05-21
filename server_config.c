#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers/server.h"
#include "headers/globals.h"

#define DEFAULT_MAP_WIDTH 20
#define DEFAULT_MAP_HEIGHT 20
#define DEFAULT_MAX_DRONES 10
#define DEFAULT_DRONE_SPEED 1
#define DEFAULT_SURVIVOR_SPAWN_RATE 5
#define DEFAULT_PORT 2100

void print_server_banner() {
    printf("\033[1;36m"); // Bright cyan color
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║     Emergency Drone Coordination System Server      ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("\033[0m"); // Reset color
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

ServerConfig get_server_config() {
    ServerConfig config = {
        .map_width = DEFAULT_MAP_WIDTH,
        .map_height = DEFAULT_MAP_HEIGHT,
        .max_drones = DEFAULT_MAX_DRONES,
        .drone_speed = DEFAULT_DRONE_SPEED,
        .survivor_spawn_rate = DEFAULT_SURVIVOR_SPAWN_RATE,
        .port = DEFAULT_PORT
    };

    while (1) {
        print_config_menu();
        
        char choice[10];
        if (fgets(choice, sizeof(choice), stdin) == NULL) {
            continue;
        }

        switch (atoi(choice)) {
            case 1:
                config.map_width = get_integer_input("Enter map width", 10, 100, DEFAULT_MAP_WIDTH);
                break;
            case 2:
                config.map_height = get_integer_input("Enter map height", 10, 100, DEFAULT_MAP_HEIGHT);
                break;
            case 3:
                config.max_drones = get_integer_input("Enter maximum number of drones", 1, 50, DEFAULT_MAX_DRONES);
                break;
            case 4:
                config.drone_speed = get_integer_input("Enter drone speed", 1, 10, DEFAULT_DRONE_SPEED);
                break;
            case 5:
                config.survivor_spawn_rate = get_integer_input("Enter survivor spawn rate (seconds)", 1, 30, DEFAULT_SURVIVOR_SPAWN_RATE);
                break;
            case 6:
                config.port = get_integer_input("Enter server port", 1024, 65535, DEFAULT_PORT);
                break;
            case 7:
                printf("\n\033[1;32mStarting server with current configuration...\033[0m\n");
                return config;
            default:
                printf("\033[1;31mInvalid choice. Please try again.\033[0m\n");
        }
    }
}

void apply_server_config(ServerConfig config) {
    // Print final configuration
    printf("\n\033[1;36mServer Configuration:\033[0m\n");
    printf("Map Size: %dx%d\n", config.map_width, config.map_height);
    printf("Maximum Drones: %d\n", config.max_drones);
    printf("Drone Speed: %d\n", config.drone_speed);
    printf("Survivor Spawn Rate: %d seconds\n", config.survivor_spawn_rate);
    printf("Server Port: %d\n", config.port);
    printf("\n");
} 