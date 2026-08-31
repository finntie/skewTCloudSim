#pragma once

#include <glm/glm.hpp>

struct dim3;
struct float4;

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


    unsigned long long noiseTexture; 
    int resolution;

    int sizeX;
    int sizeY;
    int sizeZ;
    int fullSize;
    int voxelSize;


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

	// Highly inspired from https://github.com/BigNerd95/CUDASamples/blob/master/samples/2_Graphics/volumeRender/volumeRender.cpp

	CudaRender();
    ~CudaRender();

	void initGL();
    void initQuad();
    void initShader();

	void cleanUp();

	void render();

	void display();

    // Environment Simulation
    void initEnvironmentData(const int _sizeX, const int _sizeY, const int _sizeZ, const int _voxelSize, dim3& gridDim, dim3& blockDim);
    
    void setDataEnvironment(float* Qw, float* Qc, float* Qr, float* Qs, float* Qi, float* velX, float* VelY, float* velZ, void* stream);

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


private:


	unsigned int VAO = 0;
    unsigned int VBO = 0;
	unsigned int shader = 0;
	unsigned int PBO = 0; // Pixel Buffer Object
    unsigned int m_texture = 0; // Texture

	struct cudaGraphicsResource* cudaPBOResource{};  // CUDA graphics resource to transfer PBO

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


    dim3* m_gridDim{};
    dim3* m_blockDim{};
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