#pragma once

#include <glm/vec2.hpp>

#include "core/ecs.hpp"
#include "core/geometry2d.hpp"

namespace bee::physics
{

/// <summary>
/// Stores the details of a single collision.
/// </summary>
struct CollisionData
{
    /// <summary>
    /// The ID of the first entity involved in the collision.
    /// </summary>
    bee::Entity entity1;

    /// <summary>
    /// The ID of the second entity involved in the collision.
    /// </summary>
    bee::Entity entity2;

    /// <summary>
    /// The normal vector on the point of contact, pointing away from entity2's physics body.
    /// </summary>
    glm::vec2 normal;

    /// <summary>
    /// The penetration depth of the two physics bodies
    /// (before they were displaced to resolve overlap).
    /// </summary>
    float depth = std::numeric_limits<float>::max();

    /// <summary>
    /// The approximate point of contact of the collision, in world coordinates.
    /// </summary>
    glm::vec2 contactPoint;
};

/// <summary>
/// Interface for objects that can compute mass data. All colliders should implement it.
/// </summary>
struct IComputeMassData
{
    /// <summary>
    /// Given a density (in kg / m^2), computes the mass and moment of inertia of this object.
    /// The contents of this function differ per shape.
    /// </summary>
    /// <param name="density">The input density value.</param>
    /// <param name="result_mass">(Output parameter) The mass of the object.</param>
    /// <param name="result_momentOfInertia">(Output parameter) The moment of inertia of the object.</param>
    virtual void ComputeMassData(float density, float& result_mass, float& result_momentOfInertia) const = 0;
};

/// <summary>
/// A polygon-shaped collider for physics.
/// </summary>
struct PolygonCollider final : public IComputeMassData
{
public:
    PolygonCollider(const std::vector<glm::vec2>& pts);

    const std::vector<glm::vec2>& GetLocalPoints() const { return m_pts; }
    const std::vector<glm::vec2>& GetLocalNormals() const { return m_normals; }
    const std::vector<glm::vec2>& GetWorldPoints() const { return m_ptsWorld; }
    const std::vector<glm::vec2>& GetWorldNormals() const { return m_normalsWorld; }

    /// <summary>
    /// Recomputes the world-space version of this polygon's vertices and normals.
    /// </summary>
    void ComputeWorldPoints(const glm::vec2& translation, float rotation);

    void ComputeMassData(float density, float& result_mass, float& result_momentOfInertia) const override;

private:
    /// <summary>
    /// The boundary vertices of the polygon in local coordinates,
    /// i.e. relative to the object's rotation and center of mass.
    /// </summary>
    std::vector<glm::vec2> m_pts;
    /// <summary>
    /// The normal vectors of the polygon in local coordinates,
    /// i.e. relative to the object's rotation.
    /// </summary>
    std::vector<glm::vec2> m_normals;

    /// <summary>
    /// The boundary vertices of the polygon in world coordinates.
    /// These should get updated in every physics frame.
    /// </summary>
    std::vector<glm::vec2> m_ptsWorld;

    /// <summary>
    /// The normal vectors of the polygon in world coordinates.
    /// These should get updated in every physics frame.
    /// </summary>
    std::vector<glm::vec2> m_normalsWorld;
};

/// <summary>
/// A disk-shaped collider for physics.
/// </summary>
struct DiskCollider final : public IComputeMassData
{
public:
    DiskCollider(float radius) : m_radius(radius) {}
    float GetRadius() const { return m_radius; }
    void ComputeMassData(float density, float& result_mass, float& result_momentOfInertia) const override;

private:
    float m_radius;
};

/// <summary>
/// A capsule-shaped collider for physics.
/// </summary>
struct CapsuleCollider final : public IComputeMassData
{
public:
    CapsuleCollider(float radius, float height) : m_radius(radius), m_height(height) {}
    float GetRadius() const { return m_radius; }
    float GetHeight() const { return m_height; }
    void ComputeMassData(float density, float& result_mass, float& result_momentOfInertia) const override;

private:
    float m_radius;
    float m_height;
};

/// <summary>
/// Describes a physics body (with a mass and shape) that lives in the physics world.
/// Add it as a component to your ECS entities.
/// </summary>
class Body
{
    friend class World;

public:
    enum Type
    {
        /// <summary>
        /// Indicates a physics body that does not move and is not affected by forces, as if it has infinite mass.
        /// </summary>
        Static,

        /// <summary>
        /// Indicates a physics body that can move under the influence of forces.
        /// </summary>
        Dynamic,

        /// <summary>
        /// Indicates a physics body that is not affected by forces. It moves purely according to its velocity.
        /// </summary>
        Kinematic
    };

    Body(Type type, const IComputeMassData& massComputer, float density = 1.f, float restitution = 1.0f)
        : m_type(type), m_restitution(restitution)
    {
        float mass = 0.f, momentOfInertia = 0.f;
        if (type != Type::Static) massComputer.ComputeMassData(density, mass, momentOfInertia);
        m_invMass = mass == 0.f ? 0.f : (1.f / mass);
        m_invMomentOfInertia = momentOfInertia == 0.f ? 0.f : (1.f / momentOfInertia);
    }

    Type GetType() const { return m_type; }
    inline float GetInvMass() const { return m_type == Type::Static ? 0.f : m_invMass; }
    inline float GetInvMomentOfInertia() const { return m_rotationEnabled ? m_invMomentOfInertia : 0.f; }
    inline float GetRestitution() const { return m_restitution; }

    inline const glm::vec2& GetPosition() const { return m_position; }
    inline const glm::vec2& GetLinearVelocity() const { return m_linearVelocity; }
    inline float GetRotation() const { return m_angle; }
    inline float GetAngularVelocity() const { return m_angularVelocity; }

    inline void SetPosition(const glm::vec2& pos) { m_position = pos; }
    inline void SetLinearVelocity(const glm::vec2& vel) { m_linearVelocity = vel; }
    inline void SetRotationEnabled(bool value) { m_rotationEnabled = value; }
    inline void SetRotation(float angle) { m_angle = angle; }
    inline void SetAngularVelocity(const float& vel) { m_angularVelocity = vel; }

    inline void AddForce(const glm::vec2& force)
    {
        if (m_type == Dynamic) m_force += force;
    }

    inline void AddForceAtPosition(const glm::vec2& force, const glm::vec2& worldPos)
    {
        AddTorque(geometry2d::PerpDot(force, worldPos));
    }

    inline void AddTorque(float torque)
    {
        if (m_type == Dynamic) m_torque += torque;
    }

    inline void ApplyLinearImpulse(const glm::vec2& imp)
    {
        if (m_type == Dynamic) m_linearVelocity += imp * m_invMass;
    }

    inline void ApplyAngularImpulse(const float imp)
    {
        if (m_type == Dynamic && m_rotationEnabled) m_angularVelocity += imp * m_invMomentOfInertia;
    }

    inline const std::vector<CollisionData>& GetCollisionData() const { return m_collisions; }

private:
    inline void ClearForceAndTorque()
    {
        m_force = {0.f, 0.f};
        m_torque = 0.f;
    }

    inline void AddCollisionData(const CollisionData& data) { m_collisions.push_back(data); }
    inline void ClearCollisionData() { m_collisions.clear(); }

    inline void Update(float dt)
    {
        if (m_type == Dynamic)
        {
            m_linearVelocity += m_force * m_invMass * dt;
            if (m_rotationEnabled) m_angularVelocity += m_torque * m_invMomentOfInertia * dt;
        }
        m_position += m_linearVelocity * dt;
        if (m_rotationEnabled) m_angle += m_angularVelocity * dt;
    }

    Type m_type;
    float m_invMass;
    float m_invMomentOfInertia;
    float m_restitution;

    glm::vec2 m_position = {0.f, 0.f};
    float m_angle = 0.f;
    glm::vec2 m_linearVelocity = {0.f, 0.f};
    bool m_rotationEnabled = false;
    float m_angularVelocity = 0.f;
    glm::vec2 m_force = {0.f, 0.f};
    float m_torque = 0.f;

    std::vector<CollisionData> m_collisions = {};
};

}  // namespace bee::physics