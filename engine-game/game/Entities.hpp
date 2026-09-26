// Concrete game entities. Each overrides the Source-style hooks (Spawn/Update/
// Think/OnTouch/Render). Effects (projectile, beam, floating text) are entities
// too, so the whole game is expressed through the engine's entity model.
#pragma once
#include "Combat.hpp"
#include <string>

namespace game {

class Hero : public CombatEntity {
public:
    bool isPlayer = false;
    int level = 1, gold = 0;
    float xp = 0.f, xpToLevel = 100.f;
    Vector3 spawnPos{};
    Vector3 moveOrder{};
    bool hasMoveOrder = false;
    float shieldTimer = 0.f;
    float mana = 300.f, maxMana = 300.f, manaRegen = 8.f, hpRegen = 2.f;
    float cd[4] = {0, 0, 0, 0};   // Q, W, E (blink), R (ultimate)

    void Spawn() override;
    void Update(float dt) override;
    void Render() override;
    void Render2D() override;
    bool persistent() const override { return true; }
    float damageTakenMult() const override { return shieldTimer > 0.f ? 0.5f : 1.f; }
    void onDeath(Team killer) override;

    void cast(int index, Vector3 target);   // Q/W/E/R dispatch
    void addXp(float amount);
};

class Creep : public CombatEntity {
public:
    int lane = 1;        // 0=top, 1=mid, 2=bottom
    int laneIndex = 0;   // progress along that lane's waypoints
    void Spawn() override;
    void Update(float dt) override;
    void Render() override;
    void Render2D() override { drawHpBar(); }
};

class Tower : public CombatEntity {
public:
    void Spawn() override;
    void Update(float dt) override;
    void Render() override;
    void Render2D() override { drawHpBar(); }
};

class Ancient : public CombatEntity {
public:
    void Spawn() override;
    void Update(float dt) override;
    void Render() override;
    void Render2D() override { drawHpBar(); }
    void onDeath(Team killer) override;
};

class Projectile : public eng::Entity {
public:
    Team team = Team::Radiant;
    float damage = 0.f, life = 2.f, splashRadius = 0.f;
    Color color = WHITE;
    void Spawn() override { classname = "projectile"; solid = true; }
    void Update(float dt) override;
    void OnTouch(eng::Entity& other) override;
    void Render() override;
};

class Beam : public eng::Entity {
public:
    Vector3 a{}, b{};
    Color color = WHITE;
    float life = 0.14f, maxLife = 0.14f;
    void Update(float dt) override;
    void Render() override;
};

// A small glowing particle (spark) used for spell bursts and impacts.
class Particle : public eng::Entity {
public:
    Vector3 vel{};
    float life = 0.6f, maxLife = 0.6f, size = 0.3f, grav = 0.f;
    Color color = WHITE;
    void Spawn() override { classname = "fx"; }
    void Update(float dt) override;
    void Render() override;
};

// An expanding ground ring used for AoE casts / impacts.
class Ring : public eng::Entity {
public:
    float r0 = 1.f, r1 = 12.f, life = 0.5f, maxLife = 0.5f;
    Color color = WHITE;
    void Spawn() override { classname = "fx"; }
    void Update(float dt) override;
    void Render() override;
};

class FloatText : public eng::Entity {
public:
    std::string text;
    Color color = WHITE;
    float life = 0.9f, maxLife = 0.9f;
    void Update(float dt) override;
    void Render2D() override;
};

} // namespace game
