#include "Engine.hpp"
#include "IGame.hpp"
#include "ConVar.hpp"
#include <cstdlib>
#include <cmath>

namespace eng {

Engine::Engine(int width, int height, const char* title)
    : width_(width), height_(height) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(width, height, title);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // we handle Esc ourselves (console vs quit)

    cam_.up = {0, 1, 0};
    cam_.fovy = 48.f;
    cam_.projection = CAMERA_PERSPECTIVE;
    cam_.position = {0, 50, 40};
    cam_.target = {0, 0, 0};

    if (const char* a = std::getenv("MOBA_SHOT_AT")) shotAt_ = std::atof(a);
    if (const char* f = std::getenv("MOBA_SHOT_FILE")) shotFile_ = f;

    Con::Print("engine console - type 'help' to list cvars/commands");
}

Engine::~Engine() { CloseWindow(); }

Vector3 Engine::GroundPoint() const {
    Ray r = GetMouseRay(GetMousePosition(), cam_);
    if (std::fabs(r.direction.y) < 1e-4f) return cam_.target;
    float t = -r.position.y / r.direction.y;
    return Vector3{r.position.x + r.direction.x * t, 0.f, r.position.z + r.direction.z * t};
}

void Engine::toggleConsole() {
    consoleOpen_ = !consoleOpen_;
}

void Engine::handleConsoleInput() {
    int c = GetCharPressed();
    while (c > 0) {
        if (c >= 32 && c < 127 && c != '`' && c != '~') cmdline_ += (char)c;
        c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !cmdline_.empty()) cmdline_.pop_back();
    if (IsKeyPressed(KEY_ENTER)) {
        if (!cmdline_.empty()) { Con::Exec(cmdline_); cmdline_.clear(); }
    }
}

void Engine::renderConsole() {
    int h = height_ / 2;
    DrawRectangle(0, 0, width_, h, Color{0, 0, 0, 210});
    DrawLine(0, h, width_, h, Color{80, 200, 255, 255});

    const auto& log = Con::Log();
    int lineH = 18;
    int maxLines = (h - 40) / lineH;
    int start = (int)log.size() > maxLines ? (int)log.size() - maxLines : 0;
    int y = 10;
    for (int i = start; i < (int)log.size(); ++i) {
        DrawText(log[i].c_str(), 10, y, 16, Color{200, 220, 200, 255});
        y += lineH;
    }
    // input line
    std::string prompt = "] " + cmdline_ + ((GetTime() - std::floor(GetTime()) < 0.5) ? "_" : "");
    DrawText(prompt.c_str(), 10, h - 24, 18, Color{120, 230, 255, 255});
}

void Engine::Run(IGame& game) {
    game.OnInit(*this);

    while (!WindowShouldClose() && !shouldQuit_) {
        // --- global keys ---
        if (IsKeyPressed(KEY_GRAVE)) toggleConsole();
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (consoleOpen_) consoleOpen_ = false;
            else if (game.OnEscape()) shouldQuit_ = true; // game may handle Esc (pause menu)
        }

        // --- input ---
        if (consoleOpen_) handleConsoleInput();
        else game.OnInput(*this);

        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;
        game.OnFrame(*this, dt);

        // --- fixed-timestep simulation (skipped while the game reports paused) ---
        if (game.Paused()) {
            accumulator_ = 0.f;
        } else {
            accumulator_ += dt;
            int steps = 0;
            while (accumulator_ >= world_.tickInterval && steps < 8) {
                game.OnTick(world_);
                world_.Simulate();
                accumulator_ -= world_.tickInterval;
                ++steps;
            }
        }

        // --- render ---
        BeginDrawing();
        ClearBackground(background);
        BeginMode3D(cam_);
        game.OnRender3D();
        world_.ForEach([](Entity& e) { e.Render(); });
        EndMode3D();
        world_.ForEach([](Entity& e) { e.Render2D(); });
        game.OnRenderHUD();
        if (consoleOpen_) renderConsole();
        EndDrawing();

        if (shotAt_ >= 0.f && world_.time >= shotAt_ && !shotFile_.empty()) {
            TakeScreenshot(shotFile_.c_str());
            break;
        }
    }
}

} // namespace eng
