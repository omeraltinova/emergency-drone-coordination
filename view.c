// view.c
#include "headers/view.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "cJSON/cJSON.h"
#include "headers/drone.h"
#include "headers/map.h"
#include "headers/survivor.h"

#define CELL_SIZE       20
#define DEFAULT_IP      "127.0.0.1"
#define DEFAULT_PORT    12345
#define STATUS_INTERVAL 5  // ticks

// Global externs from other modules
extern Drone *drone_fleet;
extern int num_drones;
extern List *survivors, *helpedsurvivors;
extern int running;

// Network globals
static int sock_fd = -1;
static pthread_t net_thread;
static int connected = 0;

// Function prototypes
static void *network_loop(void *arg);
static void handle_assign_mission(int client_fd, cJSON *msg);
static void handle_heartbeat(cJSON *msg, int client_fd);
static void send_status_update(void);

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    // Initialize SDL and network
    if (init_sdl_window() != 0) {
        fprintf(stderr, "Failed to initialize application\n");
        return 1;
    }
    // Main render + event loop
    while (!check_events() && running) {
        draw_map();
        SDL_Delay(100); // 100ms
    }
    quit_all();
    return 0;
}

static void *network_loop(void *arg) {
    (void)arg;
    char buffer[2048];
    ssize_t len;
    while ((len = recv(sock_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[len] = '\0';
        cJSON *msg = cJSON_Parse(buffer);
        if (!msg) continue;
        cJSON *type = cJSON_GetObjectItem(msg, "type");
        if (cJSON_IsString(type)) {
            if (strcmp(type->valuestring, "ASSIGN_MISSION") == 0) {
                handle_assign_mission(sock_fd, msg);
            } else if (strcmp(type->valuestring, "HEARTBEAT") == 0) {
                handle_heartbeat(msg, sock_fd);
            }
        }
        cJSON_Delete(msg);
    }
    return NULL;
}

static void send_status_update(void) {
    if (!connected || num_drones < 1) return;
    Drone *d = &drone_fleet[0];
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%d", d->id);
    cJSON *up = cJSON_CreateObject();
    cJSON_AddStringToObject(up, "type", "STATUS_UPDATE");
    cJSON_AddStringToObject(up, "drone_id", id_str);
    cJSON_AddNumberToObject(up, "latitude", d->coord.x);
    cJSON_AddNumberToObject(up, "longitude", d->coord.y);
    cJSON_AddNumberToObject(up, "battery", 100);
    char *out = cJSON_PrintUnformatted(up);
    send(sock_fd, out, strlen(out), 0);
    cJSON_Delete(up);
    free(out);
}

static void handle_assign_mission(int client_fd, cJSON *msg) {
    cJSON *mid = cJSON_GetObjectItem(msg, "mission_id");
    if (cJSON_IsString(mid)) {
        printf("GUI: Assign mission %s\n", mid->valuestring);
        // TODO: find idle drone and set status/target
    }
}

static void handle_heartbeat(cJSON *msg, int client_fd) {
    (void)msg;
    cJSON *hb = cJSON_CreateObject();
    cJSON_AddStringToObject(hb, "type", "HEARTBEAT_RESPONSE");
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%d", drone_fleet[0].id);
    cJSON_AddStringToObject(hb, "drone_id", id_str);
    char *out = cJSON_PrintUnformatted(hb);
    send(client_fd, out, strlen(out), 0);
    cJSON_Delete(hb);
    free(out);
}

int init_sdl_window(void) {
    window_width = map.width * CELL_SIZE;
    window_height = map.height * CELL_SIZE;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    window = SDL_CreateWindow("Drone Simulator",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              window_width,
                              window_height,
                              SDL_WINDOW_SHOWN);
    if (!window) return -1;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    // Setup network
    struct sockaddr_in serv;
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) return -1;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, DEFAULT_IP, &serv.sin_addr);
    if (connect(sock_fd, (struct sockaddr *)&serv, sizeof(serv)) < 0) return -1;
    // handshake
    cJSON *hs = cJSON_CreateObject();
    cJSON_AddStringToObject(hs, "type", "HANDSHAKE");
    cJSON_AddStringToObject(hs, "drone_id", "drone_001");
    char *out = cJSON_PrintUnformatted(hs);
    send(sock_fd, out, strlen(out), 0);
    cJSON_Delete(hs);
    free(out);
    connected = 1;
    pthread_create(&net_thread, NULL, network_loop, NULL);
    pthread_detach(net_thread);
    return 0;
}

void quit_all(void) {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (sock_fd >= 0) close(sock_fd);
    SDL_Quit();
}

int draw_map(void) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    draw_survivors();
    draw_drones();
    draw_grid();
    SDL_RenderPresent(renderer);
    send_status_update();
    return 0;
}

int check_events(void) {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return 1;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            return 1;
    }
    return 0;
}
