#include "ai/navmesh_agent.hpp"

using namespace bee::ai;

void NavmeshAgent::SetGoal(const glm::vec2& goal, bool recomputePath)
{
    m_goal = goal;
    m_recomputePath = recomputePath;
}

void NavmeshAgent::ComputePath(const Navmesh& navmesh, const glm::vec2& currentPos)
{
    // compute a new path to the goal
    m_path = navmesh.ComputePath(currentPos, m_goal);
    m_recomputePath = false;

    // reset the agent's path references
    if (m_path.empty())
    {
        m_currentPoint = PathPointReference();
        m_attractionPoint = PathPointReference();
    }
    else
    {
        m_currentPoint = {m_path[0], 0};
        m_attractionPoint = {m_path[0], 0};
    }
}

void NavmeshAgent::ComputePreferredVelocity(const glm::vec2& currentPos, float dt)
{
    float normalTravelDist = m_speed * dt;

    // if there's no path or we're at the end of the path, we don't move
    if (m_path.empty() || glm::distance2(currentPos, m_path.back()) < normalTravelDist)
    {
        m_preferredVelocity = {0.f, 0.f};
        return;
    }

    // update reference point and attraction point
    m_currentPoint = ComputeNearestPointWithinCurveDistance(m_currentPoint, currentPos, 0.5f);
    m_attractionPoint = ComputePathPointAtCurveDistance(m_currentPoint, 0.5f);

    // compute a velocity towards the attraction point with the correct speed
    m_preferredVelocity = glm::normalize(m_attractionPoint.m_point - currentPos) * m_speed;
}

NavmeshAgent::PathPointReference NavmeshAgent::ComputeNearestPointWithinCurveDistance(const PathPointReference& start,
                                                                                      const glm::vec2& ref,
                                                                                      float maxDist) const
{
    const size_t n = m_path.size();

    float dist = 0;
    const glm::vec2* currentPoint = &start.m_point;
    size_t segmentIndex = start.m_segmentIndex;

    PathPointReference best = start;
    float bestDistSquared = std::numeric_limits<float>::max();

    while (segmentIndex + 1 < n && dist < maxDist)
    {
        // compute the distance to the next vertex on the path
        const glm::vec2* nextPoint = &m_path[segmentIndex + 1];
        const glm::vec2& diff = *nextPoint - *currentPoint;
        float segmentDist = glm::length(diff);

        // compute the fraction of the next segment that we're allowed to use
        float fraction = (dist + segmentDist <= maxDist) ? 1.f : ((maxDist - dist) / segmentDist);
        const glm::vec2& endPoint = fraction == 1.f ? *nextPoint : (*currentPoint + fraction * diff);

        // compute the nearest point to ref on that (partial) segment
        const glm::vec2& nearest = geometry2d::GetNearestPointOnLineSegment(ref, *currentPoint, endPoint);

        // if that's the best result found so far, update the result
        float distN = glm::distance2(ref, nearest);
        if (distN <= bestDistSquared)
        {
            bestDistSquared = distN;
            best = {nearest, segmentIndex};
        }

        // go to the next segment
        currentPoint = nextPoint;
        dist += segmentDist;
        ++segmentIndex;
    }

    // end of path/range reached
    return best;
}

NavmeshAgent::PathPointReference NavmeshAgent::ComputePathPointAtCurveDistance(const PathPointReference& start,
                                                                               float maxDist) const
{
    const size_t n = m_path.size();

    float dist = 0;
    const glm::vec2* currentPoint = &start.m_point;
    size_t segmentIndex = start.m_segmentIndex;

    while (segmentIndex + 1 < n && dist < maxDist)
    {
        // compute the distance to the next vertex on the path
        const glm::vec2* nextPoint = &m_path[segmentIndex + 1];
        const glm::vec2& diff = *nextPoint - *currentPoint;
        float segmentDist = glm::length(diff);

        // if we can use this full distance, move on
        if (dist + segmentDist < maxDist)
        {
            currentPoint = nextPoint;
            ++segmentIndex;
        }

        // otherwise, compute the point at exactly the right distance
        else
        {
            float fraction = (maxDist - dist) / segmentDist;
            return {*currentPoint + fraction * diff, segmentIndex};
        }

        dist += segmentDist;
    }

    // end of path/range reached
    return {*currentPoint, segmentIndex};
}