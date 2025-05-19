// controller.h
#define CONTROLLER_H

// Bir sonraki görev kimliğini döndürür.
// Basit round-robin atama algoritması kullanır.
const char *get_next_mission(const char *drone_id);