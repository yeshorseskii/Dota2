// The MOBA as an engine game module (IGame). Owns the map/lane, spawns waves,
// tracks the win state, drives the camera, and offers services (enemy lookup,
// kill rewards) that entities call through the global g_game pointer.
#pragma once
#include "engine/IGame.hpp"
#include "engine/Engine.hpp"
#include "Combat.hpp"
#include "Defs.hpp"
#include <vector>
#include <string>

namespace game {

class Hero;
class Ancient;

enum class Phase { Playing, RadiantWin, DireWin };
enum class Screen { Menu, Game, Paused };

class MobaGame : public eng::IGame {
public:
    ~MobaGame() override;
    const char* Title() const override { return "Mini MOBA — engine build"; }

    void OnInit(eng::Engine& e) override;
    void OnInput(eng::Engine& e) override;
    void OnFrame(eng::Engine& e, float dt) override;
    void OnTick(eng::World& w) override;
    void OnRender3D() override;
    void OnRenderHUD() override;
    bool OnEscape() override;
    bool Paused() const override { return screen_ == Screen::Paused; }

    // Services used by entities.
    CombatEntity* NearestEnemy(Team team, Vector3 pos, float range);
    void AreaDamage(Team from, Vector3 center, float radius, float dmg);
    void OnKill(CombatEntity& victim, Team killer);
    void SpawnBeam(Vector3 a, Vector3 b, Color c);
    void SpawnText(Vector3 at, const std::string& s, Color c, float life = 0.8f);
    void SpawnParticles(Vector3 at, Color c, int count, float speed, float life = 0.6f, float grav = 0.f);
    void SpawnRing(Vector3 at, float r0, float r1, Color c, float life);
    // AoE damage + optional crowd control on all enemies of `from` in radius.
    void AreaEffect(Team from, Vector3 center, float radius, float dmg,
                    float slowDur, float slowMul, float stunDur);

    Vector3 LaneWp(int lane, int step) const;
    int LaneCount(int lane) const;
    int NumLanes() const { return (int)lanes_.size(); }
    Vector3 MapCenter() const { return {90, 0, 90}; }
    Camera3D& Cam() { return engine_->camera(); }
    eng::World& World() { return engine_->world(); }
    eng::Scene& scene() { return engine_->scene(); }
    Hero* Player() { return player_; }
    void SetWin(Phase p) { if (phase_ == Phase::Playing) phase_ = p; }

    // Character data (read by entities on spawn, edited by the in-game editor).
    Defs& defs() { return defs_; }

    // Console-facing controls.
    void Restart() { reset(); }
    void ForceWin(Team t) { phase_ = (t == Team::Radiant) ? Phase::RadiantWin : Phase::DireWin; }

private:
    void reset();
    void spawnMap();
    void spawnWave();
    void spawnCamps();
    void spawnCampAt(Vector3 c);
    void spawnBoss();
    void updateNeutrals(float dt);
    int  countNeutralsNear(Vector3 c, float r);
    void loadDefs();
    void applyDefsToLiveUnits();
    void drawEditor();
    void makeGround();   // build the procedural ground texture + plane model
    void drawMenu();   // main menu overlay
    void drawPause();  // pause overlay
    void drawShop();   // item shop overlay

    eng::Engine* engine_ = nullptr;
    std::vector<std::vector<Vector3>> lanes_;  // top / mid / bottom waypoint paths
    std::vector<Vector3> trees_;               // decorative jungle props
    Vector3 bossPit_{90, 0, 118};              // neutral boss location
    Hero* player_ = nullptr;
    Hero* enemy_ = nullptr;
    Ancient* radiantAncient_ = nullptr;
    Ancient* direAncient_ = nullptr;
    Phase phase_ = Phase::Playing;
    float waveTimer_ = 0.f;
    int waveCount_ = 0;
    std::vector<Vector3> camps_;   // neutral jungle camp centres
    float neutralTimer_ = 0.f;     // camp respawn countdown
    float bossTimer_ = 0.f;        // boss respawn countdown
    float goldTimer_ = 0.f;        // passive gold tick
    bool shopOpen_ = false;
    std::string shopMsg_;

    Defs defs_;
    std::string defsPath_;    // resolved characters.txt path (for Save)
    bool editorOpen_ = false;
    int editSel_ = 0;         // 0=hero 1=creep 2=tower 3=ancient
    std::string editorMsg_;   // last save/reload status line

    Texture2D groundTex_{};
    Model groundModel_{};
    bool groundReady_ = false;

    Screen screen_ = Screen::Menu;
};

extern MobaGame* g_game;

} // namespace game
