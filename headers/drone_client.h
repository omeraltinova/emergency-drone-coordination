#ifndef DRONE_CLIENT_H
#define DRONE_CLIENT_H

void handshake(int sockfd, const char* drone_id);
void status_update(int sockfd, const char* drone_id, int x, int y, const char* status, int battery, int speed);
void mission_complete(int sockfd, const char* drone_id, const char* mission_id);

#endif // DRONE_CLIENT_H
