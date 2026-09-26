// Implementation of the MOBA game module and all its entities, built on the
// engine (Entity/World/ticks/cvars/console).
#include "MobaGame.hpp"
#include "Entities.hpp"
#include "engine/ConVar.hpp"
#include "third_party/raygui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace game {

MobaGame* g_game = nullptr;

// ---- small color + model helpers for the low-poly unit models --------------
static Color shade(Color c, float f) {
    return Color{(unsigned char)eng::Clampf(c.r * f, 0, 255),
                 (unsigned char)eng::Clampf(c.g * f, 0, 255),
                 (unsigned char)eng::Clampf(c.b * f, 0, 255), c.a};
}
static Color mix(Color a, Color b, float t) {
    return Color{(unsigned char)(a.r + (b.r - a.r) * t),
                 (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), 255};
}
static void box(Vector3 c, float w, float h, float d, Color col) { DrawCube(c, w, h, d, col); }
static void boxEdge(Vector3 c, float w, float h, float d, Color col) { DrawCubeWires(c, w, h, d, col); }

// Push a local frame at (pos on ground) rotated by yaw so a model can be drawn
// in local space (origin at the feet, +Z forward, +Y up).
static void beginModel(Vector3 pos, float yawDeg) {
    rlPushMatrix();
    rlTranslatef(pos.x, 0.f, pos.z);
    rlRotatef(yawDeg, 0.f, 1.f, 0.f);
}
static void endModel() { rlPopMatrix(); }

static void updateYaw(CombatEntity& u, Vector3 oldPos) {
    float dx = u.pos.x - oldPos.x, dz = u.pos.z - oldPos.z;
    if (dx * dx + dz * dz > 1e-6f) u.yaw = atan2f(dx, dz) * RAD2DEG;
}

// Draw a lit primitive in the current model frame. sph = ellipsoid (sx,sy,sz),
// cyl/cone grow +Y from base. Kept short since models call it a lot.
static void P(eng::Prim p, Vector3 pos, Vector3 s, Color c) { g_game->scene().Draw(p, pos, s, c); }
static void sph(Vector3 pos, Vector3 s, Color c) { P(eng::Prim::Sphere, pos, s, c); }
static void cyl(Vector3 pos, float rad, float hgt, Color c) { P(eng::Prim::Cylinder, pos, {rad, hgt, rad}, c); }
static void cone(Vector3 pos, float rad, float hgt, Color c) { P(eng::Prim::Cone, pos, {rad, hgt, rad}, c); }

// Copy tunable stats from a UnitDef onto a live combat entity.
static void applyDef(CombatEntity& u, const UnitDef& d) {
    u.maxHp = d.hp;
    if (u.hp > u.maxHp) u.hp = u.maxHp;
    u.attackDamage = d.damage;
    u.attackRange = d.range;
    u.attackInterval = d.attackInterval;
    u.moveSpeed = d.moveSpeed;
    u.radius = d.radius;
    u.height = d.height;
}

// ------------------------------------------------------------- cvars --------
static eng::ConVar sv_wave_interval("sv_wave_interval", 22.f, "seconds between creep waves");
static eng::ConVar sv_creeps_per_wave("sv_creeps_per_wave", 4.f, "creeps spawned per wave per side");
static eng::ConVar sv_damage_scale("sv_damage_scale", 1.f, "global damage multiplier (cheat)");
static eng::ConVar cl_cam_dist("cl_cam_dist", 54.f, "camera follow distance");

// ------------------------------------------------------------- colors -------
Color TeamColor(Team t) {
    if (g_game) return g_game->defs().teamColor(t == Team::Radiant);
    return t == Team::Radiant ? Color{86, 196, 112, 255} : Color{214, 84, 84, 255};
}

// =================================================================
// CombatEntity
// =================================================================
void CombatEntity::combatTick(float dt) {
    if (attackTimer > 0.f) attackTimer -= dt;
    tryAutoAttack();
}

void CombatEntity::tryAutoAttack() {
    if (attackTimer > 0.f) return;
    CombatEntity* t = g_game->NearestEnemy(team, pos, attackRange);
    if (!t) return;
    t->takeDamage(attackDamage, team);
    attackTimer = attackInterval;
    Vector3 a = pos; a.y = height * 0.5f;
    Vector3 b = t->pos; b.y = t->height * 0.5f;
    g_game->SpawnBeam(a, b, team == Team::Radiant ? Color{180, 230, 255, 255}
                                                  : Color{255, 190, 180, 255});
}

void CombatEntity::takeDamage(float dmg, Team from) {
    if (!alive()) return;
    float applied = dmg * damageTakenMult() * sv_damage_scale.GetFloat();
    hp -= applied;
    g_game->SpawnText(Vector3Add(pos, {0, height + 0.5f, 0}),
                      TextFormat("%d", (int)applied), Color{255, 235, 120, 255}, 0.7f);
    if (hp <= 0.f) {
        hp = 0.f;
        g_game->OnKill(*this, from);
        onDeath(from);
        if (!persistent()) Remove();
    }
}

void CombatEntity::drawHpBar() {
    if (hp <= 0.f) return;
    float yOff = height + (persistent() ? 2.4f : 1.4f);
    Vector2 sp = GetWorldToScreen(Vector3{pos.x, yOff, pos.z}, g_game->Cam());
    int W = GetScreenWidth(), H = GetScreenHeight();
    if (sp.x < -60 || sp.x > W + 60 || sp.y < -60 || sp.y > H + 60) return;
    float w = barWidth, h = 6.f;
    float frac = eng::Clampf(hp / maxHp, 0.f, 1.f);
    DrawRectangle((int)(sp.x - w / 2), (int)(sp.y - h), (int)w, (int)h, Color{15, 15, 15, 220});
    DrawRectangle((int)(sp.x - w / 2), (int)(sp.y - h), (int)(w * frac), (int)h, TeamColor(team));
}

// =================================================================
// Hero
// =================================================================
void Hero::Spawn() {
    classname = "hero"; solid = true; barWidth = 52.f;
    applyDef(*this, g_game->defs().hero);
    hp = maxHp;
    spawnPos = pos; moveOrder = pos;
}

void Hero::addXp(float amount) {
    const UnitDef& d = g_game->defs().hero;
    xp += amount;
    while (xp >= xpToLevel) {
        xp -= xpToLevel; level += 1; xpToLevel *= 1.35f;
        maxHp += d.levelHp; attackDamage += d.levelDmg; hp = std::min(maxHp, hp + 60.f);
        g_game->SpawnText(Vector3Add(pos, {0, height + 1.f, 0}), "LEVEL UP!",
                          Color{255, 215, 0, 255}, 1.3f);
    }
}

void Hero::castQ(Vector3 target) {
    if (qCd > 0.f) return;
    const UnitDef& d = g_game->defs().hero;
    Vector3 dir = Vector3Normalize(Vector3Subtract(target, pos));
    if (Vector3Length(dir) < 0.1f) dir = {1, 0, 0};
    auto* p = g_game->World().Create<Projectile>();
    p->team = team; p->pos = pos; p->pos.y = 1.5f;
    p->vel = Vector3Scale(dir, 70.f); p->damage = d.qDamage; p->splashRadius = d.qSplash;
    p->color = team == Team::Radiant ? Color{120, 200, 255, 255} : Color{255, 150, 120, 255};
    qCd = d.qCooldown;
}

void Hero::castW() {
    if (wCd > 0.f) return;
    const UnitDef& d = g_game->defs().hero;
    hp = std::min(maxHp, hp + d.wHeal); shieldTimer = d.wShield; wCd = d.wCooldown;
    g_game->SpawnText(Vector3Add(pos, {0, height, 0}), "+shield", Color{120, 255, 160, 255}, 1.f);
}

void Hero::onDeath(Team /*killer*/) {
    respawnTimer = 6.f + level * 0.8f;
    hasMoveOrder = false;
}

void Hero::Update(float dt) {
    if (qCd > 0.f) qCd -= dt;
    if (wCd > 0.f) wCd -= dt;
    if (shieldTimer > 0.f) shieldTimer -= dt;
    if (respawnTimer > 0.f) {
        respawnTimer -= dt;
        if (respawnTimer <= 0.f) { respawnTimer = 0.f; hp = maxHp; pos = spawnPos; hasMoveOrder = false; }
        return;
    }
    combatTick(dt);

    Vector3 o = pos;
    if (isPlayer) {
        if (hasMoveOrder) {
            pos = eng::MoveToward(pos, moveOrder, moveSpeed * dt);
            updateYaw(*this, o);
            if (eng::Dist(pos, moveOrder) < 0.6f) hasMoveOrder = false;
        } else {
            // face the nearest enemy when standing and fighting
            if (CombatEntity* e = g_game->NearestEnemy(team, pos, attackRange + 4.f))
                yaw = atan2f(e->pos.x - pos.x, e->pos.z - pos.z) * RAD2DEG;
        }
        return;
    }

    // enemy AI: retreat when low, else push and fight along the lane.
    if (hp < maxHp * 0.30f) {
        pos = eng::MoveToward(pos, spawnPos, moveSpeed * dt);
        updateYaw(*this, o);
        castW();
        return;
    }
    CombatEntity* aggro = g_game->NearestEnemy(team, pos, 34.f);
    if (aggro) {
        if (qCd <= 0.f && eng::Dist(pos, aggro->pos) < 36.f) castQ(aggro->pos);
        if (eng::Dist(pos, aggro->pos) > attackRange * 0.9f) {
            pos = eng::MoveToward(pos, aggro->pos, moveSpeed * dt);
            updateYaw(*this, o);
        } else {
            yaw = atan2f(aggro->pos.x - pos.x, aggro->pos.z - pos.z) * RAD2DEG;
        }
    } else {
        pos = eng::MoveToward(pos, g_game->LaneWp(g_game->LaneCount() - 3), moveSpeed * dt);
        updateYaw(*this, o);
    }
}

void Hero::Render() {
    Color col = TeamColor(team);
    if (respawnTimer > 0.f) {
        Vector3 s = spawnPos; s.y = 0.1f;
        DrawCircle3D(s, radius + 0.6f, Vector3{1, 0, 0}, 90.f, Color{col.r, col.g, col.b, 120});
        return;
    }
    // ground markers
    Vector3 ring = pos; ring.y = 0.1f;
    DrawCircle3D(ring, radius + 1.0f, Vector3{1, 0, 0}, 90.f,
                 isPlayer ? RAYWHITE : shade(col, 0.7f));
    if (isPlayer && hasMoveOrder) {
        Vector3 m = moveOrder; m.y = 0.1f;
        DrawCircle3D(m, 0.8f, Vector3{1, 0, 0}, 90.f, Color{90, 200, 110, 255});
    }

    const float r = radius, h = height;
    Color leg = shade(col, 0.5f);
    Color skin = mix(col, Color{240, 224, 200, 255}, 0.75f);
    Color metal = Color{205, 210, 220, 255};
    Color trim = shade(col, 0.8f);

    beginModel(pos, yaw);
    // legs (tapered) + boots
    cyl({-0.38f * r, 0.0f, 0}, 0.2f * r, 0.5f * h, leg);
    cyl({0.38f * r, 0.0f, 0}, 0.2f * r, 0.5f * h, leg);
    sph({-0.38f * r, 0.03f * h, 0.08f * r}, {0.24f * r, 0.16f * r, 0.34f * r}, shade(leg, 0.7f));
    sph({0.38f * r, 0.03f * h, 0.08f * r}, {0.24f * r, 0.16f * r, 0.34f * r}, shade(leg, 0.7f));
    // torso (ellipsoid) + chest plate
    sph({0, 0.66f * h, 0}, {0.62f * r, 0.34f * h, 0.44f * r}, col);
    sph({0, 0.7f * h, 0.16f * r}, {0.5f * r, 0.26f * h, 0.32f * r}, trim);
    // shoulders + arms
    sph({-0.62f * r, 0.84f * h, 0}, {0.26f * r, 0.24f * r, 0.26f * r}, trim);
    sph({0.62f * r, 0.84f * h, 0}, {0.26f * r, 0.24f * r, 0.26f * r}, trim);
    cyl({-0.66f * r, 0.4f * h, 0}, 0.15f * r, 0.42f * h, leg);
    cyl({0.66f * r, 0.4f * h, 0}, 0.15f * r, 0.42f * h, leg);
    // neck, head, helmet crest
    sph({0, 0.98f * h, 0}, {0.28f * r, 0.32f * r, 0.28f * r}, skin);
    sph({0, 1.0f * h, 0.12f * r}, {0.3f * r, 0.26f * r, 0.24f * r}, trim); // helmet
    cone({0, 1.12f * h, 0}, 0.14f * r, 0.28f * h, mix(col, RAYWHITE, 0.3f));
    // sword: blade + tip + crossguard, held in right hand
    Color blade = metal;
    cyl({0.78f * r, 0.35f * h, 0.28f * r}, 0.05f * r, 0.85f * h, blade);
    cone({0.78f * r, 1.2f * h, 0.28f * r}, 0.06f * r, 0.16f * h, blade);
    sph({0.78f * r, 0.33f * h, 0.28f * r}, {0.22f * r, 0.05f * h, 0.1f * r}, trim);
    // round shield on left arm (glows while W shield is active)
    Color sh = shieldTimer > 0.f ? Color{130, 225, 255, 255} : mix(col, metal, 0.4f);
    sph({-0.8f * r, 0.6f * h, 0.14f * r}, {0.12f * r, 0.34f * r, 0.4f * r}, sh);
    endModel();
}

void Hero::Render2D() {
    if (respawnTimer > 0.f) {
        Vector2 sp = GetWorldToScreen(Vector3{spawnPos.x, height, spawnPos.z}, g_game->Cam());
        DrawText(TextFormat("%d", (int)respawnTimer + 1), (int)sp.x - 5, (int)sp.y, 20, RAYWHITE);
        return;
    }
    drawHpBar();
    Vector2 sp = GetWorldToScreen(Vector3{pos.x, height + 2.4f, pos.z}, g_game->Cam());
    DrawText(TextFormat("%d", level), (int)(sp.x - barWidth / 2 - 16), (int)(sp.y - 7), 14, RAYWHITE);
}

// =================================================================
// Creep
// =================================================================
void Creep::Spawn() {
    classname = "creep"; solid = true; barWidth = 28.f;
    applyDef(*this, g_game->defs().creep);
    hp = maxHp;
}

void Creep::Update(float dt) {
    combatTick(dt);
    // move along the lane unless an enemy is close enough to fight
    if (CombatEntity* e = g_game->NearestEnemy(team, pos, attackRange + 2.f)) {
        yaw = atan2f(e->pos.x - pos.x, e->pos.z - pos.z) * RAD2DEG;
        return;
    }
    Vector3 wp = g_game->LaneWp(laneIndex);
    Vector3 o = pos;
    pos = eng::MoveToward(pos, wp, moveSpeed * dt);
    updateYaw(*this, o);
    if (eng::Dist(pos, wp) < 1.6f) {
        laneIndex += (team == Team::Radiant) ? 1 : -1;
        laneIndex = (int)eng::Clampf((float)laneIndex, 0.f, (float)g_game->LaneCount() - 1);
    }
}

void Creep::Render() {
    Color col = TeamColor(team);
    const float r = radius, h = height;
    Color leg = shade(col, 0.5f);
    Color belly = mix(col, RAYWHITE, 0.25f);
    beginModel(pos, yaw);
    // four stubby legs
    for (float sx : {-0.45f, 0.45f})
        for (float sz : {-0.4f, 0.45f})
            cyl({sx * r, 0.f, sz * r}, 0.16f * r, 0.28f * h, leg);
    // rounded body + belly
    sph({0, 0.55f * h, 0}, {0.62f * r, 0.4f * h, 0.72f * r}, col);
    sph({0, 0.42f * h, 0.1f * r}, {0.5f * r, 0.28f * h, 0.55f * r}, belly);
    // head poking forward + snout
    sph({0, 0.66f * h, 0.7f * r}, {0.42f * r, 0.36f * r, 0.42f * r}, mix(col, RAYWHITE, 0.1f));
    cone({0, 0.62f * h, 1.05f * r}, 0.18f * r, 0.3f * r, shade(col, 1.1f));
    // ears (cones) + eyes
    cone({-0.22f * r, 0.9f * h, 0.65f * r}, 0.1f * r, 0.28f * r, leg);
    cone({0.22f * r, 0.9f * h, 0.65f * r}, 0.1f * r, 0.28f * r, leg);
    sph({-0.2f * r, 0.72f * h, 0.98f * r}, {0.08f * r, 0.08f * r, 0.08f * r}, Color{15, 15, 15, 255});
    sph({0.2f * r, 0.72f * h, 0.98f * r}, {0.08f * r, 0.08f * r, 0.08f * r}, Color{15, 15, 15, 255});
    endModel();
}

// =================================================================
// Tower
// =================================================================
void Tower::Spawn() {
    classname = "tower"; solid = true; barWidth = 56.f;
    applyDef(*this, g_game->defs().tower);
    hp = maxHp;
}

void Tower::Update(float dt) {
    combatTick(dt);
}

void Tower::Render() {
    Color col = TeamColor(team);
    const float r = radius, h = height;
    Color stone = mix(Color{125, 125, 135, 255}, col, 0.3f);
    Color dark = shade(stone, 0.7f);
    beginModel(pos, 0.f);
    // stepped round base
    cyl({0, 0.0f, 0}, 1.05f * r, 0.16f * h, dark);
    cyl({0, 0.14f * h, 0}, 0.9f * r, 0.62f * h, stone);
    // battlement ring + crenellations
    cyl({0, 0.74f * h, 0}, 0.98f * r, 0.1f * h, dark);
    for (int i = 0; i < 8; ++i) {
        float a = i / 8.f * 2.f * PI;
        sph({cosf(a) * 0.9f * r, 0.84f * h, sinf(a) * 0.9f * r},
            {0.16f * r, 0.12f * h, 0.16f * r}, stone);
    }
    // conical roof
    cone({0, 0.84f * h, 0}, 0.8f * r, 0.4f * h, shade(col, 0.85f));
    // glowing crystal on top (bobs)
    float bob = 0.05f * h * sinf((float)GetTime() * 2.f);
    Color glow = mix(col, RAYWHITE, 0.4f);
    sph({0, 1.12f * h + bob, 0}, {0.34f * r, 0.5f * r, 0.34f * r}, glow);
    endModel();
}

// =================================================================
// Ancient
// =================================================================
void Ancient::Spawn() {
    classname = "ancient"; solid = true; barWidth = 90.f;
    applyDef(*this, g_game->defs().ancient);
    hp = maxHp;
}

void Ancient::Update(float dt) {
    combatTick(dt);
}

void Ancient::onDeath(Team killer) {
    g_game->SetWin(killer == Team::Radiant ? Phase::RadiantWin : Phase::DireWin);
}

void Ancient::Render() {
    bool dead = hp <= 0.f;
    Color col = dead ? Color{70, 70, 70, 255} : TeamColor(team);
    const float r = radius, h = height;
    Color stone = mix(Color{110, 110, 120, 255}, col, 0.4f);

    // glow ring on the ground
    Vector3 ring = pos; ring.y = 0.1f;
    DrawCircle3D(ring, r + 1.5f, Vector3{1, 0, 0}, 90.f, shade(col, 0.8f));

    beginModel(pos, 0.f);
    // round tiered temple base
    cyl({0, 0.0f, 0}, 1.15f * r, 0.16f * h, shade(stone, 0.7f));
    cyl({0, 0.15f * h, 0}, 0.92f * r, 0.16f * h, stone);
    cyl({0, 0.30f * h, 0}, 0.68f * r, 0.14f * h, shade(stone, 1.1f));
    // ring of pillars
    for (int i = 0; i < 8; ++i) {
        float a = i / 8.f * 2.f * PI;
        cyl({cosf(a) * 0.8f * r, 0.42f * h, sinf(a) * 0.8f * r}, 0.1f * r, 0.34f * h, shade(stone, 0.85f));
    }
    // roof ring
    cyl({0, 0.76f * h, 0}, 0.72f * r, 0.06f * h, shade(stone, 0.75f));
    endModel();

    // floating rotating crystal core (lit + a bright unlit core for glow)
    float t = (float)GetTime();
    float bob = 0.5f + 0.25f * sinf(t * 1.5f);
    Vector3 core{pos.x, h * 0.95f + bob, pos.z};
    Color glow = dead ? DARKGRAY : mix(col, RAYWHITE, 0.55f);
    rlPushMatrix();
    rlTranslatef(core.x, core.y, core.z);
    rlRotatef(t * 40.f, 0, 1, 0);
    P(eng::Prim::Cone, {0, 0, 0}, {1.5f, 1.6f, 1.5f}, glow);            // upper facet
    P(eng::Prim::Cone, {0, 0, 0}, {1.5f, -1.6f, 1.5f}, shade(glow, 0.8f)); // lower facet
    rlPopMatrix();
    if (!dead) DrawSphere(core, 0.5f, mix(col, RAYWHITE, 0.8f));        // bright center
}

// =================================================================
// Projectile / Beam / FloatText
// =================================================================
void Projectile::Update(float dt) {
    life -= dt;
    pos = Vector3Add(pos, Vector3Scale(vel, dt));
    if (life <= 0.f) Remove();
}

void Projectile::OnTouch(eng::Entity& other) {
    auto* c = dynamic_cast<CombatEntity*>(&other);
    if (!c || !c->alive() || c->team == team) return;
    if (splashRadius > 0.f) g_game->AreaDamage(team, pos, splashRadius, damage);
    else c->takeDamage(damage, team);
    Remove();
}

void Projectile::Render() { DrawSphere(pos, radius + 0.3f, color); }

void Beam::Update(float dt) { life -= dt; if (life <= 0.f) Remove(); }
void Beam::Render() {
    unsigned char al = (unsigned char)(255 * eng::Clampf(life / maxLife, 0.f, 1.f));
    DrawLine3D(a, b, Color{color.r, color.g, color.b, al});
}

void FloatText::Update(float dt) { life -= dt; pos.y += 3.f * dt; if (life <= 0.f) Remove(); }
void FloatText::Render2D() {
    Vector2 sp = GetWorldToScreen(pos, g_game->Cam());
    unsigned char al = (unsigned char)(255 * eng::Clampf(life / maxLife, 0.f, 1.f));
    DrawText(text.c_str(), (int)sp.x - MeasureText(text.c_str(), 18) / 2, (int)sp.y, 18,
             Color{color.r, color.g, color.b, al});
}

// =================================================================
// MobaGame
// =================================================================
static eng::ConCommand cc_restart("restart", [](const eng::ConCommand::Args&) {
    if (g_game) { g_game->Restart(); eng::Con::Print("  match restarted"); }
}, "restart the match");

static eng::ConCommand cc_win("win", [](const eng::ConCommand::Args& a) {
    if (!g_game) return;
    Team t = (!a.empty() && a[0] == "dire") ? Team::Dire : Team::Radiant;
    g_game->ForceWin(t);
    eng::Con::Print(a.empty() ? "  win radiant|dire" : "  forced win");
}, "force a winner: win radiant | win dire");

void MobaGame::SpawnBeam(Vector3 a, Vector3 b, Color c) {
    auto* e = World().Create<Beam>();
    e->a = a; e->b = b; e->color = c;
}
void MobaGame::SpawnText(Vector3 at, const std::string& s, Color c, float life) {
    auto* e = World().Create<FloatText>();
    e->pos = at; e->text = s; e->color = c; e->life = e->maxLife = life;
}

Vector3 MobaGame::LaneWp(int step) const {
    int s = (int)eng::Clampf((float)step, 0.f, (float)lane_.size() - 1);
    return lane_[s];
}

CombatEntity* MobaGame::NearestEnemy(Team team, Vector3 pos, float range) {
    CombatEntity* best = nullptr;
    float bestD = range;
    for (eng::Entity* e : World().FindInRadius(pos, range + 6.f)) {
        auto* c = dynamic_cast<CombatEntity*>(e);
        if (!c || !c->alive() || c->team == team) continue;
        float d = eng::Dist(pos, c->pos) - c->radius;
        if (d <= bestD) { bestD = d; best = c; }
    }
    return best;
}

void MobaGame::AreaDamage(Team from, Vector3 center, float radius, float dmg) {
    for (eng::Entity* e : World().FindInRadius(center, radius + 4.f)) {
        auto* c = dynamic_cast<CombatEntity*>(e);
        if (!c || !c->alive() || c->team == from) continue;
        if (eng::Dist(center, c->pos) <= radius + c->radius) c->takeDamage(dmg, from);
    }
}

void MobaGame::OnKill(CombatEntity& victim, Team killer) {
    int bounty = 0, xp = 0;
    std::string cn = victim.classname;
    if (cn == "creep")       { bounty = 40;  xp = 45; }
    else if (cn == "hero")   { bounty = 200; xp = 120; }
    else if (cn == "tower")  { bounty = 250; xp = 90; }

    Hero* best = nullptr; float bestD = 1e9f;
    for (eng::Entity* e : World().FindInRadius(victim.pos, 90.f)) {
        auto* h = dynamic_cast<Hero*>(e);
        if (!h || h->team != killer) continue;
        if (h->alive()) h->addXp((float)xp);
        float d = eng::Dist(h->pos, victim.pos);
        if (d < bestD) { bestD = d; best = h; }
    }
    if (best && bounty > 0) {
        best->gold += bounty;
        SpawnText(Vector3Add(victim.pos, {0, victim.height, 0}),
                  TextFormat("+%d", bounty), Color{255, 215, 0, 255}, 0.9f);
    }
}

void MobaGame::spawnMap() {
    lane_ = {
        {16, 0, 68}, {34, 0, 60}, {54, 0, 50}, {74, 0, 40},
        {94, 0, 30}, {112, 0, 20}, {124, 0, 12},
    };
    radiantAncient_ = World().Create<Ancient>();
    radiantAncient_->team = Team::Radiant; radiantAncient_->pos = lane_.front();
    direAncient_ = World().Create<Ancient>();
    direAncient_->team = Team::Dire; direAncient_->pos = lane_.back();

    auto tower = [&](Team t, Vector3 at) {
        auto* w = World().Create<Tower>(); w->team = t; w->pos = at;
    };
    tower(Team::Radiant, lane_[2]); tower(Team::Radiant, lane_[1]);
    tower(Team::Dire, lane_[4]); tower(Team::Dire, lane_[5]);

    player_ = World().Create<Hero>();
    player_->team = Team::Radiant; player_->isPlayer = true;
    player_->pos = Vector3Add(lane_.front(), Vector3{7, 0, 7});
    player_->spawnPos = player_->pos; player_->moveOrder = player_->pos;

    enemy_ = World().Create<Hero>();
    enemy_->team = Team::Dire; enemy_->isPlayer = false;
    enemy_->pos = Vector3Add(lane_.back(), Vector3{-7, 0, -7});
    enemy_->spawnPos = enemy_->pos; enemy_->moveOrder = enemy_->pos;
}

void MobaGame::spawnWave() {
    ++waveCount_;
    int n = std::max(1, sv_creeps_per_wave.GetInt());
    for (int i = 0; i < n; ++i) {
        Vector3 j{(float)(rand() % 30 - 15) / 6.f, 0, (float)(rand() % 30 - 15) / 6.f};
        auto* rc = World().Create<Creep>();
        rc->team = Team::Radiant; rc->pos = Vector3Add(LaneWp(0), j); rc->laneIndex = 1;
        auto* dc = World().Create<Creep>();
        dc->team = Team::Dire; dc->pos = Vector3Add(LaneWp(LaneCount() - 1), j);
        dc->laneIndex = LaneCount() - 2;
    }
}

void MobaGame::reset() {
    World().Clear();
    phase_ = Phase::Playing; waveTimer_ = 3.f; waveCount_ = 0;
    spawnMap();
}

void MobaGame::loadDefs() {
    const char* cands[] = {"characters.txt", "data/characters.txt",
                           "../data/characters.txt", "engine-game/data/characters.txt"};
    for (const char* p : cands) {
        if (defs_.Load(p)) { defsPath_ = p; return; }
    }
    defsPath_ = "characters.txt"; // none found: keep defaults, Save creates it here
}

void MobaGame::applyDefsToLiveUnits() {
    World().ForEach([&](eng::Entity& e) {
        if (auto* h = dynamic_cast<Hero*>(&e))         applyDef(*h, defs_.hero);
        else if (auto* c = dynamic_cast<Creep*>(&e))   applyDef(*c, defs_.creep);
        else if (auto* t = dynamic_cast<Tower*>(&e))   applyDef(*t, defs_.tower);
        else if (auto* a = dynamic_cast<Ancient*>(&e)) applyDef(*a, defs_.ancient);
    });
}

MobaGame::~MobaGame() {
    if (groundReady_) UnloadModel(groundModel_); // also frees the diffuse texture
}

void MobaGame::makeGround() {
    // procedural tiled grass: checker base + faint perlin noise overlay
    Image img = GenImageChecked(512, 512, 48, 48, Color{33, 41, 33, 255}, Color{28, 35, 28, 255});
    Image noise = GenImagePerlinNoise(512, 512, 0, 0, 5.0f);
    ImageDraw(&img, noise, {0, 0, 512, 512}, {0, 0, 512, 512}, Color{72, 98, 72, 45});
    UnloadImage(noise);
    groundTex_ = LoadTextureFromImage(img);
    SetTextureFilter(groundTex_, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
    groundModel_ = LoadModelFromMesh(GenMeshPlane(160, 100, 1, 1));
    groundModel_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = groundTex_;
    engine_->scene().ApplyShader(groundModel_);   // ground receives lighting too
    groundReady_ = true;
}

void MobaGame::OnInit(eng::Engine& e) {
    engine_ = &e; g_game = this;
    e.background = Color{22, 26, 34, 255};
    e.scene().SetSun({-0.55f, -1.0f, -0.35f}, Color{255, 246, 228, 255}, Color{140, 146, 158, 255});
    makeGround();
    loadDefs();
    reset();
    screen_ = Screen::Menu;              // start on the main menu (match runs behind it)
    if (std::getenv("MOBA_SHOT_EDITOR")) editorOpen_ = true;
    // For gameplay screenshots, auto-start into the match unless a menu shot is asked.
    if (std::getenv("MOBA_SHOT_AT") && !std::getenv("MOBA_SHOT_MENU")) screen_ = Screen::Game;
}

bool MobaGame::OnEscape() {
    if (screen_ == Screen::Menu) return true;         // quit from the menu
    if (screen_ == Screen::Game) {
        if (editorOpen_) { editorOpen_ = false; return false; }
        screen_ = Screen::Paused; return false;       // open pause menu
    }
    screen_ = Screen::Game; return false;             // resume from pause
}

void MobaGame::OnInput(eng::Engine& e) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f)
        cl_cam_dist.SetFloat(eng::Clampf(cl_cam_dist.GetFloat() - wheel * 3.f, 20.f, 80.f));

    if (screen_ != Screen::Game) return;   // menu / pause handle input via buttons
    if (IsKeyPressed(KEY_TAB)) editorOpen_ = !editorOpen_;

    if (phase_ != Phase::Playing) { if (IsKeyPressed(KEY_R)) reset(); return; }
    if (!player_ || !player_->alive()) return;

    // Don't issue move/ability orders when clicking inside the editor panel.
    bool overPanel = editorOpen_ && GetMousePosition().x < 322.f;
    if (!overPanel && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        player_->moveOrder = e.GroundPoint(); player_->hasMoveOrder = true;
    }
    if (IsKeyPressed(KEY_Q)) player_->castQ(e.GroundPoint());
    if (IsKeyPressed(KEY_W)) player_->castW();
}

void MobaGame::OnFrame(eng::Engine& e, float /*dt*/) {
    Camera3D& c = e.camera();

    // Menu: slow cinematic orbit around the map center.
    if (screen_ == Screen::Menu) {
        Vector3 mid{70, 0, 40};
        float t = (float)GetTime() * 0.12f, d2 = 78.f;
        c.target = Vector3Lerp(c.target, mid, 0.05f);
        Vector3 want{mid.x + sinf(t) * d2, d2 * 0.85f, mid.z + cosf(t) * d2};
        c.position = Vector3Lerp(c.position, want, 0.05f);
        return;
    }

    Vector3 focus = (player_ && player_->alive()) ? player_->pos
                    : (player_ ? player_->spawnPos : Vector3{70, 0, 40});
    if (std::getenv("MOBA_SHOT_AT") && !std::getenv("MOBA_SHOT_HERO")) focus = LaneWp(3);
    float d = cl_cam_dist.GetFloat();
    c.target = Vector3Lerp(c.target, focus, 0.15f);
    c.position = Vector3Lerp(c.position, Vector3Add(c.target, Vector3{0, d, d * 0.62f}), 0.15f);
}

void MobaGame::OnTick(eng::World& w) {
    // Keep the background match alive while sitting on the menu.
    if (screen_ == Screen::Menu && phase_ != Phase::Playing) { reset(); return; }
    if (phase_ != Phase::Playing) return;
    waveTimer_ -= w.tickInterval;
    if (waveTimer_ <= 0.f) { waveTimer_ = sv_wave_interval.GetFloat(); spawnWave(); }
}

void MobaGame::OnRender3D() {
    if (groundReady_) DrawModel(groundModel_, Vector3{70, 0, 40}, 1.0f, WHITE);
    else DrawPlane(Vector3{70, 0, 40}, Vector2{160, 100}, Color{30, 34, 30, 255});
    if (!lane_.empty()) {
        DrawCircle3D(lane_.front(), 22.f, Vector3{1, 0, 0}, 90.f, Color{60, 110, 70, 255});
        DrawCircle3D(lane_.back(), 22.f, Vector3{1, 0, 0}, 90.f, Color{120, 70, 70, 255});
        // dirt path: overlapping tiles along the lane
        Color dirt{74, 64, 50, 255};
        for (int i = 0; i + 1 < (int)lane_.size(); ++i) {
            Vector3 a = lane_[i], b = lane_[i + 1]; a.y = b.y = 0.06f;
            DrawCylinderEx(a, b, 3.4f, 3.4f, 14, dirt);
        }
        for (auto wp : lane_) { wp.y = 0.06f; DrawCylinder(wp, 3.4f, 3.4f, 0.05f, 18, dirt); }
    }
}

void MobaGame::OnRenderHUD() {
    if (screen_ == Screen::Menu) { drawMenu(); return; }

    int W = GetScreenWidth(), H = GetScreenHeight();
    int mm = (int)(engine_->world().time) / 60, ss = (int)(engine_->world().time) % 60;
    DrawText(TextFormat("%02d:%02d   wave %d", mm, ss, waveCount_), W / 2 - 70, 14, 22, RAYWHITE);
    if (radiantAncient_)
        DrawText(TextFormat("Radiant Ancient: %d", (int)radiantAncient_->hp), 16, 14, 18, Color{120, 220, 140, 255});
    if (direAncient_) {
        const char* d = TextFormat("Dire Ancient: %d", (int)direAncient_->hp);
        DrawText(d, W - 16 - MeasureText(d, 18), 14, 18, Color{230, 130, 130, 255});
    }
    if (player_) {
        DrawText(TextFormat("LV %d   HP %d/%d   Gold %d", player_->level,
                            (int)std::max(0.f, player_->hp), (int)player_->maxHp, player_->gold),
                 16, H - 56, 20, RAYWHITE);
        auto cd = [&](const char* k, float v, int x) {
            DrawText(v <= 0.f ? TextFormat("%s: ready", k) : TextFormat("%s: %.1f", k, v),
                     x, H - 28, 18, v <= 0.f ? Color{150, 220, 255, 255} : GRAY);
        };
        cd("Q bolt", player_->qCd, 16);
        cd("W shield", player_->wCd, 170);
    }
    DrawText("Right-click move | Q/W | wheel zoom | ~ console | Tab editor | Esc pause",
             W - 590, H - 28, 16, GRAY);

    if (phase_ != Phase::Playing) {
        DrawRectangle(0, 0, W, H, Color{0, 0, 0, 150});
        const char* msg = phase_ == Phase::RadiantWin ? "RADIANT VICTORY" : "DIRE VICTORY";
        Color c = phase_ == Phase::RadiantWin ? Color{120, 230, 140, 255} : Color{230, 120, 120, 255};
        DrawText(msg, W / 2 - MeasureText(msg, 60) / 2, H / 2 - 60, 60, c);
        const char* sub = "Press R to play again, Esc to quit";
        DrawText(sub, W / 2 - MeasureText(sub, 22) / 2, H / 2 + 20, 22, RAYWHITE);
    }

    if (editorOpen_) drawEditor();
    if (screen_ == Screen::Paused) drawPause();
}

// ------------------------------------------------------------- menus --------
void MobaGame::drawMenu() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Color{10, 12, 14, 150});

    const char* title = "MINI MOBA";
    int ts = 84;
    int tw = MeasureText(title, ts);
    DrawText(title, W / 2 - tw / 2 + 3, 118 + 3, ts, Color{0, 0, 0, 180});   // shadow
    DrawText(title, W / 2 - tw / 2, 118, ts, Color{120, 220, 150, 255});
    const char* sub = "engine build - C++ / raylib";
    DrawText(sub, W / 2 - MeasureText(sub, 22) / 2, 214, 22, Color{180, 190, 200, 255});

    float bw = 300, bh = 52, bx = W / 2.f - bw / 2.f, by = H / 2.f - 30;
    GuiSetStyle(DEFAULT, TEXT_SIZE, 24);
    if (GuiButton({bx, by, bw, bh}, "PLAY")) { reset(); screen_ = Screen::Game; }
    if (GuiButton({bx, by + bh + 14, bw, bh}, "CHARACTER EDITOR")) {
        reset(); screen_ = Screen::Game; editorOpen_ = true;
    }
    if (GuiButton({bx, by + 2 * (bh + 14), bw, bh}, "QUIT")) engine_->Quit();
    GuiSetStyle(DEFAULT, TEXT_SIZE, 10);

    DrawText("Right-click move  |  Q / W abilities  |  Tab editor  |  ~ console",
             W / 2 - 270, H - 40, 18, Color{150, 160, 170, 255});
}

void MobaGame::drawPause() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Color{10, 12, 14, 170});
    const char* t = "PAUSED";
    DrawText(t, W / 2 - MeasureText(t, 60) / 2, H / 2 - 150, 60, RAYWHITE);

    float bw = 280, bh = 48, bx = W / 2.f - bw / 2.f, by = H / 2.f - 60;
    GuiSetStyle(DEFAULT, TEXT_SIZE, 22);
    if (GuiButton({bx, by, bw, bh}, "RESUME")) screen_ = Screen::Game;
    if (GuiButton({bx, by + bh + 12, bw, bh}, "RESTART")) { reset(); screen_ = Screen::Game; }
    if (GuiButton({bx, by + 2 * (bh + 12), bw, bh}, "MAIN MENU")) { reset(); screen_ = Screen::Menu; }
    if (GuiButton({bx, by + 3 * (bh + 12), bw, bh}, "QUIT")) engine_->Quit();
    GuiSetStyle(DEFAULT, TEXT_SIZE, 10);
}

// -------------------------------------------------------- character editor --
// A raygui panel to tune each unit type's stats + the team colors live, then
// Apply to existing units / Save to characters.txt / Reload from it.
void MobaGame::drawEditor() {
    const float x = 10, y = 82, w = 302;
    const float h = GetScreenHeight() - y - 10;
    GuiPanel({x, y, w, h}, "CHARACTER EDITOR");

    float cx = x + 12, cw = w - 24, cy = y + 34;
    GuiToggleGroup({cx, cy, (cw - 18) / 4.f, 24}, "Hero;Creep;Tower;Ancient", &editSel_);
    cy += 34;

    UnitDef& d = defs_.byIndex(editSel_);
    auto S = [&](const char* label, float* v, float mn, float mx) {
        GuiSlider({cx + 74, cy, cw - 74, 18}, label, TextFormat("%.2f", *v), v, mn, mx);
        cy += 24;
    };
    S("HP", &d.hp, 0, 3000);
    S("Damage", &d.damage, 0, 200);
    S("Range", &d.range, 0, 40);
    S("Atk int", &d.attackInterval, 0.2f, 3);
    S("Move spd", &d.moveSpeed, 0, 40);
    S("Radius", &d.radius, 0.5f, 6);
    S("Height", &d.height, 1, 16);
    if (editSel_ == 0) {
        S("Q dmg", &d.qDamage, 0, 300);
        S("Q cd", &d.qCooldown, 0, 15);
        S("W heal", &d.wHeal, 0, 400);
        S("W cd", &d.wCooldown, 0, 30);
    }

    cy += 6;
    GuiLabel({cx, cy, cw, 18}, "Team colors (Radiant / Dire)");
    cy += 20;
    GuiColorPicker({cx, cy, 96, 84}, NULL, &defs_.radiant);
    GuiColorPicker({cx + cw - 118, cy, 96, 84}, NULL, &defs_.dire);
    cy += 96;

    float bw = (cw - 12) / 3.f;
    if (GuiButton({cx, cy, bw, 26}, "Apply")) { applyDefsToLiveUnits(); editorMsg_ = "applied to live units"; }
    if (GuiButton({cx + bw + 6, cy, bw, 26}, "Save"))
        editorMsg_ = defs_.Save(defsPath_) ? ("saved: " + defsPath_) : "save FAILED";
    if (GuiButton({cx + 2 * (bw + 6), cy, bw, 26}, "Reload")) {
        defs_.Load(defsPath_); applyDefsToLiveUnits(); editorMsg_ = "reloaded";
    }
    cy += 30;
    if (!editorMsg_.empty()) GuiLabel({cx, cy, cw, 18}, editorMsg_.c_str());
}

} // namespace game
