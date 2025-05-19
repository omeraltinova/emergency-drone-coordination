#include "headers/view.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#include "headers/drone.h"
#include "headers/map.h"
#include "headers/survivor.h"

#define CELL_SIZE 20  // Pixels per map cell

// SDL globals
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Event event;
int window_width, window_height;

// Colors
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {255, 0, 0, 255};         // Waiting survivors
const SDL_Color ORANGE = {255, 165, 0, 255};    // Survivors being helped
const SDL_Color BLUE = {0, 0, 255, 255};        // Idle drones
const SDL_Color GREEN = {0, 255, 0, 255};       // Active drones
const SDL_Color YELLOW = {255, 255, 0, 255};    // Target locations
const SDL_Color WHITE = {255, 255, 255, 255};
const SDL_Color PURPLE = {128, 0, 128, 255};    // Helped survivors

int init_sdl_window() {
    window_width = map.width * CELL_SIZE;
    window_height = map.height * CELL_SIZE;

    printf("Initializing SDL window (%dx%d)\n", window_width, window_height);

    // Initialize SDL with more subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized successfully\n");

    // Set window position to centered
    window = SDL_CreateWindow("Drone Simulator", 
                            SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 
                            window_width,
                            window_height, 
                            SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        fprintf(stderr, "Window dimensions: %dx%d\n", window_width, window_height);
        SDL_Quit();
        return 1;
    }
    printf("Window created successfully with dimensions: %dx%d\n", window_width, window_height);

    // Create renderer with basic flags first
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Failed to create accelerated renderer: %s\n", SDL_GetError());
        printf("Falling back to software renderer...\n");
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) {
            fprintf(stderr, "Software renderer also failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }
    printf("Renderer created successfully\n");

    // Set render draw color to black for clearing
    if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) < 0) {
        fprintf(stderr, "SDL_SetRenderDrawColor Error: %s\n", SDL_GetError());
        return 1;
    }
    
    if (SDL_RenderClear(renderer) < 0) {
        fprintf(stderr, "SDL_RenderClear Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_RenderPresent(renderer);
    printf("Initial render completed\n");

    return 0;
}

void draw_cell(int x, int y, SDL_Color color) {
    SDL_Rect rect = {
        .x = y * CELL_SIZE,
        .y = x * CELL_SIZE,
        .w = CELL_SIZE,
        .h = CELL_SIZE
    };
    
    if (SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a) < 0) {
        fprintf(stderr, "SDL_SetRenderDrawColor Error in draw_cell: %s\n", SDL_GetError());
        return;
    }
    
    if (SDL_RenderFillRect(renderer, &rect) < 0) {
        fprintf(stderr, "SDL_RenderFillRect Error: %s\n", SDL_GetError());
        return;
    }
}

void draw_target_marker(int x, int y) {
    // Draw a cross at the target location
    SDL_SetRenderDrawColor(renderer, YELLOW.r, YELLOW.g, YELLOW.b, YELLOW.a);
    int center_x = y * CELL_SIZE + CELL_SIZE / 2;
    int center_y = x * CELL_SIZE + CELL_SIZE / 2;
    int size = CELL_SIZE / 4;
    
    SDL_RenderDrawLine(renderer, center_x - size, center_y, center_x + size, center_y);
    SDL_RenderDrawLine(renderer, center_x, center_y - size, center_x, center_y + size);
}

void draw_drones() {
    // Yeni: merkezi List *drones yapısını kullan
    extern List *drones;
    if (!drones) return;
    pthread_mutex_lock(&drones->lock);
    Node *node = drones->head;
    while (node) {
        Drone *drone = *(Drone **)node->data;
        if (!drone) { node = node->next; continue; }
        SDL_Color color = (drone->status == IDLE) ? BLUE : GREEN;
        draw_cell(drone->coord.x, drone->coord.y, color);
        draw_target_marker(drone->target.x, drone->target.y);
        node = node->next;
    }
    pthread_mutex_unlock(&drones->lock);
} // draw_drones sonu

void draw_survivors() {
    // Yeni: merkezi List *survivors ve List *helpedsurvivors kullan
    extern List *survivors;
    extern List *helpedsurvivors;
    if (survivors) {
        pthread_mutex_lock(&survivors->lock);
        Node *node = survivors->head;
        while (node) {
            Survivor *s = *(Survivor **)node->data;
            if (!s) { node = node->next; continue; }
            SDL_Color color = (s->status == 0) ? RED : ORANGE;
            draw_cell(s->coord.x, s->coord.y, color);
            node = node->next;
        }
        pthread_mutex_unlock(&survivors->lock);
    }
    if (helpedsurvivors) {
        pthread_mutex_lock(&helpedsurvivors->lock);
        Node *node = helpedsurvivors->head;
        while (node) {
            Survivor *s = *(Survivor **)node->data;
            if (!s) { node = node->next; continue; }
            draw_cell(s->coord.x, s->coord.y, PURPLE);
            node = node->next;
        }
        pthread_mutex_unlock(&helpedsurvivors->lock);
    }
} // draw_survivors sonu


void draw_grid() {
    SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b,
                           WHITE.a);
    for (int i = 0; i <= map.height; i++) {
        SDL_RenderDrawLine(renderer, 0, i * CELL_SIZE, window_width,
                           i * CELL_SIZE);
    }
    for (int j = 0; j <= map.width; j++) {
        SDL_RenderDrawLine(renderer, j * CELL_SIZE, 0, j * CELL_SIZE,
                           window_height);
    }
}

int draw_map() {
    if (!renderer) {
        fprintf(stderr, "Renderer is NULL in draw_map\n");
        return 1;
    }

    if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) < 0) {
        fprintf(stderr, "SDL_SetRenderDrawColor Error in draw_map: %s\n", SDL_GetError());
        return 1;
    }
    
    if (SDL_RenderClear(renderer) < 0) {
        fprintf(stderr, "SDL_RenderClear Error in draw_map: %s\n", SDL_GetError());
        return 1;
    }

    draw_survivors();
    draw_drones();
    draw_grid();

    SDL_RenderPresent(renderer);
    return 0;
}

int check_events() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return 1;
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE)
            return 1;
    }
    return 0;
}

void quit_all() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}