#pragma once
#include <string>

#include "core/resource.hpp"

namespace bee
{

class Model;

class Image : public Resource
{
    friend class Resources;

public:
    Image(const Model& model, int index);
    Image(FileIO::Directory directory, const std::string& path);
    ~Image();

    /// Creates a texture from RGBA provided data
    void CreateGLTextureWithData(unsigned char* data, int width, int height, int channels, bool genMipMaps);
    unsigned int GetTextureId() const { return m_texture; }

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    static std::string GetPath(const Model& model, int index);
    static std::string GetPath(FileIO::Directory directory, const std::string& path)
    {
        return Resource::GetPath(directory, path);
    }

    unsigned int m_texture = 0;
    int m_width = -1;
    int m_height = -1;
    int m_channels = -1;
};

}  // namespace bee
