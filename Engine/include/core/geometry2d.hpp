#pragma once

#include <vector>

#include "glm/vec2.hpp"

/// <summary>
/// A namespace containing functions related to 2D geometry.
/// </summary>
namespace bee::geometry2d
{
// Input data types
using Polygon = std::vector<glm::vec2>;
using PolygonList = std::vector<Polygon>;

/// <summary>
/// Simple struct representing an axis-aligned bounding box.
/// </summary>
struct AABB
{
private:
    glm::vec2 m_min;
    glm::vec2 m_max;

public:
    AABB(const glm::vec2& minPos, const glm::vec2& maxPos) : m_min(minPos), m_max(maxPos) {}

    /// <summary>
    /// Computes and returns the four boundary vertices of this AABB.
    /// </summary>
    Polygon ComputeBoundary() const { return {m_min, glm::vec2(m_max.x, m_min.y), m_max, glm::vec2(m_min.x, m_max.y)}; }

    /// <summary>
    /// Computes and returns the center coordinate of this AABB.
    /// </summary>
    glm::vec2 ComputeCenter() const { return (m_min + m_max) / 2.f; }

    /// <summary>
    /// Computes and returns the size of this AABB, wrapped in a vec2 (x=width, y=height).
    /// </summary>
    /// <returns></returns>
    glm::vec2 ComputeSize() const { return m_max - m_min; }

    const glm::vec2& GetMin() const { return m_min; }
    const glm::vec2& GetMax() const { return m_max; }

    /// <summary>
    /// Checks and returns whether this AABB overlaps with another AABB.
    /// </summary>
    /// <param name="other">The AABB to compare to.</param>
    /// <returns>true if the AABBs overlap, false otherwise.</returns>
    bool OverlapsWith(const AABB& other)
    {
        return m_max.x >= other.m_min.x && other.m_max.x >= m_min.x && m_max.y >= other.m_min.y && other.m_max.y >= m_max.y;
    }
};

/// <summary>
/// Computes and returns a 2D vector equal to the input vector v rotated by 90 degrees counterclockwise,
/// i.e. (-v.y, v.x).
/// </summary>
glm::vec2 GetPerpendicularVector(const glm::vec2& v);

/// <summary>
/// Computes and returns the perp-dot product of two 2D vectors v1 and v2, i.e. v1.x*v2.y - v1.y*v2.x.
/// </summary>
float PerpDot(const glm::vec2& v1, const glm::vec2& v2);

/// <summary>
/// Computes and returns a 2D vector equal to the input vector v rotated counterclockwise by the given angle (in radians).
/// </summary>
glm::vec2 RotateCounterClockwise(const glm::vec2& v, float angle);

/// <summary>
/// Checks and returns whether a 2D point lies strictly to the left of an infinite directed line.
/// </summary>
/// <param name="point">A query point.</param>
/// <param name="line1">A first point on the query line.</param>
/// <param name="line2">A second point on the query line.</param>
/// <returns>true if "point" lies strictly to the left of the infinite directed line through line1 and line2;
/// false otherwise (i.e. if the point lies on or to the right of the line).</return>
bool IsPointLeftOfLine(const glm::vec2& point, const glm::vec2& line1, const glm::vec2& line2);

/// <summary>
/// Checks and returns whether a 2D point lies strictly to the right of an infinite directed line.
/// </summary>
/// <param name="point">A query point.</param>
/// <param name="line1">A first point on the query line.</param>
/// <param name="line2">A second point on the query line.</param>
/// <returns>true if "point" lies strictly to the right of the infinite directed line through line1 and line2;
/// false otherwise (i.e. if the point lies on or to the left of the line).</return>
bool IsPointRightOfLine(const glm::vec2& point, const glm::vec2& line1, const glm::vec2& line2);

/// <summary>
/// Checks and returns whether the points of a simple 2D polygon are given in clockwise order.
/// </summary>
/// <param name="polygon">A list of 2D points describing the boundary of a simple polygon (i.e. at least 3 points, nonzero area,
/// not self-intersecting).</param> <returns>true if the points are given in clockwise order, false otherwise.</returns>
bool IsClockwise(const Polygon& polygon);

/// <summary>
/// Checks and returns whether a given point lies inside a given 2D polygon.
/// </summary>
/// <param name="point">A query point.</param>
/// <param name="polygon">A simple 2D polygon.</param>
/// <returns>true if the point lies inside the polygon, false otherwise.</return>
bool IsPointInsidePolygon(const glm::vec2& point, const Polygon& polygon);

/// <summary>
/// Computes and returns the centroid of a given polygon (= the average of its boundary points).
/// </summary>
glm::vec2 ComputeCenterOfPolygon(const Polygon& polygon);

/// <summary>
/// Computes and returns the nearest point on a line segment segmentA-segmentB to another point p.
/// </summary>
/// <param name="p">A query point.</param>
/// <param name="segmentA">The first endpoint of a line segment.</param>
/// <param name="segmentB">The second endpoint of a line segment.</param>
/// <returns>The point on the line segment segmentA-segmentB that is closest to p.</returns>
glm::vec2 GetNearestPointOnLineSegment(const glm::vec2& p, const glm::vec2& segmentA, const glm::vec2& segmentB);

/// <summary>
/// Given two line segments 1 and 2, computes the pair of points where the distance between 1 and 2 is smallest.
/// </summary>
/// <param name="segment1A">The first endpoint of the first line segment.</param>
/// <param name="segment1B">The second endpoint of the first line segment.</param>
/// <param name="segment2A">The first endpoint of the second line segment.</param>
/// <param name="segment2B">The second endpoint of the second line segment.</param>
/// <returns>The pair of points (on segments 1 and 2 respectively) where the distance between 1 and 2 is smallest.</returns>
std::pair<glm::vec2, glm::vec2> GetNearestPointPairBetweenLineSegments(const glm::vec2& segment1A,
                                                                       const glm::vec2& segment1B,
                                                                       const glm::vec2& segment2A,
                                                                       const glm::vec2& segment2B);
/// <summary>
/// Computes and returns the nearest point on a polygon boundary to a given point.
/// </summary>
/// <param name="point">A query point.</param>
/// <param name="polygon">A polygon.</param>
/// <returns>The point on the boundary of 'polygon' that is closest to 'point'.</returns>
glm::vec2 GetNearestPointOnPolygonBoundary(const glm::vec2& point, const Polygon& polygon);

/// <summary>
/// Triangulates a simple polygon, and returns the triangulation as vertex indices.
/// </summary>
/// <param name="polygon">The input polygon.</param>
/// <returns>A list of vertex indices representing the triangulation of the input polygon.</returns>
std::vector<size_t> TriangulatePolygon(const Polygon& polygon);

/// <summary>
/// Triangulates a set of polygons, and returns the triangulation as a new set of polygons.
/// </summary>
/// <param name="polygon">A list of simple polygons. They are allowed to overlap.</param>
/// <returns>A list of polygons representing the triangulation of the union of all input polygons.</returns>
PolygonList TriangulatePolygons(const PolygonList& polygon);

};  // namespace bee::geometry2d