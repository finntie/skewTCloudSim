#include "platform/opengl/image_gl.hpp"

#include <tinygltf/stb_image.h>  // Implementation of stb_image is in gltf_loader.cpp

#include "core/engine.hpp"
#include "core/fileio.hpp"
#include "core/resources.hpp"
#include "platform/opengl/open_gl.hpp"
#include "rendering/model.hpp"
#include "tools/log.hpp"
#include "tools/tools.hpp"

using namespace bee;
using namespace std;

Image::Image(const Model& model, int index) : Resource(ResourceType::Image)
{
    const auto& image = model.GetDocument().images[index];
    if (image.uri.empty())
    {
        if (image.bufferView >= 0)
        {
            GLubyte* data = nullptr;
            const auto& view = model.GetDocument().bufferViews[image.bufferView];
            const auto& buffer = model.GetDocument().buffers[view.buffer];
            const auto* ptr = &buffer.data.at(view.byteOffset);
            data = stbi_load_from_memory(ptr, (int)buffer.data.size(), &m_width, &m_height, &m_channels, 4);
            if (data)
            {
                CreateGLTextureWithData(data, m_width, m_height, 4, true);
                stbi_image_free(data);
            }
            else
            {
                Log::Error("Image could not be loaded from a PNG file. Image:{} URI:{}", GetPath(model, index), image.uri);
            }
        }
        else if (!image.image.empty())
        {
            m_width = image.width;
            m_height = image.height;
            m_channels = image.component;
            CreateGLTextureWithData((unsigned char*)image.image.data(), m_width, m_height, 4, true);
        }
    }
    else
    {
        auto uri = model.GetPath();
        const auto lastSlashIdx = uri.rfind("/");
        uri = uri.substr(0, lastSlashIdx + 1);
        uri += image.uri;
        // const string path = Engine.Resources().GetPath(uri);
        const auto buffer = Engine.FileIO().ReadBinaryFile(FileIO::Directory::Assets, uri);

        if (!buffer.empty())
        {
            GLubyte* data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(buffer.data()),
                                                  (int)buffer.size(),
                                                  &m_width,
                                                  &m_height,
                                                  &m_channels,
                                                  4);
            if (data)
            {
                CreateGLTextureWithData(data, m_width, m_height, 4, true);
                stbi_image_free(data);
            }
            else
            {
                Log::Error("Image could not be loaded from a PNG file. Image:{} URI:{}", GetPath(model, index), image.uri);
            }
        }
        else
        {
            Log::Error("Image could not be loaded from a file. Image:{} URI:{}", GetPath(model, index), image.uri);
        }
    }

    // LabelGL(GL_TEXTURE, m_texture, m_path);
}

Image::Image(FileIO::Directory directory, const std::string& path) : Resource(ResourceType::Image)
{
    const auto buffer = Engine.FileIO().ReadBinaryFile(directory, path);

    if (!buffer.empty())
    {
        GLubyte* data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(buffer.data()),
                                              (int)buffer.size(),
                                              &m_width,
                                              &m_height,
                                              &m_channels,
                                              4);
        if (data)
        {
            CreateGLTextureWithData(data, m_width, m_height, 4, true);
            stbi_image_free(data);
        }
        else
        {
            Log::Error("Image could not be loaded from a PNG file. Image:{}", path);
        }
    }
    else
    {
        Log::Error("Image could not be loaded from a file. Image:{}", path);
    }
}

Image::~Image()
{
    if (m_texture) glDeleteTextures(1, &m_texture);
}

string Image::GetPath(const Model& model, int index)
{
    const auto& image = model.GetDocument().images[index];
    return model.GetPath() + " | Texture-" + to_string(index) + ": " + image.name;
}

void Image::CreateGLTextureWithData(unsigned char* data, int width, int height, int channels, bool genMipMaps)
{
    // This is a public method, so the fields
    // might undefined before this call
    m_width = width;
    m_height = height;
    GLint format = GL_INVALID_VALUE;
    GLint usage = GL_INVALID_VALUE;
    m_channels = channels;
    switch (channels)
    {
        case 1:
            format = GL_R8;
            usage = GL_RED;
            break;
        case 4:
            format = GL_RGBA;
            usage = GL_RGBA;
            break;
        default:
            assert(false);
    }

    if (m_texture) glDeleteTextures(1, &m_texture);

    glGenTextures(1, &m_texture);             // Gen
    glBindTexture(GL_TEXTURE_2D, m_texture);  // Bind

    glTexImage2D(GL_TEXTURE_2D,     // What (target)
                 0,                 // Mip-map level
                 format,            // Internal format
                 m_width,           // Width
                 m_height,          // Height
                 0,                 // Border
                 usage,             // Format (how to use)
                 GL_UNSIGNED_BYTE,  // Type   (how to interpret)
                 data);             // Data

    if (genMipMaps) glGenerateMipmap(GL_TEXTURE_2D);
    LabelGL(GL_TEXTURE, m_texture, m_path);
}
