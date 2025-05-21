// Emergency Drone Coordination System - Phase 2 Drone Client Implementation
// TCP client for drone, communicates with server using JSON protocol
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include "cJSON/cJSON.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 2100
#define BUFFER_SIZE 2048

// Drone state structure
typedef struct {
    int sockfd;
    char* drone_id;
    int x;
    int y;
    int target_x;
    int target_y;
    int battery;
    int speed;
    int on_mission;
    pthread_mutex_t lock;
    pthread_cond_t mission_cv;
} DroneState;

DroneState* drone_state;

void send_json(int sockfd, cJSON *json) {
    char *data = cJSON_PrintUnformatted(json);
    send(sockfd, data, strlen(data), 0);
    send(sockfd, "\n", 1, 0);
    free(data);
}

cJSON* recv_json(int sockfd, char *buffer, size_t buflen) {
    ssize_t len = recv(sockfd, buffer, buflen - 1, 0);
    if (len <= 0) return NULL;
    buffer[len] = '\0';
    return cJSON_Parse(buffer);
}

void handshake(int sockfd, const char* drone_id) {
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", "HANDSHAKE");
    cJSON_AddStringToObject(msg, "drone_id", drone_id);
    cJSON *cap = cJSON_CreateObject();
    cJSON_AddNumberToObject(cap, "max_speed", 30);
    cJSON_AddNumberToObject(cap, "battery_capacity", 100);
    cJSON_AddStringToObject(cap, "payload", "medical");
    cJSON_AddItemToObject(msg, "capabilities", cap);
    send_json(sockfd, msg);
    cJSON_Delete(msg);
}

void status_update(int sockfd, const char* drone_id, int x, int y, const char* status, int battery, int speed) {
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", "STATUS_UPDATE");
    cJSON_AddStringToObject(msg, "drone_id", drone_id);
    cJSON_AddNumberToObject(msg, "timestamp", (int)time(NULL));
    cJSON *loc = cJSON_CreateObject();
    cJSON_AddNumberToObject(loc, "x", x);
    cJSON_AddNumberToObject(loc, "y", y);
    cJSON_AddItemToObject(msg, "location", loc);
    cJSON_AddStringToObject(msg, "status", status);
    cJSON_AddNumberToObject(msg, "battery", battery);
    cJSON_AddNumberToObject(msg, "speed", speed);
    send_json(sockfd, msg);
    cJSON_Delete(msg);
}

void mission_complete(int sockfd, const char* drone_id, const char* mission_id) {
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", "MISSION_COMPLETE");
    cJSON_AddStringToObject(msg, "drone_id", drone_id);
    cJSON_AddStringToObject(msg, "mission_id", mission_id);
    cJSON_AddNumberToObject(msg, "timestamp", (int)time(NULL));
    cJSON_AddBoolToObject(msg, "success", 1);
    cJSON_AddStringToObject(msg, "details", "Delivered aid to survivor.");
    send_json(sockfd, msg);
    cJSON_Delete(msg);
}

void* movement_thread(void* arg) {
    DroneState* state = (DroneState*)arg;
    printf("[DRONE %s] movement_thread: Entered\n", state->drone_id); // Debug entry

    while (1) {
        pthread_mutex_lock(&state->lock);
        printf("[DRONE %s] movement_thread: Waiting for mission (on_mission=%d)\n", state->drone_id, state->on_mission); // Debug wait
        while (!state->on_mission) {
            pthread_cond_wait(&state->mission_cv, &state->lock);
             printf("[DRONE %s] movement_thread: Woke up from cond_wait (on_mission=%d)\n", state->drone_id, state->on_mission); // Debug wake up
        }
        
        // Store target coordinates
        int tx = state->target_x;
        int ty = state->target_y;
        printf("[DRONE %s] movement_thread: Mission received! Target: (%d,%d). Current: (%d,%d)\n", state->drone_id, tx, ty, state->x, state->y); // Debug mission start
        pthread_mutex_unlock(&state->lock);

        // Move along X axis first
        // printf("[DRONE %s] Movement thread: Starting X-axis movement to %d from %d\n", state->drone_id, tx, state->x); // Old log, can be removed
        while (1) {
            pthread_mutex_lock(&state->lock);
            if (state->x == tx) {
                printf("[DRONE %s] movement_thread: X-axis movement complete. Current X: %d, Target X: %d\n", state->drone_id, state->x, tx); // Debug X complete
                pthread_mutex_unlock(&state->lock);
                break;
            }
            if (state->x < tx) state->x++; else state->x--;
            // printf("[DRONE %s] Movement thread: Moved to X=%d, Y=%d\n", state->drone_id, state->x, state->y); // Old log, can be removed
            status_update(state->sockfd, state->drone_id, state->x, state->y, "busy", state->battery, state->speed);
            printf("[DRONE %s] movement_thread: Sent STATUS_UPDATE (busy) for X-move. New pos: (%d,%d)\n", state->drone_id, state->x, state->y); // Debug status update
            pthread_mutex_unlock(&state->lock);
            sleep(1);
        }

        // Then move along Y axis
        // printf("[DRONE %s] Movement thread: Starting Y-axis movement to %d from %d\n", state->drone_id, ty, state->y); // Old log, can be removed
        while (1) {
            pthread_mutex_lock(&state->lock);
            if (state->y == ty) {
                printf("[DRONE %s] movement_thread: Y-axis movement complete. Current Y: %d, Target Y: %d\n", state->drone_id, state->y, ty); // Debug Y complete
                // Mission complete
                mission_complete(state->sockfd, state->drone_id, "0");
                printf("[DRONE %s] movement_thread: Sent MISSION_COMPLETE. Pos: (%d,%d)\n", state->drone_id, state->x, state->y); // Debug mission complete
                status_update(state->sockfd, state->drone_id, state->x, state->y, "idle", state->battery, state->speed);
                printf("[DRONE %s] movement_thread: Sent STATUS_UPDATE (idle) after mission. Pos: (%d,%d)\n", state->drone_id, state->x, state->y); // Debug status idle
                state->on_mission = 0;
                pthread_mutex_unlock(&state->lock);
                break;
            }
            if (state->y < ty) state->y++; else state->y--;
            // printf("[DRONE %s] Movement thread: Moved to X=%d, Y=%d\n", state->drone_id, state->x, state->y); // Old log, can be removed
            status_update(state->sockfd, state->drone_id, state->x, state->y, "busy", state->battery, state->speed);
            printf("[DRONE %s] movement_thread: Sent STATUS_UPDATE (busy) for Y-move. New pos: (%d,%d)\n", state->drone_id, state->x, state->y); // Debug status update
            pthread_mutex_unlock(&state->lock);
            sleep(1);
        }
    }
    printf("[DRONE %s] movement_thread: Exiting\n", state->drone_id); // Debug exit
    return NULL;
}

void* communication_thread(void* arg) {
    DroneState* state = (DroneState*)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        cJSON *msg = recv_json(state->sockfd, buffer, sizeof(buffer));
        if (!msg) {
            sleep(1);
            continue;
        }

        const char *type = cJSON_GetObjectItem(msg, "type")->valuestring;
        if (strcmp(type, "ASSIGN_MISSION") == 0) {
            // Parse target coordinates from nested object
            cJSON *tgt = cJSON_GetObjectItem(msg, "target");
            int tx = 0, ty = 0;
            if (tgt) {
                tx = cJSON_GetObjectItem(tgt, "x")->valueint;
                ty = cJSON_GetObjectItem(tgt, "y")->valueint;
            }
            pthread_mutex_lock(&state->lock);
            state->target_x = tx;
            state->target_y = ty;
            printf("[DRONE] Received ASSIGN_MISSION to (%d,%d)\n", tx, ty);
            state->on_mission = 1;
            pthread_cond_signal(&state->mission_cv);
            pthread_mutex_unlock(&state->lock);
        } else if (strcmp(type, "HEARTBEAT") == 0) {
            // Respond to server heartbeat
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "type", "HEARTBEAT_RESPONSE");
            cJSON_AddStringToObject(resp, "drone_id", state->drone_id);
            cJSON_AddNumberToObject(resp, "timestamp", (int)time(NULL));
            send_json(state->sockfd, resp);
            cJSON_Delete(resp);
        }
        cJSON_Delete(msg);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    const char* drone_id = argc > 1 ? argv[1] : "D1";

    // Initialize drone state
    drone_state = malloc(sizeof(DroneState));
    drone_state->drone_id = strdup(drone_id);
    drone_state->x = 0;
    drone_state->y = 0;
    drone_state->battery = 100;
    drone_state->speed = 1;
    drone_state->on_mission = 0;
    pthread_mutex_init(&drone_state->lock, NULL);
    pthread_cond_init(&drone_state->mission_cv, NULL);

    // Connect to server
    struct sockaddr_in serv_addr;
    drone_state->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (drone_state->sockfd < 0) { perror("socket"); exit(EXIT_FAILURE); }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
    
    if (connect(drone_state->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect"); exit(EXIT_FAILURE);
    }
    
    printf("[DRONE] Connected to server as %s\n", drone_id);
    
    // Initial handshake
    printf("[DRONE-DEBUG] Sending HANDSHAKE...\n");
    handshake(drone_state->sockfd, drone_id);
    printf("[DRONE-DEBUG] HANDSHAKE sent, awaiting ACK...\n");
    char buffer[BUFFER_SIZE];
    cJSON *msg = recv_json(drone_state->sockfd, buffer, sizeof(buffer));
    printf("[DRONE-DEBUG] recv_json returned %p\n", (void*)msg);
    if (msg) {
        char *resp_str = cJSON_PrintUnformatted(msg);
        if (resp_str) {
            printf("[DRONE] Server: %s\n", resp_str);
            free(resp_str);
        }
        cJSON_Delete(msg);
    }
    printf("[DRONE-DEBUG] After processing HANDSHAKE_ACK\n");

    // Send initial status
    printf("[DRONE-DEBUG] Sending initial STATUS_UPDATE...\n");
    status_update(drone_state->sockfd, drone_id, 0, 0, "idle", 100, 1);
    printf("[DRONE-DEBUG] STATUS_UPDATE sent\n");

    // Create movement and communication threads
    printf("[DRONE-DEBUG] Creating threads\n");
    pthread_t move_tid, comm_tid;
    int ret1 = pthread_create(&move_tid, NULL, movement_thread, drone_state);
    int ret2 = pthread_create(&comm_tid, NULL, communication_thread, drone_state);
    printf("[DRONE-DEBUG] Threads created: %d, %d\n", ret1, ret2);
    pthread_join(move_tid, NULL);
    pthread_join(comm_tid, NULL);
    printf("[DRONE-DEBUG] Threads joined\n");

    // Cleanup
    printf("[DRONE-DEBUG] Cleaning up and exiting\n");
    close(drone_state->sockfd);
    pthread_mutex_destroy(&drone_state->lock);
    pthread_cond_destroy(&drone_state->mission_cv);
    free(drone_state->drone_id);
    free(drone_state);
    
    return 0;
}
