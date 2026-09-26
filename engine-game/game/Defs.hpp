// Data-driven unit definitions. Stats/appearance for each unit type live here
// (loaded from and saved to a text file), so the character editor can tune them
// at runtime and new units pick up the changes without recompiling.
#pragma once
#include "raylib.h"
#include <string>

namespace game {

struct UnitDef {
    std::string name = "unit";
    float hp = 100, damage = 10, range = 6, attackInterval = 1;
    float moveSpeed = 0, radius = 1, height = 2;
    // hero-only ability + leveling params (ignored by other unit types)
    float qDamage = 60, qCooldown = 3, qSplash = 6;
    float wHeal = 80, wShield = 3, wCooldown = 9;
    float levelHp = 55, levelDmg = 6;
};

struct Defs {
    UnitDef hero, creep, tower, ancient;
    Color radiant{86, 196, 112, 255};
    Color dire{214, 84, 84, 255};

    Defs();                                   // built-in defaults
    bool Load(const std::string& file);       // override defaults from file
    bool Save(const std::string& file) const; // write current values

    Color teamColor(bool radiantTeam) const { return radiantTeam ? radiant : dire; }
    UnitDef& byIndex(int i);                  // 0=hero 1=creep 2=tower 3=ancient
    static const char* nameOf(int i);
};

} // namespace game
