#include "Game.hpp"
#include <cstdlib>
#include <ctime>

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    mb::Game game;
    game.run();
    return 0;
}
