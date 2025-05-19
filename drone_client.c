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
#include "cJSON/cJSON.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5000
#define BUFFER_SIZE 2048

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

int main(int argc, char *argv[]) {
    // Ignore SIGPIPE so send() errors don't terminate process
    signal(SIGPIPE, SIG_IGN);
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    const char* drone_id = argc > 1 ? argv[1] : "D1";
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(EXIT_FAILURE); }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect"); exit(EXIT_FAILURE);
    }
    printf("[DRONE] Connected to server as %s\n", drone_id);
    handshake(sockfd, drone_id);
    // Wait for handshake ack
    cJSON *msg = recv_json(sockfd, buffer, sizeof(buffer));
    if (msg) {
        printf("[DRONE] Server: %s\n", cJSON_Print(msg));
        cJSON_Delete(msg);
    }
    // idle starting position
    int x = 0, y = 0;
    // send initial status
    status_update(sockfd, drone_id, x, y, "idle", 100, 1);
    // mission loop
    while (1) {
        cJSON *msg = recv_json(sockfd, buffer, sizeof(buffer));
        if (!msg) {
            // No message or parse error, wait for assignment
            sleep(1);
            continue;
        }
        const char *type = cJSON_GetObjectItem(msg, "type")->valuestring;
        if (strcmp(type, "MISSION_ASSIGN") == 0) {
            int tx = cJSON_GetObjectItem(msg, "x")->valueint;
            int ty = cJSON_GetObjectItem(msg, "y")->valueint;
            printf("[DRONE] Received MISSION_ASSIGN to (%d,%d)\n", tx, ty);
            // move step by step
            while (x != tx || y != ty) {
                if (x < tx) x++; else if (x > tx) x--;
                if (y < ty) y++; else if (y > ty) y--;
                status_update(sockfd, drone_id, x, y, "on_mission", 100, 1);
                sleep(1);
            }
            // arrived
            mission_complete(sockfd, drone_id, "0");
            printf("[DRONE] Mission complete, reporting\n");
            status_update(sockfd, drone_id, x, y, "idle", 100, 1);
        }
        cJSON_Delete(msg);
    }
    close(sockfd);
    return 0;
}
