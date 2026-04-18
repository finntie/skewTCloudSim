#include "physics/physics_components.hpp"

#include <glm/gtx/norm.hpp>

using namespace bee::physics;

void DiskCollider::ComputeMassData(float density, float& result_mass, float& result_momentOfInertia) const
{
    float radiusSquared = m_radius * m_radius;

    result_mass = density * glm::pi<float>() * radiusSquared;
    result_momentOfInertia = result_mass * 0.5f * radiusSquared;
}

void CapsuleCollider::ComputeMassData(float density, float& result_mass, float& result_momentOfInertia) const
{
    float radiusSquared = m_radius * m_radius;
    float width = 2.f * m_radius;
    float heightSquared = m_height * m_height;

    float massRectangle = density * width * m_height;
    float massDisk = density * glm::pi<float>() * radiusSquared;
    result_mass = massRectangle + massDisk;

    float moiRectangle = massRectangle * (width * width + heightSquared) / 12.f;
    float moiHalfDisks = massDisk * (radiusSquared / 2.f + heightSquared / 4.f);
    result_momentOfInertia = moiRectangle + moiHalfDisks;
}

PolygonCollider::PolygonCollider(const std::vector<glm::vec2>& pts) : m_pts(pts)
{
    size_t n = pts.size();
    m_normals.resize(n);
    m_ptsWorld.resize(n);
    m_normalsWorld.resize(n);
    for (size_t i = 0; i < n; ++i)
        m_normals[i] = glm::normalize(geometry2d::GetPerpendicularVector(m_pts[i] - m_pts[(i + 1) % n]));
}

void PolygonCollider::ComputeMassData(float density, float& result_mass, float& result_momentOfInertia) const
{
    // NOTE: We assume that (0,0) is the center of mass.

    float totalCross = 0.f;
    float momentOfInertia_num = 0.f;

    size_t n = m_pts.size();
    for (size_t i = 0; i < n; ++i)
    {
        const glm::vec2& p1 = m_pts[i];
        const glm::vec2& p2 = m_pts[(i + 1) % n];

        float d = geometry2d::PerpDot(p1, p2);

        totalCross += d;
        momentOfInertia_num += d * (dot(p1, p1) + dot(p1, p2) + dot(p2, p2));
    }

    result_mass = density * totalCross * 0.5f;
    result_momentOfInertia = result_mass * momentOfInertia_num / (6.f * totalCross);
}

void PolygonCollider::ComputeWorldPoints(const glm::vec2& translation, float rotation)
{
    size_t n = m_pts.size();
    for (size_t i = 0; i < n; ++i)
    {
        m_ptsWorld[i] = translation + geometry2d::RotateCounterClockwise(m_pts[i], rotation);
        m_normalsWorld[i] = geometry2d::RotateCounterClockwise(m_normals[i], rotation);
    }
}