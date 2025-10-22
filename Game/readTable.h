#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>
#include <memory>


struct radioSondeData
{
	float* altitude = nullptr; //In meter
	float* pressure = nullptr; //In hPa
	float* dewPoint = nullptr; //In Celcius
	float* temperature = nullptr; //In Celcius
	float* windSpeed = nullptr;
	float* windDir = nullptr;

	size_t dataSize = 0;

	void allocate(size_t size)
	{
		altitude = new float[size];
		pressure = new float[size];
		dewPoint = new float[size];
		temperature = new float[size];
		windSpeed = new float[size];
		windDir = new float[size];
		dataSize = size;
	}

	radioSondeData() = default;
	~radioSondeData()
	{
		delete[] altitude;
		delete[] pressure;
		delete[] dewPoint;
		delete[] temperature;
		delete[] windSpeed;
		delete[] windDir;
	}

	// Add move constructor
	radioSondeData(radioSondeData&&) = default;
	radioSondeData& operator=(radioSondeData&&) = default;
	// Prevent copying
	radioSondeData(const radioSondeData&) = delete;
	radioSondeData& operator=(const radioSondeData&) = delete;
};

struct skewTInfo
{
	radioSondeData data = radioSondeData();
	std::map<float, float> heightToPressure;
	std::map<float, float> pressureToHeight;
	std::string stationNumberName{};

};

class readTable
{
public:

	void readKNMIFile(const char* file);

	void readDWDFile(const char* file);

	void initEnvironment();


	void heightToPressureCalculate();
	std::pair<float,int> getPressureAtHeight(float height);
	int getIndexAtHeight(float height);
	float getHeightAtPressure(float pressure);
	
	//Converts temp and height in meters or pressure to plotting so its easily modified.
	//Warning: Not using pressure (so height in meters) could lead up to different values due to observed not being the same as standard.
	glm::vec2 convertToPlottingCoordinates(const float temp, const float value, const bool pressure, const float scaleWidht, const float height);

	void debugDrawData();

	float angle = 45.0f;
	float liquid = 1.0f;
	float plotHeight = 0.005f;
	bool useI = false;
	float CAPE = 0.0f;
	const float tanTheta = glm::tan(glm::radians(45.0f));
	glm::vec2 sizeSkewT = { 1.0f,100 };


	skewTInfo skewTData;
private:



};

