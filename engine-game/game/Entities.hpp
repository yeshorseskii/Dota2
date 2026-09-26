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
    float shieldTimer = 0.f, qCd = 0.f, wCd = 0.f;

    void Spawn() override;
    void Update(float dt) override;
    void Render() override;
    void Render2D() override;
    bool persistent() const override { return true; }
    float damageTakenMult() const override { return shieldTimer > 0.f ? 0.5f : 1.f; }
    void onDeath(Team killer) override;

    void castQ(Vector3 target);
    void castW();
    void addXp(float amount);
};

class Creep : public CombatEntity {
public:
    int laneIndex = 0;
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

class FloatText : public eng::Entity {
public:
    std::string text;
    Color color = WHITE;
    float life = 0.9f, maxLife = 0.9f;
    void Update(float dt) override;
    void Render2D() override;
};

} // namespace game
