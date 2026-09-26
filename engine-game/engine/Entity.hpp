// Base entity — Source-inspired: every object in the world derives from this
// and overrides Spawn / Think / Update / OnTouch / Render.
//
//   Spawn()   once, right after the entity is created and added to the world.
//   Update()  every simulation tick (continuous motion, timers).
//   Think()   scheduled: runs when world.time >= nextThink (set via SetNextThink).
//   OnTouch() when two solid entities overlap this tick.
//   Render()  3D drawing inside the camera; Render2D() screen-space overlay.
#pragma once
#include "Math.hpp"
#include <cstdint>

namespace eng {

class World;
using EntityId = std::uint32_t;

class Entity {
public:
    virtual ~Entity() = default;

    virtual void Spawn() {}
    virtual void Update(float /*dt*/) {}
    virtual void Think() {}
    virtual void OnTouch(Entity& /*other*/) {}
    virtual void Render() {}
    virtual void Render2D() {}

    // Transform / physics-ish state.
    Vector3 pos{0, 0, 0};
    Vector3 vel{0, 0, 0};
    float radius = 1.0f;
    bool solid = false;   // participates in OnTouch overlap tests

    // Bookkeeping.
    EntityId id = 0;
    const char* classname = "entity";
    World* world = nullptr;
    bool removed = false;      // marked for deletion at end of tick
    double nextThink = -1.0;   // < 0 means no think scheduled

    void SetNextThink(double t) { nextThink = t; }
    void Remove() { removed = true; }
};

} // namespace eng
