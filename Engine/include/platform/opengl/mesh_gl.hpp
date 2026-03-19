#pragma once
#include <array>
#include <vector>
#include <glm/glm.hpp>
#include "core/resource.hpp"
#include "platform/opengl/render_gl.hpp"

namespace bee
{

class Model;

class Mesh : public Resource
{
    friend class Resources;

public:
    Mesh();
    Mesh(const Model& model, int index);
    ~Mesh() override;
    inline const std::vector<glm::vec3>& GetPositions() const { return m_positions; }
    inline unsigned int GetVAO() const { return m_vao; }
    inline uint32_t GetCount() const { return m_count; }
    inline uint32_t GetIndexType() const { return m_indexType; }

    enum class Attribute
    {
        Position,
        Normal,
        Tangent,
        Color,
        Texture,
        Texture1,
    };

    template <typename DataT>
    void SetAttribute(Attribute attribute, std::vector<DataT>& data);

    void SetIndices(std::vector<uint16_t>& data);

protected:
    void SetAttribute(Attribute attribute, size_t count, void* data);
    void ComputeTangents(const Model& model, int index);
    static std::string GetPath(const Model& model, int index);
    static std::vector<glm::vec4> ComputeTangents(const std::vector<glm::vec3>& positions,
                                                  const std::vector<glm::vec3>& normals,
                                                  const std::vector<glm::vec2>& uvs,
                                                  const std::vector<uint32_t>& indices);
    unsigned int m_vao = 0;
    unsigned int m_ebo = 0;
    std::array<unsigned int, 9> m_vbo = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t m_count = 0;
    uint32_t m_indexType = 0;
    std::vector<glm::vec3> m_positions;
};

template <typename DataT>
inline void Mesh::SetAttribute(Attribute attribute, std::vector<DataT>& data)
{
    SetAttribute(attribute, data.size() * sizeof(data[0]), &data[0]);
}

template <>
inline void Mesh::SetAttribute(Attribute attribute, std::vector<glm::vec3>& data)
{
    SetAttribute(attribute, data.size() * sizeof(data[0]), &data[0]);
    if (attribute == Attribute::Position) m_positions = data;
}

}  // namespace bee
