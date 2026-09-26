#include "World.hpp"
#include <algorithm>

namespace eng {

void World::Simulate() {
    time += tickInterval;
    ++tick;

    // 1) continuous update
    for (auto& e : entities_)
        if (!e->removed) e->Update(tickInterval);

    // 2) scheduled thinks
    for (auto& e : entities_) {
        if (e->removed) continue;
        if (e->nextThink >= 0.0 && time >= e->nextThink) {
            e->nextThink = -1.0;   // clear; Think() may reschedule
            e->Think();
        }
    }

    // 3) touch tests between solid entities (naive O(n^2); fine at this scale)
    const std::size_t n = entities_.size();
    for (std::size_t i = 0; i < n; ++i) {
        Entity* a = entities_[i].get();
        if (a->removed || !a->solid) continue;
        for (std::size_t j = i + 1; j < n; ++j) {
            Entity* b = entities_[j].get();
            if (b->removed || !b->solid) continue;
            if (Dist(a->pos, b->pos) <= a->radius + b->radius) {
                a->OnTouch(*b);
                b->OnTouch(*a);
            }
        }
    }

    // 4) cull removed entities
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
                       [](const std::unique_ptr<Entity>& e) { return e->removed; }),
        entities_.end());
}

std::vector<Entity*> World::FindInRadius(
    Vector3 center, float r, const std::function<bool(Entity&)>& filter) const {
    std::vector<Entity*> out;
    for (auto& e : entities_) {
        if (e->removed) continue;
        if (Dist(e->pos, center) > r) continue;
        if (filter && !filter(*e)) continue;
        out.push_back(e.get());
    }
    return out;
}

Entity* World::Get(EntityId id) const {
    for (auto& e : entities_)
        if (e->id == id && !e->removed) return e.get();
    return nullptr;
}

} // namespace eng
