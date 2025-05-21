#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "headers/server.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define SLIDER_WIDTH 300
#define SLIDER_HEIGHT 20
#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 40

typedef struct {
    SDL_Rect rect;
    int min_value;
    int max_value;
    int* value;
    const char* label;
    bool dragging;
} Slider;

typedef struct {
    SDL_Rect rect;
    const char* label;
    bool hover;
} Button;

// UI Colors
SDL_Color BACKGROUND = {45, 45, 45, 255};
SDL_Color SLIDER_BG = {60, 60, 60, 255};
SDL_Color SLIDER_FG = {0, 120, 215, 255};
SDL_Color BUTTON_NORMAL = {0, 120, 215, 255};
SDL_Color BUTTON_HOVER = {0, 140, 245, 255};
SDL_Color TEXT_COLOR = {255, 255, 255, 255};

void draw_slider(SDL_Renderer* renderer, Slider* slider) {
    // Draw slider background
    SDL_SetRenderDrawColor(renderer, SLIDER_BG.r, SLIDER_BG.g, SLIDER_BG.b, SLIDER_BG.a);
    SDL_RenderFillRect(renderer, &slider->rect);

    // Draw slider handle
    float percentage = (*slider->value - slider->min_value) / (float)(slider->max_value - slider->min_value);
    SDL_Rect handle = {
        .x = slider->rect.x + (int)(percentage * (slider->rect.w - 10)),
        .y = slider->rect.y - 5,
        .w = 10,
        .h = slider->rect.h + 10
    };
    SDL_SetRenderDrawColor(renderer, SLIDER_FG.r, SLIDER_FG.g, SLIDER_FG.b, SLIDER_FG.a);
    SDL_RenderFillRect(renderer, &handle);

    // Draw value text
    char value_text[32];
    snprintf(value_text, sizeof(value_text), "%s: %d", slider->label, *slider->value);
    SDL_SetRenderDrawColor(renderer, TEXT_COLOR.r, TEXT_COLOR.g, TEXT_COLOR.b, TEXT_COLOR.a);
    // Note: In a real implementation, you'd want to render this text using SDL_ttf
    // For now, we'll skip text rendering as it requires additional setup
}

void draw_button(SDL_Renderer* renderer, Button* button) {
    SDL_SetRenderDrawColor(renderer, 
        button->hover ? BUTTON_HOVER.r : BUTTON_NORMAL.r,
        button->hover ? BUTTON_HOVER.g : BUTTON_NORMAL.g,
        button->hover ? BUTTON_HOVER.b : BUTTON_NORMAL.b,
        255);
    SDL_RenderFillRect(renderer, &button->rect);
    // Note: Text rendering would go here with SDL_ttf
}

bool point_in_rect(int x, int y, SDL_Rect* rect) {
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

ServerConfig get_server_config_sdl() {
    ServerConfig config = {
        .map_width = 20,
        .map_height = 20,
        .max_drones = 10,
        .drone_speed = 1,
        .survivor_spawn_rate = 5,
        .port = 2100
    };

    SDL_Window* window = SDL_CreateWindow(
        "Server Configuration",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Create sliders
    Slider sliders[] = {
        {
            .rect = {250, 100, SLIDER_WIDTH, SLIDER_HEIGHT},
            .min_value = 10, .max_value = 100,
            .value = &config.map_width,
            .label = "Map Width",
            .dragging = false
        },
        {
            .rect = {250, 160, SLIDER_WIDTH, SLIDER_HEIGHT},
            .min_value = 10, .max_value = 100,
            .value = &config.map_height,
            .label = "Map Height",
            .dragging = false
        },
        {
            .rect = {250, 220, SLIDER_WIDTH, SLIDER_HEIGHT},
            .min_value = 1, .max_value = 50,
            .value = &config.max_drones,
            .label = "Max Drones",
            .dragging = false
        },
        {
            .rect = {250, 280, SLIDER_WIDTH, SLIDER_HEIGHT},
            .min_value = 1, .max_value = 10,
            .value = &config.drone_speed,
            .label = "Drone Speed",
            .dragging = false
        },
        {
            .rect = {250, 340, SLIDER_WIDTH, SLIDER_HEIGHT},
            .min_value = 1, .max_value = 30,
            .value = &config.survivor_spawn_rate,
            .label = "Spawn Rate",
            .dragging = false
        }
    };

    // Create start button
    Button start_button = {
        .rect = {WINDOW_WIDTH/2 - BUTTON_WIDTH/2, 500, BUTTON_WIDTH, BUTTON_HEIGHT},
        .label = "Start Server",
        .hover = false
    };

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        int mx = event.button.x;
                        int my = event.button.y;
                        
                        // Check sliders
                        for (int i = 0; i < 5; i++) {
                            if (point_in_rect(mx, my, &sliders[i].rect)) {
                                sliders[i].dragging = true;
                            }
                        }
                        
                        // Check start button
                        if (point_in_rect(mx, my, &start_button.rect)) {
                            running = false;
                        }
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        for (int i = 0; i < 5; i++) {
                            sliders[i].dragging = false;
                        }
                    }
                    break;

                case SDL_MOUSEMOTION:
                    int mx = event.motion.x;
                    int my = event.motion.y;

                    // Update sliders
                    for (int i = 0; i < 5; i++) {
                        if (sliders[i].dragging) {
                            float percentage = (mx - sliders[i].rect.x) / (float)sliders[i].rect.w;
                            if (percentage < 0) percentage = 0;
                            if (percentage > 1) percentage = 1;
                            *sliders[i].value = sliders[i].min_value + 
                                              (int)(percentage * (sliders[i].max_value - sliders[i].min_value));
                        }
                    }

                    // Update button hover state
                    start_button.hover = point_in_rect(mx, my, &start_button.rect);
                    break;
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, BACKGROUND.a);
        SDL_RenderClear(renderer);

        // Draw UI elements
        for (int i = 0; i < 5; i++) {
            draw_slider(renderer, &sliders[i]);
        }
        draw_button(renderer, &start_button);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return config;
}

void apply_server_config_sdl(ServerConfig config) {
    // This function remains the same as before
    printf("\n\033[1;36mServer Configuration:\033[0m\n");
    printf("Map Size: %dx%d\n", config.map_width, config.map_height);
    printf("Maximum Drones: %d\n", config.max_drones);
    printf("Drone Speed: %d\n", config.drone_speed);
    printf("Survivor Spawn Rate: %d seconds\n", config.survivor_spawn_rate);
    printf("Server Port: %d\n", config.port);
    printf("\n");
} 