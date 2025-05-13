#TODO edit make file for your project
CC = gcc
CFLAGS = -Wall -pthread -I.
LDFLAGS = -lSDL2 -lm

ifeq ($(shell uname -s), Darwin)
	LDFLAGS = -F/Library/Frameworks -framework SDL2
endif

SRCS = list.c view.c survivor.c controller.c drone.c map.c ai.c
OBJS = $(SRCS:.c=.o)
TARGET = drone_simulator

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean