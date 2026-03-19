#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <visit_struct/visit_struct.hpp>

namespace bee
{

class Model;
class Mesh;
struct Texture;
class Image;

struct Material
{
    Material(const Model& model, int index);
    Material() = default;

    glm::vec4 BaseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // 16
    bool UseBaseTexture = false;                                    // 4

    glm::vec3 EmissiveFactor = glm::vec3(0.0f, 0.0f, 0.0f);  // 12
    bool UseEmissiveTexture = false;                         // 4

    float NormalTextureScale = 0.0f;  // 4
    bool UseNormalTexture = false;    // 4

    float OcclusionTextureStrength = 0.0f;  // 4
    bool UseOcclusionTexture = false;       // 4

    bool UseMetallicRoughnessTexture = false;  // 4
    float MetallicFactor = 0.0f;               // 4
    float RoughnessFactor = 1.0f;              // 4

    bool IsUnlit = false;  // 4
    bool ReceiveShadows = true;

    std::shared_ptr<Texture> BaseColorTexture;
    std::shared_ptr<Texture> EmissiveTexture;
    std::shared_ptr<Texture> NormalTexture;
    std::shared_ptr<Texture> OcclusionTexture;
    std::shared_ptr<Texture> MetallicRoughnessTexture;
};

struct Light
{
    enum class Type
    {
        Point,
        Directional,
        Spot
    };
    Light() = default;
    Light(const Model& model, int index);
    Light(const glm::vec3& color, float intensity, float range, Type type)
        : Color(color), Intensity(intensity), Range(range), Type(type)
    {
    }
    glm::vec3 Color = {};
    float Intensity = 0;
    float Range = 0;
    float ShadowExtent = 30.0f;
    bool CastShadows = true;
    Type Type = Type::Point;
};

struct Camera
{
    glm::mat4 Projection;
};

struct MeshRenderer
{
    std::shared_ptr<bee::Mesh> Mesh;
    std::shared_ptr<bee::Material> Material;

    MeshRenderer() {}

    MeshRenderer(std::shared_ptr<bee::Mesh> mesh, std::shared_ptr<bee::Material> material)
    {
        Mesh = mesh;
        Material = material;
    }
};

struct Sampler
{
    Sampler(const Model& model, int index);
    Sampler();

    enum class Filter
    {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear
    };

    enum class Wrap
    {
        Repeat,
        ClampToEdge,
        MirroredRepeat
    };

    Filter MagFilter = Filter::Nearest;
    Filter MinFilter = Filter::Nearest;
    Wrap WrapS = Wrap::ClampToEdge;
    Wrap WrapT = Wrap::ClampToEdge;

private:
    static Filter GetFilter(int filter);
    static Wrap GetWrap(int wrap);
};

struct Texture
{
    Texture(const Model& model, int index);
    Texture(std::shared_ptr<Image> image, std::shared_ptr<Sampler> sampler) : Image(image), Sampler(sampler) {}
    std::shared_ptr<Image> Image;
    std::shared_ptr<Sampler> Sampler;
};

}  // namespace bee

VISITABLE_STRUCT(bee::Light, Type, Color, Intensity, Range, CastShadows, ShadowExtent);
