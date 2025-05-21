#TODO edit# Makefile for Emergency Drone Coordination System - Phase 2

CC = gcc
CFLAGS = -Wall -g -Iheaders -IcJSON $(shell pkg-config --cflags sdl2 SDL2_ttf)
LDFLAGS = -L/opt/homebrew/lib $(shell pkg-config --libs sdl2 SDL2_ttf) -lpthread

SRCS_SERVER = server.c globals.c list.c map.c survivor.c view.c server_config.c server_config_ui.c cJSON/cJSON.c
OBJS_SERVER = $(SRCS_SERVER:.c=.o)

SRCS_CLIENT = drone_client.c cJSON/cJSON.c
OBJS_CLIENT = $(SRCS_CLIENT:.c=.o)

SRCS_LAUNCHER = main_launcher.c launcher_ui.c
OBJS_LAUNCHER = $(SRCS_LAUNCHER:.c=.o)

TARGET_SERVER = server
TARGET_CLIENT = drone_client
TARGET_LAUNCHER = launcher

all: $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_LAUNCHER)

$(TARGET_SERVER): $(OBJS_SERVER)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_CLIENT): $(OBJS_CLIENT)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_LAUNCHER): $(OBJS_LAUNCHER)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_LAUNCHER) $(OBJS_SERVER) $(OBJS_CLIENT) $(OBJS_LAUNCHER)

.PHONY: all clean