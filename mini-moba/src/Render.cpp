// All drawing for the Game class.
#include "Game.hpp"
#include <algorithm>
#include <cstdio>

namespace mb {

void Game::drawText(const std::string& s, float x, float y, unsigned size,
                    sf::Color color, bool centered) {
    if (!hasFont_) return;
    sf::Text t(s, font_, size);
    t.setFillColor(color);
    if (centered) {
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
    }
    t.setPosition(x, y);
    window_.draw(t);
}

void Game::drawMap() {
    // Background gradient-ish: two team-tinted halves + a mid strip.
    sf::RectangleShape bg(sf::Vector2f(cfg::WinW, cfg::WinH));
    bg.setFillColor(sf::Color(26, 30, 26));
    window_.draw(bg);

    // Radiant / Dire tinted corners.
    sf::CircleShape glowR(360.f);
    glowR.setPosition(lane_.front() - Vec2(360.f, 360.f));
    glowR.setFillColor(sf::Color(40, 70, 45));
    window_.draw(glowR);
    sf::CircleShape glowD(360.f);
    glowD.setPosition(lane_.back() - Vec2(360.f, 360.f));
    glowD.setFillColor(sf::Color(70, 40, 40));
    window_.draw(glowD);

    // The lane as a thick polyline.
    for (std::size_t i = 0; i + 1 < lane_.size(); ++i) {
        Vec2 a = lane_[i], b = lane_[i + 1];
        Vec2 dir = normalized(b - a);
        float len = distance(a, b);
        sf::RectangleShape seg(sf::Vector2f(len, 46.f));
        seg.setOrigin(0.f, 23.f);
        seg.setPosition(a);
        seg.setRotation(std::atan2(dir.y, dir.x) * 180.f / 3.14159265f);
        seg.setFillColor(sf::Color(58, 58, 48));
        window_.draw(seg);
    }
    for (const auto& wp : lane_) {
        sf::CircleShape dot(23.f);
        dot.setOrigin(23.f, 23.f);
        dot.setPosition(wp);
        dot.setFillColor(sf::Color(58, 58, 48));
        window_.draw(dot);
    }
}

void Game::drawHealthBar(const Unit& u) {
    if (u.kind == UnitKind::Ancient) return; // drawn big in HUD-ish way below
    float w = (u.kind == UnitKind::Hero) ? 46.f
              : (u.kind == UnitKind::Tower ? 54.f : 26.f);
    float h = 5.f;
    float frac = clampf(u.hp / u.maxHp, 0.f, 1.f);
    Vec2 top = u.pos - Vec2(w / 2.f, u.radius + 12.f);
    sf::RectangleShape back(sf::Vector2f(w, h));
    back.setPosition(top);
    back.setFillColor(sf::Color(20, 20, 20, 200));
    window_.draw(back);
    sf::RectangleShape fill(sf::Vector2f(w * frac, h));
    fill.setPosition(top);
    fill.setFillColor(teamColor(u.team));
    window_.draw(fill);
}

void Game::drawUnit(const Unit& u) {
    sf::Color col = teamColor(u.team);

    if (u.kind == UnitKind::Ancient) {
        if (u.hp <= 0.f) col = sf::Color(60, 60, 60);
        sf::CircleShape c(u.radius, 6);
        c.setOrigin(u.radius, u.radius);
        c.setPosition(u.pos);
        c.setFillColor(col);
        c.setOutlineThickness(4.f);
        c.setOutlineColor(sf::Color::White);
        window_.draw(c);
        // Ancient HP bar (wide).
        float frac = clampf(u.hp / u.maxHp, 0.f, 1.f);
        sf::RectangleShape back(sf::Vector2f(90.f, 8.f));
        back.setPosition(u.pos - Vec2(45.f, u.radius + 16.f));
        back.setFillColor(sf::Color(20, 20, 20, 220));
        window_.draw(back);
        sf::RectangleShape fill(sf::Vector2f(90.f * frac, 8.f));
        fill.setPosition(u.pos - Vec2(45.f, u.radius + 16.f));
        fill.setFillColor(col);
        window_.draw(fill);
        return;
    }
    if (u.hp <= 0.f && u.kind != UnitKind::Hero) return; // dead creep/tower gone

    if (u.kind == UnitKind::Tower) {
        sf::RectangleShape r(sf::Vector2f(u.radius * 1.6f, u.radius * 1.6f));
        r.setOrigin(u.radius * 0.8f, u.radius * 0.8f);
        r.setPosition(u.pos);
        r.setFillColor(col);
        r.setOutlineThickness(3.f);
        r.setOutlineColor(sf::Color(20, 20, 20));
        window_.draw(r);
        drawHealthBar(u);
        return;
    }

    if (u.kind == UnitKind::Creep) {
        sf::CircleShape c(u.radius);
        c.setOrigin(u.radius, u.radius);
        c.setPosition(u.pos);
        c.setFillColor(col);
        window_.draw(c);
        drawHealthBar(u);
        return;
    }

    // Hero.
    if (u.respawnTimer > 0.f) {
        // Show a faded marker + countdown at the spawn.
        sf::CircleShape c(u.radius);
        c.setOrigin(u.radius, u.radius);
        c.setPosition(u.spawn);
        c.setFillColor(sf::Color(col.r, col.g, col.b, 70));
        window_.draw(c);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(u.respawnTimer) + 1);
        drawText(buf, u.spawn.x, u.spawn.y - 8.f, 18, sf::Color::White, true);
        return;
    }

    // Player gets a selection ring + move-order marker.
    if (u.isPlayer) {
        if (u.hasMoveOrder) {
            sf::CircleShape m(6.f);
            m.setOrigin(6.f, 6.f);
            m.setPosition(u.moveOrder);
            m.setFillColor(sf::Color(90, 200, 110, 180));
            window_.draw(m);
        }
        sf::CircleShape ring(u.radius + 6.f);
        ring.setOrigin(u.radius + 6.f, u.radius + 6.f);
        ring.setPosition(u.pos);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(2.f);
        ring.setOutlineColor(sf::Color(255, 255, 255, 160));
        window_.draw(ring);
    }
    sf::CircleShape c(u.radius);
    c.setOrigin(u.radius, u.radius);
    c.setPosition(u.pos);
    c.setFillColor(col);
    c.setOutlineThickness(3.f);
    c.setOutlineColor(u.shieldTimer > 0.f ? sf::Color(120, 220, 255) : sf::Color::White);
    window_.draw(c);
    drawHealthBar(u);
    // Level tag.
    char lv[8];
    std::snprintf(lv, sizeof(lv), "%d", u.level);
    drawText(lv, u.pos.x, u.pos.y - 1.f, 14, sf::Color::White, true);
}

void Game::drawHud() {
    // Top bar: match time + wave.
    int mm = static_cast<int>(matchTime_) / 60;
    int ss = static_cast<int>(matchTime_) % 60;
    char top[64];
    std::snprintf(top, sizeof(top), "%02d:%02d   wave %d", mm, ss, waveCount_);
    drawText(top, cfg::WinW / 2.f, 18.f, 20, sf::Color(230, 230, 230), true);

    // Ancient HP summary.
    if (radiantAncient_ >= 0 && direAncient_ >= 0) {
        char rr[48], dd[48];
        std::snprintf(rr, sizeof(rr), "Radiant Ancient: %d",
                      static_cast<int>(units_[radiantAncient_].hp));
        std::snprintf(dd, sizeof(dd), "Dire Ancient: %d",
                      static_cast<int>(units_[direAncient_].hp));
        drawText(rr, 16.f, 12.f, 16, sf::Color(120, 220, 140));
        drawText(dd, cfg::WinW - 210.f, 12.f, 16, sf::Color(230, 130, 130));
    }

    // Bottom-left: player stats + ability cooldowns.
    const Unit& h = units_[playerIdx_];
    char stats[96];
    std::snprintf(stats, sizeof(stats), "LV %d   HP %d/%d   Gold %d",
                  h.level, static_cast<int>(std::max(0.f, h.hp)),
                  static_cast<int>(h.maxHp), h.gold);
    drawText(stats, 16.f, cfg::WinH - 58.f, 18, sf::Color(235, 235, 235));

    auto drawCd = [&](const char* key, float cd, float x) {
        char b[24];
        if (cd <= 0.f) std::snprintf(b, sizeof(b), "%s: ready", key);
        else std::snprintf(b, sizeof(b), "%s: %.1f", key, cd);
        drawText(b, x, cfg::WinH - 30.f, 16,
                 cd <= 0.f ? sf::Color(150, 220, 255) : sf::Color(150, 150, 150));
    };
    drawCd("Q bolt", h.qCooldown, 16.f);
    drawCd("W shield", h.wCooldown, 150.f);
    drawText("Right-click: move", cfg::WinW - 200.f, cfg::WinH - 30.f, 15,
             sf::Color(170, 170, 170));
}

void Game::render() {
    window_.clear();
    drawMap();

    // Draw order: buildings, creeps, heroes, so heroes sit on top.
    for (const auto& u : units_) if (u.building()) drawUnit(u);
    for (const auto& u : units_) if (u.kind == UnitKind::Creep && u.alive()) drawUnit(u);
    for (const auto& u : units_) if (u.kind == UnitKind::Hero) drawUnit(u);

    for (const auto& b : beams_) {
        sf::Vertex line[] = {
            sf::Vertex(b.a, sf::Color(b.color.r, b.color.g, b.color.b,
                                      static_cast<sf::Uint8>(255 * (b.life / 0.14f)))),
            sf::Vertex(b.b, sf::Color(b.color.r, b.color.g, b.color.b, 40)),
        };
        window_.draw(line, 2, sf::Lines);
    }
    for (const auto& p : projectiles_) {
        sf::CircleShape c(p.radius);
        c.setOrigin(p.radius, p.radius);
        c.setPosition(p.pos);
        c.setFillColor(p.color);
        window_.draw(c);
    }
    for (const auto& t : texts_) {
        sf::Uint8 a = static_cast<sf::Uint8>(255 * clampf(t.life / t.maxLife, 0.f, 1.f));
        drawText(t.text, t.pos.x, t.pos.y, 16,
                 sf::Color(t.color.r, t.color.g, t.color.b, a), true);
    }

    drawHud();

    if (phase_ != Phase::Playing) {
        sf::RectangleShape veil(sf::Vector2f(cfg::WinW, cfg::WinH));
        veil.setFillColor(sf::Color(0, 0, 0, 150));
        window_.draw(veil);
        const char* msg = phase_ == Phase::RadiantWin ? "RADIANT VICTORY"
                                                       : "DIRE VICTORY";
        sf::Color c = phase_ == Phase::RadiantWin ? sf::Color(120, 230, 140)
                                                  : sf::Color(230, 120, 120);
        drawText(msg, cfg::WinW / 2.f, cfg::WinH / 2.f - 20.f, 54, c, true);
        drawText("Press R to play again, Esc to quit",
                 cfg::WinW / 2.f, cfg::WinH / 2.f + 40.f, 22,
                 sf::Color(220, 220, 220), true);
    }

    window_.display();
}

} // namespace mb
