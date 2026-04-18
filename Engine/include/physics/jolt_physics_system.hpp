#pragma once

#ifdef BEE_JOLT_PHYSICS


// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Math/Vec3.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/vec3.hpp>

#include "core/ecs.hpp"
#include "tools/inspectable.hpp"

namespace JPH
{
class BodyInterface;
class PhysicsSystem;
class TempAllocatorImpl;
class JobSystem;
class ShapeSettings;
class Shape;
}  // namespace JPH

namespace bee
{
struct Transform;
}

namespace bee::physics
{

/// <summary>
/// An ECS component indicating a Jolt physics body.
/// It only stores an ID; any conversions are handled by JoltSystem.
/// </summary>
struct JoltBody
{
    uint32_t m_bodyID;
};

// (Taken from Jolt samples)
// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace JoltLayers
{
static constexpr uint16_t NON_MOVING = 0;
static constexpr uint16_t MOVING = 1;
static constexpr uint16_t NUM_LAYERS = 2;
};  // namespace JoltLayers

/// <summary>
/// An ECS system that runs the Jolt physics engine.
/// </summary>
class JoltSystem : public bee::System, bee::IEntityInspector
{
public:
    JoltSystem(float fixedDeltaTime = 1.f / 60.f);
    virtual ~JoltSystem() override;
    void Update(float dt) override;
#ifdef BEE_DEBUG
    void Render() override;
#endif
#ifdef BEE_INSPECTOR
    void OnEntity(bee::Entity entity) override;
#endif

    static JPH::PhysicsSystem* GetInternalSystem() { return m_physicsSystem; }
    inline bool HasExecutedFrame() const { return m_hasExecutedFrame; }
    inline float GetFixedDeltaTime() const { return m_fixedDeltaTime; }

    void AddPhysicsBody(bee::Entity entity,
                        const bee::Transform& transform,
                        const JPH::ShapeSettings& shapeSettings,
                        float friction,
                        float restitution,
                        bool isStatic);

    void SetBodyPosition(const JoltBody& body, const glm::vec3& pos);
    void SetBodyRotation(const JoltBody& body, const glm::quat& rot);
    void SetBodyVelocity(const JoltBody& body, const glm::vec3& vel);

private:
    void DebugDrawShape(const JPH::Shape* shape,
                        const JoltBody& body,
                        const bee::Transform& transform,
                        const glm::vec3& worldOffset = glm::vec3(0.f));
    static void OnPhysicsBodyDestroy(entt::registry& registry, entt::entity entity);

    static JPH::PhysicsSystem* m_physicsSystem;
    JPH::TempAllocatorImpl* m_tempAllocator;
    JPH::JobSystem* m_jobSystem;

    float m_fixedDeltaTime;
    float m_timeSinceLastUpdate = 0.f;
    bool m_hasExecutedFrame = false;
};

template <typename T>
inline T ToJolt(const glm::vec3& v)
{
    return T(v.x, v.y, v.z);
}
inline glm::vec3 ToGlm(const JPH::Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
inline JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
inline glm::quat ToGlm(const JPH::Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

template <typename T>
inline JPH::Array<T> ToJoltArray(const std::vector<glm::vec3>& vs)
{
    JPH::Array<T> result;
    for (const auto& v : vs) result.push_back(ToJolt<T>(v));
    return result;
}

inline JPH::IndexedTriangleList ToJoltTriangleList(const std::vector<uint16_t>& indices)
{
    JPH::IndexedTriangleList result;
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        result.push_back(JPH::IndexedTriangle((uint32_t)indices[i], (uint32_t)indices[i + 1], (uint32_t)indices[i + 2]));
    }
    return result;
}

}  // namespace bee::physics

#endif  // BEE_JOLT_PHYSICS