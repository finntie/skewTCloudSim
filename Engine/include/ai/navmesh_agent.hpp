#pragma once

#include <glm/gtx/norm.hpp>
#include <glm/vec2.hpp>

#include "ai/navmesh.hpp"

namespace bee::ai
{

/// <summary>
/// Represents an agent that can compute and follow paths on a navmesh.
/// You can add it as a component to an entity.
/// </summary>
class NavmeshAgent
{
public:
    NavmeshAgent(float speed) : m_speed(speed) {}

    /// <summary>
    /// Returns whether or not this agent currently has a path to follow.
    /// </summary>
    inline bool HasPath() const { return !m_path.empty(); }

    /// <summary>
    /// Returns a non-mutable reference to the agent's current path.
    /// </summary>
    inline const Path& GetPath() const { return m_path; }

    /// <summary>
    /// Returns whether or not this agent is currently scheduled to recompute its path.
    /// </summary>
    inline bool ShouldRecomputePath() const { return m_recomputePath; }

    /// <summary>
    /// Returns the agent's current preferred velocity vector.
    /// </summary>
    inline const glm::vec2& GetPreferredVelocity() const { return m_preferredVelocity; }

    /// <summary>
    /// Updates the agent's goal to the given point, and (if desired) flags the agent's path for recomputation.
    /// </summary>
    void SetGoal(const glm::vec2& goal, bool recomputePath = true);

    /// <summary>
    /// Computes a path on the given navmesh from the given start position (currentPos) to the agent's current goal.
    /// If this succeeds, the agent's path is updated to the new path.
    /// </summary>
    void ComputePath(const ai::Navmesh& navmesh, const glm::vec2& currentPos);

    /// <summary>
    /// Computes and updates the agent's preferred velocity according to path following.
    /// Also updates the agent's path progress.
    /// </summary>
    /// <param name="currentPos">The current position of the agent.</param>
    /// <param name="dt">The time that has passed in the currente frame.</param>
    void ComputePreferredVelocity(const glm::vec2& currentPos, float dt);

private:
    const float m_speed;

    glm::vec2 m_goal = {0.f, 0.f};
    glm::vec2 m_preferredVelocity = {0.f, 0.f};

    bool m_recomputePath = false;
    Path m_path = {};

    struct PathPointReference
    {
        glm::vec2 m_point = {0.f, 0.f};
        size_t m_segmentIndex = 0;
    };

    PathPointReference m_currentPoint;
    PathPointReference m_attractionPoint;

    PathPointReference ComputeNearestPointWithinCurveDistance(const PathPointReference& start,
                                                              const glm::vec2& ref,
                                                              float maxDist) const;
    PathPointReference ComputePathPointAtCurveDistance(const PathPointReference& start, float maxDist) const;
};

};  // namespace bee::ai