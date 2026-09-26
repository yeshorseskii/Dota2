// Lit 3D scene helper: a Blinn-Phong shader (embedded GLSL) plus a small kit of
// reusable lit primitive models (sphere / cube / cylinder / cone). Games compose
// their unit/prop models from these primitives so everything is lit and can be
// textured — no external art files required.
#pragma once
#include "raylib.h"

namespace eng {

enum class Prim { Sphere, Cube, Cylinder, Cone };

class Scene {
public:
    void Init();                 // load shader + build primitive models (needs a GL context)
    void Unload();

    // Per-frame light + camera setup (call once before drawing the scene).
    void SetSun(Vector3 dir, Color color, Color ambient);
    void Update(const Camera3D& cam);

    // Draw a lit primitive at (pos) with (scale), tinted. Respects the current
    // rlgl matrix, so it can be nested inside a model's transform.
    void Draw(Prim p, Vector3 pos, Vector3 scale, Color tint);

    // Apply the lit shader to an externally created model (e.g. a textured
    // ground plane) so it receives the same lighting.
    void ApplyShader(Model& m);

    Shader shader{};
    bool ready = false;

private:
    Model prims_[4]{};
    int locViewPos_ = -1, locSunDir_ = -1, locSunCol_ = -1, locAmbient_ = -1;
    Vector3 sunDir_{-0.6f, -1.0f, -0.4f};
    Vector3 sunCol_{1.0f, 0.97f, 0.9f};
    Vector3 ambient_{0.35f, 0.37f, 0.42f};
};

} // namespace eng
