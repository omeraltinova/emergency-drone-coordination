#include "headers/launcher_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For getcwd (used in commented-out advanced launch)
#include <SDL2/SDL_ttf.h>

#define SCREEN_WIDTH 600
#define SCREEN_HEIGHT 400
#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 50
#define SMALL_BUTTON_WIDTH 40
#define PADDING 20
#define INPUT_AREA_Y (PADDING + BUTTON_HEIGHT + PADDING + BUTTON_HEIGHT + PADDING) // Y position for input elements

static SDL_Window* launcher_window = NULL;
static SDL_Renderer* launcher_renderer = NULL;
static TTF_Font* launcher_font = NULL;

// Configurable values
static int map_width_val = 20;
static int map_height_val = 20;
static int drone_count_val = 3;


// Button structure
typedef struct {
    SDL_Rect rect;
    char* text;
    SDL_Color color;
    SDL_Color text_color;
    void (*action)(); // Optional: for simple actions without parameters
} Button;

// Input field structure (simplified with +/- buttons)
typedef struct {
    SDL_Rect label_rect;
    SDL_Rect value_rect;
    SDL_Rect plus_rect;
    SDL_Rect minus_rect;
    char* label_text;
    int* value_ptr;
    int min_val;
    int max_val;
} InputField;

static Button btn_launch_server;
static Button btn_launch_drones;

static InputField input_map_width;
static InputField input_map_height;
static InputField input_drone_count;


SDL_Texture* render_text_launcher(const char* text, SDL_Color color) {
    if (!launcher_font) return NULL;
    SDL_Surface* surface = TTF_RenderText_Solid(launcher_font, text, color);
    if (!surface) {
        fprintf(stderr, "TTF_RenderText_Solid Error: %s\n", TTF_GetError());
        return NULL;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(launcher_renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
    }
    return texture;
}

void draw_text_launcher(const char* text, int x, int y, SDL_Color color) {
    SDL_Texture* texture = render_text_launcher(text, color);
    if (texture) {
        int text_w, text_h;
        SDL_QueryTexture(texture, NULL, NULL, &text_w, &text_h);
        SDL_Rect render_rect = {x, y, text_w, text_h};
        SDL_RenderCopy(launcher_renderer, texture, NULL, &render_rect);
        SDL_DestroyTexture(texture);
    }
}

void draw_button_launcher(Button* btn) {
    SDL_SetRenderDrawColor(launcher_renderer, btn->color.r, btn->color.g, btn->color.b, btn->color.a);
    SDL_RenderFillRect(launcher_renderer, &btn->rect);

    SDL_Texture* text_texture = render_text_launcher(btn->text, btn->text_color);
    if (text_texture) {
        int text_w, text_h;
        SDL_QueryTexture(text_texture, NULL, NULL, &text_w, &text_h);
        SDL_Rect text_rect = {
            btn->rect.x + (btn->rect.w - text_w) / 2,
            btn->rect.y + (btn->rect.h - text_h) / 2,
            text_w,
            text_h
        };
        SDL_RenderCopy(launcher_renderer, text_texture, NULL, &text_rect);
        SDL_DestroyTexture(text_texture);
    }
}

void draw_input_field(InputField* field) {
    // Draw label
    draw_text_launcher(field->label_text, field->label_rect.x, field->label_rect.y, (SDL_Color){255, 255, 255, 255});

    // Draw value
    char value_str[10];
    snprintf(value_str, sizeof(value_str), "%d", *field->value_ptr);
    draw_text_launcher(value_str, field->value_rect.x + (field->value_rect.w - strlen(value_str)*8)/2 , field->value_rect.y, (SDL_Color){255, 255, 255, 255}); // Simple positioning

    // Draw +/- buttons (simple rects for now, could be Buttons)
    SDL_SetRenderDrawColor(launcher_renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(launcher_renderer, &field->minus_rect);
    SDL_RenderFillRect(launcher_renderer, &field->plus_rect);
    draw_text_launcher("-", field->minus_rect.x + 12, field->minus_rect.y + 2, (SDL_Color){255,255,255,255});
    draw_text_launcher("+", field->plus_rect.x + 12, field->plus_rect.y + 2, (SDL_Color){255,255,255,255});
}


int init_launcher_ui() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    launcher_window = SDL_CreateWindow("Emergency Drone System Launcher",
                                     SDL_WINDOWPOS_UNDEFINED,
                                     SDL_WINDOWPOS_UNDEFINED,
                                     SCREEN_WIDTH, SCREEN_HEIGHT,
                                     SDL_WINDOW_SHOWN);
    if (!launcher_window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    launcher_renderer = SDL_CreateRenderer(launcher_window, -1, SDL_RENDERER_ACCELERATED);
    if (!launcher_renderer) {
        fprintf(stderr, "Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(launcher_window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    launcher_font = TTF_OpenFont("Arial.ttf", 20); // Default font, provide path or use system font
    if (!launcher_font) {
        launcher_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20); // Linux fallback
    }
    if (!launcher_font) {
        launcher_font = TTF_OpenFont("/Library/Fonts/Arial.ttf", 20); // macOS fallback
    }
     if (!launcher_font) {
        launcher_font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 20); // another macOS fallback
    }
    if (!launcher_font) {
        fprintf(stderr, "Failed to load font! TTF_Error: %s\n", TTF_GetError());
        // Continue without font, or handle error more gracefully
    }

    // Initialize buttons
    btn_launch_server.rect = (SDL_Rect){PADDING, PADDING, BUTTON_WIDTH, BUTTON_HEIGHT};
    btn_launch_server.text = "Launch Server";
    btn_launch_server.color = (SDL_Color){0, 150, 0, 255}; // Green
    btn_launch_server.text_color = (SDL_Color){255, 255, 255, 255}; // White
    btn_launch_server.action = NULL;


    btn_launch_drones.rect = (SDL_Rect){PADDING, PADDING + BUTTON_HEIGHT + PADDING, BUTTON_WIDTH, BUTTON_HEIGHT};
    btn_launch_drones.text = "Launch Drones";
    btn_launch_drones.color = (SDL_Color){0, 0, 150, 255}; // Blue
    btn_launch_drones.text_color = (SDL_Color){255, 255, 255, 255}; // White
    btn_launch_drones.action = NULL;

    // Initialize InputFields
    int current_y = INPUT_AREA_Y;
    input_map_width = (InputField){
        .label_rect = {PADDING, current_y, 150, 30},
        .value_rect = {PADDING + 160, current_y, 50, 30},
        .minus_rect = {PADDING + 160 + 60, current_y, SMALL_BUTTON_WIDTH, 30},
        .plus_rect = {PADDING + 160 + 60 + SMALL_BUTTON_WIDTH + 10, current_y, SMALL_BUTTON_WIDTH, 30},
        .label_text = "Map Width:",
        .value_ptr = &map_width_val,
        .min_val = 5, .max_val = 100
    };
    current_y += 40;
    input_map_height = (InputField){
        .label_rect = {PADDING, current_y, 150, 30},
        .value_rect = {PADDING + 160, current_y, 50, 30},
        .minus_rect = {PADDING + 160 + 60, current_y, SMALL_BUTTON_WIDTH, 30},
        .plus_rect = {PADDING + 160 + 60 + SMALL_BUTTON_WIDTH + 10, current_y, SMALL_BUTTON_WIDTH, 30},
        .label_text = "Map Height:",
        .value_ptr = &map_height_val,
        .min_val = 5, .max_val = 100
    };
    current_y += 40;
    input_drone_count = (InputField){
        .label_rect = {PADDING, current_y, 150, 30},
        .value_rect = {PADDING + 160, current_y, 50, 30},
        .minus_rect = {PADDING + 160 + 60, current_y, SMALL_BUTTON_WIDTH, 30},
        .plus_rect = {PADDING + 160 + 60 + SMALL_BUTTON_WIDTH + 10, current_y, SMALL_BUTTON_WIDTH, 30},
        .label_text = "Drone Count:",
        .value_ptr = &drone_count_val,
        .min_val = 1, .max_val = 50
    };

    return 0;
}

void launch_server_process_ui() { // Renamed to avoid conflict if header is not updated yet
    char command[256];
    snprintf(command, sizeof(command), "./server %d %d &", map_width_val, map_height_val);
    printf("[Launcher UI] Executing: %s\n", command);
    system(command);
}

void launch_drone_clients_process_ui() { // Renamed to avoid conflict
    char command[128];
    for (int i = 0; i < drone_count_val; ++i) {
        snprintf(command, sizeof(command), "./drone_client D%d &", i + 1);
        printf("[Launcher UI] Executing: %s\n", command);
        system(command);
        SDL_Delay(100); 
    }
}

void handle_input_click(InputField* field, SDL_Point* mouse_point) {
    if (SDL_PointInRect(mouse_point, &field->minus_rect)) {
        if (*field->value_ptr > field->min_val) {
            (*field->value_ptr)--;
        }
    } else if (SDL_PointInRect(mouse_point, &field->plus_rect)) {
        if (*field->value_ptr < field->max_val) {
            (*field->value_ptr)++;
        }
    }
}


void run_launcher_ui() {
    SDL_Event e;
    int running = 1;

    while (running) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                SDL_Point mouse_point = {x, y};

                if (SDL_PointInRect(&mouse_point, &btn_launch_server.rect)) {
                    printf("[Launcher UI] Launch Server button clicked! Map: %dx%d\n", map_width_val, map_height_val);
                    launch_server_process_ui();
                }
                if (SDL_PointInRect(&mouse_point, &btn_launch_drones.rect)) {
                    printf("[Launcher UI] Launch Drones button clicked! Count: %d\n", drone_count_val);
                    launch_drone_clients_process_ui();
                }
                handle_input_click(&input_map_width, &mouse_point);
                handle_input_click(&input_map_height, &mouse_point);
                handle_input_click(&input_drone_count, &mouse_point);
            }
        }

        SDL_SetRenderDrawColor(launcher_renderer, 50, 50, 50, 255); // Dark grey background
        SDL_RenderClear(launcher_renderer);

        draw_button_launcher(&btn_launch_server);
        draw_button_launcher(&btn_launch_drones);

        draw_input_field(&input_map_width);
        draw_input_field(&input_map_height);
        draw_input_field(&input_drone_count);

        SDL_RenderPresent(launcher_renderer);
        SDL_Delay(10); 
    }
}

void close_launcher_ui() {
    if (launcher_font) {
        TTF_CloseFont(launcher_font);
        launcher_font = NULL;
    }
    if (launcher_renderer) {
        SDL_DestroyRenderer(launcher_renderer);
        launcher_renderer = NULL;
    }
    if (launcher_window) {
        SDL_DestroyWindow(launcher_window);
        launcher_window = NULL;
    }
    TTF_Quit();
    SDL_Quit();
} 