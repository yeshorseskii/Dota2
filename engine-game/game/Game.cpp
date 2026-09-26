// Implementation of the MOBA game module and all its entities, built on the
// engine (Entity/World/ticks/cvars/console).
#include "MobaGame.hpp"
#include "Entities.hpp"
#include "engine/ConVar.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace game {

MobaGame* g_game = nullptr;

// ------------------------------------------------------------- cvars --------
static eng::ConVar sv_wave_interval("sv_wave_interval", 22.f, "seconds between creep waves");
static eng::ConVar sv_creeps_per_wave("sv_creeps_per_wave", 4.f, "creeps spawned per wave per side");
static eng::ConVar sv_damage_scale("sv_damage_scale", 1.f, "global damage multiplier (cheat)");
static eng::ConVar cl_cam_dist("cl_cam_dist", 54.f, "camera follow distance");

// ------------------------------------------------------------- colors -------
Color TeamColor(Team t) {
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
    classname = "hero"; solid = true; height = 3.2f; radius = 1.6f; barWidth = 52.f;
    maxHp = hp = 480.f; attackDamage = 34.f; attackRange = 11.f; attackInterval = 0.9f;
    spawnPos = pos; moveOrder = pos;
}

void Hero::addXp(float amount) {
    xp += amount;
    while (xp >= xpToLevel) {
        xp -= xpToLevel; level += 1; xpToLevel *= 1.35f;
        maxHp += 55.f; attackDamage += 6.f; hp = std::min(maxHp, hp + 60.f);
        g_game->SpawnText(Vector3Add(pos, {0, height + 1.f, 0}), "LEVEL UP!",
                          Color{255, 215, 0, 255}, 1.3f);
    }
}

void Hero::castQ(Vector3 target) {
    if (qCd > 0.f) return;
    Vector3 dir = Vector3Normalize(Vector3Subtract(target, pos));
    if (Vector3Length(dir) < 0.1f) dir = {1, 0, 0};
    auto* p = g_game->World().Create<Projectile>();
    p->team = team; p->pos = pos; p->pos.y = 1.5f;
    p->vel = Vector3Scale(dir, 70.f); p->damage = 60.f; p->splashRadius = 6.f;
    p->color = team == Team::Radiant ? Color{120, 200, 255, 255} : Color{255, 150, 120, 255};
    qCd = 3.f;
}

void Hero::castW() {
    if (wCd > 0.f) return;
    hp = std::min(maxHp, hp + 80.f); shieldTimer = 3.f; wCd = 9.f;
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

    if (isPlayer) {
        if (hasMoveOrder) {
            pos = eng::MoveToward(pos, moveOrder, 20.f * dt);
            if (eng::Dist(pos, moveOrder) < 0.6f) hasMoveOrder = false;
        }
        return;
    }

    // enemy AI: retreat when low, else push and fight along the lane.
    if (hp < maxHp * 0.30f) {
        pos = eng::MoveToward(pos, spawnPos, 20.f * dt);
        castW();
        return;
    }
    CombatEntity* aggro = g_game->NearestEnemy(team, pos, 34.f);
    if (aggro) {
        if (qCd <= 0.f && eng::Dist(pos, aggro->pos) < 36.f) castQ(aggro->pos);
        if (eng::Dist(pos, aggro->pos) > attackRange * 0.9f)
            pos = eng::MoveToward(pos, aggro->pos, 20.f * dt);
    } else {
        pos = eng::MoveToward(pos, g_game->LaneWp(g_game->LaneCount() - 3), 20.f * dt);
    }
}

void Hero::Render() {
    Color col = TeamColor(team);
    if (respawnTimer > 0.f) {
        Vector3 s = spawnPos; s.y = 0.1f;
        DrawCircle3D(s, radius + 0.6f, Vector3{1, 0, 0}, 90.f, Color{col.r, col.g, col.b, 120});
        return;
    }
    if (isPlayer) {
        Vector3 ring = pos; ring.y = 0.1f;
        DrawCircle3D(ring, radius + 1.2f, Vector3{1, 0, 0}, 90.f, RAYWHITE);
        if (hasMoveOrder) {
            Vector3 m = moveOrder; m.y = 0.1f;
            DrawCircle3D(m, 0.8f, Vector3{1, 0, 0}, 90.f, Color{90, 200, 110, 255});
        }
    }
    DrawCylinder(pos, radius, radius * 0.7f, height, 14, col);
    DrawCylinderWires(pos, radius, radius * 0.7f, height, 14,
                      shieldTimer > 0.f ? Color{120, 220, 255, 255} : RAYWHITE);
    DrawSphere(Vector3{pos.x, height + 0.9f, pos.z}, 0.9f, col);
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
    classname = "creep"; solid = true; radius = 1.1f; height = 2.2f; barWidth = 28.f;
    maxHp = hp = 120.f; attackDamage = 12.f; attackRange = 6.f; attackInterval = 1.f;
}

void Creep::Update(float dt) {
    combatTick(dt);
    // move along the lane unless an enemy is close enough to fight
    if (g_game->NearestEnemy(team, pos, attackRange + 2.f)) return;
    Vector3 wp = g_game->LaneWp(laneIndex);
    pos = eng::MoveToward(pos, wp, 9.f * dt);
    if (eng::Dist(pos, wp) < 1.6f) {
        laneIndex += (team == Team::Radiant) ? 1 : -1;
        laneIndex = (int)eng::Clampf((float)laneIndex, 0.f, (float)g_game->LaneCount() - 1);
    }
}

void Creep::Render() {
    Color col = TeamColor(team);
    DrawCylinder(pos, radius, radius, height, 10, col);
    DrawCylinderWires(pos, radius, radius, height, 10, Color{20, 20, 20, 120});
}

// =================================================================
// Tower
// =================================================================
void Tower::Update(float dt) {
    if (radius < 2.f) { radius = 2.6f; height = 8.f; barWidth = 56.f;
                        maxHp = hp = 900.f; attackDamage = 60.f; attackRange = 20.f;
                        attackInterval = 1.1f; }
    combatTick(dt);
}

void Tower::Render() {
    Color col = TeamColor(team);
    DrawCube(Vector3{pos.x, height * 0.5f, pos.z}, radius * 1.8f, height, radius * 1.8f, col);
    DrawCubeWires(Vector3{pos.x, height * 0.5f, pos.z}, radius * 1.8f, height, radius * 1.8f, Color{20, 20, 20, 255});
    DrawCube(Vector3{pos.x, height + 0.5f, pos.z}, radius * 2.2f, 1.f, radius * 2.2f, col);
}

// =================================================================
// Ancient
// =================================================================
void Ancient::Update(float dt) {
    if (radius < 3.f) { radius = 4.5f; height = 10.f; barWidth = 90.f;
                        maxHp = hp = 2600.f; attackDamage = 45.f; attackRange = 18.f;
                        attackInterval = 1.2f; }
    combatTick(dt);
}

void Ancient::onDeath(Team killer) {
    g_game->SetWin(killer == Team::Radiant ? Phase::RadiantWin : Phase::DireWin);
}

void Ancient::Render() {
    Color col = hp <= 0.f ? Color{70, 70, 70, 255} : TeamColor(team);
    DrawCube(Vector3{pos.x, height * 0.5f, pos.z}, radius * 2, height, radius * 2, col);
    DrawCubeWires(Vector3{pos.x, height * 0.5f, pos.z}, radius * 2, height, radius * 2, RAYWHITE);
    DrawSphere(Vector3{pos.x, height + 1.2f, pos.z}, 1.4f,
               hp > 0.f ? Color{255, 240, 150, 255} : DARKGRAY);
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

void MobaGame::OnInit(eng::Engine& e) {
    engine_ = &e; g_game = this;
    e.background = Color{18, 20, 22, 255};
    reset();
}

void MobaGame::OnInput(eng::Engine& e) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f)
        cl_cam_dist.SetFloat(eng::Clampf(cl_cam_dist.GetFloat() - wheel * 3.f, 20.f, 80.f));

    if (phase_ != Phase::Playing) { if (IsKeyPressed(KEY_R)) reset(); return; }
    if (!player_ || !player_->alive()) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        player_->moveOrder = e.GroundPoint(); player_->hasMoveOrder = true;
    }
    if (IsKeyPressed(KEY_Q)) player_->castQ(e.GroundPoint());
    if (IsKeyPressed(KEY_W)) player_->castW();
}

void MobaGame::OnFrame(eng::Engine& e, float /*dt*/) {
    Vector3 focus = (player_ && player_->alive()) ? player_->pos
                    : (player_ ? player_->spawnPos : Vector3{70, 0, 40});
    if (std::getenv("MOBA_SHOT_AT")) focus = LaneWp(3);
    float d = cl_cam_dist.GetFloat();
    Camera3D& c = e.camera();
    c.target = Vector3Lerp(c.target, focus, 0.15f);
    c.position = Vector3Lerp(c.position, Vector3Add(c.target, Vector3{0, d, d * 0.62f}), 0.15f);
}

void MobaGame::OnTick(eng::World& w) {
    if (phase_ != Phase::Playing) return;
    waveTimer_ -= w.tickInterval;
    if (waveTimer_ <= 0.f) { waveTimer_ = sv_wave_interval.GetFloat(); spawnWave(); }
}

void MobaGame::OnRender3D() {
    DrawPlane(Vector3{70, 0, 40}, Vector2{160, 100}, Color{30, 34, 30, 255});
    if (!lane_.empty()) {
        DrawCircle3D(lane_.front(), 22.f, Vector3{1, 0, 0}, 90.f, Color{60, 110, 70, 255});
        DrawCircle3D(lane_.back(), 22.f, Vector3{1, 0, 0}, 90.f, Color{120, 70, 70, 255});
        for (int i = 0; i + 1 < (int)lane_.size(); ++i) {
            Vector3 a = lane_[i], b = lane_[i + 1]; a.y = b.y = 0.05f;
            DrawCylinderEx(a, b, 3.2f, 3.2f, 12, Color{70, 68, 56, 255});
        }
        for (auto wp : lane_) { wp.y = 0.05f; DrawCylinder(wp, 3.2f, 3.2f, 0.1f, 16, Color{70, 68, 56, 255}); }
    }
}

void MobaGame::OnRenderHUD() {
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
    DrawText("Right-click move | Q/W abilities | wheel zoom | ~ console", W - 470, H - 28, 16, GRAY);

    if (phase_ != Phase::Playing) {
        DrawRectangle(0, 0, W, H, Color{0, 0, 0, 150});
        const char* msg = phase_ == Phase::RadiantWin ? "RADIANT VICTORY" : "DIRE VICTORY";
        Color c = phase_ == Phase::RadiantWin ? Color{120, 230, 140, 255} : Color{230, 120, 120, 255};
        DrawText(msg, W / 2 - MeasureText(msg, 60) / 2, H / 2 - 60, 60, c);
        const char* sub = "Press R to play again, Esc to quit";
        DrawText(sub, W / 2 - MeasureText(sub, 22) / 2, H / 2 + 20, 22, RAYWHITE);
    }
}

} // namespace game
