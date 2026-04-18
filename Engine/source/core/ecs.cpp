#include "core/ecs.hpp"

#include "core/transform.hpp"

using namespace bee;
using namespace std;
using namespace entt;

constexpr float kMaxDeltaTime = 1.0f / 30.0f;

EntityComponentSystem::EntityComponentSystem() = default;

bee::EntityComponentSystem::~EntityComponentSystem() = default;

void EntityComponentSystem::DeleteEntity(Entity e)
{
    assert(Registry.valid(e));

    // mark this entity for deletion
    Registry.emplace_or_replace<Delete>(e);

    auto* transform = Registry.try_get<Transform>(e);
    if (transform != nullptr)
    {
        // detach from the parent entity
        transform->SetParent(entt::null);
        // recursively mark child entities as well
        for (auto child : *transform) DeleteEntity(child);
    }
}

void EntityComponentSystem::UpdateSystems(float dt)
{
    dt = min(dt, kMaxDeltaTime);
    for (auto& s : m_systems) s->Update(dt);
}

void EntityComponentSystem::RenderSystems()
{
    for (auto& s : m_systems) s->Render();
}

void EntityComponentSystem::RemovedDeleted()
{
    bool isDeleteQueueEmpty = false;
    while (!isDeleteQueueEmpty)
    {
        // Deleting entities can cause other entities to be deleted,
        // so we need to do this in a loop
        const auto del = Registry.view<Delete>();
        Registry.destroy(del.begin(), del.end());
        isDeleteQueueEmpty = del.empty();
    }
}
