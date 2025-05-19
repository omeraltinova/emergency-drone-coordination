// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "cJSON/cJSON.h"
#include "controller.h"  // Mission assignment logic

#define DEFAULT_PORT 12345
#define BACKLOG 10

// Protokol mesaj tipleri için handler prototipleri
void handle_handshake(int client_fd, cJSON *msg);
void handle_status_update(int client_fd, cJSON *msg);
void handle_mission_complete(int client_fd, cJSON *msg);
void handle_heartbeat_response(int client_fd, cJSON *msg);

void *handle_client(void *arg);

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int port = DEFAULT_PORT;

    if (argc == 2) port = atoi(argv[1]);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Sunucu %d portunda dinleniyor...\n", port);
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    free(arg);
    char buffer[2048];
    ssize_t len;

    printf("Yeni istemci baglantisi: %d\n", client_fd);
    while ((len = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[len] = '\0';
        cJSON *root = cJSON_Parse(buffer);
        if (!root) {
            fprintf(stderr, "Geçersiz JSON: %s\n", buffer);
            continue;
        }
        cJSON *type = cJSON_GetObjectItem(root, "type");
        if (cJSON_IsString(type) && type->valuestring) {
            if (strcmp(type->valuestring, "HANDSHAKE") == 0) {
                handle_handshake(client_fd, root);
            } else if (strcmp(type->valuestring, "STATUS_UPDATE") == 0) {
                handle_status_update(client_fd, root);
            } else if (strcmp(type->valuestring, "MISSION_COMPLETE") == 0) {
                handle_mission_complete(client_fd, root);
            } else if (strcmp(type->valuestring, "HEARTBEAT_RESPONSE") == 0) {
                handle_heartbeat_response(client_fd, root);
            } else {
                fprintf(stderr, "Bilinmeyen mesaj tipi: %s\n", type->valuestring);
            }
        } else {
            fprintf(stderr, "JSON 'type' alanı eksik veya geçersiz\n");
        }
        cJSON_Delete(root);
    }

    printf("İstemci baglantisi kapandi: %d\n", client_fd);
    close(client_fd);
    return NULL;
}

void handle_handshake(int client_fd, cJSON *msg) {
    cJSON *id = cJSON_GetObjectItem(msg, "drone_id");
    if (cJSON_IsString(id) && id->valuestring) {
        printf("Handshake: Drone %s baglandi\n", id->valuestring);
    }
    // ACK gönder
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "HANDSHAKE_ACK");
    char *out = cJSON_PrintUnformatted(resp);
    send(client_fd, out, strlen(out), 0);
    cJSON_Delete(resp);
    free(out);

    // Gorev atama
    const char *mission_id = get_next_mission(id->valuestring);
    cJSON *assign = cJSON_CreateObject();
    cJSON_AddStringToObject(assign, "type", "ASSIGN_MISSION");
    cJSON_AddStringToObject(assign, "drone_id", id->valuestring);
    cJSON_AddStringToObject(assign, "mission_id", mission_id);
    char *assign_str = cJSON_PrintUnformatted(assign);
    send(client_fd, assign_str, strlen(assign_str), 0);
    printf("ASSIGN_MISSION gönderildi: %s\n", assign_str);
    cJSON_Delete(assign);
    free(assign_str);
}

// Diğer handlerlar değişmeden kaldi
void handle_status_update(int client_fd, cJSON *msg) {
    (void)client_fd;
    cJSON *id  = cJSON_GetObjectItem(msg, "drone_id");
    cJSON *lat = cJSON_GetObjectItem(msg, "latitude");
    cJSON *lon = cJSON_GetObjectItem(msg, "longitude");
    cJSON *bat = cJSON_GetObjectItem(msg, "battery");
    if (cJSON_IsString(id) && id->valuestring &&
        cJSON_IsNumber(lat) && cJSON_IsNumber(lon) && cJSON_IsNumber(bat)) {
        printf("Drone %s pozisyon: (%f, %f), batarya: %d%%\n",
               id->valuestring, lat->valuedouble, lon->valuedouble, bat->valueint);
    } else {
        fprintf(stderr, "STATUS_UPDATE: Geçersiz alanlar\n");
    }
}

void handle_mission_complete(int client_fd, cJSON *msg) {
    (void)client_fd;
    cJSON *id  = cJSON_GetObjectItem(msg, "drone_id");
    cJSON *mid = cJSON_GetObjectItem(msg, "mission_id");
    if (cJSON_IsString(id) && id->valuestring &&
        cJSON_IsString(mid) && mid->valuestring) {
        printf("Drone %s gorev tamamlandi: %s\n",
               id->valuestring, mid->valuestring);
    } else {
        fprintf(stderr, "MISSION_COMPLETE: Geçersiz alanlar\n");
    }
}

void handle_heartbeat_response(int client_fd, cJSON *msg) {
    (void)client_fd;
    cJSON *id = cJSON_GetObjectItem(msg, "drone_id");
    if (cJSON_IsString(id) && id->valuestring) {
        printf("Heartbeat response: %s\n", id->valuestring);
    }
}
