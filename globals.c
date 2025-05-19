// globals.c
#include "headers/drone.h"
#include "headers/survivor.h"
#include "headers/view.h"
#include <SDL2/SDL.h>

// Drone yönetimi
Drone *drone_fleet     = NULL;
int    num_drones      = 0;

// Kurtarılacak/kurtarılan survivor listeleri
List  *survivors           = NULL;
List  *helpedsurvivors     = NULL;
int    running             = 1;

// SDL pencere/renderer ve olay yapıları
SDL_Window   *window       = NULL;
SDL_Renderer *renderer     = NULL;
SDL_Event     event;
int           window_width = 0;
int           window_height= 0;
