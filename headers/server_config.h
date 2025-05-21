#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

// Server configuration structure
typedef struct {
    int port;               // Server port number
    int max_drones;        // Maximum number of drones
    int map_width;         // Map width
    int map_height;        // Map height
    int survivor_spawn_rate;  // Rate at which survivors spawn (in seconds)
    int drone_speed;       // Speed of the drones
} ServerConfig;

// Function declarations
void print_server_banner(void);
ServerConfig get_server_config(void);
void apply_server_config(ServerConfig config);

#endif // SERVER_CONFIG_H 