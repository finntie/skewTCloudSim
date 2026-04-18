#pragma once

#include <glm/vec2.hpp>
#include "tools/inspectable.hpp"
#include "core/ecs.hpp"

namespace bee::physics
{
class Body;
struct CollisionData;
struct PolygonCollider;

/// <summary>
/// An ECS system that runs a simple 2D physics loop.
/// Use this for basic 2D collision detection/resolution with disks, polygons, and capsules.
/// For anything more advanced, consider using a professional physics engine instead, such as Jolt (see
/// bee::physics::JoltSystem).
/// </summary>
class World : public bee::System, bee::IEntityInspector
{
public:
    World(const float fixedDeltaTime) : m_fixedDeltaTime(fixedDeltaTime), m_timeSinceLastFrame(0), m_gravity(glm::vec2(0, 0)) {}


#ifdef BEE_INSPECTOR
    void OnEntity(bee::Entity entity) override;
#endif

    void Update(float dt) override;

    /// <summary>
    /// Sets the gravity vector that will be applied to all dynamic bodies in each frame.
    /// Note: this is not a force, but an acceleration. All dynamic bodies will experience it regardless of their mass.
    /// </summary>
    /// <param name="gravity">The desired acceleration vector, in meters per second squared.</param>
    void SetGravity(const glm::vec2& gravity) { m_gravity = gravity; }

    inline bool HasExecutedFrame() const { return m_hasExecutedFrame; }
    inline float GetFixedDeltaTime() const { return m_fixedDeltaTime; }

private:
    /// The fixed timestep (in seconds) for physics-related code.
    float m_fixedDeltaTime;
    float m_timeSinceLastFrame;
    bool m_hasExecutedFrame = false;

    glm::vec2 m_gravity;

    static void ResolveCollision(const CollisionData& collision,
                                 Body& body1,
                                 Body& body2,
                                 PolygonCollider* polygon1 = nullptr,
                                 PolygonCollider* polygon2 = nullptr);
    static void RegisterCollision(CollisionData& collision,
                                  const bee::Entity& entity1,
                                  Body& body1,
                                  const bee::Entity& entity2,
                                  Body& body2);
    static void UpdateCollisionDetection();
};
}  // namespace bee::physics