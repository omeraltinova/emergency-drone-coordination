#ifndef SERVER_CONFIG_UI_H
#define SERVER_CONFIG_UI_H

#include "server_config.h" // Include base ServerConfig definition

// Function prototypes for SDL-based server configuration UI
ServerConfig get_server_config_sdl();
void apply_server_config_sdl(ServerConfig config);

#endif // SERVER_CONFIG_UI_H 