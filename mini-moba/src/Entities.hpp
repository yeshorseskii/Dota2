// Data types for every fighting thing on the map.
#pragma once
#include "Math.hpp"
#include <SFML/Graphics/Color.hpp>
#include <string>

namespace mb {

enum class Team { Radiant, Dire, Neutral };

enum class UnitKind { Hero, Creep, Tower, Ancient };

// A single fighting entity. We use one struct with a `kind` tag instead of a
// class hierarchy: the behaviour differences are small and this keeps all the
// units in flat, cache-friendly vectors that are trivial to iterate.
struct Unit {
    UnitKind kind = UnitKind::Creep;
    Team team = Team::Radiant;

    Vec2 pos;
    Vec2 spawn;          // where towers/ancients sit; creep lane origin
    float radius = 12.f; // collision / draw size

    float hp = 100.f;
    float maxHp = 100.f;

    float attackDamage = 10.f;
    float attackRange = 60.f;
    float attackInterval = 1.0f; // seconds between attacks
    float attackTimer = 0.f;     // counts down to next allowed attack
    bool ranged = false;

    float moveSpeed = 0.f; // px/sec; 0 for immobile buildings

    // Creep lane following: index of the waypoint the creep is walking toward.
    int laneIndex = 0;

    // Hero-only progression / control.
    int level = 1;
    float xp = 0.f;
    float xpToLevel = 100.f;
    int gold = 0;
    Vec2 moveOrder;      // right-click destination
    bool hasMoveOrder = false;
    float respawnTimer = 0.f; // >0 means dead & waiting to respawn
    float shieldTimer = 0.f;  // W buff remaining
    float qCooldown = 0.f;
    float wCooldown = 0.f;
    bool isPlayer = false;

    int target = -1;     // index into units vector, or -1

    bool alive() const { return hp > 0.f && respawnTimer <= 0.f; }
    bool building() const { return kind == UnitKind::Tower || kind == UnitKind::Ancient; }
};

// Straight-line projectile (hero bolt / ranged auto-attacks).
struct Projectile {
    Team team = Team::Radiant;
    Vec2 pos;
    Vec2 vel;
    float damage = 0.f;
    float radius = 6.f;
    float life = 2.f;    // seconds before it fizzles
    bool splash = false; // ability bolt hits an area on impact
    sf::Color color = sf::Color::White;
    bool dead = false;
};

// Brief instant-attack visual (a fading line from attacker to victim).
struct Beam {
    Vec2 a, b;
    sf::Color color;
    float life = 0.15f;
};

// Damage / gold numbers that float up and fade.
struct FloatText {
    Vec2 pos;
    std::string text;
    sf::Color color;
    float life = 0.9f;
    float maxLife = 0.9f;
};

} // namespace mb
