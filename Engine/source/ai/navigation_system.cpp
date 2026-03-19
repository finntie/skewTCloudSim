#include "ai/navigation_system.hpp"

#include "ai/navmesh.hpp"
#include "ai/navmesh_agent.hpp"
#include "core/engine.hpp"
#include "core/transform.hpp"
#include "physics/physics_components.hpp"
#ifdef BEE_DEBUG
#include "rendering/debug_render.hpp"
#endif
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

using namespace bee::ai;
using namespace glm;

NavigationSystem::NavigationSystem(float fixedDeltaTime, float agentRadius)
    : m_fixedDeltaTime(fixedDeltaTime), m_timeSinceLastFrame(0)
{
    // get all navmesh input
    geometry2d::PolygonList navmeshObstacles;
    geometry2d::PolygonList navmeshWalkableAreas;

    auto view = Engine.ECS().Registry.view<Transform, physics::PolygonCollider, ai::NavmeshElement>();
    for (const auto& [entity, transform, collider, nav] : view.each())
    {
        // get the polygon boundary vertices in world space
        vec2 pos(transform.GetTranslation());
        float angle = glm::eulerAngles(transform.GetRotation()).z;
        geometry2d::Polygon pts_world = collider.GetLocalPoints();
        for (auto& pt : pts_world) pt = pos + geometry2d::RotateCounterClockwise(pt, angle);

        if (nav.m_type == ai::NavmeshElement::Type::Obstacle)
            navmeshObstacles.push_back(pts_world);
        else
            navmeshWalkableAreas.push_back(pts_world);
    }

    // build the navmesh
    m_navmesh = ai::Navmesh::FromGeometry(navmeshWalkableAreas, navmeshObstacles, agentRadius);
}

NavigationSystem::~NavigationSystem() { delete m_navmesh; }

void NavigationSystem::Update(float dt)
{
    m_timeSinceLastFrame += dt;

    const auto& view = Engine.ECS().Registry.view<ai::NavmeshAgent, physics::Body>();

    if (m_timeSinceLastFrame >= m_fixedDeltaTime)
    {
        // handle navmesh agent control
        for (const auto& [entity, agent, body] : view.each())
        {
            const vec2& pos = vec2(body.GetPosition());

            // recompute path?
            if (agent.ShouldRecomputePath()) agent.ComputePath(*m_navmesh, pos);

            // update velocity
            agent.ComputePreferredVelocity(pos, m_fixedDeltaTime);
        }

        m_timeSinceLastFrame -= m_fixedDeltaTime;
    }

    // link agents to physics
    for (const auto& [entity, agent, body] : view.each())
    {
        body.SetLinearVelocity(agent.GetPreferredVelocity());
    }

#ifdef BEE_DEBUG

    // debug drawing for the navmesh
    m_navmesh->DebugDraw();

    // debug drawing for agent paths
    vec4 color_path(1.0f, 1.0f, 0.0f, 1.0f);
    for (const auto& entity : view)
    {
        const auto& path = view.get<ai::NavmeshAgent>(entity).GetPath();
        if (!path.empty())
        {
            for (size_t p = 0; p < path.size() - 1; ++p)
            {
                const auto& p1 = path[p];
                const auto& p2 = path[p + 1];
                Engine.DebugRenderer().AddLine(DebugCategory::AIDecision, vec3(p1, 0.15f), vec3(p2, 0.15f), color_path);
                Engine.DebugRenderer().AddCircle(DebugCategory::AIDecision, vec3(p1, 0.15f), 0.15f, vec3(0, 0, 1), color_path);
            }
            Engine.DebugRenderer().AddCircle(DebugCategory::AIDecision,
                                             vec3(path.back(), 0.15f),
                                             0.3f,
                                             vec3(0, 0, 1),
                                             color_path);
        }
    }
#endif
}
