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

    // Services used by entities.
    CombatEntity* NearestEnemy(Team team, Vector3 pos, float range);
    void AreaDamage(Team from, Vector3 center, float radius, float dmg);
    void OnKill(CombatEntity& victim, Team killer);
    void SpawnBeam(Vector3 a, Vector3 b, Color c);
    void SpawnText(Vector3 at, const std::string& s, Color c, float life = 0.8f);

    const std::vector<Vector3>& Lane() const { return lane_; }
    Vector3 LaneWp(int step) const;
    int LaneCount() const { return (int)lane_.size(); }
    Camera3D& Cam() { return engine_->camera(); }
    eng::World& World() { return engine_->world(); }
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
    void loadDefs();
    void applyDefsToLiveUnits();
    void drawEditor();
    void makeGround();   // build the procedural ground texture + plane model

    eng::Engine* engine_ = nullptr;
    std::vector<Vector3> lane_;
    Hero* player_ = nullptr;
    Hero* enemy_ = nullptr;
    Ancient* radiantAncient_ = nullptr;
    Ancient* direAncient_ = nullptr;
    Phase phase_ = Phase::Playing;
    float waveTimer_ = 0.f;
    int waveCount_ = 0;

    Defs defs_;
    std::string defsPath_;    // resolved characters.txt path (for Save)
    bool editorOpen_ = false;
    int editSel_ = 0;         // 0=hero 1=creep 2=tower 3=ancient
    std::string editorMsg_;   // last save/reload status line

    Texture2D groundTex_{};
    Model groundModel_{};
    bool groundReady_ = false;
};

extern MobaGame* g_game;

} // namespace game
