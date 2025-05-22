#ifndef LAUNCHER_UI_H
#define LAUNCHER_UI_H

#include <SDL2/SDL.h>

// Initializes the launcher UI window and renderer
int init_launcher_ui();

// Main event loop for the launcher UI
void run_launcher_ui();

// Cleans up launcher UI resources
void close_launcher_ui();

// Functions to be called by button presses (implementations will be in launcher_ui.c)
void launch_server_process_ui(); 
void launch_drone_clients_process_ui(); 

#endif // LAUNCHER_UI_H 