#include "Scene.hpp"
#include "raymath.h"
#include "rlgl.h"

namespace eng {

static const char* kVS = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
void main() {
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* kFS = R"(#version 330
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 ambient;
uniform vec3 viewPos;
out vec4 finalColor;
void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-sunDir);
    float diff = max(dot(N, L), 0.0);
    vec3 V = normalize(viewPos - fragPosition);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 24.0) * 0.25;
    // soft wrap + subtle rim for a stylized look
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.15;
    vec4 tex = texture(texture0, fragTexCoord);
    vec3 base = tex.rgb * colDiffuse.rgb * fragColor.rgb;
    vec3 lit = base * (ambient + sunColor * diff) + sunColor * spec + base * rim;
    finalColor = vec4(lit, tex.a * colDiffuse.a * fragColor.a);
}
)";

void Scene::Init() {
    shader = LoadShaderFromMemory(kVS, kFS);
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    locViewPos_ = GetShaderLocation(shader, "viewPos");
    locSunDir_ = GetShaderLocation(shader, "sunDir");
    locSunCol_ = GetShaderLocation(shader, "sunColor");
    locAmbient_ = GetShaderLocation(shader, "ambient");

    prims_[(int)Prim::Sphere] = LoadModelFromMesh(GenMeshSphere(0.5f, 16, 16));
    prims_[(int)Prim::Cube] = LoadModelFromMesh(GenMeshCube(1, 1, 1));
    prims_[(int)Prim::Cylinder] = LoadModelFromMesh(GenMeshCylinder(0.5f, 1.0f, 16));
    prims_[(int)Prim::Cone] = LoadModelFromMesh(GenMeshCone(0.5f, 1.0f, 16));
    for (auto& m : prims_) m.materials[0].shader = shader;
    ready = true;
}

void Scene::Unload() {
    if (!ready) return;
    for (auto& m : prims_) UnloadModel(m);
    UnloadShader(shader);
    ready = false;
}

void Scene::SetSun(Vector3 dir, Color color, Color ambient) {
    sunDir_ = Vector3Normalize(dir);
    sunCol_ = {color.r / 255.f, color.g / 255.f, color.b / 255.f};
    ambient_ = {ambient.r / 255.f, ambient.g / 255.f, ambient.b / 255.f};
}

void Scene::Update(const Camera3D& cam) {
    if (!ready) return;
    float vp[3] = {cam.position.x, cam.position.y, cam.position.z};
    SetShaderValue(shader, locViewPos_, vp, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, locSunDir_, &sunDir_, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, locSunCol_, &sunCol_, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, locAmbient_, &ambient_, SHADER_UNIFORM_VEC3);
}

void Scene::Draw(Prim p, Vector3 pos, Vector3 scale, Color tint) {
    if (!ready) return;
    // GenMeshCylinder/Cone grow along +Y from the base; sphere/cube are centered.
    DrawModelEx(prims_[(int)p], pos, {0, 1, 0}, 0.f, scale, tint);
}

void Scene::ApplyShader(Model& m) {
    if (!ready) return;
    for (int i = 0; i < m.materialCount; ++i) m.materials[i].shader = shader;
}

} // namespace eng
