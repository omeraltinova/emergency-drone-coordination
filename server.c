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
#include <signal.h>
#include <SDL2/SDL.h>
#include "headers/ai.h"
#include <limits.h>
#include <ctype.h>

#define SERVER_PORT 5000
#define MAX_CLIENTS 32
#define BUFFER_SIZE 2048

pthread_mutex_t drones_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t survivors_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
void* client_handler(void* arg);
void* ui_thread(void* arg);
void* survivor_generator(void*);
void* ai_controller(void*);

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
    // Send HANDSHAKE_ACK
    cJSON *ack = cJSON_CreateObject();
    cJSON_AddStringToObject(ack, "type", "HANDSHAKE_ACK");
    cJSON_AddStringToObject(ack, "message", "Registered");
    send_json(client_sock, ack);
    cJSON_Delete(ack);
}

void handle_status_update(int client_sock, cJSON *msg) {
    printf("[SERVER] STATUS_UPDATE from drone_id: %s, status: %s\n",
        cJSON_GetObjectItem(msg, "drone_id")->valuestring,
        cJSON_GetObjectItem(msg, "status")->valuestring);
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
            if (strcmp(st, "idle") == 0) d->status = IDLE;
            else if (strcmp(st, "on_mission") == 0) d->status = ON_MISSION;
            pthread_mutex_unlock(&d->lock);
            break;
        }
    }
    pthread_mutex_unlock(&drones_mutex);
}

void handle_mission_complete(int client_sock, cJSON *msg) {
    printf("[SERVER] MISSION_COMPLETE from drone_id: %s, mission_id: %s\n",
        cJSON_GetObjectItem(msg, "drone_id")->valuestring,
        cJSON_GetObjectItem(msg, "mission_id")->valuestring);
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
            // no complete message, wait
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
            // Send ERROR
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "type", "ERROR");
            cJSON_AddStringToObject(err, "message", "Unknown message type");
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
    if (init_sdl_window()) exit(EXIT_FAILURE);
    while (running) {
        draw_map();
        // Handle quit events
        if (check_events()) { running = 0; break; }
        SDL_Delay(100);
    }
    quit_all();
    return NULL;
}

int main() {
    int server_sock, *client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid;

    // Store pointers to Drone
    drones = create_list(sizeof(Drone*), 128);
    // Store pointers to Survivor
    survivors = create_list(sizeof(Survivor*), 128);
    // Store pointers to Survivor for helped list
    helpedsurvivors = create_list(sizeof(Survivor*), 128);
    // Initialize map dimensions (height, width)
    init_map(20, 20);

    // Start Phase1 simulator threads
    pthread_t surv_tid, ai_tid;
    pthread_create(&surv_tid,NULL,survivor_generator,NULL);
    pthread_detach(surv_tid);
    pthread_create(&ai_tid,NULL,ai_controller,NULL);
    pthread_detach(ai_tid);

    // Start UI thread
    pthread_t ui_tid;
    pthread_create(&ui_tid, NULL, ui_thread, NULL);
    pthread_detach(ui_tid);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("socket"); exit(EXIT_FAILURE); }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }
    printf("[SERVER] Listening on port %d...\n", SERVER_PORT);
    while (1) {
        client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (*client_sock < 0) { free(client_sock); continue; }
        printf("[SERVER] Accepted new connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        pthread_create(&tid, NULL, client_handler, client_sock);
        pthread_detach(tid);
    }
    close(server_sock);
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
            // send mission
            printf("[AI] Assigning survivor at (%d,%d) to drone %d\n", s->coord.x, s->coord.y, best->id);
            cJSON *js=cJSON_CreateObject();
            cJSON_AddStringToObject(js,"type","MISSION_ASSIGN");
            cJSON_AddNumberToObject(js,"x",s->coord.x);
            cJSON_AddNumberToObject(js,"y",s->coord.y);
            send_json(best->sockfd,js); cJSON_Delete(js);
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
