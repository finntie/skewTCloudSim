#include "rendering/model.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <set>

#include "core/device.hpp"
#include "core/engine.hpp"
#include "core/fileio.hpp"
#include "core/resources.hpp"
#include "core/transform.hpp"
#include "rendering/image.hpp"
#include "rendering/mesh.hpp"
#include "tools/log.hpp"
#include "tools/tools.hpp"
#include "tools/profiler.hpp"

using namespace bee;
using namespace std;

Model::Model(FileIO::Directory directory, const std::string& filename) : Resource(ResourceType::Model)
{
    m_path = filename;

    const string fullFilename = Engine.FileIO().GetPath(directory, filename);

    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool res = false;

    // Check which format to load
    if (StringEndsWith(filename, ".gltf"))
    {
        res = loader.LoadASCIIFromFile(&m_model, &err, &warn, fullFilename);
    }
    else if (StringEndsWith(filename, ".glb"))
    {
        res = loader.LoadBinaryFromFile(&m_model, &err, &warn, fullFilename);
    }

    if (!warn.empty()) Log::Warn(warn);

    if (!err.empty()) Log::Error(err);

    if (!res)
    {
        Log::Error("Failed to load glTF: {}", filename);
        return;
    }
    else
    {
        Log::Info("Loaded glTF: {}", filename);
    }

    // Load meshes
    {
        BEE_PROFILE_SCOPE("Load Meshes");
        for (int i = 0; i < static_cast<int>(m_model.meshes.size()); i++)
        {
            auto mesh = Engine.Resources().Load<Mesh>(*this, i);
            m_meshes.push_back(mesh);
        }
    }

    // Load images (texture data)
    {
        BEE_PROFILE_SCOPE("Load Images");
        for (int i = 0; i < static_cast<int>(m_model.images.size()); i++)
        {
            auto image = Engine.Resources().Load<Image>(*this, i);
            m_images.push_back(image);
        }
    }

    // Load samplers
    for (int i = 0; i < static_cast<int>(m_model.samplers.size()); i++)
    {
        auto sampler = make_shared<Sampler>(*this, i);
        m_samplers.push_back(sampler);
    }

    // Load textures
    for (int i = 0; i < static_cast<int>(m_model.textures.size()); i++)
    {
        auto texture = make_shared<Texture>(*this, i);
        m_textures.push_back(texture);
    }

    // Load materials
    for (int i = 0; i < static_cast<int>(m_model.materials.size()); i++)
    {
        auto material = make_shared<Material>(*this, i);
        m_materials.push_back(material);
    }

    // Load lights
    for (int i = 0; i < static_cast<int>(m_model.lights.size()); i++)
    {
        auto light = make_shared<Light>(*this, i);
        m_lights.push_back(light);
    }
}

Model::~Model() = default;

void Model::InstantiateNode(uint32_t nodeIdx, Entity parent) const
{
    const auto& node = m_model.nodes[nodeIdx];
    const auto entity = Engine.ECS().CreateEntity();

    // Transform
    auto& transform = Engine.ECS().CreateComponent<Transform>(entity);
    transform.Name = node.name;
    if (parent != entt::null) transform.SetParent(parent);

    if (!node.matrix.empty())
    {
        glm::mat4 transformGLM = glm::make_mat4(node.matrix.data());
        transform.SetFromMatrix(transformGLM);
    }
    else
    {
        if (!node.scale.empty()) transform.SetScale(to_vec3(node.scale));
        if (!node.rotation.empty()) transform.SetRotation(to_quat(node.rotation));
        if (!node.translation.empty()) transform.SetTranslation(to_vec3(node.translation));
    }

    // Mesh
    if (node.mesh != -1)
    {
        const auto& mesh = m_model.meshes[node.mesh];
        assert(!mesh.primitives.empty());

        const auto& osmMesh = m_meshes[node.mesh];
        if (mesh.primitives[0].material != -1)
        {
            const auto& osmMaterial = m_materials[mesh.primitives[0].material];
            Engine.ECS().CreateComponent<MeshRenderer>(entity, osmMesh, osmMaterial);
        }
        else
        {
            auto osmMaterial = make_shared<Material>();
            Engine.ECS().CreateComponent<MeshRenderer>(entity, osmMesh, osmMaterial);
        }
    }

    // Camera
    if (node.camera != -1)
    {
        auto camera = m_model.cameras[node.camera];
        glm::mat4 projection;
        if (camera.type == "perspective")
        {
            const auto& c = camera.perspective;
            float deviceAspectRatio = (float)Engine.Device().GetWidth() / (float)Engine.Device().GetHeight();
            auto aspectRatio = (float)(c.aspectRatio == 0.0 ? deviceAspectRatio : c.aspectRatio);
            projection = glm::perspective((float)c.yfov, aspectRatio, (float)c.znear, (float)c.zfar);
        }
        else if (camera.type == "orthographic")
        {
            const auto& c = camera.orthographic;
            float hack = 1.0f / 1.77f;  // Orthographic is a bit broken in Blender
            projection =
                glm::ortho(c.xmag * -0.5f, c.xmag * 0.5f, c.ymag * -0.5f * hack, c.ymag * 0.5f * hack, c.znear, c.zfar);
        }
        Engine.ECS().CreateComponent<Camera>(entity, projection);
    }

    if (node.extensions.find("KHR_lights_punctual") != node.extensions.end())
    {
        const auto& klp = node.extensions.at("KHR_lights_punctual");
        const auto& l = klp.Get("light");
        int i = l.GetNumberAsInt();
        Engine.ECS().CreateComponent<Light>(entity, *m_lights[i]);
    }

    // Load children
    for (auto childNode : node.children) InstantiateNode(childNode, entity);
}

void Model::Instantiate(Entity parent) const
{
    for (const uint32_t node : m_model.scenes[0].nodes) InstantiateNode(node, parent);
}

MeshRenderer Model::CreateMeshRendererFromNode(const std::string& nodeName) const
{
    MeshRenderer result;

    const tinygltf::Node* node = nullptr;
    for (const auto& n : m_model.nodes)
        if (n.name == nodeName) node = &n;

    if (node && node->mesh != -1)
    {
        result.Mesh = m_meshes[node->mesh];
        const auto gltfMaterial = m_model.meshes[node->mesh].primitives[0].material;
        if (gltfMaterial != -1) result.Material = m_materials[gltfMaterial];
    }

    return result;
}
