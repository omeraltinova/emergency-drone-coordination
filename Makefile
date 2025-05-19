#TODO edit# Makefile for Emergency Drone Coordination System - Phase 2

CC = gcc
CFLAGS = -Wall -g -Iheaders -IcJSON
LDFLAGS = -lpthread

CC = gcc
CFLAGS = -Wall -g -Iheaders -IcJSON
LDFLAGS = -lpthread -lSDL2

all: server drone_client

server: server.c globals.c list.c map.c survivor.c view.c cJSON/cJSON.c \
        headers/server.h headers/list.h headers/map.h headers/survivor.h headers/view.h
	$(CC) $(CFLAGS) server.c globals.c list.c map.c survivor.c view.c cJSON/cJSON.c $(LDFLAGS) -o server

drone_client: drone_client.c ai.c globals.c cJSON/cJSON.c headers/drone_client.h headers/ai.h
	$(CC) $(CFLAGS) drone_client.c ai.c globals.c cJSON/cJSON.c $(LDFLAGS) -o drone_client

clean:
	rm -f *.o server drone_client

.PHONY: all clean