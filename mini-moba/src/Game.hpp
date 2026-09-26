// Top-level game: window, world state, update + render loop.
#pragma once
#include "Entities.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <optional>

namespace mb {

namespace cfg {
constexpr unsigned WinW = 1280;
constexpr unsigned WinH = 720;
constexpr float WaveInterval = 22.f;   // seconds between creep waves
constexpr int CreepsPerWave = 4;
constexpr float CreepSpacing = 0.35f;  // stagger inside a wave (seconds)
constexpr float HeroRespawn = 6.f;     // base respawn seconds
constexpr float QDamage = 60.f;
constexpr float QCooldown = 3.f;
constexpr float WCooldown = 9.f;
constexpr float WHeal = 80.f;
constexpr float WShield = 3.f;         // seconds of half-damage
} // namespace cfg

enum class Phase { Playing, RadiantWin, DireWin };

class Game {
public:
    Game();
    void run();

private:
    // --- setup ---
    void resetWorld();
    void spawnBuildings();
    int  addUnit(const Unit& u); // returns index

    // --- loop ---
    void handleEvents();
    void update(float dt);
    void render();

    // --- systems ---
    void updateWaves(float dt);
    void spawnCreep(Team team, int laneStep);
    void updateHeroInput();
    void updateUnitAI(Unit& u, int selfIdx, float dt);
    void updateHeroAbilities(float dt);
    void updateProjectiles(float dt);
    void updateEffects(float dt);
    void applyDamage(Unit& victim, float dmg, Team from, const Vec2& src);
    void grantKillRewards(const Unit& victim, Team killer);
    void checkWinCondition();

    // --- helpers ---
    int  findNearestEnemy(const Unit& u, float withinRange) const;
    float takenMultiplier(const Unit& u) const; // shield etc.
    Vec2 laneWaypoint(Team team, int step) const;
    int  laneStepCount() const { return static_cast<int>(lane_.size()); }
    Unit& player() { return units_[playerIdx_]; }
    Team enemyOf(Team t) const { return t == Team::Radiant ? Team::Dire : Team::Radiant; }
    sf::Color teamColor(Team t) const;

    // --- rendering helpers ---
    void drawMap();
    void drawUnit(const Unit& u);
    void drawHealthBar(const Unit& u);
    void drawHud();
    void drawText(const std::string& s, float x, float y, unsigned size,
                  sf::Color color, bool centered = false);

    sf::RenderWindow window_;
    sf::Font font_;
    bool hasFont_ = false;

    std::vector<Unit> units_;
    std::vector<Projectile> projectiles_;
    std::vector<Beam> beams_;
    std::vector<FloatText> texts_;
    std::vector<Vec2> lane_;      // waypoints from Radiant ancient -> Dire ancient

    int playerIdx_ = -1;
    int enemyHeroIdx_ = -1;
    int radiantAncient_ = -1;
    int direAncient_ = -1;

    float waveTimer_ = 0.f;
    int waveCount_ = 0;
    // pending staggered creep spawns: (team, laneStep, delay)
    struct PendingSpawn { Team team; int step; float delay; };
    std::vector<PendingSpawn> pending_;

    Phase phase_ = Phase::Playing;
    float matchTime_ = 0.f;
    sf::Clock clock_;

    // Optional headless preview: set MOBA_SHOT_AT=<seconds> and MOBA_SHOT_FILE
    // to capture one frame and exit (used for docs / debugging).
    float shotAt_ = -1.f;
    std::string shotFile_;
};

} // namespace mb
