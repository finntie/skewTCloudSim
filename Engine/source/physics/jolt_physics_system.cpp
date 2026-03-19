#ifdef BEE_JOLT_PHYSICS

#include "physics/jolt_physics_system.hpp"

#include "core/engine.hpp"
#include "core/input.hpp"
#include "core/resources.hpp"
#include "core/transform.hpp"
#include "rendering/debug_render.hpp"
#include "rendering/mesh.hpp"
#include "rendering/model.hpp"
#include "rendering/render.hpp"
#include "rendering/render_components.hpp"
#ifdef BEE_INSPECTOR
#include "tools/inspector.hpp"
#endif

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

// STL includes
#include <cstdarg>
#include <iostream>
#include <thread>

using namespace bee;
using namespace bee::physics;
using namespace glm;

// Disable common warnings triggered by Jolt, you can use JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to store and
// restore the warning state
JPH_SUPPRESS_WARNINGS

// All Jolt symbols are in the JPH namespace
using namespace JPH;

// If you want your code to compile using single or double precision write 0.0_r to get a Real value that compiles to double or
// float depending if JPH_DOUBLE_PRECISION is set or not.
using namespace JPH::literals;

// We're also using STL classes in this example
using namespace std;

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char* inFMT, ...)
{
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    // Print to the TTY
    cout << buffer << endl;
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
{
    // Print to the TTY
    cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << endl;

    // Breakpoint
    return true;
};

#endif  // JPH_ENABLE_ASSERTS

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
            case JoltLayers::NON_MOVING:
                return inObject2 == JoltLayers::MOVING;  // Non moving only collides with moving
            case JoltLayers::MOVING:
                return true;  // Moving collides with everything
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers
{
static constexpr BroadPhaseLayer NON_MOVING(0);
static constexpr BroadPhaseLayer MOVING(1);
static constexpr uint NUM_LAYERS(2);
};  // namespace BroadPhaseLayers

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[JoltLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[JoltLayers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < JoltLayers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        switch ((BroadPhaseLayer::Type)inLayer)
        {
            case (BroadPhaseLayer::Type)JoltLayers::NON_MOVING:
                return "NON_MOVING";
            case (BroadPhaseLayer::Type)JoltLayers::MOVING:
                return "MOVING";
            default:
                JPH_ASSERT(false);
                return "INVALID";
        }
    }
#endif  // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    BroadPhaseLayer mObjectToBroadPhase[JoltLayers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
            case JoltLayers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case JoltLayers::MOVING:
                return true;
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

// Create mapping table from object layer to broadphase layer
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
BPLayerInterfaceImpl bp;

// Create class that filters object vs broadphase layers
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
ObjectVsBroadPhaseLayerFilterImpl ovbp;

// Create class that filters object vs object layers
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
ObjectLayerPairFilterImpl ovo;

JPH::PhysicsSystem* JoltSystem::m_physicsSystem = nullptr;

void JoltSystem::OnPhysicsBodyDestroy(entt::registry& registry, entt::entity entity)
{
    if (registry.valid(entity))
    {
        auto& bodyInterface = m_physicsSystem->GetBodyInterface();

        // get the Jolt body ID
        auto& body = registry.get<JoltBody>(entity);
        JPH::BodyID bodyID(body.m_bodyID);

        // first activate any other bodies around this one
        const auto& transformMat = bodyInterface.GetWorldTransform(bodyID);
        const auto& bbox = bodyInterface.GetShape(bodyID)->GetWorldSpaceBounds(transformMat, Vec3(1, 1, 1));
        bodyInterface.ActivateBodiesInAABox(bbox, BroadPhaseLayerFilter(), ObjectLayerFilter());

        // then remove the body itself
        bodyInterface.RemoveBody(bodyID);
    }
}

JoltSystem::JoltSystem(float fixedDeltaTime) : m_fixedDeltaTime(fixedDeltaTime)
{
    Engine.ECS().Registry.on_destroy<JoltBody>().connect<&JoltSystem::OnPhysicsBodyDestroy>();

    // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want
    // (see Memory.h). This needs to be done before any other Jolt function is called.
    RegisterDefaultAllocator();

    // Install trace and assert callbacks
    Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

    // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly
    // used for deserialization of saved data. It is not directly used in this example but still required.
    Factory::sInstance = new Factory();

    // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
    // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before
    // calling this function. If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it
    // before this function or else this function will create one for you.
    RegisterTypes();

    m_tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
    m_jobSystem = new JPH::JobSystemThreadPool(2048, 8, thread::hardware_concurrency() - 2);

    m_physicsSystem = new JPH::PhysicsSystem();
    m_physicsSystem->Init(1024, 0, 1024, 1024, bp, ovbp, ovo);

    // Optional step: Before starting the physics simulation you can optimize the broad phase.
    // This improves collision detection performance. You should definitely not call this every frame or when
    // e.g. streaming in a new level section as it is an expensive operation.
    // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
    m_physicsSystem->OptimizeBroadPhase();
}

JoltSystem::~JoltSystem()
{
    Engine.ECS().Registry.on_destroy<JoltBody>().disconnect<&JoltSystem::OnPhysicsBodyDestroy>();

    // Unregisters all types with the factory and cleans up the default material
    UnregisterTypes();

    // Destroy the factory
    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    delete m_jobSystem;
    delete m_tempAllocator;
    delete m_physicsSystem;
}

void JoltSystem::AddPhysicsBody(Entity entity,
                                const Transform& transform,
                                const ShapeSettings& shapeSettings,
                                float friction,
                                float restitution,
                                bool isStatic)
{
    // Create the shape
    ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    ShapeRefC shape =
        shapeResult.Get();  // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

    // Create the settings for the body itself. Note that here you can also set other properties like the restitution /
    // friction.
    BodyCreationSettings settings(shape,
                                  ToJolt<Vec3>(transform.GetTranslation()),
                                  ToJolt(transform.GetRotation()),
                                  isStatic ? EMotionType::Static : EMotionType::Dynamic,
                                  isStatic ? JoltLayers::NON_MOVING : JoltLayers::MOVING);
    settings.mRestitution = restitution;
    settings.mUserData = static_cast<uint64_t>(entity);
    settings.mFriction = friction;

    // The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a
    // non-locking variant of this. We're going to use the locking version (even though we're not planning to access bodies from
    // multiple threads)
    BodyID bodyID =
        GetInternalSystem()->GetBodyInterface().CreateAndAddBody(settings,
                                                                 isStatic ? EActivation::DontActivate : EActivation::Activate);

    // Create an ECS component
    Engine.ECS().CreateComponent<JoltBody>(entity, bodyID.GetIndexAndSequenceNumber());
}

void JoltSystem::SetBodyPosition(const JoltBody& body, const vec3& pos)
{
    GetInternalSystem()->GetBodyInterface().SetPosition((BodyID)body.m_bodyID, ToJolt<Vec3>(pos), EActivation::Activate);
}

void JoltSystem::SetBodyRotation(const JoltBody& body, const quat& rot)
{
    GetInternalSystem()->GetBodyInterface().SetRotation((BodyID)body.m_bodyID, ToJolt(normalize(rot)), EActivation::Activate);
}

void JoltSystem::SetBodyVelocity(const JoltBody& body, const vec3& vel)
{
    GetInternalSystem()->GetBodyInterface().SetLinearVelocity((BodyID)body.m_bodyID, ToJolt<Vec3>(vel));
}

void JoltSystem::Update(float dt)
{
    m_timeSinceLastUpdate += dt;

    // execute physics frames?
    m_hasExecutedFrame = false;
    int nrFrames = (int)(m_timeSinceLastUpdate / m_fixedDeltaTime);
    if (nrFrames > 0)
    {
        m_physicsSystem->Update(nrFrames * m_fixedDeltaTime, 5, m_tempAllocator, m_jobSystem);
        m_timeSinceLastUpdate -= nrFrames * m_fixedDeltaTime;
        m_hasExecutedFrame = true;
    }
    if (!m_hasExecutedFrame) return;

    // sync Jolt shapes with Bee
    const auto& bodyInterface = m_physicsSystem->GetBodyInterface();
    for (const auto& [entity, joltBody, transform] : Engine.ECS().Registry.view<JoltBody, Transform>().each())
    {
        BodyID bid(joltBody.m_bodyID);
        if (bodyInterface.IsAdded(bid) && bodyInterface.GetObjectLayer(bid) == JoltLayers::MOVING)
        {
            const auto& pos = bodyInterface.GetCenterOfMassPosition(bid);
            const auto& rot = bodyInterface.GetRotation(bid);
            transform.SetTranslation(ToGlm(pos));
            transform.SetRotation(ToGlm(rot));
        }
    }
}

#ifdef BEE_INSPECTOR
void JoltSystem::OnEntity(bee::Entity entity)
{
    auto* joltBody = Engine.ECS().Registry.try_get<JoltBody>(entity);
    if (joltBody)
    {
        if (ImGui::CollapsingHeader("Jolt Physics Body", ImGuiTreeNodeFlags_DefaultOpen))
        {
            BodyID bodyID(joltBody->m_bodyID);
            bool isActive = m_physicsSystem->GetBodyInterface().IsActive(bodyID);
            bool isActiveNew = isActive;
            Engine.Inspector().Inspect("Is Active", isActiveNew);
            if (isActiveNew && !isActive)
                m_physicsSystem->GetBodyInterface().ActivateBody(bodyID);
            else if (!isActiveNew && isActive)
                m_physicsSystem->GetBodyInterface().DeactivateBody(bodyID);
        }
    }
}
#endif

#ifdef BEE_DEBUG
void drawBox(const vec3& min, const vec3& max, const vec3& translation, const quat& rotation, const vec4& color)
{
    vector<vec3> pts{vec3(min.x, min.y, min.z),
                     vec3(max.x, min.y, min.z),
                     vec3(max.x, max.y, min.z),
                     vec3(min.x, max.y, min.z),
                     vec3(min.x, min.y, max.z),
                     vec3(max.x, min.y, max.z),
                     vec3(max.x, max.y, max.z),
                     vec3(min.x, max.y, max.z)};
    for (auto& pt : pts) pt = rotate(rotation, pt) + translation;

    for (int i = 0; i < 4; ++i)
    {
        Engine.DebugRenderer().AddLine(DebugCategory::Physics, pts[i], pts[(i + 1) % 4], color);
        Engine.DebugRenderer().AddLine(DebugCategory::Physics, pts[i + 4], pts[(i + 1) % 4 + 4], color);
        Engine.DebugRenderer().AddLine(DebugCategory::Physics, pts[i], pts[i + 4], color);
    }
}

void JoltSystem::DebugDrawShape(const Shape* shape, const JoltBody& body, const Transform& transform, const vec3& worldOffset)
{
    BodyID bid(body.m_bodyID);
    const auto& posCOM = ToGlm(m_physicsSystem->GetBodyInterface().GetCenterOfMassPosition(bid));
    const auto& posBody = ToGlm(m_physicsSystem->GetBodyInterface().GetPosition(bid));

    const auto& rot = ToGlm(m_physicsSystem->GetBodyInterface().GetRotation(bid));

    // draw center of mass position (red)
    Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posCOM, 0.05f, vec3(1, 0, 0), vec4(1, 0, 0, 1));
    Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posCOM, 0.05f, vec3(0, 1, 0), vec4(1, 0, 0, 1));
    Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posCOM, 0.05f, vec3(0, 0, 1), vec4(1, 0, 0, 1));

    /*// draw body position (pink; not really needed, most things in Jolt seem to be CoM-based)
    Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posBody, 0.05f, vec3(1, 0, 0), vec4(1, 0.75f, 0.75f, 1));
    Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posBody, 0.05f, vec3(0, 1, 0), vec4(1, 0.75f, 0.75f, 1));
    Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posBody, 0.05f, vec3(0, 0, 1), vec4(1, 0.75f, 0.75f, 1));*/

    if (shape->GetSubType() == EShapeSubType::OffsetCenterOfMass)
    {
        auto* ocom = reinterpret_cast<const JPH::OffsetCenterOfMassShape*>(shape);
        auto extraWorldOffset = rotate(rot, ToGlm(-ocom->GetOffset()));

        // recursively draw the shape itself
        DebugDrawShape(ocom->GetInnerShape(), body, transform, worldOffset + extraWorldOffset);
        return;
    }

    // --- draw world AABB

    const auto& joltWorldTransform = m_physicsSystem->GetBodyInterface().GetWorldTransform(bid);
    const auto& aabbOffset = posCOM - posBody + worldOffset;
    const AABox aabb(shape->GetWorldSpaceBounds(joltWorldTransform, Vec3(1, 1, 1)));
    const vec4 aabbColor(0.25f, 0.25f, 0.25f, 1);
    drawBox(ToGlm(aabb.mMin) + aabbOffset, ToGlm(aabb.mMax) + aabbOffset, vec3(0.f), glm::identity<quat>(), aabbColor);

    if (shape->GetType() != EShapeType::Convex) return;

    // --- draw shape details

    if (shape->GetSubType() == EShapeSubType::Box)
    {
        vec4 shapeColor(1, 1, 1, 1);
        auto* box = reinterpret_cast<const JPH::BoxShape*>(shape);
        const auto& half = ToGlm(box->GetHalfExtent());
        drawBox(-half, half, posCOM, rot, shapeColor);
    }
    else if (shape->GetSubType() == EShapeSubType::Sphere)
    {
        vec4 shapeColor(1, 1, 1, 1);
        auto* sphere = reinterpret_cast<const JPH::SphereShape*>(shape);
        float radius = sphere->GetRadius();

        Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posCOM, radius, rotate(rot, vec3(1, 0, 0)), shapeColor);
        Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posCOM, radius, rotate(rot, vec3(0, 1, 0)), shapeColor);
        Engine.DebugRenderer().AddCircle(DebugCategory::Physics, posCOM, radius, rotate(rot, vec3(0, 0, 1)), shapeColor);
    }

    else if (shape->GetSubType() == EShapeSubType::ConvexHull)
    {
        vec4 shapeColor(0, 1, 0, 1);
        const auto& basePos = posCOM + worldOffset;
        auto* convexHull = reinterpret_cast<const JPH::ConvexHullShape*>(shape);
        const auto nrFaces = convexHull->GetNumFaces();
        JPH::uint faceVertices[32];
        for (size_t f = 0; f < nrFaces; ++f)
        {
            auto nrFaceVertices = convexHull->GetNumVerticesInFace((JPH::uint)f);
            convexHull->GetFaceVertices((JPH::uint)f, nrFaceVertices, faceVertices);
            for (size_t v = 0; v < nrFaceVertices; ++v)
            {
                const auto& v0 = ToGlm(convexHull->GetPoint(faceVertices[v]));
                const auto& v1 = ToGlm(convexHull->GetPoint(faceVertices[(v + 1) % nrFaceVertices]));
                Engine.DebugRenderer().AddLine(DebugCategory::Physics,
                                               basePos + rotate(rot, v0),
                                               basePos + rotate(rot, v1),
                                               shapeColor);
            }
        }
    }
}

void JoltSystem::Render()
{
    for (const auto& [entity, joltBody, transform] : Engine.ECS().Registry.view<JoltBody, Transform>().each())
    {
        const auto& shape = m_physicsSystem->GetBodyInterface().GetShape(BodyID(joltBody.m_bodyID));
        DebugDrawShape(shape.GetPtr(), joltBody, transform);
    }
}
#endif

#endif  // BEE_JOLT_PHYSICS