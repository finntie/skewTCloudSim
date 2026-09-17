#include "platform/cuda/cuda_render_gl.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "platform/opengl/image_gl.hpp"
#include "platform/opengl/mesh_gl.hpp"
#include "platform/opengl/open_gl.hpp"
#include "platform/opengl/shader_gl.hpp"

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#include <cuda_profiler_api.h>

// To get width and height
#include "core/device.hpp"
#include "core/engine.hpp"
#include "core/transform.hpp"
//#include "core/resource.hpp"
#include <iostream>

#include "platform/cuda/cuda_render.cuh"
#include "platform/cuda/LUTs.cuh"


// Highly inspired from https://github.com/BigNerd95/CUDASamples/blob/master/samples/2_Graphics/volumeRender/volumeRender.cpp

// Predeclare templated function for different types due to compiler needing the body
template void copyDataToTexture<float>(float*, void*&, glm::ivec3, void*);
template void copyDataToTexture<float4>(float4*, void*&, glm::ivec3, void*);

static void RenderQuad();

CudaRender::CudaRender() 
{
    initShader();
    initQuad();
    initGL();
}

CudaRender::~CudaRender() 
{
    fillLUTS(m_envData, nullptr, nullptr, 0, 0, true);
    //cudaFree(m_envData.Qw);
    cudaFree(m_SDFClosestTarget);
    cudaFree(m_SDFDistanceNeigh);
    cudaDestroyTextureObject(m_envData.QwTexture);
    cudaFree(m_envData.Qc);
    cudaDestroyTextureObject(m_envData.QrTexture);
    cudaDestroyTextureObject(m_envData.QsTexture);
    cudaFree(m_envData.Qi);
    cudaDestroyTextureObject(m_envData.velXTexture);
    cudaDestroyTextureObject(m_envData.velYTexture);
    cudaDestroyTextureObject(m_envData.velZTexture);
    //cudaFree(m_envData.noiseTexture); Already done
    cudaDestroyTextureObject(m_envData.noiseTexture);
    cudaFreeArray(static_cast<cudaArray_t>(m_velZTextureStorage));
    cudaFreeArray(static_cast<cudaArray_t>(m_velYTextureStorage));
    cudaFreeArray(static_cast<cudaArray_t>(m_velXTextureStorage));
    cudaFreeArray(static_cast<cudaArray_t>(m_SDFTextureStorageQw));
    cudaFreeArray(static_cast<cudaArray_t>(m_noiseTextureStorage));
    cudaFreeArray(static_cast<cudaArray_t>(m_QSTextureStorage));
    cudaFreeArray(static_cast<cudaArray_t>(m_QRTextureStorage));
    cudaFreeArray(static_cast<cudaArray_t>(m_QWTextureStorage));
    cudaFree(tempArray);
}

inline int iDivUp(int a, int b) { return (a % b != 0) ? (a / b + 1) : (a / b); }

void CudaRender::initGL() 
{

    if (PBO)
    {
        cudaGraphicsUnregisterResource(cudaPBOResource);

        // Delete old resource
        glDeleteBuffers(1, &PBO);
        glDeleteTextures(1, &m_texture);
    }

    // Create Pixel buffer object
    glGenBuffers(1, &PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    glBufferData(GL_PIXEL_UNPACK_BUFFER,
                 bee::Engine.Device().GetWidth() * bee::Engine.Device().GetHeight() * sizeof(GLbyte) * 4, 0, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);


    // Check if the PBO is valid
    if (PBO == 0)
    {
        printf("PBO not initialized!\n");
        return;
    }
    // Register with CUDA
    cudaGraphicsGLRegisterBuffer(&cudaPBOResource, PBO, cudaGraphicsMapFlagsWriteDiscard);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }

    // Create texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 bee::Engine.Device().GetWidth(),
                 bee::Engine.Device().GetHeight(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void CudaRender::initQuad() 
{
    // Set quad vertices
    float vertices[] = 
    {
      // x,  y          u, v
        -1, -1,         0, 0, 
         1, -1,         1, 0, 
         1,  1,         1, 1, 
        -1, -1,         0, 0, 
         1,  1,         1, 1, 
        -1,  1,         0, 1,
    };


    // Create and bind VAO
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Create and bind VBO
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Set vertex attribute pointers
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // Color
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void CudaRender::initShader() 
{
    // Vertex Shader
    const char* vertexShaderCode =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "	TexCoord = aTexCoord;\n"
        "}\0";

    // Fragment Shader
    const char* fragShaderCode =
        "#version 330 core\n"
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D screenTexture;\n"
        "void main()\n"
        "{\n"
        "   FragColor = texture(screenTexture, TexCoord);\n"
        "}\0";

    shader = createShaderProgram(vertexShaderCode, fragShaderCode);

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "screenTexture"), 0);  // use texture unit 0
    glUseProgram(0);
}

unsigned int CudaRender::createShaderProgram(const char* vert, const char* frag) 
{
    int success;
    char infoLog[512];

    	// Bind vertex shader and compile
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vert, NULL);
    glCompileShader(vertShader);

    // Check for errors
    glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertShader, 512, NULL, infoLog);
        printf("OPENGL ERROR: vertex shader compilation failed: %s\n", infoLog);
        return 0;
    }

    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &frag, NULL);
    glCompileShader(fragShader);

    // Check for errors
    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragShader, 512, NULL, infoLog);
        printf("OPENGL ERROR: fragment shader compilation failed: %s\n", infoLog);
        return 0;
    }

    // link shaders
    GLuint program = 0;
    program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "screenTexture"), 0);  // use texture unit 0
    glUseProgram(0);

    // Check for errors
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("OPENGL ERROR: shader program linking failed: %s\n", infoLog);
        return 0;
    }

    // I don't need you anymore
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return program;
}

void CudaRender::initOpenGLCUDAInterop(unsigned int finalFrameBuffer, unsigned int colorBuffer, int width, int height)
{
    m_width = width;
    m_height = height;

        // Shaders to write to the depth texture
    m_SimpleVerShader =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "out vec2 UV;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(aPos, 1.0);\n"
        "	UV = aPos.xy;\n"
        "}\0";

    m_SimpleFragShader =
        "#version 330 core\n"
        "in vec2 UV;\n"
        "out float outDepth;\n"
        "uniform sampler2D depthTex;\n"
        "uniform float near;\n"
        "uniform float far;\n"

        "float LinearizeDepth(float depth) \n"
        "{\n"
        "    float z = depth * 2.0 - 1.0; \n" // back to NDC 
        "    return (2.0 * near * far) / (far + near - z * (far - near));\n"
        "}\n"

        "void main()\n"
        "{\n"
        "   outDepth = LinearizeDepth(texture(depthTex, ((UV + 1.0f) * 0.5f)).r);\n"
        "}\0";

    m_copyTargetShaderProgram = createShaderProgram(m_SimpleVerShader, m_SimpleFragShader);

    // Check if our color buffer uses alpha
    checkAlphaUse(colorBuffer);

    // Check how depth was initialized in this FBO, so we can copy over the FBO correctly
    checkDepthTypeOtherFBO(finalFrameBuffer);

    // Initialize our FBO
    glGenFramebuffers(1, &m_copyTargetFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_copyTargetFBO);

    // Create our own depth texture to use for the FBO, it needs to match the input FBO's depth buffer
    setDepthTexture(width, height);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "FBO incomplete!" << std::endl;
        __debugbreak();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create write target FBO
    glGenFramebuffers(1, &m_writeTargetFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_writeTargetFBO);

    // Create depth texture which we will connect to CUDA later on
    setColorDepthTexture(width, height);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "FBO incomplete!" << std::endl;
        __debugbreak();
    }

    // Unbind
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    // Regiser own FBO to CUDA resource
    cudaGraphicsGLRegisterImage(&m_colorResource, colorBuffer, GL_TEXTURE_2D, cudaGraphicsMapFlagsReadOnly);

    // Register own depth texture to be usable by CUDA
    cudaGraphicsGLRegisterImage(&m_depthResource, m_writeTargetdepthTex, GL_TEXTURE_2D, cudaGraphicsMapFlagsReadOnly);

    // Check errors
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }
}

unsigned int CudaRender::postRenderClouds(unsigned int finalFrameBuffer, unsigned int , int width, int height, float camNear, float camFar) 
{ 
    if (camFar >= 0 && camNear >= 0)
    {
        m_camNear = camNear;
        m_camFar = camFar;
        m_envData.camNear = camNear;
        m_envData.camFar = camFar;
    }

    // 1.  -- Draw to depth texture from depth buffer --

    // First copy over input FBO to own FBO in which we stored our depth texture as color attachment
    copyFBOs(finalFrameBuffer);

    // Draw into own FBO, filling depth texture
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, m_writeTargetFBO);
    glViewport(0, 0, width, height);
    glUseProgram(m_copyTargetShaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_copyTargetDepthBuffer);
    glUniform1i(glGetUniformLocation(m_copyTargetShaderProgram, "depthTex"), 0);  // use texture unit 0
    glUniform1f(glGetUniformLocation(m_copyTargetShaderProgram, "near"), camNear);  // Set near
    glUniform1f(glGetUniformLocation(m_copyTargetShaderProgram, "far"), camFar);  // Set Far
    RenderQuad();
    glUseProgram(0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf("Error in OpenGL draw: %i\n", error);
    }


    // 2.  -- Map finalFrameBuffer and depth Texture to CUDA --

    cudaArray_t colorArray, depthArray;
    // Map both resources at once
    cudaGraphicsResource_t resources[2] = {m_colorResource, m_depthResource};
    cudaGraphicsMapResources(2, resources, getStream()); 
    cudaGraphicsSubResourceGetMappedArray(&colorArray, resources[0], 0, 0);
    cudaGraphicsSubResourceGetMappedArray(&depthArray, resources[1], 0, 0);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }


    // 3.  -- Pass arrays to textures --

    createAndCopyToTextures(depthArray, colorArray);

    err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }


    // 4.  -- Render cloud simulation --

    render();

    // 5.  -- Draw rendered PBO --

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // Copy from PBO to texture
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // Draw quad on which we will show our output texture
    glUseProgram(shader);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf("Error in OpenGL draw: %i\n", error);
    }


    // Unmap
    cudaGraphicsUnmapResources(2, resources, getStream());


    // Last error check
    err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }

    return PBO;
}

void CudaRender::cleanUp()
{

    if (PBO)
    {
        cudaGraphicsUnregisterResource(cudaPBOResource);
        glDeleteBuffers(1, &PBO);
        glDeleteTextures(1, &m_texture);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shader);
}

void CudaRender::render()
{
    if (m_setData)
    {
        cudaStream_t stream = getStream();

        if (!stream)
        {
            printf("Error, could not retrieve render stream, have you initialed it?\n");
            return;
        }

        unsigned int* dOutput;
        size_t numBytes;

        // Map PBO to get CUDA device pointer
        cudaGraphicsMapResources(1, &cudaPBOResource, stream);
        cudaGraphicsResourceGetMappedPointer((void**)&dOutput, &numBytes, cudaPBOResource);

        // Clear image
        cudaMemsetAsync(dOutput, 0, bee::Engine.Device().GetWidth() * bee::Engine.Device().GetHeight() * 4, stream);

        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess)
        {
            std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
            __debugbreak();
        }

        // TODO: move to somewhere else
        dim3 blockSize(16, 16);
        dim3 gridSize;
        gridSize =
            dim3(iDivUp(bee::Engine.Device().GetWidth(), blockSize.x), iDivUp(bee::Engine.Device().GetHeight(), blockSize.y));

        // Set over the view matrix
        for (const auto& [e, camera, cameraTransform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
        {
            const glm::mat4& view = glm::transpose((cameraTransform.World()));
            float3 gridMin = make_float3(0, 0, 0);
            float3 gridMax = make_float3(float(m_envData.sizeX), float(m_envData.sizeY), float(m_envData.sizeZ));
            gridMax = make_float3(gridMin.x + gridMax.x, gridMin.y + gridMax.y, gridMin.z + gridMax.z);
            initConstants(glm::value_ptr(view), sizeof(float4) * 3, gridMin, gridMax);
        }

        fillLUTS(m_envData,
                 m_envSkyViewTextureStorage,
                 m_envAerialViewTextureStorage,
                 bee::Engine.Device().GetWidth(),
                 bee::Engine.Device().GetHeight(),
                 false);

        cudaStreamSynchronize(stream);
        err = cudaGetLastError();
        if (err != cudaSuccess)
        {
            std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
            __debugbreak();
        }

        // Actual rendering function, writing into dOutput
        renderEnvironmentCUDA(gridSize,
                              blockSize,
                              dOutput,
                              m_envData,
                              m_allResourcesRender,
                              bee::Engine.Device().GetWidth(),
                              bee::Engine.Device().GetHeight());


        cudaGraphicsUnmapResources(1, &cudaPBOResource, stream);

        err = cudaGetLastError();
        if (err != cudaSuccess)
        {
            std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
            __debugbreak();
        }
    }
}

void CudaRender::checkDepthTypeOtherFBO(unsigned int FBO) 
{ 
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    GLint depthType = 0;
    // Get attachement type
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                          &depthType);
    // If type is texture, we get the format of the texture
    if (depthType == GL_TEXTURE)
    {
        GLint texName = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                              GL_DEPTH_ATTACHMENT,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              &texName);
        glBindTexture(GL_TEXTURE_2D, texName);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &m_depthFormat);
    }
    else if (depthType == GL_RENDERBUFFER)
    {
        // If type is a renderbuffer, we get that format
        GLint rBuffName = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                              GL_DEPTH_ATTACHMENT,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              &rBuffName);
        glBindRenderbuffer(GL_RENDERBUFFER, rBuffName);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &m_depthFormat);
    }

    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf("Error in retrieving type: %i\n", error);
    }
}

void CudaRender::checkAlphaUse(unsigned int colorBuffer) 
{
    // Assuming colorBuffer is a texture
    glBindTexture(GL_TEXTURE_2D, colorBuffer);
    GLint internalFormat = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
    // Check if alpha or not
    switch (internalFormat)
    {
        case GL_RGB:
        case GL_RGB8:
            m_bufferUsesAlpha = false;
            m_envData.useAlpha = false;
            break;
        case GL_RGBA:
        case GL_RGBA8:
            m_bufferUsesAlpha = true;
            m_envData.useAlpha = true;
            break;
        default:
            break;
    }
}

void CudaRender::setDepthTexture(int width, int height)
{
    if (m_copyTargetDepthBuffer == 0) glGenTextures(1, &m_copyTargetDepthBuffer);
    // Create the depth texture
    glBindTexture(GL_TEXTURE_2D, m_copyTargetDepthBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, m_depthFormat, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);  // Set storage
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);                                        // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);                                        // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);                                     // Clamping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);                                     // Clamping

    // Attach to own FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_copyTargetDepthBuffer, 0);
    
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "Error, Frambuffer is incomplete" << std::endl;
        __debugbreak();
    }
}

void CudaRender::setColorDepthTexture(int width, int height)
{
    if (m_writeTargetdepthTex == 0) glGenTextures(1, &m_writeTargetdepthTex);
    // Create the depth texture
    glBindTexture(GL_TEXTURE_2D, m_writeTargetdepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);  // Set storage
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);                     // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);                     // Filtering

    // Attach to own FBO as color attachment
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_writeTargetdepthTex, 0);

    GLenum drawBufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBufs);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "Error, Frambuffer is incomplete" << std::endl;
        __debugbreak();
    }
}

void CudaRender::copyFBOs(unsigned int FBO) 
{ 
    // Blit FBO to our own FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);

    GLint depthType = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                          &depthType);
    if (depthType == GL_NONE)
    {
        std::cout << "WARNING: source FBO " << FBO << " has no depth attachment — depth blit will no-op!" << std::endl;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_copyTargetFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        std::cout << "Blit error: " << err << std::endl;
    }
}

void CudaRender::createAndCopyToTextures(cudaArray* depthArray, cudaArray* colorArray)
{
    // Set correct settings
    cudaTextureDesc texDesc{};
    texDesc.filterMode = cudaFilterModePoint;
    texDesc.normalizedCoords = false;
    texDesc.addressMode[0] = cudaAddressModeClamp;
    texDesc.readMode = cudaReadModeElementType;

    cudaResourceDesc resDesc{};
    resDesc.resType = cudaResourceTypeArray;

    // Create 2 texture objects and save to stored values
    resDesc.res.array.array = depthArray;
    cudaCreateTextureObject(&m_envData.depthInformationTexture, &resDesc, &texDesc, NULL);
    resDesc.res.array.array = colorArray;    
    cudaCreateTextureObject(&m_envData.colorInformationTexture, &resDesc, &texDesc, NULL);
}


// Renders a 1x1 XY quad in NDC, Copied from BEE engine
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

void CudaRender::display() 
{
    // Render fills the PBO with all data we need
    render();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, bee::Engine.Device().GetWidth(), bee::Engine.Device().GetHeight());
   
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // Copy from PBO to texture
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bee::Engine.Device().GetWidth(), bee::Engine.Device().GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);



    // Draw quad on which we will show our output texture
    glUseProgram(shader);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf("Error in OpenGL draw: %i\n", error);
    }
}

void CudaRender::initEnvironmentData(const int _sizeX,
                                     const int _sizeY,
                                     const int _sizeZ,
                                     const float _voxelSize,
                                     dim3 gridDim,
                                     dim3 blockDim)
{
    m_envData.sizeX = _sizeX;
    m_envData.sizeY = _sizeY;
    m_envData.sizeZ = _sizeZ;
    m_envData.voxelSize = _voxelSize;
    m_envData.fullSize = _sizeX * _sizeY * _sizeZ;

    m_gridDim = gridDim;
    m_blockDim = blockDim;

    // Malloc data
    //cudaMalloc((void**)&m_envData.Qw, m_envData.fullSize * sizeof(float));
    cudaMalloc((void**)&m_envData.Qc, m_envData.fullSize * sizeof(float));
    //cudaMalloc((void**)&m_envData.Qs, m_envData.fullSize * sizeof(float));
    cudaMalloc((void**)&m_envData.Qi, m_envData.fullSize * sizeof(float));

    // Signed Distance Field data
    cudaMalloc((void**)&m_SDFDistanceNeigh, m_envData.fullSize * sizeof(float));
    cudaMalloc((void**)&m_SDFClosestTarget, m_envData.fullSize * sizeof(float));

    // Init render stream
    initStream();

    // Create noise texture
    const int resolution = 256;
    const int octaves = 6;
    cudaMalloc((void**)&tempArray, resolution * resolution * resolution * sizeof(float));
    fillNoiseTexture(tempArray, resolution, octaves, 2, 2.0f, 10);
    m_envData.resolution = resolution;

    // Copy data into a CUDA texture
    initTextureObj<float>(m_noiseTextureStorage, m_envData.noiseTexture, glm::ivec3(resolution), true, true);
    copyDataToTexture<float>(tempArray, m_noiseTextureStorage, glm::ivec3(resolution), getStream());

    // Dont need this data anymore
    //cudaFree(m_envData.tempArray);

    // initialize environment data
    initTextureObj<float>(m_QWTextureStorage, m_envData.QwTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);
    initTextureObj<float>(m_SDFTextureStorageQw, m_envData.SDFTextureQw, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);
    initTextureObj<float>(m_QRTextureStorage, m_envData.QrTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);
    initTextureObj<float>(m_SDFTextureStorageQr, m_envData.SDFTextureQr, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);
    initTextureObj<float>(m_QSTextureStorage, m_envData.QsTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);
    initTextureObj<float>(m_SDFTextureStorageQs, m_envData.SDFTextureQs, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);
    initTextureObj<float>(m_velXTextureStorage, m_envData.velXTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);
    initTextureObj<float>(m_velYTextureStorage, m_envData.velYTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);
    initTextureObj<float>(m_velZTextureStorage, m_envData.velZTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), true, true);

    initTextureObj<float4>(m_envTransmittanceTextureStorage, m_envData.envTransmittanceTexture, glm::ivec3(256, 64, 0), true, true);
    initTextureObj<float4>(m_envScatteringTextureStorage, m_envData.envScatteringTexture, glm::ivec3(32, 32, 0), true, true);
    initTextureObj<float4>(m_envSkyViewTextureStorage, m_envData.envSkyViewTexture, glm::ivec3(200, 100, 0), true, true);
    initTextureObj<float4>(m_envAerialViewTextureStorage, m_envData.envAerialViewTexture, glm::ivec3(32, 32, 32), true, true);

    fillLUTSOnce(m_envData, m_envTransmittanceTextureStorage, m_envScatteringTextureStorage);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }

    m_envInitialized = true;
}
void CudaRender::setNoiseTexture(int octaves, int gridSize, float lacunarity)
{
    fillNoiseTexture(tempArray, m_envData.resolution, octaves, gridSize, lacunarity, 10);

    copyDataToTexture<float>(tempArray, m_noiseTextureStorage, glm::ivec3(m_envData.resolution), getStream());


    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }
}

void CudaRender::setExtraRenderInfo(float noiseReduction,
                                    float minQW,
                                    float maxQW,
                                    float multipleScattering,
                                    float ambientLightStrength,
                                    float rayRandomOffset,
                                    float attenuation,
                                    float contribution,
                                    float eccentricattenuation,
                                    float sunStrength,
                                    float exposure,
                                    float* sunDir,
                                    float* sunColor)
{

    m_envData.noiseReduction = noiseReduction;
    m_envData.minQw = minQW;
    m_envData.maxQw = maxQW;
    m_envData.multipleScatteringDepthPower = multipleScattering;
    m_envData.ambientLightStrength = ambientLightStrength;
    m_envData.rayRandomOffset = rayRandomOffset;
    m_envData.attenuation = attenuation;
    m_envData.contribution = contribution;
    m_envData.eccentricAttenuation = eccentricattenuation;
    m_envData.sunStrength = sunStrength;
    m_envData.exposure = exposure;
    memcpy(m_envData.sunDirection, sunDir, 3 * sizeof(float));
    memcpy(m_envData.sunColor, sunColor, 3 * sizeof(float));
}

template <typename T>
void initTextureObj(void*& storageArray,
                                unsigned long long& texture,
                                const glm::ivec3 size,
                                bool smooth,
                                bool wrapTextureBoundaryMode)
{
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<T>();
    cudaExtent extent = make_cudaExtent(size.x, size.y, size.z);
    cudaArray_t cuArray;
    cudaMalloc3DArray(&cuArray, &channelDesc, extent);

    cudaTextureDesc texDesc{};
    texDesc.filterMode = smooth ? cudaFilterModeLinear : cudaFilterModePoint;
    texDesc.normalizedCoords = true;
    texDesc.addressMode[0] = wrapTextureBoundaryMode ? cudaAddressModeWrap : cudaAddressModeClamp;
    texDesc.addressMode[1] = wrapTextureBoundaryMode ? cudaAddressModeWrap : cudaAddressModeClamp;
    texDesc.addressMode[2] = wrapTextureBoundaryMode ? cudaAddressModeWrap : cudaAddressModeClamp;
    texDesc.readMode = cudaReadModeElementType;
    cudaResourceDesc resDesc{};
    resDesc.resType = cudaResourceTypeArray;
    resDesc.res.array.array = cuArray;
    storageArray = cuArray;

    cudaTextureObject_t texObj;
    cudaCreateTextureObject(&texObj, &resDesc, &texDesc, NULL);
    texture = texObj;
}

template <typename T>
void copyDataToTexture(T* data, void*& storageArray, const glm::ivec3 size, void* stream)
{
    cudaMemcpy3DParms cpyParams{};
    cpyParams.srcPtr = make_cudaPitchedPtr(data, size.x * sizeof(T), size.x * sizeof(T), size.y);
    cpyParams.dstArray = static_cast<cudaArray_t>(storageArray);
    cpyParams.extent = make_cudaExtent(size.x, size.y, size.z == 0 ? 1 : size.z);
    cpyParams.kind = cudaMemcpyDeviceToDevice;
    cudaMemcpy3DAsync(&cpyParams, static_cast<cudaStream_t>(stream));

}

void CudaRender::setDataEnvironment(float* Qw,
                                    float*,
                                    float* Qr,
                                    float* Qs,
                                    float*,
                                    float* velX,
                                    float* velY,
                                    float* velZ,
                                    bool updateSDF,
                                    void* stream)
{
    if (m_envData.fullSize == 0)
    {
        printf(
            "Warning: environment renderer fullsize is 0 when trying to set environmnent render data, call "
            "initEnvironmentData() beforehand\n");
    }
    // cudaMemcpy(m_envData.Qw, Qw, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);

    if (Qw) copyDataToTexture<float>(Qw, m_QWTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    if (Qr) copyDataToTexture<float>(Qr, m_QRTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    if (Qs) copyDataToTexture<float>(Qs, m_QSTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    if (velX) copyDataToTexture<float>(velX, m_velXTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    if (velY) copyDataToTexture<float>(velY, m_velYTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    if (velZ) copyDataToTexture<float>(velZ, m_velZTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);

        cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }

    if (Qw && updateSDF)
    fillSDF(glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ),
            Qw,
            0.00005f,
            m_SDFTextureStorageQw,
            m_SDFDistanceNeigh,
            m_SDFClosestTarget,
            m_gridDim,
            m_blockDim,
            stream);

    if (Qr && updateSDF)
    fillSDF(glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ),
            Qr,
            0.00001f,
            m_SDFTextureStorageQr,
            m_SDFDistanceNeigh,
            m_SDFClosestTarget,
            m_gridDim,
            m_blockDim,
            stream);

    if (Qs && updateSDF)
    fillSDF(glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ),
            Qs,
            0.00001f,
            m_SDFTextureStorageQs,
            m_SDFDistanceNeigh,
            m_SDFClosestTarget,
            m_gridDim,
            m_blockDim,
            stream);
    // cudaMemcpy(m_envData.Qc, Qc, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);
    // cudaMemcpy(m_envData.Qr, Qr, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);
    // cudaMemcpy(m_envData.Qs, Qs, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);
    // cudaMemcpy(m_envData.Qi, Qi, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);
    // cudaMemcpy(m_envData.velfieldX, velX, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);
    // cudaMemcpy(m_envData.velfieldY, VelY, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);
    // cudaMemcpy(m_envData.velfieldZ, velZ, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);

    // switchActiveTexture();

    m_setData = true;

        if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }
}

