#include "platform/opengl/render_gl.hpp"

#include <imgui/imgui.h>
#include <tinygltf/stb_image.h>  // Implementation of stb_image is in gltf_loader.cpp

#include <glm/glm.hpp>

#include "core/device.hpp"
#include "core/ecs.hpp"
#include "core/engine.hpp"
#include "core/resources.hpp"
#include "core/transform.hpp"
#include "tools/inspector.hpp"
#include "platform/opengl/image_gl.hpp"
#include "platform/opengl/mesh_gl.hpp"
#include "platform/opengl/open_gl.hpp"
#include "platform/opengl/shader_gl.hpp"
#include "platform/opengl/uniforms_gl.hpp"
#include "tools/log.hpp"
#include "tools/profiler.hpp"

#define DEBUG_UBO_LOCATION (UBO_LOCATION_COUNT + 1)
#define SORT_MESH_RENDERERS TRUE

using namespace bee;
using namespace glm;
using namespace std;

static void RenderQuad();
static void RenderCube();
static void SetTexture(const shared_ptr<Texture>& texture, int location);
static int SamplerTypeToGL(Sampler::Filter filter);
static int SamplerTypeToGL(Sampler::Wrap wrap);

Renderer::Renderer()
{
    Title = "Renderer";

    m_forwardPass = Engine.Resources().Load<Shader>(FileIO::Directory::SharedAssets, "/shaders/uber.vert", "shaders/uber.frag");
    m_post = Engine.Resources().Load<Shader>(FileIO::Directory::SharedAssets, "shaders/post.vert", "shaders/post.frag");
    m_shadowPass =
        Engine.Resources().Load<Shader>(FileIO::Directory::SharedAssets, "shaders/depth_only.vert", "shaders/depth_only.frag");
    CreateFrameBuffers();
    CreateShadowMaps();

    // UBOs
    m_cameraData = new CameraUBO();
    m_cameraData->bee_eyePos = vec3(1.0f, 2.0f, 3.0f);
    m_cameraData->bee_time = 3.14f;
    m_cameraData->bee_FogColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    m_cameraData->bee_FogFar = 1000.0f;
    m_cameraData->bee_FogNear = 0.0f;
    glGenBuffers(1, &m_cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), m_cameraData, GL_DYNAMIC_DRAW);
    BEE_DEBUG_ONLY(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    glBindBufferBase(GL_UNIFORM_BUFFER, CAMERA_UBO_LOCATION, m_cameraUBO);
    LabelGL(GL_BUFFER, m_cameraUBO, ("Camera UBO (size:" + to_string(sizeof(CameraUBO)) + ")"));

    m_dirLightsData = new DirectionalLightsUBO();
    glGenBuffers(1, &m_dirLightsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_dirLightsUBO);
    auto s = sizeof(DirectionalLightsUBO);
    glBufferData(GL_UNIFORM_BUFFER, s, m_dirLightsData, GL_DYNAMIC_DRAW);
    BEE_DEBUG_ONLY(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    glBindBufferBase(GL_UNIFORM_BUFFER, DIRECTIONAL_LIGHTS_UBO_LOCATION, m_dirLightsUBO);
    LabelGL(GL_BUFFER, m_dirLightsUBO, ("Dir Lights UBO (size:" + to_string(sizeof(DirectionalLightsUBO)) + ")"));

    m_pointLightsData = new PointLightsUBO();
    glGenBuffers(1, &m_pointLightsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_pointLightsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(PointLightsUBO), m_pointLightsData, GL_DYNAMIC_DRAW);
    BEE_DEBUG_ONLY(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    glBindBufferBase(GL_UNIFORM_BUFFER, LIGHTS_UBO_LOCATION, m_pointLightsUBO);
    LabelGL(GL_BUFFER, m_pointLightsUBO, ("Point Lights UBO (size:" + to_string(sizeof(PointLightsUBO)) + ")"));

    m_transformsData = new TransformsUBO();
    glGenBuffers(1, &m_transformsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_transformsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(TransformsUBO), m_transformsData, GL_DYNAMIC_DRAW);
    BEE_DEBUG_ONLY(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    glBindBufferBase(GL_UNIFORM_BUFFER, TRANSFORMS_UBO_LOCATION, m_transformsUBO);
    LabelGL(GL_BUFFER, m_transformsUBO, ("Transforms UBO (size:" + to_string(sizeof(TransformsUBO)) + ")"));
}

Renderer::~Renderer()
{
    delete m_cameraData;
    delete m_pointLightsData;
    delete m_transformsData;
    delete m_dirLightsData;
    DeleteFrameBuffers();
    DeleteShadowMaps();
    DeleteUBOs();
}

void Renderer::CreateFrameBuffers()
{
    m_width = Engine.Device().GetWidth();
    m_height = Engine.Device().GetHeight();

    //  -- MSAA framebuffer --
    glGenFramebuffers(1, &m_msaaFramebuffer);              // Create
    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebuffer);  // Bind FBO
    LabelGL(GL_FRAMEBUFFER, m_msaaFramebuffer, "[R] MSAA Frame Buffer");

    // MSAA color buffer
    glGenTextures(1, &m_msaaColorbuffer);                         // Create MSAA color attachment
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msaaColorbuffer);  // Bind
    LabelGL(GL_TEXTURE, m_msaaColorbuffer, "[R] MSAA Color Buffer");
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_msaa, GL_RGB, m_width, m_height, GL_TRUE);  // Set storage
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_msaaColorbuffer, 0);  // Attach it

    // MSAA depth buffer
    glGenRenderbuffers(1, &m_msaaDepthbuffer);               // Create
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepthbuffer);  // Bind
    LabelGL(GL_RENDERBUFFER, m_msaaDepthbuffer, "[R] MSAA Depth Buffer");
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaa, GL_DEPTH_COMPONENT, m_width, m_height);    // Set storage
    glBindRenderbuffer(GL_RENDERBUFFER, 0);                                                              // Unbind
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_msaaDepthbuffer);  // Attach it

    // Tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int msaa_attachments[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, msaa_attachments);

    // Check that our framebuffer is OK
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) assert(false);
    BEE_DEBUG_ONLY(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    // -- Resolved framebuffer --
    glGenFramebuffers(1, &m_resolvedFramebuffer);              // Create
    glBindFramebuffer(GL_FRAMEBUFFER, m_resolvedFramebuffer);  // Bind FBO
    LabelGL(GL_FRAMEBUFFER, m_resolvedFramebuffer, "[R] Resolved Frame Buffer");

    // Color buffer
    glGenTextures(1, &m_resolvedColorbuffer);             // Resolved
    glBindTexture(GL_TEXTURE_2D, m_resolvedColorbuffer);  // Bind
    LabelGL(GL_TEXTURE, m_resolvedColorbuffer, "[R] Resolved Color Buffer");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);        // Set storage
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);                                       // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);                                       // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);                                    // Clamping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);                                    // Clamping
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_resolvedColorbuffer, 0);  // Attach it

    // Depth buffer
    glGenRenderbuffers(1, &m_resolvedDepthbuffer);               // Create
    glBindRenderbuffer(GL_RENDERBUFFER, m_resolvedDepthbuffer);  // Bind
    LabelGL(GL_RENDERBUFFER, m_resolvedDepthbuffer, "[R] Resolved Depth Buffer");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_width, m_height);                           // Set storage
    glBindRenderbuffer(GL_RENDERBUFFER, 0);                                                                  // Unbind
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_resolvedDepthbuffer);  // Attach it

    unsigned int resolved_attachments[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, resolved_attachments);

    // Check that our framebuffer is OK
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) assert(false);
    BEE_DEBUG_ONLY(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    // -- Final framebuffer --
    glGenFramebuffers(1, &m_finalFramebuffer);              // Create
    glBindFramebuffer(GL_FRAMEBUFFER, m_finalFramebuffer);  // Bind FBO
    LabelGL(GL_FRAMEBUFFER, m_finalFramebuffer, "[R] Final Frame Buffer");

    // Color buffer
    glGenTextures(1, &m_finalColorbuffer);             // Resolved
    glBindTexture(GL_TEXTURE_2D, m_finalColorbuffer);  // Bind
    LabelGL(GL_TEXTURE, m_finalColorbuffer, "[R] Final Color Buffer");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);     // Set storage
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);                                    // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);                                    // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);                                 // Clamping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);                                 // Clamping
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_finalColorbuffer, 0);  // Attach it

    unsigned int finalAttachments[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, finalAttachments);

    // Check that our framebuffer is OK
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) assert(false);
    BEE_DEBUG_ONLY(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

void Renderer::DeleteFrameBuffers()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteTextures(1, &m_msaaColorbuffer);
    glDeleteRenderbuffers(1, &m_msaaDepthbuffer);
    glDeleteFramebuffers(1, &m_msaaFramebuffer);

    glDeleteTextures(1, &m_resolvedColorbuffer);
    glDeleteRenderbuffers(1, &m_resolvedDepthbuffer);
    glDeleteFramebuffers(1, &m_resolvedFramebuffer);
}

void Renderer::DeleteShadowMaps()
{
    glDeleteTextures(6, m_shadowMaps);
    glDeleteFramebuffers(6, m_shadowFBOs);
}

void Renderer::RenderCurrentInstances(int instances)
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_transformsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(TransformsUBO), m_transformsData, GL_DYNAMIC_DRAW);
    BEE_DEBUG_ONLY(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    glBindVertexArray(m_currentMesh->GetVAO());
    glDrawElementsInstanced(GL_TRIANGLES, m_currentMesh->GetCount(), m_currentMesh->GetIndexType(), nullptr, instances);

//#ifdef BEE_INSPECTOR
    m_drawCalls++;
    m_drawInstances += instances;
//#endif
}

void Renderer::SetFog(vec4 fogColor, float forNear, float fogFar)
{
    m_cameraData->bee_FogNear = forNear;
    m_cameraData->bee_FogFar = fogFar;
    m_cameraData->bee_FogColor = fogColor;
}

void Renderer::CreateShadowMaps()
{
    for (int i = 0; i < m_max_dir_lights; i++)
    {
        const int size = m_shadowResolution;

        // Shadows being made
        glGenFramebuffers(1, &m_shadowFBOs[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBOs[i]);
        LabelGL(GL_FRAMEBUFFER, m_shadowFBOs[i], ("[R] Shadow Map FBO" + to_string(i)).c_str());

        glGenTextures(1, &m_shadowMaps[i]);
        glBindTexture(GL_TEXTURE_2D, m_shadowMaps[i]);
        LabelGL(GL_TEXTURE, m_shadowMaps[i], ("[R] Shadow Map" + to_string(i)).c_str());

        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float borderColor[] = {1.0, 1.0, 1.0, 1.0};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowMaps[i], 0);

        // Check that our framebuffer is OK
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) assert(false);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void Renderer::RenderShadowMaps()
{
    BEE_PROFILE_FUNCTION();
    int i = 0;
    for (const auto& [lightEntity, lightTransform, light] : Engine.ECS().Registry.view<Transform, Light>().each())
    {
        if (light.Type == Light::Type::Directional && i < m_max_dir_lights && light.CastShadows && light.ShadowExtent > 0)
        {
            glViewport(0, 0, m_shadowResolution, m_shadowResolution);
            glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBOs[i]);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);

            m_shadowPass->Activate();
            const float size = light.ShadowExtent;
            const mat4& projection = ortho(size * -0.5f, size * 0.5f, size * -0.5f, size * 0.5f, size * -0.5f, size * 0.5f);
            const mat4& view = inverse(lightTransform.World());
            const mat4& vp = projection * view;

            m_currentMesh.reset();
            int instances = 0;

            for (const auto& [entity, renderer, transform] : Engine.ECS().Registry.view<MeshRenderer, Transform>().each())
            {
                // Check if end of batch is reached
                if (m_currentMesh != renderer.Mesh || instances > MAX_TRANSFORM_INSTANCES - 1)
                {
                    // Check if batch is valid
                    if (m_currentMesh && instances > 0) RenderCurrentInstances(instances);

                    // Start with a new batch
                    m_currentMesh = renderer.Mesh;
                    instances = 0;
                }

                // Always fill in the information for this object
                const mat4& world = transform.World();
                m_transformsData->bee_transforms[instances].wvp = vp * world;
                instances++;
            }

            // Render last buffer
            if (instances > 0) RenderCurrentInstances(instances);

            BEE_DEBUG_ONLY(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        }
        ++i;
    }
}

void Renderer::DeleteUBOs()
{
    glDeleteBuffers(1, &m_cameraUBO);
    glDeleteBuffers(1, &m_dirLightsUBO);
    glDeleteBuffers(1, &m_pointLightsUBO);
    glDeleteBuffers(1, &m_transformsUBO);
}

void Renderer::SetAlphaBlending2D(bool value) { m_useAlphaBlending = value; }
void Renderer::SetVignette(float value) { m_vignette = glm::clamp<float>(value, 0.f, 1.f); }

void bee::Renderer::setRenderSize(int width, int height)
{
    m_width = width;
    m_height = height;

    //Reset all buffers to be the new size.
    glBindFramebuffer(GL_FRAMEBUFFER, m_msaa);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msaaColorbuffer);  // Bind
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_msaa, GL_RGB, m_width, m_height, GL_TRUE);  // Set storage
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_msaaColorbuffer, 0);  // Attach it

    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepthbuffer);  // Bind
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaa, GL_DEPTH_COMPONENT, m_width, m_height);    // Set storage
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_msaaDepthbuffer);  // Attach it

    glBindTexture(GL_TEXTURE_2D, m_finalColorbuffer);  // Bind
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);     // Set storage
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_finalColorbuffer, 0);  // Attach it

    // Unbind
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);      
}

void bee::Renderer::setBackGroundColor(float R, float G, float B)
{
    m_backColor[0] = R;
    m_backColor[1] = G;
    m_backColor[2] = B;
}

void Renderer::Render()
{
//#ifdef BEE_INSPECTOR
    m_drawCalls = 0;
    m_drawInstances = 0;
//#endif

    BEE_PROFILE_FUNCTION();

    RenderShadowMaps();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // Default framebuffer
    glClearColor(m_backColor[0], m_backColor[1], m_backColor[2], 1.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebuffer);
    glViewport(0, 0, m_width, m_height);
    glClearColor(m_backColor[0], m_backColor[1], m_backColor[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);

    if (m_useAlphaBlending)
    {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    m_forwardPass->Activate();
    vec2 resolution((float)m_width, (float)m_height);
    m_forwardPass->GetParameter("u_resolution")->SetValue(resolution);
    if (m_iblSpecularMipCount != -1) m_forwardPass->GetParameter("u_ibl_specular_mip_count")->SetValue(m_iblSpecularMipCount);
    m_forwardPass->GetParameter("use_alpha_blending")->SetValue(m_useAlphaBlending);

    auto lights = Engine.ECS().Registry.view<Transform, Light>();
    int dirLightCount = 0;
    int pointLightCount = 0;
    for (const auto& e : lights)
    {
        const auto& l = lights.get<Light>(e);
        auto& t = lights.get<Transform>(e);
        if (l.Type == Light::Type::Directional && dirLightCount < m_max_dir_lights)
        {
            const float size = l.ShadowExtent;
            const mat4 projection = ortho(size * -0.5f, size * 0.5f, size * -0.5f, size * 0.5f, size * -0.5f, size * 0.5f);
            const mat4 view = inverse(t.World());
            const mat4 vp = projection * view;

            auto& sl = m_dirLightsData->bee_directional_lights[dirLightCount];
            sl.color = l.Color;
            sl.intensity = l.Intensity;
            sl.direction = t.World() * vec4(0.0f, 0.0f, 1.0f, 0.0f);
            sl.shadow_matrix = vp;

            if (dirLightCount++ > m_max_dir_lights) break;
        }

        if (l.Type == Light::Type::Point)
        {
            auto& sl = m_pointLightsData->bee_point_lights[pointLightCount];
            sl.color = l.Color;
            sl.intensity = l.Intensity;
            sl.position = t.GetTranslation();
            sl.range = l.Range;
            if (pointLightCount++ > MAX_POINT_LIGHT_INSTANCES) break;
        }
    }

    for (int i = 0; i < m_max_dir_lights; i++)
    {
        int sampler = SHADOWMAP_LOCATION + i;
        glActiveTexture(GL_TEXTURE0 + sampler);
        glBindTexture(GL_TEXTURE_2D, m_shadowMaps[i]);
        glUniform1i(sampler, sampler);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, m_dirLightsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(DirectionalLightsUBO), m_dirLightsData, GL_DYNAMIC_DRAW);
    BEE_DEBUG_ONLY(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    glBindBuffer(GL_UNIFORM_BUFFER, m_pointLightsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(PointLightsUBO), m_pointLightsData, GL_DYNAMIC_DRAW);
    BEE_DEBUG_ONLY(glBindBuffer(GL_UNIFORM_BUFFER, 0));

#pragma region OPTIMIZATION
#if SORT_MESH_RENDERERS == TRUE
    Engine.ECS().Registry.sort<MeshRenderer>(
        [](const MeshRenderer& lhs, const MeshRenderer& rhs)
        {
            if (lhs.Material == rhs.Material) return lhs.Mesh.get() < rhs.Mesh.get();
            return lhs.Material.get() < rhs.Material.get();
        });
#endif
#pragma endregion

    for (const auto& [e, camera, cameraTransform] : Engine.ECS().Registry.view<Camera, Transform>().each())
    {
        const mat4& view = inverse(cameraTransform.World());
        const vec4 eyePos(cameraTransform.GetTranslation(), 1.0f);  // cameraWorld* vec4(0.0f, 0.0f, 0.0f, 1.0f);

        m_cameraData->bee_view = view;
        m_cameraData->bee_projection = camera.Projection;
        m_cameraData->bee_viewProjection = camera.Projection * view;
        m_cameraData->bee_eyePos = eyePos;
        m_cameraData->bee_directionalLightsCount = dirLightCount;
        m_cameraData->bee_pointLightsCount = pointLightCount;
        m_cameraData->bee_resolution = vec2((float)Engine.Device().GetWidth(), (float)Engine.Device().GetHeight());
        glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), m_cameraData, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        m_currentMaterial.reset();
        m_currentMesh.reset();

        if (m_useAlphaBlending)
        {
            // Sort the objects by z value
            std::vector<std::tuple<bee::Entity, MeshRenderer, Transform>> drawables;
            for (const auto& [entity, renderer, transform] : Engine.ECS().Registry.view<MeshRenderer, Transform>().each())
                drawables.push_back({entity, renderer, transform});

            struct CompareDrawables
            {
                bool operator()(const std::tuple<bee::Entity, MeshRenderer, Transform>& d1,
                                const std::tuple<bee::Entity, MeshRenderer, Transform>& d2)
                {
                    const auto& t1 = get<2>(d1);
                    const auto& t2 = get<2>(d2);
                    if (t1.GetTranslation().z == t2.GetTranslation().z)
                    {
                        if (t1.GetTranslation().y == t2.GetTranslation().y)
                            return t1.GetTranslation().x < t2.GetTranslation().x;
                        return t1.GetTranslation().y < t2.GetTranslation().y;
                    }
                    return t1.GetTranslation().z < t2.GetTranslation().z;
                }
            };
            std::sort(drawables.begin(), drawables.end(), CompareDrawables());

            int instances = 0;
            // Render all objects; try instancing as much as possible
            for (auto& [entity, renderer, transform] : drawables) ProcessObjectForRendering(renderer, transform, instances);

            // Render last buffer
            if (instances > 0)
            {
                ApplyMaterial(m_currentMaterial);
                RenderCurrentInstances(instances);
            }
        }
        else
        {
            // Render all objects; try instancing as much as possible
            int instances = 0;
            for (const auto& [entity, renderer, transform] : Engine.ECS().Registry.view<MeshRenderer, Transform>().each())
                ProcessObjectForRendering(renderer, transform, instances);

            // Render last buffer
            if (instances > 0)
            {
                ApplyMaterial(m_currentMaterial);
                RenderCurrentInstances(instances);
            }
        }
    }

    // Resolve MSAA
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolvedFramebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Final pass
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, m_finalFramebuffer);
    glViewport(0, 0, m_width, m_height);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_resolvedColorbuffer);
    m_post->Activate();
    m_post->GetParameter("u_vignette")->SetValue(m_vignette);
    RenderQuad();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_finalFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDrawBuffer(GL_BACK);
    glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void Renderer::ProcessObjectForRendering(const MeshRenderer& renderer, Transform& transform, int& instances)
{
    // Check if end of batch is reached
    if (m_currentMesh != renderer.Mesh || m_currentMaterial != renderer.Material || instances > MAX_TRANSFORM_INSTANCES - 1)
    {
        // Check if batch is valid
        if (m_currentMaterial && m_currentMesh && instances > 0)
        {
            ApplyMaterial(m_currentMaterial);
            RenderCurrentInstances(instances);
        }

        // Start with a new batch
        m_currentMesh = renderer.Mesh;
        m_currentMaterial = renderer.Material;
        instances = 0;
    }

    // Always fill in the information for this object
    const auto& world = transform.World();
    m_transformsData->bee_transforms[instances].world = world;
    m_transformsData->bee_transforms[instances].wvp = m_cameraData->bee_viewProjection * world;
    instances++;
}

void Renderer::LoadEnvironment(FileIO::Directory directory, const std::string& filename)
{
    /////////////////////////////////////// Load HDR texture /////////////////////////////////////////////
    {
        const auto buffer = Engine.FileIO().ReadBinaryFile(directory, filename);
        int width = 0;
        int height = 0;
        int channels = 0;

        if (!buffer.empty())
        {
            stbi_set_flip_vertically_on_load(true);
            float* data = stbi_loadf_from_memory(reinterpret_cast<const unsigned char*>(buffer.data()),
                                                 (int)buffer.size(),
                                                 &width,
                                                 &height,
                                                 &channels,
                                                 0);
            stbi_set_flip_vertically_on_load(false);
            if (data)
            {
                glGenTextures(1, &m_hdrTexture);
                glBindTexture(GL_TEXTURE_2D, m_hdrTexture);
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGB16F,
                             width,
                             height,
                             0,
                             GL_RGB,
                             GL_FLOAT,
                             data);  // note how we specify the texture's data value to be float

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                stbi_image_free(data);
            }
            else
            {
                Log::Error("Image could not be loaded from a HDR file. URI:{}", filename);
            }
        }
        else
        {
            Log::Error("Image could not be loaded from a file. URI:{}", filename);
        }
    }

    /////////////////////////////////////  Framebuffer and Renderbuffer ////////////////////////////////////
    //
    // Setup framebuffer
    unsigned int captureFBO = 0;
    unsigned int captureRBO = 0;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 1024, 1024);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    /////////////////////////////////////  Cubemap from equirectangular ////////////////////////////////////
    {
        mat4 captureProjection = perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        mat4 captureViews[] = {lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f)),
                               lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f)),
                               lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)),
                               lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f)),
                               lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, -1.0f, 0.0f)),
                               lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, -1.0f, 0.0f))};

        glGenTextures(1, &m_envCubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        auto equirectangularToCubemapShader = Engine.Resources().Load<Shader>(FileIO::Directory::SharedAssets,
                                                                              "shaders/cubemap.vert",
                                                                              "shaders/equirectangular_to_cubemap.frag");

        // pbr: convert HDR equirectangular environment map to cubemap equivalent
        equirectangularToCubemapShader->Activate();
        equirectangularToCubemapShader->GetParameter("equirectangularMap")->SetValue(0);
        equirectangularToCubemapShader->GetParameter("projection")->SetValue(captureProjection);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_hdrTexture);

        glViewport(0, 0, 512, 512);  // don't forget to configure the viewport to the capture dimensions.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (unsigned int i = 0; i < 6; ++i)
        {
            equirectangularToCubemapShader->GetParameter("view")->SetValue(captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_envCubemap, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            RenderCube();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }

    auto filterIBL =
        Engine.Resources().Load<Shader>(FileIO::Directory::SharedAssets, "shaders/quad.vert", "shaders/filter_ibl.frag");

    /////////////////////////////////////  Diffuse IBL ////////////////////////////////////
    {
        int textureSize = 32;
        glGenTextures(1, &m_diffuseIBL);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_diffuseIBL);
        for (int i = 0; i < 6; ++i)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0,
                         GL_RGB16F,
                         textureSize,
                         textureSize,
                         0,
                         GL_RGB,
                         GL_FLOAT,
                         nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_diffuseIBL);

        filterIBL->Activate();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
        filterIBL->GetParameter("u_generate_lut")->SetValue(0);
        filterIBL->GetParameter("u_lod_bias")->SetValue(2.0f);
        filterIBL->GetParameter("u_sample_count")->SetValue(2048);
        filterIBL->GetParameter("u_distribution")->SetValue(0);  // c_Lambert = 0
        filterIBL->GetParameter("u_roughness")->SetValue(0.0f);
        filterIBL->GetParameter("u_width")->SetValue(textureSize);
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glViewport(0, 0, textureSize, textureSize);
        for (int i = 0; i < 6; ++i)
        {
            filterIBL->GetParameter("u_current_face")->SetValue(i);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_diffuseIBL, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            RenderQuad();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    /////////////////////////////////////  Specular IBL ////////////////////////////////////
    // We will create a cubemap with mipmaps for the specular IBLs
    // Each level in the mipmap chain relates to a rougness value
    {
        int lowestMipLevel = 4;
        int textureSize = 512;
        m_iblSpecularMipCount = (int)floor(std::log2(textureSize)) + 1 - lowestMipLevel;

        glGenTextures(1, &m_specularIBL);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_specularIBL);
        for (int i = 0; i < 6; ++i)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0,
                         GL_RGB16F,
                         textureSize,
                         textureSize,
                         0,
                         GL_RGB,
                         GL_FLOAT,
                         nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_specularIBL);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        filterIBL->Activate();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);

        filterIBL->GetParameter("u_generate_lut")->SetValue(0);
        filterIBL->GetParameter("u_lod_bias")->SetValue(0.0f);
        filterIBL->GetParameter("u_sample_count")->SetValue(1024);
        filterIBL->GetParameter("u_distribution")->SetValue(1);  // c_GGX = 1

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (int level = 0; level < m_iblSpecularMipCount; level++)
        {
            float roughness = float(level) / (m_iblSpecularMipCount - 1.0f);
            int width = textureSize >> level;

            filterIBL->GetParameter("u_roughness")->SetValue(roughness);
            filterIBL->GetParameter("u_width")->SetValue(512);
            glViewport(0, 0, width, width);
            for (int i = 0; i < 6; ++i)
            {
                filterIBL->GetParameter("u_current_face")->SetValue(i);
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                                       GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                                       m_specularIBL,
                                       level);
                glClear(GL_COLOR_BUFFER_BIT);
                RenderQuad();
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    /////////////////////////////////////  Look Up Table IBL ////////////////////////////////////
    // We will create a LUT for the specular IBL reflection.
    // The two parameters are
    {
        int res = 512;
        glGenTextures(1, &m_lutIBL);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_lutIBL);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, res, res, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        LabelGL(GL_TEXTURE, m_lutIBL, "[R] IBL LUT");

        filterIBL->Activate();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
        glUniform1i(0, 0);
        filterIBL->GetParameter("u_generate_lut")->SetValue(1);  // true
        filterIBL->GetParameter("u_lod_bias")->SetValue(0.0f);
        filterIBL->GetParameter("u_sample_count")->SetValue(2056);
        filterIBL->GetParameter("u_distribution")
            ->SetValue(1);  // c_GGX = 1			filterIBL->GetParameter("u_roughness")->SetValue(roughness);
        filterIBL->GetParameter("u_width")->SetValue(res);
        filterIBL->GetParameter("u_current_face")->SetValue(0);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glViewport(0, 0, res, res);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_lutIBL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_lutIBL, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        RenderQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::ApplyMaterial(const std::shared_ptr<Material>& material)
{
    // if (material != m_currentMaterial)
    {
        m_currentMaterial = material;

        if (material->BaseColorTexture) SetTexture(material->BaseColorTexture, BASE_COLOR_SAMPLER_LOCATION);

        if (material->UseNormalTexture) SetTexture(material->NormalTexture, NORMAL_SAMPLER_LOCATION);

        if (material->UseMetallicRoughnessTexture)
            SetTexture(material->MetallicRoughnessTexture, ORM_SAMPLER_LOCATION);

        if (material->UseOcclusionTexture) SetTexture(material->OcclusionTexture, OCCLUSION_SAMPLER_LOCATION);

        if (material->UseEmissiveTexture) SetTexture(material->EmissiveTexture, EMISSIVE_SAMPLER_LOCATION);

        glActiveTexture(GL_TEXTURE0 + SPECULAR_SAMPER_LOCATION);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_specularIBL);
        glUniform1i(SPECULAR_SAMPER_LOCATION, SPECULAR_SAMPER_LOCATION);

        glActiveTexture(GL_TEXTURE0 + DIFFUSE_SAMPER_LOCATION);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_diffuseIBL);
        glUniform1i(DIFFUSE_SAMPER_LOCATION, DIFFUSE_SAMPER_LOCATION);

        glActiveTexture(GL_TEXTURE0 + LUT_SAMPER_LOCATION);
        glBindTexture(GL_TEXTURE_2D, m_lutIBL);
        glUniform1i(LUT_SAMPER_LOCATION, LUT_SAMPER_LOCATION);
    }

    m_forwardPass->GetParameter("base_color_factor")->SetValue(material->BaseColorFactor);
    m_forwardPass->GetParameter("use_base_texture")->SetValue(material->UseBaseTexture);
    m_forwardPass->GetParameter("use_occlusion_texture")->SetValue(material->UseBaseTexture);
    m_forwardPass->GetParameter("use_metallic_roughness_texture")->SetValue(material->UseMetallicRoughnessTexture);
    m_forwardPass->GetParameter("use_normal_texture")->SetValue(material->UseNormalTexture);
    m_forwardPass->GetParameter("metallic_factor")->SetValue(material->MetallicFactor);
    m_forwardPass->GetParameter("roughness_factor")->SetValue(material->RoughnessFactor);
    m_forwardPass->GetParameter("use_emissive_texture")->SetValue(material->UseEmissiveTexture);
    m_forwardPass->GetParameter("is_unlit")->SetValue(material->IsUnlit);
    m_forwardPass->GetParameter("u_recieve_shadows")->SetValue(material->ReceiveShadows);

#ifdef BEE_DEBUG
    m_forwardPass->GetParameter("debug_base_color")->SetValue(m_debugData.BaseColor);
    m_forwardPass->GetParameter("debug_normals")->SetValue(m_debugData.Normals);
    m_forwardPass->GetParameter("debug_normal_map")->SetValue(m_debugData.NormalMap);
    m_forwardPass->GetParameter("debug_metallic")->SetValue(m_debugData.Metallic);
    m_forwardPass->GetParameter("debug_roughness")->SetValue(m_debugData.Roughness);
    m_forwardPass->GetParameter("debug_emissive")->SetValue(m_debugData.Emissive);
    m_forwardPass->GetParameter("debug_occlusion")->SetValue(m_debugData.Occlusion);
#endif
}

// Renders a 1x1 XY quad in NDC
void RenderQuad()
{
    static unsigned int quadVAO = 0;
    static unsigned int quadVBO = 0;

    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture coordinates
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        // Linter warning "modernize-use-nullptr" does not make sense for this use case; we truly mean the value 0 here
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);  // NOLINT(modernize-use-nullptr)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// RenderCube() renders a 1x1 3D cube in NDC.
void RenderCube()
{
    static GLuint cubeVAO = 0;
    static GLuint cubeVBO = 0;
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            0.0f,
            -1.0f,
            0.0f,
            0.0f,
            // bottom-left
            1.0f,
            1.0f,
            -1.0f,
            0.0f,
            0.0f,
            -1.0f,
            1.0f,
            1.0f,
            // top-right
            1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            0.0f,
            -1.0f,
            1.0f,
            0.0f,
            // bottom-right
            1.0f,
            1.0f,
            -1.0f,
            0.0f,
            0.0f,
            -1.0f,
            1.0f,
            1.0f,
            // top-right
            -1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            0.0f,
            -1.0f,
            0.0f,
            0.0f,
            // bottom-left
            -1.0f,
            1.0f,
            -1.0f,
            0.0f,
            0.0f,
            -1.0f,
            0.0f,
            1.0f,
            // top-left
            // front face
            -1.0f,
            -1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            // bottom-left
            1.0f,
            -1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            0.0f,
            // bottom-right
            1.0f,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            // top-right
            1.0f,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            // top-right
            -1.0f,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            1.0f,
            // top-left
            -1.0f,
            -1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            // bottom-left
            // left face
            -1.0f,
            1.0f,
            1.0f,
            -1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            // top-right
            -1.0f,
            1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            // top-left
            -1.0f,
            -1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            // bottom-left
            -1.0f,
            -1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            // bottom-left
            -1.0f,
            -1.0f,
            1.0f,
            -1.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            // bottom-right
            -1.0f,
            1.0f,
            1.0f,
            -1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,  // top-right
            // right face
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,  // top-left
            1.0f,
            -1.0f,
            -1.0f,
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,  // bottom-right
            1.0f,
            1.0f,
            -1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,  // top-right
            1.0f,
            -1.0f,
            -1.0f,
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,  // bottom-right
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,  // top-left
            1.0f,
            -1.0f,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,  // bottom-left
            // bottom face
            -1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            -1.0f,
            0.0f,
            0.0f,
            1.0f,  // top-right
            1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            -1.0f,
            0.0f,
            1.0f,
            1.0f,  // top-left
            1.0f,
            -1.0f,
            1.0f,
            0.0f,
            -1.0f,
            0.0f,
            1.0f,
            0.0f,  // bottom-left
            1.0f,
            -1.0f,
            1.0f,
            0.0f,
            -1.0f,
            0.0f,
            1.0f,
            0.0f,  // bottom-left
            -1.0f,
            -1.0f,
            1.0f,
            0.0f,
            -1.0f,
            0.0f,
            0.0f,
            0.0f,  // bottom-right
            -1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            -1.0f,
            0.0f,
            0.0f,
            1.0f,  // top-right
            // top face
            -1.0f,
            1.0f,
            -1.0f,
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,  // top-left
            1.0f,
            1.0f,
            1.0f,
            0.0f,
            1.0f,
            0.0f,
            1.0f,
            0.0f,  // bottom-right
            1.0f,
            1.0f,
            -1.0f,
            0.0f,
            1.0f,
            0.0f,
            1.0f,
            1.0f,  // top-right
            1.0f,
            1.0f,
            1.0f,
            0.0f,
            1.0f,
            0.0f,
            1.0f,
            0.0f,  // bottom-right
            -1.0f,
            1.0f,
            -1.0f,
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,  // top-left
            -1.0f,
            1.0f,
            1.0f,
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            0.0f  // bottom-left
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        // Linter warning "modernize-use-nullptr" does not make sense for this use case; we truly mean the value 0 here
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);  // NOLINT(modernize-use-nullptr)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void SetTexture(const shared_ptr<Texture>& texture, int location)
{
    glActiveTexture(GL_TEXTURE0 + location);
    glBindTexture(GL_TEXTURE_2D, texture->Image->GetTextureId());
    glUniform1i(location, location);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, SamplerTypeToGL(texture->Sampler->MinFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, SamplerTypeToGL(texture->Sampler->MagFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, SamplerTypeToGL(texture->Sampler->WrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, SamplerTypeToGL(texture->Sampler->WrapT));
}

int SamplerTypeToGL(Sampler::Filter filter)
{
    switch (filter)
    {
        case bee::Sampler::Filter::Nearest:
            return GL_NEAREST;
        case bee::Sampler::Filter::Linear:
            return GL_LINEAR;
        case bee::Sampler::Filter::NearestMipmapNearest:
            return GL_NEAREST_MIPMAP_NEAREST;
        case bee::Sampler::Filter::LinearMipmapNearest:
            return GL_LINEAR_MIPMAP_NEAREST;
        case bee::Sampler::Filter::NearestMipmapLinear:
            return GL_NEAREST_MIPMAP_LINEAR;
        case bee::Sampler::Filter::LinearMipmapLinear:
            return GL_LINEAR_MIPMAP_LINEAR;
    }
    return 0;
}

int SamplerTypeToGL(Sampler::Wrap wrap)
{
    switch (wrap)
    {
        case bee::Sampler::Wrap::ClampToEdge:
            return GL_CLAMP_TO_EDGE;
        case bee::Sampler::Wrap::MirroredRepeat:
            return GL_MIRRORED_REPEAT;
        case bee::Sampler::Wrap::Repeat:
            return GL_REPEAT;
    }
    return 0;
}

//#ifdef BEE_INSPECTOR

void Renderer::Reload()
{
    if (m_reload)
    {
        m_forwardPass->Reload();
        m_post->Reload();
        m_reload = false;
    }
}

void Renderer::OnStatsBar()
{
    ImGui::Text("%s %d [%d]", ICON_FA_CAMERA, m_drawCalls, m_drawInstances);
}

void Renderer::OnPanel()
{
    Engine.Inspector().Inspect(m_debugData);
    ImGui::DragFloat("Vignette", &m_vignette, 0.01f, 0.0f, 1.0f);
    ImGui::DragInt("Draw Calls", &m_drawCalls);
    ImGui::DragInt("Draw Instances", &m_drawInstances);
    if (ImGui::Button("Reload")) m_reload = true;
}

void Renderer::OnEntity(entt::entity e)
{
    ImGui::PushID((void*)e);
    auto* l = Engine.ECS().Registry.try_get<Light>(e);
    if (l)
    {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Engine.Inspector().Inspect(*l);
        }
    }
    auto* mr = Engine.ECS().Registry.try_get<MeshRenderer>(e);
    if (mr)
    {
        if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Engine.Inspector().Inspect(*mr);
        }
    }
    ImGui::PopID();
}

//#endif