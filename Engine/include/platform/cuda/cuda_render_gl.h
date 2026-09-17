#pragma once

#include <glm/glm.hpp>
#include <vector_types.h>

struct dim3;
struct float4;
struct cudaArray;

struct environmentData
{
    //float* Qv;       //  Mixing Ratio of Water Vapor
    //float* Qw;  //	Mixing Ratio of	Liquid Water
    unsigned long long QwTexture; 
    float* Qc;       //	Mixing Ratio of Ice
    //float* Qr;       //	Mixing Ratio of Rain
    unsigned long long QrTexture;
    unsigned long long QsTexture;
    //float* Qs;       //	Mixing Ratio of Snow
    float* Qi;       //	Mixing Ratio of Ice (precip)
    //float* velfieldX;
    //float* velfieldY;
    //float* velfieldZ;
    unsigned long long velXTexture;
    unsigned long long velYTexture;
    unsigned long long velZTexture;

    unsigned long long SDFTextureQw;
    unsigned long long SDFTextureQr;
    unsigned long long SDFTextureQs;

    unsigned long long envTransmittanceTexture;
    unsigned long long envScatteringTexture;
    unsigned long long envSkyViewTexture;
    unsigned long long envAerialViewTexture;

    unsigned long long depthInformationTexture;
    unsigned long long colorInformationTexture;

    unsigned long long noiseTexture; 
    int resolution;

    int sizeX;
    int sizeY;
    int sizeZ;
    int fullSize;
    float voxelSize;

    float camFar = 0.0f;
    float camNear = 0.0f;
    bool useAlpha = false;
    
    // Extra Render info
    float noiseReduction = 0.45f;
    float minQw = 0.0001f;
    float maxQw = 0.005f;
    float noisePlateauValue = 0.32f;
    float sunStrength = 40.0f;
    float sunDirection[3] = {1, 1, 1};
    float sunColor[3] = {1, 1, 1};
    float exposure = 1.0f;
    float attenuation = 0.8f;
    float contribution = 0.5f;
    float eccentricAttenuation = 0.5f;
    float rayRandomOffset = 0.05f;
    float multipleScatteringDepthPower = 1.0f;
    float ambientLightStrength = 0.1f;
};

class CudaRender
{
public:
    // Highly inspired from
    // https://github.com/BigNerd95/CUDASamples/blob/master/samples/2_Graphics/volumeRender/volumeRender.cpp
    CudaRender();
    ~CudaRender();

    /// <summary>
    /// Registers the framebuffer with CUDA for use later on. Also registers the depth texture.
    /// </summary>
    /// <param name="frameBuffer">OpenGL FrameBuffer used for the draw pass, will be used for the post cloud renderer.</param>
    /// <param name="colorBuffer">OpenGL Color FrameBuffer used for the draw pass, will be used for the post cloud renderer.</param>
    /// <param name="width height">Size of the screen </param>
    void initOpenGLCUDAInterop(unsigned int frameBuffer, unsigned int colorBuffer, int width, int height);

    /// <summary>
    /// Maps OpenGL buffers to CUDA and renders clouds/atmosphere with respect to the depth.
    /// <para> Depth frameBuffer will be rendered to a depth texture before it is able to be used by CUDA. </para>
    /// </summary>
    /// <param name="finalFrameBuffer">OpenGL FrameBuffer used for final draw pass, will be used for the post cloud renderer.</param>
    /// <param name="colorBuffer">OpenGL Color frameBuffer</param>
    /// <param name="width height">Size of the screen </param>
    /// <returns></returns>
    unsigned int postRenderClouds(unsigned int finalFrameBuffer, unsigned int colorBuffer, int width, int height, float camNear = -1, float camFar = -1);

    void cleanUp();

    void display();

    // Environment Simulation
    void initEnvironmentData(const int _sizeX,
                             const int _sizeY,
                             const int _sizeZ,
                             const float _voxelSize,
                             dim3 gridDim,
                             dim3 blockDim);

    void setDataEnvironment(float* Qw,
                            float* Qc,
                            float* Qr,
                            float* Qs,
                            float* Qi,
                            float* velX,
                            float* VelY,
                            float* velZ,
                            bool updateSDF,
                            void* stream);

    void setNoiseTexture(int octaves, int gridSize, float lacunarity);
    void setExtraRenderInfo(float noiseReduction,
                            float minQW,
                            float maxQw,
                            float multipleScattering,
                            float ambientLightStrength,
                            float rayRandomOffset,
                            float attenuation,
                            float contribution,
                            float eccentricattenuation,
                            float sunStrength,
                            float exposure,
                            float* sunDir,
                            float* sunColor);

    // Remove delay of renderer to improve render time, will decrease resources towards other GPU functions (such as simulating)
    void setOnlyRenderResource(bool value) { m_allResourcesRender = value; }

private:
    void initGL();
    void initQuad();
    void initShader();
    unsigned int createShaderProgram(const char* vert, const char* frag);

    void render();

    void checkDepthTypeOtherFBO(unsigned int FBO);
    void checkAlphaUse(unsigned int colorBuffer);

    void setDepthTexture(int width, int height);
    void setColorDepthTexture(int width, int height);

    void copyFBOs(unsigned int FBO);

    void createAndCopyToTextures(cudaArray* depthArray, cudaArray* colorArray);

	unsigned int VAO = 0;
    unsigned int VBO = 0;
	unsigned int shader = 0;
	unsigned int PBO = 0; // Pixel Buffer Object
    unsigned int m_texture = 0; // Texture

    unsigned int m_copyTargetFBO = 0;
    unsigned int m_copyTargetShaderProgram = 0;
    unsigned int m_copyTargetDepthBuffer = 0; // Depth buffer for own FBO
    unsigned int m_writeTargetFBO = 0; // FBO to which we will write
    unsigned int m_writeTargetdepthTex = 0;  // Depth texture which will be registered

    const char* m_SimpleVerShader;
    const char* m_SimpleFragShader;

    float m_camNear = 0.0f;
    float m_camFar = 0.0f;

    // OpenGL CUDA Interop variables
    struct cudaGraphicsResource* m_colorResource{};
    struct cudaGraphicsResource* m_depthResource{};


	struct cudaGraphicsResource* cudaPBOResource{};  // CUDA graphics resource to transfer PBO

    int m_depthFormat = 0;
    bool m_bufferUsesAlpha = false;

    int m_width = 0;
    int m_height = 0;

    float* tempArray;  // Malloced inside the creation function

    environmentData m_envData{};
    void* m_noiseTextureStorage; //In which we store the texture data
    void* m_QWTextureStorage; 
    void* m_QRTextureStorage; 
    void* m_QSTextureStorage; 

    float* m_SDFDistanceNeigh;
    int* m_SDFClosestTarget;
    void* m_SDFTextureStorageQw;
    void* m_SDFTextureStorageQr;
    void* m_SDFTextureStorageQs;

    void* m_velXTextureStorage;
    void* m_velYTextureStorage;
    void* m_velZTextureStorage;

    void* m_envTransmittanceTextureStorage;
    void* m_envScatteringTextureStorage;
    void* m_envSkyViewTextureStorage;
    void* m_envAerialViewTextureStorage;

    bool m_envInitialized{false};
    bool m_setData{false};

    bool m_allResourcesRender{false};

    dim3 m_gridDim{};
    dim3 m_blockDim{};
};


// Initialize a cuda texture to be filled of type T, using smooth transition, which boundary condition (warp or clamp) and how
// many dimensions
template <typename T>
void initTextureObj(void*& storageArray,
                    unsigned long long& texture,
                    const glm::ivec3 size,
                    bool smoothTransition,
                    bool wrapTextureBoundaryMode);

template <typename T>
void copyDataToTexture(T* data, void*& storageArray, const glm::ivec3 size, void* stream);