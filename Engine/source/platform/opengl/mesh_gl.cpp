#include "platform/opengl/mesh_gl.hpp"
#include "platform/opengl/open_gl.hpp"
#include "platform/opengl/uniforms_gl.hpp"
#include "rendering/model.hpp"
#include "tools/log.hpp"

using namespace bee;
using namespace std;

static uint32_t CalculateDataTypeSize(tinygltf::Accessor const& accessor) noexcept;

Mesh::Mesh() : Resource(ResourceType::Mesh)
{
    m_generated = true;
    m_ebo = 0;
    m_path = "Generated Mesh " + std::to_string(Resource::GetNextGeneratedID());

    for (auto& v : m_vbo) v = 0;
    glCreateVertexArrays(1, &m_vao);
    LabelGL(GL_VERTEX_ARRAY, m_vao, "VAO | " + m_path);
}

Mesh::Mesh(const Model& model, int index) : Resource(ResourceType::Mesh)
{
    for (auto& v : m_vbo) v = 0;

    const auto& document = model.GetDocument();
    auto mesh = document.meshes[index];

    m_path = GetPath(model, index);

    // Vertex Array Object
    glCreateVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    LabelGL(GL_VERTEX_ARRAY, m_vao, "VAO:" + m_path);

    // Only one primitive per mesh used in bee
    assert(!mesh.primitives.empty());
    auto primitive = mesh.primitives[0];

    // Loop over all attributes
    for (auto& attribute : primitive.attributes)
    {
        // Get attribute data from GLTF
        const auto& accessor = document.accessors[attribute.second];
        const auto& view = document.bufferViews[accessor.bufferView];
        const auto& buffer = document.buffers[view.buffer];

        // Note: accessor.max / accessor.min are the AABB of the mesh. You can use this for culling if you want.

        if (attribute.first == "POSITION")
        {
            // for consistency sake it's better to skip this lint
            glCreateBuffers(1, &m_vbo[POSITION_LOCATION]);  // NOLINT(readability-container-data-pointer)
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo[POSITION_LOCATION]);
            glBufferData(GL_ARRAY_BUFFER,
                         accessor.count * 12,
                         &buffer.data.at(view.byteOffset + accessor.byteOffset),
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(POSITION_LOCATION);
            glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, (GLsizei)view.byteStride, nullptr);
            LabelGL(GL_BUFFER, m_vbo[POSITION_LOCATION], "VBO |" + m_path + " | POSITION");
            m_positions.resize(accessor.count);
            memcpy(m_positions.data(), &buffer.data.at(view.byteOffset + accessor.byteOffset), accessor.count * 12);
        }
        else if (attribute.first == "NORMAL")
        {
            glCreateBuffers(1, &m_vbo[NORMAL_LOCATION]);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo[NORMAL_LOCATION]);
            glBufferData(GL_ARRAY_BUFFER,
                         accessor.count * 12,
                         &buffer.data.at(view.byteOffset + accessor.byteOffset),
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(NORMAL_LOCATION);
            glVertexAttribPointer(NORMAL_LOCATION, 3, GL_FLOAT, GL_FALSE, (GLsizei)view.byteStride, nullptr);
            LabelGL(GL_BUFFER, m_vbo[NORMAL_LOCATION], "VBO |" + m_path + " | NORMAL");
        }
        else if (attribute.first == "TANGENT")
        {
            glCreateBuffers(1, &m_vbo[TANGENT_LOCATION]);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo[TANGENT_LOCATION]);
            glBufferData(GL_ARRAY_BUFFER,
                         accessor.count * 16,
                         &buffer.data.at(view.byteOffset + accessor.byteOffset),
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(TANGENT_LOCATION);
            glVertexAttribPointer(TANGENT_LOCATION, 4, GL_FLOAT, GL_FALSE, (GLsizei)view.byteStride, nullptr);
        }
        else if (attribute.first == "TEXCOORD_0")
        {
            glCreateBuffers(1, &m_vbo[TEXTURE0_LOCATION]);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo[TEXTURE0_LOCATION]);
            glBufferData(GL_ARRAY_BUFFER,
                         accessor.count * 8,
                         &buffer.data.at(view.byteOffset + accessor.byteOffset),
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(TEXTURE0_LOCATION);
            glVertexAttribPointer(TEXTURE0_LOCATION, 2, GL_FLOAT, GL_FALSE, (GLsizei)view.byteStride, nullptr);
            LabelGL(GL_BUFFER, m_vbo[TEXTURE0_LOCATION], "VBO |" + m_path + " | TEXCOORD_0");
        }
        else if (attribute.first == "TEXCOORD_1")
        {
            glCreateBuffers(1, &m_vbo[TEXTURE1_LOCATION]);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo[TEXTURE1_LOCATION]);
            glBufferData(GL_ARRAY_BUFFER,
                         accessor.count * 8,
                         &buffer.data.at(view.byteOffset + accessor.byteOffset),
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(TEXTURE1_LOCATION);
            glVertexAttribPointer(TEXTURE1_LOCATION, 2, GL_FLOAT, GL_FALSE, (GLsizei)view.byteStride, nullptr);
            LabelGL(GL_BUFFER, m_vbo[TEXTURE1_LOCATION], "VBO |" + m_path + " | TEXCOORD_1");
        }
    }

    // Index buffer
    // Get index data from GLTF
    const auto& accessor = document.accessors[primitive.indices];
    const auto& view = document.bufferViews[accessor.bufferView];
    const auto& buffer = document.buffers[view.buffer];

    // This many to render
    m_count = static_cast<uint32_t>(accessor.count);
    m_indexType = static_cast<uint32_t>(accessor.componentType);
    auto typeSize = CalculateDataTypeSize(accessor);

    glCreateBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 accessor.count * typeSize,
                 &buffer.data.at(accessor.byteOffset + view.byteOffset),
                 GL_DYNAMIC_DRAW);
    LabelGL(GL_BUFFER, m_ebo, "EBO |" + m_path);

    if (m_vbo[TANGENT_LOCATION] == 0) ComputeTangents(model, index);

    BEE_DEBUG_ONLY(glBindVertexArray(0));
}

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(6, m_vbo.data());
    glDeleteBuffers(1, &m_ebo);
}

void Mesh::SetIndices(std::vector<uint16_t>& data)
{
    m_count = (int)data.size();
    m_indexType = GL_UNSIGNED_SHORT;
    glBindVertexArray(m_vao);
    if (m_ebo != 0) glDeleteBuffers(1, &m_ebo);
    glCreateBuffers(1, &m_ebo);
    LabelGL(GL_BUFFER, m_ebo, "EBO:" + m_path);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size() * sizeof(uint16_t), data.data(), GL_DYNAMIC_DRAW);
    BEE_DEBUG_ONLY(glBindVertexArray(0));
    BEE_DEBUG_ONLY(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

void Mesh::SetAttribute(Attribute attribute, size_t count, void* data)
{
    glBindVertexArray(m_vao);

    unsigned int location = 0;
    unsigned int size = 0;
    string locationName;
    switch (attribute)
    {
        case Attribute::Position:
            location = POSITION_LOCATION;
            locationName = "POSITION";
            size = 3;
            break;
        case Attribute::Normal:
            location = NORMAL_LOCATION;
            locationName = "NORMAL";
            size = 3;
            break;
        case Attribute::Tangent:
            location = TANGENT_LOCATION;
            locationName = "TANGENT";
            size = 4;
            break;
        case Attribute::Color:
            location = COLOR_LOCATION;
            locationName = "COLOR";
            size = 3;
            break;
        case Attribute::Texture:
            location = TEXTURE0_LOCATION;
            locationName = "TEXTURE";
            size = 2;
            break;
        case Attribute::Texture1:
            location = TEXTURE1_LOCATION;
            locationName = "TEXTURE1";
            size = 2;
            break;
        default:
            assert(false);
            break;
    }

    if (m_vbo[location] != 0)
    {
        glDeleteBuffers(1, &m_vbo[location]);
        m_vbo[location] = 0;
    }
    glCreateBuffers(1, &m_vbo[location]);
    LabelGL(GL_BUFFER, m_vbo[location], "VBO:" + m_path + " | " + locationName);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo[location]);
    glBufferData(GL_ARRAY_BUFFER, count, data, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, (GLsizei)0, nullptr);
    BEE_DEBUG_ONLY(glBindVertexArray(0));
    BEE_DEBUG_ONLY(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void Mesh::ComputeTangents(const Model& model, int index)
{
    const auto& document = model.GetDocument();
    auto mesh = document.meshes[index];
    auto primitive = mesh.primitives[0];

    std::vector<vec3> normals;
    std::vector<vec2> uvs;
    std::vector<uint32_t> indices;

    // Loop over all attributes
    for (auto& attribute : primitive.attributes)
    {
        // Get attribute data from GLTF
        const auto& accessor = document.accessors[attribute.second];
        const auto& view = document.bufferViews[accessor.bufferView];
        const auto& buffer = document.buffers[view.buffer];

        if (attribute.first == "NORMAL")
        {
            normals.resize(accessor.count);
            memcpy(normals.data(), &buffer.data.at(view.byteOffset + accessor.byteOffset), accessor.count * 12);
        }
        else if (attribute.first == "TEXCOORD_0")
        {
            uvs.resize(accessor.count);
            memcpy(uvs.data(), &buffer.data.at(view.byteOffset + accessor.byteOffset), accessor.count * 8);
        }
    }

    // Index buffer
    const auto& accessor = document.accessors[primitive.indices];
    const auto& view = document.bufferViews[accessor.bufferView];
    const auto& buffer = document.buffers[view.buffer];

    auto typeSize = CalculateDataTypeSize(accessor);
    if (typeSize == 4)
    {
        indices.resize(accessor.count);
        memcpy(indices.data(), &buffer.data.at(view.byteOffset + accessor.byteOffset), accessor.count * sizeof(uint32_t));
    }
    if (typeSize == 2)
    {
        std::vector<uint16_t> temp;
        temp.resize(accessor.count);
        memcpy(temp.data(), &buffer.data.at(view.byteOffset + accessor.byteOffset), accessor.count * sizeof(uint16_t));
        indices.resize(accessor.count);
        for (size_t i = 0; i < temp.size(); i++) indices[i] = temp[i];
    }

    if (m_positions.size() == normals.size() && uvs.size() == normals.size())
    {
        std::vector<vec4> tangents = ComputeTangents(m_positions, normals, uvs, indices);
        SetAttribute(Attribute::Tangent, tangents);
    }
}

std::string Mesh::GetPath(const Model& model, int index)
{
    const auto& mesh = model.GetDocument().meshes[index];
    return model.GetPath() + " | Mesh-" + std::to_string(index) + ": " + mesh.name;
}

std::vector<vec4> Mesh::ComputeTangents(const std::vector<vec3>& positions,
                                        const std::vector<vec3>&,
                                        const std::vector<vec2>& uvs,
                                        const std::vector<uint32_t>& indices)
{
    std::vector<vec4> tangents;
    tangents.resize(positions.size());

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        const auto& v0_pos = positions[indices[i + 0]];
        const auto& v1_pos = positions[indices[i + 1]];
        const auto& v2_pos = positions[indices[i + 2]];
        const auto& v0_tex = uvs[indices[i + 0]];
        const auto& v1_tex = uvs[indices[i + 1]];
        const auto& v2_tex = uvs[indices[i + 2]];
        auto& v0_tangent = tangents[indices[i + 0]];
        auto& v1_tangent = tangents[indices[i + 1]];
        auto& v2_tangent = tangents[indices[i + 2]];

        vec3 edge1 = v1_pos - v0_pos;
        vec3 edge2 = v2_pos - v0_pos;

        float deltaU1 = v1_tex.x - v0_tex.x;
        float deltaV1 = v1_tex.y - v0_tex.y;
        float deltaU2 = v2_tex.x - v0_tex.x;
        float deltaV2 = v2_tex.y - v0_tex.y;
        float f = 1.0f / (deltaU1 * deltaV2 - deltaU2 * deltaV1);

        vec4 tangent;
        tangent.x = f * (deltaV2 * edge1.x - deltaV1 * edge2.x);
        tangent.y = f * (deltaV2 * edge1.y - deltaV1 * edge2.y);
        tangent.z = f * (deltaV2 * edge1.z - deltaV1 * edge2.z);
        // tangent.w = 0.0f;

        // tangent = glm::normalize(tangent);
        tangent.w = 1.0f;

        // Bitangent.x = f * (-deltaU2 * edge1.x + deltaU1 * edge2.x);
        // Bitangent.y = f * (-deltaU2 * edge1.y + deltaU1 * edge2.y);
        // Bitangent.z = f * (-deltaU2 * edge1.z + deltaU1 * edge2.z);

        v0_tangent += tangent;
        v1_tangent += tangent;
        v2_tangent += tangent;
    }

    for (auto& t : tangents) t = glm::normalize(t);

    return tangents;
}

static uint32_t CalculateDataTypeSize(tinygltf::Accessor const& accessor) noexcept
{
    uint32_t elementSize = 0;
    switch (accessor.componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            elementSize = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            elementSize = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
        case TINYGLTF_COMPONENT_TYPE_INT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            elementSize = 4;
            break;
        default:
            assert(false);
    }

    switch (accessor.type)
    {
        case TINYGLTF_TYPE_MAT2:
            return 4 * elementSize;
        case TINYGLTF_TYPE_MAT3:
            return 9 * elementSize;
        case TINYGLTF_TYPE_MAT4:
            return 16 * elementSize;
        case TINYGLTF_TYPE_SCALAR:
            return elementSize;
        case TINYGLTF_TYPE_VEC2:
            return 2 * elementSize;
        case TINYGLTF_TYPE_VEC3:
            return 3 * elementSize;
        case TINYGLTF_TYPE_VEC4:
            return 4 * elementSize;
        default:
            assert(false);
    }

    return 0;
}
