// Emergency Drone Coordination System - Phase 2 Server Implementation
// Multi-threaded TCP server for drone coordination
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "cJSON/cJSON.h"
#include "headers/drone.h"
#include "headers/list.h"
#include "headers/survivor.h"
#include "headers/map.h"
#include "headers/view.h"
#include "headers/globals.h"
#include "headers/server_config.h"
#include <signal.h>
#include <SDL2/SDL.h>
#include "headers/ai.h"
#include <limits.h>
#include <ctype.h>
#include <time.h>

#define SERVER_PORT 2100
#define MAX_CLIENTS 64
#define BUFFER_SIZE 2048

// Protocol intervals
#define STATUS_UPDATE_INTERVAL 5
#define HEARTBEAT_INTERVAL 10

pthread_mutex_t drones_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t survivors_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
void* client_handler(void* arg);
void* ui_thread(void* arg);
void* survivor_generator(void*);
void* ai_controller(void*);
void *heartbeat_thread(void *arg);

void send_json(int sockfd, cJSON *json) {
    char *data = cJSON_PrintUnformatted(json);
    send(sockfd, data, strlen(data), 0);
    send(sockfd, "\n", 1, 0); // newline as delimiter
    free(data);
}

cJSON* recv_json(int sockfd, char *buffer, size_t buflen) {
    ssize_t len = recv(sockfd, buffer, buflen - 1, 0);
    if (len <= 0) return NULL;
    buffer[len] = '\0';
    return cJSON_Parse(buffer);
}

void handle_handshake(int client_sock, cJSON *msg) {
    printf("[SERVER] HANDSHAKE received from drone_id: %s\n", cJSON_GetObjectItem(msg, "drone_id")->valuestring);
    // Register drone, add to drone list
    const char *idstr = cJSON_GetObjectItem(msg, "drone_id")->valuestring;
    int id = 0;
    if ((idstr[0] == 'd' || idstr[0] == 'D') && idstr[1]) id = atoi(idstr + 1);
    else id = atoi(idstr);
    Drone *d = malloc(sizeof(Drone));
    memset(d,0,sizeof(Drone));
    d->id = id; d->sockfd = client_sock; d->status = IDLE;
    d->coord = (Coord){0,0}; d->target = d->coord;
    pthread_mutex_init(&d->lock,NULL); d->lock_initialized=true;
    pthread_cond_init(&d->mission_cv,NULL); d->cv_initialized=true;
    pthread_mutex_lock(&drones_mutex);
    drones->add(drones,&d);
    pthread_mutex_unlock(&drones_mutex);
    // Send HANDSHAKE_ACK with session_id and config
    cJSON *ack = cJSON_CreateObject();
    cJSON_AddStringToObject(ack, "type", "HANDSHAKE_ACK");
    cJSON_AddStringToObject(ack, "session_id", "S1");
    cJSON *cfg = cJSON_CreateObject();
    cJSON_AddNumberToObject(cfg, "status_update_interval", STATUS_UPDATE_INTERVAL);
    cJSON_AddNumberToObject(cfg, "heartbeat_interval", HEARTBEAT_INTERVAL);
    cJSON_AddItemToObject(ack, "config", cfg);
    send_json(client_sock, ack);
    cJSON_Delete(ack);
}

void handle_status_update(int client_sock, cJSON *msg) {
    int timestamp = cJSON_GetObjectItem(msg, "timestamp")->valueint;
    int battery = cJSON_GetObjectItem(msg, "battery")->valueint;
    int speed = cJSON_GetObjectItem(msg, "speed")->valueint;
    printf("[SERVER] STATUS_UPDATE from drone_id: %s, status: %s, timestamp: %d, battery: %d, speed: %d\n",
        cJSON_GetObjectItem(msg, "drone_id")->valuestring,
        cJSON_GetObjectItem(msg, "status")->valuestring,
        timestamp, battery, speed);
    // Update drone status and position in list
    const char *idstr = cJSON_GetObjectItem(msg, "drone_id")->valuestring;
    int id = 0;
    if ((idstr[0] == 'd' || idstr[0] == 'D') && idstr[1]) id = atoi(idstr + 1);
    else id = atoi(idstr);
    cJSON *loc = cJSON_GetObjectItem(msg, "location");
    int x = cJSON_GetObjectItem(loc, "x")->valueint;
    int y = cJSON_GetObjectItem(loc, "y")->valueint;
    const char *st = cJSON_GetObjectItem(msg, "status")->valuestring;
    pthread_mutex_lock(&drones_mutex);
    for (Node *n = drones->head; n; n = n->next) {
        Drone *d = *(Drone **)n->data;
        if (d->id == id) {
            pthread_mutex_lock(&d->lock);
            d->coord.x = x;
            d->coord.y = y;
            if (strcmp(st, "idle") == 0) {
                if (d->status != ON_MISSION) {
                    d->status = IDLE;
                }
            } else if (strcmp(st, "busy") == 0) {
                d->status = ON_MISSION;
            }
            pthread_mutex_unlock(&d->lock);
            break;
        }
    }
    pthread_mutex_unlock(&drones_mutex);
}

void handle_mission_complete(int client_sock, cJSON *msg) {
    int timestamp = cJSON_GetObjectItem(msg, "timestamp")->valueint;
    int success = cJSON_GetObjectItem(msg, "success")->valueint;
    const char *details = cJSON_GetObjectItem(msg, "details")->valuestring;
    printf("[SERVER] MISSION_COMPLETE from drone_id: %s, mission_id: %s, timestamp: %d, success: %s, details: %s\n",
        cJSON_GetObjectItem(msg, "drone_id")->valuestring,
        cJSON_GetObjectItem(msg, "mission_id")->valuestring,
        timestamp,
        success ? "true" : "false",
        details);
    // Mark mission complete: set drone to IDLE
    const char *idstr = cJSON_GetObjectItem(msg, "drone_id")->valuestring;
    int id = 0;
    if ((idstr[0] == 'd' || idstr[0] == 'D') && idstr[1]) id = atoi(idstr + 1);
    else id = atoi(idstr);
    pthread_mutex_lock(&drones_mutex);
    for (Node *n = drones->head; n; n = n->next) {
        Drone *d = *(Drone **)n->data;
        if (d->id == id) {
            pthread_mutex_lock(&d->lock);
            d->status = IDLE;
            pthread_mutex_unlock(&d->lock);
            break;
        }
    }
    pthread_mutex_unlock(&drones_mutex);
}

void handle_heartbeat_response(int client_sock, cJSON *msg) {
    printf("[SERVER] HEARTBEAT_RESPONSE from drone_id: %s\n",
        cJSON_GetObjectItem(msg, "drone_id")->valuestring);
    // Optionally update last-seen timestamp
}

void* client_handler(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    printf("[SERVER] New client connected, socket: %d\n", client_sock);
    char buffer[BUFFER_SIZE];
    while (running) {
        // receive JSON message
        cJSON *msg = recv_json(client_sock, buffer, sizeof(buffer));
        if (!msg) {
            // Invalid or no JSON: send ERROR
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "type", "ERROR");
            cJSON_AddNumberToObject(err, "code", 400);
            cJSON_AddStringToObject(err, "message", "Invalid JSON");
            cJSON_AddNumberToObject(err, "timestamp", (int)time(NULL));
            send_json(client_sock, err);
            cJSON_Delete(err);
            sleep(1);
            continue;
        }
        const char *type = cJSON_GetObjectItem(msg, "type")->valuestring;
        if (strcmp(type, "HANDSHAKE") == 0) {
            handle_handshake(client_sock, msg);
        } else if (strcmp(type, "STATUS_UPDATE") == 0) {
            handle_status_update(client_sock, msg);
        } else if (strcmp(type, "MISSION_COMPLETE") == 0) {
            handle_mission_complete(client_sock, msg);
        } else if (strcmp(type, "HEARTBEAT_RESPONSE") == 0) {
            handle_heartbeat_response(client_sock, msg);
        } else {
            // Send ERROR for unknown message type
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "type", "ERROR");
            cJSON_AddNumberToObject(err, "code", 400);
            cJSON_AddStringToObject(err, "message", "Unknown message type");
            cJSON_AddNumberToObject(err, "timestamp", (int)time(NULL));
            send_json(client_sock, err);
            cJSON_Delete(err);
        }
        cJSON_Delete(msg);
    }
    printf("[SERVER] Client handler exiting, socket: %d\n", client_sock);
    close(client_sock);
    pthread_exit(NULL);
}

// UI thread to handle SDL rendering
void *ui_thread(void *arg) {
    // Initialize SDL and create window in UI thread
    if (init_sdl_window()) exit(EXIT_FAILURE);

    while (running) {
        draw_map();
        if (check_events()) { running = 0; break; }
        SDL_Delay(100);
    }
    // Cleanup SDL
    quit_all();
    return NULL;
}

int main(int argc, char *argv[]) {
    // Display welcome banner and get configuration
    print_server_banner();
    ServerConfig config = get_server_config();
    // Override map dimensions from command-line if provided
    if (argc == 3) {
        int w = atoi(argv[1]);
        int h = atoi(argv[2]);
        if (w > 0 && h > 0) {
            config.map_width = w;
            config.map_height = h;
        } else {
            fprintf(stderr, "Invalid map size. Using defaults.\n");
        }
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [map_width map_height]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    apply_server_config(config);

    int server_sock, *client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid;

    // Store pointers to Drone
    drones = create_list(sizeof(Drone*), config.max_drones);
    // Store pointers to Survivor
    survivors = create_list(sizeof(Survivor*), 128);
    // Store pointers to Survivor for helped list
    helpedsurvivors = create_list(sizeof(Survivor*), 128);
    
    // Initialize map dimensions with configured values (height, width)
    init_map(config.map_height, config.map_width);

    // SDL initialization: on macOS do here; on Linux do in UI thread
    #ifdef __APPLE__
    if (init_sdl_main_thread()) { fprintf(stderr, "Failed to initialize SDL\n"); exit(EXIT_FAILURE); }
    #endif
    // Start Phase1 simulator threads
    pthread_t surv_tid, ai_tid, ui_tid;
    
    // Start survivor generator with configured spawn rate
    struct timespec spawn_interval = {.tv_sec = config.survivor_spawn_rate, .tv_nsec = 0};
    pthread_create(&surv_tid, NULL, survivor_generator, &spawn_interval);
    pthread_detach(surv_tid);
    
    pthread_create(&ai_tid, NULL, ai_controller, NULL);
    pthread_detach(ai_tid);

    // Start UI thread
    pthread_create(&ui_tid, NULL, ui_thread, NULL);
    pthread_detach(ui_tid);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("socket"); exit(EXIT_FAILURE); }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config.port);
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
    
    if (listen(server_sock, config.max_drones) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }
    
    printf("[SERVER] Listening on port %d...\n", config.port);

    // start heartbeat thread
    pthread_t hb_tid;
    pthread_create(&hb_tid, NULL, heartbeat_thread, NULL);
    pthread_detach(hb_tid);

    while (running) {
        // Handle SDL events on macOS
        if (check_events()) {
            running = 0;
            break;
        }

        // Use select to handle both network and events
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(server_sock, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout

        int activity = select(server_sock + 1, &readfds, NULL, NULL, &tv);
        
        if (activity > 0 && FD_ISSET(server_sock, &readfds)) {
            client_sock = malloc(sizeof(int));
            *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
            if (*client_sock < 0) { 
                free(client_sock); 
                continue; 
            }
            printf("[SERVER] Accepted new connection from %s:%d\n", 
                   inet_ntoa(client_addr.sin_addr), 
                   ntohs(client_addr.sin_port));
            pthread_create(&tid, NULL, client_handler, client_sock);
            pthread_detach(tid);
        }
    }

    close(server_sock);
    quit_all();
    destroy(drones);
    destroy(survivors);
    destroy(helpedsurvivors);
    return 0;
}

// AI assigns survivors to idle drones
void *ai_controller(void *arg) {
    while (running) {
        // wait until at least one drone connected
        pthread_mutex_lock(&drones_mutex);
        if (!drones->head) {
            pthread_mutex_unlock(&drones_mutex);
            sleep(1);
            continue;
        }
        pthread_mutex_unlock(&drones_mutex);
        // get the oldest survivor (tail)
        Survivor *s = NULL;
        pthread_mutex_lock(&survivors_mutex);
        Node *n = survivors->tail;
        if (n) {
            s = *(Survivor**)n->data;
            // remove that node
            survivors->removenode(survivors, n);
        }
        pthread_mutex_unlock(&survivors_mutex);
        if (!s) { sleep(1); continue; }
        // find closest idle drone
        Drone *best=NULL; int mind=INT_MAX;
        pthread_mutex_lock(&drones_mutex);
        for (Node *dn = drones->head; dn; dn=dn->next) {
            Drone *d = *(Drone**)dn->data;
            pthread_mutex_lock(&d->lock);
            if (d->status==IDLE) {
                int dist=abs(d->coord.x-s->coord.x)+abs(d->coord.y-s->coord.y);
                if (dist<mind) { mind=dist; best=d; }
            }
            pthread_mutex_unlock(&d->lock);
        }
        pthread_mutex_unlock(&drones_mutex);
        if (best) {
            // send assign mission
            int expiry = (int)time(NULL) + 300;
            static int mission_counter = 1;
            char mid[16]; sprintf(mid, "M%d", mission_counter++);
            printf("[AI] Assigning survivor at (%d,%d) to drone %d\n", s->coord.x, s->coord.y, best->id);
            cJSON *js = cJSON_CreateObject();
            cJSON_AddStringToObject(js, "type", "ASSIGN_MISSION");
            cJSON_AddStringToObject(js, "mission_id", mid);
            cJSON_AddStringToObject(js, "priority", "medium");
            cJSON *target = cJSON_CreateObject();
            cJSON_AddNumberToObject(target, "x", s->coord.x);
            cJSON_AddNumberToObject(target, "y", s->coord.y);
            cJSON_AddItemToObject(js, "target", target);
            cJSON_AddNumberToObject(js, "expiry", expiry);
            cJSON_AddStringToObject(js, "checksum", "a1b2c3");
            send_json(best->sockfd, js); cJSON_Delete(js);
            pthread_mutex_lock(&best->lock);
            best->status=ON_MISSION; 
            best->target = s->coord;
            pthread_mutex_unlock(&best->lock);
            helpedsurvivors->add(helpedsurvivors,&s);
        } else {
            printf("[AI] No idle drone available for survivor at (%d,%d), requeue\n", s->coord.x, s->coord.y);
            pthread_mutex_lock(&survivors_mutex);
            survivors->add(survivors,&s);
            pthread_mutex_unlock(&survivors_mutex);
        }
        sleep(1);
    }
    return NULL;
}

// Heartbeat thread
void *heartbeat_thread(void *arg) {
    while (running) {
        cJSON *hb = cJSON_CreateObject();
        cJSON_AddStringToObject(hb, "type", "HEARTBEAT");
        cJSON_AddNumberToObject(hb, "timestamp", (int)time(NULL));
        pthread_mutex_lock(&drones_mutex);
        for (Node *n = drones->head; n; n = n->next) {
            Drone *d = *(Drone **)n->data;
            send_json(d->sockfd, hb);
        }
        pthread_mutex_unlock(&drones_mutex);
        cJSON_Delete(hb);
        sleep(HEARTBEAT_INTERVAL);
    }
    return NULL;
}
