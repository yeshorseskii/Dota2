// Small 2D vector helpers built on sf::Vector2f.
#pragma once
#include <SFML/System/Vector2.hpp>
#include <cmath>

namespace mb {

using Vec2 = sf::Vector2f;

inline float dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }

inline float lengthSq(const Vec2& v) { return dot(v, v); }

inline float length(const Vec2& v) { return std::sqrt(lengthSq(v)); }

inline float distance(const Vec2& a, const Vec2& b) { return length(a - b); }

inline float distanceSq(const Vec2& a, const Vec2& b) { return lengthSq(a - b); }

// Unit-length version of v; returns {0,0} for a zero vector.
inline Vec2 normalized(const Vec2& v) {
    const float len = length(v);
    if (len <= 1e-6f) return Vec2(0.f, 0.f);
    return Vec2(v.x / len, v.y / len);
}

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Move `pos` toward `target` by at most `maxStep`; returns the new position.
inline Vec2 moveToward(const Vec2& pos, const Vec2& target, float maxStep) {
    const Vec2 delta = target - pos;
    const float d = length(delta);
    if (d <= maxStep || d <= 1e-6f) return target;
    return pos + delta * (maxStep / d);
}

} // namespace mb
