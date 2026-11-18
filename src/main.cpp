// Dream Player — a GTK 4 (gtkmm) desktop frontend for the rs_dream_player
// core on Linux.
#include "app/Application.hpp"

#include <unistd.h>

int main(int argc, char* argv[]) {
    int status = 0;
    {
        auto app = dp::Application::create();
        status = app->run(argc, argv);
    }
    if (dp::restart_requested()) {
        // the language (and anything else read at startup) applies anew.
        execv("/proc/self/exe", argv);
    }
    return status;
}
