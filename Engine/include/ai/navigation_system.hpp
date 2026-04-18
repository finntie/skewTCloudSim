#pragma once

#include "core/ecs.hpp"

namespace bee::ai
{
class Navmesh;

/// <summary>A simple ECS component indicating that the associated entity
/// should be included during navmesh generation.</summary>
struct NavmeshElement
{
    enum Type
    {
        Obstacle,
        WalkableArea
    };
    Type m_type;
    NavmeshElement(Type type) : m_type(type) {}
};

/// <summary>System that handles navmesh generation and the AI/navigation loop.</summary>
class NavigationSystem : public bee::System
{
public:
    NavigationSystem(float fixedDeltaTime, float agentRadius);
    ~NavigationSystem() override;
    void Update(float dt) override;

private:
    Navmesh* m_navmesh;

    /// The fixed timestep (in seconds) for AI-related code.
    float m_fixedDeltaTime = 0.1f;
    float m_timeSinceLastFrame = 0.0f;
};
}  // namespace bee::ai