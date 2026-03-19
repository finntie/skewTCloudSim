#include "ai/navmesh.hpp"

#include <clipper/include/clipper2/clipper.h>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <queue>

#include "core/geometry2d.hpp"
#include "graph/graph_search.hpp"

using namespace bee::ai;
using namespace bee::graph;
using namespace bee::geometry2d;

Navmesh* Navmesh::FromGeometry(const PolygonList& walkableAreas, const PolygonList& obstacles, float agentRadius)
{
    // If there's only 1 outer polygon and the holes don't overlap, we can triangulate it straight away.
    // Otherwise, we need to clean up the input geometry first: remove all overlaps, and determine what is a hole inside what.
    // Let's do that all the time, just for safety.
    const PolygonList& boundaries = CleanupGeometry(walkableAreas, obstacles, agentRadius);

    // Triangulate the cleaned-up geometry
    const PolygonList& polygons = geometry2d::TriangulatePolygons(boundaries);

    // Create a dual graph for navigation
    const EuclideanGraph& graph = EuclideanGraph::CreateDualGraph(polygons);

    return new Navmesh(polygons, graph);
}

int Navmesh::GetContainingPolygon(const glm::vec2& pos) const
{
    // Check all polygons and return the first one that contains the query point.
    // Note: You can still improve performance with an acceleration structure.
    for (int i = 0; i < (int)m_polygons.size(); ++i)
        if (IsPointInsidePolygon(pos, m_polygons[i])) return i;

    return -1;
}

int Navmesh::GetNearestPolygon(const glm::vec2& pos) const
{
    float bestDistance = std::numeric_limits<float>::max();
    int bestIndex = -1;

    // Check all polygons and return the one to which pos is nearest
    // Note: You can still improve performance with an acceleration structure.
    for (int i = 0; i < (int)m_polygons.size(); ++i)
    {
        if (IsPointInsidePolygon(pos, m_polygons[i])) return i;

        const auto& n = GetNearestPointOnPolygonBoundary(pos, m_polygons[i]);
        float dist = glm::length2(pos - n);
        if (dist < bestDistance)
        {
            bestDistance = dist;
            bestIndex = i;
        }
    }

    return bestIndex;
}

std::pair<size_t, size_t> GetSharedEdge(const Polygon& p1, const Polygon& p2)
{
    const size_t n1 = p1.size(), n2 = p2.size();

    for (size_t i = 0; i < n1; ++i)
    {
        size_t i2 = (i + 1) % n1;
        const glm::vec2& pa = p1[i];
        const glm::vec2& pb = p1[i2];

        for (size_t j = 0; j < n2; ++j)
        {
            const glm::vec2& qa = p2[j];
            const glm::vec2& qb = p2[(j + 1) % n2];

            if (pa == qb && pb == qa) return {i2, i};
        }
    }

    return {1, 0};
}

glm::vec2 GetMidpointOfSharedEdge(const Polygon& p1, const Polygon& p2)
{
    const auto& sharedEdge = GetSharedEdge(p1, p2);
    return (p1[sharedEdge.first] + p1[sharedEdge.second]) / 2.0f;
}

void FunnelAlgorithmStepLeft(const glm::vec2& newPoint,
                             std::deque<glm::vec2>& leftFunnel,
                             std::deque<glm::vec2>& rightFunnel,
                             Path& path)
{
    const glm::vec2& lastPoint = leftFunnel.empty() ? path.back() : leftFunnel.back();
    if (newPoint != lastPoint)
    {
        // add point to left boundary
        leftFunnel.push_back(newPoint);

        // simplify left boundary
        while (leftFunnel.size() > 1 && !IsPointLeftOfLine(leftFunnel.size() == 2 ? path.back() : *(leftFunnel.rbegin() + 2),
                                                           *(leftFunnel.rbegin() + 1),
                                                           leftFunnel.back()))
        {
            glm::vec2 p = leftFunnel.back();
            leftFunnel.pop_back();
            leftFunnel.pop_back();
            leftFunnel.push_back(p);
        }

        // check if left boundary crosses right boundary
        while (!rightFunnel.empty() && !IsPointLeftOfLine(leftFunnel.back(), path.back(), rightFunnel.front()))
        {
            path.push_back(rightFunnel.front());
            rightFunnel.pop_front();
        }
    }
}

void FunnelAlgorithmStepRight(const glm::vec2& newPoint,
                              std::deque<glm::vec2>& leftFunnel,
                              std::deque<glm::vec2>& rightFunnel,
                              Path& path)
{
    const glm::vec2& lastPoint = rightFunnel.empty() ? path.back() : rightFunnel.back();
    if (newPoint != lastPoint)
    {
        // add point to right boundary
        rightFunnel.push_back(newPoint);

        // simplify right boundary
        while (rightFunnel.size() > 1 &&
               !IsPointRightOfLine(rightFunnel.size() == 2 ? path.back() : *(rightFunnel.rbegin() + 2),
                                   *(rightFunnel.rbegin() + 1),
                                   rightFunnel.back()))
        {
            glm::vec2 p = rightFunnel.back();
            rightFunnel.pop_back();
            rightFunnel.pop_back();
            rightFunnel.push_back(p);
        }

        // check if right boundary crosses left boundary
        while (!leftFunnel.empty() && !IsPointRightOfLine(rightFunnel.back(), path.back(), leftFunnel.front()))
        {
            path.push_back(leftFunnel.front());
            leftFunnel.pop_front();
        }
    }
}

Path Navmesh::ComputeShortestPath(const glm::vec2& start, const glm::vec2& goal, const std::vector<int>& cells) const
{
    // Based on this explanation: https://medium.com/@reza.teshnizi/the-funnel-algorithm-explained-visually-41e374172d2d
    // This version of the funnel algorithm only works with convex navmesh cells.

    Path path = {start};

    std::deque<glm::vec2> leftFunnel, rightFunnel;

    size_t n = cells.size();
    for (size_t i = 0; i < n; ++i)
    {
        if (i + 1 < n)
        {
            const auto& poly1 = m_polygons[cells[i]];
            const auto& poly2 = m_polygons[cells[i + 1]];
            const auto& sharedEdge12 = GetSharedEdge(poly1, poly2);

            FunnelAlgorithmStepLeft(poly1[sharedEdge12.first], leftFunnel, rightFunnel, path);
            FunnelAlgorithmStepRight(poly1[sharedEdge12.second], leftFunnel, rightFunnel, path);
        }
        else
        {
            FunnelAlgorithmStepLeft(goal, leftFunnel, rightFunnel, path);
            FunnelAlgorithmStepRight(goal, leftFunnel, rightFunnel, path);
        }
    }

    return path;
}

Path Navmesh::ComputeMidpointPath(const glm::vec2& start, const glm::vec2& goal, const std::vector<int>& cells) const
{
    Path result = {start};

    const size_t n = cells.size();
    for (size_t i = 0; i + 1 < n; ++i)
        result.push_back(GetMidpointOfSharedEdge(m_polygons[cells[i]], m_polygons[cells[i + 1]]));
    result.push_back(goal);

    return result;
}

Path Navmesh::ComputePath(const glm::vec2& start, const glm::vec2& goal) const
{
    // find the nearest cell to the start point
    int startID = GetNearestPolygon(start);
    if (startID == -1) return {};

    // find the nearest cell to the goal point
    int goalID = GetNearestPolygon(goal);
    if (goalID == -1) return {};

    // do an A* search
    const std::vector<int>& cells = graph::AStar(m_graph, startID, goalID, graph::AStarHeuristic_EuclideanDistance);
    if (cells.empty()) return {};

    // convert sequence of cells to a nice path
    return ComputeShortestPath(start, goal, cells);  // shortest path, based on funnel algorithm
    // return ComputeMidpointPath(start, goal, cells); // a path that connects the triangle edge midpoints
}

template <typename T1, typename T2, typename T3>
std::vector<T2> convertPolygon(const std::vector<T1>& polygon)
{
    std::vector<T2> result;
    result.resize(polygon.size());
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        result[i].x = (T3)polygon[i].x;
        result[i].y = (T3)polygon[i].y;
    }
    return result;
}

PolygonList Navmesh::CleanupGeometry(const PolygonList& walkableAreas, const PolygonList& obstacles, float radius)
{
    // convert to Clipper2-compliant data
    Clipper2Lib::PathsD walkableAreasD, obstaclesD;
    for (const Polygon& p : walkableAreas) walkableAreasD.push_back(convertPolygon<glm::vec2, Clipper2Lib::PointD, double>(p));
    for (const Polygon& p : obstacles) obstaclesD.push_back(convertPolygon<glm::vec2, Clipper2Lib::PointD, double>(p));

    // Cut all holes out of all outer polygons. The Clipper2 library can do all this in one function call.
    // If we've supplied an agent radius > 0, shrink the output by that radius.
    const Clipper2Lib::PathsD& polygonsAndHoles =
        radius > 0.f
            ? Clipper2Lib::InflatePaths(Clipper2Lib::Difference(walkableAreasD, obstaclesD, Clipper2Lib::FillRule::NonZero),
                                        -radius,
                                        Clipper2Lib::JoinType::Square,
                                        Clipper2Lib::EndType::Polygon)
            : Clipper2Lib::Difference(walkableAreasD, obstaclesD, Clipper2Lib::FillRule::NonZero);

    // Convert back to our own format
    PolygonList result;
    for (const Clipper2Lib::PathD& pd : polygonsAndHoles)
        result.push_back(convertPolygon<Clipper2Lib::PointD, glm::vec2, float>(pd));

    return result;
}

#ifdef BEE_DEBUG

#include "core/engine.hpp"
#include "rendering/debug_render.hpp"

void Navmesh::DebugDraw() const
{
    // debug drawing for navmesh polygons
    glm::vec4 color_navmesh(0.0f, 0.6f, 1.0f, 1.0f);
    for (const auto& polygon : m_polygons)
    {
        size_t n = polygon.size();
        for (size_t i = 0; i < n; ++i)
        {
            const auto& p1 = polygon[i];
            const auto& p2 = polygon[(i + 1) % n];
            Engine.DebugRenderer().AddLine(DebugCategory::AINavigation,
                                           glm::vec3(p1, 0.1f),
                                           glm::vec3(p2, 0.1f),
                                           color_navmesh);
        }
    }

    // debug drawing for navmesh graph
    glm::vec4 color_graph(0.7f, 0.0f, 0.1f, 1.0f);
    for (size_t v = 0; v < m_graph.GetNumberOfVertices(); ++v)
    {
        const auto& p1 = m_graph.GetVertex((int)v).position;
        Engine.DebugRenderer().AddCircle(DebugCategory::AINavigation,
                                         glm::vec3(p1, 0.1f),
                                         0.2f,
                                         glm::vec3(0, 0, 1),
                                         color_graph);

        for (const auto& edge : m_graph.GetEdgesFromVertex((int)v))
        {
            const auto& p2 = m_graph.GetVertex(edge.m_targetVertex).position;
            Engine.DebugRenderer().AddLine(DebugCategory::AINavigation, glm::vec3(p1, 0.1f), glm::vec3(p2, 0.1f), color_graph);
        }
    }
}

#endif