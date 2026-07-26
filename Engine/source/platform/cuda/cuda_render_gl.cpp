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


// Highly inspired from https://github.com/BigNerd95/CUDASamples/blob/master/samples/2_Graphics/volumeRender/volumeRender.cpp


CudaRender::CudaRender() 
{
    initShader();
    initQuad();
    initGL();
}

CudaRender::~CudaRender() 
{
    //cudaFree(m_envData.Qw);
    cudaFree(m_SDFClosestTarget);
    cudaFree(m_SDFDistanceNeigh);
    cudaDestroyTextureObject(m_envData.QwTexture);
    cudaFree(m_envData.Qc);
    cudaDestroyTextureObject(m_envData.QrTexture);
    cudaFree(m_envData.Qs);
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
    cudaFreeArray(static_cast<cudaArray_t>(m_QRTextureStorage));
    cudaFreeArray(static_cast<cudaArray_t>(m_QWTextureStorage));
    cudaFree(m_envData.tempArray);
}

int iDivUp(int a, int b) { return (a % b != 0) ? (a / b + 1) : (a / b); }

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

    err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }
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
    int success;
    char infoLog[512];

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

    	// Bind vertex shader and compile
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertexShaderCode, NULL);
    glCompileShader(vertShader);

    // Check for errors
    glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertShader, 512, NULL, infoLog);
        printf("OPENGL ERROR: vertex shader compilation failed: %s\n", infoLog);
        return;
    }

    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragShaderCode, NULL);
    glCompileShader(fragShader);

    // Check for errors
    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragShader, 512, NULL, infoLog);
        printf("OPENGL ERROR: fragment shader compilation failed: %s\n", infoLog);
        return;
    }

    // link shaders
    shader = glCreateProgram();
    glAttachShader(shader, vertShader);
    glAttachShader(shader, fragShader);
    glLinkProgram(shader);

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "screenTexture"), 0);  // use texture unit 0
    glUseProgram(0);

    // Check for errors
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shader, 512, NULL, infoLog);
        printf("OPENGL ERROR: shader program linking failed: %s\n", infoLog);
        return;
    }

    // I don't need you anymore
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
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

        // Actual rendering function, writing into dOutput
        renderEnvironmentCUDA(gridSize,
                              blockSize,
                              dOutput,
                              m_envData,
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
                                     const int _voxelSize,
                                     dim3& gridDim,
                                     dim3& blockDim)
{
    m_envData.sizeX = _sizeX;
    m_envData.sizeY = _sizeY;
    m_envData.sizeZ = _sizeZ;
    m_envData.voxelSize = _voxelSize;
    m_envData.fullSize = _sizeX * _sizeY * _sizeZ;

    m_gridDim = &gridDim;
    m_blockDim = &blockDim;

    // Malloc data
    //cudaMalloc((void**)&m_envData.Qw, m_envData.fullSize * sizeof(float));
    cudaMalloc((void**)&m_envData.Qc, m_envData.fullSize * sizeof(float));
    cudaMalloc((void**)&m_envData.Qs, m_envData.fullSize * sizeof(float));
    cudaMalloc((void**)&m_envData.Qi, m_envData.fullSize * sizeof(float));

    // Signed Distance Field data
    cudaMalloc((void**)&m_SDFDistanceNeigh, m_envData.fullSize * sizeof(float));
    cudaMalloc((void**)&m_SDFClosestTarget, m_envData.fullSize * sizeof(float));

    // Init render stream
    initStream();

    // Create noise texture
    const int resolution = 256;
    const int octaves = 6;
    cudaMalloc((void**)&m_envData.tempArray, resolution * resolution * resolution * sizeof(float));
    fillNoiseTexture(m_envData.tempArray, resolution, octaves, 2, 2.0f, 10);
    m_envData.resolution = resolution;

    // Copy data into a CUDA texture
    initTextureObj(m_noiseTextureStorage, m_envData.noiseTexture, glm::ivec3(resolution));
    copyDataToTexture(m_envData.tempArray, m_noiseTextureStorage, glm::ivec3(resolution), getStream());

    // Dont need this data anymore
    //cudaFree(m_envData.tempArray);

    // initialize environment data
    initTextureObj(m_QWTextureStorage, m_envData.QwTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ));
    initTextureObj(m_SDFTextureStorageQw, m_envData.SDFTextureQw, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ));
    initTextureObj(m_QRTextureStorage, m_envData.QrTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ));
    initTextureObj(m_SDFTextureStorageQr, m_envData.SDFTextureQr, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ));
    initTextureObj(m_velXTextureStorage, m_envData.velXTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ));
    initTextureObj(m_velYTextureStorage, m_envData.velYTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ));
    initTextureObj(m_velZTextureStorage, m_envData.velZTexture, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ));
    
    m_envInitialized = true;
}
void CudaRender::setNoiseTexture(int octaves, int gridSize, float lacunarity)
{
    fillNoiseTexture(m_envData.tempArray, m_envData.resolution, octaves, gridSize, lacunarity, 10);

    copyDataToTexture(m_envData.tempArray, m_noiseTextureStorage, glm::ivec3(m_envData.resolution), getStream());


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
                                    float rayRandomOffset,
                                    float sunStrength,
                                    float* sunDir,
                                    float* sunColor)
{

    m_envData.noiseReduction = noiseReduction;
    m_envData.minQw = minQW;
    m_envData.maxQw = maxQW;
    m_envData.multipleScatteringDepthPower = multipleScattering;
    m_envData.rayRandomOffset = rayRandomOffset;
    m_envData.sunStrength = sunStrength;
    memcpy(m_envData.sunDirection, sunDir, 3 * sizeof(float));
    memcpy(m_envData.sunColor, sunColor, 3 * sizeof(float));
}


void CudaRender::initTextureObj(void*& storageArray, unsigned long long& texture, const glm::ivec3 size)
{
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<float>();
    cudaExtent extent = make_cudaExtent(size.x, size.y, size.z);
    cudaArray_t cuArray;
    cudaMalloc3DArray(&cuArray, &channelDesc, extent);

    cudaTextureDesc texDesc{};
    texDesc.filterMode = cudaFilterModeLinear;
    texDesc.normalizedCoords = true;
    texDesc.addressMode[0] = cudaAddressModeWrap;
    texDesc.addressMode[1] = cudaAddressModeWrap;
    texDesc.addressMode[2] = cudaAddressModeWrap;
    texDesc.readMode = cudaReadModeElementType;
    cudaResourceDesc resDesc{};
    resDesc.resType = cudaResourceTypeArray;
    resDesc.res.array.array = cuArray;
    storageArray = cuArray;

    cudaTextureObject_t texObj;
    cudaCreateTextureObject(&texObj, &resDesc, &texDesc, NULL);
    texture = texObj;
}

void CudaRender::copyDataToTexture(float* data, void*& storageArray, const glm::ivec3 size, void* stream)
{
    cudaMemcpy3DParms cpyParams{};
    cpyParams.srcPtr = make_cudaPitchedPtr(data, size.x * sizeof(float), size.x, size.y);
    cpyParams.dstArray = static_cast<cudaArray_t>(storageArray);
    cpyParams.extent = make_cudaExtent(size.x, size.y, size.z);
    cpyParams.kind = cudaMemcpyDeviceToDevice;
    cudaMemcpy3DAsync(&cpyParams, static_cast<cudaStream_t>(stream));

}

void CudaRender::setDataEnvironment(float* Qw,
                                    float*,
                                    float* Qr,
                                    float*,
                                    float*,
                                    float* velX,
                                    float* velY,
                                    float* velZ,
                                    void* stream)
{
    if (m_envData.fullSize == 0)
    {
        printf(
            "Warning: environment renderer fullsize is 0 when trying to set environmnent render data, call "
            "initEnvironmentData() beforehand\n");
    }
    // cudaMemcpy(m_envData.Qw, Qw, m_envData.fullSize * sizeof(float), cudaMemcpyDeviceToDevice);

    copyDataToTexture(Qw, m_QWTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    copyDataToTexture(Qr, m_QRTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    copyDataToTexture(velX, m_velXTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    copyDataToTexture(velY, m_velYTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);
    copyDataToTexture(velZ, m_velZTextureStorage, glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ), stream);

    fillSDF(glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ),
            Qw,
            0.00005f,
            m_SDFTextureStorageQw,
            m_SDFDistanceNeigh,
            m_SDFClosestTarget,
            *m_gridDim,
            *m_blockDim,
            stream);

    fillSDF(glm::ivec3(m_envData.sizeX, m_envData.sizeY, m_envData.sizeZ),
            Qr,
            0.00001f,
            m_SDFTextureStorageQr,
            m_SDFDistanceNeigh,
            m_SDFClosestTarget,
            *m_gridDim,
            *m_blockDim,
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
}

