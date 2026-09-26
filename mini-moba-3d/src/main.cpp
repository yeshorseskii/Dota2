// Mini MOBA — 3D edition, built on raylib 5.5.
//
// Same gameplay as the 2D version but rendered in 3D: a ground plane with a
// diagonal lane, cube/cylinder/sphere units, an angled camera that follows the
// player hero, and mouse ray-picking for movement and abilities.
//
// Controls:
//   Right mouse  — move the hero to the clicked ground point
//   Q            — fire a bolt toward the cursor (area damage)
//   W            — heal + damage shield
//   Mouse wheel  — zoom the camera
//   R            — restart after the match ends
//   Esc          — quit
#include "raylib.h"
#include "raymath.h"

#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <algorithm>

using std::vector;

// --------------------------------------------------------------- config -----
namespace cfg {
constexpr int   WinW = 1280;
constexpr int   WinH = 720;
constexpr float WaveInterval = 22.f;
constexpr int   CreepsPerWave = 4;
constexpr float CreepSpacing = 0.35f;
constexpr float HeroRespawn = 6.f;
constexpr float QDamage = 60.f;
constexpr float QCooldown = 3.f;
constexpr float QSplash = 6.f;
constexpr float WCooldown = 9.f;
constexpr float WHeal = 80.f;
constexpr float WShield = 3.f;
}

enum Team { RADIANT, DIRE };
enum Kind { HERO, CREEP, TOWER, ANCIENT };
enum Phase { PLAYING, RADIANT_WIN, DIRE_WIN };

// --------------------------------------------------------------- types ------
struct Unit {
    Kind kind = CREEP;
    Team team = RADIANT;
    Vector3 pos{};
    Vector3 spawn{};
    float radius = 1.f;
    float height = 2.f;

    float hp = 100.f, maxHp = 100.f;
    float attackDamage = 10.f;
    float attackRange = 6.f;
    float attackInterval = 1.f;
    float attackTimer = 0.f;

    float moveSpeed = 0.f;
    int   laneIndex = 0;

    int   level = 1, gold = 0;
    float xp = 0.f, xpToLevel = 100.f;
    Vector3 moveOrder{};
    bool  hasMoveOrder = false;
    float respawnTimer = 0.f;
    float shieldTimer = 0.f;
    float qCooldown = 0.f, wCooldown = 0.f;
    bool  isPlayer = false;

    bool alive() const { return hp > 0.f && respawnTimer <= 0.f; }
    bool building() const { return kind == TOWER || kind == ANCIENT; }
};

struct Projectile {
    Team team = RADIANT;
    Vector3 pos{}, vel{};
    float damage = 0.f, radius = 0.5f, life = 2.f;
    bool splash = false, dead = false;
    Color color = WHITE;
};

struct Beam { Vector3 a{}, b{}; Color color; float life = 0.14f; };
struct FloatText { Vector3 pos{}; std::string text; Color color; float life = 0.9f, maxLife = 0.9f; };

// helper: XZ-plane distance (units live at y=0)
static float dist(const Vector3& a, const Vector3& b) { return Vector3Distance(a, b); }
static Vector3 moveToward(Vector3 p, Vector3 t, float step) {
    Vector3 d = Vector3Subtract(t, p);
    float len = Vector3Length(d);
    if (len <= step || len < 1e-5f) return t;
    return Vector3Add(p, Vector3Scale(d, step / len));
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// --------------------------------------------------------------- game --------
class Game {
public:
    Game() {
        SetTraceLogLevel(LOG_WARNING);
        InitWindow(cfg::WinW, cfg::WinH, "Mini MOBA 3D");
        SetTargetFPS(60);
        if (const char* a = std::getenv("MOBA_SHOT_AT")) shotAt_ = std::atof(a);
        if (const char* f = std::getenv("MOBA_SHOT_FILE")) shotFile_ = f;
        reset();
    }
    ~Game() { CloseWindow(); }

    void run() {
        while (!WindowShouldClose()) {
            float dt = GetFrameTime();
            if (dt > 0.05f) dt = 0.05f;
            input();
            if (phase_ == PLAYING) update(dt);
            updateCamera(dt);
            render();
            if (shotAt_ >= 0.f && matchTime_ >= shotAt_ && !shotFile_.empty()) {
                TakeScreenshot(shotFile_.c_str());
                break;
            }
        }
    }

private:
    // ----- world state -----
    vector<Unit> units_;
    vector<Projectile> projectiles_;
    vector<Beam> beams_;
    vector<FloatText> texts_;
    vector<Vector3> lane_;
    int playerIdx_ = -1, enemyHeroIdx_ = -1, radiantAncient_ = -1, direAncient_ = -1;
    float waveTimer_ = 0.f; int waveCount_ = 0;
    struct Pending { Team team; int step; float delay; };
    vector<Pending> pending_;
    Phase phase_ = PLAYING;
    float matchTime_ = 0.f;
    Camera3D cam_{};
    float camDist_ = 54.f;
    float shotAt_ = -1.f; std::string shotFile_;

    Team enemyOf(Team t) const { return t == RADIANT ? DIRE : RADIANT; }
    Color teamColor(Team t) const {
        return t == RADIANT ? Color{86, 196, 112, 255} : Color{214, 84, 84, 255};
    }
    Unit& player() { return units_[playerIdx_]; }
    int laneCount() const { return (int)lane_.size(); }
    Vector3 laneWp(int step) const {
        int s = (int)clampf((float)step, 0.f, (float)laneCount() - 1);
        return lane_[s];
    }

    int addUnit(const Unit& u) {
        if (u.kind == CREEP) {
            for (size_t i = 0; i < units_.size(); ++i)
                if (units_[i].kind == CREEP && units_[i].hp <= 0.f) { units_[i] = u; return (int)i; }
        }
        units_.push_back(u);
        return (int)units_.size() - 1;
    }

    void reset() {
        units_.clear(); projectiles_.clear(); beams_.clear(); texts_.clear();
        pending_.clear(); lane_.clear();
        phase_ = PLAYING; matchTime_ = 0.f; waveTimer_ = 3.f; waveCount_ = 0;

        lane_ = {
            {16, 0, 68}, {34, 0, 60}, {54, 0, 50}, {74, 0, 40},
            {94, 0, 30}, {112, 0, 20}, {124, 0, 12},
        };
        spawnBuildings();

        Unit hero;
        hero.kind = HERO; hero.team = RADIANT;
        hero.pos = Vector3Add(lane_.front(), Vector3{7, 0, 7});
        hero.spawn = hero.pos; hero.radius = 1.6f; hero.height = 3.2f;
        hero.maxHp = hero.hp = 480.f; hero.attackDamage = 34.f;
        hero.attackRange = 11.f; hero.attackInterval = 0.9f; hero.moveSpeed = 20.f;
        hero.isPlayer = true; hero.moveOrder = hero.pos;
        playerIdx_ = addUnit(hero);

        Unit foe = hero;
        foe.team = DIRE; foe.isPlayer = false;
        foe.pos = Vector3Add(lane_.back(), Vector3{-4, 0, -3});
        foe.spawn = foe.pos; foe.moveOrder = foe.pos;
        enemyHeroIdx_ = addUnit(foe);

        // camera
        cam_.up = {0, 1, 0};
        cam_.fovy = 48.f;
        cam_.projection = CAMERA_PERSPECTIVE;
        cam_.target = player().pos;
        cam_.position = Vector3Add(cam_.target, Vector3{0, camDist_, camDist_ * 0.62f});
    }

    void spawnBuildings() {
        auto tower = [&](Team t, Vector3 at) {
            Unit u; u.kind = TOWER; u.team = t; u.pos = u.spawn = at;
            u.radius = 2.6f; u.height = 8.f; u.maxHp = u.hp = 900.f;
            u.attackDamage = 60.f; u.attackRange = 20.f; u.attackInterval = 1.1f;
            addUnit(u);
        };
        auto ancient = [&](Team t, Vector3 at) {
            Unit u; u.kind = ANCIENT; u.team = t; u.pos = u.spawn = at;
            u.radius = 4.5f; u.height = 10.f; u.maxHp = u.hp = 2600.f;
            u.attackDamage = 45.f; u.attackRange = 18.f; u.attackInterval = 1.2f;
            return addUnit(u);
        };
        radiantAncient_ = ancient(RADIANT, lane_.front());
        direAncient_ = ancient(DIRE, lane_.back());
        tower(RADIANT, lane_[2]); tower(RADIANT, lane_[1]);
        tower(DIRE, lane_[4]); tower(DIRE, lane_[5]);
    }

    // ----- input -----
    Vector3 groundPoint() {
        Ray r = GetMouseRay(GetMousePosition(), cam_);
        if (std::fabs(r.direction.y) < 1e-4f) return player().pos;
        float t = -r.position.y / r.direction.y;
        return Vector3{r.position.x + r.direction.x * t, 0.f, r.position.z + r.direction.z * t};
    }

    void input() {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.f) camDist_ = clampf(camDist_ - wheel * 3.f, 20.f, 80.f);

        if (phase_ != PLAYING) { if (IsKeyPressed(KEY_R)) reset(); return; }
        Unit& h = player();
        if (!h.alive()) return;

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            h.moveOrder = groundPoint(); h.hasMoveOrder = true;
        }
        if (IsKeyPressed(KEY_Q) && h.qCooldown <= 0.f) {
            Vector3 g = groundPoint();
            Vector3 dir = Vector3Normalize(Vector3Subtract(g, h.pos));
            if (Vector3Length(dir) < 0.1f) dir = {1, 0, 0};
            Projectile p; p.team = h.team; p.pos = h.pos; p.pos.y = 1.5f;
            p.vel = Vector3Scale(dir, 70.f); p.damage = cfg::QDamage;
            p.splash = true; p.radius = 0.9f; p.color = Color{120, 200, 255, 255};
            projectiles_.push_back(p);
            h.qCooldown = cfg::QCooldown;
        }
        if (IsKeyPressed(KEY_W) && h.wCooldown <= 0.f) {
            h.hp = std::min(h.maxHp, h.hp + cfg::WHeal);
            h.shieldTimer = cfg::WShield; h.wCooldown = cfg::WCooldown;
            texts_.push_back({Vector3Add(h.pos, {0, h.height, 0}), "+shield",
                              Color{120, 255, 160, 255}, 1.0f, 1.0f});
        }
    }

    void updateCamera(float) {
        Vector3 focus = player().alive() ? player().pos : player().spawn;
        if (shotAt_ > 0.f) focus = laneWp(3); // frame the lane for the preview capture
        cam_.target = Vector3Lerp(cam_.target, focus, 0.15f);
        Vector3 want = Vector3Add(cam_.target, Vector3{0, camDist_, camDist_ * 0.62f});
        cam_.position = Vector3Lerp(cam_.position, want, 0.15f);
    }

    // ----- simulation -----
    void update(float dt) {
        matchTime_ += dt;
        updateWaves(dt);
        for (auto& ps : pending_) {
            ps.delay -= dt;
            if (ps.delay <= 0.f) { spawnCreep(ps.team, ps.step); ps.delay = 1e9f; }
        }
        pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                       [](const Pending& p) { return p.delay > 1e8f; }), pending_.end());

        for (auto& u : units_) {
            if (u.attackTimer > 0.f) u.attackTimer -= dt;
            if (u.kind == HERO) {
                if (u.qCooldown > 0.f) u.qCooldown -= dt;
                if (u.wCooldown > 0.f) u.wCooldown -= dt;
                if (u.shieldTimer > 0.f) u.shieldTimer -= dt;
                if (u.respawnTimer > 0.f) {
                    u.respawnTimer -= dt;
                    if (u.respawnTimer <= 0.f) {
                        u.respawnTimer = 0.f; u.hp = u.maxHp;
                        u.pos = u.spawn; u.moveOrder = u.spawn; u.hasMoveOrder = false;
                    }
                }
            }
        }

        for (size_t i = 0; i < units_.size(); ++i)
            if (units_[i].alive()) ai(units_[i], dt);

        updateProjectiles(dt);
        updateEffects(dt);
        checkWin();
    }

    void updateWaves(float dt) {
        waveTimer_ -= dt;
        if (waveTimer_ > 0.f) return;
        waveTimer_ = cfg::WaveInterval; ++waveCount_;
        for (int n = 0; n < cfg::CreepsPerWave; ++n) {
            pending_.push_back({RADIANT, 0, n * cfg::CreepSpacing});
            pending_.push_back({DIRE, laneCount() - 1, n * cfg::CreepSpacing});
        }
    }

    void spawnCreep(Team team, int laneStep) {
        Unit c; c.kind = CREEP; c.team = team;
        Vector3 jitter{(float)(rand() % 30 - 15) / 6.f, 0, (float)(rand() % 30 - 15) / 6.f};
        c.pos = Vector3Add(laneWp(laneStep), jitter);
        c.radius = 1.1f; c.height = 2.2f; c.maxHp = c.hp = 120.f;
        c.attackDamage = 12.f; c.attackRange = 6.f; c.attackInterval = 1.f; c.moveSpeed = 9.f;
        c.laneIndex = (team == RADIANT) ? 1 : laneCount() - 2;
        addUnit(c);
    }

    int nearestEnemy(const Unit& u, float range) const {
        int best = -1; float bestD = range; Team foe = enemyOf(u.team);
        for (size_t i = 0; i < units_.size(); ++i) {
            const Unit& v = units_[i];
            if (!v.alive() || v.team != foe) continue;
            float d = dist(u.pos, v.pos) - v.radius;
            if (d <= bestD) { bestD = d; best = (int)i; }
        }
        return best;
    }

    void ai(Unit& u, float dt) {
        // attack
        int tgt = nearestEnemy(u, u.attackRange);
        if (tgt >= 0 && u.attackTimer <= 0.f) {
            applyDamage(units_[tgt], u.attackDamage, u.team);
            u.attackTimer = u.attackInterval;
            Vector3 a = u.pos; a.y = u.height * 0.5f;
            Vector3 b = units_[tgt].pos; b.y = units_[tgt].height * 0.5f;
            beams_.push_back({a, b, u.team == RADIANT ? Color{180, 230, 255, 255}
                                                      : Color{255, 190, 180, 255}, 0.14f});
        }
        if (u.building()) return;

        if (u.kind == CREEP) {
            if (nearestEnemy(u, u.attackRange + 2.f) < 0) {
                Vector3 wp = lane_[(int)clampf((float)u.laneIndex, 0.f, (float)laneCount() - 1)];
                u.pos = moveToward(u.pos, wp, u.moveSpeed * dt);
                if (dist(u.pos, wp) < 1.6f) {
                    u.laneIndex += (u.team == RADIANT) ? 1 : -1;
                    u.laneIndex = (int)clampf((float)u.laneIndex, 0.f, (float)laneCount() - 1);
                }
            }
            return;
        }

        if (u.kind == HERO && !u.isPlayer) {
            if (u.hp < u.maxHp * 0.30f) {
                u.pos = moveToward(u.pos, u.spawn, u.moveSpeed * dt);
                if (u.wCooldown <= 0.f) {
                    u.hp = std::min(u.maxHp, u.hp + cfg::WHeal);
                    u.shieldTimer = cfg::WShield; u.wCooldown = cfg::WCooldown;
                }
                return;
            }
            int aggro = nearestEnemy(u, 34.f);
            if (aggro >= 0) {
                Unit& v = units_[aggro];
                if (u.qCooldown <= 0.f && dist(u.pos, v.pos) < 36.f) {
                    Vector3 dir = Vector3Normalize(Vector3Subtract(v.pos, u.pos));
                    Projectile p; p.team = u.team; p.pos = u.pos; p.pos.y = 1.5f;
                    p.vel = Vector3Scale(dir, 70.f); p.damage = cfg::QDamage;
                    p.splash = true; p.radius = 0.9f; p.color = Color{255, 150, 120, 255};
                    projectiles_.push_back(p); u.qCooldown = cfg::QCooldown;
                }
                if (dist(u.pos, v.pos) > u.attackRange * 0.9f)
                    u.pos = moveToward(u.pos, v.pos, u.moveSpeed * dt);
            } else {
                u.pos = moveToward(u.pos, laneWp(laneCount() - 3), u.moveSpeed * dt);
            }
            return;
        }

        if (u.kind == HERO && u.isPlayer && u.hasMoveOrder) {
            u.pos = moveToward(u.pos, u.moveOrder, u.moveSpeed * dt);
            if (dist(u.pos, u.moveOrder) < 0.6f) u.hasMoveOrder = false;
        }
    }

    float takenMult(const Unit& u) const {
        return (u.kind == HERO && u.shieldTimer > 0.f) ? 0.5f : 1.f;
    }

    void applyDamage(Unit& v, float dmg, Team from) {
        if (!v.alive()) return;
        float applied = dmg * takenMult(v);
        v.hp -= applied;
        char buf[16]; std::snprintf(buf, sizeof(buf), "%d", (int)applied);
        texts_.push_back({Vector3Add(v.pos, {0, v.height + 0.5f, 0}), buf,
                          Color{255, 235, 120, 255}, 0.7f, 0.7f});
        if (v.hp <= 0.f) {
            v.hp = 0.f; rewards(v, from);
            if (v.kind == HERO) { v.respawnTimer = cfg::HeroRespawn + v.level * 0.8f; v.hasMoveOrder = false; }
        }
    }

    void rewards(const Unit& victim, Team killer) {
        int bounty = 0, xp = 0;
        switch (victim.kind) {
            case CREEP:   bounty = 40;  xp = 45;  break;
            case HERO:    bounty = 200; xp = 120; break;
            case TOWER:   bounty = 250; xp = 90;  break;
            case ANCIENT: break;
        }
        int best = -1; float bestD = 1e9f;
        for (size_t i = 0; i < units_.size(); ++i) {
            Unit& u = units_[i];
            if (u.kind != HERO || u.team != killer || !u.alive()) continue;
            float d = dist(u.pos, victim.pos);
            if (d < 90.f) {
                u.xp += xp;
                while (u.xp >= u.xpToLevel) {
                    u.xp -= u.xpToLevel; u.level += 1; u.xpToLevel *= 1.35f;
                    u.maxHp += 55.f; u.attackDamage += 6.f;
                    u.hp = std::min(u.maxHp, u.hp + 60.f);
                    texts_.push_back({Vector3Add(u.pos, {0, u.height + 1.f, 0}), "LEVEL UP!",
                                      Color{255, 215, 0, 255}, 1.3f, 1.3f});
                }
            }
            if (d < bestD) { bestD = d; best = (int)i; }
        }
        if (best >= 0 && bounty > 0) {
            units_[best].gold += bounty;
            char buf[16]; std::snprintf(buf, sizeof(buf), "+%d", bounty);
            texts_.push_back({Vector3Add(victim.pos, {0, victim.height, 0}), buf,
                              Color{255, 215, 0, 255}, 0.9f, 0.9f});
        }
    }

    void updateProjectiles(float dt) {
        for (auto& p : projectiles_) {
            if (p.dead) continue;
            p.life -= dt; p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
            if (p.life <= 0.f) { p.dead = true; continue; }
            for (auto& u : units_) {
                if (!u.alive() || u.team == p.team) continue;
                if (dist(u.pos, p.pos) <= p.radius + u.radius) {
                    if (p.splash) {
                        for (auto& v : units_) {
                            if (!v.alive() || v.team == p.team) continue;
                            if (dist(v.pos, p.pos) <= cfg::QSplash) applyDamage(v, p.damage, p.team);
                        }
                    } else applyDamage(u, p.damage, p.team);
                    p.dead = true; break;
                }
            }
        }
        projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                           [](const Projectile& p) { return p.dead; }), projectiles_.end());
    }

    void updateEffects(float dt) {
        for (auto& b : beams_) b.life -= dt;
        beams_.erase(std::remove_if(beams_.begin(), beams_.end(),
                     [](const Beam& b) { return b.life <= 0.f; }), beams_.end());
        for (auto& t : texts_) { t.life -= dt; t.pos.y += 3.f * dt; }
        texts_.erase(std::remove_if(texts_.begin(), texts_.end(),
                     [](const FloatText& t) { return t.life <= 0.f; }), texts_.end());
    }

    void checkWin() {
        if (radiantAncient_ >= 0 && units_[radiantAncient_].hp <= 0.f) phase_ = DIRE_WIN;
        if (direAncient_ >= 0 && units_[direAncient_].hp <= 0.f) phase_ = RADIANT_WIN;
    }

    // ----- rendering -----
    void drawGround() {
        DrawPlane(Vector3{70, 0, 40}, Vector2{160, 100}, Color{30, 34, 30, 255});
        // team-tinted base areas
        DrawCircle3D(lane_.front(), 22.f, Vector3{1, 0, 0}, 90.f, Color{60, 110, 70, 255});
        DrawCircle3D(lane_.back(), 22.f, Vector3{1, 0, 0}, 90.f, Color{120, 70, 70, 255});
        // lane as connected segments lying on the ground
        for (int i = 0; i + 1 < laneCount(); ++i) {
            Vector3 a = lane_[i], b = lane_[i + 1]; a.y = b.y = 0.05f;
            DrawCylinderEx(a, b, 3.2f, 3.2f, 12, Color{70, 68, 56, 255});
        }
        for (auto wp : lane_) { wp.y = 0.05f; DrawCylinder(wp, 3.2f, 3.2f, 0.1f, 16, Color{70, 68, 56, 255}); }
    }

    void drawUnit(const Unit& u) {
        Color col = teamColor(u.team);
        if (u.kind == ANCIENT) {
            if (u.hp <= 0.f) col = Color{70, 70, 70, 255};
            DrawCube(Vector3{u.pos.x, u.height * 0.5f, u.pos.z}, u.radius * 2, u.height, u.radius * 2, col);
            DrawCubeWires(Vector3{u.pos.x, u.height * 0.5f, u.pos.z}, u.radius * 2, u.height, u.radius * 2, RAYWHITE);
            DrawSphere(Vector3{u.pos.x, u.height + 1.2f, u.pos.z}, 1.4f,
                       u.hp > 0.f ? Color{255, 240, 150, 255} : DARKGRAY);
            return;
        }
        if (u.hp <= 0.f && u.kind != HERO) return;
        if (u.kind == TOWER) {
            DrawCube(Vector3{u.pos.x, u.height * 0.5f, u.pos.z}, u.radius * 1.8f, u.height, u.radius * 1.8f, col);
            DrawCubeWires(Vector3{u.pos.x, u.height * 0.5f, u.pos.z}, u.radius * 1.8f, u.height, u.radius * 1.8f, Color{20, 20, 20, 255});
            DrawCube(Vector3{u.pos.x, u.height + 0.5f, u.pos.z}, u.radius * 2.2f, 1.f, u.radius * 2.2f, col);
            return;
        }
        if (u.kind == CREEP) {
            DrawCylinder(u.pos, u.radius, u.radius, u.height, 10, col);
            DrawCylinderWires(u.pos, u.radius, u.radius, u.height, 10, Color{20, 20, 20, 120});
            return;
        }
        // hero
        if (u.respawnTimer > 0.f) {
            Vector3 s = u.spawn; s.y = 0.1f;
            DrawCircle3D(s, u.radius + 0.6f, Vector3{1, 0, 0}, 90.f, Color{col.r, col.g, col.b, 120});
            return;
        }
        if (u.isPlayer) {
            Vector3 ring = u.pos; ring.y = 0.1f;
            DrawCircle3D(ring, u.radius + 1.2f, Vector3{1, 0, 0}, 90.f, RAYWHITE);
            if (u.hasMoveOrder) {
                Vector3 m = u.moveOrder; m.y = 0.1f;
                DrawCircle3D(m, 0.8f, Vector3{1, 0, 0}, 90.f, Color{90, 200, 110, 255});
            }
        }
        DrawCylinder(u.pos, u.radius, u.radius * 0.7f, u.height, 14, col);
        Color outline = u.shieldTimer > 0.f ? Color{120, 220, 255, 255} : RAYWHITE;
        DrawCylinderWires(u.pos, u.radius, u.radius * 0.7f, u.height, 14, outline);
        DrawSphere(Vector3{u.pos.x, u.height + 0.9f, u.pos.z}, 0.9f, col);
    }

    void hpBar(const Unit& u) {
        if (u.hp <= 0.f) return;
        Vector3 top = Vector3{u.pos.x, u.height + (u.kind == HERO ? 2.4f : 1.4f), u.pos.z};
        Vector2 sp = GetWorldToScreen(top, cam_);
        if (sp.x < -50 || sp.x > cfg::WinW + 50 || sp.y < -50 || sp.y > cfg::WinH + 50) return;
        float w = (u.kind == ANCIENT) ? 90.f : (u.kind == HERO ? 52.f : (u.kind == TOWER ? 56.f : 28.f));
        float h = 6.f;
        float frac = clampf(u.hp / u.maxHp, 0.f, 1.f);
        DrawRectangle((int)(sp.x - w / 2), (int)(sp.y - h), (int)w, (int)h, Color{15, 15, 15, 220});
        Color c = teamColor(u.team);
        DrawRectangle((int)(sp.x - w / 2), (int)(sp.y - h), (int)(w * frac), (int)h, c);
        if (u.kind == HERO) {
            char lv[8]; std::snprintf(lv, sizeof(lv), "%d", u.level);
            DrawText(lv, (int)(sp.x - w / 2 - 14), (int)(sp.y - h - 1), 14, RAYWHITE);
        }
    }

    void hud() {
        int mm = (int)matchTime_ / 60, ss = (int)matchTime_ % 60;
        DrawText(TextFormat("%02d:%02d   wave %d", mm, ss, waveCount_), cfg::WinW / 2 - 70, 14, 22, RAYWHITE);
        if (radiantAncient_ >= 0 && direAncient_ >= 0) {
            DrawText(TextFormat("Radiant Ancient: %d", (int)units_[radiantAncient_].hp), 16, 14, 18, Color{120, 220, 140, 255});
            const char* d = TextFormat("Dire Ancient: %d", (int)units_[direAncient_].hp);
            DrawText(d, cfg::WinW - 16 - MeasureText(d, 18), 14, 18, Color{230, 130, 130, 255});
        }
        const Unit& h = units_[playerIdx_];
        DrawText(TextFormat("LV %d   HP %d/%d   Gold %d", h.level, (int)std::max(0.f, h.hp), (int)h.maxHp, h.gold),
                 16, cfg::WinH - 56, 20, RAYWHITE);
        auto cd = [&](const char* k, float v, int x) {
            const char* s = v <= 0.f ? TextFormat("%s: ready", k) : TextFormat("%s: %.1f", k, v);
            DrawText(s, x, cfg::WinH - 28, 18, v <= 0.f ? Color{150, 220, 255, 255} : GRAY);
        };
        cd("Q bolt", h.qCooldown, 16);
        cd("W shield", h.wCooldown, 170);
        DrawText("Right-click move  |  Q/W abilities  |  wheel zoom", cfg::WinW - 430, cfg::WinH - 28, 16, GRAY);

        if (phase_ != PLAYING) {
            DrawRectangle(0, 0, cfg::WinW, cfg::WinH, Color{0, 0, 0, 150});
            const char* msg = phase_ == RADIANT_WIN ? "RADIANT VICTORY" : "DIRE VICTORY";
            Color c = phase_ == RADIANT_WIN ? Color{120, 230, 140, 255} : Color{230, 120, 120, 255};
            int fs = 60;
            DrawText(msg, cfg::WinW / 2 - MeasureText(msg, fs) / 2, cfg::WinH / 2 - 60, fs, c);
            const char* sub = "Press R to play again, Esc to quit";
            DrawText(sub, cfg::WinW / 2 - MeasureText(sub, 22) / 2, cfg::WinH / 2 + 20, 22, RAYWHITE);
        }
    }

    void render() {
        BeginDrawing();
        ClearBackground(Color{18, 20, 22, 255});
        BeginMode3D(cam_);
        drawGround();
        for (auto& u : units_) if (u.building()) drawUnit(u);
        for (auto& u : units_) if (u.kind == CREEP && u.alive()) drawUnit(u);
        for (auto& u : units_) if (u.kind == HERO) drawUnit(u);
        for (auto& b : beams_) DrawLine3D(b.a, b.b, b.color);
        for (auto& p : projectiles_) DrawSphere(p.pos, p.radius + 0.3f, p.color);
        EndMode3D();

        // 2D overlays
        for (auto& u : units_) hpBar(u);
        for (auto& t : texts_) {
            Vector2 sp = GetWorldToScreen(t.pos, cam_);
            unsigned char a = (unsigned char)(255 * clampf(t.life / t.maxLife, 0.f, 1.f));
            DrawText(t.text.c_str(), (int)sp.x - MeasureText(t.text.c_str(), 18) / 2, (int)sp.y, 18,
                     Color{t.color.r, t.color.g, t.color.b, a});
        }
        hud();
        EndDrawing();
    }
};

int main() {
    std::srand((unsigned)std::time(nullptr));
    Game g;
    g.run();
    return 0;
}
