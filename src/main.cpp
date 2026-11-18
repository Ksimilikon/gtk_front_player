// Dream Player — a GTK 4 (gtkmm) desktop frontend for the rs_dream_player
// core on Linux.
#include "app/Application.hpp"

int main(int argc, char* argv[]) {
    auto app = dp::Application::create();
    return app->run(argc, argv);
}
