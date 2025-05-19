// controller.c
#include "controller.h"
#include <stdio.h>

// Ön tanımlı görev listesi
static const char *missions[] = {
    "mission_001",
    "mission_002",
    "mission_003"
};
static int mission_count = sizeof(missions) / sizeof(missions[0]);
static int next_index = 0;

const char *get_next_mission(const char *drone_id) {
    const char *mission = missions[next_index];
    printf("[Controller] %s için %s gorev atandi\n", drone_id, mission);
    next_index = (next_index + 1) % mission_count;
    return mission;
}