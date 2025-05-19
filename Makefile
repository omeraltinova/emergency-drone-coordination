CC = gcc
CFLAGS = -Wall -Wextra -I./cJSON -I/usr/include/SDL2 -I./headers
LDFLAGS = -lpthread -lSDL2

# Ortak kaynaklar (cJSON, controller)
SRC_COMMON = cJSON/cJSON.c controller.c globals.c
OBJ_COMMON = $(SRC_COMMON:.c=.o)

# Sunucu
SRC_SERVER = server.c
OBJ_SERVER = $(SRC_SERVER:.c=.o)

# CLI istemci
SRC_CLI = drone_client.c
OBJ_CLI = $(SRC_CLI:.c=.o)

# GUI istemci (SDL + view)
SRC_GUI = view.c map.c list.c survivor.c
OBJ_GUI = $(SRC_GUI:.c=.o)

# Hedef uygulamalar
TARGETS = server drone_client gui

all: $(TARGETS)

server: $(OBJ_SERVER) $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Komut satırı istemcisi
drone_client: $(OBJ_CLI) $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# SDL tabanlı GUI istemcisi
gui: $(OBJ_GUI) $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Genel kural: .c -> .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_COMMON) $(OBJ_SERVER) $(OBJ_CLI) $(OBJ_GUI) $(TARGETS)