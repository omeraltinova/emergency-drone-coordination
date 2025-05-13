#ifndef VIEW_H
#define VIEW_H

#include <SDL2/SDL.h>

// SDL globals
extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern SDL_Event event;
extern int window_width, window_height;

// Function declarations
extern int init_sdl_window(void);
extern void draw_cell(int x, int y, SDL_Color color);
extern void draw_drones(void);
extern void draw_survivors(void);
extern void draw_grid(void);
extern int draw_map(void);
extern int check_events(void);
extern void quit_all(void);

#endif // VIEW_H