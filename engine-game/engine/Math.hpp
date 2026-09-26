// Small math helpers on top of raylib/raymath used across the engine.
#pragma once
#include "raylib.h"
#include "raymath.h"

namespace eng {

inline float Dist(const Vector3& a, const Vector3& b) { return Vector3Distance(a, b); }

inline float Clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Move `p` toward `t` by at most `step`; returns the new position.
inline Vector3 MoveToward(Vector3 p, Vector3 t, float step) {
    Vector3 d = Vector3Subtract(t, p);
    float len = Vector3Length(d);
    if (len <= step || len < 1e-5f) return t;
    return Vector3Add(p, Vector3Scale(d, step / len));
}

} // namespace eng
