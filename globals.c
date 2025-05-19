#include "headers/globals.h"

List *survivors = NULL;
List *helpedsurvivors = NULL;
List *drones = NULL;

volatile sig_atomic_t running = 1;
