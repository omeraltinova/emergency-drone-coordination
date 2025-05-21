#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include "headers/server.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 650
#define SLIDER_WIDTH 300
#define SLIDER_HEIGHT 20
#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 40
#define TEXT_INPUT_HEIGHT 30

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

TTF_Font* font = NULL;

void render_text(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) {
        printf("Failed to create text surface: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        printf("Failed to create text texture: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }
    SDL_Rect dst_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst_rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void draw_slider(SDL_Renderer* renderer, Slider* slider) {
    SDL_SetRenderDrawColor(renderer, SLIDER_BG.r, SLIDER_BG.g, SLIDER_BG.b, SLIDER_BG.a);
    SDL_RenderFillRect(renderer, &slider->rect);

    float percentage = (*slider->value - slider->min_value) / (float)(slider->max_value - slider->min_value);
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 1.0f) percentage = 1.0f;

    SDL_Rect handle = {
        .x = slider->rect.x + (int)(percentage * (slider->rect.w - 10)),
        .y = slider->rect.y - 5,
        .w = 10,
        .h = slider->rect.h + 10
    };
    SDL_SetRenderDrawColor(renderer, SLIDER_FG.r, SLIDER_FG.g, SLIDER_FG.b, SLIDER_FG.a);
    SDL_RenderFillRect(renderer, &handle);

    char text_buffer[64];
    snprintf(text_buffer, sizeof(text_buffer), "%s: %d", slider->label, *slider->value);
    render_text(renderer, text_buffer, slider->rect.x + slider->rect.w + 15, slider->rect.y, TEXT_COLOR);
    render_text(renderer, slider->label, slider->rect.x - 150, slider->rect.y, TEXT_COLOR);
}

void draw_button(SDL_Renderer* renderer, Button* button) {
    SDL_SetRenderDrawColor(renderer,
        button->hover ? BUTTON_HOVER.r : BUTTON_NORMAL.r,
        button->hover ? BUTTON_HOVER.g : BUTTON_NORMAL.g,
        button->hover ? BUTTON_HOVER.b : BUTTON_NORMAL.b,
        255);
    SDL_RenderFillRect(renderer, &button->rect);
    if (font && button->label) {
        int text_width, text_height;
        TTF_SizeText(font, button->label, &text_width, &text_height);
        int text_x = button->rect.x + (button->rect.w - text_width) / 2;
        int text_y = button->rect.y + (button->rect.h - text_height) / 2;
        render_text(renderer, button->label, text_x, text_y, TEXT_COLOR);
    }
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

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return config;
    }

    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return config;
    }

    font = TTF_OpenFont("Arial.ttf", 18);
    if (!font) {
        #ifdef _WIN32
        font = TTF_OpenFont("C:/Windows/Fonts/Arial.ttf", 18);
        #elif __APPLE__
        font = TTF_OpenFont("/Library/Fonts/Arial.ttf", 18);
        if (!font) {
            font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 18);
        }
        if (!font) {
            font = TTF_OpenFont("/System/Library/Fonts/HelveticaNeue.ttc", 18);
        }
        #elif __linux__
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18);
        if (!font) font = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 18);
        #endif
    }
    if (!font) {
        printf("Failed to load font: %s\nPlease ensure a common TTF font (e.g., Arial.ttf, DejaVuSans.ttf) is available in the system or project directory.\n", TTF_GetError());
    }

    SDL_Window* window = SDL_CreateWindow(
        "Server Configuration",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

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
            .label = "Spawn Rate (s)",
            .dragging = false
        },
        {
            .rect = {250, 400, SLIDER_WIDTH, SLIDER_HEIGHT},
            .min_value = 1024, .max_value = 49151,
            .value = &config.port,
            .label = "Server Port",
            .dragging = false
        }
    };
    const int num_sliders = sizeof(sliders) / sizeof(sliders[0]);

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
                        
                        for (int i = 0; i < num_sliders; i++) {
                            if (point_in_rect(mx, my, &sliders[i].rect) ||
                                (mx >= sliders[i].rect.x && mx <= sliders[i].rect.x + sliders[i].rect.w &&
                                 my >= sliders[i].rect.y -5 && my <= sliders[i].rect.y + sliders[i].rect.h + 5)) {
                                sliders[i].dragging = true;
                                float percentage = (mx - sliders[i].rect.x) / (float)sliders[i].rect.w;
                                if (percentage < 0) percentage = 0;
                                if (percentage > 1) percentage = 1;
                                *sliders[i].value = sliders[i].min_value +
                                                  (int)(percentage * (sliders[i].max_value - sliders[i].min_value));
                            }
                        }
                        
                        if (point_in_rect(mx, my, &start_button.rect)) {
                            running = false;
                        }
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        for (int i = 0; i < num_sliders; i++) {
                            sliders[i].dragging = false;
                        }
                    }
                    break;

                case SDL_MOUSEMOTION:
                    // int mx_motion = event.motion.x; // Removed
                    // int my_motion = event.motion.y; // Removed

                    // Update sliders -  Removed logic for updating sliders during mouse motion
                    // for (int i = 0; i < num_sliders; i++) { 
                    //     if (sliders[i].dragging) {
                    //         float percentage = (mx_motion - sliders[i].rect.x) / (float)sliders[i].rect.w;
                    //         if (percentage < 0) percentage = 0;
                    //         if (percentage > 1) percentage = 1;
                    //         *sliders[i].value = sliders[i].min_value + 
                    //                           (int)(percentage * (sliders[i].max_value - sliders[i].min_value));
                    //     }
                    // }

                    // Update button hover state - Removed hover update
                    // start_button.hover = point_in_rect(mx_motion, my_motion, &start_button.rect);
                    
                    // No action needed in MOUSEMOTION for this simplified version
                    break;
            }
        }

        SDL_SetRenderDrawColor(renderer, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, BACKGROUND.a);
        SDL_RenderClear(renderer);

        for (int i = 0; i < num_sliders; i++) {
            draw_slider(renderer, &sliders[i]);
        }
        draw_button(renderer, &start_button);

        SDL_RenderPresent(renderer);
    }

    if (font) {
        TTF_CloseFont(font);
    }
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return config;
}

void apply_server_config_sdl(ServerConfig config) {
    printf("\n\033[1;36mServer Configuration:\033[0m\n");
    printf("Map Size: %dx%d\n", config.map_width, config.map_height);
    printf("Maximum Drones: %d\n", config.max_drones);
    printf("Drone Speed: %d\n", config.drone_speed);
    printf("Survivor Spawn Rate: %d seconds\n", config.survivor_spawn_rate);
    printf("Server Port: %d\n", config.port);
    printf("\n");
} 