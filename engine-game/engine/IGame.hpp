// Game module interface. A game (e.g. the MOBA) implements this and is plugged
// into the engine, keeping engine and game code separate — like a Source "mod".
#pragma once

namespace eng {

class Engine;
class World;

class IGame {
public:
    virtual ~IGame() = default;

    virtual void OnInit(Engine& /*e*/) {}   // set up map, spawn initial entities
    virtual void OnInput(Engine& /*e*/) {}  // read input, issue orders (skipped while console open)
    virtual void OnFrame(Engine& /*e*/, float /*dt*/) {} // every frame (e.g. camera), even with console open
    virtual void OnTick(World& /*w*/) {}    // per fixed tick: waves, win checks, etc.
    virtual void OnRender3D() {}            // extra world-space drawing (map/lane)
    virtual void OnRenderHUD() {}           // screen-space HUD
    virtual const char* Title() const { return "engine game"; }
};

} // namespace eng
