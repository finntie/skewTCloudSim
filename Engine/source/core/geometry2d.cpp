#include "core/geometry2d.hpp"
#include "tools/warnings.hpp"

BEE_DISABLE_WARNING_PUSH
BEE_DISABLE_WARNING_SIZE_T_CONVERSION
BEE_DISABLE_WARNING_COMMA_IN_SUBSCRIPT
#include <cdt/CDT.h>
BEE_DISABLE_WARNING_POP

#include <glm/gtx/norm.hpp>
#include <predicates/predicates.h>

using namespace bee;
using namespace geometry2d;
using namespace glm;

glm::vec2 geometry2d::GetPerpendicularVector(const glm::vec2& v) { return {-v.y, v.x}; }

float geometry2d::PerpDot(const vec2& v1, const vec2& v2) { return v1.x * v2.y - v1.y * v2.x; }

glm::vec2 geometry2d::RotateCounterClockwise(const glm::vec2& v, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

bool geometry2d::IsPointLeftOfLine(const vec2& point, const vec2& line1, const vec2& line2)
{
    double p[2] = {point.x, point.y};
    double la[2] = {line1.x, line1.y};
    double lb[2] = {line2.x, line2.y};
    return RobustPredicates::orient2d(p, la, lb) > 0;
}

bool geometry2d::IsPointRightOfLine(const vec2& point, const vec2& line1, const vec2& line2)
{
    double p[2] = {point.x, point.y};
    double la[2] = {line1.x, line1.y};
    double lb[2] = {line2.x, line2.y};
    return RobustPredicates::orient2d(p, la, lb) < 0;
}

bool geometry2d::IsClockwise(const Polygon& polygon)
{
    size_t n = polygon.size();
    assert(n > 2);
    float signedArea = 0.f;

    for (size_t i = 0; i < n; ++i)
    {
        const auto& p0 = polygon[i];
        const auto& p1 = polygon[(i + 1) % n];

        signedArea += (p0.x * p1.y - p1.x * p0.y);
    }

    // Technically we now have 2 * the signed area.
    // But for the "is clockwise" check, we only care about the sign of this number,
    // so there is no need to divide by 2.
    return signedArea < 0.f;
}

bool geometry2d::IsPointInsidePolygon(const vec2& point, const Polygon& polygon)
{
    // Adapted from: https://wrfranklin.org/Research/Short_Notes/pnpoly.html

    size_t i = 0, j = 0;
    size_t n = polygon.size();
    bool inside = false;

    for (i = 0, j = n - 1; i < n; j = i++)
    {
        if ((polygon[i].y > point.y != polygon[j].y > point.y) &&
            (point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x))
            inside = !inside;
    }

    return inside;
}

vec2 geometry2d::GetNearestPointOnLineSegment(const vec2& p, const vec2& segmentA, const vec2& segmentB)
{
    float t = dot(p - segmentA, segmentB - segmentA) / distance2(segmentA, segmentB);
    if (t <= 0) return segmentA;
    if (t >= 1) return segmentB;
    return (1 - t) * segmentA + t * segmentB;
}

vec2 geometry2d::GetNearestPointOnPolygonBoundary(const vec2& point, const Polygon& polygon)
{
    float bestDist = std::numeric_limits<float>::max();
    vec2 bestNearest(0.f, 0.f);

    size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i)
    {
        const vec2& nearest = GetNearestPointOnLineSegment(point, polygon[i], polygon[(i + 1) % n]);
        float dist = distance2(point, nearest);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestNearest = nearest;
        }
    }

    return bestNearest;
}

std::pair<vec2, vec2> geometry2d::GetNearestPointPairBetweenLineSegments(const vec2& segment1A,
                                                                         const vec2& segment1B,
                                                                         const vec2& segment2A,
                                                                         const vec2& segment2B)
{
    // Based on https://stackoverflow.com/questions/2824478/shortest-distance-between-two-line-segments

    const vec2& r = segment2A - segment1A;
    const vec2& u = segment1B - segment1A;
    const vec2& v = segment2B - segment2A;

    auto ru = dot(r, u), rv = dot(r, v), uu = dot(u, u), uv = dot(u, v), vv = dot(v, v);
    float det = uu * vv - uv * uv;

    float s = 0.0f, t = 0.0f;
    if (det < 0.0001f)  // parallel lines
    {
        s = clamp<float>(ru / uu, 0.f, 1.f);
        t = 0.f;
    }
    else
    {
        s = clamp<float>((ru * vv - rv * uv) / det, 0.f, 1.f);
        t = clamp<float>((ru * uv - rv * uu) / det, 0.f, 1.f);
    }

    auto S = clamp<float>((t * uv + ru) / uu, 0.f, 1.f);
    auto T = clamp<float>((s * uv - rv) / vv, 0.f, 1.f);

    return {segment1A + S * u, segment2A + T * v};
}

vec2 geometry2d::ComputeCenterOfPolygon(const Polygon& polygon)
{
    vec2 total(0, 0);
    for (const vec2& p : polygon) total += p;
    return total / (float)polygon.size();
}

std::vector<size_t> geometry2d::TriangulatePolygon(const Polygon& polygon)
{
    // build up an overall list of CDT vertices and edges
    std::vector<CDT::V2d<double>> vertices;
    std::vector<CDT::Edge> edges;
    size_t nrVertices = polygon.size();

    // Convert the polygon data to CDT input
    for (size_t i = 0; i < nrVertices; ++i)
    {
        vertices.push_back({polygon[i].x, polygon[i].y});
        edges.push_back({(CDT::VertInd)i, (CDT::VertInd)((i + 1) % nrVertices)});
    }

    // Triangulate using the CDT library.
    // The algorithm is incremental, so inserting vertices/edges will update the triangulation.
    CDT::Triangulation<double> cdt;
    cdt.insertVertices(vertices);
    cdt.insertEdges(edges);

    // Clean up the triangulation
    cdt.eraseOuterTrianglesAndHoles();

    // Convert back to our own list of vertex indices
    std::vector<size_t> result;
    for (const auto& triangle : cdt.triangles)
    {
        result.push_back(static_cast<size_t>(triangle.vertices[0]));
        result.push_back(static_cast<size_t>(triangle.vertices[1]));
        result.push_back(static_cast<size_t>(triangle.vertices[2]));
    }

    return result;
}

PolygonList geometry2d::TriangulatePolygons(const PolygonList& boundaries)
{
    // build up an overall list of CDT vertices and edges
    std::vector<CDT::V2d<double>> vertices;
    std::vector<CDT::Edge> edges;
    size_t nrVertices = 0;

    for (const Polygon& poly : boundaries)
    {
        // Convert the polygon data to CDT input
        size_t polySize = poly.size();
        for (size_t i = 0; i < polySize; ++i)
        {
            vertices.push_back({poly[i].x, poly[i].y});
            edges.push_back({(CDT::VertInd)(nrVertices + i), (CDT::VertInd)(nrVertices + (i + 1) % polySize)});
        }
        nrVertices += polySize;
    }

    // Triangulate using the CDT library.
    // The algorithm is incremental, so inserting vertices/edges will update the triangulation.
    CDT::Triangulation<double> cdt;
    cdt.insertVertices(vertices);
    cdt.insertEdges(edges);

    // Clean up the triangulation
    cdt.eraseOuterTrianglesAndHoles();

    // Convert back to our own polygon format
    PolygonList result;
    for (const auto& triangle : cdt.triangles)
    {
        const auto& v0 = cdt.vertices[triangle.vertices[0]];
        const auto& v1 = cdt.vertices[triangle.vertices[1]];
        const auto& v2 = cdt.vertices[triangle.vertices[2]];
        result.push_back({{(float)v0.x, (float)v0.y}, {(float)v1.x, (float)v1.y}, {(float)v2.x, (float)v2.y}});
    }

    return result;
}