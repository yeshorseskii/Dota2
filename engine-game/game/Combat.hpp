// Combat entity: the game-layer base for anything that fights (hero, creep,
// tower, ancient). Extends the engine Entity with health, a team, and an
// auto-attack. Damage/kill/reward logic lives in MobaGame.
#pragma once
#include "engine/Entity.hpp"
#include "raylib.h"

namespace game {

enum class Team { Radiant, Dire };
inline Team Enemy(Team t) { return t == Team::Radiant ? Team::Dire : Team::Radiant; }
Color TeamColor(Team t);

class CombatEntity : public eng::Entity {
public:
    Team  team = Team::Radiant;
    float hp = 100.f, maxHp = 100.f;
    float attackDamage = 10.f, attackRange = 6.f, attackInterval = 1.f, attackTimer = 0.f;
    float moveSpeed = 0.f;   // units/sec (0 for buildings)
    float height = 2.f;
    float yaw = 0.f;         // facing angle (degrees), updated when moving
    float respawnTimer = 0.f;
    float barWidth = 28.f;   // screen-space HP bar width

    bool alive() const { return hp > 0.f && respawnTimer <= 0.f && !removed; }
    virtual bool persistent() const { return false; } // heroes respawn instead of vanishing
    virtual void onDeath(Team /*killer*/) {}
    virtual float damageTakenMult() const { return 1.f; }

    void takeDamage(float dmg, Team from);
    void tryAutoAttack();       // auto-attack the nearest enemy in range
    void combatTick(float dt);  // decrement attack timer + tryAutoAttack
    void drawHpBar();           // Render2D helper (screen-space bar)
};

} // namespace game
