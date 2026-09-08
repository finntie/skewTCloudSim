#pragma once
#include "environment.h"
#include <string>
#include <vector>


struct cloudFileInfo
{
	bool loaded = false;
	int sizeX = 0, sizeY = 0, sizeZ = 0;
	float voxelSize = 0.0f;
	int totalFrames = 0;
	std::string types;
};

class cloudFile
{
public:

	cloudFile() = default;
	~cloudFile();

	void panel();
	void deletePopup();
	void confirmPopup();

	bool isOneTypeSelected();
	int totalTypesSelected();
	float* typeToPointer(int type, environment::gridDataSky* skyData, environment::gridDataGround* groundData, bool sky);
	std::string typeToString(int type, bool sky);
	int stringToType(std::string type, bool& outputSky);

	// Check if file is valid, also include extension (.bin)
	bool checkFile(const char* fileName, std::string& outputFullPath);

	// Checks if a frame has to be created based on recording status and time. 
	// Copies data into one frame if needed
	// Returns true if frame was created
	bool tryCreateFrame(environment::gridDataSky& skyData, environment::gridDataGround& groundData, float time);

	void saveToFile();

	std::vector<std::string> getSimulationFiles();

	
	// Get meta data information of the file, not loading the data but only the information about it
	bool getMetaData(const char* fileName, int& sizeX, int& sizeY, int& sizeZ, float& voxelSize, int& totalFrames, std::string& includedTypes);

	bool loadFile(const char* fileName, bool loadOnlyMetaData = false);


	float getFirstFrameTime() { return m_frames.front(); }
	float getLastFrameTime() { return m_frames.back(); }

	// Use data functions

	// Get lerped data in between 2 frames.
	// frameTime: Ranging 0 - 1, with decimal point being in between frames
	void getLerpedFrameData(environment::gridDataSky* skyData, environment::gridDataGround* groundData, int currentFrame, float frameTime);

	// Receive closest times around the input time, returns false if outside the max and min
	// If outside, outputBeforeTime will be filled if time is LATER than timeframes
	// If outside, outputAfterTime will be filled if time is EARLIER than timeframes
	// afterFrameNum outputs the frame number of outputBeforeTime, meaning the frame before the time
	bool getSurroundedFrameTimes(float time, float& outputBeforeTime, float& outputAfterTime, int& afterFrameNum);

	// Malloc data for the GPU, uses current grid sizes
	void initGPUData(void* stream);

	environment::gridDataSkyGPU& CPUtoGPUDataSky(environment::gridDataSky& skyData, void* stream);
	environment::gridDataGroundGPU& CPUtoGPUDataGround(environment::gridDataGround& groundData, void* stream);

private:

	void transformLerp(float* array1, float* array2, float* outputArray, float lerp, int size);

	// Data of all frames
	std::vector<environment::gridDataSky> m_skyData;
	std::vector<environment::gridDataGround> m_groundData;

	environment::gridDataSkyGPU m_skyDataGPU;
	environment::gridDataGroundGPU m_groundDataGPU;
	bool m_GPUDataInitialized = false;

	std::string m_fileName;
	std::string m_fullFilePath;

	int m_tempSizeX = 0;
	int m_tempSizeY = 0;
	int m_tempSizeZ = 0;
	float m_tempVoxelSize = 0.0f;
	int m_amountTypes = 0;
	int m_totalFrames = 0;
	int m_framesPerHour = 60;
	bool m_recording = false;
	bool m_paused = false;
	float m_currentTime = 0.0f;
	bool m_validFileName = false;


	// Qw, Qc, Qr, Qs, Qi, Qv, Temp, WindX, WindY, WindZ, Pressure
	bool m_typesSky[11]{ false, false,false,false,false,false,false,false,false,false,false };
	// Temp, Water, Qr, Qs, Qi
	bool m_typesGround[5]{ false,false,false,false,false };

	// Stores timeframes of the frames
	std::vector<float> m_frames;

};

// Helper function
bool coloredSelectable(const char* label, bool* value);
