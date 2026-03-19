#include "core/transform.hpp"

#include "tools/warnings.hpp"

BEE_DISABLE_WARNING_PUSH
BEE_DISABLE_WARNING_DEPRECATED_DECLARATIONS
#include <entt/entity/helper.hpp>
BEE_DISABLE_WARNING_POP

#include "core/ecs.hpp"
#include "core/engine.hpp"

#include <cassert>

using namespace bee;
using namespace glm;

void Transform::SetParent(Entity parent)
{
    assert(parent == entt::null || Engine.ECS().Registry.valid(parent));

    const auto& storage = Engine.ECS().Registry.storage<Transform>();   
    auto thisEntity = entt::to_entity(storage, *this);

    // if this transform already has a parent, detach it from that
    if (m_parent != entt::null && Engine.ECS().Registry.valid(m_parent))
    {
        auto& oldParentTransform = Engine.ECS().Registry.get<Transform>(m_parent);
        oldParentTransform.RemoveChild(thisEntity);
    }

    // if we want to set a new parent, attach it to that
    if (parent != entt::null && Engine.ECS().Registry.valid(parent))
    {
        auto& newParentTransform = Engine.ECS().Registry.get<Transform>(parent);
        newParentTransform.AddChild(thisEntity);
    }

    m_parent = parent;

    // mark this transform and its children as dirty
    SetMatrixDirty();
}

void Transform::AddChild(Entity child)
{
    assert(Engine.ECS().Registry.valid(child));

    // if there are no children yet, set m_first to child
    if (m_first == entt::null)
    {
        m_first = child;
    }

    // otherwise, find the last child and set its m_next to child
    else
    {
        auto itr = m_first;
        while (true)
        {
            auto& t = Engine.ECS().Registry.get<Transform>(itr);
            if (t.m_next == entt::null)
            {
                t.m_next = child;
                break;
            }
            itr = t.m_next;
        }
    }
}

void Transform::RemoveChild(Entity child)
{
    assert(Engine.ECS().Registry.valid(child));

    // if it's the first child, replace m_first by the next sibling of child
    if (m_first == child)
    {
        auto& t = Engine.ECS().Registry.get<Transform>(m_first);
        m_first = t.m_next;
    }

    // otherwise, update the m_next of the transform *before* child, removing child from the sequence
    else
    {
        auto itr = m_first;
        while (itr != entt::null)
        {
            auto& t = Engine.ECS().Registry.get<Transform>(itr);
            if (t.m_next == child)
            {
                auto& t2 = Engine.ECS().Registry.get<Transform>(t.m_next);
                t.m_next = t2.m_next;
                break;
            }
            itr = t.m_next;
        }
    }
}

void Transform::SetFromMatrix(const mat4& m44)
{
    m_translation.x = m44[3][0];
    m_translation.y = m44[3][1];
    m_translation.z = m44[3][2];

    m_scale.x = length(vec3(m44[0][0], m44[0][1], m44[0][2]));
    m_scale.y = length(vec3(m44[1][0], m44[1][1], m44[1][2]));
    m_scale.z = length(vec3(m44[2][0], m44[2][1], m44[2][2]));

    mat4 myrot(m44[0][0] / m_scale.x,
               m44[0][1] / m_scale.x,
               m44[0][2] / m_scale.x,
               0,
               m44[1][0] / m_scale.y,
               m44[1][1] / m_scale.y,
               m44[1][2] / m_scale.y,
               0,
               m44[2][0] / m_scale.z,
               m44[2][1] / m_scale.z,
               m44[2][2] / m_scale.z,
               0,
               0,
               0,
               0,
               1);
    m_rotation = quat_cast(myrot);

    SetMatrixDirty();
}

Transform::Iterator::Iterator(entt::entity ent) : m_current(ent) {}

Transform::Iterator& Transform::Iterator::operator++()
{
    assert(Engine.ECS().Registry.valid(m_current));
    auto& t = Engine.ECS().Registry.get<Transform>(m_current);
    m_current = t.m_next;
    return *this;
}

bool Transform::Iterator::operator!=(const Iterator& iterator) { return m_current != iterator.m_current; }

Entity Transform::Iterator::operator*() { return m_current; }

void Transform::SetMatrixDirty()
{
    m_worldMatrixDirty = true;
    for (auto child : *this) Engine.ECS().Registry.get<Transform>(child).SetMatrixDirty();
}

const glm::mat4& Transform::World()
{
    if (m_worldMatrixDirty)
    {
        const auto translation = glm::translate(glm::mat4(1.0f), m_translation);
        const auto rotation = glm::toMat4(m_rotation);
        const auto scale = glm::scale(glm::mat4(1.0f), m_scale);
        if (m_parent == entt::null)
        {
            m_worldMatrix = translation * rotation * scale;
        }
        else
        {
            assert(Engine.ECS().Registry.valid(m_parent));
            auto& parent = Engine.ECS().Registry.get<Transform>(m_parent);
            m_worldMatrix = parent.World() * translation * rotation * scale;
        }
        m_worldMatrixDirty = false;
    }

    return m_worldMatrix;
}