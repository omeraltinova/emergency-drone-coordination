// drone_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "cJSON/cJSON.h"

#define DEFAULT_IP       "127.0.0.1"
#define DEFAULT_PORT     12345
#define BUFFER_SIZE      2048
#define STATUS_INTERVAL  5  // saniye
#define MISSION_DURATION 10 // örnek görev süresi (saniye)

// Gelen mesajları işleyen fonksiyonlar
void handle_assign_mission(int client_fd, cJSON *msg);
void handle_heartbeat(cJSON *msg, int client_fd);

void *receive_thread(void *arg) {
    int sock = *((int *)arg);
    char buffer[BUFFER_SIZE];
    ssize_t len;

    while ((len = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[len] = '\0';
        cJSON *msg = cJSON_Parse(buffer);
        if (!msg) {
            fprintf(stderr, "Geçersiz JSON alındı: %s\n", buffer);
            continue;
        }
        cJSON *type = cJSON_GetObjectItem(msg, "type");
        if (cJSON_IsString(type)) {
            if (strcmp(type->valuestring, "ASSIGN_MISSION") == 0) {
                handle_assign_mission(sock, msg);
            } else if (strcmp(type->valuestring, "HEARTBEAT") == 0) {
                handle_heartbeat(msg, sock);
            }
        }
        cJSON_Delete(msg);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    const char *server_ip = DEFAULT_IP;
    int port = DEFAULT_PORT;
    if (argc >= 2) server_ip = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    // TCP soket oluşturma
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); close(sock); return EXIT_FAILURE;
    }
    printf("Sunucuya bağlandı: %s:%d\n", server_ip, port);

    // HANDSHAKE gönder
    cJSON *hs = cJSON_CreateObject();
    cJSON_AddStringToObject(hs, "type", "HANDSHAKE");
    cJSON_AddStringToObject(hs, "drone_id", "drone_001");
    char *payload = cJSON_PrintUnformatted(hs);
    send(sock, payload, strlen(payload), 0);
    printf("HANDSHAKE gönderildi: %s\n", payload);
    cJSON_Delete(hs);
    free(payload);

    // HANDSHAKE_ACK bekle
    char buffer[BUFFER_SIZE];
    ssize_t len = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (len <= 0) { perror("recv"); close(sock); return EXIT_FAILURE; }
    buffer[len] = '\0';
    printf("Sunucudan gelen: %s\n", buffer);

    // Mesaj dinleme işlevini ayrı iş parçacığında başlat
    pthread_t tid;
    pthread_create(&tid, NULL, receive_thread, &sock);
    pthread_detach(tid);

    // Periyodik STATUS_UPDATE döngüsü
    while (1) {
        cJSON *up = cJSON_CreateObject();
        cJSON_AddStringToObject(up, "type", "STATUS_UPDATE");
        cJSON_AddStringToObject(up, "drone_id", "drone_001");
        cJSON_AddNumberToObject(up, "latitude", 0.0);
        cJSON_AddNumberToObject(up, "longitude", 0.0);
        cJSON_AddNumberToObject(up, "battery", 100);
        char *stat = cJSON_PrintUnformatted(up);
        send(sock, stat, strlen(stat), 0);
        printf("STATUS_UPDATE gönderildi: %s\n", stat);
        cJSON_Delete(up);
        free(stat);
        sleep(STATUS_INTERVAL);
    }

    close(sock);
    return 0;
}

void handle_assign_mission(int client_fd, cJSON *msg) {
    cJSON *mid = cJSON_GetObjectItem(msg, "mission_id");
    if (cJSON_IsString(mid)) {
        printf("Yeni görev atandı: %s\n", mid->valuestring);
        // Görevi simüle et (örnek uyku)
        sleep(MISSION_DURATION);
        // MISSION_COMPLETE gönder
        cJSON *mc = cJSON_CreateObject();
        cJSON_AddStringToObject(mc, "type", "MISSION_COMPLETE");
        cJSON_AddStringToObject(mc, "drone_id", "drone_001");
        cJSON_AddStringToObject(mc, "mission_id", mid->valuestring);
        char *compl = cJSON_PrintUnformatted(mc);
        send(client_fd, compl, strlen(compl), 0);
        printf("MISSION_COMPLETE gönderildi: %s\n", compl);
        cJSON_Delete(mc);
        free(compl);
    }
}

void handle_heartbeat(cJSON *msg, int client_fd) {
    cJSON *hb = cJSON_CreateObject();
    cJSON_AddStringToObject(hb, "type", "HEARTBEAT_RESPONSE");
    cJSON_AddStringToObject(hb, "drone_id", "drone_001");
    char *resp = cJSON_PrintUnformatted(hb);
    send(client_fd, resp, strlen(resp), 0);
    printf("HEARTBEAT_RESPONSE gönderildi: %s\n", resp);
    cJSON_Delete(hb);
    free(resp);
}
