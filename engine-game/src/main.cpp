// Entry point: create the engine, plug in the MOBA game module, run.
#include "engine/Engine.hpp"
#include "game/MobaGame.hpp"
#include <cstdlib>
#include <ctime>

int main() {
    std::srand((unsigned)std::time(nullptr));
    eng::Engine engine(1280, 720, "Mini MOBA — engine build");
    game::MobaGame moba;
    engine.Run(moba);
    return 0;
}
