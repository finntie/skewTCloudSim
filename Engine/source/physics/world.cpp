#include "physics/world.hpp"

#include "core/engine.hpp"
#include "core/geometry2d.hpp"
#include "core/transform.hpp"
#include "physics/physics_components.hpp"
#ifdef BEE_DEBUG
#include "rendering/debug_render.hpp"
#endif
#ifdef BEE_INSPECTOR
#include "tools/inspector.hpp"
#endif

using namespace glm;
using namespace bee::geometry2d;
using namespace bee::physics;

bool CollisionCheckDiskDisk(const vec2& center1, float radius1, const vec2& center2, float radius2, CollisionData& result)
{
    // check for overlap
    vec2 diff(center1 - center2);
    float l2 = length2(diff);
    float r = radius1 + radius2;
    if (l2 >= r * r) return false;

    // compute collision details
    result.normal = normalize(diff);
    result.depth = r - sqrt(l2);
    result.contactPoint = center2 + result.normal * radius2;

    return true;
}

bool CollisionCheckDiskCapsule(const vec2& center1,
                               float radius1,
                               const vec2& pos2A,
                               const vec2& pos2B,
                               float radius2,
                               CollisionData& result)
{
    // check for overlap
    const vec2& nearest = GetNearestPointOnLineSegment(center1, pos2A, pos2B);
    vec2 diff(center1 - nearest);
    float l2 = length2(diff);
    float r = radius1 + radius2;
    if (l2 >= r * r) return false;

    // compute collision details
    float l = sqrt(l2);
    result.normal = diff / l;
    result.depth = r - l;
    result.contactPoint = nearest + result.normal * radius2;

    return true;
}

bool CollisionCheckDiskPolygon(const vec2& diskCenter, float diskRadius, const Polygon& polygonPoints, CollisionData& result)
{
    const vec2& nearest = GetNearestPointOnPolygonBoundary(diskCenter, polygonPoints);
    vec2 diff(diskCenter - nearest);
    float l2 = length2(diff);

    if (IsPointInsidePolygon(diskCenter, polygonPoints))
    {
        float l = sqrt(l2);
        result.normal = -diff / l;
        result.depth = l + diskRadius;
        result.contactPoint = nearest;
        return true;
    }

    if (l2 >= diskRadius * diskRadius) return false;

    // compute collision details
    float l = sqrt(l2);
    result.normal = diff / l;
    result.depth = diskRadius - l;
    result.contactPoint = nearest;
    return true;
}

bool CollisionCheckCapsuleCapsule(const vec2& pos1A,
                                  const vec2& pos1B,
                                  float radius1,
                                  const vec2& pos2A,
                                  const vec2& pos2B,
                                  float radius2,
                                  CollisionData& result)
{
    // check for overlap
    const auto& [p1, p2] = GetNearestPointPairBetweenLineSegments(pos1A, pos1B, pos2A, pos2B);

    vec2 diff(p1 - p2);
    float l2 = length2(diff);
    float r = radius1 + radius2;
    if (l2 >= r * r) return false;

    if (l2 == 0)  // line segments intersect; find the best way to move capsule 1 out of capsule 2
    {
        const auto& nearestA = GetNearestPointOnLineSegment(pos1A, pos2A, pos2B);
        const auto& nearestB = GetNearestPointOnLineSegment(pos1B, pos2A, pos2B);
        const auto& diffA = pos1A - nearestA;
        const auto& diffB = pos1B - nearestB;
        float distA = length2(diffA), distB = length2(diffB);
        if (distA <= distB)
        {
            result.normal = -diffA / sqrtf(distA);
            result.depth = distA + r;
            result.contactPoint = nearestA;
        }
        else
        {
            result.normal = -diffB / sqrtf(distB);
            result.depth = distB + r;
            result.contactPoint = nearestB;
        }
    }

    else  // line segments don't intersect
    {
        float l = sqrt(l2);
        result.normal = diff / l;
        result.depth = r - l;
        result.contactPoint = p2 + result.normal * radius2;
    }

    return true;
}

struct ProjectionRange
{
    float min;
    float max;
};

ProjectionRange ComputeProjectionRange(const Polygon& polygon, const vec2& axis)
{
    float min = std::numeric_limits<float>::max(), max = -std::numeric_limits<float>::max();
    for (const auto& p : polygon)
    {
        float d = glm::dot(p, axis);
        if (d < min) min = d;
        if (d > max) max = d;
    }
    return {min, max};
}

bool DoRangesOverlap(const ProjectionRange& range1, const ProjectionRange& range2)
{
    return range1.min <= range2.max && range1.max >= range2.min;
}

float GetRangeOverlap(const ProjectionRange& range1, const ProjectionRange& range2)
{
    return std::max(0.f, std::min(range1.max, range2.max) - std::max(range1.min, range2.min));
}

bool CollisionCheckPolygonPolygon_SingleAxis(const Polygon& polygon1,
                                             const Polygon& polygon2,
                                             const vec2& axis,
                                             CollisionData& result,
                                             bool normalShouldPointAwayFrom1)
{
    // project both shapes onto the axis
    const ProjectionRange& p1 = ComputeProjectionRange(polygon1, axis);
    const ProjectionRange& p2 = ComputeProjectionRange(polygon2, axis);

    // if the projections don't overlap, we can guarantee that the shapes do not overlap
    if (!DoRangesOverlap(p1, p2)) return false;

    // get the overlap; if it's the smallest one so far, we have a new best candidate for the collision result
    float overlap = GetRangeOverlap(p1, p2);
    if (overlap < result.depth)
    {
        result.depth = overlap;
        result.normal = axis;

        // make sure the collision normal is pointing away from the right polygon
        if (normalShouldPointAwayFrom1)
        {
            if (p1.min < p2.min && p1.max > p2.min) result.normal = -result.normal;
        }
        else
        {
            if (p2.min < p1.min && p2.max > p1.min) result.normal = -result.normal;
        }
    }

    return true;
}

void CollisionCheckPolygonPolygon_ComputeContactPoint(const Polygon& polygon1, const Polygon& polygon2, CollisionData& result)
{
    // vector by which polygon1 would need to be displaced to resolve the collision
    const auto& offset = result.normal * result.depth;

    // after displacement, what is the closest point on polygon2's boundary from polygon1?
    float bestDist = std::numeric_limits<float>::max();
    for (const auto& p1 : polygon1)
    {
        const auto& p1b = p1 + offset;
        const auto& n = GetNearestPointOnPolygonBoundary(p1b, polygon2);
        float d = distance2(p1b, n);
        if (d < bestDist)
        {
            bestDist = d;
            result.contactPoint = n;
        }
    }

    // and what is the closest point on polygon1's boundary from polygon2?
    for (const auto& p2 : polygon2)
    {
        const auto& p2b = p2 - offset;
        const auto& n = GetNearestPointOnPolygonBoundary(p2b, polygon1);
        float d = distance2(p2b, n);
        if (d < bestDist)
        {
            bestDist = d;
            result.contactPoint = n + offset;
        }
    }
}

bool CollisionCheckPolygonPolygon(const Polygon& polygon1,
                                  const std::vector<glm::vec2>& normals1,
                                  const Polygon& polygon2,
                                  const std::vector<glm::vec2>& normals2,
                                  CollisionData& result)
{
    result.depth = std::numeric_limits<float>::max();

    // Separating Axis Theorem, implementation based on https://dyn4j.org/2010/01/sat/
    for (const auto& axis : normals1)
        if (!CollisionCheckPolygonPolygon_SingleAxis(polygon1, polygon2, axis, result, true)) return false;
    for (const auto& axis : normals2)
        if (!CollisionCheckPolygonPolygon_SingleAxis(polygon2, polygon1, axis, result, false)) return false;

    // If we arrive here, all axes gave a range overlap, so there's a collision.
    // We'll use the axis/normal that gave the smallest overlap, but we don't have the contact point yet.
    CollisionCheckPolygonPolygon_ComputeContactPoint(polygon1, polygon2, result);

    return true;
}

bool CollisionCheckCapsulePolygon(const vec2& capsulePointA,
                                  const vec2& capsulePointB,
                                  float capsuleRadius,
                                  const Polygon& polygon,
                                  const std::vector<vec2>& normals,
                                  CollisionData& result)
{
    // check collision at endpoints
    CollisionData resultA;
    bool collisionA = CollisionCheckDiskPolygon(capsulePointA, capsuleRadius, polygon, resultA);
    CollisionData resultB;
    bool collisionB = CollisionCheckDiskPolygon(capsulePointB, capsuleRadius, polygon, resultB);

    // check collision for the rectangle inbetween
    const vec2& dir = normalize(capsulePointB - capsulePointA);
    const vec2& side = GetPerpendicularVector(dir);
    const vec2& offset = side * capsuleRadius;
    Polygon rect{
        capsulePointA - offset,
        capsulePointB - offset,
        capsulePointB + offset,
        capsulePointA + offset,
    };
    Polygon rectNormals{-side, dir, side, -dir};
    CollisionData resultRect;
    bool collisionRect = CollisionCheckPolygonPolygon(rect, rectNormals, polygon, normals, resultRect);

    // pick the option with the highest penetration depth
    result.depth = -std::numeric_limits<float>::max();
    if (collisionA) result = resultA;
    if (collisionB && resultB.depth > result.depth) result = resultB;
    if (collisionRect && resultRect.depth > result.depth) result = resultRect;

    return collisionA || collisionB || collisionRect;
}

void World::ResolveCollision(const CollisionData& collision,
                             Body& body1,
                             Body& body2,
                             PolygonCollider* polygon1,
                             PolygonCollider* polygon2)
{
#ifdef BEE_DEBUG
    Engine.DebugRenderer().AddCircle(DebugCategory::Physics,
                                     vec3(collision.contactPoint, 0.15f),
                                     0.25f,
                                     vec3(0, 0, 1),
                                     vec4(1, 0, 0, 0));
    Engine.DebugRenderer().AddLine(DebugCategory::Physics,
                                   vec3(collision.contactPoint, 0.15f),
                                   vec3(collision.contactPoint + collision.normal, 0.15f),
                                   vec4(1, 0, 0, 0));
#endif

    // if both bodies are not dynamic, there's nothing left to do
    if (body1.GetType() != Body::Type::Dynamic && body2.GetType() != Body::Type::Dynamic) return;

    // displace the objects to resolve overlap
    float m1 = body1.GetInvMass();
    float m2 = body2.GetInvMass();
    float totalInvMass = m1 + m2;
    const vec2& dist = (collision.depth / totalInvMass) * collision.normal;

    if (body1.GetType() == Body::Type::Dynamic)
    {
        body1.SetPosition(body1.GetPosition() + dist * m1);
        if (polygon1) polygon1->ComputeWorldPoints(body1.m_position, body1.m_angle);
    }
    if (body2.GetType() == Body::Type::Dynamic)
    {
        body2.SetPosition(body2.GetPosition() - dist * m2);
        if (polygon2) polygon2->ComputeWorldPoints(body2.m_position, body2.m_angle);
    }

    const vec2& p1 = collision.contactPoint - body1.m_position;
    const vec2& p2 = collision.contactPoint - body2.m_position;
    const vec2& perp1 = GetPerpendicularVector(p1);
    const vec2& perp2 = GetPerpendicularVector(p2);

    // compute total velocities at the point of collision
    const vec2& v1 = body1.GetLinearVelocity() + body1.GetAngularVelocity() * perp1;
    const vec2& v2 = body2.GetLinearVelocity() + body2.GetAngularVelocity() * perp2;

    // compute and apply impulses
    float dotVelocity = glm::dot(v1 - v2, collision.normal);
    if (dotVelocity < 0)
    {
        float restitution = std::min(body1.GetRestitution(), body2.GetRestitution());
        float perpDot1 = dot(perp1, collision.normal);
        float perpDot2 = dot(perp2, collision.normal);
        float inertiaFactor1 = perpDot1 * perpDot1 * body1.GetInvMomentOfInertia();
        float inertiaFactor2 = perpDot2 * perpDot2 * body2.GetInvMomentOfInertia();
        float j = -(1 + restitution) * dotVelocity / (totalInvMass + inertiaFactor1 + inertiaFactor2);

        body1.ApplyLinearImpulse(j * collision.normal);
        body1.ApplyAngularImpulse(j * perpDot1);

        body2.ApplyLinearImpulse(-j * collision.normal);
        body2.ApplyAngularImpulse(-j * perpDot2);
    }
}

void World::RegisterCollision(CollisionData& collision, const Entity& entity1, Body& body1, const Entity& entity2, Body& body2)
{
    // store references to the entities in the CollisionData object
    collision.entity1 = entity1;
    collision.entity2 = entity2;

    // store collision data in both bodies, also if they are kinematic (implying custom collision resolution)
    if (body1.GetType() != Body::Type::Static) body1.AddCollisionData(collision);
    if (body2.GetType() != Body::Type::Static)
        body2.AddCollisionData(
            CollisionData{collision.entity2, collision.entity1, -collision.normal, collision.depth, collision.contactPoint});
}

void World::UpdateCollisionDetection()
{
    const auto& view_disk = Engine.ECS().Registry.view<Body, DiskCollider>();
    const auto& view_capsule = Engine.ECS().Registry.view<Body, CapsuleCollider>();
    const auto& view_polygon = Engine.ECS().Registry.view<Body, PolygonCollider>();

    for (const auto& [entity1, body1, disk1] : view_disk.each())
    {
        // --- disk-disk collisions
        for (const auto& [entity2, body2, disk2] : view_disk.each())
        {
            // avoid duplicate checks
            if (entity1 >= entity2) continue;
            if (body1.GetType() == Body::Type::Static && body2.GetType() == Body::Type::Static) continue;

            CollisionData collision;
            if (CollisionCheckDiskDisk(body1.GetPosition(),
                                       disk1.GetRadius(),
                                       body2.GetPosition(),
                                       disk2.GetRadius(),
                                       collision))
            {
                ResolveCollision(collision, body1, body2);
                RegisterCollision(collision, entity1, body1, entity2, body2);
            }
        }

        // --- disk-capsule collisions
        for (const auto& [entity2, body2, capsule2] : view_capsule.each())
        {
            if (body1.GetType() == Body::Type::Static && body2.GetType() == Body::Type::Static) continue;

            const vec2& halfOffset2 = RotateCounterClockwise(vec2(0, 0.5f * capsule2.GetHeight()), body2.m_angle);
            const vec2& pos2A = body2.GetPosition() - halfOffset2;
            const vec2& pos2B = body2.GetPosition() + halfOffset2;
            CollisionData collision;
            if (CollisionCheckDiskCapsule(body1.GetPosition(),
                                          disk1.GetRadius(),
                                          pos2A,
                                          pos2B,
                                          capsule2.GetRadius(),
                                          collision))
            {
                ResolveCollision(collision, body1, body2);
                RegisterCollision(collision, entity1, body1, entity2, body2);
            }
        }

        // --- disk-polygon collisions
        for (const auto& [entity2, body2, polygon2] : view_polygon.each())
        {
            if (body1.GetType() == Body::Type::Static && body2.GetType() == Body::Type::Static) continue;

            CollisionData collision;
            if (CollisionCheckDiskPolygon(body1.GetPosition(), disk1.GetRadius(), polygon2.GetWorldPoints(), collision))
            {
                ResolveCollision(collision, body1, body2, nullptr, &polygon2);
                RegisterCollision(collision, entity1, body1, entity2, body2);
            }
        }
    }

    for (const auto& [entity1, body1, capsule1] : view_capsule.each())
    {
        // --- capsule-capsule collisions
        for (const auto& [entity2, body2, capsule2] : view_capsule.each())
        {
            // avoid duplicate checks
            if (entity1 >= entity2) continue;
            if (body1.GetType() == Body::Type::Static && body2.GetType() == Body::Type::Static) continue;

            const vec2& halfOffset1 = RotateCounterClockwise(vec2(0, 0.5f * capsule1.GetHeight()), body1.m_angle);
            const vec2& pos1A = body1.GetPosition() - halfOffset1;
            const vec2& pos1B = body1.GetPosition() + halfOffset1;
            const vec2& halfOffset2 = RotateCounterClockwise(vec2(0, 0.5f * capsule2.GetHeight()), body2.m_angle);
            const vec2& pos2A = body2.GetPosition() - halfOffset2;
            const vec2& pos2B = body2.GetPosition() + halfOffset2;

            CollisionData collision;
            if (CollisionCheckCapsuleCapsule(pos1A, pos1B, capsule1.GetRadius(), pos2A, pos2B, capsule2.GetRadius(), collision))
            {
                ResolveCollision(collision, body1, body2);
                RegisterCollision(collision, entity1, body1, entity2, body2);
            }
        }

        // --- capsule-polygon collisions
        for (const auto& [entity2, body2, polygon2] : view_polygon.each())
        {
            if (body1.GetType() == Body::Type::Static && body2.GetType() == Body::Type::Static) continue;

            const vec2& halfOffset1 = RotateCounterClockwise(vec2(0, 0.5f * capsule1.GetHeight()), body1.m_angle);
            const vec2& pos1A = body1.GetPosition() - halfOffset1;
            const vec2& pos1B = body1.GetPosition() + halfOffset1;

            CollisionData collision;
            if (CollisionCheckCapsulePolygon(pos1A,
                                             pos1B,
                                             capsule1.GetRadius(),
                                             polygon2.GetWorldPoints(),
                                             polygon2.GetWorldNormals(),
                                             collision))
            {
                ResolveCollision(collision, body1, body2, nullptr, &polygon2);
                RegisterCollision(collision, entity1, body1, entity2, body2);
            }
        }
    }

    for (const auto& [entity1, body1, polygon1] : view_polygon.each())
    {
        // --- polygon-polygon collisions
        for (const auto& [entity2, body2, polygon2] : view_polygon.each())
        {
            // avoid duplicate checks
            if (entity1 >= entity2) continue;
            if (body1.GetType() == Body::Type::Static && body2.GetType() == Body::Type::Static) continue;

            CollisionData collision;
            if (CollisionCheckPolygonPolygon(polygon1.GetWorldPoints(),
                                             polygon1.GetWorldNormals(),
                                             polygon2.GetWorldPoints(),
                                             polygon2.GetWorldNormals(),
                                             collision))
            {
                ResolveCollision(collision, body1, body2, &polygon1, &polygon2);
                RegisterCollision(collision, entity1, body1, entity2, body2);
            }
        }
    }
}

void World::Update(float dt)
{
    const auto& view = Engine.ECS().Registry.view<Body>();

    // determine how many frames to simulate
    m_hasExecutedFrame = false;
    m_timeSinceLastFrame += dt;

    if (m_timeSinceLastFrame >= m_fixedDeltaTime)
    {
        // clear collision data from previous frame
        for (const auto& [entity, body] : view.each())
        {
            body.ClearCollisionData();
        }
    }

    while (m_timeSinceLastFrame >= m_fixedDeltaTime)
    {
        // update world coordinates of polygons
        auto polygons = Engine.ECS().Registry.view<Body, PolygonCollider>();
        for (const auto& [entity, body, polygon] : polygons.each())
        {
            polygon.ComputeWorldPoints(body.m_position, body.m_angle);
        }

        // apply gravity
        if (m_gravity != vec2(0, 0))
        {
            for (const auto& [entity, body] : view.each())
            {
                if (body.GetType() == Body::Type::Dynamic) body.AddForce(m_gravity / body.m_invMass);
            }
        }

        // update velocity and position
        for (const auto& [entity, body] : view.each())
        {
            if (body.GetType() != Body::Type::Static) body.Update(m_fixedDeltaTime);
        }

        // update world coordinates of polygons again
        for (const auto& [entity, body, polygon] : polygons.each())
        {
            polygon.ComputeWorldPoints(body.m_position, body.m_angle);
        }

        // collision detection and resolution
        UpdateCollisionDetection();

        // reset data for next frame
        for (const auto& [entity, body] : view.each())
        {
            body.ClearForceAndTorque();
        }

        // update internal timer
        m_timeSinceLastFrame -= m_fixedDeltaTime;

        m_hasExecutedFrame = true;
    }

    // debug rendering of physics objects
#ifdef BEE_DEBUG

    std::vector<vec4> typeColors = {vec4(0, 1, 0, 1), vec4(1, 0, 1, 1), vec4(1, 0, 0, 1)};

    const auto& view_disk = Engine.ECS().Registry.view<Body, DiskCollider>();
    const auto& view_polygon = Engine.ECS().Registry.view<Body, PolygonCollider>();
    const auto& view_capsule = Engine.ECS().Registry.view<Body, CapsuleCollider>();

    for (const auto& [entity, body, disk] : view_disk.each())
    {
        const vec4& color = typeColors[body.GetType()];
        Engine.DebugRenderer().AddCircle(bee::DebugCategory::Physics,
                                         vec3(body.GetPosition(), 0.01f),
                                         disk.GetRadius(),
                                         vec3(0, 0, 1),
                                         color);
        Engine.DebugRenderer().AddLine(
            bee::DebugCategory::Physics,
            vec3(body.GetPosition(), 0.01f),
            vec3(body.GetPosition() + RotateCounterClockwise(vec2(disk.GetRadius(), 0.f), body.GetRotation()), 0.01f),
            color);
    }

    for (const auto& [entity, body, polygon] : view_polygon.each())
    {
        const vec4& color = typeColors[body.GetType()];
        size_t n = polygon.GetLocalPoints().size();
        for (size_t i = 0; i < n; ++i)
        {
            Engine.DebugRenderer().AddCircle(
                bee::DebugCategory::Physics,
                vec3(body.GetPosition() + RotateCounterClockwise(polygon.GetLocalPoints()[i], body.GetRotation()), 0.01f),
                0.1f,
                vec3(0, 0, 1),
                color);
            Engine.DebugRenderer().AddLine(
                bee::DebugCategory::Physics,
                vec3(body.GetPosition() + RotateCounterClockwise(polygon.GetLocalPoints()[i], body.GetRotation()), 0.01f),
                vec3(body.GetPosition() + RotateCounterClockwise(polygon.GetLocalPoints()[(i + 1) % n], body.GetRotation()),
                     0.01f),
                color);
        }
    }

    for (const auto& [entity, body, capsule] : view_capsule.each())
    {
        const vec4& color = typeColors[body.GetType()];
        const vec2& halfOffset = RotateCounterClockwise(vec2(0, 0.5f * capsule.GetHeight()), body.m_angle);
        const vec2& p1 = body.GetPosition() - halfOffset;
        const vec2& p2 = body.GetPosition() + halfOffset;
        const vec2& side = RotateCounterClockwise(vec2(capsule.GetRadius(), 0), body.m_angle);

        Engine.DebugRenderer().AddCircle(bee::DebugCategory::Physics,
                                         vec3(p1, 0.01f),
                                         capsule.GetRadius(),
                                         vec3(0, 0, 1),
                                         color);
        Engine.DebugRenderer().AddCircle(bee::DebugCategory::Physics,
                                         vec3(p2, 0.01f),
                                         capsule.GetRadius(),
                                         vec3(0, 0, 1),
                                         color);
        Engine.DebugRenderer().AddLine(bee::DebugCategory::Physics, vec3(p1 - side, 0.01f), vec3(p2 - side, 0.01f), color);
        Engine.DebugRenderer().AddLine(bee::DebugCategory::Physics, vec3(p1 + side, 0.01f), vec3(p2 + side, 0.01f), color);
    }

#endif

    // synchronize transforms with physics bodies.
    // Note: For now, we simply snap to the last known physics position. You can still add interpolation if you want.
    for (const auto& [entity, body, transform] : Engine.ECS().Registry.view<Body, Transform>().each())
    {
        if (body.GetType() != Body::Type::Static)
        {
            transform.SetTranslation(vec3(body.GetPosition().x, body.GetPosition().y, transform.GetTranslation().z));
            transform.SetRotation(angleAxis(body.GetRotation(), vec3(0, 0, 1)));
        }
    }
}

#ifdef BEE_INSPECTOR

void World::OnEntity(bee::Entity entity)
{
    auto* body = Engine.ECS().Registry.try_get<Body>(entity);
    if (body)
    {
        if (ImGui::CollapsingHeader("Physics Body", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Engine.Inspector().Inspect("Body Position", body->m_position);
            Engine.Inspector().Inspect("Inv Mass", body->m_invMass);
            Engine.Inspector().Inspect("Inv Moment of Inertia", body->m_invMomentOfInertia);
            Engine.Inspector().Inspect("Restitution", body->m_restitution);
        }
    }
}

#endif