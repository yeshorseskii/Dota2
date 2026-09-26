// Engine: owns the window, the World, the camera, the in-game console, and the
// fixed-timestep main loop. A game module (IGame) is driven by Run().
#pragma once
#include "World.hpp"
#include "raylib.h"
#include <string>

namespace eng {

class IGame;

class Engine {
public:
    Engine(int width, int height, const char* title);
    ~Engine();

    void Run(IGame& game);

    // Accessors for game modules.
    World&    world() { return world_; }
    Camera3D& camera() { return cam_; }
    int  screenW() const { return width_; }
    int  screenH() const { return height_; }
    bool consoleOpen() const { return consoleOpen_; }

    // Mouse ray intersected with the ground plane (y = 0).
    Vector3 GroundPoint() const;

    // Background clear color (games may tweak).
    Color background{18, 20, 22, 255};

private:
    void toggleConsole();
    void handleConsoleInput();
    void renderConsole();

    World world_;
    Camera3D cam_{};
    int width_, height_;

    bool consoleOpen_ = false;
    bool shouldQuit_ = false;
    std::string cmdline_;

    float accumulator_ = 0.f;

    // Optional headless preview capture (MOBA_SHOT_AT / MOBA_SHOT_FILE).
    float shotAt_ = -1.f;
    std::string shotFile_;
};

} // namespace eng
