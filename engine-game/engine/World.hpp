// World / entity manager: owns all entities, steps the fixed-tick simulation,
// and provides spawn / query helpers.
#pragma once
#include "Entity.hpp"
#include <memory>
#include <vector>
#include <functional>

namespace eng {

class World {
public:
    double time = 0.0;     // accumulated simulation time (seconds)
    int    tick = 0;       // tick counter
    float  tickInterval = 1.0f / 64.0f;

    // Create an entity of type T, register it, call Spawn(), return it.
    template <class T, class... Args>
    T* Create(Args&&... args) {
        auto up = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = up.get();
        raw->id = nextId_++;
        raw->world = this;
        entities_.push_back(std::move(up));
        raw->Spawn();
        return raw;
    }

    // Advance the world by one fixed tick: Update(), due Think(), touch tests,
    // then remove entities flagged during the tick.
    void Simulate();

    void ForEach(const std::function<void(Entity&)>& fn) {
        for (auto& e : entities_) if (!e->removed) fn(*e);
    }

    std::vector<Entity*> FindInRadius(
        Vector3 center, float r,
        const std::function<bool(Entity&)>& filter = nullptr) const;

    Entity* Get(EntityId id) const;
    std::size_t Count() const { return entities_.size(); }
    void Clear() { entities_.clear(); nextId_ = 1; time = 0; tick = 0; }

private:
    std::vector<std::unique_ptr<Entity>> entities_;
    EntityId nextId_ = 1;
};

} // namespace eng
