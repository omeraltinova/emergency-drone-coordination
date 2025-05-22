#include "headers/launcher_ui.h"

int main(int argc, char* argv[]) {
    if (init_launcher_ui() != 0) {
        return 1;
    }

    run_launcher_ui();

    close_launcher_ui();

    return 0;
} 