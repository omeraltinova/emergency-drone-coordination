#ifndef GLOBALS_H
#define GLOBALS_H

#include <signal.h>
#include "map.h"
#include "drone.h"
#include "survivor.h"
#include "list.h"
#include "coord.h"

extern Map map;
extern List *survivors, *helpedsurvivors, *drones;
extern volatile sig_atomic_t running;

#endif