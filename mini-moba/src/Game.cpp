#include "Game.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace mb {

// ---- candidate font paths so text works out of the box on most machines ----
static const char* kFontCandidates[] = {
    "assets/font.ttf",
    "mini-moba/assets/font.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/Library/Fonts/Arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "C:/Windows/Fonts/arial.ttf",
};

Game::Game()
    : window_(sf::VideoMode(cfg::WinW, cfg::WinH), "Mini MOBA",
              sf::Style::Titlebar | sf::Style::Close) {
    window_.setFramerateLimit(60);
    for (const char* p : kFontCandidates) {
        if (font_.loadFromFile(p)) { hasFont_ = true; break; }
    }
    if (const char* at = std::getenv("MOBA_SHOT_AT")) shotAt_ = std::atof(at);
    if (const char* f = std::getenv("MOBA_SHOT_FILE")) shotFile_ = f;
    resetWorld();
}

// ------------------------------------------------------------------ world ---

Team otherTeam(Team t) { return t == Team::Radiant ? Team::Dire : Team::Radiant; }

void Game::resetWorld() {
    units_.clear();
    projectiles_.clear();
    beams_.clear();
    texts_.clear();
    pending_.clear();
    lane_.clear();
    phase_ = Phase::Playing;
    matchTime_ = 0.f;
    waveTimer_ = 3.f; // first wave shortly after start
    waveCount_ = 0;

    // Lane: a polyline from Radiant base (bottom-left) to Dire base (top-right).
    lane_ = {
        {150.f, 590.f}, {300.f, 545.f}, {470.f, 460.f}, {640.f, 370.f},
        {820.f, 285.f}, {990.f, 190.f}, {1140.f, 135.f},
    };

    spawnBuildings();

    // Player hero (Radiant), starts near its ancient.
    Unit hero;
    hero.kind = UnitKind::Hero;
    hero.team = Team::Radiant;
    hero.pos = lane_.front() + Vec2(40.f, 30.f);
    hero.spawn = hero.pos;
    hero.radius = 15.f;
    hero.maxHp = hero.hp = 480.f;
    hero.attackDamage = 34.f;
    hero.attackRange = 95.f;
    hero.attackInterval = 0.9f;
    hero.moveSpeed = 175.f;
    hero.isPlayer = true;
    hero.moveOrder = hero.pos;
    playerIdx_ = addUnit(hero);

    // Enemy hero (Dire), AI-controlled.
    Unit foe = hero;
    foe.team = Team::Dire;
    foe.isPlayer = false;
    foe.pos = lane_.back() + Vec2(-40.f, -30.f);
    foe.spawn = foe.pos;
    foe.moveOrder = foe.pos;
    enemyHeroIdx_ = addUnit(foe);
}

void Game::spawnBuildings() {
    auto makeTower = [&](Team team, Vec2 at) {
        Unit t;
        t.kind = UnitKind::Tower;
        t.team = team;
        t.pos = t.spawn = at;
        t.radius = 24.f;
        t.maxHp = t.hp = 900.f;
        t.attackDamage = 60.f;
        t.attackRange = 180.f;
        t.attackInterval = 1.1f;
        t.ranged = true;
        addUnit(t);
    };
    auto makeAncient = [&](Team team, Vec2 at) -> int {
        Unit a;
        a.kind = UnitKind::Ancient;
        a.team = team;
        a.pos = a.spawn = at;
        a.radius = 40.f;
        a.maxHp = a.hp = 2600.f;
        a.attackDamage = 45.f;
        a.attackRange = 170.f;
        a.attackInterval = 1.2f;
        a.ranged = true;
        return addUnit(a);
    };

    radiantAncient_ = makeAncient(Team::Radiant, lane_.front());
    direAncient_ = makeAncient(Team::Dire, lane_.back());

    // Two towers per side along the lane.
    makeTower(Team::Radiant, lane_[2]);
    makeTower(Team::Radiant, lane_[1]);
    makeTower(Team::Dire, lane_[4]);
    makeTower(Team::Dire, lane_[5]);
}

int Game::addUnit(const Unit& u) {
    // Reuse a dead creep slot to keep the vector bounded; heroes and buildings
    // keep stable indices because we only ever reuse creep slots.
    if (u.kind == UnitKind::Creep) {
        for (std::size_t i = 0; i < units_.size(); ++i) {
            if (units_[i].kind == UnitKind::Creep && units_[i].hp <= 0.f) {
                units_[i] = u;
                return static_cast<int>(i);
            }
        }
    }
    units_.push_back(u);
    return static_cast<int>(units_.size() - 1);
}

// ------------------------------------------------------------------- loop ---

void Game::run() {
    while (window_.isOpen()) {
        float dt = clock_.restart().asSeconds();
        if (dt > 0.05f) dt = 0.05f; // clamp big hitches
        handleEvents();
        if (phase_ == Phase::Playing) update(dt);
        render();

        if (shotAt_ >= 0.f && matchTime_ >= shotAt_ && !shotFile_.empty()) {
            sf::Texture tex;
            tex.create(window_.getSize().x, window_.getSize().y);
            tex.update(window_);
            tex.copyToImage().saveToFile(shotFile_);
            window_.close();
        }
    }
}

void Game::handleEvents() {
    sf::Event e;
    while (window_.pollEvent(e)) {
        if (e.type == sf::Event::Closed) window_.close();
        else if (e.type == sf::Event::KeyPressed) {
            if (e.key.code == sf::Keyboard::Escape) window_.close();
            else if (e.key.code == sf::Keyboard::R && phase_ != Phase::Playing)
                resetWorld();
            else if (phase_ == Phase::Playing && player().alive()) {
                Vec2 m(static_cast<float>(sf::Mouse::getPosition(window_).x),
                       static_cast<float>(sf::Mouse::getPosition(window_).y));
                if (e.key.code == sf::Keyboard::Q && player().qCooldown <= 0.f) {
                    Unit& h = player();
                    Projectile p;
                    p.team = h.team;
                    p.pos = h.pos;
                    p.vel = normalized(m - h.pos) * 620.f;
                    p.damage = cfg::QDamage;
                    p.splash = true;
                    p.radius = 9.f;
                    p.color = sf::Color(120, 200, 255);
                    projectiles_.push_back(p);
                    h.qCooldown = cfg::QCooldown;
                } else if (e.key.code == sf::Keyboard::W && player().wCooldown <= 0.f) {
                    Unit& h = player();
                    h.hp = std::min(h.maxHp, h.hp + cfg::WHeal);
                    h.shieldTimer = cfg::WShield;
                    h.wCooldown = cfg::WCooldown;
                    texts_.push_back({h.pos, "+shield", sf::Color(120, 255, 160), 1.0f, 1.0f});
                }
            }
        } else if (e.type == sf::Event::MouseButtonPressed) {
            if (phase_ == Phase::Playing && player().alive() &&
                e.mouseButton.button == sf::Mouse::Right) {
                player().moveOrder = Vec2(static_cast<float>(e.mouseButton.x),
                                          static_cast<float>(e.mouseButton.y));
                player().hasMoveOrder = true;
            }
        }
    }
}

void Game::update(float dt) {
    matchTime_ += dt;

    updateWaves(dt);

    // Staggered creep spawns queued by a wave.
    for (auto& ps : pending_) {
        ps.delay -= dt;
        if (ps.delay <= 0.f) { spawnCreep(ps.team, ps.step); ps.delay = 1e9f; }
    }
    pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                   [](const PendingSpawn& p) { return p.delay > 1e8f; }),
                   pending_.end());

    // Cooldowns / timers for heroes.
    for (auto& u : units_) {
        if (u.attackTimer > 0.f) u.attackTimer -= dt;
        if (u.kind == UnitKind::Hero) {
            if (u.qCooldown > 0.f) u.qCooldown -= dt;
            if (u.wCooldown > 0.f) u.wCooldown -= dt;
            if (u.shieldTimer > 0.f) u.shieldTimer -= dt;
            if (u.respawnTimer > 0.f) {
                u.respawnTimer -= dt;
                if (u.respawnTimer <= 0.f) { // respawn at own ancient
                    u.respawnTimer = 0.f;
                    u.hp = u.maxHp;
                    u.pos = u.spawn;
                    u.moveOrder = u.spawn;
                    u.hasMoveOrder = false;
                }
            }
        }
    }

    updateHeroInput();

    // AI + auto-attacks for everything.
    for (std::size_t i = 0; i < units_.size(); ++i) {
        Unit& u = units_[i];
        if (!u.alive()) continue;
        updateUnitAI(u, static_cast<int>(i), dt);
    }

    updateProjectiles(dt);
    updateEffects(dt);
    checkWinCondition();
}

// ------------------------------------------------------------------ waves ---

void Game::updateWaves(float dt) {
    waveTimer_ -= dt;
    if (waveTimer_ > 0.f) return;
    waveTimer_ = cfg::WaveInterval;
    ++waveCount_;
    for (int n = 0; n < cfg::CreepsPerWave; ++n) {
        pending_.push_back({Team::Radiant, 0, n * cfg::CreepSpacing});
        pending_.push_back({Team::Dire, laneStepCount() - 1, n * cfg::CreepSpacing});
    }
}

void Game::spawnCreep(Team team, int laneStep) {
    Unit c;
    c.kind = UnitKind::Creep;
    c.team = team;
    c.pos = laneWaypoint(team, laneStep) + Vec2((rand() % 30) - 15.f, (rand() % 30) - 15.f);
    c.radius = 11.f;
    c.maxHp = c.hp = 120.f;
    c.attackDamage = 12.f;
    c.attackRange = 48.f;
    c.attackInterval = 1.0f;
    c.moveSpeed = 72.f;
    // Radiant pushes toward higher indices, Dire toward lower.
    c.laneIndex = (team == Team::Radiant) ? 1 : laneStepCount() - 2;
    addUnit(c);
}

// -------------------------------------------------------------------- AI ----

void Game::updateHeroInput() {
    // Movement toward the right-click order is applied in updateUnitAI() using
    // the frame dt, so this hook is reserved for future input polling.
}

void Game::updateUnitAI(Unit& u, int selfIdx, float dt) {
    // 1) Attack the nearest enemy in range (all unit kinds).
    int tgt = findNearestEnemy(u, u.attackRange);
    if (tgt >= 0 && u.attackTimer <= 0.f) {
        Unit& v = units_[tgt];
        float dmg = u.attackDamage;
        applyDamage(v, dmg, u.team, u.pos);
        u.attackTimer = u.attackInterval;
        beams_.push_back({u.pos, v.pos,
                          u.team == Team::Radiant ? sf::Color(180, 230, 255)
                                                  : sf::Color(255, 190, 180),
                          0.14f});
    }

    // 2) Movement, by kind.
    if (u.kind == UnitKind::Tower || u.kind == UnitKind::Ancient) return; // immobile

    if (u.kind == UnitKind::Creep) {
        // Stop to fight if an enemy is close (a bit beyond attack range).
        int near = findNearestEnemy(u, u.attackRange + 20.f);
        if (near < 0) {
            Vec2 wp = lane_[static_cast<std::size_t>(
                clampf(static_cast<float>(u.laneIndex), 0.f,
                       static_cast<float>(laneStepCount() - 1)))];
            u.pos = moveToward(u.pos, wp, u.moveSpeed * dt);
            if (distance(u.pos, wp) < 14.f) {
                u.laneIndex += (u.team == Team::Radiant) ? 1 : -1;
                u.laneIndex = static_cast<int>(
                    clampf(static_cast<float>(u.laneIndex), 0.f,
                           static_cast<float>(laneStepCount() - 1)));
            }
        }
        return;
    }

    if (u.kind == UnitKind::Hero && !u.isPlayer) {
        // Simple bot: retreat when low, else advance and fight along the lane.
        Vec2 home = u.spawn;
        if (u.hp < u.maxHp * 0.30f) {
            u.pos = moveToward(u.pos, home, u.moveSpeed * dt);
            if (u.wCooldown <= 0.f) { // panic heal
                u.hp = std::min(u.maxHp, u.hp + cfg::WHeal);
                u.shieldTimer = cfg::WShield;
                u.wCooldown = cfg::WCooldown;
            }
            return;
        }
        int aggro = findNearestEnemy(u, 340.f);
        if (aggro >= 0) {
            Unit& v = units_[aggro];
            // Cast Q at range.
            if (u.qCooldown <= 0.f && distance(u.pos, v.pos) < 360.f) {
                Projectile p;
                p.team = u.team;
                p.pos = u.pos;
                p.vel = normalized(v.pos - u.pos) * 620.f;
                p.damage = cfg::QDamage;
                p.splash = true;
                p.radius = 9.f;
                p.color = sf::Color(255, 150, 120);
                projectiles_.push_back(p);
                u.qCooldown = cfg::QCooldown;
            }
            // Kite to just inside attack range.
            if (distance(u.pos, v.pos) > u.attackRange * 0.9f)
                u.pos = moveToward(u.pos, v.pos, u.moveSpeed * dt);
        } else {
            // Push toward enemy side along the lane.
            Vec2 wp = laneWaypoint(u.team, u.team == Team::Dire ? 2 : laneStepCount() - 3);
            u.pos = moveToward(u.pos, wp, u.moveSpeed * dt);
        }
        return;
    }

    // Player hero: step toward the move order using real dt.
    if (u.kind == UnitKind::Hero && u.isPlayer && u.hasMoveOrder) {
        u.pos = moveToward(u.pos, u.moveOrder, u.moveSpeed * dt);
        if (distance(u.pos, u.moveOrder) < 4.f) u.hasMoveOrder = false;
    }
}

// --------------------------------------------------------------- combat -----

float Game::takenMultiplier(const Unit& u) const {
    return (u.kind == UnitKind::Hero && u.shieldTimer > 0.f) ? 0.5f : 1.0f;
}

void Game::applyDamage(Unit& victim, float dmg, Team from, const Vec2& src) {
    (void)src;
    if (!victim.alive()) return;
    float applied = dmg * takenMultiplier(victim);
    victim.hp -= applied;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(applied));
    texts_.push_back({victim.pos + Vec2(0.f, -victim.radius - 4.f), buf,
                      sf::Color(255, 235, 120), 0.7f, 0.7f});
    if (victim.hp <= 0.f) {
        victim.hp = 0.f;
        grantKillRewards(victim, from);
        if (victim.kind == UnitKind::Hero) {
            victim.respawnTimer = cfg::HeroRespawn + victim.level * 0.8f;
            victim.hasMoveOrder = false;
        }
    }
}

void Game::grantKillRewards(const Unit& victim, Team killer) {
    int bounty = 0, xp = 0;
    switch (victim.kind) {
        case UnitKind::Creep:   bounty = 40;  xp = 45;  break;
        case UnitKind::Hero:    bounty = 200; xp = 120; break;
        case UnitKind::Tower:   bounty = 250; xp = 90;  break;
        case UnitKind::Ancient: bounty = 0;   xp = 0;   break;
    }
    // Gold to the nearest allied hero; XP to allied heroes within radius.
    int best = -1; float bestD = 1e9f;
    for (std::size_t i = 0; i < units_.size(); ++i) {
        Unit& u = units_[i];
        if (u.kind != UnitKind::Hero || u.team != killer || !u.alive()) continue;
        float d = distance(u.pos, victim.pos);
        if (d < 900.f) {
            u.xp += xp;
            while (u.xp >= u.xpToLevel) { // level up
                u.xp -= u.xpToLevel;
                u.level += 1;
                u.xpToLevel *= 1.35f;
                u.maxHp += 55.f;
                u.attackDamage += 6.f;
                u.hp = std::min(u.maxHp, u.hp + 60.f);
                texts_.push_back({u.pos + Vec2(0.f, -30.f), "LEVEL UP!",
                                  sf::Color(255, 215, 0), 1.3f, 1.3f});
            }
        }
        if (d < bestD) { bestD = d; best = static_cast<int>(i); }
    }
    if (best >= 0 && bounty > 0) {
        units_[best].gold += bounty;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "+%d", bounty);
        texts_.push_back({victim.pos, buf, sf::Color(255, 215, 0), 0.9f, 0.9f});
    }
}

void Game::updateProjectiles(float dt) {
    for (auto& p : projectiles_) {
        if (p.dead) continue;
        p.life -= dt;
        p.pos += p.vel * dt;
        if (p.life <= 0.f || p.pos.x < -20 || p.pos.x > cfg::WinW + 20 ||
            p.pos.y < -20 || p.pos.y > cfg::WinH + 20) {
            p.dead = true;
            continue;
        }
        for (auto& u : units_) {
            if (!u.alive() || u.team == p.team || u.team == Team::Neutral) continue;
            if (distance(u.pos, p.pos) <= p.radius + u.radius) {
                if (p.splash) { // small AoE
                    for (auto& v : units_) {
                        if (!v.alive() || v.team == p.team) continue;
                        if (distance(v.pos, p.pos) <= 55.f)
                            applyDamage(v, p.damage, p.team, p.pos);
                    }
                } else {
                    applyDamage(u, p.damage, p.team, p.pos);
                }
                p.dead = true;
                break;
            }
        }
    }
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                       [](const Projectile& p) { return p.dead; }),
                       projectiles_.end());
}

void Game::updateEffects(float dt) {
    for (auto& b : beams_) b.life -= dt;
    beams_.erase(std::remove_if(beams_.begin(), beams_.end(),
                 [](const Beam& b) { return b.life <= 0.f; }), beams_.end());
    for (auto& t : texts_) { t.life -= dt; t.pos.y -= 22.f * dt; }
    texts_.erase(std::remove_if(texts_.begin(), texts_.end(),
                 [](const FloatText& t) { return t.life <= 0.f; }), texts_.end());
}

void Game::checkWinCondition() {
    if (radiantAncient_ >= 0 && units_[radiantAncient_].hp <= 0.f) phase_ = Phase::DireWin;
    if (direAncient_ >= 0 && units_[direAncient_].hp <= 0.f) phase_ = Phase::RadiantWin;
}

// -------------------------------------------------------------- helpers -----

int Game::findNearestEnemy(const Unit& u, float withinRange) const {
    int best = -1; float bestD = withinRange;
    Team foe = otherTeam(u.team);
    for (std::size_t i = 0; i < units_.size(); ++i) {
        const Unit& v = units_[i];
        if (!v.alive() || v.team != foe) continue;
        float d = distance(u.pos, v.pos) - v.radius; // edge distance
        if (d <= bestD) { bestD = d; best = static_cast<int>(i); }
    }
    return best;
}

Vec2 Game::laneWaypoint(Team, int step) const {
    int s = static_cast<int>(clampf(static_cast<float>(step), 0.f,
                                    static_cast<float>(laneStepCount() - 1)));
    return lane_[static_cast<std::size_t>(s)];
}

sf::Color Game::teamColor(Team t) const {
    if (t == Team::Radiant) return sf::Color(90, 200, 110);
    if (t == Team::Dire) return sf::Color(210, 80, 80);
    return sf::Color(180, 180, 90);
}

} // namespace mb
