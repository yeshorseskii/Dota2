#include "Defs.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace game {

Defs::Defs() {
    // Built-in defaults (used when no data file is present). These mirror the
    // original hardcoded values so the game works even without characters.txt.
    hero = {"Hero", 480, 34, 11, 0.9f, 20, 1.6f, 3.2f,
            60, 3, 6, 80, 3, 9, 55, 6};
    creep = {"Creep", 120, 12, 6, 1.0f, 9, 1.1f, 2.2f,
             0, 0, 0, 0, 0, 0, 0, 0};
    tower = {"Tower", 900, 60, 20, 1.1f, 0, 2.6f, 8.0f,
             0, 0, 0, 0, 0, 0, 0, 0};
    ancient = {"Ancient", 2600, 45, 18, 1.2f, 0, 4.5f, 10.0f,
               0, 0, 0, 0, 0, 0, 0, 0};
}

UnitDef& Defs::byIndex(int i) {
    switch (i) {
        case 1: return creep;
        case 2: return tower;
        case 3: return ancient;
        default: return hero;
    }
}
const char* Defs::nameOf(int i) {
    switch (i) { case 1: return "Creep"; case 2: return "Tower"; case 3: return "Ancient"; default: return "Hero"; }
}

namespace {
std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
void assign(UnitDef& d, const std::string& k, float v) {
    if (k == "hp") d.hp = v;
    else if (k == "damage") d.damage = v;
    else if (k == "range") d.range = v;
    else if (k == "attack_interval") d.attackInterval = v;
    else if (k == "move_speed") d.moveSpeed = v;
    else if (k == "radius") d.radius = v;
    else if (k == "height") d.height = v;
    else if (k == "q_damage") d.qDamage = v;
    else if (k == "q_cooldown") d.qCooldown = v;
    else if (k == "q_splash") d.qSplash = v;
    else if (k == "w_heal") d.wHeal = v;
    else if (k == "w_shield") d.wShield = v;
    else if (k == "w_cooldown") d.wCooldown = v;
    else if (k == "level_hp") d.levelHp = v;
    else if (k == "level_damage") d.levelDmg = v;
}
Color parseColor(const std::string& v) {
    int r = 200, g = 200, b = 200;
    std::sscanf(v.c_str(), "%d,%d,%d", &r, &g, &b);
    return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
}
} // namespace

bool Defs::Load(const std::string& file) {
    std::ifstream in(file);
    if (!in) return false;
    std::string line, section;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trim(line.substr(0, eq));
        std::string v = trim(line.substr(eq + 1));
        if (section == "colors") {
            if (k == "radiant") radiant = parseColor(v);
            else if (k == "dire") dire = parseColor(v);
            continue;
        }
        float fv = 0.f;
        try { fv = std::stof(v); } catch (...) { if (k == "name") { byIndex(section=="creep"?1:section=="tower"?2:section=="ancient"?3:0).name = v; } continue; }
        if (section == "hero") assign(hero, k, fv);
        else if (section == "creep") assign(creep, k, fv);
        else if (section == "tower") assign(tower, k, fv);
        else if (section == "ancient") assign(ancient, k, fv);
    }
    return true;
}

bool Defs::Save(const std::string& file) const {
    std::ofstream out(file);
    if (!out) return false;
    auto writeUnit = [&](const char* sec, const UnitDef& d, bool hero) {
        out << "[" << sec << "]\n";
        out << "name = " << d.name << "\n";
        out << "hp = " << d.hp << "\n";
        out << "damage = " << d.damage << "\n";
        out << "range = " << d.range << "\n";
        out << "attack_interval = " << d.attackInterval << "\n";
        out << "move_speed = " << d.moveSpeed << "\n";
        out << "radius = " << d.radius << "\n";
        out << "height = " << d.height << "\n";
        if (hero) {
            out << "q_damage = " << d.qDamage << "\n";
            out << "q_cooldown = " << d.qCooldown << "\n";
            out << "q_splash = " << d.qSplash << "\n";
            out << "w_heal = " << d.wHeal << "\n";
            out << "w_shield = " << d.wShield << "\n";
            out << "w_cooldown = " << d.wCooldown << "\n";
            out << "level_hp = " << d.levelHp << "\n";
            out << "level_damage = " << d.levelDmg << "\n";
        }
        out << "\n";
    };
    out << "# Mini MOBA character definitions. Edit here or use the in-game\n"
           "# editor (Tab). Colors are R,G,B (0-255).\n\n";
    writeUnit("hero", hero, true);
    writeUnit("creep", creep, false);
    writeUnit("tower", tower, false);
    writeUnit("ancient", ancient, false);
    out << "[colors]\n";
    out << "radiant = " << (int)radiant.r << "," << (int)radiant.g << "," << (int)radiant.b << "\n";
    out << "dire = " << (int)dire.r << "," << (int)dire.g << "," << (int)dire.b << "\n";
    return true;
}

} // namespace game
