#pragma once

#include "graph/euclidean_graph.hpp"

namespace bee::ai
{
using Path = std::vector<glm::vec2>;

/// <summary>
/// Represents a navigation mesh: a representation of a game level for pathfinding.
/// </summary>
class Navmesh
{
public:
    /// <summary>
    /// Computes and returns a new Navmesh from a set of walkable areas and obstacles.
    /// </summary>
    /// <param name="walkableAreas">A set of simple polygons describing the walkable space.</param>
    /// <param name="obstacles">A set of simple polygons describing the obstacles for navigation.</param>
    /// <param name="agentRadius">The radius by which the walkable space will be shrunk.</param>
    /// <returns>A navmesh representation of the walkable space (i.e. the union of all walkableAreas with all obstacles cut
    /// out).</returns>
    static Navmesh* FromGeometry(const geometry2d::PolygonList& walkableAreas,
                                 const geometry2d::PolygonList& obstacles,
                                 float agentRadius);

    /// <summary>
    /// Computes and returns a path on this navmesh from a start to a goal position.
    /// The result is empty if a path could not be computed,
    /// i.e. if start and/or goal do not lie inside the walkable space, or if no path between start and goal exists.
    /// </summary>
    Path ComputePath(const glm::vec2& start, const glm::vec2& goal) const;

#ifdef BEE_DEBUG
    void DebugDraw() const;
#endif

private:
    geometry2d::PolygonList m_polygons;
    graph::EuclideanGraph m_graph;

    Navmesh(const geometry2d::PolygonList& polygons, const graph::EuclideanGraph& graph) : m_polygons(polygons), m_graph(graph)
    {
    }

    int GetContainingPolygon(const glm::vec2& pos) const;
    int GetNearestPolygon(const glm::vec2& pos) const;
    Path ComputeShortestPath(const glm::vec2& start, const glm::vec2& goal, const std::vector<int>& cells) const;
    Path ComputeMidpointPath(const glm::vec2& start, const glm::vec2& goal, const std::vector<int>& cells) const;

    /// <summary>
    /// Converts a raw set of (possibly overlapping) polygons and holes to a non-overlapping set of simple polygons with holes.
    /// The output can be used as input for navmesh calculation.
    /// </summary>
    /// <param name="walkableAreas">A list of simple 2D polygons representing the walkable areas.
    /// There may be overlap among these polygons.</param>
    /// <param name="obstacles">A list of simple 2D polygons representing the obostacles.
    /// There may be overlap among these polygons.</param>
    /// <returns>A list of polygons representing the union of WAs minus the union of obstacles.
    /// Each polygon is an outer boundary or a hole.</returns>
    static geometry2d::PolygonList CleanupGeometry(const geometry2d::PolygonList& walkableAreas,
                                                   const geometry2d::PolygonList& obstacles,
                                                   float radius);

    /// <summary>
    /// Computes a Constrained Delaunay Triangulation (CDT) of a set of non-overlapping polygons with holes.
    /// </summary>
    /// <param name="boundaries">A list of non-overlapping outer boundaries and holes of the input geometry.</param>
    /// <returns>A list of polygons, where each entry is a single triangle in the output triangulation.</returns>
    static geometry2d::PolygonList Triangulate(const geometry2d::PolygonList& polygonsWithHoles);
};

}  // namespace bee::ai