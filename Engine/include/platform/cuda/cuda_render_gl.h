#pragma once

#include <glm/glm.hpp>

struct dim3;

struct environmentData
{
    //float* Qv;       //  Mixing Ratio of Water Vapor
    //float* Qw;  //	Mixing Ratio of	Liquid Water
    unsigned long long QwTexture; 
    float* Qc;       //	Mixing Ratio of Ice
    //float* Qr;       //	Mixing Ratio of Rain
    unsigned long long QrTexture;
    float* Qs;       //	Mixing Ratio of Snow
    float* Qi;       //	Mixing Ratio of Ice (precip)
    //float* velfieldX;
    //float* velfieldY;
    //float* velfieldZ;
    unsigned long long velXTexture;
    unsigned long long velYTexture;
    unsigned long long velZTexture;

    unsigned long long SDFTextureQw;
    unsigned long long SDFTextureQr;

    float* tempArray; // Malloced inside the creation function
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

    float rayRandomOffset = 0.05f;
    float multipleScatteringDepthPower = 1.0f;
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
    void initTextureObj(void*& storageArray, unsigned long long& texture, const glm::ivec3 size);
    void copyDataToTexture(float* data, void*& storageArray, const glm::ivec3 size, void* stream);
    void setDataEnvironment(float* Qw, float* Qc, float* Qr, float* Qs, float* Qi, float* velX, float* VelY, float* velZ, void* stream);

    void setNoiseTexture(int octaves, int gridSize, float lacunarity);
    void setExtraRenderInfo(float noiseReduction,
                            float minQW,
                            float maxQw,
                            float multipleScattering,
                            float rayRandomOffset,
                            float sunStrength,
                            float* sunDir,
                            float* sunColor);


private:


	unsigned int VAO = 0;
    unsigned int VBO = 0;
	unsigned int shader = 0;
	unsigned int PBO = 0; // Pixel Buffer Object
    unsigned int m_texture = 0; // Texture

	struct cudaGraphicsResource* cudaPBOResource{};  // CUDA graphics resource to transfer PBO

    environmentData m_envData{};
    void* m_noiseTextureStorage; //In which we store the texture data
    void* m_QWTextureStorage; 
    void* m_QRTextureStorage; 

    float* m_SDFDistanceNeigh;
    int* m_SDFClosestTarget;
    void* m_SDFTextureStorageQw;
    void* m_SDFTextureStorageQr;
    void* m_velXTextureStorage;
    void* m_velYTextureStorage;
    void* m_velZTextureStorage;

    bool m_envInitialized{false};
    bool m_setData{false};


    dim3* m_gridDim{};
    dim3* m_blockDim{};
};
